#!/bin/sh
# audit_settings_semantics.sh -- proves the Phase 4D presentation refactor
# preserves every legacy setting semantic except the intentional expansion of
# SET_UI_ANIMS from a boolean into Full/Reduced/Off and the bounded input pump.
# Value mutations remain byte-identical outside that one switch arm, except
# that a game's Reset to defaults calls settingsResetGame instead of
# config_defaults, so it keeps the game's Comment and Status (the Settings redesign), the
# new Menu Color arm (SET_UI_COLOR) only steps swissSettings.uiColor round
# its colors, the four new face icon arms (SET_*_ICON) each only step
# their own face's icon round that face's own four, the Library Layout arm
# (SET_LIBRARY_LAYOUT) only steps swissSettings.libraryLayout round its
# three layouts, and the Save Folder arm (SET_SAVE_FOLDER) only sets
# swissSettings.saveFolder to the folder Memory Cards' chooser returns.
#
# The Right/Left/Up/Down/L/R/B/A action block changed on purpose in the Settings redesign:
# phase 1 made B leave (Save & Exit when something changed), A advance choice
# rows, L/R wrap and video rows confirm; phase 2 moved it onto views (Quick,
# Game Defaults, Setup and its sections, one game's settings); phase 3 added
# X and the reset prompt; phase 4 names a new Configuration Device in SRAM
# only once a save has reached it; phase 1b opens a list of values on A for
# long choices. It is pinned by content hash rather than
# by a base commit, so a rebase cannot break the pin but any further drift
# still fails until SHOW_ACTIONS_SHA256 is updated deliberately.
#
# usage: audit_settings_semantics.sh [base-ref]
# With no base ref the recorded base is used: fixtures/settings_toggle.base.c,
# settings_toggle exactly as it stood before the presentation refactor.
set -eu
BASE="${1:-}"
FILE="cube/swiss/source/gui/settings.c"
cd "$(git rev-parse --show-toplevel)"

extract() { # extract <input> <function-name>  -- brace-balanced body
	awk -v fn="$2" '
		index($0, fn "(") && !found { found = 1 }
		found {
			print
			n = gsub(/{/, "{"); m = gsub(/}/, "}")
			depth += n - m
			if (started && depth == 0) exit
			if (n > 0) started = 1
		}
	' "$1"
}

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
if [ -n "$BASE" ]; then
	git show "$BASE:$FILE" > "$TMP/base.c"
else
	cp buildtools/ui/tests/fixtures/settings_toggle.base.c "$TMP/base.c"
	BASE="the recorded base"
fi

normalize_intended_changes() {
	python3 - "$1" "$2" <<'PY'
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_text()
pattern = re.compile(
    r"(?ms)^(\s*case SET_UI_ANIMS:\s*\n).*?^(\s*break;\s*)$"
)
normalized, count = pattern.subn(
    r"\1\t\t\t\t/* explicit motion-mode arm */\n\2", source, count=1
)
if count != 1:
    raise SystemExit("could not normalize SET_UI_ANIMS")
normalized = normalized.replace("\t\t\t\t\tsettingsResetGame(gameConfig);\n",
                                "\t\t\t\t\tconfig_defaults(gameConfig);\n")
# The Menu Color arm is new, so the base has nothing to compare it with.
normalized = re.sub(
    r"(?ms)^\t+case SET_UI_COLOR:\n"
    r"\t+swissSettings\.uiColor \+= direction;\n"
    r"\t+swissSettings\.uiColor = \(swissSettings\.uiColor \+ UI_COLOR_MAX\) % UI_COLOR_MAX;\n"
    r"\t+break;\n",
    "", normalized, count=1)
# So are the four face icon arms, one per Home face.
normalized, icons = re.subn(
    r"(?ms)^\t+case SET_(LIBRARY|SOURCE|SETTINGS|SYSTEM)_ICON:\n"
    r"\t+swissSettings\.(library|source|settings|system)Icon \+= direction;\n"
    r"\t+swissSettings\.\2Icon = \(swissSettings\.\2Icon \+ UI_HOME_ICON_CHOICES\) % UI_HOME_ICON_CHOICES;\n"
    r"\t+break;\n",
    lambda arm: "" if arm.group(1).lower() == arm.group(2) else arm.group(0),
    normalized)
# And the Library Layout arm, round its three layouts.
normalized = re.sub(
    r"(?ms)^\t+case SET_LIBRARY_LAYOUT:\n"
    r"\t+swissSettings\.libraryLayout \+= direction;\n"
    r"\t+swissSettings\.libraryLayout = \(swissSettings\.libraryLayout \+ UI_GAMEFLOW_LAYOUT_COUNT\) % UI_GAMEFLOW_LAYOUT_COUNT;\n"
    r"\t+break;\n",
    "", normalized, count=1)
# And the Save Folder arm: the folder the chooser returns, or no change.
normalized = re.sub(
    r"(?ms)^\t+case SET_SAVE_FOLDER:\n"
    r"\t+\{\n"
    r"\t+char folder\[PATHNAME_MAX\];\n"
    r"\n"
    r"\t+if\(devices\[DEVICE_CONFIG\] != NULL && saves_choose_folder\(folder, sizeof\(folder\)\)\) \{\n"
    r"\t+strlcpy\(swissSettings\.saveFolder, folder, sizeof\(swissSettings\.saveFolder\)\);\n"
    r"\t+\}\n"
    r"\t+\}\n"
    r"\t+break;\n",
    "", normalized, count=1)
normalized = re.sub(r"[ \t]+(?=\n|$)", "", normalized)
Path(sys.argv[2]).write_text(normalized)
PY
}

