package run.xid.unigeek.transport

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import androidx.core.content.ContextCompat
import com.hoho.android.usbserial.driver.UsbSerialDriver
import com.hoho.android.usbserial.driver.UsbSerialPort
import com.hoho.android.usbserial.driver.UsbSerialProber
import com.hoho.android.usbserial.util.SerialInputOutputManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import run.xid.unigeek.protocol.Proto
import java.io.IOException
import kotlin.coroutines.resume

private const val ACTION_USB_PERMISSION = "run.xid.unigeek.USB_PERMISSION"

/** USB-OTG discovery, runtime permission, and port opening for ESP32 UART bridges. */
object UsbSupport {
    private fun manager(ctx: Context) =
        ctx.getSystemService(Context.USB_SERVICE) as UsbManager

    fun drivers(ctx: Context): List<UsbSerialDriver> =
        UsbSerialProber.getDefaultProber().findAllDrivers(manager(ctx))

    fun firstDriver(ctx: Context): UsbSerialDriver? = drivers(ctx).firstOrNull()

    fun hasPermission(ctx: Context, device: UsbDevice) = manager(ctx).hasPermission(device)

    /** Show the system USB-permission dialog and suspend until the user answers. */
    suspend fun ensurePermission(ctx: Context, device: UsbDevice): Boolean {
        val m = manager(ctx)
        if (m.hasPermission(device)) return true
        return suspendCancellableCoroutine { cont ->
            val receiver = object : BroadcastReceiver() {
                override fun onReceive(c: Context?, intent: Intent?) {
                    if (intent?.action != ACTION_USB_PERMISSION) return
                    try { ctx.unregisterReceiver(this) } catch (_: Exception) {}
                    val granted = intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)
                    if (cont.isActive) cont.resume(granted)
                }
            }
            ContextCompat.registerReceiver(
                ctx, receiver, IntentFilter(ACTION_USB_PERMISSION),
                ContextCompat.RECEIVER_NOT_EXPORTED,
            )
            val pi = PendingIntent.getBroadcast(
                ctx, 0,
                Intent(ACTION_USB_PERMISSION).setPackage(ctx.packageName),
                PendingIntent.FLAG_MUTABLE,
            )
            cont.invokeOnCancellation { try { ctx.unregisterReceiver(receiver) } catch (_: Exception) {} }
            m.requestPermission(device, pi)
        }
    }

    /** Open the first port of [driver]. Caller must already hold permission. */
    fun openPort(ctx: Context, driver: UsbSerialDriver): UsbSerialPort {
        val connection = manager(ctx).openDevice(driver.device)
            ?: throw IOException("Can't open USB device — grant the permission prompt and retry.")
        val port = driver.ports.first()
        port.open(connection)
        return port
    }
}

/**
 * Async serial transport for Remote + File Manager. A background reader thread
 * (SerialInputOutputManager) pumps inbound bytes into [onBytes]; writes block
 * briefly on the caller's IO dispatcher.
 *
 * The flasher does NOT use this — it needs synchronous reads + DTR/RTS and drives
 * the port itself (see EspFlasher).
 */
class UsbSerialTransport(
    private val port: UsbSerialPort,
    override val label: String,
    onBytes: (ByteArray) -> Unit,
    onClosed: (Throwable?) -> Unit,
    baudRate: Int = 115200,
) : Transport {

    override val chunkSize = Proto.PUT_CHUNK_SERIAL

    private val io = SerialInputOutputManager(
        port,
        object : SerialInputOutputManager.Listener {
            override fun onNewData(data: ByteArray) = onBytes(data)
            override fun onRunError(e: Exception) = onClosed(e)
        },
    )

    init {
        port.setParameters(baudRate, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)
        // Most boards run with DTR/RTS asserted; deassert avoids spurious resets.
        runCatching { port.dtr = false; port.rts = false }
        io.start()
    }

    override suspend fun write(data: ByteArray) = withContext(Dispatchers.IO) {
        port.write(data, 2000)
    }

    override suspend fun close() {
        runCatching { io.stop() }
        withContext(Dispatchers.IO) { runCatching { port.close() } }
    }
}
