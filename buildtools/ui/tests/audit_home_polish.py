#!/usr/bin/env python3
"""Structural gates for the hardware-driven Home polish baseline.

This audit deliberately keeps the Phase 4G readability, publication, motion
policy, and command-rail invariants while accepting Phase 4H's semantic Home
reducer and four-face scene API. ``read_text`` uses universal-newline mode, so
the assertions behave identically for Swiss's CRLF translation units.
"""

from pathlib import Path
import re


if not __debug__:
    raise SystemExit(
        "audit_home_polish.py requires Python assertions; rerun without -O/PYTHONOPTIMIZE"
    )


ROOT = Path(__file__).resolve().parents[3]


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n").replace("\r", "\n")


FRAMEBUFFER = read(ROOT / "cube/swiss/source/gui/FrameBufferMagic.c")
INDIGO = read(ROOT / "cube/swiss/source/gui/indigo_background.c")
HOME_LAYOUT = read(ROOT / "cube/swiss/source/gui/ui_home_layout.h")
SETTINGS = read(ROOT / "cube/swiss/source/gui/settings.c")
SWISS = read(ROOT / "cube/swiss/source/swiss.c")


def extract_function(source: str, marker: str) -> str:
    start = source.index(marker)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"unterminated function: {marker}")


titlebar = extract_function(FRAMEBUFFER, "static void _DrawTitleBar(")
home_text = extract_function(FRAMEBUFFER, "static void _DrawHomeText(")
home = extract_function(FRAMEBUFFER, "static void _DrawHomeRoot(")
update = extract_function(FRAMEBUFFER, "void DrawUpdateHome(")
video_loop = extract_function(FRAMEBUFFER, "static void *videoUpdate(")
motion_policy = extract_function(FRAMEBUFFER, "static uiMotionMode_t _CurrentMotionMode(")
background = extract_function(FRAMEBUFFER, "static void _DrawBackground(")
menu_loop = extract_function(SWISS, "void menu_loop()")

# Phase 4G's hardware readability fixes remain mandatory after the Phase 4H
# renderer rename. drawStringMedium itself owns the one native-pixel weight.
assert "TEX_SWISS" not in titlebar, "permanent Swiss logo returned to title bar"
assert "previousSelection" not in home, "Home again draws an outgoing title in the focus lane"
assert home_text.count("drawStringMedium(") == 1, (
	"Home text must use exactly one bounded medium-weight call"
)
assert "drawString(" not in home_text, "Home reintroduced a redundant shadow pass"
assert "_DrawHomeText(" in home, "Home bypasses the readable IPL type treatment"
travel_match = re.search(
    r"#define UI_HOME_LAYOUT_SELECTED_TRAVEL\s+([0-9]+)", HOME_LAYOUT
)
assert travel_match and float(travel_match.group(1)) >= 28.0, (
    "Home focus travel regressed below the hardware-accepted baseline"
)
assert "UIMotion_Amplitude((float)UI_HOME_LAYOUT_SELECTED_TRAVEL," in home

# Retained foreground and video-owned cube state must be committed together.
assert "UIScene_RequestHome(state);" in update, (
    "Home snapshot no longer publishes the matching semantic cube turn"
)
assert update.index("LWP_MutexLock(_videomutex);") < update.index(
    "data->state = *state;"
) < update.index("UIScene_RequestHome(state);") < update.index(
    "LWP_MutexUnlock(_videomutex);"
), "Home foreground and cube target are no longer published in one transaction"
assert video_loop.index("LWP_MutexLock(_videomutex);") < video_loop.index(
    "UIScene_Update(UIAnim_Delta(), _CurrentMotionMode());"
), "scene state can advance before the retained Home snapshot is locked"

# Decorative backdrop preferences remain independent of primary navigation.
assert "disableAnimatedBackdrop" not in motion_policy, (
    "decorative backdrop preference again weakens primary navigation motion"
)
assert "decorativeAnimated = _CurrentMotionMode() == UI_MOTION_FULL;" in background, (
	"Reduced/Off motion can still reach time-driven cube decoration"
)
assert "decorativeAnimated && !swissSettings.disableAnimatedBackdrop" in background
assert "\n\t\tdecorativeAnimated,\n" in background, (
	"cube decoration no longer has an explicit Full-only policy input"
)
assert "bool backdropMotionActive" in INDIGO and "bool cubeMotionActive" in INDIGO
assert "disableAnimatedBackdrop ? UI_SETLAYOUT_MOTION_REDUCED" not in SETTINGS, (
    "Settings focus still couples primary motion to decorative backdrop preference"
)

# Semantic faces may breathe, but the old unbounded whole-cube spin must not
# return. The sway amplitude is intentionally capped well below five degrees.
sway_match = re.search(r"#define CUBE_IDLE_SWAY_RADIANS\s+([0-9.]+)f", INDIGO)
assert sway_match and 0.0 < float(sway_match.group(1)) <= 0.08
assert "#define CUBE_IDLE_SWAY_RATE" in INDIGO
assert "sinf(seconds * CUBE_IDLE_SWAY_RATE)" in INDIGO
assert "CUBE_IDLE_SWAY_RADIANS * idleBlend" in INDIGO
assert "float idleBlend = scene->homeIdleBlend;" in INDIGO
assert "CUBE_IDLE_TURN_RATE" not in INDIGO
assert "fmodf(seconds *" not in INDIGO

# A direction press publishes immediately; physical button release cannot
# delay the selected title/cube target as it did in Alpha V1.
navigation = menu_loop.index("UIHome_Apply(&homeState, navigation")
publish = menu_loop.index("homePublish(true);", navigation)
release_wait = menu_loop.rindex(
	"padsMenuInputPoll(&homeMenuInput,", publish
)
assert navigation < publish < release_wait, (
    "Home focus/cube target is again delayed until button release"
)

# Preserve the already-accepted Library/Detail single-owner command rail.
assert "UICommandRail_Gameflow(frame->detailProgress, &commandRail);" in FRAMEBUFFER
assert "commandRail.owner == UI_COMMAND_RAIL_LIBRARY" in FRAMEBUFFER
assert "commandRail->owner == UI_COMMAND_RAIL_DETAIL" in FRAMEBUFFER

print("home polish structural audit passed")
