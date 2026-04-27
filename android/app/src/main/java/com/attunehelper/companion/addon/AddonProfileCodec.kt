package com.attunehelper.companion.addon

import android.util.Base64
import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.nio.charset.StandardCharsets
import java.util.zip.GZIPInputStream
import java.util.zip.GZIPOutputStream

object AddonProfileCodec {
    const val PREFIX = "AHC-P1:"
    private const val MAX_B64_CHARS = 256 * 1024
    private const val MAX_JSON_BYTES = 128 * 1024

    data class Profile(
        val name: String,
        val addonIds: List<String>,
        val description: String = "",
        val author: String = "",
        val tags: List<String> = emptyList(),
    )

    fun encode(profile: Profile): String {
        val root = JSONObject()
        root.put("v", 1)
        root.put("name", profile.name.ifBlank { "Add-on Profile" })
        if (profile.description.isNotBlank()) {
            root.put("description", profile.description)
        }
        if (profile.author.isNotBlank()) {
            root.put("author", profile.author)
        }
        root.put("addon_ids", JSONArray(profile.addonIds.distinct()))
        if (profile.tags.isNotEmpty()) {
            root.put("tags", JSONArray(profile.tags))
        }
        val gz = gzip(root.toString().toByteArray(StandardCharsets.UTF_8))
        return PREFIX + Base64.encodeToString(gz, Base64.NO_WRAP or Base64.URL_SAFE)
    }

    fun decodeOrNull(code: String): Profile? {
        var text = code.trim()
        if (!text.startsWith(PREFIX)) {
            return null
        }
        text = text.removePrefix(PREFIX).filter { !it.isWhitespace() }
        if (text.isEmpty() || text.length > MAX_B64_CHARS) {
            return null
        }
        return try {
            val gz = Base64.decode(text, Base64.NO_WRAP or Base64.URL_SAFE)
            val json = ungzip(gz, MAX_JSON_BYTES).toString(StandardCharsets.UTF_8)
            parseProfile(JSONObject(json))
        } catch (e: Exception) {
            null
        }
    }

    fun parseProfile(root: JSONObject): Profile? {
        if (root.optInt("v") != 1) {
            return null
        }
        val ids = readStringArray(root.optJSONArray("addon_ids")).distinct()
        if (ids.isEmpty()) {
            return null
        }
        return Profile(
            name = root.optString("name", "Imported Profile").ifBlank { "Imported Profile" },
            addonIds = ids,
            description = root.optString("description", ""),
            author = root.optString("author", ""),
            tags = readStringArray(root.optJSONArray("tags")),
        )
    }

    fun readStringArray(arr: JSONArray?): List<String> {
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

    private fun gzip(bytes: ByteArray): ByteArray {
        val out = ByteArrayOutputStream()
        GZIPOutputStream(out).use { it.write(bytes) }
        return out.toByteArray()
    }

    private fun ungzip(bytes: ByteArray, maxBytes: Int): ByteArray {
        val out = ByteArrayOutputStream()
        GZIPInputStream(ByteArrayInputStream(bytes)).use { input ->
            val buf = ByteArray(8192)
            while (true) {
                val n = input.read(buf)
                if (n == -1) {
                    break
                }
                if (out.size() + n > maxBytes) {
                    throw IllegalArgumentException("Profile code is too large.")
                }
                out.write(buf, 0, n)
            }
        }
        return out.toByteArray()
    }
}
