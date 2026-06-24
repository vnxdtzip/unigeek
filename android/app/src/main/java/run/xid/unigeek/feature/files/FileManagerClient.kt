package run.xid.unigeek.feature.files

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeoutOrNull
import run.xid.unigeek.protocol.Frame
import run.xid.unigeek.protocol.FrameParser
import run.xid.unigeek.protocol.Proto
import run.xid.unigeek.protocol.encodeFrame
import run.xid.unigeek.transport.Transport
import java.io.IOException
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicInteger

data class FileEntry(val isDir: Boolean, val name: String, val size: Long)

class FmException(message: String) : IOException(message)

/**
 * File-manager protocol client (ctx 'F'). Transport-agnostic: works identically
 * over USB serial and BLE. Mirror of the website's `createTransport` request layer —
 * seq-matched requests, streaming GET via repeated T_GET_CHUNK ending on an empty chunk.
 */
class FileManagerClient {

    private class Pending(
        val ctx: Int,
        val onChunk: ((ByteArray) -> Unit)?,
        val done: CompletableDeferred<ByteArray>,
    )

    private val parser = FrameParser(::onFrame)
    private val pending = ConcurrentHashMap<Int, Pending>()
    private val seqGen = AtomicInteger(0)

    @Volatile private var transport: Transport? = null

    val chunkSize: Int get() = transport?.chunkSize ?: Proto.PUT_CHUNK_SERIAL

    /** Called by the transport's reader with raw inbound bytes. */
    fun ingest(bytes: ByteArray) = parser.feed(bytes)

    fun attach(t: Transport) { transport = t }
    fun detach() {
        transport = null
        pending.values.forEach { it.done.completeExceptionally(IOException("disconnected")) }
        pending.clear()
    }

    private fun nextSeq(): Int {
        while (true) {
            val cur = seqGen.get()
            var n = (cur + 1) and 0xFF
            if (n == 0) n = 1
            if (seqGen.compareAndSet(cur, n)) return n
        }
    }

    private fun onFrame(f: Frame) {
        val p = pending[f.seq] ?: return
        if (p.ctx != f.ctx) return
        when (f.type) {
            Proto.T_OK -> p.done.complete(f.payload)
            Proto.T_ERR -> p.done.completeExceptionally(
                FmException(f.payload.toString(Charsets.UTF_8).ifEmpty { "device error" })
            )
            Proto.T_GET_CHUNK -> {
                val cb = p.onChunk ?: return
                if (f.payload.isEmpty()) p.done.complete(ByteArray(0)) else cb(f.payload)
            }
        }
    }

    private suspend fun send(
        type: Int,
        payload: ByteArray?,
        onChunk: ((ByteArray) -> Unit)? = null,
        idleMs: Long = 8000,
    ): ByteArray {
        val t = transport ?: throw IOException("Not connected")
        val seq = nextSeq()
        val activity = Channel<Unit>(Channel.CONFLATED)
        val done = CompletableDeferred<ByteArray>()
        val wrapped: ((ByteArray) -> Unit)? = onChunk?.let { cb ->
            { chunk -> cb(chunk); activity.trySend(Unit) }
        }
        pending[seq] = Pending(Proto.CTX_FM, wrapped, done)
        try {
            t.write(encodeFrame(Proto.CTX_FM, type, seq, payload))
            return coroutineScope {
                val watchdog = launch {
                    while (true) {
                        // Idle timeout that re-arms on every chunk of activity.
                        val tick = withTimeoutOrNull(idleMs) { activity.receive() }
                        if (tick == null) {
                            done.completeExceptionally(IOException("Request timed out"))
                            break
                        }
                    }
                }
                try { done.await() } finally { watchdog.cancel() }
            }
        } finally {
            pending.remove(seq)
        }
    }

    // ── High-level API ────────────────────────────────────────────────────────

    suspend fun info(): String = send(Proto.C_INFO, null).toString(Charsets.UTF_8)

    suspend fun list(path: String): List<FileEntry> {
        val text = send(Proto.C_LS, path.toByteArray(Charsets.UTF_8)).toString(Charsets.UTF_8)
        return text.split('\n').filter { it.isNotBlank() }.map { line ->
            val i = line.indexOf(':')
            val j = line.lastIndexOf(':')
            val kind = line.substring(0, i)
            val name = line.substring(i + 1, j)
            val size = line.substring(j + 1).trim().toLongOrNull() ?: 0L
            FileEntry(isDir = kind == "DIR", name = name, size = size)
        }.sortedWith(compareByDescending<FileEntry> { it.isDir }.thenBy { it.name.lowercase() })
    }

    /** Stream a file from the device. [onChunk] is invoked on the reader thread. */
    suspend fun get(path: String, onChunk: (ByteArray) -> Unit) {
        send(Proto.C_GET, path.toByteArray(Charsets.UTF_8), onChunk, idleMs = 15000)
    }

    /** Upload [data] to [path], reporting bytes sent. */
    suspend fun put(path: String, data: ByteArray, onProgress: (Long) -> Unit = {}) {
        val pathBytes = path.toByteArray(Charsets.UTF_8)
        val begin = ByteArray(4 + pathBytes.size)
        val total = data.size
        begin[0] = (total and 0xFF).toByte()
        begin[1] = ((total ushr 8) and 0xFF).toByte()
        begin[2] = ((total ushr 16) and 0xFF).toByte()
        begin[3] = ((total ushr 24) and 0xFF).toByte()
        System.arraycopy(pathBytes, 0, begin, 4, pathBytes.size)
        send(Proto.C_PUT_BEGIN, begin)
        var sent = 0
        val cs = chunkSize
        while (sent < data.size) {
            val end = minOf(sent + cs, data.size)
            send(Proto.C_PUT_CHUNK, data.copyOfRange(sent, end))
            sent = end
            onProgress(sent.toLong())
        }
        send(Proto.C_PUT_END, null)
    }

    suspend fun delete(path: String) { send(Proto.C_RM, path.toByteArray(Charsets.UTF_8)) }

    suspend fun mkdir(path: String) { send(Proto.C_MKDIR, path.toByteArray(Charsets.UTF_8)) }

    suspend fun createFile(path: String) { send(Proto.C_FTOUCH, path.toByteArray(Charsets.UTF_8)) }

    suspend fun rename(src: String, dst: String) {
        val s = src.toByteArray(Charsets.UTF_8)
        val d = dst.toByteArray(Charsets.UTF_8)
        val buf = ByteArray(s.size + 1 + d.size)
        System.arraycopy(s, 0, buf, 0, s.size)
        buf[s.size] = 0
        System.arraycopy(d, 0, buf, s.size + 1, d.size)
        send(Proto.C_MV, buf)
    }
}
