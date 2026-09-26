#!/usr/bin/env python3
"""Mechanical contract for strict /games retained-Library dispatch."""

from pathlib import Path


if not __debug__:
    raise SystemExit(
        "audit_gameflow_dispatch.py requires Python assertions; rerun without -O/PYTHONOPTIMIZE"
    )


ROOT = Path(__file__).resolve().parents[3]
SWISS = (ROOT / "cube/swiss/source/swiss.c").read_text()
MAIN = (ROOT / "cube/swiss/source/main.c").read_text()
FILES = (ROOT / "cube/swiss/source/files.c").read_text()
UTIL = (ROOT / "cube/swiss/source/util.c").read_text()
CONFIG = (ROOT / "cube/swiss/source/config/config.c").read_text()
SETTINGS = (ROOT / "cube/swiss/source/gui/settings.c").read_text()
PROFILE_PATH = Path(__file__).parent / "fixtures/phase4f_legacy_global.ini"
HOME_PROFILE_PATH = Path(__file__).parent / "fixtures/phase4f_home_global.ini"


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


menu = extract_function(SWISS, "void menu_loop()")
carousel = extract_function(SWISS, "uiDrawObj_t* renderFileCarousel(")
requested = menu.index("int fileBrowserType = swissSettings.fileBrowserType")
games_preference = menu.index(
    "fileBrowserType = swissSettings.gameBrowserType", requested
)
policy_call = menu.index(
    "fileBrowserType = UIGameflowLibrary_SelectBrowser(", games_preference
)
strict_probe = menu.index("gameflowLibraryMode(", policy_call)
requested_browser = menu.index("fileBrowserType, BROWSER_CAROUSEL", strict_probe)
dispatch = menu.index("switch(fileBrowserType)", requested_browser)
carousel_case = menu.index("case BROWSER_CAROUSEL:", dispatch)
retained_renderer = menu.index("renderFileCarousel(", carousel_case)
fullwidth = menu.index("case BROWSER_FULLWIDTH:", retained_renderer)

assert (
    requested
    < games_preference
    < policy_call
    < strict_probe
    < requested_browser
    < dispatch
    < carousel_case
    < retained_renderer
    < fullwidth
)
assert menu.count("UIGameflowLibrary_SelectBrowser(") == 1

# Production-upgrade contract: the preserved profile omits GameBrowserType,
# inherits Fullwidth, and keeps Swiss's default /games flattening. That scans
# strict folders as image files, so both modes must reach retained Detail.
profile = {}
for raw_line in PROFILE_PATH.read_text().splitlines():
    name, value = raw_line.split("=", 1)
    profile[name] = value
assert profile == {
    "FileBrowserType": "Fullwidth",
    "FlattenDir": "*/games",
    "Autoload": "gcldr:/games/Pokemon Colosseum [GC6E01].iso",
    "RecentListLevel": "Lazy",
}
assert "GameBrowserType" not in profile
assert 'swissSettings.gameBrowserType = BROWSER_FULLWIDTH;' in MAIN
assert 'strcpy(swissSettings.flattenDir, "*/games");' in MAIN
assert "fileBrowserTypeStr[] = {\"Standard\", \"Fullwidth\", \"Carousel\"}" in SETTINGS
assert 'else if(!strcmp("GameBrowserType", name))' in CONFIG
assert "fnmatch(swissSettings.flattenDir, curDir.name" in FILES

home_profile = {}
for raw_line in HOME_PROFILE_PATH.read_text().splitlines():
    name, value = raw_line.split("=", 1)
    home_profile[name] = value
assert home_profile == {
    "FileBrowserType": "Fullwidth",
    "FlattenDir": "*/games",
    "Autoload": "",
    "RecentListLevel": "Lazy",
}

