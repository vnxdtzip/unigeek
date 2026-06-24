package run.xid.unigeek.feature.install

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.net.HttpURLConnection
import java.net.URL

/**
 * Firmware artifacts. `.bin`s resolve through the same CORS-clean Cloudflare proxy
 * the web flasher uses; the version→board map is pulled live from the repo's
 * `release-notes/_boards.json` (raw GitHub), with a bundled fallback.
 */
object Firmware {
    const val PROXY = "https://bin-unigeek.xid.run"
    private const val BOARDS_JSON =
        "https://raw.githubusercontent.com/lshaf/unigeek/main/release-notes/_boards.json"

    /** Latest version known at build time — used if the live manifest can't be fetched. */
    const val FALLBACK_VERSION = "1.8.2"

    data class Release(val version: String, val boardIds: Set<String>)

    fun binUrl(version: String, boardId: String): String =
        "$PROXY/$version/unigeek-$boardId.bin"

    /** Fetch every release + its board set, newest first. Falls back to the full catalog. */
    suspend fun versions(): List<Release> = withContext(Dispatchers.IO) {
        runCatching {
            val text = httpGetText(BOARDS_JSON)
            val obj = JSONObject(text)
            val out = ArrayList<Release>()
            for (key in obj.keys()) {
                val arr = obj.getJSONArray(key)
                val ids = HashSet<String>(arr.length())
                for (i in 0 until arr.length()) ids.add(arr.getString(i))
                out.add(Release(key, ids))
            }
            out.sortedWith(compareByDescending(VersionComparator) { it.version })
        }.getOrElse {
            listOf(Release(FALLBACK_VERSION, Boards.all.map { it.id }.toSet()))
        }
    }

    /** Download a board's merged image, reporting (received, total). total is -1 if unknown. */
    suspend fun download(
        version: String,
        boardId: String,
        onProgress: (received: Long, total: Long) -> Unit,
    ): ByteArray = withContext(Dispatchers.IO) {
        val conn = (URL(binUrl(version, boardId)).openConnection() as HttpURLConnection).apply {
            connectTimeout = 15000
            readTimeout = 30000
            instanceFollowRedirects = true
        }
        try {
            val code = conn.responseCode
            if (code != 200) throw java.io.IOException(
                if (code == 404) "This board wasn't part of v$version — pick a newer firmware or a different board."
                else "Download failed (HTTP $code)."
            )
            val total = conn.contentLengthLong
            val out = ByteArrayOutputStream(maxOf(total.toInt(), 1 shl 20))
            conn.inputStream.use { input ->
                val buf = ByteArray(16 * 1024)
                var received = 0L
                while (true) {
                    val n = input.read(buf)
                    if (n < 0) break
                    out.write(buf, 0, n)
                    received += n
                    onProgress(received, total)
                }
            }
            out.toByteArray()
        } finally {
            conn.disconnect()
        }
    }

    private fun httpGetText(url: String): String {
        val conn = (URL(url).openConnection() as HttpURLConnection).apply {
            connectTimeout = 10000; readTimeout = 10000; instanceFollowRedirects = true
        }
        return try {
            conn.inputStream.bufferedReader().use { it.readText() }
        } finally {
            conn.disconnect()
        }
    }

    /** Compare semver-ish tags: >0 if [a] is newer than [b] ("1.8.10" > "1.8.2"). */
    fun compareVersions(a: String, b: String): Int {
        val pa = a.split('.', '-'); val pb = b.split('.', '-')
        val n = maxOf(pa.size, pb.size)
        for (i in 0 until n) {
            val x = pa.getOrNull(i)?.toIntOrNull() ?: 0
            val y = pb.getOrNull(i)?.toIntOrNull() ?: 0
            if (x != y) return x - y
        }
        return 0
    }

    private val VersionComparator = Comparator<String> { a, b -> compareVersions(a, b) }
}
