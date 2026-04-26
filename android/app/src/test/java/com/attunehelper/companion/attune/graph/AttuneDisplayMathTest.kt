package com.attunehelper.companion.attune.graph

import com.attunehelper.companion.attune.AttuneSnapshot
import org.junit.Assert.assertEquals
import org.junit.Test

class AttuneDisplayMathTest {
    @Test
    fun `ordinal roundtrip`() {
        for (iso in listOf("2020-01-01", "2024-02-29", "2024-12-31", "2000-06-15")) {
            val t = parseIsoDateTriplet(iso)!!
            val o = daysFromCivil(t.first, t.second, t.third)
            assertEquals(iso, ordinalToIsoDate(o))
        }
    }

    @Test
    fun `buildDisplayHistory fills gap`() {
        val rows = listOf(
            AttuneSnapshot("2024-01-01", 10, 0, 0, 0),
            AttuneSnapshot("2024-01-04", 15, 1, 0, 0),
        )
        val d = buildDisplayHistory(rows, 400)
        assertEquals(4, d.size)
        assertEquals("2024-01-01", d[0].snapshot.date)
        assertEquals(false, d[0].synthetic)
        assertEquals("2024-01-02", d[1].snapshot.date)
        assertEquals(true, d[1].synthetic)
        assertEquals(10, d[1].snapshot.account)
        assertEquals("2024-01-03", d[2].snapshot.date)
        assertEquals("2024-01-04", d[3].snapshot.date)
    }

    @Test
    fun `dailyMetricValue account step`() {
        val rows = listOf(
            DisplayRow(AttuneSnapshot("2024-01-01", 5, 0, 0, 0), false),
            DisplayRow(AttuneSnapshot("2024-01-02", 8, 0, 0, 0), false),
        )
        assertEquals(0, dailyMetricValue(rows, 2, 0, GraphMetric.ACCOUNT))
        assertEquals(3, dailyMetricValue(rows, 2, 1, GraphMetric.ACCOUNT))
    }

    @Test
    fun `maxDelta at least 1`() {
        val rows = listOf(
            DisplayRow(AttuneSnapshot("2024-01-01", 5, 0, 0, 0), false),
        )
        assertEquals(1, maxDeltaForMetric(rows, 1, GraphMetric.ACCOUNT))
    }

    @Test
    fun `account attunes per tracked day`() {
        val d = buildDisplayHistory(
            listOf(
                AttuneSnapshot("2024-01-01", 0, 0, 0, 0),
                AttuneSnapshot("2024-01-04", 9, 0, 0, 0),
            ),
            400,
        )
        val c = d.size
        val avg = accountAttunesPerTrackedDay(d, c)
        assertEquals(3.0, avg, 0.001)
    }

    @Test
    fun `plot is default display`() {
        assertEquals(GraphDisplay.PLOT, GraphDisplay.DEFAULT)
    }

    @Test
    fun `attune summaries use commas`() {
        val d = buildDisplayHistory(
            listOf(
                AttuneSnapshot("2024-01-01", 0, 0, 0, 0),
                AttuneSnapshot("2024-01-03", 2469, 12, 34, 56),
            ),
            400,
        )
        assertEquals("Account Attunes per Day\n1,234.5", attuneGraphAverageText(d))
        assertEquals(
            "Latest snapshot: 2024-01-03 \u00b7 Account 2,469 \u00b7 TF 56 \u00b7 WF 12 \u00b7 LF 34",
            attuneSyncLatestText(d.map { it.snapshot }),
        )
    }
}
