package com.attunehelper.companion

import android.Manifest
import android.app.PendingIntent
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.res.ColorStateList
import android.content.pm.PackageManager
import android.graphics.Typeface
import android.net.Uri
import android.nfc.NdefMessage
import android.nfc.NfcAdapter
import android.os.Build
import android.os.Bundle
import android.text.Editable
import android.text.TextWatcher
import android.view.View
import android.widget.EditText
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
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
    private var selectedAddonEntry: AddonInstall.Entry? = null
    private var selectedAddonCategory: String = ""
    private var addonInstalling = false
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

        setupBottomNavigation()
        setupAddonSearch()
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

        showSection(R.id.section_catalog)
        updateTreeLabel()
        refreshAttuneText()
        loadAddonCatalog()
    }

    private fun setupBottomNavigation() {
        findViewById<MaterialButton>(R.id.btn_nav_catalog).setOnClickListener { showSection(R.id.section_catalog) }
        findViewById<MaterialButton>(R.id.btn_nav_sync).setOnClickListener { showSection(R.id.section_sync) }
        findViewById<MaterialButton>(R.id.btn_nav_transfer).setOnClickListener { showSection(R.id.section_transfer) }
        findViewById<MaterialButton>(R.id.btn_nav_play).setOnClickListener { showSection(R.id.section_play) }
    }

    private fun showSection(sectionId: Int) {
        val sections = listOf(
            R.id.section_catalog,
            R.id.section_sync,
            R.id.section_transfer,
            R.id.section_play,
        )
        for (id in sections) {
            findViewById<View>(id).visibility = if (id == sectionId) View.VISIBLE else View.GONE
        }
        val nav = mapOf(
            R.id.btn_nav_catalog to R.id.section_catalog,
            R.id.btn_nav_sync to R.id.section_sync,
            R.id.btn_nav_transfer to R.id.section_transfer,
            R.id.btn_nav_play to R.id.section_play,
        )
        for ((buttonId, targetId) in nav) {
            setNavButtonSelected(findViewById(buttonId), targetId == sectionId)
        }
        findViewById<ScrollView>(R.id.main_scroll).post {
            findViewById<ScrollView>(R.id.main_scroll).smoothScrollTo(0, 0)
        }
    }

    private fun setNavButtonSelected(button: MaterialButton, selected: Boolean) {
        val bg = if (selected) R.color.pumpkin_primary else R.color.pumpkin_surface_high
        val fg = if (selected) R.color.pumpkin_on_primary else R.color.pumpkin_secondary_soft
        button.backgroundTintList = ColorStateList.valueOf(getColor(bg))
        button.setTextColor(getColor(fg))
    }

    private fun setupAddonSearch() {
        findViewById<EditText>(R.id.edit_addon_search).addTextChangedListener(
            object : TextWatcher {
                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) = Unit
                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) = Unit
                override fun afterTextChanged(s: Editable?) {
                    renderAddonCatalog()
                }
            }
        )
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
            c.visibility = View.VISIBLE
            findViewById<MaterialButton>(R.id.btn_nfc_prepare).isEnabled = false
            findViewById<TextView>(R.id.text_nfc_status).setText(R.string.nfc_nfc_unavailable)
            return
        }
        c.visibility = View.VISIBLE
        if (nfcAdapter?.isEnabled != true) {
            findViewById<MaterialButton>(R.id.btn_nfc_prepare).isEnabled = false
            findViewById<TextView>(R.id.text_nfc_status).setText(R.string.nfc_disabled)
        } else {
            findViewById<MaterialButton>(R.id.btn_nfc_prepare).isEnabled = true
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
        val badge = findViewById<TextView>(R.id.text_tree_badge)
        val s = store.synastriaTreeUri()
        if (s == null) {
            t.text = getString(R.string.no_tree_yet)
            badge.setText(R.string.tree_missing_short)
        } else {
            t.text = getString(R.string.tree_selected_prefix) + s
            badge.setText(R.string.tree_ready_short)
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

    private fun loadAddonCatalog() {
        lifecycleScope.launch {
            val list = withContext(Dispatchers.IO) { AddonInstall.listEntries(this@MainActivity) }
            addonEntries = list
            selectedAddonEntry = list.firstOrNull()
            renderCategoryFilters()
            renderAddonCatalog()
            updateSelectedAddonPanel()
        }
    }

    private fun renderCategoryFilters() {
        val host = findViewById<LinearLayout>(R.id.layout_category_filters)
        host.removeAllViews()
        val categories = addonEntries
            .flatMap { entry ->
                val values = ArrayList<String>()
                if (entry.category.isNotBlank()) {
                    values.add(entry.category)
                }
                values.addAll(entry.categories)
                values
            }
            .map { it.trim() }
            .filter { it.isNotEmpty() }
            .distinct()
            .sorted()
        val allCategories = listOf("") + categories
        for (category in allCategories) {
            val button = MaterialButton(this)
            button.text = if (category.isEmpty()) getString(R.string.catalog_all) else category
            button.minHeight = dp(44)
            button.cornerRadius = dp(18)
            button.isAllCaps = false
            button.backgroundTintList = ColorStateList.valueOf(
                getColor(if (selectedAddonCategory == category) R.color.pumpkin_primary else R.color.pumpkin_surface_variant)
            )
            button.setTextColor(
                getColor(if (selectedAddonCategory == category) R.color.pumpkin_on_primary else R.color.pumpkin_text)
            )
            button.setOnClickListener {
                selectedAddonCategory = category
                renderCategoryFilters()
                renderAddonCatalog()
            }
            host.addView(
                button,
                LinearLayout.LayoutParams(
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT,
                ).apply {
                    marginEnd = dp(8)
                }
            )
        }
    }

    private fun renderAddonCatalog() {
        val host = findViewById<LinearLayout>(R.id.layout_addon_catalog)
        val empty = findViewById<TextView>(R.id.text_addon_empty)
        val count = findViewById<TextView>(R.id.text_catalog_count)
        host.removeAllViews()
        val filtered = filteredAddonEntries()
        count.text = getString(R.string.catalog_count, filtered.size)
        empty.visibility = if (filtered.isEmpty()) View.VISIBLE else View.GONE
        for (entry in filtered) {
            host.addView(createAddonCard(entry))
        }
        if (selectedAddonEntry != null && selectedAddonEntry !in addonEntries) {
            selectedAddonEntry = filtered.firstOrNull()
            updateSelectedAddonPanel()
        }
    }

    private fun filteredAddonEntries(): List<AddonInstall.Entry> {
        val query = findViewById<EditText>(R.id.edit_addon_search).text?.toString()?.trim()?.lowercase().orEmpty()
        return addonEntries.filter { entry ->
            val categoryMatch = selectedAddonCategory.isEmpty() ||
                entry.category.equals(selectedAddonCategory, ignoreCase = true) ||
                entry.categories.any { it.equals(selectedAddonCategory, ignoreCase = true) }
            val queryMatch = query.isEmpty() || listOf(
                entry.name,
                entry.author,
                entry.category,
                entry.description,
                entry.folder,
                entry.version,
            ).any { it.lowercase().contains(query) }
            categoryMatch && queryMatch
        }
    }

    private fun createAddonCard(entry: AddonInstall.Entry): View {
        val selected = selectedAddonEntry?.id == entry.id
        val card = MaterialCardView(this)
        card.radius = dp(22).toFloat()
        card.strokeWidth = dp(if (selected) 2 else 1)
        card.strokeColor = getColor(if (selected) R.color.pumpkin_primary else R.color.pumpkin_outline)
        card.setCardBackgroundColor(getColor(if (selected) R.color.pumpkin_surface_high else R.color.pumpkin_surface))
        card.isClickable = true
        card.isFocusable = true
        card.setOnClickListener {
            selectedAddonEntry = entry
            updateSelectedAddonPanel()
            renderAddonCatalog()
        }

        val body = LinearLayout(this)
        body.orientation = LinearLayout.VERTICAL
        body.setPadding(dp(16), dp(14), dp(16), dp(14))

        body.addView(textView(entry.name, 18f, R.color.pumpkin_text, true))
        body.addView(
            textView(
                getString(R.string.addon_meta, displayAuthor(entry), displayCategory(entry)),
                13f,
                R.color.pumpkin_primary,
            ).apply {
                setPadding(0, dp(4), 0, 0)
            }
        )
        body.addView(
            textView(
                entry.description.ifBlank { getString(R.string.addon_description_empty) },
                14f,
                R.color.pumpkin_text_muted,
            ).apply {
                maxLines = 2
                setPadding(0, dp(6), 0, 0)
            }
        )

        card.addView(body)
        return card.apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT,
            ).apply {
                topMargin = dp(8)
            }
        }
    }

    private fun updateSelectedAddonPanel() {
        val entry = selectedAddonEntry
        val button = findViewById<MaterialButton>(R.id.btn_install_addon)
        val name = findViewById<TextView>(R.id.text_addon_selected_name)
        val meta = findViewById<TextView>(R.id.text_addon_selected_meta)
        val description = findViewById<TextView>(R.id.text_addon_selected_description)
        if (entry == null) {
            name.setText(R.string.catalog_install_ready)
            meta.text = ""
            description.text = ""
            button.isEnabled = false
            return
        }
        name.text = getString(R.string.catalog_selected, entry.name)
        val version = if (entry.version.isBlank()) "" else " - " + getString(R.string.addon_version, entry.version)
        meta.text = getString(R.string.addon_meta, displayAuthor(entry), displayCategory(entry)) +
            version + "\n" + getString(R.string.addon_folder, entry.folder)
        description.text = entry.description.ifBlank { getString(R.string.addon_description_empty) }
        button.isEnabled = !addonInstalling
    }

    private fun installSelectedAddon() {
        val e = selectedAddonEntry
        if (e == null) {
            Toast.makeText(this, R.string.catalog_pick_first, Toast.LENGTH_LONG).show()
            return
        }
        val s = store.synastriaTreeUri() ?: run {
            Toast.makeText(this, "Choose Synastria folder first (write access for AddOns).", Toast.LENGTH_LONG).show()
            showSection(R.id.section_sync)
            return
        }
        val u = Uri.parse(s)
        val status = findViewById<TextView>(R.id.text_addon_status)
        val button = findViewById<MaterialButton>(R.id.btn_install_addon)
        addonInstalling = true
        button.isEnabled = false
        status.setTextColor(getColor(R.color.pumpkin_primary))
        status.text = getString(R.string.catalog_installing, e.name)
        lifecycleScope.launch {
            val r = withContext(Dispatchers.IO) { AddonInstall.installGitAddon(this@MainActivity, u, e) }
            addonInstalling = false
            button.isEnabled = true
            if (r.isSuccess) {
                status.setTextColor(getColor(R.color.pumpkin_success))
                status.text = r.getOrNull()
                Toast.makeText(this@MainActivity, r.getOrNull(), Toast.LENGTH_LONG).show()
            } else {
                val message = r.exceptionOrNull()?.message ?: "Install failed"
                status.setTextColor(getColor(R.color.pumpkin_error))
                status.text = message
                Toast.makeText(this@MainActivity, message, Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun displayAuthor(entry: AddonInstall.Entry): String {
        return entry.author.ifBlank { getString(R.string.addon_author_unknown) }
    }

    private fun displayCategory(entry: AddonInstall.Entry): String {
        return entry.category.ifBlank { entry.categories.firstOrNull().orEmpty() }
            .ifBlank { getString(R.string.addon_category_unknown) }
    }

    private fun textView(text: String, sizeSp: Float, color: Int, bold: Boolean = false): TextView {
        return TextView(this).apply {
            this.text = text
            textSize = sizeSp
            if (bold) {
                typeface = Typeface.DEFAULT_BOLD
            }
            setTextColor(getColor(color))
        }
    }

    private fun dp(value: Int): Int {
        return (value * resources.displayMetrics.density).toInt()
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
