package run.xid.unigeek.protocol

/**
 * UniGeek serial/BLE framing — must stay byte-compatible with the firmware and the
 * website clients (`RemoteAccessClient.js`, `FileManagerClient.js`).
 *
 * Frame: A5 5A | ctx type seq | len[4 LE] | payload | crc32[4 LE]
 * CRC32 (reflected, poly 0xEDB88320) is computed over [ctx .. end of payload].
 */
object Proto {
    const val SOF1 = 0xA5
    const val SOF2 = 0x5A

    // Subsystem contexts (ASCII letters).
    const val CTX_SCR = 'S'.code // 0x53 — screen mirror / remote
    const val CTX_FM = 'F'.code  // 0x46 — file manager

    // ── Remote (Screen Mirror), ctx 'S' ──
    const val C_START = 0x01
    const val C_STOP = 0x02
    const val C_INPUT = 0x10 // [dir:1][event:1]
    const val C_TOUCH = 0x11 // [x:2][y:2]
    const val C_KEY = 0x12   // [char:1]
    const val T_HELLO = 0xA0 // [w:2][h:2][format:1][caps:1]
    const val T_FRAME = 0xA1 // [x:2][y:2][w:2][h:2][rgb565 LE …]
    const val T_FILL = 0xA2  // [x:2][y:2][w:2][h:2][color:2]
    const val CAP_TOUCH = 0x01
    const val CAP_KEYBOARD = 0x02

    // INavigation::Direction
    const val DIR_UP = 1
    const val DIR_DOWN = 2
    const val DIR_LEFT = 3
    const val DIR_RIGHT = 4
    const val DIR_PRESS = 5
    const val DIR_BACK = 6

    // ── File manager, ctx 'F' ──
    const val T_OK = 0xF0
    const val T_ERR = 0xF1
    const val T_GET_CHUNK = 0xF2
    const val C_INFO = 0x01
    const val C_LS = 0x02
    const val C_STAT = 0x03
    const val C_GET = 0x10
    const val C_PUT_BEGIN = 0x20 // [total:4 LE][path]
    const val C_PUT_CHUNK = 0x21
    const val C_PUT_END = 0x22
    const val C_RM = 0x30
    const val C_MV = 0x31 // [src]\0[dst]
    const val C_MKDIR = 0x32
    const val C_FTOUCH = 0x33 // create empty file (distinct from remote C_TOUCH)

    // Nordic UART Service (BleFileManager.cpp).
    const val NUS_SERVICE = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
    const val NUS_RX = "6e400002-b5a3-f393-e0a9-e50e24dcca9e" // host -> device (write)
    const val NUS_TX = "6e400003-b5a3-f393-e0a9-e50e24dcca9e" // device -> host (notify)

    // Per-transport tuning (mirrors the web client).
    const val PUT_CHUNK_SERIAL = 1024
    const val PUT_CHUNK_BLE = 256
    const val BLE_WRITE_MAX = 180 // ATT_MTU - 3, conservative
}

data class Frame(val ctx: Int, val type: Int, val seq: Int, val payload: ByteArray) {
    override fun equals(other: Any?) =
        other is Frame && ctx == other.ctx && type == other.type &&
            seq == other.seq && payload.contentEquals(other.payload)

    override fun hashCode(): Int {
        var r = ctx; r = 31 * r + type; r = 31 * r + seq
        r = 31 * r + payload.contentHashCode(); return r
    }
}
