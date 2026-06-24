package run.xid.unigeek

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.ui.Modifier
import run.xid.unigeek.ui.UniGeekApp
import run.xid.unigeek.ui.theme.Geek
import run.xid.unigeek.ui.theme.UniGeekTheme

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        enableEdgeToEdge()
        super.onCreate(savedInstanceState)
        setContent {
            UniGeekTheme {
                Surface(Modifier.fillMaxSize().background(Geek.Bg), color = Geek.Bg) {
                    UniGeekApp()
                }
            }
        }
    }
}
