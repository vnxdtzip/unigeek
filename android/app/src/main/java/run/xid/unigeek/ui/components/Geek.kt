package run.xid.unigeek.ui.components

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import run.xid.unigeek.ui.theme.Geek
import run.xid.unigeek.ui.theme.GeekMono

private val SharpCorner = RoundedCornerShape(2.dp)

/** Faint PCB trace grid with corner-fade — the site's `.pcb-grid`. */
fun Modifier.pcbGrid(): Modifier = drawBehind {
    val step = 64.dp.toPx()
    val color = Color(0x0AE6E8EA)
    var x = 0f
    while (x < size.width) {
        drawLine(color, Offset(x, 0f), Offset(x, size.height), 1f)
        x += step
    }
    var y = 0f
    while (y < size.height) {
        drawLine(color, Offset(0f, y), Offset(size.width, y), 1f)
        y += step
    }
}

/** Uppercase mono label — `.section-label`, `.annotation`, panel headers. */
@Composable
fun MonoLabel(
    text: String,
    modifier: Modifier = Modifier,
    color: Color = Geek.InkMuted,
    size: Int = 11,
) {
    Text(
        text = text.uppercase(),
        modifier = modifier,
        color = color,
        style = TextStyle(fontFamily = GeekMono, fontSize = size.sp, letterSpacing = 1.2.sp),
    )
}

/** Section divider with the accent tick — `.section-label::before`. */
@Composable
fun SectionLabel(text: String, modifier: Modifier = Modifier) {
    Row(modifier = modifier, verticalAlignment = Alignment.CenterVertically) {
        Box(Modifier.size(width = 24.dp, height = 1.dp).background(Geek.Accent))
        MonoLabel(text, Modifier.padding(start = 12.dp))
    }
}

/** Bordered surface panel; optional accent corner ticks (`.panel-corners`). */
@Composable
fun GeekPanel(
    modifier: Modifier = Modifier,
    corners: Boolean = false,
    contentPadding: PaddingValues = PaddingValues(0.dp),
    content: @Composable () -> Unit,
) {
    Box(
        modifier
            .background(Geek.BgElev)
            .border(BorderStroke(1.dp, Geek.Line))
            .then(if (corners) Modifier.drawBehind { drawCornerTicks() } else Modifier)
            .padding(contentPadding)
    ) { content() }
}

private fun androidx.compose.ui.graphics.drawscope.DrawScope.drawCornerTicks() {
    val len = 8.dp.toPx()
    val c = Geek.Accent.copy(alpha = 0.8f)
    val w = size.width
    val h = size.height
    // TL, TR, BL, BR L-shaped ticks
    drawLine(c, Offset(0f, 0f), Offset(len, 0f)); drawLine(c, Offset(0f, 0f), Offset(0f, len))
    drawLine(c, Offset(w, 0f), Offset(w - len, 0f)); drawLine(c, Offset(w, 0f), Offset(w, len))
    drawLine(c, Offset(0f, h), Offset(len, h)); drawLine(c, Offset(0f, h), Offset(0f, h - len))
    drawLine(c, Offset(w, h), Offset(w - len, h)); drawLine(c, Offset(w, h), Offset(w, h - len))
}

/** Panel header strip — `.panel-header`. */
@Composable
fun PanelHeader(title: String, trailing: (@Composable () -> Unit)? = null) {
    Row(
        Modifier
            .fillMaxWidth()
            .background(Geek.BgElev)
            .border(BorderStroke(1.dp, Geek.Line))
            .padding(horizontal = 16.dp, vertical = 10.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        MonoLabel(title)
        trailing?.invoke()
    }
}

enum class GeekButtonStyle { Primary, Default, Ghost }

/** Mono uppercase button with `.btn` framing. */
@Composable
fun GeekButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    style: GeekButtonStyle = GeekButtonStyle.Default,
    enabled: Boolean = true,
) {
    val bg: Color
    val fg: Color
    val border: Color
    when (style) {
        GeekButtonStyle.Primary -> { bg = Geek.Accent; fg = Geek.Bg; border = Geek.Accent }
        GeekButtonStyle.Default -> { bg = Color.Transparent; fg = Geek.Ink; border = Geek.LineStrong }
        GeekButtonStyle.Ghost   -> { bg = Color.Transparent; fg = Geek.InkDim; border = Geek.Line }
    }
    val alpha = if (enabled) 1f else 0.5f
    Box(
        modifier
            .background(bg.copy(alpha = bg.alpha * alpha), SharpCorner)
            .border(BorderStroke(1.dp, border.copy(alpha = border.alpha * alpha)), SharpCorner)
            .then(if (enabled) Modifier.clickableNoRipple(onClick) else Modifier)
            .padding(horizontal = 20.dp, vertical = 12.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            text.uppercase(),
            color = fg.copy(alpha = alpha),
            style = TextStyle(
                fontFamily = GeekMono, fontSize = 13.sp, letterSpacing = 0.6.sp,
                fontWeight = if (style == GeekButtonStyle.Primary) FontWeight.Medium else FontWeight.Normal,
            ),
        )
    }
}

