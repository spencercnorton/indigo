#!/usr/bin/env python3
"""Fail-closed wiring audit for Home controller, pose, and live clock fixes."""

from __future__ import annotations

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[3]
GUI = ROOT / "cube/swiss/source/gui"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n").replace("\r", "\n")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"hardware follow-up audit failed: {message}")


def block(source: str, marker: str) -> str:
    start = source.find(marker)
    require(start >= 0, f"missing marker {marker!r}")
    opening = source.find("{", start)
    require(opening >= 0, f"missing opening brace for {marker!r}")
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise SystemExit(f"hardware follow-up audit failed: unterminated {marker!r}")


def ordered(source: str, *tokens: str) -> None:
    cursor = 0
    for token in tokens:
        position = source.find(token, cursor)
        require(position >= 0, f"missing/out-of-order token {token!r}")
        cursor = position + len(token)


def check_consumers(home: str, recent: str, browser: str,
                    carousel: str, fullwidth: str) -> None:
    ordered(home,
            "homeState.surface == UI_HOME_SURFACE_RING",
            "allowedAxes = UI_MENU_INPUT_AXIS_BOTH;",
            "homeState.surface == UI_HOME_SURFACE_RESTART_CONFIRM",
            "allowedAxes = UI_MENU_INPUT_AXIS_BOTH;",
            "allowedAxes = UI_MENU_INPUT_AXIS_VERTICAL;")
    navigation = block(home, "if(!(btns & BUTTON_B))")
    ring = block(navigation, "if(homeState.surface == UI_HOME_SURFACE_RING)")
    for digital, analog, output in (("BUTTON_UP", "UI_MENU_INPUT_UP", "UI_HOME_INPUT_UP"),
                                   ("BUTTON_DOWN", "UI_MENU_INPUT_DOWN", "UI_HOME_INPUT_DOWN")):
        marker = f"if((btns & {digital}) || analog == {analog})"
        require(marker in ring, "Home ring omits digital or analog vertical navigation")
        arm = block(ring, marker)
        require(f"navigation = {output};" in arm,
                "Home ring aliases vertical input to a horizontal turn")
    require(home.count("padsMenuInputPoll(&homeMenuInput,") == 2,
            "Home must have one poll and one digital release drain")
    require("allowedAxes, true);" in home,
            "Home digital release does not inhibit/disarm analog")
    require("UI_MENU_INPUT_REPEAT" not in home,
            "Home hold repeats can oscillate two-row/ring selections")
    require("padsStickX" not in home and "padsStickY" not in home,
            "Home still reads aggregate axes")
    for name, source, axis in (
        ("Recent", recent, "UI_MENU_INPUT_AXIS_VERTICAL"),
        ("browser", browser, "UI_MENU_INPUT_AXIS_VERTICAL"),
        ("fullwidth", fullwidth, "UI_MENU_INPUT_AXIS_VERTICAL"),
    ):
        require(source.count("padsMenuInputPoll(") == 2,
                f"{name} must have one poll and one digital release drain")
        policy = f"{axis} | UI_MENU_INPUT_REPEAT"
        require(source.count(policy) == 2,
                f"{name} has the wrong axis/repeat policy")
        require("padsStickX" not in source and "padsStickY" not in source,
                f"{name} still reads aggregate axes")
    # The carousel's stick follows the Library layout: across the carousel
    # (and the legacy carousel), up and down the column, both ways in the
    # grid, always repeating. Its poll and its drain share that one policy.
    require(carousel.count("padsMenuInputPoll(") == 2,
            "carousel must have one poll and one digital release drain")
    require("u32 menuInputPolicy = gameflowMenuInputPolicy(layout);" in carousel and
            carousel.count("\t\t\t\tmenuInputPolicy, ") == 2,
            "carousel poll and drain do not share the layout's policy")
    require("useGameflow ? gameflowLayout() :\n\t\t\tUI_GAMEFLOW_LAYOUT_HORIZONTAL;" in carousel,
            "the legacy carousel does not keep the horizontal stick")
    for arm in ("case UI_GAMEFLOW_LAYOUT_VERTICAL:\n\t\t\treturn UI_MENU_INPUT_AXIS_VERTICAL | UI_MENU_INPUT_REPEAT;",
                "case UI_GAMEFLOW_LAYOUT_GRID:\n\t\t\treturn UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT;",
                "default:\n\t\t\treturn UI_MENU_INPUT_AXIS_HORIZONTAL | UI_MENU_INPUT_REPEAT;"):
        require(arm in carousel, "carousel has the wrong axis/repeat policy")
    require("padsStickX" not in carousel and "padsStickY" not in carousel,
            "carousel still reads aggregate axes")


