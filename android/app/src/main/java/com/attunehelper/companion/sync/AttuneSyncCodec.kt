package com.attunehelper.companion.sync

import android.util.Base64
import com.attunehelper.companion.attune.AttuneHelperLuaParser
import com.attunehelper.companion.attune.AttuneSnapshot
import com.attunehelper.companion.attune.graph.dateToOrdinalOut
import com.attunehelper.companion.attune.graph.ordinalToIsoDate
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
    private const val MULTI_QR_PREFIX = "AHC-Q2:"
    private const val MULTI_QR_VERSION = 1
    private const val MULTI_QR_MAX_DAYS = 30
    private const val MULTI_QR_MAX_RAW_BYTES = 1024
    private const val MULTI_QR_MAX_B64_CHARS = 1800

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

    fun encodeMultiDayQr(snapshots: List<AttuneSnapshot>, maxDays: Int = MULTI_QR_MAX_DAYS): String? {
        val picked = snapshots
            .sortedBy { it.date }
            .filter { isMultiQrEligible(it) }
            .takeLast(maxDays.coerceIn(1, MULTI_QR_MAX_DAYS))
        if (picked.isEmpty()) {
            return null
        }
        val out = ByteArrayOutputStream()
        out.write(MULTI_QR_VERSION)
        out.write(picked.size)
        var previousOrdinal = 0
        for ((i, snapshot) in picked.withIndex()) {
            val ord = intArrayOf(0)
            if (!dateToOrdinalOut(snapshot.date, ord)) {
                return null
            }
            if (i == 0) {
                writeIntBe(out, ord[0])
                writeUVarInt(out, 0)
            } else {
                val delta = ord[0] - previousOrdinal
                if (delta < 0) {
                    return null
                }
                writeUVarInt(out, delta)
            }
            previousOrdinal = ord[0]
            writeUVarInt(out, snapshot.account)
            writeUVarInt(out, snapshot.warforged)
            writeUVarInt(out, snapshot.lightforged)
            writeUVarInt(out, snapshot.titanforged)
        }
        val body = out.toByteArray()
        if (body.size > MULTI_QR_MAX_RAW_BYTES) {
            return null
        }
        val b64 = Base64.encodeToString(body, Base64.NO_WRAP or Base64.NO_PADDING or Base64.URL_SAFE)
        return MULTI_QR_PREFIX + b64
    }

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

    fun decodeMultiDayQrOrNull(s: String): List<AttuneSnapshot>? {
        var t = s.trim()
        if (t.startsWith("\uFEFF")) {
            t = t.substring(1).trim()
        }
        if (!t.startsWith(MULTI_QR_PREFIX)) {
            return null
        }
        t = t.removePrefix(MULTI_QR_PREFIX).filter { !it.isWhitespace() }
        if (t.isEmpty() || t.length > MULTI_QR_MAX_B64_CHARS) {
            return null
        }
        return try {
            val raw = Base64.decode(t, Base64.NO_WRAP or Base64.URL_SAFE)
            decodeMultiQrRaw(raw)
        } catch (e: Exception) {
            null
        }
    }

    private fun decodeMultiQrRaw(raw: ByteArray): List<AttuneSnapshot>? {
        if (raw.size < 7 || raw.size > MULTI_QR_MAX_RAW_BYTES) {
            return null
        }
        var off = 0
        val version = raw[off++].toInt() and 0xff
        if (version != MULTI_QR_VERSION) {
            return null
        }
        val count = raw[off++].toInt() and 0xff
        if (count < 1 || count > MULTI_QR_MAX_DAYS) {
            return null
        }
        var ordinal = readIntBe(raw, off)
        off += 4
        val out = ArrayList<AttuneSnapshot>(count)
        for (i in 0 until count) {
            val delta = readUVarInt(raw, off) ?: return null
            off = delta.nextOffset
            if (i == 0) {
                if (delta.value != 0) {
                    return null
                }
            } else {
                ordinal += delta.value
            }
            val account = readUVarInt(raw, off) ?: return null
            off = account.nextOffset
            val warforged = readUVarInt(raw, off) ?: return null
            off = warforged.nextOffset
            val lightforged = readUVarInt(raw, off) ?: return null
            off = lightforged.nextOffset
            val titanforged = readUVarInt(raw, off) ?: return null
            off = titanforged.nextOffset
            out.add(
                AttuneSnapshot(
                    date = ordinalToIsoDate(ordinal),
                    account = account.value,
                    warforged = warforged.value,
                    lightforged = lightforged.value,
                    titanforged = titanforged.value,
                )
            )
        }
        if (off != raw.size) {
            return null
        }
        return out
    }

    private fun isMultiQrEligible(snapshot: AttuneSnapshot): Boolean {
        val ord = intArrayOf(0)
        return snapshot.account >= 0 &&
            snapshot.warforged >= 0 &&
            snapshot.lightforged >= 0 &&
            snapshot.titanforged >= 0 &&
            dateToOrdinalOut(snapshot.date, ord)
    }

    private fun writeIntBe(out: ByteArrayOutputStream, value: Int) {
        out.write((value ushr 24) and 0xff)
        out.write((value ushr 16) and 0xff)
        out.write((value ushr 8) and 0xff)
        out.write(value and 0xff)
    }

    private fun readIntBe(raw: ByteArray, offset: Int): Int {
        return ((raw[offset].toInt() and 0xff) shl 24) or
            ((raw[offset + 1].toInt() and 0xff) shl 16) or
            ((raw[offset + 2].toInt() and 0xff) shl 8) or
            (raw[offset + 3].toInt() and 0xff)
    }

    private fun writeUVarInt(out: ByteArrayOutputStream, value: Int) {
        require(value >= 0) { "negative varint" }
        var v = value
        do {
            var b = v and 0x7f
            v = v ushr 7
            if (v != 0) {
                b = b or 0x80
            }
            out.write(b)
        } while (v != 0)
    }

    private data class VarIntRead(
        val value: Int,
        val nextOffset: Int,
    )

    private fun readUVarInt(raw: ByteArray, offset: Int): VarIntRead? {
        var off = offset
        var value = 0
        var shift = 0
        while (off < raw.size && shift <= 28) {
            val b = raw[off++].toInt() and 0xff
            value = value or ((b and 0x7f) shl shift)
            if ((b and 0x80) == 0) {
                return VarIntRead(value, off)
            }
            shift += 7
        }
        return null
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
