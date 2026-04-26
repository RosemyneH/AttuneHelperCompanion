package com.attunehelper.companion.nfc

import android.app.Activity
import android.nfc.NdefMessage
import android.nfc.NfcAdapter
import java.lang.reflect.InvocationTargetException

object NfcNdefPushCompat {
    private val setNdefPushMessageMethod: java.lang.reflect.Method? = run {
        try {
            NfcAdapter::class.java.getMethod(
                "setNdefPushMessage",
                NdefMessage::class.java,
                Activity::class.java,
                Array<Activity>::class.java
            )
        } catch (e: NoSuchMethodException) {
            null
        }
    }

    fun set(adapter: NfcAdapter?, message: NdefMessage?, activity: Activity) {
        val m = setNdefPushMessageMethod ?: return
        if (adapter == null) {
            return
        }
        try {
            m.invoke(adapter, message, activity, null)
        } catch (e: IllegalAccessException) {
        } catch (e: InvocationTargetException) {
        }
    }
}
