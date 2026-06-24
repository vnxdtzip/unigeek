package run.xid.unigeek.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import run.xid.unigeek.ui.components.GeekButton
import run.xid.unigeek.ui.components.GeekButtonStyle
import run.xid.unigeek.ui.components.MonoLabel
import run.xid.unigeek.ui.components.clickableNoRipple
import run.xid.unigeek.ui.connection.ConnStatus
import run.xid.unigeek.ui.connection.ConnectionViewModel
import run.xid.unigeek.ui.connection.InstallStatus
import run.xid.unigeek.ui.screens.ConnectScreen
import run.xid.unigeek.ui.screens.DeviceScreen
import run.xid.unigeek.ui.screens.files.FilesScreen
import run.xid.unigeek.ui.screens.install.InstallContent
import run.xid.unigeek.ui.screens.remote.RemoteScreen
import run.xid.unigeek.ui.theme.Geek
import run.xid.unigeek.ui.theme.GeekMono

@Composable
fun UniGeekApp() {
    val conn: ConnectionViewModel = viewModel()
    val nav = rememberNavController()

    NavHost(navController = nav, startDestination = "connect", modifier = Modifier.fillMaxSize()) {
        composable("connect") {
            LaunchedEffect(conn.status) {
                if (conn.status == ConnStatus.Connected) {
                    nav.navigate("device") { popUpTo("connect") { inclusive = true } }
                }
            }
            Box(Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.statusBars)) {
                ConnectScreen(conn, onFlash = { nav.navigate("install") })
            }
        }
        composable("device") {
            LaunchedEffect(conn.status, conn.installStatus) {
                if (conn.status != ConnStatus.Connected && conn.installStatus == InstallStatus.Idle) {
                    nav.navigate("connect") { popUpTo("device") { inclusive = true } }
                }
            }
            ConnectedShell(conn)
        }
        composable("install") {
            Box(Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.statusBars)) {
                StandaloneInstall(conn, onBack = { nav.popBackStack() })
            }
        }
    }
}

private data class Tab(val label: String, val glyph: String)

private val TABS = listOf(
    Tab("Device", "▢"),
    Tab("Remote", "▦"),
    Tab("Files", "▤"),
)

@Composable
private fun ConnectedShell(conn: ConnectionViewModel) {
    var tab by remember { mutableIntStateOf(0) }
    Scaffold(
        containerColor = Geek.Bg,
        bottomBar = { BottomBar(tab) { tab = it } },
    ) { pad ->
        Box(Modifier.fillMaxSize().padding(pad)) {
            when (tab) {
                0 -> DeviceScreen(conn)
                1 -> RemoteScreen(conn)
                else -> FilesScreen(conn)
            }
        }
    }
}

@Composable
private fun BottomBar(current: Int, onTab: (Int) -> Unit) {
    Row(Modifier.fillMaxWidth().background(Geek.Bg).windowInsetsPadding(WindowInsets.navigationBars)) {
        TABS.forEachIndexed { i, t ->
            val active = current == i
            Column(
                Modifier.weight(1f).height(62.dp).clickableNoRipple { onTab(i) }
                    .background(if (active) Geek.AccentDim else Color.Transparent)
                    .border(1.dp, Geek.Line).padding(top = 12.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Text(t.glyph, color = if (active) Geek.Accent else Geek.InkMuted, style = TextStyle(fontFamily = GeekMono, fontSize = 15.sp))
                Text(
                    t.label.uppercase(),
                    color = if (active) Geek.Accent else Geek.InkDim,
                    modifier = Modifier.padding(top = 6.dp),
                    style = TextStyle(fontFamily = GeekMono, fontSize = 10.sp, letterSpacing = 1.0.sp, fontWeight = if (active) FontWeight.Medium else FontWeight.Normal),
                )
            }
        }
    }
}

@Composable
private fun StandaloneInstall(conn: ConnectionViewModel, onBack: () -> Unit) {
    Column(Modifier.fillMaxSize()) {
        Row(Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 12.dp), verticalAlignment = Alignment.CenterVertically) {
            Box(Modifier.clickableNoRipple(onBack)) { MonoLabel("← back", color = Geek.InkDim) }
            Spacer(Modifier.size(12.dp))
            MonoLabel("install firmware", color = Geek.Accent)
        }
        InstallContent(conn, fixedBoardId = null, title = "Install")
    }
}
