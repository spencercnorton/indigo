#!/usr/bin/env python3
"""Fail-closed guardrails for shared retained presentation states."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]


def fail(message: str) -> None:
    raise SystemExit(f"shared presentation audit failed: {message}")


def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def extract_function(source: str, marker: str) -> str:
    try:
        start = source.index(marker)
        opening = source.index("{", start)
    except ValueError as error:
        fail(f"missing function marker {marker!r}: {error}")
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    fail(f"unterminated function {marker!r}")


framebuffer = (ROOT / "cube/swiss/source/gui/FrameBufferMagic.c").read_text()
swiss = (ROOT / "cube/swiss/source/swiss.c").read_text()
model = (ROOT / "cube/swiss/source/gui/ui_presentation.c").read_text()
header = (ROOT / "cube/swiss/source/gui/ui_presentation.h").read_text()

for kind in (
    "UI_PRESENTATION_EMPTY",
    "UI_PRESENTATION_LOADING",
    "UI_PRESENTATION_RECOVERABLE_ERROR",
    "UI_PRESENTATION_INFORMATION",
):
    require(kind in header and kind in model, f"state kind {kind} is incomplete")

snapshot_start = header.index("typedef struct {")
snapshot_end = header.index("} uiPresentationSnapshot_t;", snapshot_start)
snapshot = header[snapshot_start:snapshot_end]
for field in ("title", "message", "detail", "action"):
    require(f"char {field}[" in snapshot, f"retained {field} copy is missing")
require("char *" not in snapshot and "const char *" not in snapshot,
        "retained snapshot owns a pointer")

renderer = extract_function(framebuffer, "static void _DrawPresentation(")
for forbidden in (
    "snprintf(",
    "sprintf(",
    "strcpy(",
    "strcat(",
    "malloc(",
    "calloc(",
    "free(",
    "GetTextSizeInPixels(",
    "GetTextScaleToFitInWidth",
    "UIPresentation_Valid(",
):
    require(forbidden not in renderer, f"render path contains {forbidden}")
for retained_field in (
    "data->snapshot.title",
    "data->snapshot.message",
    "data->snapshot.detail",
    "data->snapshot.action",
):
    require(retained_field in renderer, f"renderer ignores {retained_field}")
require("UIAnim_Seconds()" in renderer, "loading cells are not time based")
require(renderer.count("motionMode != UI_MOTION_OFF") >= 2,
        "loading cells do not settle under Motion Off")
require("motionMode == UI_MOTION_REDUCED" in renderer,
        "loading cells do not honor Reduced motion")

prepare = extract_function(framebuffer, "static bool _PreparePresentation(")
require(prepare.count("UIHomeText_CopyFitted(") == 4,
        "presentation typography is not prepared exactly once per field")

resolver = extract_function(swiss, "static bool gameflowResolveAndLoadFolder(")
require("UI_PRESENTATION_LOADING" in resolver,
        "folder resolution has no retained loading state")
require("UI_PRESENTATION_RECOVERABLE_ERROR" in resolver,
        "folder resolution has no recoverable error state")
require("DrawUpdatePresentation(" in resolver,
        "loading-to-error update replaces rather than retains the state")
require("gameflowWaitForPresentationDismiss(" in resolver,
        "resolver error is not explicitly dismissible")
require("sleep(" not in resolver, "resolver still uses a timed blocking sleep")
require("gameflowReleasePrivateChildren(&context);" in resolver,
        "resolver no longer releases private children")

dismiss = extract_function(swiss, "static void gameflowWaitForPresentationDismiss(")
require(dismiss.count("gameflowPresentationInput() !=") == 2,
        "dismiss path must drain both entry and exit samples")
require("UIPresentation_AcceptsInput(" in dismiss,
        "dismiss path bypasses the pure input policy")

# Empty routing deliberately remains unchanged until the scan/deinit lifecycle
# has a separately reviewed contract. The primitive exists; this slice must not
# make an unreachable empty carousel appear safe by deleting that teardown.
require("if(getCurrentDirEntryCount()<=0)" in swiss,
        "zero-entry lifecycle changed in the shared-state slice")
require("devices[DEVICE_CUR]->deinit(devices[DEVICE_CUR]->initial);" in swiss,
        "zero-entry source teardown changed in the shared-state slice")

print("shared presentation audit OK")
