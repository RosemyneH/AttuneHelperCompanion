package com.attunehelper.companion

import android.Manifest
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.widget.ArrayAdapter
import android.widget.ImageView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.widget.doAfterTextChanged
import androidx.lifecycle.lifecycleScope
import com.attunehelper.companion.addon.AddonInstall
import com.attunehelper.companion.data.AttuneHistoryStore
import com.attunehelper.companion.saf.SynastriaFolder
import com.attunehelper.companion.sync.AttuneSyncCodec
import com.attunehelper.companion.util.QrBitmaps
import com.attunehelper.companion.util.WinlatorIntents
import com.google.android.material.button.MaterialButton
import com.journeyapps.barcodescanner.ScanContract
import com.journeyapps.barcodescanner.ScanIntentResult
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : AppCompatActivity() {
    private lateinit var store: AttuneHistoryStore
    private var addonEntries: List<AddonInstall.Entry> = emptyList()

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

        findViewById<android.widget.EditText>(R.id.edit_sync_code).doAfterTextChanged { }

        updateTreeLabel()
        refreshAttuneText()
        loadAddonSpinner()
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
                val sn = res.getOrNull()!!.snapshot
                store.upsert(sn)
                st.text = getString(R.string.scan_ok, sn.date)
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
        val qr = AttuneSyncCodec.decodeQrOrNull(s)
        if (qr != null) {
            store.upsert(qr)
            refreshAttuneText()
            Toast.makeText(this, "Imported snapshot for ${qr.date}.", Toast.LENGTH_LONG).show()
            return
        }
        val bulk = AttuneSyncCodec.decodeFullOrNull(s)
        if (bulk != null) {
            store.mergeIncoming(bulk)
            refreshAttuneText()
            Toast.makeText(this, "Merged ${bulk.size} day(s).", Toast.LENGTH_LONG).show()
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
