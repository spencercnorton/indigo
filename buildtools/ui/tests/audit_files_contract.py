#!/usr/bin/env python3
"""The File Browser keeps Swiss's list contract (swiss.c's renderFileList).

Its left pane is the shared listing menu_loop scans, so it does what Swiss's
lists did with it: stops the meta thread before anything acts on a file,
locks an entry while it reads it, keeps current_view_start/end on the left
pane's visible rows whichever pane is focused, reads the visible rows'
banners a row a frame (and the focused entry's before A acts on it) and
never in a draw, and takes the presses a busy moment would lose, but not
those the Source picker had. Its right pane reads its own listing and never touches
what the shared one feeds.

The right pane may hold a storage of its own, mounted outside devices[]. It
lets it go before control leaves the screen and before anything that mounts
or unmounts storage itself (menu_loop's Library and its empty-folder branch,
starting a file, Autoload, Recent, a Source change, either Other devices...,
B), a reset shuts it down, a read that fails mounts it once more, and Swiss's
box on its entries borrows the Source's slot and gives it back. Swiss's box
on either side keeps the other side's storage mounted. Every rule has a
mutant that must fail it.
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
    before(manage, "meta_thread_stop();", "filesManageFile(")
    for action in ("static void filesActivate(", "static bool filesManage("):
        body = function(swiss, action)
        assert "manage_file()" not in body, f"{action} runs Swiss's box past filesManageFile"
        for call in ("load_file();", "filesManageFile("):
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
                   "static void filesOtherRelease(", "static void filesOtherAcquire(",
                   "static void filesOtherFree(", "static bool filesOtherMount(",
                   "static void filesOtherChoose(", "static void filesOtherPick("):
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
    before(screen, "/* A box (Z, Autoload, a storage menu) may have left presses of its\n\t\t * own. */",
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
    check_second_device(swiss, screen, menu, dispatch, refresh, manage)


def check_second_device(swiss: str, screen: str, menu: str, dispatch: str, refresh: str,
                        manage: str) -> None:
    release = "filesOtherRelease();"
    acquire = function(swiss, "static void filesOtherAcquire(")
    letgo = function(swiss, "static void filesOtherRelease(")
    read = function(swiss, "static void filesOtherRead(")
    pick = function(swiss, "static void filesOtherPick(")
    change = function(swiss, "static void filesSourceChange(")
    swap = function(swiss, "static bool filesSwapSides(")
    reset = function(swiss, "static s32 filesOtherOnReset(")
    # menu_loop: before the Library, which mounts and unmounts the
    # Configuration Device by itself, and before the empty folder's Source
    # goes. The scene requests are the Library's alone.
    library = dispatch[:dispatch.index("else {")]
    before(library, release, "renderFileCarousel(")
    assert library.index(release) < library.index("UIScene_Request("), "the Library is asked for first"
    assert "UIScene_Request" not in dispatch[dispatch.index("else {"):], "the File Browser asks for a scene"
    before(refresh, release, "->deinit(devices[DEVICE_CUR]->initial);")
    # It mounts its own and lets only its own go.
    assert re.search(r"if\(filesOther\.mount == UI_FILES_OWN\) \{\s*filesOther\.device->deinit\(", letgo)
    before(letgo, "filesOtherFree();", "->deinit(")
    assert "filesOther.mount = UI_FILES_UNMOUNTED;" in letgo
    mount = function(swiss, "static bool filesOtherMount(")
    assert re.search(r"if\(device == devices\[DEVICE_CUR\]\) \{\s*filesOther\.mount = UI_FILES_SHARED;\s*return true;", mount)
    before(mount, "deviceHandler_setStatEnabled(0);", "->init(device->initial);")
    before(mount, "->init(device->initial);", "deviceHandler_setStatEnabled(1);")
    # The read is the mount check: a failed read on its own mount mounts once more.
    assert re.search(r"if\(filesOther\.read < 0 && filesOther\.mount == UI_FILES_OWN\) \{"
                     r"[^}]*if\(filesOtherMount\(\)\) \{\s*filesOther\.read = device->readDir\(", read), \
        "a failed read isn't the mount check"
    for rule in ("filesOther.mount == UI_FILES_SHARED && filesOther.device != source",
                 "filesOther.mount == UI_FILES_UNMOUNTED"):
        assert rule in acquire, rule
    # In renderFileList: before starting a file, Autoload (a settings save),
    # Recent, either Other devices... and B; a Source change and Y let it go
    # in their helpers.
    arm = screen[screen.index("if(type == IS_SPECIAL && filesAtRoot(&curDir))"):]
    before(arm, "if(type == IS_FILE && loads) {\n\t\t\t\t\t\tfilesOtherRelease();", "filesActivate(directory, false);")
    toggles = [m.start() for m in re.finditer(r"filesToggleAutoload\(", screen)]
    assert len(toggles) == 2, "Autoload from somewhere else"
    for at in toggles:
        assert screen[:at].rstrip().endswith(release), "Autoload with the right pane's storage held"
        assert screen[at:].split("\n", 2)[1].strip() == "filesOtherAcquire();", "Autoload doesn't bring it back"
    recent = screen[screen.index("if((buttons & BUTTON_START)"):]
    before(recent, release, "filesRecent(false)")
    other = screen[screen.index("if(choice == filesMenu.devices) {"):]
    before(other, release, "needsDeviceChange = 1;")
    back = function(screen, "if(buttons & BUTTON_B)")
    before(back, release, "filesHome(directory, true);")
    before(change, release, "sourceCommit(device);")
    before(swap, "UIFiles_CanSwap(", "sourceCommit(")
    before(swap, release, "sourceCommit(right);")
    assert re.search(r"if\(!UIFiles_CanSwap\([^)]*\)\) \{[^}]*return false;", swap), "Y onto a pane that isn't ready"
    assert "if(curMenuLocation != ON_FILLIST || needsDeviceChange) {\n\t\tfilesOtherRelease();" in screen
    # R, Other devices...: Swiss's destination picker with an empty slot,
    # the slot and the scene put back after.
    before(pick, release, "devices[DEVICE_DEST] = NULL;")
    before(pick, "devices[DEVICE_DEST] = NULL;", "select_device(DEVICE_DEST);")
    before(pick, "while(padsButtonsHeld() & PAD_BUTTON_A) VIDEO_WaitVSync();", "select_device(DEVICE_DEST);")
    before(pick, "select_device(DEVICE_DEST);", "devices[DEVICE_DEST] = dest;")
    before(pick, "select_device(DEVICE_DEST);", "UIScene_Request(filesScene);")
    assert "devices[DEVICE_DEST] = chosen" not in pick, "the pane's storage kept in DEVICE_DEST"
    # A reset shuts down the pane's own storage: registered from swiss.c,
    # after the interface stops reading (priority 1, as deviceHandler.c's).
    assert re.search(r"__attribute__\(\(constructor\)\)\s*static void \w+\(void\)\s*\{\s*"
                     r"SYS_RegisterResetFunc\(&filesOtherResetInfo\);", swiss), "no reset function"
    assert re.search(r"static sys_resetinfo filesOtherResetInfo = \{\s*\{NULL, NULL\}, filesOtherOnReset, 1\s*\};",
                     swiss), "the reset function's priority"
    assert re.search(r"if\(!final && filesOther\.mount == UI_FILES_OWN && filesOther\.device != NULL &&\s*"
                     r"!\(filesOther\.device->quirks & QUIRK_NO_DEINIT\)\) \{\s*filesOther\.device->deinit\(",
                     reset), "the reset shuts down what it shouldn't, or nothing"
    # Swiss's box on a right-pane entry borrows the Source's slot and gives
    # it back on its one way out; the Source stays mounted meanwhile.
    before(manage, "devices[DEVICE_CUR] = filesOther.device;", "filesManageFile(")
    before(manage, "filesManageFile(", "devices[DEVICE_CUR] = cur;")
    assert "filesManageFile(other ? cur : NULL);" in manage and "manage_file()" not in manage, \
        "the box on a right entry doesn't keep the Source mounted"
    # Swiss's box keeps the other side's storage: the Source for a right
    # entry, else the right pane's own mount; never in DEVICE_DEST, where the
    # destination picker unmounts it.
    keeper = function(swiss, "static bool filesManageFile(DEVICEHANDLER_INTERFACE *keep)\n{")
    assert re.search(r"if\(keep == NULL && filesOther\.mount == UI_FILES_OWN\) \{\s*keep = filesOther\.device;",
                     keeper), "the right pane's own storage isn't kept"
    assert re.search(r"if\(keep != NULL && devices\[DEVICE_DEST\] == keep\) \{\s*devices\[DEVICE_DEST\] = NULL;",
                     keeper), "the kept storage left in DEVICE_DEST"
    before(keeper, "devices[DEVICE_DEST] = NULL;", "manage_file();")
    before(keeper, "manageKeep = keep;", "manage_file();")
    before(keeper, "manage_file();", "manageKeep = NULL;")
    assert len(re.findall(r"(?<![\w])manage_file\(\)", swiss[swiss.index(" * The File Browser: two panes side by side"):
                                                     swiss.index("uiDrawObj_t* renderFileBrowser(")])) == 1
    # Choosing another storage lets the old one go, including a clash reset.
    choose = function(swiss, "static void filesOtherChoose(")
    before(choose, release, "filesOther.device = device;")
    assert re.search(r"if\(filesOther\.device == NULL \|\| filesClash\([^)]*\)\) \{\s*filesOtherRelease\(\);",
                     acquire), "a clash reset keeps the old storage mounted"
    # The first device: the Source without File Management.
    assert "UIFiles_RightOnConfig(fileManagementAllowed()," in function(
        swiss, "static DEVICEHANDLER_INTERFACE *filesOtherDefault("), "a second mount with nothing to do"
    # Swiss's picker greys nothing: a clash keeps the pane and says why.
    before(pick, "filesClash(chosen, devices[DEVICE_CUR], filesNoteText", "filesOtherChoose(chosen);")
    assert re.search(r"filesNote = filesNoteText;\s*chosen = NULL;", pick), "the picker's clash is silent"
    # A greyed storage can't be chosen.
    assert "if((buttons & BUTTON_A) && !((filesSnapshot.menu.dim >> focus) & 1u)) {" in function(
        swiss, "static int filesStorageMenu("), "A chooses a greyed storage"
    # L's device becomes the Source in place, mounted, and X at the top
    # opens the left menu rather than Swiss's Source picker.
    before(change, "sourceCommit(device);", "(void)sourceMount();")
    assert re.search(r"if\(filesMenuDevices\[choice\] != devices\[DEVICE_CUR\]\) \{\s*"
                     r"filesSourceChange\(filesMenuDevices\[choice\]\);\s*break;", screen), "L's choice does nothing"
    before(screen, "else if((buttons & PAD_BUTTON_X) && filesAtRoot(&curDir)) {\n\t\t\t\tstorage = UI_FILES_LEFT;",
           "else if(buttons & PAD_BUTTON_X) {\n\t\t\t\tfilesUp(directory[0]);")
    assert manage.count("return") == 1 and manage.index("devices[DEVICE_CUR] = cur;") < manage.index("return")
    manager = function(swiss, "bool manage_file() {")
    assert manager.count("devices[DEVICE_DEST] != devices[DEVICE_CUR] && devices[DEVICE_DEST] != manageKeep") == 2
    # L and R choose storage; only the C-stick pages.
    for page in ("UI_FILES_INPUT_PAGE_UP", "UI_FILES_INPUT_PAGE_DOWN"):
        at = screen.index(page)
        assert "BUTTON_L" not in screen[at - 80:at] and "BUTTON_R" not in screen[at - 80:at], page
    # The Source changes through the picker's own two steps.
    commit = function(swiss, "static void sourceCommit(DEVICEHANDLER_INTERFACE *device)\n{")
    for step in ("freeFiles();", "DrawGameflowCancelPosters();", "->deinit(devices[DEVICE_CUR]->initial);",
                 "devices[DEVICE_CUR] = device;"):
        assert step in commit, step
    assert "sourceCommit(selectedDevice);" in function(swiss, "static bool select_device_internal(")
    assert "!sourceMount()" in menu


check(SWISS, FRAME)

MUTANTS = (
    ("the meta thread runs into manage_file", SWISS,
     "\tmeta_thread_stop();\n\t/* An entry on the right pane's own storage",
     "\t/* An entry on the right pane's own storage"),
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
    ("the Library under a mounted right pane", SWISS,
     "\t\t\t\tfilesOtherRelease();\n\t\t\t\tfilesKeepPresses = false;\n", "\t\t\t\tfilesKeepPresses = false;\n"),
    ("a file starts with the right pane mounted", SWISS,
     "\t\t\t\t\tif(type == IS_FILE && loads) {\n\t\t\t\t\t\tfilesOtherRelease();\n\t\t\t\t\t}\n", ""),
    ("left Autoload under a mounted right pane", SWISS,
     "\t\t\t\tfilesOtherRelease();\n\t\t\t\tfilesToggleAutoload(&curDir.name[0]);",
     "\t\t\t\tfilesToggleAutoload(&curDir.name[0]);"),
    ("right Autoload under a mounted right pane", SWISS,
     "\t\t\t\t\tfilesOtherRelease();\n\t\t\t\t\tfilesToggleAutoload(filesOther.dir.name);",
     "\t\t\t\t\tfilesToggleAutoload(filesOther.dir.name);"),
    ("Autoload doesn't bring the right pane back", SWISS,
     "\t\t\t\tfilesToggleAutoload(&curDir.name[0]);\n\t\t\t\tfilesOtherAcquire();\n",
     "\t\t\t\tfilesToggleAutoload(&curDir.name[0]);\n"),
    ("Recent under a mounted right pane", SWISS,
     "\t\t\tfilesOtherRelease();\n\t\t\t(void)filesRecent(false);", "\t\t\t(void)filesRecent(false);"),
    ("the left Other devices... under a mounted right pane", SWISS,
     "\t\t\t\t\tfilesOtherRelease();\n\t\t\t\t\tneedsDeviceChange = 1;", "\t\t\t\t\tneedsDeviceChange = 1;"),
    ("B leaves the right pane mounted", SWISS,
     "\t\t\tfilesOtherRelease();\n\t\t\t/* No Swiss row to dim on the way out. */",
     "\t\t\t/* No Swiss row to dim on the way out. */"),
    ("a Source change under a mounted right pane", SWISS,
     "\tfilesOtherRelease();\n\tmeta_thread_stop();\n\tsourceCommit(device);", "\tmeta_thread_stop();\n\tsourceCommit(device);"),
    ("Y under a mounted right pane", SWISS,
     "\t\tfilesOtherRelease();\n\t\tmeta_thread_stop();\n\t\tfilesOther.device = left;",
     "\t\tmeta_thread_stop();\n\t\tfilesOther.device = left;"),
    ("Y onto a pane that isn't ready", SWISS,
     "\tif(!UIFiles_CanSwap(filesOther.mount, !filesOther.readFailed)) {", "\tif(false) {"),
    ("the right Other devices... under a mounted right pane", SWISS,
     "\tfilesOtherRelease();\n\tdevices[DEVICE_DEST] = NULL;", "\tdevices[DEVICE_DEST] = NULL;"),
    ("the right picker unmounts DEVICE_DEST", SWISS,
     "\tdevices[DEVICE_DEST] = NULL;\n\t/* The picker", "\t/* The picker"),
    ("the right picker takes the A that chose it", SWISS,
     "\twhile(padsButtonsHeld() & PAD_BUTTON_A) VIDEO_WaitVSync();\n\tselect_device(DEVICE_DEST);",
     "\tselect_device(DEVICE_DEST);"),
    ("the right picker keeps the pane in DEVICE_DEST", SWISS,
     "\tdevices[DEVICE_DEST] = dest;\n\tUIScene_Request(filesScene);", "\tUIScene_Request(filesScene);"),
    ("the right picker leaves the Library's scene", SWISS,
     "\tdevices[DEVICE_DEST] = dest;\n\tUIScene_Request(filesScene);", "\tdevices[DEVICE_DEST] = dest;"),
    ("the pane unmounts the Source's storage", SWISS,
     "\tif(filesOther.mount == UI_FILES_OWN) {\n\t\tfilesOther.device->deinit(",
     "\tif(filesOther.mount != UI_FILES_UNMOUNTED) {\n\t\tfilesOther.device->deinit("),
    ("a failed read isn't the mount check", SWISS,
     "\t\tif(filesOtherMount()) {\n\t\t\tfilesOther.read = device->readDir(",
     "\t\tif(false) {\n\t\t\tfilesOther.read = device->readDir("),
    ("no reset function", SWISS,
     "\tSYS_RegisterResetFunc(&filesOtherResetInfo);\n", "\n"),
    ("the reset runs before the interface stops", SWISS,
     "{NULL, NULL}, filesOtherOnReset, 1\n", "{NULL, NULL}, filesOtherOnReset, 0\n"),
    ("the reset unmounts the Source's storage", SWISS,
     "\tif(!final && filesOther.mount == UI_FILES_OWN && filesOther.device != NULL &&",
     "\tif(!final && filesOther.device != NULL &&"),
    ("the Source's slot isn't given back", SWISS,
     "\tmemcpy(entry, &curFile, sizeof(file_handle));\n\tdevices[DEVICE_CUR] = cur;\n",
     "\tmemcpy(entry, &curFile, sizeof(file_handle));\n"),
    ("Swiss's box unmounts the Source it borrowed from", SWISS,
     "\t\tif(devices[DEVICE_DEST] != devices[DEVICE_CUR] && devices[DEVICE_DEST] != manageKeep) {\n\t\t\tdevices[DEVICE_DEST]->deinit( devices[DEVICE_DEST]->initial );\t",
     "\t\tif(devices[DEVICE_DEST] != devices[DEVICE_CUR]) {\n\t\t\tdevices[DEVICE_DEST]->deinit( devices[DEVICE_DEST]->initial );\t"),
    ("a left box unmounts the right pane's storage", SWISS,
     "\tif(keep == NULL && filesOther.mount == UI_FILES_OWN) {\n\t\tkeep = filesOther.device;\n\t}\n", ""),
    ("the picker unmounts the kept storage", SWISS,
     "\tif(keep != NULL && devices[DEVICE_DEST] == keep) {\n\t\tdevices[DEVICE_DEST] = NULL;\n\t}\n", ""),
    ("manage_file doesn't know what to keep", SWISS,
     "\tmanageKeep = keep;\n", ""),
    ("the right box keeps nothing", SWISS,
     "filesManageFile(other ? cur : NULL);", "filesManageFile(NULL);"),
    ("A's box keeps nothing", SWISS,
     "\t\t\tneedsRefresh = filesManageFile(NULL) ? 1:0;\n\t\t}\n\t\tmemcpy(directory[curSelection]",
     "\t\t\tneedsRefresh = manage_file() ? 1:0;\n\t\t}\n\t\tmemcpy(directory[curSelection]"),
    ("choosing storage keeps the old one mounted", SWISS,
     "\tfilesOtherRelease();\n\tfilesOther.device = device;", "\tfilesOther.device = device;"),
    ("a clash reset keeps the old one mounted", SWISS,
     "source, NULL, 0)) {\n\t\tfilesOtherRelease();\n", "source, NULL, 0)) {\n"),
    ("the right pane on its own storage without File Management", SWISS,
     "UIFiles_RightOnConfig(fileManagementAllowed(),", "UIFiles_RightOnConfig(true,"),
    ("the picker's clash replaced silently", SWISS,
     "\t\tfilesNote = filesNoteText;\n\t\tchosen = NULL;\n", ""),
    ("A chooses a greyed storage", SWISS,
     "if((buttons & BUTTON_A) && !((filesSnapshot.menu.dim >> focus) & 1u)) {", "if(buttons & BUTTON_A) {"),
    ("L's Source isn't mounted", SWISS,
     "\tsourceCommit(device);\n\t(void)sourceMount();\n", "\tsourceCommit(device);\n"),
    ("L's choice does nothing", SWISS,
     "\t\t\t\t\tfilesSourceChange(filesMenuDevices[choice]);\n", ""),
    ("X at the top opens Swiss's Source picker", SWISS,
     "\t\t\telse if((buttons & PAD_BUTTON_X) && filesAtRoot(&curDir)) {\n\t\t\t\tstorage = UI_FILES_LEFT;\n\t\t\t}\n", ""),
    ("L still pages", SWISS,
     "\t\tif(page == UI_MENU_INPUT_UP) {", "\t\tif((buttons & BUTTON_L) || page == UI_MENU_INPUT_UP) {"),
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
