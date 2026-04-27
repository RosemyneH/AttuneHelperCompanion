package com.attunehelper.companion.addon

import android.content.Context
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import okhttp3.HttpUrl.Companion.toHttpUrlOrNull
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.util.UUID
import java.util.concurrent.TimeUnit
import java.util.zip.ZipFile

object AddonInstall {
    private const val MAX_DOWNLOAD_BYTES = 64L * 1024 * 1024
    private const val MAX_MANIFEST_BYTES = 2L * 1024 * 1024
    private const val MAX_EXTRACTED_TOTAL_BYTES = 256L * 1024 * 1024
    private const val MAX_SINGLE_ZIP_FILE_BYTES = 32L * 1024 * 1024
    private const val CATALOG_CACHE_TTL_MS = 24L * 60L * 60L * 1000L
    private const val REMOTE_MANIFEST_URL = "https://raw.githubusercontent.com/RosemyneH/AttuneHelperCompanion/master/manifest/addons.json"
    private const val MOBILE_CONTROLLER_CATEGORY = "Mobile & Controller"
    private const val HTTP_USER_AGENT = "AttuneHelperCompanion-Android/1.0"

    enum class InstallKind {
        GIT,
        DIRECT_ZIP,
    }

    data class Entry(
        val id: String,
        val name: String,
        val author: String,
        val source: String,
        val category: String,
        val categories: List<String>,
        val description: String,
        val version: String,
        val avatarUrl: String,
        val folder: String,
        val sourceSubdir: String,
        val installKind: InstallKind,
        val installUrl: String,
    )

    data class CatalogResult(
        val entries: List<Entry>,
        val sourceLabel: String,
    )

    @Volatile
    private var cached: CatalogResult? = null

    private val client = OkHttpClient.Builder()
        .callTimeout(3, TimeUnit.MINUTES)
        .readTimeout(3, TimeUnit.MINUTES)
        .build()

    fun listEntries(context: Context): List<Entry> {
        return loadCatalog(context).entries
    }

    fun loadCatalog(context: Context, forceRemoteRefresh: Boolean = false): CatalogResult {
        val hit = cached
        if (hit != null && !forceRemoteRefresh) {
            return hit
        }

        val cacheFile = File(context.filesDir, "addon_catalog_cache.json")
        var cacheFresh = cacheFile.isFile && isCatalogCacheFresh(cacheFile)
        if (!forceRemoteRefresh && cacheFresh) {
            parseCatalogFile(cacheFile)?.let {
                return rememberCatalog(CatalogResult(it, "Cached remote catalog"))
            }
            cacheFresh = false
        }

        if (forceRemoteRefresh || !cacheFresh) {
            fetchRemoteCatalog(context, cacheFile)?.let {
                return rememberCatalog(CatalogResult(it, "Remote catalog"))
            }
        }

        if (cacheFile.isFile) {
            parseCatalogFile(cacheFile)?.let {
                return rememberCatalog(CatalogResult(it, "Cached remote catalog"))
            }
        }

        val text = context.assets.open("addons.json").bufferedReader().use { it.readText() }
        return rememberCatalog(CatalogResult(parseEntries(text), "Bundled catalog"))
    }

    private fun rememberCatalog(result: CatalogResult): CatalogResult {
        cached = result
        return result
    }

    private fun isCatalogCacheFresh(cacheFile: File): Boolean {
        val age = System.currentTimeMillis() - cacheFile.lastModified()
        return age in 0..CATALOG_CACHE_TTL_MS
    }

    private fun parseCatalogFile(file: File): List<Entry>? {
        return try {
            parseEntries(file.readText())
        } catch (e: Exception) {
            null
        }
    }

    private fun fetchRemoteCatalog(context: Context, cacheFile: File): List<Entry>? {
        val h = REMOTE_MANIFEST_URL.toHttpUrlOrNull() ?: return null
        if (h.scheme != "https" || h.host != "raw.githubusercontent.com") {
            return null
        }
        val tmp = File(context.cacheDir, "addon-catalog-" + System.currentTimeMillis() + ".json")
        if (!httpDownloadToFile(h, tmp, MAX_MANIFEST_BYTES)) {
            tmp.delete()
            return null
        }
        return try {
            val entries = parseEntries(tmp.readText())
            tmp.copyTo(cacheFile, overwrite = true)
            entries
        } catch (e: Exception) {
            null
        } finally {
            tmp.delete()
        }
    }

