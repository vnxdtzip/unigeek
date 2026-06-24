package run.xid.unigeek.ui.screens.install

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import run.xid.unigeek.feature.install.Board
import run.xid.unigeek.feature.install.Boards
import run.xid.unigeek.ui.components.Banner
import run.xid.unigeek.ui.components.BadgeKind
import run.xid.unigeek.ui.components.Console
import run.xid.unigeek.ui.components.GeekButton
import run.xid.unigeek.ui.components.GeekButtonStyle
import run.xid.unigeek.ui.components.MonoLabel
import run.xid.unigeek.ui.components.ProgressBar
import run.xid.unigeek.ui.components.SectionLabel
import run.xid.unigeek.ui.components.clickableNoRipple
import run.xid.unigeek.ui.connection.ConnectionViewModel
import run.xid.unigeek.ui.connection.InstallStatus
import run.xid.unigeek.ui.theme.Geek
import run.xid.unigeek.ui.theme.GeekMono
import run.xid.unigeek.ui.theme.GeekSans

/**
 * Firmware flash UI. When [fixedBoardId] is set (the Update tab for a detected
 * board) it locks to that board with an option to pick another; otherwise it shows
 * the full board picker (standalone flash for a bare/other-firmware ESP32).
 */
@Composable
fun InstallContent(conn: ConnectionViewModel, fixedBoardId: String?, title: String) {
    val scroll = rememberScrollState()
    var chosen by remember(fixedBoardId) { mutableStateOf(fixedBoardId) }
    var pickerOpen by remember(fixedBoardId) { mutableStateOf(fixedBoardId == null) }
    val supported = conn.supportedBoardIds
    val busy = conn.installStatus == InstallStatus.Downloading || conn.installStatus == InstallStatus.Flashing

    Column(Modifier.fillMaxWidth().verticalScroll(scroll).padding(horizontal = 20.dp, vertical = 16.dp)) {
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween, verticalAlignment = Alignment.CenterVertically) {
            SectionLabel(title)
            VersionPicker(conn.selectedVersion, conn.versions.map { it.version }, enabled = !busy) { conn.selectVersion(it) }
        }

        Spacer(Modifier.height(16.dp))
        if (fixedBoardId != null && !pickerOpen) {
            // Locked to the connected board.
            val board = Boards.byId[fixedBoardId]
            Box(Modifier.fillMaxWidth().background(Geek.AccentDim).border(1.dp, Geek.Accent).padding(20.dp)) {
                Column {
                    MonoLabel("connected board", size = 10, color = Geek.Accent)
                    Spacer(Modifier.height(6.dp))
                    Text(board?.name ?: fixedBoardId, color = Geek.Ink, style = TextStyle(fontFamily = GeekSans, fontSize = 20.sp, fontWeight = FontWeight.Medium))
                    board?.chip?.let { Spacer(Modifier.height(6.dp)); MonoLabel(it, size = 10, color = Geek.Accent) }
                }
            }
            Spacer(Modifier.height(10.dp))
            Text("Flash a different board →", color = Geek.InkDim, modifier = Modifier.clickableNoRipple { pickerOpen = true; chosen = null },
                style = TextStyle(fontFamily = GeekMono, fontSize = 11.sp))
        } else {
            MonoLabel("select board")
            Spacer(Modifier.height(10.dp))
            Boards.all.chunked(2).forEach { row ->
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                    row.forEach { board ->
                        BoardCard(board, chosen == board.id, board.id in supported, Modifier.weight(1f)) { chosen = board.id }
                    }
                    if (row.size == 1) Spacer(Modifier.weight(1f))
                }
                Spacer(Modifier.height(10.dp))
            }
        }

        Spacer(Modifier.height(12.dp))
        FlashPanel(conn, chosen?.let { Boards.byId[it] }, busy)
        Spacer(Modifier.height(20.dp))
    }
}

@Composable
private fun VersionPicker(current: String, versions: List<String>, enabled: Boolean, onPick: (String) -> Unit) {
    var open by remember { mutableStateOf(false) }
    Box {
        Row(Modifier.border(1.dp, Geek.Line).background(Geek.BgElev).clickableNoRipple { if (enabled) open = true }.padding(horizontal = 12.dp, vertical = 6.dp), verticalAlignment = Alignment.CenterVertically) {
            Text("v$current", color = Geek.Ink, style = TextStyle(fontFamily = GeekMono, fontSize = 12.sp))
            Spacer(Modifier.size(6.dp)); Text("▾", color = Geek.InkMuted, style = TextStyle(fontFamily = GeekMono, fontSize = 12.sp))
        }
        DropdownMenu(expanded = open, onDismissRequest = { open = false }, modifier = Modifier.background(Geek.BgElev)) {
            versions.forEach { v ->
                DropdownMenuItem(text = { Text("v$v", color = if (v == current) Geek.Accent else Geek.Ink, style = TextStyle(fontFamily = GeekMono, fontSize = 13.sp)) }, onClick = { onPick(v); open = false })
            }
        }
    }
}

