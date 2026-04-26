package com.attunehelper.companion.saf

import android.content.Context
import android.content.ContentResolver
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import com.attunehelper.companion.attune.AttuneHelperLuaParser
import com.attunehelper.companion.attune.AttuneSnapshot
import java.io.InputStream
import java.util.ArrayDeque

object SynastriaFolder {
    private const val MAX_FILES = 8000
    private const val MAX_BYTES = 8 * 1024 * 1024

    data class ScanResult(
        val snapshot: AttuneSnapshot,
        val relativeNote: String,
        val attuneHelperLuaUri: Uri,
    )

    fun findAttuneHelperSnapshot(
        context: Context,
        treeUri: Uri,
    ): Result<ScanResult> {
        val root = DocumentFile.fromTreeUri(context, treeUri) ?: return Result.failure(
            IllegalStateException("Not a storage tree")
        )
        val resolver: ContentResolver = context.contentResolver
        var steps = 0
        var triedNamed = 0
        val q: ArrayDeque<DocumentFile> = ArrayDeque()
        q.add(root)
        while (q.isNotEmpty() && steps < MAX_FILES) {
            val d = q.removeFirst()
            steps++
            if (!d.isDirectory) {
                if (d.name == "AttuneHelper.lua") {
                    triedNamed++
                    val r = readAndParse(resolver, d, "AttuneHelper.lua")
                    if (r.isSuccess) {
                        return r
                    }
                }
                continue
            }
            for (c in d.listFiles().orEmpty()) {
                if (c.isDirectory) {
                    q.add(c)
                } else if (c.name == "AttuneHelper.lua") {
                    triedNamed++
                    val r = readAndParse(
                        resolver,
                        c,
                        "…/AttuneHelper.lua",
                    )
                    if (r.isSuccess) {
                        return r
                    }
                }
            }
        }
        if (triedNamed > 0) {
            return Result.failure(
                IllegalStateException(
                    "Found AttuneHelper.lua but could not read DailyAttuneSnapshot (file layout or size).",
                )
            )
        }
        return Result.failure(
            IllegalStateException(
                "AttuneHelper.lua not found under the selected folder (max $MAX_FILES items scanned).",
            )
        )
    }

    private fun readAndParse(
        resolver: ContentResolver,
        f: DocumentFile,
        rel: String,
    ): Result<ScanResult> {
        return try {
            val bytes = readAll(resolver, f.uri)
            val s = AttuneHelperLuaParser.parse(bytes)
                ?: return Result.failure(
                    IllegalStateException("Found AttuneHelper.lua but could not read DailyAttuneSnapshot.")
                )
            Result.success(ScanResult(s, rel, f.uri))
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    private fun readAll(resolver: ContentResolver, uri: Uri): ByteArray {
        val input: InputStream = resolver.openInputStream(uri) ?: throw IllegalStateException("openInputStream")
        return input.use { ins ->
            val b = ins.readBytes()
            if (b.size > MAX_BYTES) {
                throw IllegalStateException("File too large")
            }
            b
        }
    }
}