home_route = extract_function(SWISS, "static bool gameflowEnterLibraryFromHome(")
assert "UIGameflowLibrary_IsGamesRootEntry(" in home_route
assert "memcpy(&curDir, directory[i], sizeof(file_handle));" in home_route
assert "needsRefresh = 1;" in home_route
retained_root = home_route.index("UIGameflowLibrary_Locate(gamesRoot, curDir.name)")
root_return = home_route.index("return true;", retained_root)
root_scan = home_route.index("directory = getSortedDirEntries();", root_return)
assert retained_root < root_return < root_scan
assert "curSelection" not in home_route[retained_root:root_scan]
assert "needsRefresh" not in home_route[retained_root:root_scan]
home_dispatch = extract_function(SWISS, "static void homeDispatchEffect(")
library_effect = home_dispatch.index("case UI_HOME_EFFECT_OPEN_LIBRARY:")
library_call = home_dispatch.index("gameflowEnterLibraryFromHome()", library_effect)
next_effect = home_dispatch.index("case UI_HOME_EFFECT_CHANGE_SOURCE:", library_call)
assert library_effect < library_call < next_effect
assert "homeLibraryEntryPending = true;" in home_dispatch
pending_scan = menu.index("if(homeLibraryEntryPending)")
pending_clear = menu.index("homeLibraryEntryPending = false;", pending_scan)
pending_promote = menu.index("if(gameflowEnterLibraryFromHome())", pending_clear)
pending_continue = menu.index("continue;", pending_promote)
assert pending_scan < pending_clear < pending_promote < pending_continue
assert menu.count("gameflowEnterLibraryFromHome(") == 1, (
    "Library promotion bypasses the reducer-owned pending scan"
)
assert "int postScanLocation = curMenuLocation;" in menu
assert menu.count("curMenuLocation = postScanLocation;") == 2
browser_owned_loop = menu[menu.index("uiDrawObj_t *filePanel = NULL;"):]
assert browser_owned_loop.count("homePublishBrowserTransition(&filePanel);") == 2
assert "homePublish(curMenuLocation == ON_OPTIONS);" not in browser_owned_loop
home_transition = extract_function(SWISS, "static void homePublishBrowserTransition(")
assert home_transition.index("DrawDispose(*filePanel);") < home_transition.index(
    "*filePanel = NULL;"
) < home_transition.index("homePublish(visible);")
assert "DrawUpdateMenuButtons" not in menu
startup_policy = menu.index("UIGameflowLibrary_ShouldStartHome(")
startup_root = menu.index(
    "memcpy(&curDir, devices[DEVICE_CUR]->initial,", startup_policy
)
startup_refresh = menu.index("needsRefresh = 1;", startup_root)
startup_home = menu.index("curMenuLocation = ON_OPTIONS;", startup_refresh)
startup_release = menu.index(
    "while(padsButtonsHeld() & BUTTON_B)", startup_home
)
home_wait = menu.index("UIScene_Request(UI_SCENE_HOME)", startup_release)
assert (
    startup_policy
    < startup_root
    < startup_refresh
    < startup_home
    < startup_release
    < home_wait
)

library_mode = extract_function(
    SWISS, "static uiGameflowLibraryMode_t gameflowLibraryMode("
)
assert "UIGameflowLibrary_Locate(gamesRoot, curDir.name)" in library_mode
assert "UIGameflowLibrary_ClassifierInit(&classifier, location);" in library_mode
assert "UIGameflowLibrary_ClassifierAdd(&classifier, type, name)" in library_mode
assert "UIGameflowLibrary_ClassifierFinish(&classifier)" in library_mode

autoload_lookup = extract_function(UTIL, "int find_existing_entry(")
assert "return RECENT_ERR_ENT_MISSING;" in autoload_lookup
assert "curMenuLocation = ON_FILLIST;" in autoload_lookup
assert "needsRefresh = 0;" in autoload_lookup

detail_policy = carousel.index("UIGameflowLibrary_UsesRetainedDetail(")
folder_detail = carousel.index("gameflowResolveAndLoadFolder(", detail_policy)
image_detail = carousel.index("gameflowLoadImageWithContext(", folder_detail)
legacy_fallback = carousel.index("load_file();", image_detail)
assert detail_policy < folder_detail < image_detail < legacy_fallback

image_loader = extract_function(SWISS, "static bool gameflowLoadImageWithContext(")
assert "gameflowReadResolverHeader(" in image_loader
# A Library launch reopens the file its banner was read through and forgets a
# stale sector map, before the header read, and does the same for disc 2.
fresh = extract_function(SWISS, "static void gameflowFreshHandle(")
assert "file->device->closeFile(file);" in fresh
assert ("if(file->status == STATUS_HAS_MAPPING) {\n"
        "\t\tfile->status = STATUS_NOT_MAPPED;") in fresh
assert image_loader.index("gameflowFreshHandle(image);") < \
    image_loader.index("gameflowReadResolverHeader(image, &headerEntry)")
