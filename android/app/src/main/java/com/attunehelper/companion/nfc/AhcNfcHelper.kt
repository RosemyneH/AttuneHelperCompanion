package com.attunehelper.companion.nfc

import android.content.Intent
import android.nfc.NdefMessage
import android.nfc.NdefRecord
import android.nfc.NfcAdapter
import java.nio.charset.StandardCharsets
import java.util.ArrayList

object AhcNfcHelper {
    const val MIME_AHC = "application/x-ahc"
    const val MIME_LUA = "application/x-ahc-lua"
    private const val PKG = "com.attunehelper.companion"
    private const val MAX_NDEF = 7 * 1024

    fun buildPushNdef(
        fullHistoryCode: String,
        oneDayCode: String,
        luaAhcLua1OrNull: String?,
    ): NdefMessage {
        val aar = NdefRecord.createApplicationRecord(PKG)
        val ahcPrimary = if (byteLenUtf8(fullHistoryCode) <= 3800) {
            fullHistoryCode
        } else {
            oneDayCode
        }
        val ahcRec = NdefRecord.createMime(
            MIME_AHC,
            ahcPrimary.toByteArray(StandardCharsets.UTF_8)
        )
        if (luaAhcLua1OrNull == null) {
            return makeUnderCap(arrayOf(ahcRec, aar), oneDayCode, aar)
        }
        val luaRec = NdefRecord.createMime(
            MIME_LUA,
            luaAhcLua1OrNull.toByteArray(StandardCharsets.UTF_8)
        )
        val m = NdefMessage(arrayOf(ahcRec, luaRec, aar))
        if (m.byteArrayLength <= MAX_NDEF) {
            return m
        }
        return makeUnderCap(arrayOf(ahcRec, aar), oneDayCode, aar)
    }

    private fun makeUnderCap(
        base: Array<NdefRecord>,
        oneDay: String,
        aar: NdefRecord
    ): NdefMessage {
        var m = NdefMessage(base)
        if (m.byteArrayLength > MAX_NDEF) {
            val ahc = NdefRecord.createMime(
                MIME_AHC,
                oneDay.toByteArray(StandardCharsets.UTF_8)
            )
            m = NdefMessage(arrayOf(ahc, aar))
        }
        return m
    }

    private val NdefMessage.byteArrayLength: Int
        get() = toByteArray().size

    private fun byteLenUtf8(s: String) = s.toByteArray(StandardCharsets.UTF_8).size

    fun readTextPayloadsFromIntent(intent: Intent): List<String> {
        if (intent.action != NfcAdapter.ACTION_NDEF_DISCOVERED) {
            return emptyList()
        }
        @Suppress("DEPRECATION")
        val raw = intent.getParcelableArrayExtra(NfcAdapter.EXTRA_NDEF_MESSAGES)
            ?: return emptyList()
        val out = ArrayList<String>()
        for (n in raw) {
            if (n !is NdefMessage) {
                continue
            }
            for (i in 0 until n.size) {
                out.addAll(decodeToStrings(n.get(i)))
            }
        }
        return out
    }

    @Suppress("DEPRECATION")
    private fun decodeToStrings(r: NdefRecord): List<String> {
        if (r.tnf == NdefRecord.TNF_MIME_MEDIA) {
            return listOf(String(r.payload, StandardCharsets.UTF_8))
        }
        if (r.tnf == NdefRecord.TNF_WELL_KNOWN && r.type contentEquals NdefRecord.RTD_TEXT) {
            val p = r.payload
            if (p.isEmpty()) {
                return emptyList()
            }
            return listOf(String(p, 1, p.size - 1, StandardCharsets.UTF_8))
        }
        return emptyList()
    }
}