    private fun parseEntries(text: String): List<Entry> {
        val o = JSONObject(text)
        val arr = o.optJSONArray("addons") ?: throw IllegalArgumentException("Manifest missing addons array.")
        val out = ArrayList<Entry>(arr.length())
        for (i in 0 until arr.length()) {
            val a = arr.optJSONObject(i) ?: continue
            val inst = a.optJSONObject("install") ?: continue
            val type = inst.optString("type", "")
            val u = inst.optString("url", "").trim()
            if (u.isEmpty()) {
                continue
            }
            val kind: InstallKind
            when (type) {
                "git" -> {
                    if (!u.contains("github.com")) {
                        continue
                    }
                    kind = InstallKind.GIT
                }
                "direct_zip" -> {
                    if (!u.startsWith("https://")) {
                        continue
                    }
                    kind = InstallKind.DIRECT_ZIP
                }
                else -> continue
            }
            out.add(
                Entry(
                    id = a.optString("id"),
                    name = a.optString("name"),
                    author = a.optString("author"),
                    source = a.optString("source"),
                    category = a.optString("category"),
                    categories = readStringArray(a.optJSONArray("categories")),
                    description = a.optString("description"),
                    version = a.optString("version"),
                    avatarUrl = a.optString("avatar_url"),
                    folder = a.optString("folder"),
                    sourceSubdir = a.optString("source_subdir", ""),
                    installKind = kind,
                    installUrl = u,
                )
            )
        }
        out.sortWith(
            compareBy<Entry> { if (it.hasCategory(MOBILE_CONTROLLER_CATEGORY)) 0 else 1 }
                .thenBy(String.CASE_INSENSITIVE_ORDER) { it.name }
        )
        if (out.isEmpty()) {
            throw IllegalArgumentException("Manifest contained no installable add-ons.")
        }
        return out
    }

    private fun Entry.hasCategory(value: String): Boolean {
        return category.equals(value, ignoreCase = true) ||
            categories.any { it.equals(value, ignoreCase = true) }
    }

    private fun readStringArray(arr: JSONArray?): List<String> {
        if (arr == null) {
            return emptyList()
        }
        val out = ArrayList<String>(arr.length())
        for (i in 0 until arr.length()) {
            val value = arr.optString(i).trim()
            if (value.isNotEmpty()) {
                out.add(value)
            }
        }
        return out
    }

    fun codeloadZipUrl(gitUrl: String, branch: String): String? {
        val u = gitUrl.trim()
        val m = Regex("https?://github\\.com/([^/]+)/(.+?)(?:\\.git)?$").find(u) ?: return null
        var repo = m.groupValues[2]
        if (repo.endsWith(".git")) {
            repo = repo.removeSuffix(".git")
        }
        return "https://codeload.github.com/${m.groupValues[1]}/$repo/zip/refs/heads/$branch"
    }

    fun installAddon(
        context: Context,
        synastriaTree: Uri,
        e: Entry,
    ): Result<String> {
        return when (e.installKind) {
            InstallKind.GIT -> installFromGitZip(context, synastriaTree, e)
            InstallKind.DIRECT_ZIP -> installFromHttpZip(context, synastriaTree, e)
        }
    }

    private fun installFromGitZip(
        context: Context,
        synastriaTree: Uri,
        e: Entry,
    ): Result<String> {
        if (e.installKind != InstallKind.GIT) {
            return Result.failure(IllegalStateException("Not a git add-on."))
        }
        val addOns = getOrCreateInterfaceAddOns(context, synastriaTree) ?: return Result.failure(
            IllegalStateException("Interface/AddOns not available (grant storage access, pick Synastria).")
        )
        val work = File(context.cacheDir, "unzip-" + UUID.randomUUID())
        work.deleteRecursively()
        if (!work.mkdirs()) {
            return Result.failure(IllegalStateException("Cache mkdir"))
        }
        return try {
            val zipF = downloadGitZipToCache(context, e)
                ?: return Result.failure(
                    IllegalStateException("Download failed (try again; repo may use a branch other than main/master).")
                )
            try {
                unzipToDir(zipF, work)
            } finally {
                zipF.delete()
            }
            val srcDir = findAddonSourceDir(work, e) ?: return Result.failure(
                IllegalStateException("Folder \"${e.folder}\" not found in the downloaded archive.")
            )
            copyAddonFolder(context, addOns, e, srcDir)
        } catch (e2: Exception) {
            Result.failure(e2)
        } finally {
            work.deleteRecursively()
        }
    }

