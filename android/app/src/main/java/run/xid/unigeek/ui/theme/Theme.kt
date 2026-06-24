package run.xid.unigeek.ui.theme

import android.app.Activity
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.platform.LocalView
import androidx.core.view.WindowCompat

private val GeekColorScheme = darkColorScheme(
    primary = Geek.Accent,
    onPrimary = Geek.Bg,
    secondary = Geek.Amber,
    background = Geek.Bg,
    onBackground = Geek.Ink,
    surface = Geek.BgElev,
    onSurface = Geek.Ink,
    surfaceVariant = Geek.BgElev2,
    onSurfaceVariant = Geek.InkDim,
    error = Geek.Danger,
    outline = Geek.LineStrong,
)

@Composable
fun UniGeekTheme(content: @Composable () -> Unit) {
    val view = LocalView.current // app is always dark — no system-theme switch
    if (!view.isInEditMode) {
        SideEffect {
            val window = (view.context as Activity).window
            WindowCompat.getInsetsController(window, view).isAppearanceLightStatusBars = false
        }
    }
    MaterialTheme(
        colorScheme = GeekColorScheme,
        typography = GeekTypography,
        content = content,
    )
}
