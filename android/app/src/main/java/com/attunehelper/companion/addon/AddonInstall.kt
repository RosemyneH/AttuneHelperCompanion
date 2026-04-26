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
import java.util.UUID
import java.util.concurrent.TimeUnit
import java.util.zip.ZipFile

object AddonInstall {
    data class Entry(
        val id: String,
        val name: String,
        val folder: String,
        val sourceSubdir: String,
        val gitUrl: String,
    )

    @Volatile
    private var cached: List<Entry>? = null

    private val client = OkHttpClient.Builder()
        .callTimeout(3, TimeUnit.MINUTES)
        .readTimeout(3, TimeUnit.MINUTES)
        .build()

    fun listEntries(context: Context): List<Entry> {
        val hit = cached
        if (hit != null) {
            return hit
        }
        val text = context.assets.open("addons.json").bufferedReader().use { it.readText() }
        val o = JSONObject(text)
        val arr = o.optJSONArray("addons") ?: JSONArray()
        val out = ArrayList<Entry>(arr.length())
        for (i in 0 until arr.length()) {
            val a = arr.optJSONObject(i) ?: continue
            val inst = a.optJSONObject("install") ?: continue
            if (inst.optString("type", "") != "git") {
                continue
            }
            val u = inst.optString("url", "")
            if (u.isEmpty() || !u.contains("github.com")) {
                continue
            }
            out.add(
                Entry(
                    id = a.optString("id"),
                    name = a.optString("name"),
                    folder = a.optString("folder"),
                    sourceSubdir = a.optString("source_subdir", ""),
                    gitUrl = u,
                )
            )
        }
        out.sortBy { it.name }
        cached = out
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

    fun installGitAddon(
        context: Context,
        synastriaTree: Uri,
        e: Entry,
    ): Result<String> {
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
                IllegalStateException("Folder “${e.folder}” not found in the downloaded archive.")
            )
            var target: DocumentFile = addOns.findFile(e.folder) ?: addOns.createDirectory(e.folder)
                ?: return Result.failure(IllegalStateException("Could not use AddOns/${e.folder}"))
            for (c in target.listFiles().orEmpty()) {
                c.delete()
            }
            copyFileTreeToDocument(context, srcDir, target)
            Result.success("Installed: Interface/AddOns/${e.folder}/")
        } catch (e2: Exception) {
            Result.failure(e2)
        } finally {
            work.deleteRecursively()
        }
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
        val uMain = codeloadZipUrl(e.gitUrl, "main")?.toHttpUrlOrNull() ?: return null
        val out = File(context.cacheDir, "addon-" + e.id + "-" + System.currentTimeMillis() + ".zip")
        if (httpDownloadToFile(uMain, out) && out.length() > 64) {
            return out
        }
        out.delete()
        val uMaster = codeloadZipUrl(e.gitUrl, "master")?.toHttpUrlOrNull() ?: return null
        if (httpDownloadToFile(uMaster, out) && out.length() > 64) {
            return out
        }
        out.delete()
        return null
    }

    private fun httpDownloadToFile(h: okhttp3.HttpUrl, out: File): Boolean {
        return try {
            val req = Request.Builder().url(h).build()
            client.newCall(req).execute().use { r ->
                if (!r.isSuccessful) {
                    return false
                }
                r.body?.byteStream()?.use { ins ->
                    FileOutputStream(out).use { ins.copyTo(it) }
                } ?: return false
            }
            true
        } catch (e: Exception) {
            false
        }
    }

    private fun unzipToDir(zip: File, dest: File) {
        ZipFile(zip).use { zf ->
            val ens = zf.entries()
            while (ens.hasMoreElements()) {
                val en = ens.nextElement()
                val t = File(dest, en.name)
                if (en.isDirectory) {
                    t.mkdirs()
                } else {
                    t.parentFile?.mkdirs()
                    zf.getInputStream(en).use { ins ->
                        FileOutputStream(t).use { o -> ins.copyTo(o) }
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