assert image_loader.index("gameflowFindOppositeImage(image, &headerEntry,") < \
    image_loader.index("gameflowFreshHandle(context.oppositeDisc);") < \
    image_loader.index("gameflowPopulateResolvedMeta(context.oppositeDisc,")
assert "gameflowFindOppositeImage(image, &headerEntry," in image_loader
assert "meta_find_disc2(" not in image_loader
assert "gameflowProtectMetaFile(image);" in image_loader
opposite_finder = extract_function(SWISS, "static file_handle *gameflowFindOppositeImage(")
assert "image->device->quirks & QUIRK_GCLOADER_NO_DISC_2" in opposite_finder
context_load = image_loader.index("load_file_with_context(&context);")
primary_close = image_loader.index(
    "devices[DEVICE_CUR]->closeFile(&curFile);", context_load
)
opposite_close = image_loader.index(
    "devices[DEVICE_CUR]->closeFile(context.oppositeDisc);", primary_close
)
copy_back = image_loader.index("memcpy(image, &curFile", opposite_close)
assert context_load < primary_close < opposite_close < copy_back
assert "DrawSetGameflowMode(context.event, UI_GAMEFLOW_MODE_LIBRARY);" in image_loader
assert "DrawClearGameflowDetail(context.event);" in image_loader

folder_loader = extract_function(SWISS, "static bool gameflowResolveAndLoadFolder(")
assert "folder->device->quirks & QUIRK_GCLOADER_NO_DISC_2" in folder_loader
resolve_call = folder_loader.index("UIGameflowResolver_Resolve(")
primary_meta = folder_loader.index(
    "gameflowPopulateResolvedMeta(context.primary,", resolve_call
)
opposite_meta = folder_loader.index(
    "gameflowPopulateResolvedMeta(context.oppositeDisc,", primary_meta
)
resolver_free = folder_loader.index("free(resolverEntries);", opposite_meta)
assert resolve_call < primary_meta < opposite_meta < resolver_free

meta_population = extract_function(SWISS, "static bool gameflowPopulateResolvedMeta(")
assert "populate_meta(file);" in meta_population
assert "file->meta->diskId.gamename" in meta_population
assert "file->meta->diskId.company" in meta_population
assert "file->meta->diskId.disknum" in meta_population
assert "file->meta->diskId.gamever" in meta_population

meta_protection = extract_function(SWISS, "static void gameflowProtectMetaFile(")
assert "getSortedDirEntries()" in meta_protection
assert "getSortedDirEntryCount()" in meta_protection
assert "getCurrentDirEntries()" not in meta_protection
assert "current_view_end = i;" in meta_protection

snapshot_record = extract_function(SWISS, "static void gameflowSnapshotRecord(")
assert "if(!record->gameId[0])" in snapshot_record
assert "gameflowReadResolverHeader(file, &headerEntry)" in snapshot_record
assert "file->device == &__device_flippy" in snapshot_record
assert "file->device == &__device_flippyflash" in snapshot_record
assert "file->device->closeFile(file);" in snapshot_record
assert "memcpy(record->gameId, headerEntry.gameId" in snapshot_record
assert "sizeof(headerEntry.gameId)" in snapshot_record
assert "record->gameId[sizeof(record->gameId) - 1u] = '\\0';" in snapshot_record


