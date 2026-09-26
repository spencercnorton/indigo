#!/usr/bin/env python3
"""Mechanical guardrails for Game Detail controller and read-only seams."""

from pathlib import Path
import re


if not __debug__:
    raise SystemExit(
        "audit_game_detail_safety.py requires Python assertions; rerun without -O/PYTHONOPTIMIZE"
    )


ROOT = Path(__file__).resolve().parents[3]


def extract_function(source: str, marker: str) -> str:
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"unterminated function: {marker}")


cheats_source = (ROOT / "cube/swiss/source/cheats/cheats.c").read_text()
swiss_source = (ROOT / "cube/swiss/source/swiss.c").read_text()
framebuffer_source = (
    ROOT / "cube/swiss/source/gui/FrameBufferMagic.c"
).read_text()

probe = extract_function(cheats_source, "static bool probeCheatsFile")
device_probe = extract_function(
    cheats_source, "static bool probeCheatsOnDevice"
)
internal = extract_function(cheats_source, "static int findCheatsInternal")
legacy = extract_function(cheats_source, "int findCheats(bool silent)")
read_only = extract_function(cheats_source, "int findCheatsReadOnly(void)")
selection_internal = extract_function(
    cheats_source, "static bool loadCheatsSelectionInternal"
)
selection_legacy = extract_function(
    cheats_source, "bool loadCheatsSelection(void)"
)
selection_read_only = extract_function(
    cheats_source, "bool loadCheatsSelectionReadOnly(void)"
)
selection_file = extract_function(
    cheats_source, "static CheatSelectionLoadResult loadCheatSelectionFile"
)

assert internal.count("ensure_path(") == 2
cursor = 0
while True:
    cursor = internal.find("ensure_path(", cursor)
    if cursor < 0:
        break
    assert "if(allowPathMutation)" in internal[max(0, cursor - 180) : cursor]
    cursor += 1

fallback_gate = internal.index("searchCount = allowFallbackInit ? 4u : 1u")
first_fallback_init = internal.index("->init(")
fallback_miss = internal.index("if(!found)", first_fallback_init)
assert fallback_gate < first_fallback_init < fallback_miss
probe_read = probe.index("->readFile(cheatsFile, testBuffer, 8)")
probe_close = probe.index("->closeFile(cheatsFile)", probe_read)
probe_zero = probe.index("cheatsFile->size = 0", probe_close)
probe_failure = probe.index("return false;", probe_zero)
assert probe_read < probe_close < probe_zero < probe_failure
assert device_probe.count("probeCheatsFile(") == 1
assert internal.count("probeCheatsOnDevice(") == 1
assert "readFile(cheatsFile, &testBuffer, 8)" not in internal
short_read_null = internal.index("devices[DEVICE_CHEATS] = NULL", fallback_miss)
assert fallback_miss < short_read_null
early_failure = internal.index(
    "if(devices[DEVICE_CHEATS] == NULL || cheatsFile->size == 0)"
)
assert "if(cheatsFileOpen && devices[DEVICE_CHEATS] != NULL" in internal
early_close = internal.index("->closeFile(cheatsFile)", early_failure)
early_free = internal.index("free(cheatsFile);", early_close)
assert early_failure < early_close < early_free
parse_start = internal.index("bool readComplete = false")
parse_read = internal.index("bytesRead == (s32)cheatsFile->size", parse_start)
parse_guard = internal.index(
    "if(!readComplete)", parse_read
)
parse_zero = internal.index("return 0;", parse_guard)
assert parse_start < parse_read < parse_guard < parse_zero
assert "findCheatsInternal(silent, true, true)" in legacy
assert "findCheatsInternal(true, false, false)" in read_only
assert "ensure_path(" not in read_only and "->init(" not in read_only
assert "if(cheatsFile == NULL)" in internal
delete_guard = selection_file.index("else if(deleteMismatch)")
delete_call = selection_file.index("->deleteFile(", delete_guard)
assert delete_guard < delete_call
assert "loadCheatsSelectionInternal(true)" in selection_legacy
assert "loadCheatsSelectionInternal(false)" in selection_read_only
assert "deleteFile" not in selection_read_only
assert "selectionFile == NULL || enabledFlags == NULL" in selection_file
assert "selectionFile->size == expectedSize" in selection_file
assert "enabledFlags[i] > 1u" in selection_file

launch_marker = "static void load_game_with_context"
launch = extract_function(
    swiss_source[swiss_source.rindex(launch_marker) :], launch_marker
)
assert "bool cheatsFound = findCheats(true) > 0;" in launch
assert "if(cheatsFound)" in launch
assert "loadCheatsSelection();" in launch