enum class BadgeKind { Ok, Warn, Danger, Neutral }

@Composable
fun Badge(text: String, kind: BadgeKind = BadgeKind.Neutral, modifier: Modifier = Modifier) {
    val (fg, border, bg) = when (kind) {
        BadgeKind.Ok      -> Triple(Geek.Accent, Geek.Accent, Geek.AccentDim)
        BadgeKind.Warn    -> Triple(Geek.Amber, Geek.Amber, Geek.AmberDim)
        BadgeKind.Danger  -> Triple(Geek.Danger, Geek.Danger, Geek.DangerDim)
        BadgeKind.Neutral -> Triple(Geek.InkDim, Geek.LineStrong, Geek.BgElev)
    }
    Box(
        modifier
            .background(bg, SharpCorner)
            .border(BorderStroke(1.dp, border), SharpCorner)
            .padding(horizontal = 8.dp, vertical = 3.dp)
    ) {
        Text(
            text.uppercase(),
            color = fg,
            style = TextStyle(fontFamily = GeekMono, fontSize = 10.sp, letterSpacing = 0.8.sp),
        )
    }
}

/** Pulsing accent dot — `.status-dot` (opacity blink + soft glow). */
@Composable
fun StatusDot(color: Color = Geek.Accent, modifier: Modifier = Modifier, pulse: Boolean = true) {
    val alpha = if (pulse) rememberPulse() else 1f
    Box(modifier.size(6.dp).background(color.copy(alpha = alpha), RoundedCornerShape(50)))
}

/** Banner row (`.fm-banner-err` etc). */
@Composable
fun Banner(text: String, kind: BadgeKind = BadgeKind.Danger, modifier: Modifier = Modifier) {
    val (fg, border, bg) = when (kind) {
        BadgeKind.Ok      -> Triple(Geek.Accent, Geek.Accent, Geek.AccentDim)
        BadgeKind.Warn    -> Triple(Geek.Amber, Geek.Amber, Geek.AmberDim)
        BadgeKind.Danger  -> Triple(Geek.Danger, Geek.Danger, Geek.DangerDim)
        BadgeKind.Neutral -> Triple(Geek.InkDim, Geek.LineStrong, Geek.BgElev)
    }
    Box(
        modifier
            .fillMaxWidth()
            .background(bg)
            .border(BorderStroke(1.dp, border))
            .padding(horizontal = 14.dp, vertical = 10.dp)
    ) {
        Text(text, color = fg, style = TextStyle(fontFamily = GeekMono, fontSize = 12.sp, lineHeight = 18.sp))
    }
}

/** Scrolling mono console (`.fm-console` / `.console`). Auto-sticks to bottom. */
@Composable
fun Console(
    lines: List<String>,
    modifier: Modifier = Modifier,
    chrome: String = "// console",
) {
    val scroll = rememberScrollState()
    LaunchedEffect(lines.size) { scroll.animateScrollTo(scroll.maxValue) }
    Column(
        modifier
            .background(Geek.Bg)
            .border(BorderStroke(1.dp, Geek.Line))
    ) {
        Row(
            Modifier
                .fillMaxWidth()
                .background(Geek.BgElev2)
                .padding(horizontal = 16.dp, vertical = 8.dp)
        ) { MonoLabel(chrome, size = 10) }
        Column(
            Modifier
                .fillMaxWidth()
                .height(220.dp)
                .verticalScroll(scroll)
                .padding(horizontal = 16.dp, vertical = 12.dp),
            verticalArrangement = Arrangement.spacedBy(2.dp),
        ) {
            if (lines.isEmpty()) {
                Text("no activity yet", color = Geek.InkMuted,
                    style = TextStyle(fontFamily = GeekMono, fontSize = 12.sp))
            } else {
                lines.forEach {
                    Text(it, color = Geek.InkDim,
                        style = TextStyle(fontFamily = GeekMono, fontSize = 12.sp, lineHeight = 18.sp))
                }
            }
        }
    }
}

/** Thin accent progress bar (`.progress-bar`). */
@Composable
fun ProgressBar(fraction: Float, modifier: Modifier = Modifier) {
    Box(
        modifier
            .fillMaxWidth()
            .height(4.dp)
            .background(Geek.BgElev2)
    ) {
        Box(
            Modifier
                .fillMaxWidth(fraction.coerceIn(0f, 1f))
                .height(4.dp)
                .background(Geek.Accent)
        )
    }
}