# Library Layout: each layout's window, presses and stick; Y opens a game's
# settings through its retained Detail. Every rule below has a mutant that
# must fail it.
def check_layouts(swiss: str) -> None:
    carousel = extract_function(swiss, "uiDrawObj_t* renderFileCarousel(")
    build = extract_function(swiss, "static bool gameflowBuildSnapshot(")
    layout_of = extract_function(swiss, "static uiGameflowLayout_t gameflowLayout(")
    detail = extract_function(swiss[swiss.rindex("static int gameflow_info_game"):],
                              "static int gameflow_info_game")
    folder = extract_function(swiss, "static bool gameflowResolveAndLoadFolder(")
    image = extract_function(swiss, "static bool gameflowLoadImageWithContext(")

    # Anything but Vertical or Grid is the carousel.
    assert "swissSettings.libraryLayout > UI_GAMEFLOW_LAYOUT_HORIZONTAL &&" in layout_of
    assert "swissSettings.libraryLayout < UI_GAMEFLOW_LAYOUT_COUNT ?" in layout_of
    assert "UI_GAMEFLOW_LAYOUT_HORIZONTAL;" in layout_of
    # The grid window only for the grid; its off-screen rows never read.
    grid_at = build.index("if(layout == UI_GAMEFLOW_LAYOUT_GRID) {")
    ring_at = build.index("UIGameflowLibrary_BuildWindow(", grid_at)
    filter_at = build.index("unread = file->fileType == IS_FILE && file->meta == NULL;")
    assert grid_at < build.index("UIGameflowLibrary_BuildGridWindow(", grid_at) < \
        filter_at < ring_at
    assert ("if(!unread || (slots[i].relativeSlot >= -1 &&\n"
            "\t\t\t\tslots[i].relativeSlot <= 1)) {") in build
    # Each layout's presses; the carousel's are the ones it always had.
    ring = carousel[carousel.index("/* The carousel: left and right step, up and down page. */"):]
    ring = ring[:ring.index("for(int move = 0;")]
    for line in ("if(left) moves[moveCount++] = UI_GAMEFLOW_LIBRARY_MOVE_PREVIOUS;",
                 "if(right) moves[moveCount++] = UI_GAMEFLOW_LIBRARY_MOVE_NEXT;",
                 "if(up || (browserButtons & BUTTON_L))\n\t\t\t\t\tmoves[moveCount++] = UI_GAMEFLOW_LIBRARY_MOVE_PAGE_BACK;",
                 "if(down || (browserButtons & BUTTON_R))\n\t\t\t\t\tmoves[moveCount++] = UI_GAMEFLOW_LIBRARY_MOVE_PAGE_ON;"):
        assert line in ring, line
    column = carousel[carousel.index("/* The column: up and down step, left and right page. */"):]
    column = column[:column.index("else if(layout == UI_GAMEFLOW_LAYOUT_GRID)")]
    assert "if(up) moves[moveCount++] = UI_GAMEFLOW_LIBRARY_MOVE_PREVIOUS;" in column
    assert "if(down) moves[moveCount++] = UI_GAMEFLOW_LIBRARY_MOVE_NEXT;" in column
    assert "if(left || (browserButtons & BUTTON_L))" in column
    grid = carousel[carousel.index("/* The grid: every direction steps, L and R page. */"):]
    grid = grid[:grid.index("/* The carousel:")]
    assert "if(up) moves[moveCount++] = UI_GAMEFLOW_LIBRARY_MOVE_UP;" in grid
    assert "if(down) moves[moveCount++] = UI_GAMEFLOW_LIBRARY_MOVE_DOWN;" in grid
    assert ("columns ?\n\t\t\t\t\tUI_GAMEFLOW_LIBRARY_GRID_ROWS : FILES_PER_PAGE_CAROUSEL,"
            in carousel)
    # Y: physical Y only, never on the parent card, A wins; the loaders
    # carry it into Detail, which opens the settings once and never boots.
    entry = carousel[carousel.index("bool openSettings ="):]
    entry = entry[:entry.index(";")]
    assert "!(browserButtons & BUTTON_A)" in entry
    assert "(browserButtons & PAD_BUTTON_Y)" in entry
    assert "UIGameflowLibrary_UsesRetainedDetail(" in entry
    branch = carousel.index("if((browserButtons & BUTTON_A) || openSettings) {")
    loaders = carousel.index("gameflowSnapshot, openSettings);", branch)
    loaders = carousel.index("gameflowSnapshot, openSettings);", loaders + 1)
    stop = carousel.index("if(openSettings) {", loaders)
    stop_break = carousel.index("break;", stop)
    legacy = carousel.index("//go into a folder or select a file", stop)
    assert branch < loaders < stop < stop_break < legacy
    for loader in (folder, image):
        assert "bool openSettings)" in loader
        assert "context.openSettings = openSettings;" in loader
    assert "(swissSettings.autoBoot && !context->openSettings)) {" in detail
    assert "openSettings = context->openSettings;\n\tcontext->openSettings = false;" in detail
    assert "return openSettings ? 0 : info_game(config);" in detail
    once = detail.index("if(openSettings) {")
    assert detail.index("openSettings = false;", once) < \
        detail.index("action = UI_GAMEFLOW_DETAIL_ACTION_SETTINGS;", once) < \
        detail.index("buttons = UIMenuAction_Update(", once)
    assert detail.count("show_settings_view(VIEW_GAME, 0, config)") == 1


