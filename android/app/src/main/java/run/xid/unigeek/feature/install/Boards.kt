package run.xid.unigeek.feature.install

/** A flashable board. Mirrors `website/content/boards.js` (id / name / chip / flashBaud). */
data class Board(
    val id: String,
    val name: String,
    val chip: String,        // "ESP32" | "ESP32-S3"
    val flashBaud: Int,
) {
    val isS3: Boolean get() = chip.contains("S3")
}

/**
 * Static catalog, kept in sync with the website's BOARDS list. The set of boards
 * actually shipped in a given firmware version comes from `_boards.json` at runtime
 * (Firmware.versions); this just supplies display name + chip + flash baud.
 */
object Boards {
    val all: List<Board> = listOf(
        Board("m5stickcplus_11", "M5StickC Plus 1.1", "ESP32", 115200),
        Board("m5stickcplus_2", "M5StickC Plus 2", "ESP32", 115200),
        Board("m5sticks3", "M5Stick S3", "ESP32-S3", 921600),
        Board("m5_cardputer", "M5 Cardputer", "ESP32-S3", 921600),
        Board("m5_cardputer_adv", "M5 Cardputer ADV", "ESP32-S3", 921600),
        Board("m5_cores3", "M5 CoreS3", "ESP32-S3", 921600),
        Board("t_display", "T-Display", "ESP32", 921600),
        Board("t_display_s3", "T-Display S3", "ESP32-S3", 921600),
        Board("t_display_s3_touch", "T-Display S3 Touch", "ESP32-S3", 921600),
        Board("t_lora_pager", "T-Lora Pager", "ESP32-S3", 921600),
        Board("t_embed_cc1101", "T-Embed CC1101", "ESP32-S3", 921600),
        Board("diy_smoochie", "DIY Smoochie", "ESP32-S3", 921600),
        Board("cyd_2432s028", "CYD 2432S028", "ESP32", 921600),
        Board("cyd_2432s028_2usb", "CYD 2432S028 (2USB)", "ESP32", 921600),
        Board("cyd_2432w328c", "CYD 2432W328C", "ESP32", 921600),
        Board("cyd_2432w328c_2", "CYD 2432W328C v2", "ESP32", 921600),
        Board("cyd_2432w328r", "CYD 2432W328R", "ESP32", 921600),
        Board("cyd_3248s035r", "CYD 3248S035R", "ESP32", 921600),
        Board("cyd_3248s035c", "CYD 3248S035C", "ESP32", 921600),
    )

    val byId: Map<String, Board> = all.associateBy { it.id }
}