fail=0
for fn in settings_toggle; do
	extract "$TMP/base.c" "$fn" > "$TMP/base_$fn"
	extract "$FILE" "$fn" > "$TMP/head_$fn"
	if [ ! -s "$TMP/base_$fn" ] || [ ! -s "$TMP/head_$fn" ]; then
		echo "AUDIT ERROR: could not extract $fn" >&2
		fail=1
	elif ! cmp -s "$TMP/base_$fn" "$TMP/head_$fn"; then
		if [ "$fn" = settings_toggle ]; then
			normalize_intended_changes "$TMP/base_$fn" "$TMP/base_${fn}_normalized"
			normalize_intended_changes "$TMP/head_$fn" "$TMP/head_${fn}_normalized"
			if cmp -s "$TMP/base_${fn}_normalized" "$TMP/head_${fn}_normalized"; then
				printf '  %-18s unchanged outside motion, color, icon, layout and save folder arms and game reset\n' "$fn"
			else
				echo "AUDIT FAILED: $fn differs outside SET_UI_ANIMS, SET_UI_COLOR, SET_*_ICON, SET_LIBRARY_LAYOUT, SET_SAVE_FOLDER and the game reset" >&2
				diff -u "$TMP/base_${fn}_normalized" \
					"$TMP/head_${fn}_normalized" | head -40 >&2 || true
				fail=1
			fi
		else
			echo "AUDIT FAILED: $fn differs from $BASE" >&2
			diff -u "$TMP/base_$fn" "$TMP/head_$fn" | head -40 >&2 || true
			fail=1
		fi
	else
		printf '  %-18s unchanged (%s)\n' "$fn" \
			"$(shasum -a 256 < "$TMP/head_$fn" | cut -c1-12)"
	fi
done

normalize_show_actions() {
	python3 - "$1" "$2" <<'PY'
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_text().replace("\r\n", "\n").replace("\r", "\n")
try:
    start = source.index("\t\tif(btns & BUTTON_RIGHT)")
except ValueError as error:
    raise SystemExit(f"missing Settings action block start: {error}")
ends = []
for marker in ("\n\t\twhile ((padsButtonsHeld() & BUTTON_RIGHT)",
               "\n\t\tif(view != inputView || inputMayBlock)"):
    try:
        ends.append(source.index(marker, start))
    except ValueError:
        pass
if not ends:
    raise SystemExit("missing Settings action block end")
region = source[start:min(ends)]
region = re.sub(
    r"\n\t\t\t\tsettingsInhibitThroughDigitalRelease\(&menuInput(?:,\n"
    r"\t\t\t\t\t&menuInputRetrace)?\);",
    "",
    region,
)
Path(sys.argv[2]).write_text(region)
PY
}

normalize_show_actions "$FILE" "$TMP/head_show_actions"
SHOW_ACTIONS_SHA256="836a8708d15f0cb53182815bbb80987c9098962fef053aefd6f0975404afab32"
actual_show_actions=$(shasum -a 256 < "$TMP/head_show_actions" | cut -d' ' -f1)
if [ "$actual_show_actions" = "$SHOW_ACTIONS_SHA256" ]; then
	echo "  show actions       B/A/L/R/value semantics match the pinned hash"
else
	echo "AUDIT FAILED: Settings action block drifted (sha256 $actual_show_actions)" >&2
	echo "  Update SHOW_ACTIONS_SHA256 only for a deliberate, reviewed change." >&2
	fail=1
fi