check_layouts(SWISS)
layout_mutants = (
    ("an unknown layout is kept", "swissSettings.libraryLayout < UI_GAMEFLOW_LAYOUT_COUNT ?",
     "swissSettings.libraryLayout < 99 ?"),
    ("the carousel gets the grid window", "if(layout == UI_GAMEFLOW_LAYOUT_GRID) {\n\t\tsize_t kept = 0u;",
     "if(layout != UI_GAMEFLOW_LAYOUT_HORIZONTAL) {\n\t\tsize_t kept = 0u;"),
    ("visible grid rows are skipped", "(slots[i].relativeSlot >= -1 &&\n\t\t\t\tslots[i].relativeSlot <= 1)",
     "(slots[i].relativeSlot == 0)"),
    ("the carousel loses its up page", "if(up || (browserButtons & BUTTON_L))\n\t\t\t\t\tmoves[moveCount++] = UI_GAMEFLOW_LIBRARY_MOVE_PAGE_BACK;\n\t\t\t\tif(down || (browserButtons & BUTTON_R))\n\t\t\t\t\tmoves[moveCount++] = UI_GAMEFLOW_LIBRARY_MOVE_PAGE_ON;\n\t\t\t}\n\t\t\tfor",
     "if(browserButtons & BUTTON_L)\n\t\t\t\t\tmoves[moveCount++] = UI_GAMEFLOW_LIBRARY_MOVE_PAGE_BACK;\n\t\t\t\tif(down || (browserButtons & BUTTON_R))\n\t\t\t\t\tmoves[moveCount++] = UI_GAMEFLOW_LIBRARY_MOVE_PAGE_ON;\n\t\t\t}\n\t\t\tfor"),
    ("the column steps on left", "if(up) moves[moveCount++] = UI_GAMEFLOW_LIBRARY_MOVE_PREVIOUS;\n\t\t\t\tif(down)",
     "if(left) moves[moveCount++] = UI_GAMEFLOW_LIBRARY_MOVE_PREVIOUS;\n\t\t\t\tif(down)"),
    ("the grid pages by cards", "UI_GAMEFLOW_LIBRARY_GRID_ROWS : FILES_PER_PAGE_CAROUSEL,",
     "FILES_PER_PAGE_CAROUSEL : FILES_PER_PAGE_CAROUSEL,"),
    ("the C-stick opens settings", "(browserButtons & PAD_BUTTON_Y) &&\n\t\t\tUIGameflowLibrary_UsesRetainedDetail(",
     "(browserButtons & BUTTON_Y) &&\n\t\t\tUIGameflowLibrary_UsesRetainedDetail("),
    ("Y on the parent card", "(browserButtons & PAD_BUTTON_Y) &&\n\t\t\tUIGameflowLibrary_UsesRetainedDetail(gameflowMode,\n\t\t\t\tgameflowEntryType(directory[curSelection]));",
     "(browserButtons & PAD_BUTTON_Y);"),
    ("Y falls into the legacy file path", "\t\t\tif(openSettings) {\n\t\t\t\t/* Y has no legacy meaning: it never opens or boots a file. */\n\t\t\t\twhile(padsButtonsHeld() & PAD_BUTTON_Y) VIDEO_WaitVSync();\n\t\t\t\tbreak;\n\t\t\t}\n",
     ""),
    ("a loader drops Y", "\tcontext.openSettings = openSettings;\n\tmemcpy(context.gameId, headerEntry.gameId",
     "\tmemcpy(context.gameId, headerEntry.gameId"),
    ("Y boots without prompts", "(swissSettings.autoBoot && !context->openSettings)) {",
     "swissSettings.autoBoot) {"),
    ("Y opens the settings twice", "openSettings = context->openSettings;\n\tcontext->openSettings = false;",
     "openSettings = context->openSettings;"),
    ("a failed Detail boots from Y", "return openSettings ? 0 : info_game(config);",
     "return info_game(config);"),
)
for label, old_text, new_text in layout_mutants:
    assert SWISS.count(old_text) == 1, f"mutation anchor missing: {label}"
    try:
        check_layouts(SWISS.replace(old_text, new_text, 1))
    except (AssertionError, ValueError):
        continue
    raise AssertionError(f"layout mutant escaped the dispatch audit: {label}")

print(f"gameflow dispatch audit OK ({len(layout_mutants)} layout mutants rejected)")
