package run.xid.unigeek.ui.components

fun formatBytes(n: Long): String = when {
    n <= 0 -> "0 B"
    n < 1024 -> "$n B"
    n < 1024 * 1024 -> "%.1f KB".format(n / 1024.0)
    n < 1024L * 1024 * 1024 -> "%.1f MB".format(n / (1024.0 * 1024))
    else -> "%.2f GB".format(n / (1024.0 * 1024 * 1024))
}
