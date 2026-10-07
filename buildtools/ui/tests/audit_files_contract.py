#!/usr/bin/env python3
"""The File Browser keeps Swiss's list contract (swiss.c's renderFileList).

Its left pane is the shared listing menu_loop scans, so it does what Swiss's
lists did with it: stops the meta thread before anything acts on a file,
locks an entry while it reads it, keeps current_view_start/end on the left
pane's visible rows whichever pane is focused, reads the visible rows'
banners a row a frame (and the focused entry's before A acts on it) and
never in a draw, and takes the presses a busy moment would lose, but not
those the Source picker had. Its right pane reads its own listing and never touches
what the shared one feeds. Every rule has a mutant that must fail it.
"""

import re
import sys
from pathlib import Path

if not __debug__:
    raise SystemExit("audit_files_contract.py needs Python assertions; run it without -O")

ROOT = Path(__file__).resolve().parents[3]
SWISS = (ROOT / "cube/swiss/source/swiss.c").read_text()
FRAME = (ROOT / "cube/swiss/source/gui/FrameBufferMagic.c").read_text()


def function(source: str, marker: str) -> str:
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        depth += (source[index] == "{") - (source[index] == "}")
        if depth == 0:
            return source[start:index + 1]
    raise AssertionError(marker)


def before(text: str, first: str, second: str, start: int = 0) -> None:
    a = text.index(first, start)
    assert a < text.index(second, a), f"{first!r} must come before {second!r}"