    private fun installFromHttpZip(
        context: Context,
        synastriaTree: Uri,
        e: Entry,
    ): Result<String> {
        if (e.installKind != InstallKind.DIRECT_ZIP) {
            return Result.failure(IllegalStateException("Not a direct zip add-on."))
        }
        val addOns = getOrCreateInterfaceAddOns(context, synastriaTree) ?: return Result.failure(
            IllegalStateException("Interface/AddOns not available (grant storage access, pick Synastria).")
        )
        val work = File(context.cacheDir, "unzip-" + UUID.randomUUID())
        work.deleteRecursively()
        if (!work.mkdirs()) {
            return Result.failure(IllegalStateException("Cache mkdir"))
        }
        return try {
            val zipF = downloadHttpZipToCache(context, e.installUrl)
                ?: return Result.failure(
                    IllegalStateException("Download failed (check connection and URL).")
                )
            try {
                unzipToDir(zipF, work)
            } finally {
                zipF.delete()
            }
            val srcDir = findAddonSourceDir(work, e) ?: return Result.failure(
                IllegalStateException("Folder \"${e.folder}\" not found in the downloaded zip.")
            )
            copyAddonFolder(context, addOns, e, srcDir)
        } catch (e2: Exception) {
            Result.failure(e2)
        } finally {
            work.deleteRecursively()
        }
    }

    private fun copyAddonFolder(
        context: Context,
        addOns: DocumentFile,
        e: Entry,
        srcDir: File,
    ): Result<String> {
        var target: DocumentFile = addOns.findFile(e.folder) ?: addOns.createDirectory(e.folder)
            ?: return Result.failure(IllegalStateException("Could not use AddOns/${e.folder}"))
        for (c in target.listFiles().orEmpty()) {
            c.delete()
        }
        copyFileTreeToDocument(context, srcDir, target)
        return Result.success("Installed: Interface/AddOns/${e.folder}/")
    }

    private fun getOrCreateInterfaceAddOns(context: Context, tree: Uri): DocumentFile? {
        val base = DocumentFile.fromTreeUri(context, tree) ?: return null
        var iface: DocumentFile? = null
        for (c in base.listFiles().orEmpty()) {
            if (c.isDirectory && c.name == "Interface") {
                iface = c
                break
            }
        }
        val intDir = iface ?: base.createDirectory("Interface") ?: return null
        for (c in intDir.listFiles().orEmpty()) {
            if (c.isDirectory && c.name == "AddOns") {
                return c
            }
        }
        return intDir.createDirectory("AddOns")
    }

    private fun downloadGitZipToCache(context: Context, e: Entry): File? {
        val uMain = codeloadZipUrl(e.installUrl, "main")?.toHttpUrlOrNull() ?: return null
        val out = File(context.cacheDir, "addon-" + e.id + "-" + System.currentTimeMillis() + ".zip")
        if (httpDownloadToFile(uMain, out) && out.length() > 64) {
            return out
        }
        out.delete()
        val uMaster = codeloadZipUrl(e.installUrl, "master")?.toHttpUrlOrNull() ?: return null
        if (httpDownloadToFile(uMaster, out) && out.length() > 64) {
            return out
        }
        out.delete()
        return null
    }

    private fun downloadHttpZipToCache(context: Context, zipUrl: String): File? {
        val h = zipUrl.toHttpUrlOrNull() ?: return null
        if (h.scheme != "https") {
            return null
        }
        val out = File(context.cacheDir, "addon-" + System.currentTimeMillis() + ".zip")
        if (httpDownloadToFile(h, out) && out.length() > 64) {
            return out
        }
        out.delete()
        return null
    }

    private fun httpDownloadToFile(h: okhttp3.HttpUrl, out: File, maxBytes: Long = MAX_DOWNLOAD_BYTES): Boolean {
        return try {
            out.delete()
            val req = Request.Builder()
                .url(h)
                .header("User-Agent", HTTP_USER_AGENT)
                .build()
            var ok = false
            client.newCall(req).execute().use { r ->
                if (!r.isSuccessful) {
                    return@use
                }
                r.body?.byteStream()?.use { ins ->
                    FileOutputStream(out).use { o ->
                        val buf = ByteArray(16384)
                        var total = 0L
                        while (true) {
                            val n = ins.read(buf)
                            if (n == -1) {
                                ok = true
                                break
                            }
                            if (total + n > maxBytes) {
                                out.delete()
                                return@use
                            }
                            o.write(buf, 0, n)
                            total += n
                        }
                    }
                }
            }
            ok && out.length() > 64
        } catch (e: Exception) {
            out.delete()
            false
        }
    }

