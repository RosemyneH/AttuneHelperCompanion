package com.attunehelper.companion

import android.Manifest
import android.app.PendingIntent
import android.content.ActivityNotFoundException
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
import android.os.Handler
import android.os.Looper
import android.text.Editable
import android.text.SpannableString
import android.text.Spanned
import android.text.TextWatcher
import android.text.style.ForegroundColorSpan
import android.view.View
import com.google.android.material.chip.Chip
import com.google.android.material.chip.ChipGroup
import android.widget.EditText
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.R as AppCompatR
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.attunehelper.companion.addon.AddonInstall
import com.attunehelper.companion.data.AttuneHistoryStore
import com.attunehelper.companion.nfc.AhcNfcHelper
import com.attunehelper.companion.nfc.NfcNdefPushCompat
import com.attunehelper.companion.saf.SynastriaFolder
import com.attunehelper.companion.sync.AttuneSyncCodec
import com.attunehelper.companion.attune.graph.AttuneGraphView
import com.attunehelper.companion.attune.graph.GraphDisplay
import com.attunehelper.companion.attune.graph.GraphMetric
import com.attunehelper.companion.attune.graph.attuneGraphAverageText
import com.attunehelper.companion.attune.graph.attuneGraphDetailText
import com.attunehelper.companion.attune.graph.attuneGraphLatestDetailText
import com.attunehelper.companion.attune.graph.attuneSyncLatestText
import com.attunehelper.companion.util.QrBitmaps
import com.attunehelper.companion.util.WinlatorIntents
import com.google.android.material.button.MaterialButton
import com.google.android.material.card.MaterialCardView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.navigationrail.NavigationRailView
import com.google.android.material.textfield.TextInputEditText
import com.google.zxing.BarcodeFormat
import com.journeyapps.barcodescanner.ScanContract
import com.journeyapps.barcodescanner.ScanIntentResult
import com.journeyapps.barcodescanner.ScanOptions
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : AppCompatActivity() {
    private lateinit var store: AttuneHistoryStore
    private var addonEntries: List<AddonInstall.Entry> = emptyList()
    private var selectedAddonEntry: AddonInstall.Entry? = null
    private var selectedAddonCategory: String = ""
    private var addonInstalling = false
    private var addonCatalogLoading = false
    private var addonCatalogSourceLabel = ""
    private var nfcAdapter: NfcAdapter? = null
    private var nfcPrepared: NdefMessage? = null
    private var ignoreRailSelection: Boolean = false
    private var categoryChipListenerPaused: Boolean = false
    private var addonSearchBlobByEntryIndex: List<String> = emptyList()
    private lateinit var attuneGraph: AttuneGraphView
    private lateinit var textAttuneGraphDetail: TextView
    private lateinit var textAttuneGraphAverage: TextView
    private lateinit var textSyncLatest: TextView
    private val mainHandler: Handler = Handler(Looper.getMainLooper())
    private val debouncedRenderAddonCatalog: Runnable = Runnable {
        if (isFinishing) {
            return@Runnable
        }
        renderAddonCatalog()
    }

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
        if (isFinishing || isDestroyed) {
            return@registerForActivityResult
        }
        val t = r?.contents?.trim() ?: return@registerForActivityResult
        if (t.isEmpty()) {
            return@registerForActivityResult
        }
        try {
            applyIncomingToken(t)
        } catch (e: Exception) {
            Toast.makeText(
                this,
                "Could not import code: ${e.message ?: e.javaClass.simpleName}",
                Toast.LENGTH_LONG
            ).show()
        }
    }

    private val requestCamera = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { ok: Boolean ->
        if (ok) {
            launchQrScan()
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

        setupNavigationRail()
        setupAddonSearch()
        findViewById<MaterialButton>(R.id.btn_open_winlator).setOnClickListener {
            try {
                startActivity(
                    Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse("https://github.com/brunodev85/winlator")
                    )
                )
            } catch (e: ActivityNotFoundException) {
                Toast.makeText(
                    this,
                    "No app can open this link. Install a browser.",
                    Toast.LENGTH_LONG,
                ).show()
            }
        }
        findViewById<MaterialButton>(R.id.btn_pick_tree).setOnClickListener { pickTree.launch(null) }
        findViewById<MaterialButton>(R.id.btn_scan_snapshot).setOnClickListener { scanSnapshot() }
        findViewById<MaterialButton>(R.id.btn_import_code).setOnClickListener { importFromField() }
        findViewById<MaterialButton>(R.id.btn_export_code).setOnClickListener { exportToField() }
        findViewById<MaterialButton>(R.id.btn_copy_code).setOnClickListener { copyField() }
        findViewById<MaterialButton>(R.id.btn_qr_today).setOnClickListener { showTodayQr() }
        findViewById<MaterialButton>(R.id.btn_qr_multi).setOnClickListener { showMultiDayQr() }
        findViewById<MaterialButton>(R.id.btn_scan_qr).setOnClickListener { onScanQrClicked() }
        findViewById<MaterialButton>(R.id.btn_play_winlator).setOnClickListener { openWinlator() }
        findViewById<MaterialButton>(R.id.btn_nfc_prepare).setOnClickListener { prepareNfcPush() }
        findViewById<MaterialButton>(R.id.btn_attunes_open_sync).setOnClickListener { showSection(R.id.section_sync) }
        findViewById<MaterialButton>(R.id.btn_refresh_catalog).setOnClickListener { loadAddonCatalog(forceRemoteRefresh = true) }
        attuneGraph = findViewById(R.id.attune_graph)
        textAttuneGraphDetail = findViewById(R.id.text_attune_graph_detail)
        textAttuneGraphAverage = findViewById(R.id.text_attune_graph_average)
        textSyncLatest = findViewById(R.id.text_sync_latest)
        setupAttuneGraphChips()
        attuneGraph.setOnAttuneGraphSelectionListener { _: Int? -> updateAttuneGraphDetail() }

        showSection(R.id.section_attunes)
        updateTreeLabel()
        refreshAttuneText()
        loadAddonCatalog()
    }

    override fun onDestroy() {
        mainHandler.removeCallbacks(debouncedRenderAddonCatalog)
        super.onDestroy()
    }

    private fun setupNavigationRail() {
        findViewById<NavigationRailView>(R.id.navigation_rail).setOnItemSelectedListener { item ->
            if (ignoreRailSelection) {
                return@setOnItemSelectedListener true
            }
            when (item.itemId) {
                R.id.nav_menu_attunes -> {
                    showSection(R.id.section_attunes)
                    true
                }
                R.id.nav_menu_catalog -> {
                    showSection(R.id.section_catalog)
                    true
                }
                R.id.nav_menu_sync -> {
                    showSection(R.id.section_sync)
                    true
                }
                R.id.nav_menu_play -> {
                    showSection(R.id.section_play)
                    true
                }
                else -> false
            }
        }
    }

    private fun showSection(sectionId: Int) {
        val attunesHost = findViewById<View>(R.id.section_attunes)
        val mainScroll = findViewById<ScrollView>(R.id.main_scroll)
        if (sectionId == R.id.section_attunes) {
            attunesHost.visibility = View.VISIBLE
            mainScroll.visibility = View.GONE
        } else {
            attunesHost.visibility = View.GONE
            mainScroll.visibility = View.VISIBLE
        }
        val scrollSections = listOf(
            R.id.section_catalog,
            R.id.section_sync,
            R.id.section_play,
        )
        for (id in scrollSections) {
            findViewById<View>(id).visibility = if (id == sectionId) View.VISIBLE else View.GONE
        }
        if (sectionId != R.id.section_sync) {
            findViewById<ImageView>(R.id.image_qr).setImageDrawable(null)
        }
        val menuId = when (sectionId) {
            R.id.section_attunes -> R.id.nav_menu_attunes
            R.id.section_catalog -> R.id.nav_menu_catalog
            R.id.section_sync -> R.id.nav_menu_sync
            R.id.section_play -> R.id.nav_menu_play
            else -> R.id.nav_menu_attunes
        }
        val rail = findViewById<NavigationRailView>(R.id.navigation_rail)
        if (rail.selectedItemId != menuId) {
            ignoreRailSelection = true
            try {
                rail.selectedItemId = menuId
            } finally {
                ignoreRailSelection = false
            }
        }
        mainScroll.post {
            if (mainScroll.isShown) {
                mainScroll.smoothScrollTo(0, 0)
            }
        }
    }

    private fun setupAddonSearch() {
        findViewById<TextInputEditText>(R.id.edit_addon_search).addTextChangedListener(
            object : TextWatcher {
                override fun beforeTextChanged(s: CharSequence?, start: Int, count: Int, after: Int) = Unit
                override fun onTextChanged(s: CharSequence?, start: Int, before: Int, count: Int) = Unit
                override fun afterTextChanged(s: Editable?) {
                    mainHandler.removeCallbacks(debouncedRenderAddonCatalog)
                    mainHandler.postDelayed(debouncedRenderAddonCatalog, 220L)
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
            badge.visibility = View.VISIBLE
        } else {
            t.text = getString(R.string.tree_selected_prefix) + s
            badge.visibility = View.GONE
        }
    }

    private fun scanSnapshot() {
        val s = store.synastriaTreeUri() ?: run {
            Toast.makeText(this, "Choose a Synastria folder first (Play → Library).", Toast.LENGTH_LONG).show()
            showSection(R.id.section_play)
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

    private fun setupAttuneGraphChips() {
        val gMetric = findViewById<ChipGroup>(R.id.chip_group_attune_metric)
        val gDisp = findViewById<ChipGroup>(R.id.chip_group_attune_display)
        gMetric.setOnCheckedStateChangeListener { _, checkedIds ->
            if (checkedIds.isEmpty()) {
                return@setOnCheckedStateChangeListener
            }
            when (checkedIds[0]) {
                R.id.chip_attune_m_account -> attuneGraph.graphMetric = GraphMetric.ACCOUNT
                R.id.chip_attune_m_tf -> attuneGraph.graphMetric = GraphMetric.TITANFORGED
                R.id.chip_attune_m_wf -> attuneGraph.graphMetric = GraphMetric.WARFORGED
                R.id.chip_attune_m_lf -> attuneGraph.graphMetric = GraphMetric.LIGHTFORGED
            }
            updateAttuneGraphDetail()
        }
        gDisp.setOnCheckedStateChangeListener { _, checkedIds ->
            if (checkedIds.isEmpty()) {
                return@setOnCheckedStateChangeListener
            }
            when (checkedIds[0]) {
                R.id.chip_attune_d_bars -> attuneGraph.graphDisplay = GraphDisplay.BARS
                R.id.chip_attune_d_plot -> attuneGraph.graphDisplay = GraphDisplay.PLOT
            }
            updateAttuneGraphDetail()
        }
    }

    private fun updateAttuneGraphDetail() {
        val idx = attuneGraph.selectedIndex
        val rows = attuneGraph.getDisplayRows()
        val detail = if (idx == null || idx !in rows.indices) {
            attuneGraphLatestDetailText(rows, attuneGraph.graphMetric)
        } else {
            attuneGraphDetailText(rows, idx, attuneGraph.graphMetric)
        }
        textAttuneGraphDetail.text = colorAttuneSummary(detail)
        textAttuneGraphAverage.text = attuneGraphAverageText(rows)
    }

    private fun refreshAttuneText() {
        val all = store.getAll()
        attuneGraph.setSnapshots(all)
        textSyncLatest.text = colorSyncLatestSummary(attuneSyncLatestText(all))
        updateAttuneGraphDetail()
        updateAttunesIntroCardVisibility()
    }

    private fun colorAttuneSummary(raw: String): SpannableString {
        val span = SpannableString(raw)
        var start = 0
        while (start <= raw.length) {
            val end = raw.indexOf('\n', start).let { if (it >= 0) it else raw.length }
            val line = raw.substring(start, end)
            val color = when {
                line == "Latest snapshot" -> R.color.ahc_gold
                line.firstOrNull()?.isDigit() == true -> R.color.ahc_cyan
                line.startsWith("New account") || line.startsWith("Account:") -> R.color.ahc_accent
                line.startsWith("New TF") || line.startsWith("TF") -> R.color.ahc_tf
                line.startsWith("New WF") || line.startsWith("WF") -> R.color.ahc_wf
                line.startsWith("New LF") || line.startsWith("LF") -> R.color.ahc_lf
                else -> 0
            }
            if (color != 0 && end > start) {
                span.setSpan(ForegroundColorSpan(getColor(color)), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
            }
            if (end == raw.length) {
                break
            }
            start = end + 1
        }
        return span
    }

    private fun colorSyncLatestSummary(raw: String): SpannableString {
        val span = SpannableString(raw)
        colorRange(span, raw, "Latest snapshot", R.color.ahc_cyan)
        colorDelimitedSegment(span, raw, "Account ", R.color.ahc_accent)
        colorDelimitedSegment(span, raw, "TF ", R.color.ahc_tf)
        colorDelimitedSegment(span, raw, "WF ", R.color.ahc_wf)
        colorDelimitedSegment(span, raw, "LF ", R.color.ahc_lf)
        return span
    }

    private fun colorDelimitedSegment(span: SpannableString, raw: String, marker: String, color: Int) {
        val start = raw.indexOf(marker)
        if (start < 0) {
            return
        }
        val end = raw.indexOf(" \u00b7 ", start).let { if (it >= 0) it else raw.length }
        span.setSpan(ForegroundColorSpan(getColor(color)), start, end, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
    }

    private fun colorRange(span: SpannableString, raw: String, marker: String, color: Int) {
        val start = raw.indexOf(marker)
        if (start < 0) {
            return
        }
        span.setSpan(ForegroundColorSpan(getColor(color)), start, start + marker.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
    }

    private fun localDateYmdUs(): String {
        val c = java.util.Calendar.getInstance()
        return String.format(
            java.util.Locale.US,
            "%04d-%02d-%02d",
            c.get(java.util.Calendar.YEAR),
            c.get(java.util.Calendar.MONTH) + 1,
            c.get(java.util.Calendar.DAY_OF_MONTH),
        )
    }

    private fun hasAttuneRowForLocalToday(): Boolean {
        val today = localDateYmdUs()
        return store.getAll().any { it.date == today }
    }

    private fun updateAttunesIntroCardVisibility() {
        val intro = findViewById<MaterialCardView>(R.id.card_attunes_intro)
        intro.visibility = if (hasAttuneRowForLocalToday()) View.GONE else View.VISIBLE
        findViewById<MaterialCardView>(R.id.card_attunes_graph).requestLayout()
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
        val fromQr = AttuneSyncCodec.decodeQrOrNull(s)
        if (fromQr != null) {
            store.upsert(fromQr)
            refreshAttuneText()
            Toast.makeText(this, "Imported snapshot for ${fromQr.date} (AHC-Q1).", Toast.LENGTH_LONG).show()
            return
        }
        val denseQr = AttuneSyncCodec.decodeMultiDayQrOrNull(s)
        if (denseQr != null) {
            store.mergeIncoming(denseQr)
            refreshAttuneText()
            Toast.makeText(this, "Merged ${denseQr.size} day(s) from dense AHC-Q2 QR.", Toast.LENGTH_LONG).show()
            return
        }
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
        Toast.makeText(this, "Unrecognized code format.", Toast.LENGTH_LONG).show()
    }

    private fun exportToField() {
        val e = AttuneSyncCodec.encodeFullHistory(store.getAll())
        findViewById<android.widget.EditText>(R.id.edit_sync_code).setText(e)
        Toast.makeText(this, "Full AHC1 log encoded for paste/import. QR buttons are 1-day easy scan or dense multi-day.", Toast.LENGTH_LONG).show()
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
        iv.setImageDrawable(null)
        val bmp = QrBitmaps.fromText(token, 512)
        iv.setImageBitmap(bmp)
        iv.setMaxHeight(dp(360))
        Toast.makeText(this, "AHC-Q1 ready: one easy-scan day (${last.date}).", Toast.LENGTH_LONG).show()
    }

    private fun showMultiDayQr() {
        val all = store.getAll()
        if (all.isEmpty()) {
            Toast.makeText(this, "No data yet. Scan a snapshot or import a code.", Toast.LENGTH_LONG).show()
            return
        }
        val token = AttuneSyncCodec.encodeMultiDayQr(all)
        if (token == null) {
            Toast.makeText(this, "Could not build dense multi-day QR from this history.", Toast.LENGTH_LONG).show()
            return
        }
        val days = AttuneSyncCodec.decodeMultiDayQrOrNull(token)?.size ?: 0
        val iv = findViewById<ImageView>(R.id.image_qr)
        iv.setImageDrawable(null)
        val bmp = QrBitmaps.fromText(token, 768)
        iv.setImageBitmap(bmp)
        iv.setMaxHeight(dp(420))
        Toast.makeText(this, "AHC-Q2 ready: $days day(s), dense QR. Use good light and a steady hand.", Toast.LENGTH_LONG).show()
    }

    private fun buildScanOptions(): ScanOptions {
        return ScanOptions()
            .setDesiredBarcodeFormats(listOf(BarcodeFormat.QR_CODE.name))
            .setBeepEnabled(false)
            .setOrientationLocked(false)
            .setPrompt(getString(R.string.scan_qr_prompt))
    }

    private fun launchQrScan() {
        if (!packageManager.hasSystemFeature(PackageManager.FEATURE_CAMERA_ANY)) {
            Toast.makeText(this, "This device has no camera.", Toast.LENGTH_LONG).show()
            return
        }
        val opts = buildScanOptions()
        try {
            scanQr.launch(opts)
        } catch (e: Exception) {
            Toast.makeText(
                this,
                "Could not open the scanner: ${e.message ?: e.javaClass.simpleName}",
                Toast.LENGTH_LONG,
            ).show()
        }
    }

    private fun onScanQrClicked() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
            launchQrScan()
        } else {
            requestCamera.launch(Manifest.permission.CAMERA)
        }
    }

    private fun loadAddonCatalog(forceRemoteRefresh: Boolean = false) {
        addonCatalogLoading = true
        renderAddonCatalog()
        lifecycleScope.launch {
            try {
                val result = withContext(Dispatchers.IO) {
                    AddonInstall.loadCatalog(this@MainActivity, forceRemoteRefresh)
                }
                val blobs = withContext(Dispatchers.Default) {
                    List(result.entries.size) { i -> precomputedSearchBlobForAddon(result.entries[i]) }
                }
                addonEntries = result.entries
                addonCatalogSourceLabel = result.sourceLabel
                addonSearchBlobByEntryIndex = blobs
                selectedAddonEntry = result.entries.firstOrNull()
                renderCategoryFilters()
                if (forceRemoteRefresh) {
                    Toast.makeText(this@MainActivity, getString(R.string.catalog_refreshed), Toast.LENGTH_SHORT).show()
                }
            } catch (e: Exception) {
                Toast.makeText(
                    this@MainActivity,
                    "Could not load add-on catalog: ${e.message ?: e.javaClass.simpleName}",
                    Toast.LENGTH_LONG,
                ).show()
            } finally {
                addonCatalogLoading = false
                renderAddonCatalog()
            }
        }
    }

    private fun renderCategoryFilters() {
        val chipGroup = findViewById<ChipGroup>(R.id.layout_category_filters)
        categoryChipListenerPaused = true
        chipGroup.setOnCheckedStateChangeListener(null)
        chipGroup.removeAllViews()
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
        var checkId = View.NO_ID
        // ʕ •ᴥ•ʔ Filter chips use 36dp min height (below 40dp a11y ideal) for density; row scrolls horizontally.
        for (category in allCategories) {
            val chip = Chip(this, null, com.google.android.material.R.attr.chipStyle)
            chip.text = if (category.isEmpty()) getString(R.string.catalog_all) else category
            chip.tag = category
            chip.isCheckable = true
            chip.id = View.generateViewId()
            chip.isCheckedIconVisible = false
            if (selectedAddonCategory == category) {
                checkId = chip.id
            }
            chipGroup.addView(chip)
        }
        if (checkId == View.NO_ID && chipGroup.childCount > 0) {
            val first = chipGroup.getChildAt(0) as Chip
            checkId = first.id
            selectedAddonCategory = first.tag as? String ?: ""
        }
        if (checkId != View.NO_ID) {
            chipGroup.check(checkId)
        }
        chipGroup.setOnCheckedStateChangeListener { g, checkedIds ->
            if (categoryChipListenerPaused) {
                return@setOnCheckedStateChangeListener
            }
            if (checkedIds.isEmpty()) {
                return@setOnCheckedStateChangeListener
            }
            val chip = g.findViewById<Chip>(checkedIds[0])
            selectedAddonCategory = chip.tag as? String ?: ""
            renderAddonCatalog()
        }
        categoryChipListenerPaused = false
    }

    private fun renderAddonCatalog() {
        val host = findViewById<LinearLayout>(R.id.layout_addon_catalog)
        val empty = findViewById<TextView>(R.id.text_addon_empty)
        val count = findViewById<TextView>(R.id.text_catalog_count)
        host.removeAllViews()
        val filtered = filteredAddonEntries()
        val source = addonCatalogSourceLabel.ifBlank { getString(R.string.catalog_source_loading) }
        count.text = getString(R.string.catalog_count_source, filtered.size, source)
        findViewById<MaterialButton>(R.id.btn_refresh_catalog).isEnabled = !addonCatalogLoading
        empty.visibility = if (filtered.isEmpty()) View.VISIBLE else View.GONE
        for (entry in filtered) {
            host.addView(createAddonCard(entry))
        }
        if (selectedAddonEntry != null && selectedAddonEntry !in addonEntries) {
            selectedAddonEntry = filtered.firstOrNull()
        }
    }

    private fun filteredAddonEntries(): List<AddonInstall.Entry> {
        val query = findViewById<TextInputEditText>(R.id.edit_addon_search).text?.toString()?.trim()?.lowercase().orEmpty()
        if (addonEntries.isEmpty()) {
            return emptyList()
        }
        val blobs = addonSearchBlobByEntryIndex
        return addonEntries.mapIndexedNotNull { i, entry ->
            val categoryMatch = selectedAddonCategory.isEmpty() ||
                entry.category.equals(selectedAddonCategory, ignoreCase = true) ||
                entry.categories.any { it.equals(selectedAddonCategory, ignoreCase = true) }
            if (!categoryMatch) {
                return@mapIndexedNotNull null
            }
            if (query.isNotEmpty()) {
                val blob = blobs.getOrNull(i) ?: return@mapIndexedNotNull null
                if (!blob.contains(query)) {
                    return@mapIndexedNotNull null
                }
            }
            entry
        }
    }

    private fun precomputedSearchBlobForAddon(e: AddonInstall.Entry): String {
        return buildString {
            append(e.name)
            append('\u0000')
            append(e.author)
            append('\u0000')
            append(e.source)
            append('\u0000')
            append(e.id)
            append('\u0000')
            append(e.category)
            append('\u0000')
            append(e.description)
            append('\u0000')
            append(e.folder)
            append('\u0000')
            append(e.version)
            for (c in e.categories) {
                append('\u0000')
                append(c)
            }
        }.lowercase()
    }

    private fun createAddonCard(entry: AddonInstall.Entry): View {
        val selected = selectedAddonEntry?.id == entry.id
        val card = MaterialCardView(this)
        card.useCompatPadding = false
        val cPad = dp(1)
        card.setContentPadding(cPad, cPad, cPad, cPad)
        card.radius = resources.getDimension(R.dimen.corner_card_list)
        card.cardElevation = resources.getDimension(R.dimen.elevation_list_card)
        card.strokeWidth = dp(if (selected) 2 else 1)
        card.strokeColor = getColor(if (selected) R.color.ahc_accent else R.color.ahc_outline)
        card.setCardBackgroundColor(getColor(if (selected) R.color.ahc_surface_high else R.color.ahc_surface))
        card.isClickable = true
        card.isFocusable = true
        card.setOnClickListener {
            selectedAddonEntry = entry
            renderAddonCatalog()
        }

        val root = LinearLayout(this)
        root.orientation = LinearLayout.VERTICAL
        root.setPadding(dp(6), dp(2), dp(6), dp(2))

        val topRow = LinearLayout(this)
        topRow.orientation = LinearLayout.HORIZONTAL

        val icon = ImageView(this)
        icon.setImageResource(R.drawable.ic_addon_plugin)
        icon.setPadding(0, 0, dp(6), 0)
        icon.contentDescription = entry.name
        icon.imageTintList = ColorStateList.valueOf(getColor(R.color.ahc_accent))
        topRow.addView(
            icon,
            LinearLayout.LayoutParams(dp(40), dp(40)).apply {
                gravity = android.view.Gravity.TOP
            }
        )

        val body = LinearLayout(this)
        body.orientation = LinearLayout.VERTICAL
        val bodyLp = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
        val nameTv = textView(entry.name, 15.5f, R.color.ahc_text, true)
        nameTv.maxLines = 6
        nameTv.layoutParams = LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT,
        )
        body.addView(nameTv)
        if (entry.version.isNotBlank()) {
            body.addView(
                textView("v" + entry.version, 12f, R.color.ahc_text_muted).apply {
                    setPadding(0, dp(1), 0, 0)
                }
            )
        }
        val cat = displayCategory(entry)
        if (cat.isNotBlank()) {
            body.addView(
                textView(cat, 11.5f, R.color.ahc_text_muted).apply {
                    setPadding(0, dp(1), 0, 0)
                    maxLines = 3
                }
            )
        }
        body.addView(
            textView(
                entry.description.ifBlank { getString(R.string.addon_description_empty) },
                13.5f,
                R.color.ahc_text,
            ).apply {
                setPadding(0, dp(2), 0, 0)
                maxLines = 8
            }
        )
        topRow.addView(body, bodyLp)
        root.addView(topRow)

        val actionRow = LinearLayout(this)
        actionRow.orientation = LinearLayout.HORIZONTAL
        actionRow.gravity = android.view.Gravity.END
        val installBtn = MaterialButton(
            this,
            null,
            com.google.android.material.R.attr.materialButtonOutlinedStyle,
        )
        installBtn.text = getString(R.string.addon_install)
        installBtn.isAllCaps = false
        installBtn.minWidth = 0
        installBtn.minHeight = dp(40)
        installBtn.setPadding(dp(8), 0, dp(8), 0)
        installBtn.strokeColor = ColorStateList.valueOf(getColor(R.color.ahc_accent))
        installBtn.setTextColor(getColor(R.color.ahc_accent))
        installBtn.isEnabled = !addonInstalling
        installBtn.setOnClickListener { installAddon(entry) }
        actionRow.addView(installBtn)
        if (entry.installUrl.isNotBlank() && (entry.installUrl.startsWith("http://", true) || entry.installUrl.startsWith("https://", true))) {
            val copyBtn = MaterialButton(this, null, AppCompatR.attr.borderlessButtonStyle)
            copyBtn.text = getString(R.string.addon_copy_link)
            copyBtn.isAllCaps = false
            copyBtn.minHeight = dp(36)
            copyBtn.setTextColor(getColor(R.color.ahc_text_muted))
            copyBtn.setTextSize(12f)
            copyBtn.setOnClickListener { copyAddonUrl(entry.installUrl) }
            val lp2 = LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT)
            lp2.marginStart = dp(4)
            copyBtn.layoutParams = lp2
            actionRow.addView(copyBtn)
        }
        val arLp = LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT)
        arLp.topMargin = dp(0)
        root.addView(actionRow, arLp)

        card.addView(root)
        return card.apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT,
            ).apply {
                topMargin = 0
            }
        }
    }

    private fun copyAddonUrl(url: String) {
        val cm = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        cm.setPrimaryClip(ClipData.newPlainText("add-on", url))
        Toast.makeText(this, R.string.addon_copied_link, Toast.LENGTH_SHORT).show()
    }

    private fun installAddon(entry: AddonInstall.Entry) {
        val s = store.synastriaTreeUri() ?: run {
            Toast.makeText(this, "Choose Synastria folder first (Play → Library).", Toast.LENGTH_LONG).show()
            showSection(R.id.section_play)
            return
        }
        val u = Uri.parse(s)
        val status = findViewById<TextView>(R.id.text_addon_status)
        selectedAddonEntry = entry
        addonInstalling = true
        status.visibility = View.VISIBLE
        status.setTextColor(getColor(R.color.ahc_accent))
        status.text = getString(R.string.catalog_installing, entry.name)
        renderAddonCatalog()
        lifecycleScope.launch {
            val r = withContext(Dispatchers.IO) { AddonInstall.installAddon(this@MainActivity, u, entry) }
            addonInstalling = false
            if (r.isSuccess) {
                status.setTextColor(getColor(R.color.ahc_success))
                status.text = r.getOrNull()
                Toast.makeText(this@MainActivity, r.getOrNull(), Toast.LENGTH_LONG).show()
            } else {
                val message = r.exceptionOrNull()?.message ?: "Install failed"
                status.setTextColor(getColor(R.color.ahc_error))
                status.text = message
                Toast.makeText(this@MainActivity, message, Toast.LENGTH_LONG).show()
            }
            renderAddonCatalog()
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
            try {
                startActivity(i)
            } catch (e: ActivityNotFoundException) {
                Toast.makeText(
                    this,
                    e.message ?: "Could not start Winlator.",
                    Toast.LENGTH_LONG,
                ).show()
            }
        } else {
            try {
                startActivity(WinlatorIntents.playStoreOrGitHubPage())
            } catch (e: ActivityNotFoundException) {
                Toast.makeText(
                    this,
                    "No app can open the Winlator page. Install a browser and try again.",
                    Toast.LENGTH_LONG,
                ).show()
                return
            }
            Toast.makeText(
                this,
                "Winlator (com.winlator) not found. Opened the releases page — install, then return here.",
                Toast.LENGTH_LONG
            ).show()
        }
    }
}