def changed(source: str, old: str, new: str) -> str:
    require(old in source, f"mutation anchor missing: {old!r}")
    return source.replace(old, new, 1)


def expect_consumer_mutant_rejected(label: str, sources: tuple[str, ...]) -> None:
    try:
        check_consumers(*sources)
    except SystemExit:
        return
    raise SystemExit(f"hardware follow-up audit failed: consumer mutant escaped: {label}")


INPUT_H = read(ROOT / "cube/swiss/include/input.h")
INPUT_C = read(ROOT / "cube/swiss/source/input.c")
VIDEO = read(ROOT / "cube/swiss/source/video.c")
MENU_H = read(GUI / "ui_menu_input.h")
MENU_C = read(GUI / "ui_menu_input.c")
SCENE_H = read(GUI / "ui_scene.h")
SCENE_C = read(GUI / "ui_scene.c")
CLOCK_C = read(GUI / "ui_clock.c")
FRAME = read(GUI / "FrameBufferMagic.c")
INDIGO_H = read(GUI / "indigo_background.h")
INDIGO = read(GUI / "indigo_background.c")
SWISS = read(ROOT / "cube/swiss/source/swiss.c")
ISOLATION = read(ROOT / "buildtools/check_ui_isolation.sh")

choose = block(INPUT_C, "s8 __chooseMaxMagnitiude(")
scan = block(INPUT_C, "void padsScan(void)")
poll = block(INPUT_C, "uiMenuInputDirection_t padsMenuInputPoll(")
post_retrace = block(VIDEO, "static void ProperScanPADS(")
update = block(MENU_C, "uiMenuInputDirection_t UIMenuInput_Update(")
elapsed = block(SWISS, "static u32 menuInputElapsedMicroseconds(u32 *lastRetrace)\n{")
menu_loop = block(SWISS, "void menu_loop()")
home_input = block(menu_loop, "else if (curMenuLocation==ON_OPTIONS)")
recent = block(SWISS, "void select_recent_entry()")
browser = block(SWISS, "uiDrawObj_t* renderFileBrowser(")
carousel = (block(SWISS, "static u32 gameflowMenuInputPolicy(") + "\n" +
            block(SWISS, "uiDrawObj_t* renderFileCarousel("))
fullwidth = block(SWISS, "uiDrawObj_t* renderFileFullwidth(")

# Controller scanning stays once per retrace; menu polls consume an atomic
# low-bit validity snapshot and raw axes from each physical port independently.
require("void padsScan(void);" in INPUT_H, "padsScan lacks an exact prototype")
ordered(scan, "PAD_ScanPads()", "__atomic_store_n", "__ATOMIC_RELAXED")
require("(1u << UI_MENU_INPUT_CHANNEL_COUNT) - 1u" in scan,
        "PAD_ScanPads low-bit mask is not bounded to four ports")
require("padsScan();" in post_retrace and "PAD_ScanPads" not in post_retrace,
        "post-retrace scan bypasses the validity snapshot wrapper")
ordered(poll, "__atomic_load_n", "__ATOMIC_RELAXED")
require("PAD_GetType" not in poll and "SI_GetType" not in poll,
        "menu hot loop probes controller types")
for channel in range(4):
    require(f"PAD_StickX(PAD_CHAN{channel})" in poll, f"port {channel} X omitted")
    require(f"PAD_StickY(PAD_CHAN{channel})" in poll, f"port {channel} Y omitted")
    require(f"1u << PAD_CHAN{channel}" in poll, f"port {channel} validity omitted")
for channel in range(4):
    require(f"int p{channel}a = abs((int)p{channel});" in choose,
            f"legacy magnitude for port {channel} can overflow s8")
require(">= maxa" not in choose, "equal analog magnitudes no longer retain port order")

