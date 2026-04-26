package com.attunehelper.companion.attune.graph

import com.attunehelper.companion.attune.AttuneSnapshot
import java.util.Locale

const val AHC_DISPLAY_HISTORY_CAPACITY: Int = 400

data class DisplayRow(
    val snapshot: AttuneSnapshot,
    val synthetic: Boolean,
)

enum class GraphMetric {
    ACCOUNT,
    TITANFORGED,
    WARFORGED,
    LIGHTFORGED,
    ;

    companion object {
        val DEFAULT: GraphMetric = ACCOUNT
    }
}

enum class GraphDisplay {
    BARS,
    PLOT,
    ;

    companion object {
        val DEFAULT: GraphDisplay = BARS
    }
}

/** Mirrors [apps/companion/main.c] `parse_iso_date` / sscanf `%d-%d-%d` */
fun parseIsoDateTriplet(date: String): Triple<Int, Int, Int>? {
    val t = date.trim().split('-')
    if (t.size != 3) {
        return null
    }
    val y = t[0].toIntOrNull() ?: return null
    val m = t[1].toIntOrNull() ?: return null
    val d = t[2].toIntOrNull() ?: return null
    if (m < 1 || m > 12 || d < 1 || d > 31) {
        return null
    }
    return Triple(y, m, d)
}

/** C `days_from_civil` */
fun daysFromCivil(year: Int, month: Int, day: Int): Int {
    var y = year
    y -= if (month <= 2) 1 else 0
    val era = (if (y >= 0) y else y - 399) / 400
    val yoe = (y - era * 400).toUInt()
    val mo = month
    val doy =
        (153u * (mo + if (mo > 2) -3 else 9).toUInt() + 2u) / 5u + day.toUInt() - 1u
    val doe = yoe * 365u + yoe / 4u - yoe / 100u + doy
    return era * 146097 + doe.toInt() - 719468
}

/** C `date_to_ordinal` */
fun dateToOrdinalOut(date: String, ordinalOut: IntArray): Boolean {
    val t = parseIsoDateTriplet(date) ?: return false
    ordinalOut[0] = daysFromCivil(t.first, t.second, t.third)
    return true
}

/** C `ordinal_to_date` + snprintf format */
fun ordinalToIsoDate(ordinal: Int): String {
    var days = ordinal
    days += 719468
    val era = (if (days >= 0) days else days - 146096) / 146097
    var doe = (days - era * 146097).toUInt()
    val yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u
    var y = yoe.toInt() + era * 400
    var doy = (doe - (365u * yoe + yoe / 4u - yoe / 100u)).toInt()
    val mp = (5 * doy + 2) / 153
    var d = doy - (153 * mp + 2) / 5 + 1
    var m = mp + if (mp < 10) 3 else -9
    y += if (m <= 2) 1 else 0
    return String.format("%04d-%02d-%02d", y, m, d)
}

/** C `build_display_history` */
fun buildDisplayHistory(sorted: List<AttuneSnapshot>, cap: Int = AHC_DISPLAY_HISTORY_CAPACITY): List<DisplayRow> {
    if (cap < 1) {
        return emptyList()
    }
    val out = ArrayList<DisplayRow>(minOf(cap, sorted.size * 2))
    val pOrd = intArrayOf(0)
    val cOrd = intArrayOf(0)
    for (snap in sorted) {
        if (out.isEmpty()) {
            out.add(DisplayRow(snap, false))
            continue
        }
        if (dateToOrdinalOut(out.last().snapshot.date, pOrd) && dateToOrdinalOut(snap.date, cOrd)) {
            for (day in pOrd[0] + 1 until cOrd[0]) {
                if (out.size >= cap) {
                    break
                }
                val prev = out.last().snapshot
                out.add(
                    DisplayRow(
                        AttuneSnapshot(
                            date = ordinalToIsoDate(day),
                            account = prev.account,
                            warforged = prev.warforged,
                            lightforged = prev.lightforged,
                            titanforged = prev.titanforged,
                        ),
                        synthetic = true,
                    ),
                )
            }
        }
        if (out.size < cap) {
            out.add(DisplayRow(snap, false))
        }
    }
    return out
}

fun snapshotMetricValue(s: AttuneSnapshot, m: GraphMetric): Int = when (m) {
    GraphMetric.ACCOUNT -> s.account
    GraphMetric.WARFORGED -> s.warforged
    GraphMetric.LIGHTFORGED -> s.lightforged
    GraphMetric.TITANFORGED -> s.titanforged
}

