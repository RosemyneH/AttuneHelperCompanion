package com.attunehelper.companion.util

import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import com.google.zxing.BarcodeFormat
import com.google.zxing.EncodeHintType
import com.google.zxing.qrcode.QRCodeWriter
import com.google.zxing.qrcode.decoder.ErrorCorrectionLevel

object WinlatorIntents {
    private const val PKG_MAIN = "com.winlator"

    fun tryLaunch(packages: PackageManager): Intent? {
        return packages.getLaunchIntentForPackage(PKG_MAIN)?.apply {
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        }
    }

    fun playStoreOrGitHubPage(): Intent {
        return Intent(
            Intent.ACTION_VIEW,
            Uri.parse("https://github.com/brunodev85/winlator/releases")
        )
    }
}

object QrBitmaps {
    private const val MAX_QR_TEXT_CHARS = 4000

    fun fromText(text: String, size: Int = 600): android.graphics.Bitmap {
        if (text.length > MAX_QR_TEXT_CHARS) {
            throw IllegalArgumentException("QR text too long (max $MAX_QR_TEXT_CHARS).")
        }
        val hints = mapOf(
            EncodeHintType.ERROR_CORRECTION to ErrorCorrectionLevel.M,
            EncodeHintType.MARGIN to 4
        )
        val matrix = QRCodeWriter().encode(text, BarcodeFormat.QR_CODE, size, size, hints)
        val w = matrix.width
        val h = matrix.height
        val pm = IntArray(w * h)
        for (y in 0 until h) {
            for (x in 0 until w) {
                pm[y * w + x] = if (matrix.get(x, y)) {
                    0xFF000000.toInt()
                } else {
                    0xFFFFFFFF.toInt()
                }
            }
        }
        return android.graphics.Bitmap.createBitmap(
            w,
            h,
            android.graphics.Bitmap.Config.ARGB_8888
        ).apply {
            setPixels(pm, 0, w, 0, 0, w, h)
        }
    }
}