detail_marker = "static int gameflow_info_game"
detail = extract_function(
    swiss_source[swiss_source.rindex(detail_marker) :], detail_marker
)
assert detail.count("findCheatsReadOnly()") == 1
assert "findCheats(" not in detail
assert detail.count("loadCheatsSelectionReadOnly()") == 1
assert "loadCheatsSelection();" not in detail

def check_detail_input(controller: str, mapping: str) -> None:
    # Host policy tests exercise the edges. Bind the actual controller to that
    # policy, including the entry/modal quarantine and physical face buttons.
    assert "const u32 detailButtons = PAD_BUTTON_X | BUTTON_B | BUTTON_A |" in controller
    assert "PAD_BUTTON_Y | BUTTON_Z | BUTTON_R;" in controller
    assert "buttons = UIMenuAction_Update(&detailInput, padsButtonsHeld()," in controller
    assert "detailButtons, BUTTON_L, BUTTON_B);" in controller
    assert controller.count("UIMenuAction_Init(&detailInput, padsButtonsHeld());") == 2
    assert "gameflowWaitDetailButtonsReleased" not in controller
    assert "while(!(padsButtonsHeld() & detailButtons))" not in controller
    assert "if(buttons & PAD_BUTTON_X) input |= UI_GAMEFLOW_DETAIL_INPUT_X;" in mapping
    assert "if(buttons & PAD_BUTTON_Y) input |= UI_GAMEFLOW_DETAIL_INPUT_Y;" in mapping
    assert "if(buttons & BUTTON_X)" not in mapping
    assert "if(buttons & BUTTON_Y)" not in mapping
    assert "if(buttons & BUTTON_L) input |= UI_GAMEFLOW_DETAIL_INPUT_L;" in mapping
    library_exit = extract_function(
        controller, "if(action == UI_GAMEFLOW_DETAIL_ACTION_LIBRARY)"
    )
    assert "return 0;" in library_exit and "while(" not in library_exit


detail_mapping = extract_function(swiss_source, "static u32 gameflowDetailInput")
check_detail_input(detail, detail_mapping)
detail_input_mutants = (
    (detail.replace("UIMenuAction_Update", "oldHeldAction", 1), detail_mapping),
    (detail.replace("detailButtons, BUTTON_L, BUTTON_B);",
                    "detailButtons, BUTTON_L, 0u);", 1), detail_mapping),
    (detail.replace("UIMenuAction_Init(&detailInput, padsButtonsHeld());", "", 1), detail_mapping),
    (detail.replace("UIMenuAction_Init(&detailInput, padsButtonsHeld());", "", 2), detail_mapping),
    (detail.replace("PAD_BUTTON_X", "BUTTON_X", 1), detail_mapping),
    (detail, detail_mapping.replace("PAD_BUTTON_Y", "BUTTON_Y", 1)),
    (detail, detail_mapping.replace("if(buttons & BUTTON_L)", "if(false)", 1)),
)
for mutant_controller, mutant_mapping in detail_input_mutants:
    try:
        check_detail_input(mutant_controller, mutant_mapping)
    except AssertionError:
        pass
    else:
        raise AssertionError("Detail input regression escaped wiring audit")

carousel = extract_function(swiss_source, "uiDrawObj_t* renderFileCarousel")
declaration = carousel.index("u32 browserButtons;")
sample = carousel.index("browserButtons = padsButtonsHeld();", declaration)
activation = carousel.index("bool retainedActivation", sample)
nav_guard = carousel.index("if(!retainedActivation)", activation)
first_nav = carousel.index("browserButtons & BUTTON_LEFT", nav_guard)
activate_branch = carousel.index("if((browserButtons & BUTTON_A) || openSettings)",
                                 first_nav)
assert declaration < sample < activation < nav_guard < first_nav < activate_branch
# Y, like A, owns its frame: no navigation runs while either is down.
assert "(browserButtons & (BUTTON_A | PAD_BUTTON_Y))" in carousel[activation:nav_guard]

cheats_renderer = extract_function(framebuffer_source, "static void _DrawCheats")
scroll_block = extract_function(
    cheats_renderer, "if(s->count > UI_CHEATS_VISIBLE_ROWS)"
)
cheats_header = (ROOT / "cube/swiss/source/gui/ui_cheats.h").read_text()
visible_rows = re.search(r"#define UI_CHEATS_VISIBLE_ROWS\s+(\d+)\b", cheats_header)
assert visible_rows and int(visible_rows.group(1)) > 0
# Both denominators must stay under the strict count > visible-rows guard:
# empty, single-entry, and exactly-one-page catalogs must not divide by zero.
assert "/ s->count" in scroll_block
assert re.search(r"/\s*\(s->count - UI_CHEATS_VISIBLE_ROWS\)", scroll_block)
assert not re.search(r"/\s*\(?\s*s->count", cheats_renderer.replace(scroll_block, "", 1))
assert "if(s->rowCount == 0)" in cheats_renderer

print("game detail safety audit OK")