    private fun isSafeZipEntryName(name: String): Boolean {
        if (name.isEmpty()) {
            return false
        }
        for (c in name) {
            if (c.code < 32) {
                return false
            }
        }
        val n = name.replace('\\', '/')
        if (n.startsWith("/") || n.contains(":/") || n.startsWith("../") || n.trim() == "..") {
            return false
        }
        for (p in n.split("/")) {
            if (p == ".." || p == "~") {
                return false
            }
        }
        return true
    }

    private fun isFileWithinRoot(root: File, candidate: File): Boolean {
        return try {
            val rc = root.canonicalFile
            val cc = candidate.canonicalFile
            val prefix = rc.path + File.separator
            val cp = cc.path
            cp == rc.path || cp.startsWith(prefix)
        } catch (_: IOException) {
            false
        }
    }

    private fun copyZipEntryWithCaps(
        ins: InputStream,
        out: OutputStream,
        declaredUncompressed: Long,
        totalWritten: LongArray,
    ) {
        val perFile = when {
            declaredUncompressed < 0 -> MAX_SINGLE_ZIP_FILE_BYTES
            declaredUncompressed == 0L -> MAX_SINGLE_ZIP_FILE_BYTES
            else -> minOf(declaredUncompressed, MAX_SINGLE_ZIP_FILE_BYTES)
        }
        val buf = ByteArray(8192)
        var fileBytes = 0L
        while (fileBytes < perFile) {
            val remTotal = MAX_EXTRACTED_TOTAL_BYTES - totalWritten[0]
            if (remTotal <= 0L) {
                break
            }
            val toRead = minOf(buf.size.toLong(), perFile - fileBytes, remTotal).toInt()
            if (toRead <= 0) {
                break
            }
            val n = ins.read(buf, 0, toRead)
            if (n == -1) {
                break
            }
            if (n == 0) {
                continue
            }
            out.write(buf, 0, n)
            fileBytes += n
            totalWritten[0] = totalWritten[0] + n
        }
    }

    private fun unzipToDir(zip: File, dest: File) {
        if (!dest.exists() && !dest.mkdirs()) {
            throw IOException("unzip target mkdir")
        }
        val root = dest.canonicalFile
        val totalWritten = longArrayOf(0L)
        ZipFile(zip).use { zf ->
            val ens = zf.entries()
            while (ens.hasMoreElements()) {
                if (totalWritten[0] >= MAX_EXTRACTED_TOTAL_BYTES) {
                    throw IOException("Unzip size limit")
                }
                val en = ens.nextElement()
                if (!isSafeZipEntryName(en.name)) {
                    continue
                }
                val t = File(dest, en.name)
                if (!isFileWithinRoot(root, t)) {
                    continue
                }
                if (en.isDirectory) {
                    t.mkdirs()
                } else {
                    t.parentFile?.mkdirs()
                    if (en.size > MAX_SINGLE_ZIP_FILE_BYTES && en.size > 0) {
                        continue
                    }
                    zf.getInputStream(en).use { ins ->
                        FileOutputStream(t).use { o ->
                            copyZipEntryWithCaps(
                                ins,
                                o,
                                if (en.size > 0) en.size else -1,
                                totalWritten,
                            )
                        }
                    }
                }
            }
        }
    }

    private fun findAddonSourceDir(work: File, e: Entry): File? {
        if (e.sourceSubdir.isNotEmpty()) {
            val subRoot = work.walkTopDown().firstOrNull { it.isDirectory && it.name == e.sourceSubdir }
            if (subRoot != null) {
                val a = subRoot.walkTopDown().firstOrNull { it.isDirectory && it.name == e.folder }
                if (a != null) {
                    return a
                }
            }
        }
        return work.walkTopDown().firstOrNull { it.isDirectory && it.name == e.folder }
    }

    private fun copyFileTreeToDocument(context: Context, src: File, dest: DocumentFile) {
        for (c in src.listFiles().orEmpty()) {
            if (c.isDirectory) {
                val ch = dest.createDirectory(c.name) ?: continue
                copyFileTreeToDocument(context, c, ch)
            } else {
                for (e in dest.listFiles().orEmpty()) {
                    if (e.name == c.name) {
                        e.delete()
                    }
                }
                val f = dest.createFile("application/octet-stream", c.name) ?: continue
                context.contentResolver.openOutputStream(f.uri).use { os ->
                    if (os == null) {
                        return@use
                    }
                    FileInputStream(c).use { it.copyTo(os) }
                }
            }
        }
    }
}