# Ownership, neutral arming, inhibition, disconnect, hysteresis and bounded
# repeat are all explicit in the pure controller-agnostic policy.
for token in (
    "UI_MENU_INPUT_ENGAGE 32", "UI_MENU_INPUT_RELEASE 20",
    "UI_MENU_INPUT_INITIAL_REPEAT_US 320000u",
    "UI_MENU_INPUT_REPEAT_US 120000u",
    "UI_MENU_INPUT_MAX_ELAPSED_US 50000u",
):
    require(token in MENU_H, f"missing policy constant {token}")
ordered(update,
        "if(inhibited)", "releaseOwner(state)",
        "if(elapsedMicroseconds > UI_MENU_INPUT_MAX_ELAPSED_US)",
        "if(!samples[state->owner].valid)",
        "A disconnect must not expose a gesture", "releaseOwner(state)")
require("channel != state->owner" in update and
        "state->channelArmed[channel] = false;" in update,
        "secondary held gestures can queue under an owner")
require("(policy & UI_MENU_INPUT_REPEAT) == 0u" in update,
        "one-shot Home policy is not implemented")
require("value == INT_MIN" in MENU_C, "INT_MIN magnitude is not guarded")

# Home is one-shot; list surfaces repeat at measured retrace cadence. Mutation
# checks prove each intended consumer is mechanically bound to that contract.
consumer_sources = (home_input, recent, browser, carousel, fullwidth)
check_consumers(*consumer_sources)
consumer_mutants = (
    ("Home Up aliases horizontal", (
        changed(home_input, "navigation = UI_HOME_INPUT_UP;", "navigation = UI_HOME_INPUT_LEFT;"),
        recent, browser, carousel, fullwidth)),
    ("Home Down aliases horizontal", (
        changed(home_input, "navigation = UI_HOME_INPUT_DOWN;", "navigation = UI_HOME_INPUT_RIGHT;"),
        recent, browser, carousel, fullwidth)),
    ("Home ring loses vertical navigation", (
        changed(home_input, "allowedAxes = UI_MENU_INPUT_AXIS_BOTH;",
                "allowedAxes = UI_MENU_INPUT_AXIS_HORIZONTAL;"),
        recent, browser, carousel, fullwidth)),
    ("Home restart loses both axes", (
        changed(home_input, "else if(homeState.surface == UI_HOME_SURFACE_RESTART_CONFIRM) {\n\t\t\t\tallowedAxes = UI_MENU_INPUT_AXIS_BOTH;",
                "else if(homeState.surface == UI_HOME_SURFACE_RESTART_CONFIRM) {\n\t\t\t\tallowedAxes = UI_MENU_INPUT_AXIS_HORIZONTAL;"),
        recent, browser, carousel, fullwidth)),
    ("Home ring ignores analog up", (
        changed(home_input, "analog == UI_MENU_INPUT_UP", "analog == UI_MENU_INPUT_NONE"),
        recent, browser, carousel, fullwidth)),
    ("Home ring ignores digital down", (
        changed(home_input, "(btns & BUTTON_DOWN)", "false"),
        recent, browser, carousel, fullwidth)),
    ("Home repeats held gestures", (
        changed(home_input, "allowedAxes, (btns & homeButtons)",
                "allowedAxes | UI_MENU_INPUT_REPEAT, (btns & homeButtons)"),
        recent, browser, carousel, fullwidth)),
    ("Recent uses horizontal axis", (home_input,
        changed(recent, "UI_MENU_INPUT_AXIS_VERTICAL | UI_MENU_INPUT_REPEAT",
                "UI_MENU_INPUT_AXIS_HORIZONTAL | UI_MENU_INPUT_REPEAT"),
        browser, carousel, fullwidth)),
    ("browser restores aggregate stick", (home_input, recent,
        changed(browser, "padsMenuInputPoll(&menuInput,",
                "padsStickY(); padsMenuInputPoll(&menuInput,"),
        carousel, fullwidth)),
    ("carousel uses vertical axis", (home_input, recent, browser,
        changed(carousel, "default:\n\t\t\treturn UI_MENU_INPUT_AXIS_HORIZONTAL | UI_MENU_INPUT_REPEAT;",
                "default:\n\t\t\treturn UI_MENU_INPUT_AXIS_VERTICAL | UI_MENU_INPUT_REPEAT;"),
        fullwidth)),
    ("vertical Library keeps the horizontal stick", (home_input, recent, browser,
        changed(carousel, "return UI_MENU_INPUT_AXIS_VERTICAL | UI_MENU_INPUT_REPEAT;",
                "return UI_MENU_INPUT_AXIS_HORIZONTAL | UI_MENU_INPUT_REPEAT;"),
        fullwidth)),
    ("grid loses repeat", (home_input, recent, browser,
        changed(carousel, "return UI_MENU_INPUT_AXIS_BOTH | UI_MENU_INPUT_REPEAT;",
                "return UI_MENU_INPUT_AXIS_BOTH;"),
        fullwidth)),
    ("carousel drain ignores the layout", (home_input, recent, browser,
        changed(carousel, "\t\t\t\tmenuInputPolicy, true);",
                "\t\t\t\tUI_MENU_INPUT_AXIS_HORIZONTAL | UI_MENU_INPUT_REPEAT, true);"),
        fullwidth)),
    ("legacy carousel follows the Library layout", (home_input, recent, browser,
        changed(carousel, "useGameflow ? gameflowLayout() :\n\t\t\tUI_GAMEFLOW_LAYOUT_HORIZONTAL;",
                "gameflowLayout();"),
        fullwidth)),
    ("fullwidth loses repeat", (home_input, recent, browser, carousel,
        changed(fullwidth, "UI_MENU_INPUT_AXIS_VERTICAL | UI_MENU_INPUT_REPEAT",
                "UI_MENU_INPUT_AXIS_VERTICAL"))),
)
for mutant_label, mutant_sources in consumer_mutants:
    expect_consumer_mutant_rejected(mutant_label, mutant_sources)