def check(swiss: str, frame: str) -> None:
    screen = function(swiss, "static uiDrawObj_t* renderFileList(")
    menu = function(swiss, "void menu_loop()")
    dispatch = function(menu, "if(devices[DEVICE_CUR] != NULL && curMenuLocation==ON_FILLIST)")
    manage = function(swiss, "static bool filesManageEntry(")
    step = function(swiss, "static bool filesMetaStep(")
    panes = function(swiss, "static bool filesPaneSnapshot(")
    info = function(swiss, "static bool filesInfoSnapshot(")
    focus = function(swiss, "static void filesLeftFocusName(")
    carousel = function(swiss, "uiDrawObj_t* renderFileCarousel(")

    # It browses the shared listing, the one scanFiles keeps.
    assert re.search(r"renderFileList\(getSortedDirEntries\(\),\s*getSortedDirEntryCount\(\), filePanel\)",
                     dispatch), "the File Browser reads another listing"
    # The meta thread: started once the page is up, stopped on every way out
    # and before any action on a file (filesActivate and filesManage stop it
    # themselves; filesManageEntry is the screen's own).
    before(screen, "filesPublish(directory, &filePanel, true)", "meta_thread_start(loadingBox);")
    assert screen.rstrip().endswith("return filePanel;\n}") and \
        "meta_thread_stop();\n\tDrawDispose(loadingBox);" in screen, "the meta thread outlives the screen"
    before(manage, "meta_thread_stop();", "manage_file();")
    for action in ("static void filesActivate(", "static bool filesManage("):
        body = function(swiss, action)
        for call in ("load_file();", "manage_file()"):
            if call in body:
                before(body, "meta_thread_stop();", call)
    # After an action on the right pane, scanFiles finds the left pane's
    # focus again, not the right entry's path.
    before(manage, "memcpy(entry, &curFile, sizeof(file_handle));",
           "strlcpy(curFile.name, leftFocus, sizeof(curFile.name));")
    before(screen, "filesLeftFocusName(directory, leftFocus, sizeof(leftFocus));",
           "filesManageEntry(entry, leftFocus)")
    # Entries the meta thread may be filling are read under their lock.
    for body, name in ((panes, "rows"), (info, "info bar")):
        # Locked, or (refreshing while idle) given up when the entry is busy.
        assert re.search(r"if\(!wait && !trylockFile\(entry\)\) \{\s*return false;", body) and \
            re.search(r"if\(wait\) \{\s*lockFile\(entry\);", body) and \
            "unlockFile(entry);" in body, f"{name} reads unlocked"
    for body, name in ((focus, "focus name"), (step, "banner read")):
        assert re.search(r"(?<!un)lockFile\(", body) and "unlockFile(" in body, f"{name} reads unlocked"
    before(step, "lockFile(directory[i]);", "populate_meta(directory[i]);")
    # current_view_* is the left pane's window, whichever pane is focused.
    section = swiss[swiss.index(" * The File Browser: two panes side by side"):
                    swiss.index("uiDrawObj_t* renderFileBrowser(")]
    assert not re.search(r"current_view_(?:start|end)\s*=", section), \
        "current_view_* set other than from UIFiles_LeftView"
    assert "&current_view_" not in section.replace(
        "UIFiles_LeftView(&filesState, &current_view_start, &current_view_end);", "")
    assert screen.count("UIFiles_LeftView(&filesState, &current_view_start, &current_view_end);") == 2
    # Banners: one visible row a frame from this thread, with a meta thread
    # or without (it keeps the meta cache's eviction going), the focused
    # entry's before A, and never while drawing.
    assert "FEAT_THREAD_SAFE" not in step and step.count("populate_meta(") == 1
    before(step, "populate_meta(directory[i]);", "return true;")
    press = function(screen, "if(buttons & BUTTON_A)")
    before(press, "populate_meta(directory[curSelection]);", "filesActivate(directory, false);")
    for draw in ("static void _DrawFiles(", "static void _FilesShapes(", "static void _FilesWords(",
                 "static void _FilesCube("):
        body = function(frame, draw)
        for banned in ("populate_meta", "lockFile", "GetTextSizeInPixels", "alloc(", "free("):
            assert banned not in body, f"{draw} {banned}"
    # The right pane's listing is its own.
    for reader in ("static void filesOtherRead(", "static void filesOtherOpen(",
                   "static void filesOtherRelease(", "static void filesOtherAcquire("):
        body = function(swiss, reader)
        for banned in ("scanFiles", "populate_meta", "meta_thread_", "current_view_",
                       "curSelection", "curFile", "curDir"):
            assert banned not in body, f"{reader} {banned}"
    assert "->readDir(&filesOther.dir, &filesOther.entries, -1)" in function(
        swiss, "static void filesOtherRead(")
    # Presses: taken from the scans; the old ones dropped on opening and
    # after a box, those made while a left folder is read kept.
    wait = screen[screen.index("\t\twhile(1) {\n\t\t\tu32 now"):screen.index("filesNote = NULL;\n\t\tactive")]
    assert "padsButtonsTaken(waitButtons);" in wait and "(void)padsButtonsTaken" not in wait
    before(screen, "if(!filesKeepPresses) {\n\t\t(void)padsButtonsTaken(waitButtons);",
           "\twhile(1) {")
    assert screen.count("filesKeepPresses = ") == 3, "presses kept for more than a folder change"
    # The Source picker reads held buttons: what it was given is not kept.
    change = function(menu, "if(needsDeviceChange) {\n\t\tDEVICEHANDLER_INTERFACE")
    before(change, "filesKeepPresses = false;", "select_device_internal(DEVICE_CUR)")
    before(screen, "/* A box (Z, Autoload) may have left presses of its own. */",
           "(void)padsButtonsTaken(waitButtons);",
           screen.index("if(buttons & BUTTON_B)"))
    # B: the page leaves before Home is published.
    back = function(screen, "if(buttons & BUTTON_B)")
    before(back, "filesSnapshot.leaving = 1;", "filesWait(")
    before(back, "filesWait(", "filesHome(directory, true);")
    # A folder being read says so, then the right listing goes before the
    # Source can.
    refresh = function(menu, "if(devices[DEVICE_CUR] != NULL && needsRefresh)")
    before(refresh, "DrawUpdateFilesReading(filePanel, UI_FILES_LEFT);", "scanFiles();")
    for opening in ("filesOtherOpen(NULL, NULL);", "filesOtherOpen(entry, \"\");"):
        at = screen.index(opening)
        assert "DrawUpdateFilesReading(filePanel, UI_FILES_RIGHT);" in screen[at - 120:at], opening
    before(refresh, "filesOtherRelease();", "->deinit(devices[DEVICE_CUR]->initial);")
    assert "if(curMenuLocation != ON_FILLIST || needsDeviceChange) {\n\t\tfilesOtherRelease();" in screen
    # The Library that can't draw a folder leaves it to the File Browser,
    # read again in fileComparator's order; the flag lasts one listing.
    fallback = carousel[carousel.index("if(!useGameflow) {\n\t\t\t/* No Library"):]
    fallback = fallback[:fallback.index("break;")]
    for token in ("memcpy(curFile.name, directory[curSelection]->name",
                  "gameflowListFallback = true;", "needsRefresh = 1;"):
        assert token in fallback, token
    assert "drawFilesCarousel(" not in carousel
    assert screen.index("gameflowListFallback = false;") < screen.index("if(num_files<=0)")
    # The page covers the frame, so nothing behind it is drawn.
    assert "event->type == EV_FILES" in function(frame, "static bool _FrameCovered(")


