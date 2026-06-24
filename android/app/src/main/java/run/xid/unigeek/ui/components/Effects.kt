package run.xid.unigeek.ui.components

import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput

/** Tap with no ripple/indication — the site's controls are flat, ripple reads as Material. */
fun Modifier.clickableNoRipple(onClick: () -> Unit): Modifier =
    pointerInput(onClick) { detectTapGestures(onTap = { onClick() }) }

/** CRT scanline veil — the website's `.terminal::before` repeating gradient. */
fun Modifier.scanlines(alpha: Float = 0.05f): Modifier = drawWithContent {
    drawContent()
    val gap = 3f
    var y = 0f
    val c = Color.White.copy(alpha = alpha)
    while (y < size.height) {
        drawLine(c, Offset(0f, y), Offset(size.width, y), 1f)
        y += gap
    }
}

/** Slow opacity pulse for live-status indicators — `@keyframes pulse`. */
@Composable
fun rememberPulse(periodMs: Int = 2000): Float {
    val t = rememberInfiniteTransition(label = "pulse")
    val a by t.animateFloat(
        initialValue = 1f,
        targetValue = 0.35f,
        animationSpec = infiniteRepeatable(tween(periodMs / 2), RepeatMode.Reverse),
        label = "pulse-alpha",
    )
    return a
}
