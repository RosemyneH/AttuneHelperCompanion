package com.attunehelper.companion

import android.Manifest
import android.app.PendingIntent
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.net.Uri
import android.nfc.NdefMessage
import android.nfc.NfcAdapter
import android.os.Build
import android.os.Bundle
import android.view.View
import android.widget.ArrayAdapter
import android.widget.ImageView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.attunehelper.companion.addon.AddonInstall
import com.attunehelper.companion.data.AttuneHistoryStore
import com.attunehelper.companion.nfc.AhcNfcHelper
import com.attunehelper.companion.nfc.NfcNdefPushCompat
import com.attunehelper.companion.saf.SynastriaFolder
import com.attunehelper.companion.sync.AttuneSyncCodec
import com.attunehelper.companion.util.QrBitmaps
import com.attunehelper.companion.util.WinlatorIntents
import com.google.android.material.button.MaterialButton
import com.google.android.material.card.MaterialCardView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.journeyapps.barcodescanner.ScanContract
import com.journeyapps.barcodescanner.ScanIntentResult
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : AppCompatActivity() {
    private lateinit var store: AttuneHistoryStore
    private var addonEntries: List<AddonInstall.Entry> = emptyList()
    private var nfcAdapter: NfcAdapter? = null
    private var nfcPrepared: NdefMessage? = null

    private val pickTree = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { u: Uri? ->
        if (u == null) {
            return@registerForActivityResult
        }
        val f = (Intent.FLAG_GRANT_READ_URI_PERMISSION
            or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        try {
            contentResolver.takePersistableUriPermission(u, f)
        } catch (e: Exception) {
            Toast.makeText(this, "Could not keep folder access: ${e.message}", Toast.LENGTH_LONG).show()
        }
        store.setSynastriaTreeUri(u.toString())
        updateTreeLabel()
    }

    private val scanQr = registerForActivityResult(ScanContract()) { r: ScanIntentResult? ->
        val t = r?.contents?.trim() ?: return@registerForActivityResult
        applyIncomingToken(t)
    }

    private val requestCamera = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { ok: Boolean ->
        if (ok) {
            scanQr.launch(null)
        } else {
            Toast.makeText(this, "Camera permission is needed to scan a QR code.", Toast.LENGTH_LONG).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        store = AttuneHistoryStore(applicationContext)
        nfcAdapter = NfcAdapter.getDefaultAdapter(this)
        configureNfcSection()
        CredentialsStore.isStubAvailable()

        findViewById<MaterialButton>(R.id.btn_open_winlator).setOnClickListener {
            startActivity(
                Intent(
                    Intent.ACTION_VIEW,
                    Uri.parse("https://github.com/brunodev85/winlator")
                )
            )
        }
        findViewById<MaterialButton>(R.id.btn_pick_tree).setOnClickListener { pickTree.launch(null) }
        findViewById<MaterialButton>(R.id.btn_scan_snapshot).setOnClickListener { scanSnapshot() }
        findViewById<MaterialButton>(R.id.btn_import_code).setOnClickListener { importFromField() }
        findViewById<MaterialButton>(R.id.btn_export_code).setOnClickListener { exportToField() }
        findViewById<MaterialButton>(R.id.btn_copy_code).setOnClickListener { copyField() }
        findViewById<MaterialButton>(R.id.btn_qr_today).setOnClickListener { showTodayQr() }
        findViewById<MaterialButton>(R.id.btn_scan_qr).setOnClickListener { onScanQrClicked() }
        findViewById<MaterialButton>(R.id.btn_install_addon).setOnClickListener { installSelectedAddon() }
        findViewById<MaterialButton>(R.id.btn_play_winlator).setOnClickListener { openWinlator() }
        findViewById<MaterialButton>(R.id.btn_nfc_prepare).setOnClickListener { prepareNfcPush() }

        updateTreeLabel()
        refreshAttuneText()
        loadAddonSpinner()
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        if (NfcAdapter.ACTION_NDEF_DISCOVERED == intent.action) {
            deliverNfcPayloads(intent)
        }
    }

    private fun nfcDispatchPending(): PendingIntent {
        val i = Intent(this, javaClass).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP)
        val f = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_MUTABLE
        } else {
            PendingIntent.FLAG_UPDATE_CURRENT
        }
        return PendingIntent.getActivity(this, 0, i, f)
    }

    override fun onResume() {
        super.onResume()
        if (nfcAdapter == null) {
            return
        }
        NfcNdefPushCompat.set(nfcAdapter, nfcPrepared, this)
        if (nfcAdapter?.isEnabled != true) {
            return
        }
        val filters = arrayOf(
            IntentFilter(NfcAdapter.ACTION_NDEF_DISCOVERED).apply {
                try {
                    addDataType(AhcNfcHelper.MIME_AHC)
                    addDataType(AhcNfcHelper.MIME_LUA)
                } catch (e: Exception) {
                }
            }
        )
        try {
            nfcAdapter?.enableForegroundDispatch(this, nfcDispatchPending(), filters, null)
        } catch (e: Exception) {
        }
    }

    override fun onPause() {
        super.onPause()
        if (nfcAdapter == null) {
            return
        }
        try {
            nfcAdapter?.disableForegroundDispatch(this)
        } catch (e: Exception) {
        }
        NfcNdefPushCompat.set(nfcAdapter, null, this)
    }

    private fun configureNfcSection() {
        val c = findViewById<MaterialCardView>(R.id.card_nfc)
        if (nfcAdapter == null) {
            c.visibility = View.GONE
            return
        }
        c.visibility = View.VISIBLE
        if (nfcAdapter?.isEnabled != true) {
            findViewById<TextView>(R.id.text_nfc_status).setText(R.string.nfc_disabled)
        }
    }

    private fun prepareNfcPush() {
        if (nfcAdapter == null) {
            Toast.makeText(this, R.string.nfc_nfc_unavailable, Toast.LENGTH_LONG).show()
            return
        }
        if (nfcAdapter?.isEnabled != true) {
            Toast.makeText(this, R.string.nfc_disabled, Toast.LENGTH_LONG).show()
            return
        }
        if (store.getAll().isEmpty()) {
            Toast.makeText(this, R.string.nfc_nothing_to_send, Toast.LENGTH_LONG).show()
            return
        }
        findViewById<TextView>(R.id.text_nfc_status).setText(R.string.scanning)
        lifecycleScope.launch {
            val nfcMsg = withContext(Dispatchers.IO) {
                val all = store.getAll()
                val last = all.maxByOrNull { it.date }!!
                val full = AttuneSyncCodec.encodeFullHistory(all)
                val oneDay = AttuneSyncCodec.encodeOneDayQr(last)
                var lua: String? = null
                val s = store.lastAttuneHelperLuaDocumentUri()
                if (s != null) {
                    try {
                        val b = contentResolver.openInputStream(Uri.parse(s))?.use { st -> st.readBytes() }
                        if (b != null) {
                            lua = AttuneSyncCodec.encodeGzippedLuaFileOrNullForNfc(b)
                        }
                    } catch (e: Exception) {
                    }
                }
                AhcNfcHelper.buildPushNdef(full, oneDay, lua)
            }
            nfcPrepared = nfcMsg
            NfcNdefPushCompat.set(nfcAdapter, nfcMsg, this@MainActivity)
            findViewById<TextView>(R.id.text_nfc_status).setText(R.string.nfc_prepared)
        }
    }

    private fun deliverNfcPayloads(incoming: Intent) {
        val p = AhcNfcHelper.readAhcMimePayloadsFromIntent(incoming)
        if (p.isEmpty() || isFinishing || isDestroyed) {
            return
        }
        MaterialAlertDialogBuilder(this)
            .setTitle(R.string.nfc_import_dialog_title)
            .setMessage(R.string.nfc_import_dialog_message)
            .setNegativeButton(R.string.nfc_import_dialog_cancel, null)
            .setPositiveButton(R.string.nfc_import_dialog_import) { _, _ ->
                for (t in p) {
                    applyIncomingToken(t)
                }
            }
            .show()
    }

    private fun updateTreeLabel() {
        val t = findViewById<TextView>(R.id.text_tree_uri)
        val s = store.synastriaTreeUri()
        t.text = if (s == null) {
            getString(R.string.no_tree_yet)
        } else {
            getString(R.string.tree_selected_prefix) + s
        }
    }

    private fun scanSnapshot() {
        val s = store.synastriaTreeUri() ?: run {
            Toast.makeText(this, "Choose a Synastria folder first.", Toast.LENGTH_LONG).show()
            return
        }
        val u = Uri.parse(s)
        val st = findViewById<TextView>(R.id.text_scan_status)
        st.text = getString(R.string.scanning)
        lifecycleScope.launch {
            val res = withContext(Dispatchers.IO) {
                SynastriaFolder.findAttuneHelperSnapshot(this@MainActivity, u)
            }
            if (res.isSuccess) {
                val r = res.getOrNull()!!
                store.upsert(r.snapshot)
                store.setLastAttuneHelperLuaDocumentUri(r.attuneHelperLuaUri.toString())
                st.text = getString(R.string.scan_ok, r.snapshot.date)
                refreshAttuneText()
            } else {
                st.text = res.exceptionOrNull()?.message ?: "Scan failed"
            }
        }
    }

    private fun refreshAttuneText() {
        val lines = store.getAll()
        val t = findViewById<TextView>(R.id.text_attune_line)
        if (lines.isEmpty()) {
            t.text = getString(R.string.no_attune_yet)
        } else {
            val last = lines.takeLast(8).joinToString("\n") { s ->
                "${s.date}  acc=${s.account}  wf=${s.warforged}  lf=${s.lightforged}  tf=${s.titanforged}"
            }
            t.text = last
        }
    }

    private fun importFromField() {
        val raw = findViewById<android.widget.EditText>(R.id.edit_sync_code).text?.toString() ?: ""
        if (raw.isBlank()) {
            Toast.makeText(this, "Paste a code first.", Toast.LENGTH_SHORT).show()
            return
        }
        applyIncomingToken(raw.trim())
    }

    private fun applyIncomingToken(token: String) {
        val s = token.trim()
        val bulk = AttuneSyncCodec.decodeFullOrNull(s)
        if (bulk != null) {
            store.mergeIncoming(bulk)
            refreshAttuneText()
            Toast.makeText(this, "Merged ${bulk.size} day(s).", Toast.LENGTH_LONG).show()
            return
        }
        val fromLua = AttuneSyncCodec.decodeGzippedLuaToSnapshotOrNull(s)
        if (fromLua != null) {
            store.upsert(fromLua)
            refreshAttuneText()
            Toast.makeText(this, "Imported snapshot from AttuneHelper.lua (NFC) — ${fromLua.date}.", Toast.LENGTH_LONG).show()
            return
        }
        val qr = AttuneSyncCodec.decodeQrOrNull(s)
        if (qr != null) {
            store.upsert(qr)
            refreshAttuneText()
            Toast.makeText(this, "Imported snapshot for ${qr.date}.", Toast.LENGTH_LONG).show()
            return
        }
        Toast.makeText(this, "Unrecognized code format.", Toast.LENGTH_LONG).show()
    }

    private fun exportToField() {
        val e = AttuneSyncCodec.encodeFullHistory(store.getAll())
        findViewById<android.widget.EditText>(R.id.edit_sync_code).setText(e)
        Toast.makeText(this, "Full log encoded. Use Copy, or a QR is only for one day (button below).", Toast.LENGTH_LONG).show()
    }

    private fun copyField() {
        val t = findViewById<android.widget.EditText>(R.id.edit_sync_code).text?.toString() ?: ""
        if (t.isEmpty()) {
            Toast.makeText(this, "Nothing to copy.", Toast.LENGTH_SHORT).show()
            return
        }
        val cm = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        cm.setPrimaryClip(ClipData.newPlainText("attune sync", t))
        Toast.makeText(this, "Copied.", Toast.LENGTH_SHORT).show()
    }

    private fun showTodayQr() {
        val all = store.getAll()
        if (all.isEmpty()) {
            Toast.makeText(this, "No data yet. Scan a snapshot or import a code.", Toast.LENGTH_LONG).show()
            return
        }
        val last = all.maxByOrNull { it.date }!!
        val token = AttuneSyncCodec.encodeOneDayQr(last)
        val iv = findViewById<ImageView>(R.id.image_qr)
        val bmp = QrBitmaps.fromText(token, 720)
        iv.setImageBitmap(bmp)
        Toast.makeText(this, "QR encodes the latest day only: ${last.date}.", Toast.LENGTH_LONG).show()
    }

    private fun onScanQrClicked() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
            scanQr.launch(null)
        } else {
            requestCamera.launch(Manifest.permission.CAMERA)
        }
    }

    private fun loadAddonSpinner() {
        lifecycleScope.launch {
            val list = withContext(Dispatchers.IO) { AddonInstall.listEntries(this@MainActivity) }
            addonEntries = list
            val names = list.map { it.name }
            val ad = ArrayAdapter(
                this@MainActivity,
                android.R.layout.simple_spinner_item,
                names
            )
            ad.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
            findViewById<androidx.appcompat.widget.AppCompatSpinner>(R.id.spinner_addons).adapter = ad
        }
    }

    private fun installSelectedAddon() {
        val idx = findViewById<androidx.appcompat.widget.AppCompatSpinner>(R.id.spinner_addons).selectedItemPosition
        if (idx < 0 || idx >= addonEntries.size) {
            Toast.makeText(this, "Pick an add-on from the list.", Toast.LENGTH_LONG).show()
            return
        }
        val e = addonEntries[idx]
        val s = store.synastriaTreeUri() ?: run {
            Toast.makeText(this, "Choose Synastria folder first (write access for AddOns).", Toast.LENGTH_LONG).show()
            return
        }
        val u = Uri.parse(s)
        Toast.makeText(this, "Downloading… (may take a while)", Toast.LENGTH_LONG).show()
        lifecycleScope.launch {
            val r = withContext(Dispatchers.IO) { AddonInstall.installGitAddon(this@MainActivity, u, e) }
            if (r.isSuccess) {
                Toast.makeText(this@MainActivity, r.getOrNull(), Toast.LENGTH_LONG).show()
            } else {
                Toast.makeText(this@MainActivity, r.exceptionOrNull()?.message ?: "Install failed", Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun openWinlator() {
        val i = WinlatorIntents.tryLaunch(packageManager)
        if (i != null) {
            startActivity(i)
        } else {
            startActivity(WinlatorIntents.playStoreOrGitHubPage())
            Toast.makeText(
                this,
                "Winlator (com.winlator) not found. Opened the releases page — install, then return here.",
                Toast.LENGTH_LONG
            ).show()
        }
    }
}