# Reduced is additive persistence: old Disable=No/Yes files remain Full/Off,
# while the new key is emitted once and accepted in both legacy/global parsers.
CONFIG="cube/swiss/source/config/config.c"
FRAME="cube/swiss/source/gui/FrameBufferMagic.c"
reduce_key_count=$(grep -c '"Reduce UI Animations"' "$CONFIG" || true)
if [ "$reduce_key_count" -ne 2 ] ||
	! grep -q 'Reduce UI Animations=%s' "$CONFIG" ||
	! grep -q 'UIMotion_CycleMode' "$FILE"; then
	echo "AUDIT FAILED: explicit motion persistence/cycle wiring incomplete" >&2
	fail=1
else
	echo "  motion preference  additive key + two parsers + explicit cycle"
fi

# Reduced preserves required scene/focus travel but must suppress every
# time-driven decorative path. The scene springs consume the motion mode
# separately; these booleans and the Detail pulse are presentation-only.
if ! grep -q 'decorativeAnimated = _CurrentMotionMode() == UI_MOTION_FULL' "$FRAME" ||
	! grep -q 'decorativeAnimated && !swissSettings.disableAnimatedBackdrop' "$FRAME" ||
	! grep -q '^[[:space:]]*decorativeAnimated,$' "$FRAME" ||
	! grep -q '_CurrentMotionMode() == UI_MOTION_FULL ?' "$FRAME"; then
	echo "AUDIT FAILED: Reduced motion still permits decorative travel" >&2
	fail=1
else
	echo "  reduced motion     required springs only; decorative travel suppressed"
fi

# The bounded pump accepts exactly the pre-existing Settings digital controls;
# analog directions translate one-for-one into that unchanged semantic path.
if ! python3 - "$FILE" <<'PY'
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_text().replace("\r\n", "\n").replace("\r", "\n")
mask = re.search(
    r"#define\s+SETTINGS_DIGITAL_INPUT_MASK\s+\((.*?)\)\s*#define",
    source,
    re.DOTALL,
)
expected_mask = [
    "BUTTON_RIGHT", "BUTTON_LEFT", "BUTTON_UP", "BUTTON_DOWN",
    "BUTTON_B", "BUTTON_A", "BUTTON_Y", "BUTTON_R", "BUTTON_L", "BUTTON_X",
]
if mask is None:
    raise SystemExit("Settings digital mask is not extractable")
normalized_mask = re.sub(r"\\\s*\n", "", mask.group(1))
normalized_mask = re.sub(r"\s+", "", normalized_mask)
if normalized_mask != "|".join(expected_mask):
    raise SystemExit("Settings digital mask is not the exact legacy control set")

start = source.index("static u32 settingsButtonForMenuDirection(")
opening = source.index("{", start)
depth = 0
for end in range(opening, len(source)):
    if source[end] == "{":
        depth += 1
    elif source[end] == "}":
        depth -= 1
        if depth == 0:
            break
else:
    raise SystemExit("unterminated analog direction map")
direction_map = source[start:end + 1]
actual = re.findall(
    r"case\s+(UI_MENU_INPUT_(?:LEFT|RIGHT|UP|DOWN)):\s*"
    r"return\s+(BUTTON_(?:LEFT|RIGHT|UP|DOWN));",
    direction_map,
)
expected = [
    ("UI_MENU_INPUT_LEFT", "BUTTON_LEFT"),
    ("UI_MENU_INPUT_RIGHT", "BUTTON_RIGHT"),
    ("UI_MENU_INPUT_UP", "BUTTON_UP"),
    ("UI_MENU_INPUT_DOWN", "BUTTON_DOWN"),
]
if actual != expected:
    raise SystemExit("analog directions do not map one-for-one to legacy actions")
if direction_map.count("case UI_MENU_INPUT_NONE:") != 1 or \
        direction_map.count("return 0u;") != 1:
    raise SystemExit("neutral analog mapping is not fail-closed")
PY
then
	echo "AUDIT FAILED: exact Settings controller contract drifted" >&2
	fail=1
else
	echo "  input controls     exact digital mask + four-way analog mapping"
fi
if ! grep -q 'SETTINGS_MENU_INPUT_POLICY (UI_MENU_INPUT_AXIS_BOTH' "$FILE" ||
	! grep -q 'UI_MENU_INPUT_REPEAT' "$FILE"; then
	echo "AUDIT FAILED: Settings analog policy is not both-axis controlled repeat" >&2
	fail=1
else
	echo "  input policy       existing digital path + both-axis controlled repeat"
fi

# The view tables replaced settings_count_pp (Settings redesign, phase 2); their
# coverage is checked by test_settings_views.py.

[ "$fail" -eq 0 ] && echo "settings semantics audit OK"
exit "$fail"
