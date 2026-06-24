package run.xid.unigeek.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import run.xid.unigeek.ui.components.Badge
import run.xid.unigeek.ui.components.BadgeKind
import run.xid.unigeek.ui.components.Banner
import run.xid.unigeek.ui.components.Console
import run.xid.unigeek.ui.components.GeekButton
import run.xid.unigeek.ui.components.GeekButtonStyle
import run.xid.unigeek.ui.components.MonoLabel
import run.xid.unigeek.ui.components.ProgressBar
import run.xid.unigeek.ui.components.SectionLabel
import run.xid.unigeek.ui.components.StatusDot
import run.xid.unigeek.ui.components.clickableNoRipple
import run.xid.unigeek.ui.components.formatBytes
import run.xid.unigeek.ui.connection.ConnectionViewModel
import run.xid.unigeek.ui.connection.InstallStatus
import run.xid.unigeek.ui.theme.Geek
import run.xid.unigeek.ui.theme.GeekMono
import run.xid.unigeek.ui.theme.GeekSans

@Composable
fun DeviceScreen(conn: ConnectionViewModel) {
    val scroll = rememberScrollState()
    Column(Modifier.fillMaxSize().verticalScroll(scroll).padding(horizontal = 20.dp, vertical = 16.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            SectionLabel("Device")
            Row(verticalAlignment = Alignment.CenterVertically) { StatusDot(); Spacer(Modifier.size(8.dp)); MonoLabel(conn.kind?.name?.uppercase() ?: "", color = Geek.Accent) }
        }

        Spacer(Modifier.height(18.dp))
        // Board / firmware card
        Column(Modifier.fillMaxWidth().background(Geek.BgElev).border(1.dp, Geek.Line).padding(20.dp)) {
            Text(conn.boardName ?: "Unknown board", color = Geek.Ink, style = TextStyle(fontFamily = GeekSans, fontSize = 22.sp, fontWeight = FontWeight.Medium))
            Spacer(Modifier.height(8.dp))
            Row {
                conn.detectedBoard?.chip?.let { MonoLabel(it, size = 10, color = Geek.Accent); Spacer(Modifier.size(10.dp)) }
                conn.info.version?.let { MonoLabel("fw v$it", size = 10) }
            }
        }

        // Storage
        if (conn.info.total > 0) {
            Spacer(Modifier.height(12.dp))
            Column(Modifier.fillMaxWidth().background(Geek.BgElev).border(1.dp, Geek.Line).padding(20.dp)) {
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    MonoLabel("storage")
                    MonoLabel("${formatBytes(conn.info.used)} / ${formatBytes(conn.info.total)}", size = 10, color = Geek.InkDim)
                }
                Spacer(Modifier.height(10.dp))
                ProgressBar(if (conn.info.total > 0) conn.info.used.toFloat() / conn.info.total else 0f)
            }
        }

        // Capabilities
        Spacer(Modifier.height(20.dp))
        MonoLabel("capabilities")
        Spacer(Modifier.height(10.dp))
        CapabilityRow("File Manager", conn.fmActive, if (conn.fmActive) "active" else "off")
        Spacer(Modifier.height(8.dp))
        CapabilityRow("Screen Mirror", conn.mirrorActive, if (conn.mirrorActive) "active" else "off")

        // Firmware update — USB only (flashing drives the ESP bootloader over USB-OTG).
        Spacer(Modifier.height(24.dp))
        MonoLabel("firmware")
        Spacer(Modifier.height(10.dp))
        if (conn.kind?.name == "Ble") {
            Text(
                "Firmware is flashed over USB. Reconnect with a USB-OTG cable to update.",
                color = Geek.InkMuted,
                style = TextStyle(fontFamily = GeekMono, fontSize = 12.sp, lineHeight = 18.sp),
            )
        } else {
            FirmwareUpdate(conn)
        }

        Spacer(Modifier.height(28.dp))
        GeekButton("Disconnect", { conn.disconnect() }, style = GeekButtonStyle.Ghost)
        Spacer(Modifier.height(20.dp))
    }
}

