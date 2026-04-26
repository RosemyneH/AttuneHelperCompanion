package com.attunehelper.companion.data

import android.content.Context
import com.attunehelper.companion.attune.AttuneSnapshot
import org.json.JSONArray
import org.json.JSONObject

class AttuneHistoryStore(
    private val appContext: Context,
) {
    private val prefs
        get() = appContext.getSharedPreferences("ahc_attune_history", Context.MODE_PRIVATE)

    fun getAll(): List<AttuneSnapshot> {
        val raw = prefs.getString(KEY_SNAPSHOTS, "[]") ?: "[]"
        return parseList(raw)
    }

    fun upsert(snapshot: AttuneSnapshot) {
        val all = getAll().associateBy { it.date }.toMutableMap()
        all[snapshot.date] = snapshot
        val sorted = all.values.sortedBy { it.date }
        saveList(sorted)
    }

    fun mergeIncoming(incoming: List<AttuneSnapshot>) {
        val m = getAll().associateBy { it.date }.toMutableMap()
        for (i in incoming) {
            m[i.date] = i
        }
        val sorted = m.values.sortedBy { it.date }
        saveList(sorted)
    }

    fun replaceWith(incoming: List<AttuneSnapshot>) {
        saveList(incoming.sortedBy { it.date })
    }

    fun clear() {
        prefs.edit().remove(KEY_SNAPSHOTS).apply()
    }

    private fun saveList(list: List<AttuneSnapshot>) {
        val a = JSONArray()
        for (s in list) {
            val o = JSONObject()
            o.put("date", s.date)
            o.put("account", s.account)
            o.put("warforged", s.warforged)
            o.put("lightforged", s.lightforged)
            o.put("titanforged", s.titanforged)
            a.put(o)
        }
        prefs.edit().putString(KEY_SNAPSHOTS, a.toString()).apply()
    }

    private fun parseList(json: String): List<AttuneSnapshot> {
        if (json.isEmpty()) {
            return emptyList()
        }
        val out = ArrayList<AttuneSnapshot>()
        val a = try {
            JSONArray(json)
        } catch (e: Exception) {
            return out
        }
        for (i in 0 until a.length()) {
            val o = a.optJSONObject(i) ?: continue
            val date = o.optString("date", "")
            if (date.isEmpty()) {
                continue
            }
            out.add(
                AttuneSnapshot(
                    date = date,
                    account = o.optInt("account"),
                    warforged = o.optInt("warforged"),
                    lightforged = o.optInt("lightforged"),
                    titanforged = o.optInt("titanforged"),
                )
            )
        }
        return out
    }

    fun synastriaTreeUri(): String? = prefs.getString(KEY_URI, null)

    fun setSynastriaTreeUri(asString: String?) {
        if (asString == null) {
            prefs.edit().remove(KEY_URI).apply()
        } else {
            prefs.edit().putString(KEY_URI, asString).apply()
        }
    }

    fun lastAttuneHelperLuaDocumentUri(): String? = prefs.getString(KEY_LUA_URI, null)

    fun setLastAttuneHelperLuaDocumentUri(value: String?) {
        if (value == null) {
            prefs.edit().remove(KEY_LUA_URI).apply()
        } else {
            prefs.edit().putString(KEY_LUA_URI, value).apply()
        }
    }

    companion object {
        private const val KEY_SNAPSHOTS = "snaps"
        private const val KEY_URI = "synastria_tree_uri"
        private const val KEY_LUA_URI = "attunehelper_lua_uri"
    }
}
