package run.xid.unigeek.transport

/**
 * A raw byte pipe to the device. Framing lives above this layer (FrameParser),
 * so a Transport only moves bytes. Both implementations push inbound bytes through
 * the `onBytes` sink supplied at construction.
 */
interface Transport {
    /** Suggested PUT chunk size for file transfers on this link. */
    val chunkSize: Int

    /** A human label for logs ("USB · CP2102", "BLE · UniGeek"). */
    val label: String

    suspend fun write(data: ByteArray)
    suspend fun close()
}

enum class TransportKind { Usb, Ble }
