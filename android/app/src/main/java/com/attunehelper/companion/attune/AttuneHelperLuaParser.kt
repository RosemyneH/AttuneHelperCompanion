package com.attunehelper.companion.attune

import java.nio.charset.StandardCharsets

object AttuneHelperLuaParser {
    private const val MARKER = "[\"DailyAttuneSnapshot\"]"

    fun parse(content: ByteArray): AttuneSnapshot? {
        val text = String(content, StandardCharsets.UTF_8)
        return parseText(text)
    }

    fun parseText(text: String): AttuneSnapshot? {
        val i = text.indexOf(MARKER)
        if (i < 0) {
            return null
        }
        val after = text.indexOf('{', i + MARKER.length)
        if (after < 0) {
            return null
        }
        val end = findMatchingClose(text, after) ?: return null
        val inner = text.substring(after + 1, end)
        return parseTableInner(inner)
    }

    private fun findMatchingClose(s: String, openIdx: Int): Int? {
        if (openIdx < 0 || openIdx >= s.length || s[openIdx] != '{') {
            return null
        }
        var depth = 1
        var inStr = false
        var p = openIdx + 1
        while (p < s.length) {
            val c = s[p]
            if (inStr) {
                if (c == '\\' && p + 1 < s.length) {
                    p += 2
                    continue
                }
                if (c == '"') {
                    inStr = false
                }
                p++
                continue
            }
            when (c) {
                '"' -> inStr = true
                '{' -> depth++
                '}' -> {
                    depth--
                    if (depth == 0) {
                        return p
                    }
                }
            }
            p++
        }
        return null
    }

    private fun parseTableInner(body: String): AttuneSnapshot? {
        var date: String? = null
        var account = 0
        var war = 0
        var light = 0
        var titan = 0
        var found = false
        val re = Regex(
            """\[\s*\"([^\"]+)\"\s*\]\s*=\s*("([^"]*)"|(-?\d+))""",
            setOf(RegexOption.DOT_MATCHES_ALL)
        )
        re.findAll(body).forEach { m ->
            val key = m.groupValues[1]
            val strVal = m.groupValues[3]
            val intVal = m.groupValues[4]
            when (key) {
                "date" -> {
                    if (strVal.isNotEmpty()) {
                        date = strVal
                        found = true
                    }
                }
                "account" -> intVal.toIntOrNull()?.let { account = it; found = true }
                "warforged" -> intVal.toIntOrNull()?.let { war = it; found = true }
                "lightforged" -> intVal.toIntOrNull()?.let { light = it; found = true }
                "titanforged" -> intVal.toIntOrNull()?.let { titan = it; found = true }
            }
        }
        if (!found || date == null) {
            return null
        }
        return AttuneSnapshot(
            date = date,
            account = account,
            warforged = war,
            lightforged = light,
            titanforged = titan,
        )
    }
}