ordered(elapsed, "elapsed =", "UI_MENU_INPUT_MAX_ELAPSED_US", "lrintf(elapsed)")

# Reducer orientation, face, surface and row are one coherent request. Authored
# context row tilts remain distinct from physical vertical navigation.
require("UIScene_RequestHome(const uiHomeState_t *home)" in SCENE_H,
        "scene request omits Home surface/selection")
request_valid = block(SCENE_C, "static bool homeRequestValid(")
require("surfaceMatchesFace" in request_valid and
        "face == UI_HOME_FACE_SOURCE" in request_valid and
        "face == UI_HOME_FACE_SYSTEM" in request_valid,
        "context pose can bind to an unrelated cube face")
home_pose = block(SCENE_C, "static uiScenePose_t homePose(")
require("UI_HOME_SURFACE_SOURCE" in home_pose and
        "UI_HOME_SURFACE_SYSTEM" in home_pose,
        "vertical context pose is not selection-bound")
require("UI_HOME_SURFACE_RESTART_CONFIRM" not in home_pose,
        "horizontal restart choice invents a vertical route")
for field in ("homeSurface", "homeSelection"):
    require(field in SCENE_H and f"state.frame.{field} =" in SCENE_C,
            f"video frame omits {field}")
require("retargetPose(state.appliedScene, motionMode);" in SCENE_C,
        "selection-only reducer revisions do not retarget cube pose")

# One pre-traversal civil-time sample feeds the title instrument and cube. A
# missing clock hides hands but never suppresses the independent temperature.
instrument = block(FRAME, "static void _UpdateSystemInstrument(void)\n{")
title = block(FRAME, "static void _DrawTitleBar(")
video_loop = block(FRAME, "static void *videoUpdate(")
background = block(FRAME, "static void _DrawBackground(")
clock_compose = block(CLOCK_C, "bool UIClock_Compose(")
motifs = block(INDIGO, "static void drawClockIcon(")
ordered(instrument, "gettimeofday(&now, NULL)", "!systemInstrument.temperatureSampled",
        "SYS_GetCoreTemperature()", "UIClock_Compose")
require("localtime_r" in instrument and "strftime" in instrument,
        "clock text is not refreshed safely once per sampled second")
require("gettimeofday" not in title and "localtime" not in title,
        "title bar samples a second clock")