@Composable
private fun FirmwareUpdate(conn: ConnectionViewModel) {
    when (conn.installStatus) {
        InstallStatus.Done -> {
            Banner("Firmware updated — unplug and reconnect the device.", BadgeKind.Ok)
            Spacer(Modifier.height(12.dp))
            GeekButton("Done", { conn.resetInstall() }, style = GeekButtonStyle.Ghost)
        }
        InstallStatus.Downloading, InstallStatus.Flashing, InstallStatus.Error -> {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                MonoLabel(conn.installPhase, size = 10)
                Spacer(Modifier.size(12.dp))
                Box(Modifier.weight(1f)) { ProgressBar(conn.installProgress) }
                Spacer(Modifier.size(12.dp))
                MonoLabel("${(conn.installProgress * 100).toInt()}%", size = 10, color = Geek.Accent)
            }
            Spacer(Modifier.height(10.dp))
            Console(conn.installLog, chrome = "// flasher · esp-rom · usb-otg")
            conn.installError?.let {
                Spacer(Modifier.height(10.dp))
                Banner(it, BadgeKind.Danger)
                Spacer(Modifier.height(10.dp))
                Row {
                    GeekButton("Retry", { conn.updateFirmware() }, style = GeekButtonStyle.Primary)
                    Spacer(Modifier.size(8.dp))
                    GeekButton("Cancel", { conn.resetInstall() }, style = GeekButtonStyle.Ghost)
                }
            }
        }
        InstallStatus.Idle -> UpdateButton(conn)
    }
}

@Composable
private fun UpdateButton(conn: ConnectionViewModel) {
    val available = conn.updateAvailable
    val label = when {
        available -> "Update to v${conn.latestVersion}"
        conn.detectedBoard == null -> "Can't update — board not recognised"
        conn.latestVersion == null -> "Checking for updates…"
        else -> "Up to date · v${conn.info.version ?: "?"}"
    }
    Box(
        Modifier
            .fillMaxWidth()
            .background(if (available) Geek.Accent else Geek.BgElev2)
            .let { if (available) it.clickableNoRipple { conn.updateFirmware() } else it }
            .padding(vertical = 16.dp),
        contentAlignment = Alignment.Center,
    ) {
        Text(
            label.uppercase(),
            color = if (available) Geek.Bg else Geek.InkMuted,
            style = TextStyle(fontFamily = GeekMono, fontSize = 13.sp, letterSpacing = 1.0.sp, fontWeight = FontWeight.Bold),
        )
    }
}

@Composable
private fun CapabilityRow(name: String, active: Boolean, state: String) {
    Row(
        Modifier.fillMaxWidth().background(Geek.BgElev).border(1.dp, Geek.Line).padding(horizontal = 16.dp, vertical = 14.dp),
        horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(name, color = Geek.Ink, style = TextStyle(fontFamily = GeekSans, fontSize = 15.sp))
        Badge(state, if (active) BadgeKind.Ok else BadgeKind.Neutral)
    }
}

/** Shown in a feature tab when the device doesn't currently expose that feature. */
@Composable
fun FeatureUnavailable(title: String, reason: String, hint: String? = null) {
    Column(
        Modifier.fillMaxSize().padding(horizontal = 20.dp, vertical = 16.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        SectionLabel(title, Modifier.fillMaxWidth())
        Spacer(Modifier.height(40.dp))
        Box(Modifier.fillMaxWidth().background(Geek.BgElev).border(1.dp, Geek.LineStrong).padding(24.dp)) {
            Column {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("⚠", color = Geek.Amber, style = TextStyle(fontFamily = GeekMono, fontSize = 16.sp))
                    Spacer(Modifier.size(10.dp))
                    Text("Not available", color = Geek.Ink, style = TextStyle(fontFamily = GeekSans, fontSize = 16.sp, fontWeight = FontWeight.Medium))
                }
                Spacer(Modifier.height(12.dp))
                Text(reason, color = Geek.InkDim, style = TextStyle(fontFamily = GeekSans, fontSize = 14.sp, lineHeight = 21.sp))
                hint?.let {
                    Spacer(Modifier.height(12.dp))
                    Text(it, color = Geek.InkMuted, style = TextStyle(fontFamily = GeekMono, fontSize = 12.sp, lineHeight = 18.sp))
                }
            }
        }
    }
}
