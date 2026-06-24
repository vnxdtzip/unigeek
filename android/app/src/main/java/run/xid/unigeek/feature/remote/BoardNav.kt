package run.xid.unigeek.feature.remote

/** One row of a board's native navigation scheme (physical input → on-device action). */
data class NavGuideRow(val input: String, val action: String)

/**
 * Per-board navigation model — ported from `website/content/boards.js` `nav`.
 * `touch` / `keyboard` are also reported live in the mirror HELLO caps (which win at
 * runtime); `fourWay` (whether Left/Right are part of nav) is board-only, so the
 * Remote controls size themselves from here.
 */
data class BoardNav(
    val scheme: String,
    val touch: Boolean,
    val keyboard: Boolean,
    val fourWay: Boolean,
    val guide: List<NavGuideRow>,
    val note: String? = null,
)

object BoardNavs {
    private fun r(i: String, a: String) = NavGuideRow(i, a)

    private val cydTouch = BoardNav(
        "Touch", touch = true, keyboard = false, fourWay = false,
        guide = listOf(r("Tap left edge", "Back"), r("Tap right-top", "Up"), r("Tap right-middle", "Select"), r("Tap right-bottom", "Down")),
        note = "Touch-only — tap the mirror to navigate.",
    )

    private val byId: Map<String, BoardNav> = mapOf(
        "m5stickcplus_11" to BoardNav("Buttons", false, false, false, listOf(
            r("AXP power button", "Up"), r("BTN_A (front)", "Select"), r("BTN_B (short)", "Down"), r("BTN_B (hold >600ms)", "Back")),
            "No dedicated Back — hold BTN_B."),
        "m5stickcplus_2" to BoardNav("Buttons", false, false, false, listOf(
            r("BTN_UP (top)", "Up"), r("BTN_A (front)", "Select"), r("BTN_B (short)", "Down"), r("BTN_B (hold >600ms)", "Back")),
            "No dedicated Back — hold BTN_B."),
        "m5sticks3" to BoardNav("Buttons", false, false, false, listOf(
            r("BTN_A (front)", "Select"), r("BTN_B (single)", "Down"), r("BTN_B (double)", "Up"), r("BTN_B (hold >600ms)", "Back"))),
        "m5_cardputer" to BoardNav("Keyboard", false, true, true, listOf(
            r("Key ;", "Up"), r("Key .", "Down"), r("Key ,", "Left"), r("Key /", "Right"), r("Enter", "Select"), r("Backspace", "Back"))),
        "m5_cardputer_adv" to BoardNav("Keyboard", false, true, true, listOf(
            r("Key ;", "Up"), r("Key .", "Down"), r("Key ,", "Left"), r("Key /", "Right"), r("Enter", "Select"), r("Backspace", "Back"))),
        "m5_cores3" to BoardNav("Touch", true, false, false, listOf(
            r("Tap left edge", "Back"), r("Tap right-top", "Up"), r("Tap right-middle", "Select"), r("Tap right-bottom", "Down")),
            "Touch-only — tap the mirror to navigate."),
        "t_display" to BoardNav("Buttons", false, false, false, listOf(
            r("BTN_UP (top)", "Up"), r("BTN_B (single)", "Down"), r("BTN_B (double)", "Select"), r("BTN_B (hold >600ms)", "Back"))),
        "t_display_s3" to BoardNav("Buttons", false, false, false, listOf(
            r("BTN_UP (top)", "Up"), r("BTN_B (single)", "Down"), r("BTN_B (double)", "Select"), r("BTN_B (hold >600ms)", "Back"))),
        "t_display_s3_touch" to BoardNav("Buttons + Touch", true, false, false, listOf(
            r("BTN_UP", "Up"), r("BTN_B (single)", "Down"), r("BTN_B (double)", "Select"), r("BTN_B (hold)", "Back"),
            r("Tap left edge", "Back"), r("Tap right-top", "Up"), r("Tap right-middle", "Select"), r("Tap right-bottom", "Down")),
            "Hybrid — tap the mirror or use the buttons."),
        "t_lora_pager" to BoardNav("Encoder + Keyboard", false, true, false, listOf(
            r("Encoder CCW (left)", "Up"), r("Encoder CW (right)", "Down"), r("Encoder push", "Select"), r("Enter", "Select"), r("Backspace", "Back"))),
        "t_embed_cc1101" to BoardNav("Encoder", false, false, false, listOf(
            r("Encoder CW (right)", "Up"), r("Encoder CCW (left)", "Down"), r("Encoder push", "Select"), r("Back button", "Back"))),
        "diy_smoochie" to BoardNav("Buttons", false, false, true, listOf(
            r("BTN_UP", "Up"), r("BTN_DOWN", "Down"), r("BTN_LEFT (short)", "Left"), r("BTN_RIGHT", "Right"), r("BTN_SEL (center)", "Select"), r("BTN_LEFT (hold)", "Back")),
            "No dedicated Back — hold BTN_LEFT."),
    )

    /** All CYD variants share the touch-zone scheme. */
    fun forId(id: String?): BoardNav? = id?.let { byId[it] ?: if (it.startsWith("cyd_")) cydTouch else null }
}
