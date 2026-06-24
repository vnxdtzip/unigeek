package run.xid.unigeek.ui.screens

import android.Manifest
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
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
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import run.xid.unigeek.R
import run.xid.unigeek.transport.BleDevice
import run.xid.unigeek.ui.components.Banner
import run.xid.unigeek.ui.components.BadgeKind
import run.xid.unigeek.ui.components.GeekButton
import run.xid.unigeek.ui.components.GeekButtonStyle
import run.xid.unigeek.ui.components.MonoLabel
import run.xid.unigeek.ui.components.StatusDot
import run.xid.unigeek.ui.components.clickableNoRipple
import run.xid.unigeek.ui.components.pcbGrid
import run.xid.unigeek.ui.connection.ConnStatus
import run.xid.unigeek.ui.connection.ConnectionViewModel
import run.xid.unigeek.ui.theme.Geek
import run.xid.unigeek.ui.theme.GeekMono
import run.xid.unigeek.ui.theme.GeekSans

@Composable
fun ConnectScreen(conn: ConnectionViewModel, onFlash: () -> Unit) {
    val blePerm = rememberLauncherForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { result -> if (result.values.all { it }) conn.startBleScan() }

    val scroll = rememberScrollState()
    Column(
        Modifier.fillMaxSize().pcbGrid().verticalScroll(scroll).padding(horizontal = 20.dp),
    ) {
        // ── Centered boot-mark hero ──
        Spacer(Modifier.height(56.dp))
        Column(Modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally) {
            Image(
                painter = painterResource(R.drawable.ic_launcher_foreground),
                contentDescription = null,
                modifier = Modifier.size(112.dp),
            )
            Spacer(Modifier.height(18.dp))
            Row {
                Text("uni", color = Geek.Ink, style = TextStyle(fontFamily = GeekMono, fontSize = 16.sp, letterSpacing = 3.sp))
                Text("geek", color = Geek.Accent, style = TextStyle(fontFamily = GeekMono, fontSize = 16.sp, letterSpacing = 3.sp))
            }
            Spacer(Modifier.height(8.dp))
            val (dotColor, statusText, pulse) = when (conn.status) {
                ConnStatus.Connecting -> Triple(Geek.Amber, "connecting…", true)
                ConnStatus.Scanning -> Triple(Geek.Amber, "scanning…", true)
                ConnStatus.Error -> Triple(Geek.Danger, "not connected", false)
                else -> Triple(Geek.InkMuted, "awaiting device", false)
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                StatusDot(dotColor, pulse = pulse)
                Spacer(Modifier.size(8.dp))
                MonoLabel(statusText, size = 10, color = dotColor)
            }
            Spacer(Modifier.height(28.dp))
            Text(
                "Connect your device",
                textAlign = TextAlign.Center,
                style = TextStyle(fontFamily = GeekSans, fontSize = 30.sp, fontWeight = FontWeight.Normal, letterSpacing = (-0.5).sp),
                color = Geek.Ink,
            )
            Spacer(Modifier.height(12.dp))
            Text(
                "USB-OTG carries everything — remote, files, and flashing. Bluetooth carries remote and files.",
                modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp),
                textAlign = TextAlign.Center,
                color = Geek.InkDim,
                style = TextStyle(fontFamily = GeekSans, fontSize = 15.sp, lineHeight = 22.sp),
            )
        }

        Spacer(Modifier.height(44.dp))
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            TransportCard("USB-OTG", "Wired", "Remote · Files · Flash", Modifier.weight(1f), enabled = conn.status != ConnStatus.Connecting) { conn.connectUsb() }
            TransportCard("Bluetooth", "Wireless", "Remote · Files", Modifier.weight(1f), enabled = conn.status != ConnStatus.Connecting) { blePerm.launch(blePermissions()) }
        }

        conn.error?.let { Spacer(Modifier.height(16.dp)); Banner(it, BadgeKind.Danger) }

        if (conn.status == ConnStatus.Scanning || conn.bleDevices.isNotEmpty()) {
            Spacer(Modifier.height(24.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
                MonoLabel(if (conn.status == ConnStatus.Scanning) "scanning…" else "devices")
                if (conn.status == ConnStatus.Scanning) GeekButton("Stop", { conn.stopBleScan() }, style = GeekButtonStyle.Ghost)
            }
            Spacer(Modifier.height(10.dp))
            conn.bleDevices.forEach { d -> DeviceRow(d) { conn.connectBle(d) }; Spacer(Modifier.height(8.dp)) }
            if (conn.bleDevices.isEmpty()) Text("No UniGeek devices yet — keep the device nearby with Bluetooth on.", color = Geek.InkMuted, style = TextStyle(fontFamily = GeekMono, fontSize = 12.sp))
        }

        // Flash escape hatch for bare / other-firmware ESP32s.
        Spacer(Modifier.height(36.dp))
        Box(Modifier.fillMaxWidth().height(1.dp).background(Geek.Line))
        Spacer(Modifier.height(20.dp))
        MonoLabel("no unigeek firmware?")
        Spacer(Modifier.height(8.dp))
        Text("Flash a board over USB without connecting first — the chip is verified before writing.",
            color = Geek.InkDim, style = TextStyle(fontFamily = GeekSans, fontSize = 13.sp, lineHeight = 19.sp))
        Spacer(Modifier.height(12.dp))
        GeekButton("Flash firmware", onFlash, style = GeekButtonStyle.Default)
        Spacer(Modifier.height(40.dp))
    }
}

@Composable
private fun TransportCard(title: String, tag: String, sub: String, modifier: Modifier, enabled: Boolean, onClick: () -> Unit) {
    Box(
        modifier
            .background(Geek.BgElev)
            .border(1.dp, Geek.Line)
            .clickableNoRipple { if (enabled) onClick() }
            .padding(20.dp),
    ) {
        Column {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text(title, color = Geek.Ink, style = TextStyle(fontFamily = GeekSans, fontSize = 17.sp, fontWeight = FontWeight.Medium))
            }
            Spacer(Modifier.height(6.dp))
            MonoLabel(tag, size = 10, color = Geek.Accent)
            Spacer(Modifier.height(10.dp))
            MonoLabel(sub, size = 9)
        }
    }
}

@Composable
private fun DeviceRow(d: BleDevice, onClick: () -> Unit) {
    Row(
        Modifier.fillMaxWidth().background(Geek.BgElev).border(1.dp, Geek.Line).clickableNoRipple(onClick).padding(14.dp),
        horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically,
    ) {
        Column {
            Text(d.name ?: "(unnamed)", color = Geek.Ink, style = TextStyle(fontFamily = GeekSans, fontSize = 14.sp))
            MonoLabel(d.device.address, size = 10)
        }
        MonoLabel("${d.rssi} dBm", size = 10, color = Geek.Accent)
    }
}

internal fun blePermissions(): Array<String> =
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
        arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
    else
        arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
