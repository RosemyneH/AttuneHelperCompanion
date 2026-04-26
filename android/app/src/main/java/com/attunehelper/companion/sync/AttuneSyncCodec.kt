package com.attunehelper.companion.sync

import android.util.Base64
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
    private const val QR_PREFIX = "AHC-Q1:"

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
        return try {
            val gz = Base64.decode(s, Base64.NO_WRAP or Base64.URL_SAFE)
            val json = ungzip(gz).toString(StandardCharsets.UTF_8)
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

    fun decodeQrOrNull(s: String): AttuneSnapshot? {
        if (!s.trim().startsWith(QR_PREFIX)) {
            return null
        }
        val t = s.trim().removePrefix(QR_PREFIX)
        val parts = t.split('|')
        if (parts.size != 5) {
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

    private fun ungzip(data: ByteArray): ByteArray {
        val b = ByteArrayInputStream(data)
        GZIPInputStream(b).use { g ->
            return g.readBytes()
        }
    }
}
