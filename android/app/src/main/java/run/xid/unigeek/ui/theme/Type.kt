package run.xid.unigeek.ui.theme

import androidx.compose.material3.Typography
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.sp

/**
 * The website uses Geist / Geist Mono. Those aren't bundled here, so we fall back
 * to the platform sans + monospace. Drop `geist.ttf` / `geist_mono.ttf` into
 * res/font and swap the two families below to get pixel-parity later.
 */
val GeekSans: FontFamily = FontFamily.SansSerif
val GeekMono: FontFamily = FontFamily.Monospace

val GeekTypography = Typography(
    bodyLarge = TextStyle(fontFamily = GeekSans, fontSize = 15.sp, lineHeight = 22.sp),
    bodyMedium = TextStyle(fontFamily = GeekSans, fontSize = 14.sp, lineHeight = 20.sp),
    bodySmall = TextStyle(fontFamily = GeekSans, fontSize = 13.sp, lineHeight = 18.sp),
    titleLarge = TextStyle(fontFamily = GeekSans, fontSize = 28.sp, letterSpacing = (-0.5).sp),
    labelLarge = TextStyle(fontFamily = GeekMono, fontSize = 13.sp, letterSpacing = 0.5.sp),
    labelMedium = TextStyle(fontFamily = GeekMono, fontSize = 11.sp, letterSpacing = 0.8.sp),
    labelSmall = TextStyle(fontFamily = GeekMono, fontSize = 10.sp, letterSpacing = 1.0.sp),
)

/** Uppercase mono label — the website's pervasive `font-mono; text-transform: uppercase`. */
val MonoLabel = TextStyle(
    fontFamily = GeekMono,
    fontSize = 11.sp,
    letterSpacing = 1.2.sp,
    textDecoration = TextDecoration.None,
)
