package run.xid.unigeek.feature.install

import com.hoho.android.usbserial.driver.UsbSerialPort
import java.io.IOException
import java.util.ArrayDeque

/** Raised when sync fails so the UI can show "enter download mode" guidance. */
class BootloaderException(message: String) : IOException(message)

/**
 * Minimal ESP ROM-loader flasher — a Kotlin port of the slice of esptool the web
 * installer relies on. Drives a UsbSerialPort directly (synchronous reads + DTR/RTS),
 * so it does NOT use the async Transport layer.
 *
 * Strategy: stub-less ROM loader, uncompressed FLASH_DATA, single merged image at
 * offset 0 — exactly the `{ address: 0 }` write the website does, minus the stub's
 * compression. Robust over clever; flashing speed is the cost.
 *
 * Reset uses the classic CP2102/CH340 auto-reset (RTS→EN, DTR→IO0). Native-USB-CDC
 * boards (most ESP32-S3) may need a manual BOOT+RESET; we surface that on sync fail.
 */
class EspFlasher(
    private val port: UsbSerialPort,
    private val log: (String) -> Unit,
) {
    // ESP ROM command opcodes.
    private companion object {
        const val FLASH_BEGIN = 0x02
        const val FLASH_DATA = 0x03
        const val FLASH_END = 0x04
        const val SYNC = 0x08
        const val READ_REG = 0x0A
        const val SPI_ATTACH = 0x0D
        const val CHANGE_BAUD = 0x0F

        const val CHIP_DETECT_MAGIC_REG = 0x40001000
        const val FLASH_BLOCK = 0x4000 // 16 KB ROM write block
        const val STATUS_LEN = 2

        val SYNC_PAYLOAD = byteArrayOf(0x07, 0x07, 0x12, 0x20) + ByteArray(32) { 0x55 }
    }

    private val rxQueue = ArrayDeque<Int>()
    private var chipName = "unknown"

    // ── public entry ────────────────────────────────────────────────────────────

    /** Flash [image] at offset 0. Validates the chip family against [expectS3]. */
    fun flash(image: ByteArray, expectS3: Boolean, flashBaud: Int, onProgress: (Float) -> Unit) {
        port.setParameters(115200, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)

        connect()
        detectChip()
        log("detected chip: $chipName")
        if (chipName.contains("S3") != expectS3) {
            throw IOException("Wrong chip — board expects ${if (expectS3) "ESP32-S3" else "ESP32"}, found $chipName. Disconnect to avoid bricking.")
        }

        maybeRaiseBaud(flashBaud)
        spiAttach()

        log("erasing flash …")
        flashBegin(image.size)

        val blocks = (image.size + FLASH_BLOCK - 1) / FLASH_BLOCK
        for (seq in 0 until blocks) {
            val start = seq * FLASH_BLOCK
            val block = ByteArray(FLASH_BLOCK) { 0xFF.toByte() }
            val len = minOf(FLASH_BLOCK, image.size - start)
            System.arraycopy(image, start, block, 0, len)
            flashData(block, seq)
            onProgress((seq + 1).toFloat() / blocks)
        }

        flashEnd()
        log("resetting board …")
        hardReset()
    }

    // ── connection / chip ─────────────────────────────────────────────────────────

    private fun connect() {
        var lastErr: Exception? = null
        for (attempt in 0 until 6) {
            enterBootloader()
            drainInput()
            repeat(5) {
                try {
                    command(SYNC, SYNC_PAYLOAD, timeoutMs = 500, checkStatus = false)
                    drainInput() // ROM echoes the sync several times
                    log("sync ok")
                    return
                } catch (e: Exception) { lastErr = e }
            }
        }
        throw BootloaderException(
            "Couldn't reach the bootloader. Put the board in download mode (hold BOOT, tap RESET, release BOOT) and press Flash again." +
                (lastErr?.let { " [${it.message}]" } ?: "")
        )
    }

    private fun detectChip() {
        val magic = readReg(CHIP_DETECT_MAGIC_REG)
        chipName = when (magic) {
            0x00f01d83L -> "ESP32"
            0x000007c6L -> "ESP32-S2"
            0x00000009L -> "ESP32-S3"
            0xeb004136L -> "ESP32-S3"
            0x6921506fL, 0x1b31506fL -> "ESP32-C3"
            else -> "unknown(0x${magic.toString(16)})"
        }
    }

    // ── ROM commands ──────────────────────────────────────────────────────────────

    private fun readReg(addr: Int): Long {
        val data = le32(addr)
        val (value, _) = command(READ_REG, data, timeoutMs = 1000)
        return value
    }

    private fun spiAttach() {
        // ROM (no stub) expects an 8-byte arg: spi config word + 4 reserved.
        command(SPI_ATTACH, ByteArray(8), timeoutMs = 3000)
    }

    private fun flashBegin(size: Int) {
        val blocks = (size + FLASH_BLOCK - 1) / FLASH_BLOCK
        val fiveWords = chipName != "ESP32" // S2/S3/C3 ROM FLASH_BEGIN carries an encrypted flag
        val data = le32(size) + le32(blocks) + le32(FLASH_BLOCK) + le32(0) +
            (if (fiveWords) le32(0) else ByteArray(0))
        command(FLASH_BEGIN, data, timeoutMs = 30000) // includes the erase
    }

    private fun flashData(block: ByteArray, seq: Int) {
        val header = le32(block.size) + le32(seq) + le32(0) + le32(0)
        command(FLASH_DATA, header + block, checksum = checksum(block), timeoutMs = 5000)
    }

    private fun flashEnd() {
        // 1 = stay in loader (clean); we trigger the run via a hard reset.
        command(FLASH_END, le32(1), timeoutMs = 3000, checkStatus = false)
    }

    private fun maybeRaiseBaud(target: Int) {
        if (target <= 115200) return
        try {
            command(CHANGE_BAUD, le32(target) + le32(0), timeoutMs = 3000, checkStatus = false)
            port.setParameters(target, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)
            Thread.sleep(50)
            drainInput()
            // Verify the link survives the switch; revert to 115200 if not.
            command(SYNC, SYNC_PAYLOAD, timeoutMs = 500, checkStatus = false)
            drainInput()
            log("baud raised to $target")
        } catch (_: Exception) {
            log("baud change failed — staying at 115200")
            port.setParameters(115200, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)
            drainInput()
        }
    }

    // ── SLIP command transport ──────────────────────────────────────────────────────

    /** Send a command, return (value word, response body). Throws on error/timeout. */
    private fun command(
        op: Int,
        data: ByteArray,
        checksum: Int = 0,
        timeoutMs: Long = 3000,
        checkStatus: Boolean = true,
    ): Pair<Long, ByteArray> {
        val header = byteArrayOf(
            0x00, op.toByte(),
            (data.size and 0xFF).toByte(), ((data.size ushr 8) and 0xFF).toByte(),
            (checksum and 0xFF).toByte(), ((checksum ushr 8) and 0xFF).toByte(),
            ((checksum ushr 16) and 0xFF).toByte(), ((checksum ushr 24) and 0xFF).toByte(),
        )
        port.write(slipEncode(header + data), 2000)

        val deadline = System.nanoTime() + timeoutMs * 1_000_000
        while (System.nanoTime() < deadline) {
            val pkt = readPacket(deadline) ?: break
            if (pkt.size < 8) continue
            if ((pkt[0].toInt() and 0xFF) != 0x01) continue        // must be a response
            if ((pkt[1].toInt() and 0xFF) != op) continue          // for this op
            val size = u16(pkt, 2)
            val value = u32(pkt, 4)
            val body = pkt.copyOfRange(8, minOf(8 + size, pkt.size))
            if (checkStatus && body.size >= STATUS_LEN) {
                val statusByte = body[body.size - STATUS_LEN].toInt() and 0xFF
                if (statusByte != 0) {
                    val err = body[body.size - 1].toInt() and 0xFF
                    throw IOException("command 0x${op.toString(16)} failed (status $statusByte / err $err)")
                }
            }
            return value to body
        }
        throw IOException("no response to command 0x${op.toString(16)}")
    }

    // ── reset lines ─────────────────────────────────────────────────────────────────

    /** Classic auto-reset into the download bootloader. */
    private fun enterBootloader() {
        try {
            port.dtr = false; port.rts = true   // EN low → in reset
            Thread.sleep(100)
            port.dtr = true; port.rts = false    // EN high (run), IO0 low (download)
            Thread.sleep(50)
            port.dtr = false                     // release IO0
            Thread.sleep(50)
        } catch (_: Exception) { /* some CDC ports ignore line control */ }
    }

    private fun hardReset() {
        try {
            port.dtr = false; port.rts = true
            Thread.sleep(100)
            port.rts = false
        } catch (_: Exception) {}
    }

    // ── low-level I/O ─────────────────────────────────────────────────────────────────

    private fun drainInput() {
        rxQueue.clear()
        val tmp = ByteArray(256)
        repeat(4) { if (port.read(tmp, 30) <= 0) return }
    }

    private fun readByte(deadline: Long): Int {
        while (rxQueue.isEmpty()) {
            if (System.nanoTime() > deadline) return -1
            val tmp = ByteArray(256)
            val n = port.read(tmp, 100)
            for (i in 0 until n) rxQueue.add(tmp[i].toInt() and 0xFF)
        }
        return rxQueue.poll()
    }

    /** Read one SLIP packet (between 0xC0 delimiters), unescaped. */
    private fun readPacket(deadline: Long): ByteArray? {
        // seek frame start
        while (true) {
            val b = readByte(deadline)
            if (b < 0) return null
            if (b == 0xC0) break
        }
        val out = ArrayList<Byte>(64)
        while (true) {
            val b = readByte(deadline)
            if (b < 0) return null
            when (b) {
                0xC0 -> if (out.isEmpty()) continue else return out.toByteArray()
                0xDB -> {
                    val n = readByte(deadline); if (n < 0) return null
                    out.add(when (n) { 0xDC -> 0xC0.toByte(); 0xDD -> 0xDB.toByte(); else -> n.toByte() })
                }
                else -> out.add(b.toByte())
            }
        }
    }

    private fun slipEncode(p: ByteArray): ByteArray {
        val out = ArrayList<Byte>(p.size + 2)
        out.add(0xC0.toByte())
        for (b in p) when (b.toInt() and 0xFF) {
            0xC0 -> { out.add(0xDB.toByte()); out.add(0xDC.toByte()) }
            0xDB -> { out.add(0xDB.toByte()); out.add(0xDD.toByte()) }
            else -> out.add(b)
        }
        out.add(0xC0.toByte())
        return out.toByteArray()
    }

    // ── helpers ─────────────────────────────────────────────────────────────────────

    private fun checksum(data: ByteArray): Int {
        var chk = 0xEF
        for (b in data) chk = chk xor (b.toInt() and 0xFF)
        return chk
    }

    private fun le32(v: Int) = byteArrayOf(
        (v and 0xFF).toByte(), ((v ushr 8) and 0xFF).toByte(),
        ((v ushr 16) and 0xFF).toByte(), ((v ushr 24) and 0xFF).toByte(),
    )

    private fun u16(b: ByteArray, o: Int) = (b[o].toInt() and 0xFF) or ((b[o + 1].toInt() and 0xFF) shl 8)

    private fun u32(b: ByteArray, o: Int): Long =
        (b[o].toLong() and 0xFF) or ((b[o + 1].toLong() and 0xFF) shl 8) or
            ((b[o + 2].toLong() and 0xFF) shl 16) or ((b[o + 3].toLong() and 0xFF) shl 24)
}
