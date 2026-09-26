#!/usr/bin/env python3
"""Fail-closed structural guardrails for the retained Detail presentation."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]


def fail(message: str) -> None:
    raise SystemExit(f"detail presentation audit failed: {message}")


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


framebuffer = (
    ROOT / "cube/swiss/source/gui/FrameBufferMagic.c"
).read_text()
detail_source = (
    ROOT / "cube/swiss/source/gui/ui_gameflow_detail.c"
).read_text()
detail_header = (
    ROOT / "cube/swiss/source/gui/ui_gameflow_detail.h"
).read_text()

renderer = extract_function(
    framebuffer, "static void _GameflowDrawDetailDashboard("
)
metadata = extract_function(
    framebuffer, "static void _GameflowDrawMetadata("
)
for forbidden in (
    "snprintf(",
    "sprintf(",
    "strcpy(",
    "strcat(",
    "_GameflowAppendAction(",
    "GetTextScaleToFitInWidth",
    "UIAssets_DominantColor(",
):
    require(forbidden not in renderer, f"render path contains {forbidden}")
    require(forbidden not in metadata,
            f"Library metadata render path contains {forbidden}")

for retained_field in (
    "detail->statusText",
    "detail->lastPlayedText",
    "detail->saveStatusText",
    "detail->cheatSummary",
    "detail->cheatPreview",
    "detail->settingsSummary",
    "detail->settingsPreview",
    "detail->launchLabel",
    "detail->primaryActions",
    "detail->advancedLineOne",
    "detail->advancedLineTwo",
):
    require(retained_field in renderer, f"renderer ignores {retained_field}")

require("_GameflowDrawDetailPlanes(" in renderer, "selected CTA planes missing")
require('"STARTING GAME..."' in renderer, "launch feedback missing")
# One line, drawn with button icons for the button names.
require("_DrawHintText(320, 433, detail->primaryActions" in renderer,
        "Detail command rail is not single-owner/single-line")

prepare = extract_function(
    framebuffer, "static void _GameflowPrepareDetailPresentation("
)
require("UIAssets_DominantColor(" in prepare,
        "cover accent is not cached at publication")
require(prepare.count("_GameflowPrepareDetailText(") >= 10,
        "dynamic typography is not fully cached")

card_prepare = extract_function(
    framebuffer, "static void _GameflowPrepareCardPresentation("
)
require(card_prepare.count("_GameflowPrepareDetailText(") == 3,
        "Library metadata typography is not cached")
copy_snapshot = extract_function(
    framebuffer, "static void _GameflowCopySnapshot("
)
require("_GameflowPrepareCardPresentation(data, i);" in copy_snapshot,
        "Library metadata is not prepared during snapshot publication")

publish = extract_function(framebuffer, "bool DrawUpdateGameflowDetail(")
copy_at = publish.find("memcpy(&data->detail, snapshot")
prepare_at = publish.find("_GameflowPrepareDetailPresentation(data)")
publish_at = publish.find("updated = true")
require(-1 not in (copy_at, prepare_at, publish_at),
        "publication transaction is incomplete")
require(copy_at < prepare_at < publish_at,
        "presentation must be prepared inside the publication transaction")

build = extract_function(detail_source, "bool UIGameflowDetail_Build(")
clear_capability = build.find("UI_GAMEFLOW_DETAIL_CAN_CHEATS")
format_at = build.find("buildPresentation(snapshot)")
require(clear_capability >= 0 and format_at > clear_capability,
        "presentation was formatted before final capabilities were known")

for field in (
    "statusText",
    "lastPlayedText",
    "saveStatusText",
    "cheatSummary",
    "cheatPreview",
    "settingsSummary",
    "settingsPreview",
    "launchLabel",
    "primaryActions",
    "advancedLineOne",
    "advancedLineTwo",
):
    require(f"char {field}[" in detail_header,
            f"pointer-free snapshot field {field} missing")

# A game's own settings get their own inset, formatted with the rest of the
# presentation once capabilities are final; the X action keeps a plain name,
# so the count is said once.
settings_line = extract_function(detail_source,
                                 "static void buildSettingsPresentation(")
require(build.find("buildSettingsPresentation(snapshot, source->firstCustomSetting)") >
        format_at, "the settings line is formatted before the presentation")
require("UI_GAMEFLOW_DETAIL_CAN_SETTINGS" in settings_line and
        '"Game Defaults"' in settings_line and '"%lu custom"' in settings_line,
        "the settings line does not say whether the game has its own settings")
require('"SETTINGS"' in renderer and
        renderer.index('"SETTINGS"') < renderer.index('"CHEATS"'),
        "the dashboard does not show SETTINGS beside CHEATS")
require("hasSettings" in extract_function(framebuffer,
        "static void _GameflowDrawDetailPlanes("),
        "the settings inset has no panel")

presentation = extract_function(detail_source, "static void buildPresentation(")
require("CUSTOM)" not in presentation,
        "the X action repeats the custom count the settings inset shows")
for primary in ('"A  LAUNCH"', '"B  LIBRARY"', '"X  SETTINGS"',
                '"Y  CHEATS"'):
    require(primary in presentation, f"primary action {primary} missing")
for advanced in ('"Z  AUTOLOAD"', '"R  VERIFY"', '"L+A  CLEAN BOOT"'):
    require(advanced in presentation, f"advanced shortcut {advanced} missing")

print("detail presentation audit OK")