check(SWISS, FRAME)

MUTANTS = (
    ("the meta thread runs into manage_file", SWISS,
     "\tmeta_thread_stop();\n\tmemcpy(&curFile, entry, sizeof(file_handle));\n\tchanged = manage_file();",
     "\tmemcpy(&curFile, entry, sizeof(file_handle));\n\tchanged = manage_file();"),
    ("the left focus is not put back", SWISS,
     "\tif(leftFocus != NULL) {\n\t\tstrlcpy(curFile.name, leftFocus, sizeof(curFile.name));\n\t}\n", ""),
    ("rows read unlocked", SWISS, "\t\t\tif(wait) {\n\t\t\t\tlockFile(entry);\n\t\t\t}\n\t\t}\n\t\trow->kind",
     "\t\t}\n\t\trow->kind"),
    ("the view follows the right pane", SWISS,
     "\tUIFiles_LeftView(&filesState, &current_view_start, &current_view_end);\n\tfilesLeftListing++;",
     "\tcurrent_view_start = 0;\n\tfilesLeftListing++;"),
    ("banners left to a meta thread", SWISS,
     "static bool filesMetaStep(file_handle **directory)\n{\n",
     "static bool filesMetaStep(file_handle **directory)\n{\n\tif(devices[DEVICE_CUR]->features & FEAT_THREAD_SAFE) return false;\n"),
    ("A before the focused entry's meta", SWISS,
     "\t\t\t\tpopulate_meta(directory[curSelection]);\n\t\t\t\ttype", "\t\t\t\ttype"),
    ("the picker's presses kept", SWISS,
     "\t\tfilesKeepPresses = false;\n\t\thomePublish(", "\t\thomePublish("),
    ("no word while the right pane reads", SWISS,
     "\t\t\t\t(void)DrawUpdateFilesReading(filePanel, UI_FILES_RIGHT);\n\t\t\t\tfilesOtherOpen(entry",
     "\t\t\t\tfilesOtherOpen(entry"),
    ("the right pane reads banners", SWISS,
     "\tfilesOther.listed = true;\n\tfilesOther.listing++;",
     "\tpopulate_meta(filesOther.sorted[0]);\n\tfilesOther.listed = true;\n\tfilesOther.listing++;"),
    ("presses before opening kept", SWISS,
     "\tif(!filesKeepPresses) {\n\t\t(void)padsButtonsTaken(waitButtons);\n\t}\n", ""),
    ("a press during a read lost", SWISS,
     "\t\t\tbuttons = padsButtonsTaken(waitButtons);\n", "\t\t\tbuttons = held;\n"),
    ("B goes Home before the page leaves", SWISS,
     "\t\t\tfilesSnapshot.leaving = 1;\n", ""),
    ("no word while a folder is read", SWISS,
     "\t\t\t(void)DrawUpdateFilesReading(filePanel, UI_FILES_LEFT);\n", ""),
    ("the Source goes under the right pane", SWISS,
     "\t\t\t\thomeLibraryEntryPending = false;\n\t\t\t\tfilesOtherRelease();\n",
     "\t\t\t\thomeLibraryEntryPending = false;\n"),
    ("the fallback keeps the Library's order", SWISS,
     "\t\t\tgameflowListFallback = true;\n\t\t\tneedsRefresh = 1;\n", "\t\t\tgameflowListFallback = true;\n"),
    ("the page leaves the background drawn", FRAME,
     " || event->type == EV_FILES", ""),
    ("a draw reads banners", FRAME,
     "\tUIFiles_Layout(UIStage_Left(), UIStage_Right(), &layout);\n\t_SaveCubesBackdrop(",
     "\tUIFiles_Layout(UIStage_Left(), UIStage_Right(), &layout);\n\tpopulate_meta(NULL);\n\t_SaveCubesBackdrop("),
)
for label, source, old, new in MUTANTS:
    assert source.count(old) == 1, f"mutation anchor missing: {label}"
    mutated = source.replace(old, new, 1)
    try:
        check(mutated if source is SWISS else SWISS, mutated if source is FRAME else FRAME)
    except (AssertionError, ValueError):
        continue
    raise AssertionError(f"mutant escaped the File Browser contract audit: {label}")

print(f"File Browser contract audit OK ({len(MUTANTS)} mutants rejected)")
sys.exit(0)
