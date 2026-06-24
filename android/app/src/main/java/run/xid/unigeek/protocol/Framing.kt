package run.xid.unigeek.protocol

/** CRC32 (reflected, poly 0xEDB88320) — matches the firmware and JS clients. */
object Crc32 {
    private val table = IntArray(256) {
        var c = it
        repeat(8) { c = if (c and 1 != 0) (c ushr 1) xor 0xEDB88320.toInt() else c ushr 1 }
        c
    }

    fun compute(bytes: ByteArray, from: Int = 0, to: Int = bytes.size): Long {
        var crc = -1 // 0xFFFFFFFF
        for (i in from until to) {
            crc = (crc ushr 8) xor table[(crc xor bytes[i].toInt()) and 0xFF]
        }
        return (crc.inv().toLong()) and 0xFFFFFFFFL
    }
}

/** Build a complete on-wire frame. */
fun encodeFrame(ctx: Int, type: Int, seq: Int, payload: ByteArray?): ByteArray {
    val len = payload?.size ?: 0
    val out = ByteArray(9 + len + 4)
    out[0] = Proto.SOF1.toByte()
    out[1] = Proto.SOF2.toByte()
    out[2] = ctx.toByte()
    out[3] = type.toByte()
    out[4] = seq.toByte()
    out[5] = (len and 0xFF).toByte()
    out[6] = ((len ushr 8) and 0xFF).toByte()
    out[7] = ((len ushr 16) and 0xFF).toByte()
    out[8] = ((len ushr 24) and 0xFF).toByte()
    if (payload != null) System.arraycopy(payload, 0, out, 9, len)
    val crc = Crc32.compute(out, 2, 9 + len)
    out[9 + len] = (crc and 0xFF).toByte()
    out[9 + len + 1] = ((crc ushr 8) and 0xFF).toByte()
    out[9 + len + 2] = ((crc ushr 16) and 0xFF).toByte()
    out[9 + len + 3] = ((crc ushr 24) and 0xFF).toByte()
    return out
}

/**
 * Streaming frame parser — tolerant of garbage, resyncs on SOF, drops CRC-bad
 * frames silently. Feed it raw bytes from any transport; it emits verified frames.
 */
class FrameParser(private val onFrame: (Frame) -> Unit) {
    private var state = 0
    private val hdr = ByteArray(7) // ctx, type, seq, len[4]
    private var hdrIdx = 0
    private var ctx = 0
    private var type = 0
    private var seq = 0
    private var len = 0
    private var payload = ByteArray(0)
    private var payloadIdx = 0
    private val crcBuf = ByteArray(4)
    private var crcIdx = 0

    fun feed(chunk: ByteArray, length: Int = chunk.size) {
        for (i in 0 until length) {
            val b = chunk[i].toInt() and 0xFF
            when (state) {
                0 -> if (b == Proto.SOF1) state = 1
                1 -> when (b) {
                    Proto.SOF2 -> { state = 2; hdrIdx = 0 }
                    Proto.SOF1 -> {}
                    else -> state = 0
                }
                2 -> {
                    hdr[hdrIdx++] = b.toByte()
                    if (hdrIdx == 7) {
                        ctx = hdr[0].toInt() and 0xFF
                        type = hdr[1].toInt() and 0xFF
                        seq = hdr[2].toInt() and 0xFF
                        len = (hdr[3].toInt() and 0xFF) or
                            ((hdr[4].toInt() and 0xFF) shl 8) or
                            ((hdr[5].toInt() and 0xFF) shl 16) or
                            ((hdr[6].toInt() and 0xFF) shl 24)
                        if (len < 0 || len > 65536) {
                            state = 0 // sanity: frames are banded, resync
                        } else {
                            payloadIdx = 0; crcIdx = 0
                            if (len > 0) { payload = ByteArray(len); state = 3 }
                            else { payload = ByteArray(0); state = 4 }
                        }
                    }
                }
                3 -> {
                    payload[payloadIdx++] = b.toByte()
                    if (payloadIdx >= len) state = 4
                }
                4 -> {
                    crcBuf[crcIdx++] = b.toByte()
                    if (crcIdx == 4) {
                        val expected = ((crcBuf[0].toLong() and 0xFF) or
                            ((crcBuf[1].toLong() and 0xFF) shl 8) or
                            ((crcBuf[2].toLong() and 0xFF) shl 16) or
                            ((crcBuf[3].toLong() and 0xFF) shl 24))
                        val buf = ByteArray(7 + len)
                        System.arraycopy(hdr, 0, buf, 0, 7)
                        if (len > 0) System.arraycopy(payload, 0, buf, 7, len)
                        if (Crc32.compute(buf) == expected) {
                            onFrame(Frame(ctx, type, seq, payload))
                        }
                        state = 0
                    }
                }
            }
        }
    }
}
