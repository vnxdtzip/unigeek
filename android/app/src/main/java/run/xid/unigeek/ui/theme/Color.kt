package run.xid.unigeek.ui.theme

import androidx.compose.ui.graphics.Color

/**
 * Engineering-dark palette — ported 1:1 from the website's `globals.css` `:root`
 * tokens (oklch values resolved to sRGB). Keep these in sync with the site so the
 * app and web flasher read as one product.
 */
object Geek {
    val Bg        = Color(0xFF0B0D0E)
    val BgElev    = Color(0xFF12161A)
    val BgElev2   = Color(0xFF181D22)

    val Line       = Color(0x14E6E8EA) // rgba(230,232,234,0.08)
    val LineStrong = Color(0x2EE6E8EA) // rgba(230,232,234,0.18)

    val Ink      = Color(0xFFE6E8EA)
    val InkDim   = Color(0xFF9AA2A8)
    val InkMuted = Color(0xFF6B7379)

    val Accent     = Color(0xFF35E08A) // oklch(0.82 0.17 155)
    val AccentDim  = Color(0x2635E08A) // ~0.15 alpha
    val Amber      = Color(0xFFE0A93C) // oklch(0.78 0.14 75)
    val AmberDim   = Color(0x26E0A93C)
    val Danger     = Color(0xFFE5533D) // oklch(0.68 0.19 25)
    val DangerDim  = Color(0x26E5533D)

    // Achievement tiers (website .tier.*)
    val Bronze   = Color(0xFFC28668)
    val Silver   = Color(0xFFC4C8CC)
    val Gold     = Color(0xFFE2B840)
}