require("gettimeofday" not in INDIGO and "localtime" not in INDIGO,
        "cube renderer samples wall time directly")
ordered(video_loop, "UIScene_Update", "_UpdateSystemInstrument();",
        "videoFrameSerial++", "videoDrawEvent(videoEvent)")
require("&systemInstrument.clock" in background,
        "background cube does not receive shared clock frame")
require("IndigoBackground_DrawBootOverlay" in video_loop and
        "&systemInstrument.clock" in video_loop,
        "boot cube does not receive shared clock frame")
require("const uiClockFrame_t *clock" in INDIGO_H,
        "Indigo API omits numeric clock frame")
require("GX_Begin(GX_QUADS, GX_VTXFMT0, 220);" in motifs,
        "clock icon GX vertex contract changed unexpectedly")
require(motifs.count("putSemanticFaceHand(") == 3,
        "cube clock does not emit exactly three fixed-count hands")
require("hiddenHand" in motifs and "clockAvailable ?" in motifs,
        "unavailable civil time is not rendered as hidden hands")
for vector in ("hourX", "hourY", "minuteX", "minuteY", "secondX", "secondY"):
    require(vector in clock_compose, f"clock geometry omits {vector}")

# The device-agnostic UI gate admits only the exact retrace wrapper hunk.
for token in ("VIDEO_PATH='cube/swiss/source/video.c'", "EXPECTED_VIDEO_SCAN",
              "unexpected video.c diff", "-\\tPAD_ScanPads();", "+\\tpadsScan();"):
    require(token in ISOLATION, f"isolation gate omits exact video exception: {token}")

# The two pickers left from Swiss (destination folder, alternate DOL) share
# the list policy too. A full stick used to move a row every ~24 ms there.
def check_pickers(dest_dir: str, alt_dol: str) -> None:
    for name, source in (("folder picker", dest_dir), ("DOL picker", alt_dol)):
        require(source.count("padsMenuInputPoll(&menuInput,") == 2,
                f"{name} must have one poll and one digital release drain")
        require(source.count("UI_MENU_INPUT_AXIS_VERTICAL | UI_MENU_INPUT_REPEAT") == 2,
                f"{name} has the wrong axis/repeat policy")
        require("padsStick" not in source and "usleep(" not in source,
                f"{name} still times the stick itself")


dest_dir = block(SWISS, "bool select_dest_dir(file_handle* initial, file_handle* selection)")
alt_dol = block(SWISS, "ExecutableFile* select_alt_dol(")
check_pickers(dest_dir, alt_dol)
picker_mutants = (
    ("folder picker times the stick again", (changed(
        dest_dir, "UIMenuInput_Init(&menuInput);",
        "UIMenuInput_Init(&menuInput); usleep(50000 - abs(padsStickY()*256));"), alt_dol)),
    ("DOL picker loses repeat", (dest_dir, changed(
        alt_dol, "UI_MENU_INPUT_AXIS_VERTICAL | UI_MENU_INPUT_REPEAT,\n",
        "UI_MENU_INPUT_AXIS_VERTICAL,\n"))),
)
for mutant_label, mutant_sources in picker_mutants:
    try:
        check_pickers(*mutant_sources)
    except SystemExit:
        continue
    raise SystemExit(f"hardware follow-up audit failed: picker mutant escaped: {mutant_label}")

# The disc drive is the "Game Disc" wherever the UI names a device. Only the
# patches device (never the drive) still prints its own name, in load messages.
INFO = read(GUI / "info.c")
require('return device == &__device_dvd ? "Game Disc" : device->deviceName;' in FRAME,
        "DeviceDisplayName no longer names the disc drive Game Disc")
for name, source in (("swiss.c", SWISS), ("FrameBufferMagic.c", FRAME), ("info.c", INFO)):
    raw = [line.strip() for line in source.splitlines()
           if re.search(r"(?<!snapshot)->deviceName\b(?!Scale)", line)
           and "DEVICE_PATCHES" not in line and "print_debug" not in line
           and "return device == &__device_dvd" not in line]
    require(not raw, f"{name} shows a raw device name: {raw[:1]}")

print(f"Home hardware follow-up structural audit passed "
      f"({len(consumer_mutants)} consumer and {len(picker_mutants)} picker mutants rejected)")