/**
 * C `daily_metric_value`: day-over-day positive delta on the cumulative metric.
 * Index 0 or out of range returns 0.
 */
fun dailyMetricValue(rows: List<DisplayRow>, count: Int, index: Int, metric: GraphMetric): Int {
    if (index <= 0 || index >= count) {
        return 0
    }
    val prevS = rows[index - 1].snapshot
    val curS = rows[index].snapshot
    val p = snapshotMetricValue(prevS, metric)
    val c = snapshotMetricValue(curS, metric)
    return (c - p).coerceAtLeast(0)
}

fun maxDeltaForMetric(rows: List<DisplayRow>, count: Int, metric: GraphMetric): Int {
    var maxD = 1
    for (i in 0 until count) {
        val d = dailyMetricValue(rows, count, i, metric)
        if (d > maxD) {
            maxD = d
        }
    }
    return maxD
}

/**
 * C `account_attunes_per_tracked_day`.
 */
fun accountAttunesPerTrackedDay(rows: List<DisplayRow>, count: Int): Double {
    if (count < 2) {
        return 0.0
    }
    val first = rows[0].snapshot.account
    val last = rows[count - 1].snapshot.account
    val fOrd = intArrayOf(0)
    val lOrd = intArrayOf(0)
    if (last <= first) {
        return 0.0
    }
    if (!dateToOrdinalOut(rows[0].snapshot.date, fOrd) || !dateToOrdinalOut(rows[count - 1].snapshot.date, lOrd)) {
        return 0.0
    }
    var trackedDays = lOrd[0] - fOrd[0]
    if (trackedDays < 1) {
        trackedDays = 1
    }
    return (last - first).toDouble() / trackedDays.toDouble()
}

/** X-axis sublabel: C uses `date + 5` -> MM-DD part of YYYY-MM-DD. */
fun dateLabelForAxis(iso: String): String = if (iso.length >= 10) iso.substring(5, 10) else iso

/** X-axis date label stride (C `label_step`) */
fun graphDateLabelStep(displayCount: Int): Int = displayCount / 6 + 1

/**
 * C `format_tooltip_count_with_delta`: "Label  value  (+delta)" for delta > 0.
 */
fun formatCountWithDelta(label: String, value: Int, delta: Int): String {
    val v = formatIntWithCommas(value)
    if (delta > 0) {
        val d = formatIntWithCommas(delta)
        return "$label  $v  (+$d)"
    }
    return "$label  $v"
}

fun formatIntWithCommas(n: Int): String = String.format(Locale.US, "%,d", n)

/** Metric name for the selected line in the tooltip. */
fun graphMetricGainLabel(metric: GraphMetric): String = when (metric) {
    GraphMetric.ACCOUNT -> "New account attunes"
    GraphMetric.WARFORGED -> "New WF"
    GraphMetric.LIGHTFORGED -> "New LF"
    GraphMetric.TITANFORGED -> "New TF"
}

/**
 * Multiline text matching desktop graph tooltip (main.c `draw_graph_card` hover block).
 */
fun attuneGraphDetailText(rows: List<DisplayRow>, index: Int, metric: GraphMetric): String {
    if (index < 0 || index >= rows.size) {
        return ""
    }
    val count = rows.size
    val h = rows[index]
    val dailySel = dailyMetricValue(rows, count, index, metric)
    val sb = StringBuilder()
    sb.append(h.snapshot.date)
    if (h.synthetic) {
        sb.append("  |  filled day")
    }
    sb.append('\n')
    sb.append(graphMetricGainLabel(metric))
    sb.append(": ")
    sb.append(formatIntWithCommas(dailySel))
    sb.append('\n')
    sb.append("Account: ")
    sb.append(formatIntWithCommas(h.snapshot.account))
    sb.append('\n')
    sb.append(
        formatCountWithDelta(
            "TF",
            h.snapshot.titanforged,
            dailyMetricValue(rows, count, index, GraphMetric.TITANFORGED),
        ),
    )
    sb.append('\n')
    sb.append(
        formatCountWithDelta(
            "WF",
            h.snapshot.warforged,
            dailyMetricValue(rows, count, index, GraphMetric.WARFORGED),
        ),
    )
    sb.append('\n')
    sb.append(
        formatCountWithDelta(
            "LF",
            h.snapshot.lightforged,
            dailyMetricValue(rows, count, index, GraphMetric.LIGHTFORGED),
        ),
    )
    return sb.toString()
}
