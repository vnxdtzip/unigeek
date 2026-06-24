package run.xid.unigeek.feature.remote

import run.xid.unigeek.protocol.Frame
import run.xid.unigeek.protocol.FrameParser
import run.xid.unigeek.protocol.Proto
import run.xid.unigeek.protocol.encodeFrame
import run.xid.unigeek.transport.Transport
import java.util.concurrent.atomic.AtomicInteger

data class DeviceCaps(val width: Int, val height: Int, val touch: Boolean, val keyboard: Boolean)

/**
 * Screen-mirror client (ctx 'S'). Decodes HELLO / FRAME / FILL into ARGB rectangles
 * and sends navigation / touch / key control frames. Transport here is always USB
 * serial (the firmware only mirrors over UART). Mirror of `RemoteAccessClient.js`.
 *
 * Callbacks fire on the transport reader thread — keep them cheap and thread-safe.
 */
class RemoteClient {

    private val parser = FrameParser(::onFrame)
    private val seqGen = AtomicInteger(10)

    @Volatile private var transport: Transport? = null

    /** Whether incoming RGB565 should be byte-swapped (some panels store BE). */
    @Volatile var swapBytes: Boolean = false

    var onHello: ((DeviceCaps) -> Unit)? = null
    var onRect: ((x: Int, y: Int, w: Int, h: Int, argb: IntArray) -> Unit)? = null
    var onFill: ((x: Int, y: Int, w: Int, h: Int, argb: Int) -> Unit)? = null

    fun ingest(bytes: ByteArray) = parser.feed(bytes)
    fun attach(t: Transport) { transport = t }
    fun detach() { transport = null }

    private fun rd16(p: ByteArray, o: Int) = (p[o].toInt() and 0xFF) or ((p[o + 1].toInt() and 0xFF) shl 8)

    private fun rgb565ToArgb(v: Int): Int {
        val r = ((v ushr 11) and 0x1F) * 255 / 31
        val g = ((v ushr 5) and 0x3F) * 255 / 63
        val b = (v and 0x1F) * 255 / 31
        return (0xFF shl 24) or (r shl 16) or (g shl 8) or b
    }

    private fun onFrame(f: Frame) {
        if (f.ctx != Proto.CTX_SCR) return
        val p = f.payload
        when (f.type) {
            Proto.T_HELLO -> {
                val w = rd16(p, 0); val h = rd16(p, 2)
                val caps = if (p.size > 5) p[5].toInt() and 0xFF else 0
                onHello?.invoke(
                    DeviceCaps(
                        width = w, height = h,
                        touch = caps and Proto.CAP_TOUCH != 0,
                        keyboard = caps and Proto.CAP_KEYBOARD != 0,
                    )
                )
            }
            Proto.T_FRAME -> {
                val x = rd16(p, 0); val y = rd16(p, 2); val w = rd16(p, 4); val h = rd16(p, 6)
                val count = w * h
                if (count <= 0 || 8 + count * 2 > p.size) return
                val out = IntArray(count)
                val sw = swapBytes
                var o = 8
                for (i in 0 until count) {
                    val lo = p[o].toInt() and 0xFF
                    val hi = p[o + 1].toInt() and 0xFF
                    val v = if (sw) (lo shl 8) or hi else (hi shl 8) or lo
                    out[i] = rgb565ToArgb(v)
                    o += 2
                }
                onRect?.invoke(x, y, w, h, out)
            }
            Proto.T_FILL -> {
                val x = rd16(p, 0); val y = rd16(p, 2); val w = rd16(p, 4); val h = rd16(p, 6)
                val c = rd16(p, 8)
                onFill?.invoke(x, y, w, h, rgb565ToArgb(c))
            }
        }
    }

    private fun nextSeq() = seqGen.updateAndGet { (it + 1) and 0xFF }

    private suspend fun write(type: Int, payload: ByteArray?) {
        transport?.write(encodeFrame(Proto.CTX_SCR, type, nextSeq(), payload))
    }

    suspend fun startStream() = write(Proto.C_START, null)
    suspend fun stopStream() = write(Proto.C_STOP, null)

    suspend fun sendDirection(dir: Int, event: Int = 0) =
        write(Proto.C_INPUT, byteArrayOf(dir.toByte(), event.toByte()))

    suspend fun sendKey(code: Int) = write(Proto.C_KEY, byteArrayOf((code and 0xFF).toByte()))

    suspend fun sendTouch(x: Int, y: Int) = write(
        Proto.C_TOUCH,
        byteArrayOf(
            (x and 0xFF).toByte(), ((x ushr 8) and 0xFF).toByte(),
            (y and 0xFF).toByte(), ((y ushr 8) and 0xFF).toByte(),
        )
    )
}