@Composable
private fun BoardCard(board: Board, selected: Boolean, supported: Boolean, modifier: Modifier, onClick: () -> Unit) {
    Box(
        modifier.background(if (selected) Geek.AccentDim else Geek.BgElev).border(1.dp, if (selected) Geek.Accent else Geek.Line).clickableNoRipple { if (supported) onClick() }.padding(16.dp),
    ) {
        Column {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                Text(board.name, color = if (supported) Geek.Ink else Geek.InkMuted, style = TextStyle(fontFamily = GeekSans, fontSize = 14.sp, fontWeight = FontWeight.Medium), modifier = Modifier.weight(1f))
                if (selected) Text("✓", color = Geek.Accent, style = TextStyle(fontFamily = GeekMono, fontSize = 13.sp))
            }
            Spacer(Modifier.height(8.dp))
            Row(verticalAlignment = Alignment.CenterVertically) {
                MonoLabel(board.chip, size = 9, color = if (selected) Geek.Accent else Geek.InkMuted)
                if (!supported) { Spacer(Modifier.size(6.dp)); Text("not in build", color = Geek.InkMuted, style = TextStyle(fontFamily = GeekMono, fontSize = 9.sp)) }
            }
        }
    }
}

@Composable
private fun FlashPanel(conn: ConnectionViewModel, board: Board?, busy: Boolean) {
    Column(Modifier.fillMaxWidth().border(1.dp, Geek.Line).background(Geek.BgElev)) {
        Column(Modifier.padding(16.dp)) {
            KV("Board", board?.name ?: "—")
            KV("Chip", board?.chip ?: "—")
            KV("Firmware", "v${conn.selectedVersion}")
            KV("Target", board?.let { "${it.id}.bin" } ?: "—")
        }
        Console(conn.installLog, chrome = "// flasher · esp-rom · usb-otg")
        Column(Modifier.padding(16.dp)) {
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                MonoLabel(conn.installPhase, size = 10)
                Spacer(Modifier.size(12.dp))
                Box(Modifier.weight(1f)) { ProgressBar(conn.installProgress) }
                Spacer(Modifier.size(12.dp))
                MonoLabel("${(conn.installProgress * 100).toInt()}%", size = 10, color = Geek.Accent)
            }
            conn.installError?.let { Spacer(Modifier.height(12.dp)); Banner(it, BadgeKind.Danger) }
            Spacer(Modifier.height(16.dp))
            if (conn.installStatus == InstallStatus.Done) {
                Banner("Flash complete — unplug and reconnect the board.", BadgeKind.Ok)
                Spacer(Modifier.height(12.dp))
                GeekButton("Done", { conn.resetInstall() }, style = GeekButtonStyle.Ghost)
            } else {
                FlashButton(
                    label = when (conn.installStatus) {
                        InstallStatus.Downloading -> "Downloading…"
                        InstallStatus.Flashing -> "Flashing…"
                        InstallStatus.Error -> "Retry"
                        else -> "Flash firmware"
                    },
                    enabled = board != null && !busy,
                ) { board?.let { conn.flash(it.id) } }
                if (board == null) { Spacer(Modifier.height(8.dp)); Text("Pick a board to begin.", color = Geek.InkMuted, style = TextStyle(fontFamily = GeekMono, fontSize = 11.sp)) }
            }
        }
    }
}

@Composable
private fun FlashButton(label: String, enabled: Boolean, onClick: () -> Unit) {
    Box(
        Modifier.fillMaxWidth().background(if (enabled) Geek.Accent else Geek.BgElev2).clickableNoRipple { if (enabled) onClick() }.padding(vertical = 18.dp),
        contentAlignment = Alignment.Center,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("▶", color = if (enabled) Geek.Bg else Geek.InkMuted, style = TextStyle(fontFamily = GeekMono, fontSize = 12.sp))
            Spacer(Modifier.size(10.dp))
            Text(label.uppercase(), color = if (enabled) Geek.Bg else Geek.InkMuted, style = TextStyle(fontFamily = GeekMono, fontSize = 14.sp, letterSpacing = 1.2.sp, fontWeight = FontWeight.Bold))
        }
    }
}

@Composable
private fun KV(key: String, value: String) {
    Row(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        Box(Modifier.size(width = 90.dp, height = 18.dp)) { MonoLabel(key, size = 10) }
        Text(value, color = Geek.Ink, style = TextStyle(fontFamily = GeekMono, fontSize = 12.sp))
    }
}
