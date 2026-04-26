package com.attunehelper.companion.attune.graph

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.util.TypedValue
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import androidx.core.content.ContextCompat
import com.attunehelper.companion.R
import com.attunehelper.companion.attune.AttuneSnapshot
import kotlin.math.max
import kotlin.math.min

class AttuneGraphView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyle: Int = 0,
) : View(context, attrs, defStyle) {

    var graphMetric: GraphMetric = GraphMetric.DEFAULT
        set(v) {
            if (field != v) {
                field = v
                invalidate()
            }
        }

    var graphDisplay: GraphDisplay = GraphDisplay.DEFAULT
        set(v) {
            if (field != v) {
                field = v
                invalidate()
            }
        }

    private var displayRows: List<DisplayRow> = emptyList()
    var selectedIndex: Int? = null
        private set

    private var onSelectionChange: ((Int?) -> Unit)? = null
    private val pad: Float
    private val text14: Float
    private val text12: Float
    private val text16: Float

    private var plotLeft = 0f
    private var plotRight = 0f
    private var plotTop = 0f
    private var baseY = 0f
    private var plotHeight = 0f

    private val pAxis = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pMuted = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pGrid = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pFill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pStroke = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pSelect = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pLine = Paint(Paint.ANTI_ALIAS_FLAG)
    private val pBarZero = Paint(Paint.ANTI_ALIAS_FLAG)
    private var colorAccent = 0
    private var colorMetric = 0
    private var colorMuted = 0
    private var colorBorder = 0
    private var colorText = 0

    init {
        val dm = resources.displayMetrics
        pad = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 8f, dm)
        text14 = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 14f, dm)
        text12 = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 12f, dm)
        text16 = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 16f, dm)
        pAxis.style = Paint.Style.STROKE
        pAxis.strokeWidth = max(1f, TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 1f, dm))
        pGrid.style = Paint.Style.STROKE
        pGrid.strokeWidth = pAxis.strokeWidth
        pStroke.style = Paint.Style.STROKE
        pStroke.strokeWidth = pAxis.strokeWidth
        pLine.style = Paint.Style.STROKE
        pLine.strokeWidth = max(1f, TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 2f, dm))
        pFill.style = Paint.Style.FILL
        pFill.isAntiAlias = true
        pBarZero.style = Paint.Style.FILL
        pBarZero.isAntiAlias = true
        pMuted.isAntiAlias = true
        refreshThemedColors()
    }

    private fun refreshThemedColors() {
        colorAccent = ContextCompat.getColor(context, R.color.ahc_accent)
        colorMuted = ContextCompat.getColor(context, R.color.ahc_text_muted)
        colorBorder = ContextCompat.getColor(context, R.color.ahc_outline_bright)
        colorText = ContextCompat.getColor(context, R.color.ahc_text)
        pGrid.color = ContextCompat.getColor(context, R.color.ahc_graph_grid)
        pSelect.color = ContextCompat.getColor(context, R.color.ahc_graph_select)
        pAxis.color = ContextCompat.getColor(context, R.color.ahc_outline)
        pMuted.color = colorMuted
        pStroke.color = colorBorder
    }

    private fun colorForCurrentMetric(): Int = when (graphMetric) {
        GraphMetric.ACCOUNT -> colorAccent
        GraphMetric.TITANFORGED -> ContextCompat.getColor(context, R.color.ahc_tf)
        GraphMetric.WARFORGED -> ContextCompat.getColor(context, R.color.ahc_wf)
        GraphMetric.LIGHTFORGED -> ContextCompat.getColor(context, R.color.ahc_lf)
    }

    fun setOnAttuneGraphSelectionListener(listener: ((Int?) -> Unit)?) {
        onSelectionChange = listener
    }

    fun setSnapshots(snapshots: List<AttuneSnapshot>) {
        displayRows = buildDisplayHistory(snapshots.sortedBy { it.date })
        selectedIndex = null
        onSelectionChange?.invoke(null)
        invalidate()
    }

    private fun setSelectedInternal(i: Int?) {
        selectedIndex = i
        onSelectionChange?.invoke(i)
    }

    fun clearSelection() {
        setSelectedInternal(null)
        invalidate()
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        val leftIn = paddingLeft + pad + dp(40f)
        val rightIn = w - (paddingRight + pad + dp(20f))
        val topIn = (paddingTop + pad + text14 * 1.2f)
        val bottomIn = h - (paddingBottom + pad + text12 * 1.8f)
        plotLeft = leftIn
        plotRight = rightIn
        plotTop = topIn
        baseY = bottomIn
        if (baseY < plotTop + dp(80f)) {
            baseY = (plotTop + dp(80f)).coerceAtMost(bottomIn)
        }
        plotHeight = baseY - plotTop
    }

    private fun dp(v: Float): Float =
        TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, resources.displayMetrics)

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        colorMetric = colorForCurrentMetric()
        pFill.color = colorMetric
        pFill.style = Paint.Style.FILL
        pLine.color = colorMetric
        pLine.style = Paint.Style.STROKE
        if (displayRows.isEmpty()) {
            drawEmpty(canvas)
            return
        }
        val count = displayRows.size
        val maxD = maxDeltaForMetric(displayRows, count, graphMetric)
        val axis = context.getString(
            R.string.attune_graph_axis,
            formatIntWithCommas(maxD),
            displayRows.first().snapshot.date,
            displayRows.last().snapshot.date,
        )
        pMuted.textSize = text14
        canvas.drawText(
            axis,
            plotLeft,
            plotTop - text14 * 0.2f,
            pMuted,
        )
        canvas.drawLine(plotLeft, plotTop, plotLeft, baseY, pAxis)
        canvas.drawLine(plotLeft, baseY, plotRight, baseY, pAxis)
        for (g in 1..3) {
            val y = baseY - (plotHeight * g) / 4f
            canvas.drawLine(plotLeft + 1f, y, plotRight, y, pGrid)
        }
        val slotW = (plotRight - plotLeft) / count.toFloat()
        var barW = slotW * 0.46f
        if (barW < dp(6f)) {
            barW = dp(6f)
        }
        if (barW > dp(30f)) {
            barW = dp(30f)
        }
        val dateLabelPeriod = graphDateLabelStep(count)
        var prevX = 0f
        var prevY = 0f
        var hasPrev = false
        pMuted.style = Paint.Style.FILL
        pMuted.textAlign = Paint.Align.LEFT
        for (i in 0 until count) {
            val x = plotLeft + slotW * i + slotW * 0.5f
            val d = dailyMetricValue(displayRows, count, i, graphMetric)
            val barH = if (d > 0) {
                (d.toFloat() / maxD.toFloat()) * (plotHeight - dp(12f))
            } else {
                dp(3f)
            }
            val yPt = baseY - (d.toFloat() / maxD.toFloat()) * (plotHeight - dp(12f))
            val hit = RectF(
                plotLeft + slotW * i,
                plotTop,
                plotLeft + slotW * (i + 1f),
                baseY + dp(6f),
            )
            if (selectedIndex == i) {
                canvas.drawRoundRect(
                    RectF(hit.left + 2f, hit.top, hit.right - 2f, hit.bottom + dp(4f)),
                    dp(6f),
                    dp(6f),
                    pSelect,
                )
            }
            if (graphDisplay == GraphDisplay.PLOT) {
                if (hasPrev) {
                    canvas.drawLine(prevX, prevY, x, yPt, pLine)
                }
                val r = if (selectedIndex == i) dp(6.5f) else dp(5f)
                canvas.drawCircle(x, yPt, r, pFill)
                canvas.drawCircle(x, yPt, if (selectedIndex == i) dp(9f) else dp(7f), pStroke)
                prevX = x
                prevY = yPt
                hasPrev = true
            } else {
                if (d > 0) {
                    val r = max(1f, 0.35f * min(barW, max(barH * 0.1f, dp(1f))))
                    val top = baseY - barH
                    canvas.drawRoundRect(
                        RectF(x - barW * 0.5f, top, x + barW * 0.5f, baseY),
                        r,
                        r,
                        pFill,
                    )
                    canvas.drawRoundRect(
                        RectF(x - barW * 0.5f, top, x + barW * 0.5f, baseY),
                        r,
                        r,
                        pStroke,
                    )
                } else {
                    pBarZero.color = if (selectedIndex == i) colorText else colorMuted
                    val z = RectF(x - dp(5f), baseY - dp(3f), x + dp(5f), baseY)
                    canvas.drawRoundRect(z, dp(4.5f), dp(4.5f), pBarZero)
                }
            }
            if (i % dateLabelPeriod == 0 || i + 1 == count) {
                val lab = dateLabelForAxis(displayRows[i].snapshot.date)
                pMuted.textSize = text12
                pMuted.color = colorMuted
                canvas.drawText(
                    lab,
                    x,
                    baseY + text12 * 1.1f,
                    pMuted,
                )
            }
        }
    }

    private fun drawEmpty(canvas: Canvas) {
        pMuted.textSize = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 18f, resources.displayMetrics)
        pMuted.textAlign = Paint.Align.CENTER
        pMuted.color = colorMuted
        val cx = width / 2f
        var y = max(height, suggestedMinimumHeight) * 0.3f
        if (y < text16 * 2f) {
            y = text16 * 2f
        }
        canvas.drawText(context.getString(R.string.attune_graph_empty_1), cx, y, pMuted)
        y += text16 * 1.6f
        pMuted.textSize = text16
        canvas.drawText(context.getString(R.string.attune_graph_empty_2), cx, y, pMuted)
        pMuted.textAlign = Paint.Align.LEFT
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (displayRows.isEmpty()) {
            return false
        }
        if (event.action == MotionEvent.ACTION_UP) {
            val x = event.x
            val y = event.y
            val inPlotX = x >= plotLeft && x <= plotRight
            val inPlotY = y >= plotTop && y <= baseY
            val count = displayRows.size
            val slotW = (plotRight - plotLeft) / count
            if (inPlotX && inPlotY) {
                val i = ((x - plotLeft) / slotW).toInt().coerceIn(0, count - 1)
                if (selectedIndex == i) {
                    setSelectedInternal(null)
                } else {
                    setSelectedInternal(i)
                    performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)
                }
            } else {
                setSelectedInternal(null)
            }
            invalidate()
        }
        return true
    }

    override fun getSuggestedMinimumHeight(): Int =
        (dp(280f) + paddingTop + paddingBottom).toInt()

    override fun onMeasure(widthSpec: Int, heightSpec: Int) {
        setMeasuredDimension(
            resolveSize(suggestedMinimumWidth, widthSpec),
            resolveSize(getSuggestedMinimumHeight(), heightSpec),
        )
    }

    fun getDisplayRows(): List<DisplayRow> = displayRows
}
