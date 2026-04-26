package com.attunehelper.companion.sync

import android.util.Base64
import com.attunehelper.companion.attune.AttuneHelperLuaParser
import com.attunehelper.companion.attune.AttuneSnapshot
import org.json.JSONArray
import org.json.JSONObject
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.nio.charset.StandardCharsets
import java.util.zip.GZIPInputStream
import java.util.zip.GZIPOutputStream

object AttuneSyncCodec {
    const val BULK_PREFIX = "AHC1:"
    const val LUA_PREFIX = "AHC-LUA1:"
    private const val QR_PREFIX = "AHC-Q1:"

    private const val MAX_NFC_LUA_UTF8 = 6 * 1024

    private const val MAX_BULK_B64_CHARS = 2_000_000
    private const val MAX_BULK_GZIP_BYTES = 1 shl 20
    private const val MAX_BULK_JSON_UNCOMPRESSED = 1024 * 1024

    private const val MAX_LUA_B64_CHARS = 2_000_000
    private const val MAX_LUA_GZIP_BYTES = 1 shl 20
    private const val MAX_LUA_UNCOMPRESSED = 512 * 1024

    fun encodeFullHistory(snapshots: List<AttuneSnapshot>): String {
        val a = jsonArray(snapshots)
        val o = JSONObject()
        o.put("v", 1)
        o.put("snapshots", a)
        val body = o.toString().toByteArray(StandardCharsets.UTF_8)
        val gz = gzip(body)
        val b64 = Base64.encodeToString(gz, Base64.NO_WRAP or Base64.URL_SAFE)
        return BULK_PREFIX + b64
    }

    fun decodeFullOrNull(token: String): List<AttuneSnapshot>? {
        var s = token.trim()
        if (!s.startsWith(BULK_PREFIX)) {
            return null
        }
        s = s.removePrefix(BULK_PREFIX).filter { !it.isWhitespace() }
        if (s.length > MAX_BULK_B64_CHARS) {
            return null
        }
        return try {
            val gz = Base64.decode(s, Base64.NO_WRAP or Base64.URL_SAFE)
            val un = ungzipWithLimit(gz, maxDecompressed = MAX_BULK_JSON_UNCOMPRESSED, maxCompressed = MAX_BULK_GZIP_BYTES)
                ?: return null
            val json = un.toString(StandardCharsets.UTF_8)
            val o = JSONObject(json)
            if (o.optInt("v") != 1) {
                return null
            }
            fromJsonArray(o.optJSONArray("snapshots") ?: return null)
        } catch (e: Exception) {
            null
        }
    }

    fun encodeOneDayQr(s: AttuneSnapshot): String =
        "$QR_PREFIX${s.date}|${s.account}|${s.warforged}|${s.lightforged}|${s.titanforged}"

    fun encodeGzippedLuaFileOrNullForNfc(fileBytes: ByteArray): String? {
        if (fileBytes.isEmpty() || fileBytes.size > 512 * 1024) {
            return null
        }
        val gz = gzip(fileBytes)
        val b64 = Base64.encodeToString(gz, Base64.NO_WRAP or Base64.URL_SAFE)
        val s = LUA_PREFIX + b64
        if (s.toByteArray(StandardCharsets.UTF_8).size > MAX_NFC_LUA_UTF8) {
            return null
        }
        return s
    }

    fun decodeGzippedLuaToSnapshotOrNull(s: String): AttuneSnapshot? {
        var t = s.trim()
        if (!t.startsWith(LUA_PREFIX)) {
            return null
        }
        t = t.removePrefix(LUA_PREFIX).filter { !it.isWhitespace() }
        if (t.length > MAX_LUA_B64_CHARS) {
            return null
        }
        return try {
            val raw = Base64.decode(t, Base64.NO_WRAP or Base64.URL_SAFE)
            val lua = ungzipWithLimit(raw, maxDecompressed = MAX_LUA_UNCOMPRESSED, maxCompressed = MAX_LUA_GZIP_BYTES)
                ?: return null
            AttuneHelperLuaParser.parse(lua) ?: return null
        } catch (e: Exception) {
            null
        }
    }

    fun decodeQrOrNull(s: String): AttuneSnapshot? {
        var t = s.trim()
        if (t.startsWith("\uFEFF")) {
            t = t.substring(1).trim()
        }
        if (!t.startsWith(QR_PREFIX)) {
            return null
        }
        t = t.removePrefix(QR_PREFIX).trim()
        val parts = t.split('|').map { it.trim() }
        if (parts.size != 5) {
            return null
        }
        if (parts.any { it.isEmpty() }) {
            return null
        }
        return AttuneSnapshot(
            date = parts[0],
            account = parts[1].toIntOrNull() ?: return null,
            warforged = parts[2].toIntOrNull() ?: return null,
            lightforged = parts[3].toIntOrNull() ?: return null,
            titanforged = parts[4].toIntOrNull() ?: return null,
        )
    }

    private fun fromJsonArray(a: JSONArray): List<AttuneSnapshot> {
        val out = ArrayList<AttuneSnapshot>(a.length())
        for (i in 0 until a.length()) {
            val o = a.optJSONObject(i) ?: continue
            val d = o.optString("date", "")
            if (d.isEmpty()) {
                continue
            }
            out.add(
                AttuneSnapshot(
                    date = d,
                    account = o.optInt("account"),
                    warforged = o.optInt("warforged"),
                    lightforged = o.optInt("lightforged"),
                    titanforged = o.optInt("titanforged"),
                )
            )
        }
        return out
    }

    private fun jsonArray(snapshots: List<AttuneSnapshot>): JSONArray {
        val a = JSONArray()
        for (s in snapshots) {
            val o = JSONObject()
            o.put("date", s.date)
            o.put("account", s.account)
            o.put("warforged", s.warforged)
            o.put("lightforged", s.lightforged)
            o.put("titanforged", s.titanforged)
            a.put(o)
        }
        return a
    }

    private fun gzip(data: ByteArray): ByteArray {
        val bos = ByteArrayOutputStream()
        GZIPOutputStream(bos).use { it.write(data) }
        return bos.toByteArray()
    }

    private fun ungzipWithLimit(data: ByteArray, maxDecompressed: Int, maxCompressed: Int): ByteArray? {
        if (data.isEmpty() || data.size > maxCompressed) {
            return null
        }
        return try {
            GZIPInputStream(ByteArrayInputStream(data)).use { g ->
                val out = ByteArrayOutputStream(8192)
                val buf = ByteArray(8192)
                var total = 0
                while (true) {
                    val n = g.read(buf)
                    if (n == -1) {
                        break
                    }
                    if (n == 0) {
                        continue
                    }
                    if (total + n > maxDecompressed) {
                        return null
                    }
                    out.write(buf, 0, n)
                    total += n
                }
                out.toByteArray()
            }
        } catch (e: Exception) {
            null
        }
    }
}
