## 2026-10-07 — Swiss's old list styles removed

Sixth step of the two-pane File Browser: Swiss's three list renderers,
which nothing reached once the dispatch sent every folder outside the
Library to renderFileList, are deleted rather than kept as a fallback.

swiss.c: drawCurrentDevice, drawFiles, renderFileBrowser,
drawCurrentDeviceCarousel, drawFilesCarousel (already uncalled),
drawFilesFullwidth, renderFileFullwidth and gameflowSceneLayout (called
only from the Library branch, so its HORIZONTAL arm was dead; the dispatch
asks gameflowLayout()). renderFileCarousel is the Library alone: no
useGameflow; one `drawn` flag per frame (snapshot allocated, built and
taken) and `if(!drawn)` is the File Browser fallback with its re-scan, as
before; Y is always in waitButtons, posters always polled, B inside a folder
is gameflowInsideFolder() alone, filesActivate(directory, true) and
gameflowNavigateParent(true, ...). The two UIScene_RequestLibraryLayout
(HORIZONTAL) calls for the legacy carousel are gone. filesHome(void) is
`curMenuLocation = ON_OPTIONS` (no Swiss row to dim); filesManage drops its
cardArt flag (the Library is its only caller and always pauses CardArt).
filesActivate keeps its flag (false from renderFileList and
filesOpenDetail's fallback). swiss.h: renderFileBrowser and drawFiles
prototypes, FILES_PER_PAGE_FULLWIDTH. FILES_PER_PAGE (select_dest_dir) and
FILES_PER_PAGE_CAROUSEL (the Library's paging) stay; select_dest_dir,
select_recent_entry and Swiss's info_game are untouched.

FrameBufferMagic.c/.h: _DrawFileBrowserButton, DrawFileBrowserButton,
DrawFileBrowserButtonMeta, DrawFileCarouselEntry,
DrawUpdateFileBrowserButton, drawFileBrowserButtonEvent_t,
EV_FILEBROWSERBUTTON (enum, typeStrings name, dispose and draw cases),
bannerMaskTexObj, starTexObj and TEX_STAR. DrawVertScrollBar,
DrawSelectableButton, DrawEmptyBox and DrawTransparentBox stay
(select_dest_dir, the patch list, the args selector). IPLFontWrite.c/.h:
drawStringEllipsis and GetCharsThatFitInWidth (only the button used them).
images/: banner_mask, star_16 and the nine 96x32 type tags (dir, dol,
dolcli, elf, file, fpkg, gcm, mp3, tgc) leave the .scf files and the tree;
their GXTexObj globals stay, never loaded, because upstream filemeta.c still
points a file's meta at them (nothing reads fileTypeTexObj any more). The
region tags stay (info_game draws them).

Reachability: every removed name has no reference left in cube/swiss
(grep), and an ELF symbol diff against the previous head shows the
renderers were already dropped by --gc-sections; what goes from the link is
drawStringEllipsis, the two texture objects and the textures. DOL
5,532,080 -> 5,412,560 bytes (-119,520; text -6,752, data -112,812).

Settings: FileBrowserType, AppsBrowserType and GameBrowserType are still
read and written (round trip in test_settings_file.py, fileBrowserTypeStr
stays) and nothing reads them. Their rows left the pages earlier; now their
help strings (the Standard, Fullwidth and Carousel text), rowCycle cases
and toggle arms go too, with their three SET_*BROWSER_TYPE ids
(test_settings_views no longer lists them as dropped rows; nothing stores
an interface row's number), and audit_settings_semantics.sh drops the
three arms from its recorded base.
docs/examples/global.ini and SETTINGS.md's example set Library Layout=Grid
instead of GameBrowserType=Carousel.

Tests: audit_files_contract (the File Browser section now ends at
gameflowSnapshotGeneration; Swiss's list names absent from swiss.c and
FrameBufferMagic.c, their prototypes, font helper and images from the
headers and .scf files; no useGameflow in the Library; 3 more mutants, 122),
audit_gameflow_dispatch (gameflowLayout() in the dispatch; the Library's
calls as the Library; no legacy renderers), audit_home_hardware_followup
(no fullwidth consumer; the Library's stick follows gameflowLayout(), with a
mutant in place of the legacy one), audit_game_detail_safety,
test_browser_home_lifecycle (the File Browser's and the Library's B arms,
the same 400 returns), test_gameflow_folder_navigation, test_ui_color.
Host lanes plain, contracts, sanitized (clang here, gcc in the build image),
whitespace, source checks and the upstream check pass. Lab (CI's emulator
image and limits, two at a time, this DOL in a card zip of its own): all ten
CI routes pass: files (both boots, Detail and the launch; the frame-span
guard never saw Swiss's list), folders, smoke on DVD and on the GC Loader
with non-default settings, game on DVD PAL (and the AESND stop test), SD2SP2
and GC Loader in 40 pieces, virtual cards in 4:3 and wide, and the failing
card's save.

## 2026-10-07 — A game's Game Detail from the File Browser

Fifth step of the two-pane File Browser: A on a game in the left pane opens
Indigo's Game Detail, through the Library's own image path, and B comes
back to the same row.

swiss.c: renderFileList decides `detail` under the entry's lock (loads and
filesOpensDetail: IS_FILE, UIGameflowLibrary_IsGameImageName, the Source has
FEAT_BOOT_GCM), disposes its loading wheel and calls filesOpenDetail, then
returns to menu_loop, which calls it again on the same page (filePanel ==
filesPage: no opening, filesScene requested). filesOpenDetail: meta thread
stopped, filesOtherRelease, lock, gameflowFreshHandle, the disc header
(gameflowReadResolverHeader) as the eligibility proof, then
filesDetailSnapshot (a one-record Horizontal window: itemCount the listing's,
selectedIndex and libraryIndex curSelection, slot 0, gameflowSnapshotRecord
in IMAGE_FILES mode, gameId from the header), the Library's poster request
and one poll, DrawGameflow (not published), and gameflowLoadImageWithContext
with gameflowFromFiles set. In: gameflow_info_game's Detail mode and
load_game_with_context's Launch mode (Boot without prompts) go through
gameflowShowFromFiles, which, before the event was ever shown, sets the
mode with DrawSetGameflowModeNow (UI_MOTION_OFF) and DrawRepublish(page,
event); afterwards it is DrawSetGameflowMode as before. Out:
gameflowBackToFiles publishes DrawFilesSettled(&filesSnapshot) on top before
B's and the image loader's switch back to Library mode (once). The event is
disposed after, or DrawDiscard'ed when it never showed (a header mismatch);
then filesActivate (Swiss's load_file) when nothing was handled, and
UIScene_Request(filesScene). gameID_early_set is untouched: it still runs in
load_game_with_context before gameflow_info_game. gameflowPublishDetail adds
UI_GAMEFLOW_DETAIL_BACK_FILES, which ui_gameflow_detail.c draws as "B  BACK".
Recent and Autoload of a disc still go load_file → Swiss's info_game. WODE
keeps the Library's rule (no B in Detail there).

From the File Browser, Detail's Z goes through autoloadToggleConfirmed
before turning Autoload on, as Swiss's info_game asked there.
gameflowFindOppositeImage takes swissChoice (gameflowFromFiles): two
possible other discs no longer give none; the first that
UIGameflowResolver_NamedAsOppositeDisc names wins (meta_find_disc2's rule:
as long as the primary's name, the first case-insensitive difference equal
to the disc numbers'), else the last match, as Swiss chose. The info bar's
hint is A Start (kind OTHER) on a disc where filesOpensDetail is false (no
FEAT_BOOT_GCM, a `._` name). gameflowShowFromFiles keeps the page up, 60
vsyncs at most, until UIScene_Frame()->libraryReveal is 1: the Library
event draws nothing before the cube is behind it, and from a File Browser
over Home the Home cube showed for about 6 frames between the page and
Detail, which then faded in over it (a 60-a-second grab).

FrameBufferMagic.c: DrawSetGameflowModeNow (_GameflowSetMode shared with
DrawSetGameflowMode), DrawFilesSettled (seconds 60: past the opening),
DrawDiscard (frees an object never published). ui_files.c: the storage
menu's focus loop shifts an unsigned (gcc's -Wsign-conversion failed the
sanitized lane on the last step's CI run).

Tests: test_files_detail.py compiles filesDetailSnapshot and filesOpensDetail
out of swiss.c with FrameBufferMagic.c's _GameflowSnapshotValid and
gameflowSelectedRecord (first, middle, last of 2,000 rows; the header's ID
over a stale meta one; disc names on and off a booting Source), both lanes,
10 mutants; test_gameflow_detail testBackToFiles; audit_files_contract
check_detail (the lock, the release, header before posters, the flag, no
show from filesOpenDetail, in and out sites, the GameID before Detail, the
scene, the FBM pieces, the wait for the reveal, Z's question, the disc 2
choice, the hint), 28 more mutants (119). test_gameflow_resolver
testOppositeDiscNamedAsSwissDoes (disc 2 beside disc 1 over its NKit copy,
either way round, case aside; another first difference; no names).
Emulator: card.listing takes a folder; Emulator.burst grabs the screen at
60 frames a second from one ffmpeg (frame() starts a process each time,
about 0.2 s apart, which missed both the Home cube and a flying card). The
files route's second boot, while SD2SP2 still writes: files_detail (games/
on the GC Loader, DOWN to the probe's game, Flatten directory dropping
Racing.v1's row; A under an 8 s burst: DETAIL_CARD_BOX in each of the first
18 frames without the File Browser within 8.0 of the burst's last; B → the
same info-bar name and left rows, and the right pane's /backups rows), X
back up to games/; the failing copy; then files_launch (games/, A, A → the
probe with GPRE01).

Lab (CI's image; run.py starts the card zip's ipl.dol, so each build's DOL
goes into a copy of the zip): files route passes (both boots, the launch
reaches the probe). The first version's DOL fails the burst check (the
Home cube, cover 47 away), and so does the fly-in mutant
(DrawSetGameflowMode in gameflowShowFromFiles: cover 48 away for all 18
frames); an instrumented build waited 13 vsyncs for the reveal. DVD smoke
and PAL game routes passed on the first version; captures in 4:3 (GC
Loader, non-default settings) and widescreen.

After review: the first Detail after boot showed the banner, not the
poster, until it was opened again. filesOpenDetail's request opens the
pack, and cacheInit's invalidateAll quarantined every slot for 40 ms
(UI_ASSETS_EVICT_QUARANTINE_MS) although a new arena has never been drawn
from, so its one poll read nothing; and nothing polled while Detail was up.
The Library hid the first because its list polls every idle retrace before
A, but its Detail had the second: A within 40 ms of a slot being let go (a
quick scroll past 25 games, a source change) kept the banner too. Now
cacheInit stamps a new arena's slots with no quarantine (a re-Init over
drawn posters keeps it), and gameflow_info_game polls posters on its idle
retraces, one read at most, as the Library's list does; a late poster fades
in (_GameflowArrival). test_ui_assets test_fresh_arena_reads_at_once (2
failures without the fix); audit_files_contract pins the Detail poll, with a
mutant (135). Emulator: files_poster at the end of the files route's first
boot (no Detail before it): Astral Circuit's Detail, under an 8 s burst,
shows card.poster_light in DETAIL_CARD_BOX (POSTER_PIXELS 1000; the banner
has about 14, the poster about 2,600) within POSTER_FRAMES (30) of its first
frame. Lab: without the fix no frame of the 8 s has it; with only the
Detail poll it fades in 26 frames after Detail's first; with both it is on
the first. Files, DVD and GC Loader smoke and PAL game routes pass.

Second review: the idle poll put reads on Detail's menu thread (a poster,
43,648 B, and a still, 38,400 B, at most one each a retrace), and Detail
saw only held buttons, so a tap made and let go during a read was lost.
The Library's list and the File Browser already OR in padsButtonsTaken.
gameflow_info_game now passes padsButtonsHeld() | padsButtonsTaken(
detailButtons) to UIMenuAction_Update and drains the latched presses after
each UIMenuAction_Init (entry and after a modal), so a B that left Settings
isn't seen again; audit_game_detail_safety pins both, with 3 mutants. The
poll finishes every job already queued for the window, not only Detail's
poster: from the Library up to 8 neighbours and, in spotlight, 3 stills;
from the File Browser, stills a spotlight visit left pending. Each is one
bounded read on an idle retrace, Detail's own poster first unless its slot
is quarantined. Kept: it is what the Library's list would read next anyway.
files_poster now allows POSTER_FRAMES 2 (was 30, wall-clock frames of the
60 fps grab): with both fixes the poster is on Detail's first frame (41/41),
the poll-only build had it 26 frames on, which now fails, so the route
holds the fresh-arena fix too.

## 2026-10-07 — The File Browser's operations to the other side

Fourth step of the two-pane File Browser: Z opens Indigo's Actions box
beside the row, and Copy and Move go to the folder open in the other pane,
through Swiss's own file operations.

swiss.c: manage_file is manage_file_ex(MANAGE_ASK, NULL). manage_file_ex
(option, destDir) runs Swiss's Z box only for MANAGE_ASK (re-indented,
`int option = 0` now an assignment) and, with destDir, skips
select_device(DEVICE_DEST), the destination mount and select_dest_dir: the
caller has set and mounted DEVICE_DEST, and destFile->name is destDir. Both
blocks are pinned by hash (one tab in, trailing blanks off) in the contract
audit. The target name is manageDestName (stripInvalidChars, .gci off a
card), which the screen uses for the fit, the same-name lookup and the
ghost row. manageDropPartial (closeFile again, harmless, then the
destination's deleteFile) runs at the read failure, the write failure, the
final write or close failure, and on Stop (`cancelled &&`), before
free(destFile); Swiss's own callers get it too. Move's deleteFile result is
now kept: a failed delete says "Copied, but couldn't take it off ...", and a
Move without deleteFile reports as a copy. Presentation hooks, all behind
filesBoxes (set only around manage_file_ex in filesManageFrom, both Autoload
toggles and A on a firmware file): confirmAction asks filesAsk
(UIFiles_ParseQuestion; -1 falls back to Swiss's box); Delete asks
filesAskDelete with Swiss's own strings (duplicated, pinned twice); the
file-exists box is filesAskExists, whose choice becomes the button Swiss's
loop reads (A Keep both, Z Replace it, B Cancel), Keep both greyed when
only replacing makes room (filesFitsBoth); the progress card is
DrawProgressBarFiles; every result is manageTell / manageCopied →
filesSay (maroon message, 2 s or A/B, failures wait for A; the outcome
feeds the focus rule). manageLanded records where a copy landed after Keep
both. filesBox is the one box runner (storage menus now use it too): Up/
Down wrap, A or a letter chip chooses, greyed items take the focus and show
why, B or the close buttons shut it, chord = L held with A (A alone on
Delete does nothing; L, A and B let go after). filesActions builds
UIFiles_Availability from both sides (filesSide reads info() only on a
mounted side), the letters, line 1 (UIFiles_ActionLine) and line 2 (the
reason or the fit); runs only an enabled action; Copy asks "Copy to <x>?"
with the ghost row (UIFiles_LandingIndex, UIFiles_InsertGhost on the
snapshot, restored after); Move's ghost shows under Swiss's Move question.
filesManageFrom swaps the slots for a right-pane entry and restores both on
its one exit. Focus after (UIFiles_FocusAfter, by name, from the old
listing): the left pane's into curFile, the right's into focusName, the
other side on manageLanded; a right-entry action always writes the left's
own focus back. A on a file that doesn't start, and on a .fpkg off a
FlippyDrive (filesLoads), opens Actions. filesManageEntry is gone; the
legacy lists still use filesManage / filesManageFile. Two Swiss lines lost
trailing blanks (select_recent_entry, upToParent) for the whitespace gate.

ui_files.c/.h: UIFiles_ActionLine, UIFiles_Result (finished, stopped,
write/read failed, kept), UIFiles_InsertGhost, UI_FILES_ROW_GHOST;
uiFilesMenu_t gains row, rose and letter[]; uiFilesStorageMenu_t gains warn
(a storage menu's = dim); the snapshot carries message[2], its serial and
measured width. FrameBufferMagic.c: _FilesMenu beside any row, letter chips
(flat, no sqrtf), Delete's rose edge; ghost rows outlined and pulsing
(0.84 + 0.16 sin 3t, steady with less motion); _FilesMessage (one or two
lines, in over 0.10 s); the progress card's files flag (scrim, B Stop).

Tests: test_ui_files testAvailabilityTable (manage_file_ex's six permission
lines copied verbatim, held equal to swiss.c's by the audit; every device
and entry: Rename/Hide/Delete exactly Swiss's, Copy/Move only where Swiss
allows them, a Move only where Swiss renames or deletes the original),
testActionWords, testGhost, the storage menu's warn bits;
audit_files_contract 91 mutants (the four drop sites, a finished copy
deleted, the drop on the Source, Swiss's box and picker hashes, the preset
folder, the name built twice, a greyed action run, Move offered without
delete, a failed delete silent, the chord and its release, Delete's and
Move's words, filesBoxes left on, Keep both offered, the slots); the waits
audit names manage_file_ex; test_ui_color keeps the rose edge and the
message's second line. Frame budget: files-actions 9,800 vertices, 116
sqrtf; files-copy 12,168; files-message 9,644 (with a flashing row), 87
sqrtf; the five earlier scenes unchanged. Emulator: files_box (a box's top and bottom edges
across the right of a pane, not a row's outline), the Z step now reads it;
a new route `files` (GC Loader Source, card.second_card in SD2SP2): copy
(byte for byte on the second image), Keep both, Move right to left (gone,
whole on the GC Loader), Rename via the keyboard, Delete (A alone deletes
nothing, L+A does), and a 160 MB copy stopped after 0.6 s (nothing left on
the GC Loader); a CI matrix job runs it. Docs: system.md (Copy, move,
rename, hide and delete, two pictures), controls.md, CHANGELOG.

Review follow-up, before merging. Memory cards: a card's write strips the
GCI header and returns length-64, and the trailing writeFile(NULL, 0) on a
plain file asks for another block (-11), so card copies reported failure
and the cleanup's delete (by the save's internal name) removed the finished
save; Keep both onto a card wrote over the save in place. Now
manageDropPartial never deletes on a card (Swiss's own copy keeps what it
wrote, as before) and UIFiles_Availability greys Copy and Move onto a card
("Use Memory Cards to copy saves."; uiFilesDevice_t.card). manageDropPartial
also leaves a same-named folder alone (statFile says IS_DIR), and the box
greys Copy and Move when the landing name is a folder there
(uiFilesEntry_t.existsFolder). Replace it now checks deleteFile and cancels
with "Couldn't replace it." when the old file stays; a replacing copy that
stops or fails says the old file is gone (UIFiles_Result's replaced). The
result message is kept (filesSayLater) and said by renderFileList after
both panes are read again (filesSayPending; at once when nothing changed),
so it sits over the new state; a Copy's landed row flashes under it
(UI_FILES_ROW_FLASH, UIFiles_Flash: two pulses Full, one Reduced, none
Off) and the message fades out over 0.15 s (messageLeaving). The ghost row
is a light lilac fill with a 2 px white edge, unlike the focus outline; the
File Browser's boxes and message are opaque. Box titles are cut in the
middle to the pane (filesFitAt, title is UI_FILES_TEXT_CAPACITY,
UIFiles_MenuBox caps the width at the pane less 12) and message lines to
560 px. The audit pins manageDropPartial's body, the card and folder guards,
the stopped Move's `if(!cancelled)`, Replace's checked delete, the
destination argument (thereDir) and the kept message, each with a mutant.
The files route adds a stopped Move (original whole on the SD card,
nothing on the GC Loader), a copy into backups/ on the right, a progress
check before each B (the red B glyph, files_progress) and a stop message
unlike the finished one; then it boots again with the SD2SP2 card failing
writes to every cluster it had free (card.free_sectors) and copies into
backups/: the failure waits for A and no whole copy lands. On that
emulated card every write after the first failed one fails too (deleting
another file fails), so the unfinished file stays and the message says part
of it is left; deleting where the card allows rests on the audit.

Lab (CI's emulator image, one Dolphin at a time): the files route, both
boots, passes; earlier the DVD smoke and the GC Loader smoke (non-default
settings) routes passed with the Actions box.

## 2026-10-07 — The File Browser's second device

Third step of the two-pane File Browser: the right pane can hold a storage
of its own, and L and R choose each side's, as in Memory Cards.

swiss.c: filesPane_t gains mount (uiFilesMount_t), readFailed, status and
its own free text; its device is kept for the session and never stored in
devices[] (Settings' Load at startup unmounts whatever DEVICE_DEST held).
filesOtherFree drops the listing; filesOtherRelease drops it and deinits a
mount of the pane's own; filesOtherMount is manage_file's mount block
(deinit, stats off, init, stats on), SHARED when the device is the Source;
filesOtherRead's failed read on its own mount mounts once more before it
reports (the read is the mount check), then a gone folder falls back to
the top. filesOtherAcquire picks the first device (UIFiles_RightOnConfig:
the Configuration Device when set, detected and not the Source; the Source
without File Management), drops a device that clashes with a new Source,
and notices a Source change under a shared pane. Release points:
menu_loop's Library branch and empty-folder branch (from the last step),
starting a file (A on a loadable left file), both Autoload toggles (then
acquire again), START when Recent is on, the left Other devices...,
filesSourceChange, filesSwapSides, filesOtherPick, B, and the end of
renderFileList on any exit or Source change. A constructor registers
filesOtherResetInfo (priority 1, as deviceHandler.c's): on reset it deinits
an OWN mount without QUIRK_NO_DEINIT. sourceCommit (select_device_internal's
DEVICE_CUR tail; EXI_ProbeReset now runs after the slot is set, which reads
nothing of it) and sourceMount (menu_loop's confirmed-source block) are
pulled out unchanged and used for L's in-place Source change and Y.
filesStorageMenu lists the detected readable devices (allDevices order, at
most six) and Other devices...; Up/Down wrap, A chooses (greyed bumps), B,
L or R close; the info bar shows the focused device (filesMenuInfo). R's
Other devices... is filesOtherPick: release, DEVICE_DEST emptied, the A
that chose it released (the picker reads held buttons; without the wait it
took the A and chose its first device), select_device(DEVICE_DEST), the
choice taken, the slot and filesScene restored. X or A on ".." at a pane's
top (and A or X on an empty right pane that failed to mount) open that
side's menu; with no entry there the hints are "A Choose storage" and B
Home (UI_FILES_HINTS_STORAGE) and the info bar names the device. While a
menu is open, an item's second sentence goes to the info bar's second line
when it has no reason to show there. A memory card's or Qoob's free space
is in blocks (filesBlockSize, shared with the rows). renderFileList now drops stale presses after acquiring the
right pane, just before the page is published: a press made while a disc
drive reset for the pane (Y onto the disc) was otherwise taken as the next
action's. Y is
filesSwapSides, gated by UIFiles_CanSwap with "<Device> isn't ready, so the
sides can't swap." Z on a right entry on another device swaps DEVICE_CUR to
the pane's device around manage_file (filesManageEntry) and sets manageKeep
to the Source; manage_file's destination mount and its cancel unmount skip
manageKeep, so choosing the Source as the destination neither remounts nor
(on B in select_dest_dir) unmounts it. Every box in the screen goes through
filesManageFile(keep): keep is the Source for a right entry on another
device, else the right pane's own (OWN) mount, so a left Z or A on a file
that can't start never deinits the pane's device (choosing it as the
destination, B in the folder chooser, a failed init, or the picker's
"deinit the old destination" on a DEVICE_DEST a finished copy left
holding it). A DEVICE_DEST holding what is kept is emptied during the
call. filesOtherPick checks the picker's choice against the Source
(filesClash now gives the reason): a clash keeps the pane's device and the
info bar says why, as the picker greys nothing. Paging is the C-stick's only. Hints keep Y Swap
sides; the right pane's game note says "Y swaps the two sides."

ui_files.c/.h: UIFiles_RightOnConfig, UIFiles_NotReady (three message
lines: "<Device> isn't ready.", the status in brackets or "Couldn't read
<folder>.", "Press R to choose storage."), UIFiles_MenuMotion (0.92x, 0.97x
Reduced, 0.08 s in, 0.10 s out), uiFilesMenu_t and UIFiles_StorageMenu
(titles, items, greyed clashes with their reasons, what choosing does,
focus). The pane snapshot's message has three lines; the snapshot carries
the menu. FrameBufferMagic.c: _FilesMenu (Memory Cards' box look, any
count up to nine, placed by UIFiles_MenuBox below the pane's button),
drawn after the words with its own spring and open/close clock;
_SaveCubesMenu untouched.

Tests: test_ui_files testSecondDevice and the storage hints;
audit_files_contract (54 mutants: every release point, including choosing
storage and a clash reset, the reset hook's registration, priority and OWN
guard, the mount check, the picker's slot, A release, scene and clash
note, the slot swap's restore, what filesManageFile keeps and the slot it
empties, the first device without File Management, a greyed item, L's
Source mounted, X at the top, L not paging); frame budget files-storage (4:3,
left menu) and files-storage-right (wide, right menu, a pane not ready):
10,184 and 9,748 vertices, 116 sqrtf; the button words moved files,
files-right and files-wide by 96 vertices. Emulator: files_menu (title and
items boxes' top edges, 4:3), files_storage on every route with cards (R
and B; R, Memory Card - Slot A and A; RIGHT and DOWN browse, read by the
info bar's name since Jet Black's focus bar isn't text-bright; Y swaps, read
by the Source's name moving to the right pane, and Y swaps back; L and B),
files_not_ready on the DVD route (UP twice to Other devices..., Z, RIGHT
twice to KunaiGC: the pane says so, Y bumps, R puts the disc back); two
Dolphin fixtures. Docs (system.md storage section and picture, controls.md) and
CHANGELOG. No upstream file changed.

Lab, beyond the routes: Z on a Slot A save in the right pane beside a GC
Loader Source, X Copy, the GC Loader: CloudBakeryData.gci on the SD image
with the save's blocks; Move of that file from the left to Slot B: the GCI
folder gained it and the SD image lost it. With an SD2SP2 card beside the
GC Loader Source on the right: a left Copy to it cancelled in the folder
chooser, a Copy to it, then a Copy to the Source cancelled; after each the
right pane's Delete removed a file from the SD2SP2 image (before the fix
both cancels left it unmounted and Delete failed). Other devices... on the
right, FlippyDrive Flash beside the GC Loader: the pane stays and the info
bar says "GC Loader is open on the other side." Not measured: the free heap with
two 2,000-entry folders (a console job).

## 2026-10-07 — The File Browser's two panes in place of Swiss's lists

Second step of the two-pane File Browser: the screen, everywhere outside the
Library. menu_loop's dispatch is now the Library wherever
gameflowLibraryMode says so, else renderFileList; File Browser Type is no
longer read, and the scene requests moved into the Library's branch (the
File Browser asks for none, so Memory Cards' backdrop hands the cube over
where Home or the Library left it; it records which and asks for it again
on a return, after Swiss's game screen turned it). The legacy renderers
stay compiled and unreachable until they are deleted; renderFileCarousel's
fallback (no memory for its snapshot) now sets gameflowListFallback, puts
the focused name in curFile and asks for a fresh scan, since
gameflowLibraryEntries has reordered the listing; the flag lasts one
renderFileList call.

FrameBufferMagic.c: EV_FILES (appended, so EV_SAVE_DETAILS keeps its
number), DrawFiles / DrawUpdateFiles / DrawUpdateFilesReading, _FilesShapes
(boxes, buttons, focus bars, cubes, tracks), _FilesWords, _FilesCube (flat
quads only: no sqrtf, no trig, no texture), and _FrameCovered's EV_FILES.
The page draws _SaveCubesBackdrop with UIFiles_Stage's paper and handover
(Memory Cards' timings, new in ui_files.c with LeaveSeconds); B waits for
the page to leave before Home is published. The loading wheel has a
PROGRESS_BOX_FILES position in the info bar's corner (the word stays: the
clock-corner test pins that line). Colours that keep their hue (teal,
amber, pink, the bolt and the program folder's mark) are in test_ui_color's
SEMANTIC; the text cube's paper is a neutral.

swiss.c: renderFileList on the shared listing. Every string is fitted on
the menu thread into a uiFilesSnapshot_t (widths measured there too, so
the draw measures nothing). Left rows lock their entry; the idle refresh
(four times a second, for the meta thread's banners) trylocks and gives up
on a busy entry. The wait loop populates one visible left row a frame on
every device (filesMetaStep, reset per window and per listing, skipping
rows already read), as drawFiles read its visible rows: without
FEAT_THREAD_SAFE it is the only reader, and with a meta thread it is the
caller that lets meta_alloc evict rows out of view once the 512-entry pool
is full (the meta thread only kills itself there). A populates the focused
entry first, so a program folder or a second disc is known before it acts.
current_view_start/end come only from UIFiles_LeftView. Input acts on
press edges (padsButtonsTaken) plus its own Up/Down repeat (320/120 ms) and
two UIMenuInput polls, the stick (rows) and the C-stick (pages,
padsSubMenuInputPoll, new in input.c); presses are dropped on opening and
after a box, kept across a left folder change (filesKeepPresses), and
dropped by menu_loop's device-change block, since the Source picker reads
held buttons and leaves its A or B in the latch. X at the top waits for
its release before the picker, whose X toggles the EXI-speed mode. X is
PAD_BUTTON_X alone: BUTTON_X includes C-stick Right/Down. The right pane
(filesOther) is readDir + sortFiles on the Source, at its own folder, with
".." left out at the device's top (no storage to choose there yet); it is
read again when the Source changes, and released when the screen is left,
when menu_loop goes to the Library and before the empty-folder branch
deinits the Source. A, X, Z, START, B and the clap go through the shared
list-action helpers; Z on a right entry or a program folder goes through
filesManageEntry (today's Z menu), which writes the left focus name back
into curFile; filesToggleAutoload takes its folder. A change made from
either pane marks the right listing unread, so both are read again; a
right-pane change to the left's folder or one above it (filesWithin) moves
the left pane to the right pane's folder first, rather than letting
scanFiles fail and unmount the Source. The right pane goes up a folder by
the ".." entry's fileBase (a disc's readDir finds a folder by it), or the
Source's initial at the top. L and R page. DrawUpdateFilesReading(page,
pane) runs before menu_loop's scanFiles (left) and the right pane's folder
reads; the band hides the row words under it as it does their cubes. The
device name above each pane is fitted (UIFiles_FitDevice: 0.92 down to
0.64, then cut at its end) into what the SOURCE chip and the free box
leave, so "Memory Card - Slot A" no longer runs into the box.

Settings: File Browser Type's three rows left libraryRows[] (ids,
tooltips, cases, toggle arms and keys stay; UI_SETLAYOUT_ROWS_LIBRARY 11);
UIGameflowLibrary_SelectBrowser deleted with its tests.

Tests: audit_files_contract.py (new, 17 mutants); audit_gameflow_dispatch
(new dispatch shape, renderFileList among the latched waits, filesMetaStep
as I/O); audit_home_hardware_followup (renderFileList's two polls, the
C-stick port reads); test_browser_home_lifecycle (renderFileList's B arm,
EV_FILES covers the frame); test_gameflow_folder_navigation (renderFileList
stub, the fallback, releases); test_settings_views, test_ui_settings_layout
(rows), test_ui_files (UIFiles_Stage, UIFiles_FitDevice with Swiss's long
device names in both shapes), frame budget scenes files, files-right,
files-wide. The budget counts the words too, through a font stand-in (11 px
a character, a quad a character a pass, two passes) with Redump-length
names fitted into every row, and the hint line's buttons: 9,604 / 9,628 /
12,796 vertices and 232 sqrtf. That is above the first 6,500 estimate,
which assumed about 26 characters a row; drawing the inactive pane's
words in a single pass waits for a console measurement. Emulator: files_screen and active_pane replace the legacy
detector in the routes (it stays as the folder-transition guard);
file_browser checks both panes' focus, DOWN and A on a right folder and X
back to the same rows, a left folder change and X back, Z opening Swiss's
box over the rows (by the text in its band) and B closing it, and B back to
System's same label; files_text reads each pane's path line, the left one
from the capture in Menu Widescreen; files_face and
library_after_file_browser use the new detector; four Dolphin fixtures. Docs and CHANGELOG; docs/guide/images/files.png replaces
library-file-list.png. No upstream file changed.

Validation: host plain and contracts lanes; whitespace, source and
upstream checks; a preview DOL through the Emulator DVD smoke route, the GC
Loader smoke route and the folders route.

## 2026-10-07 — The two-pane File Browser's model

First step of the two-pane File Browser: gui/ui_files.c and .h, pure C
with no libogc, so the host tests build it. Nothing calls it yet; swiss.c
and FrameBufferMagic.c are unchanged, and nothing a player sees changes.

It holds: the two panes' focus (Up/Down wrap, pages stop at the ends,
Left/Right cross and each pane keeps its row, an eight-row window with a
row of margin, and the left window as current_view_start/end whichever
pane is active); UIFiles_Layout for 4:3 and Menu Widescreen (outer edges
at the stage's edge plus 40, inner edges fixed at 312 and 328); row kinds
from the extension; UIFiles_IsProgramFolder and the folder a rewritten
entry stands for; ".." row names; sizes as formatBytes writes them (a
port, since util.c needs libogc), blocks on a card or a Qoob; the middle
cut of real names keeping only the extension and a "(Disc N)" before it,
never inside a UTF-8 sequence, and the path line cut from the left at a
folder; the hint lines; UIFiles_Availability with every reason, the fit
(unknown free space never greys; replace-only when only replacing makes
room); UIFiles_FreeKnown, UIFiles_StorageClash with its reason,
UIFiles_CanSwap, UIFiles_ExistsChoices, UIFiles_MenuBox,
UIFiles_LandingIndex (fileComparator's order), the focus after an action,
and confirmAction's texts read into a title, a line and two items.

Choices made here: a card's or a Qoob's row size is "N
blocks" (Swiss's own text, "56 KiB (7 blocks)", is too wide for the size
column); Move's folder reason says "moved"; Move counts as possible when
the device renames across folders (Swiss's same-device Move) or deletes
the original after copying, so it never ends as a copy; the fit reason
puts both amounts in the larger one's unit to three figures ("0.80 GB
free; this needs 1.35 GB"); the 112..336 window holds 7 items with a
title or 8 without (UI_FILES_MENU_ITEMS), so a storage menu lists at most
6 devices (UI_FILES_STORAGE_DEVICES) and "Other devices..." reaches the
rest; a cut keeps a tail only with at least 6 characters of the start
before it (UI_FILES_NAME_MIN_HEAD), else the extension alone, so a
narrow column never shows a bare "(Disc 1).rvz"; replace-only is per
action, never set for a same-device Move (a rename); two network
devices always clash, even before the adapter's slot is known.

Tests: test_ui_files.c in TESTS and the Makefile, both lanes. The pane
reducer is checked against an independent model over every input
sequence of five on lists of 0 to 25 entries, with the window's rules
(inside the list, the margin, moving only when it must) and the left
window after every step. Layout to the pixel in 4:3, and the wide
stage's edges; long Redump-style names at both shapes' name widths with
and without a size, cut to the longest start that fits, with region and
language tags right before the extension cut too; the shrink at the
largest scale that fits and at both scale boundaries; every
availability reason, in its order of priority where two apply, and the
fit at exactly the free space and, over 4,096 combinations, never more than
manage_file's own canCopy/canMove/canRename/canHide/canDelete; every
storage pair; landing against a scan of a qsorted
listing; delete and rename at the list's edges through a re-read and the
reducer; every confirmAction text in swiss.c byte for byte. Hint lines
parse through ui_hint.c with at most five round buttons in every
context. No upstream file changed.

Validation: host plain, sanitized and contracts lanes; whitespace,
source and upstream checks.

## 2026-10-07 — One copy of the file list's actions

Behaviour-free groundwork for a new file list. renderFileBrowser,
renderFileFullwidth and renderFileCarousel each had their own copy of A,
X, Z (with Autoload on ".."), START, B and the Bongo clap. They now call
filesActivate, filesUp, filesManage (and filesToggleAutoload), filesRecent,
filesHome and filesBarrelGame in swiss.c, in the same order with the same
conditions, locks and returns. The carousel's differences are parameters:
useGameflow (".." through gameflowNavigateParent; no Swiss row to dim on
B) and cardArt (CardArt_Pause after meta_thread_stop on Z and START).
gameflowNavigateParent moved up beside them and ends in filesUp, which is
the old tail. Each helper reads directory[curSelection] as the loops did.

Tests that pinned the copies' text now pin the helpers:
audit_gameflow_dispatch (the carousel's legacy A, its three parent routes,
B inside a folder before Home), test_browser_home_lifecycle (B arms with
filesHome compiled in), test_gameflow_folder_navigation (".." taken from
filesActivate; filesUp and filesHome in the harness; older revisions still
replay), test_settings_file (three config_update_autoload calls, each
with its box). No upstream file changed.

Validation: host plain and contracts lanes, source checks, the whitespace
and upstream checks; a preview DOL through the Emulator smoke routes (DVD
and GC Loader) and the folders route, built from this commit and its
parent: the same 155, 158 and 299 checks pass on both, none fail.

## 2026-10-06 — Take latched presses around every device read; whole presses in the emulator test

Follow-up to the Library and Apps lost-press fix. A survey of every
menu-thread loop that reads padsButtonsHeld(): Home, Settings
(settingsWaitForInput and its boxes), the retained Detail, the cheat
browser, System info, the legacy lists (renderFileFullwidth, Recent,
select_dest_dir, select_alt_dol) and the confirm boxes read once per VSync
with nothing else between reads, so held buttons see any press of a frame or
more. I/O there runs between waits, where the per-wait clear drops a press
anyway. Memory Cards' boxes (save details, Actions, storage, folder page)
already read through inputNext, which takes the latch. The loops that read
held buttons around I/O were manage_file's copy and verify_game, a chunk
(up to 256 KB and 512 KB) per pass with B to cancel: both now take
padsButtonsTaken(BUTTON_B) too, after dropping an older B.

audit_gameflow_dispatch.py's check_pollers became check_waits: it finds
every loop in Indigo's own sources (check_upstream.OWN) whose own body reads
held buttons and does I/O (CardArt_Poll, artLoad, readFile, writeFile,
readDir, populate_meta), and requires a padsButtonsTaken read in it, no
clear inside it, and a clear of the same mask right before it (inputNext's
is inputInit, which audit_saves_safety checks). 15 mutants per run: each
loop reading held only, clearing inside, or keeping old presses, and a
Settings wait that starts polling card art.

The CI failure that started this (run 37541764782, GC Loader smoke, "B
closes the box") was not the console: the box reads the latch, the PC
samples show the idle thread for all ten seconds after B with no card read,
and libogc2's PAD_ScanPads keeps an edge across reads that fail. B never
reached the console. Deadline took its start from Dolphin's last TICKS
report, which is up to 0.1 s old, so press() could let go about 20 ms after
holding when it followed a screen grab (the step before reads the screen
twice and saves a picture). press() now uses fresh waits: they count from
the first report after the wait began, so a press and the release after it
each last at least their seconds of console time, at up to 0.1 s more per
press.

Validation: host plain and contracts lanes, the emulator harness tests
(new test_a_fresh_wait_counts_from_a_report_after_it_began),
check_whitespace.sh origin/beta, source_checks.sh. The audit fails on the
copy loop as it was. Not run on a console.

## 2026-10-07 — File Browser as a face of the cube

UI_HOME_FACE_FILES after Emulators, on no side until Settings gives it one
(Up, Left, Right or Down Face › File Browser); always there once placed, as
Memory Cards is. A on it and System's row both go through ui_home.c's
openFiles(): UI_HOME_EFFECT_OPEN_FILES with a source, CHANGE_SOURCE without
(hint A OPEN / A SELECT SOURCE, as Library). Its icon is Source's folder
(drawFolderIcon) at `UI_HOME_FACE_FILES * UI_HOME_ICON_CHOICES`. B from
Swiss's list needs no new code: OPEN_FILES never touches homeState, so Home
comes back on the face that opened the list (audit pins that).

Eight faces: every per-face literal in the stroke stream (including the
zero-filled icon arrays that would have drawn face 7's choice 0 on an
uninitialised basis), frame budget, the scene's faceLift/facePitch; the
motif state still fits 640 bytes (636). The scene, motif, reducer oracle and
render pose tests put File Browser on each side, Classic and Infinite, with
and without a source. The Dolphin route's Down Face flip now takes four
RIGHTs from Apps to None, then one LEFT to File Browser: RIGHT from System
turns to it, A shows Swiss's list, B returns to it; three LEFTs restore Apps.

## 2026-10-06 — Choose the save copy a game starts with

Left and Right on a game's details step through the copies the Saves box
counts. Saves_CollectGameStats now also lists where each is (Slot A or B, a
Save Folder file, or a card image and its ordinal; the first eight), and
Detail reads the cards once per open (context->savesScanned): Left and Right
republish without reading them again. The box shows "Copy N of M | where"
and In use or Loads at launch; its label becomes « SAVES ».

A on Launch Game with a copy that isn't on the card the game reads asks
first (A LOAD, B KEEP). Saves_LoadCopy then writes the card's own copy of
that save to the Save Folder (read back), removes it, writes the chosen copy
(cardWrite reads it back) and, if that fails, puts the card's own copy back
(cardReplace); Detail stays open on B or any failure. The card is the first
slot holding a copy (Slot A first), else the first with a card. Not offered
while Emulate Memory Card is on.

Validation: test_saves_stats (copy places), test_saves_card_io (replace:
success, write fails, read-back fails, restore fails, no own copy),
test_gameflow_detail, test_launch_gx_stream (the choice inside the inset),
audit_game_detail_safety (asked before the load, the load before the launch
screen, Emulate Memory Card), audit_saves_safety (kept before replaced, put
back on failure), both host lanes; end to end in Dolphin with a GC Loader
card and a GCI card in Slot A: RIGHT, A, A left Slot A holding the image's
copy and the Save Folder the card's own. With a MemCard PRO emulated in
Slot A (the issue #102 lab patch, GameID on): the game ID goes out before
Detail opens (load_game_with_context), so on the first open after a switch
Detail's read of Slot A is cut off by the switch and the box misses that
card's copies (only the image's: no box); on a reopen the card is in already,
Detail reads the game's own card, and RIGHT, A, A put the image's copy on it,
its own copy in the Save Folder, the startup card untouched. That first-open
read is issue #102's open question, not this change's. Known:
game-details-saves-after-copy.png shows the old label.

## 2026-10-06 — Show the details screen's Saves box for two or more copies

UIGameflowDetail_Build sets UI_GAMEFLOW_DETAIL_HAS_SAVES only for two or
more save copies; with fewer, Detail is drawn in its v2.2 places (no inset,
rows 20 px higher, panel 328 tall) from one layout table. Settings › Setup ›
Library › Saves on Details (on by default; `Hide Saves on Details` in
global.ini, a hide flag so zero is the default) turns the box off, and then
gameflowPublishDetail never calls Saves_CollectGameStats: opening a game's
details touches neither memory card slot.

Why (issue #102): with a MemCard PRO GC the box reads the cards straight
after Indigo sends the game ID, so it can count the previous game's virtual
card, and for one copy it says nothing useful; the reporter asked for a way
to hide it. Whether that read also stops the switch is unconfirmed on
hardware; the GameID send itself is upstream's code at the same point as in
Swiss.

Validation: host plain and contracts lanes; GX stream tests pin both
layouts and the focus frame's rows; detail_frame_budget re-baselined (124
vertices without the inset); the emulator's virtual-cards route checks no
box with one copy and the box with two (both virtual-cards jobs);
non-default.ini seeds `Hide Saves on Details=Yes` so the key survives saves.

## 2026-10-06 — GC Loader starts Indigo at power-on as boot.dol

Stock Swiss auto-starts `*/boot.dol` from the device it booted from (util.c
autoboot_dols; before r1730 any name ending in `/boot.dol`), when the device
has FEAT_AUTOLOAD_DOL, before it draws anything; no setting turns it off.
Indigo's DOL carries no Swiss commit trailer, so it qualifies. Console test
2026-10-06 on Spencer's GC Loader: stock Swiss r2119 as `boot.iso` (15 pieces)
and the v2.3.1-rc.2 `ipl.dol` as `/boot.dol` started straight into Indigo with
no Swiss screen. The guide gains "GC Loader, at power-on" as its default for
GC Loader; "from Swiss" stays. No Indigo `boot.iso` is shipped: GC Loader can't
boot one in more than 40 pieces, and new copies on macOS 27 often are.

## 2026-10-05 — Install in four steps, copying two files

The install guide now asks what starts the GameCube and gives four steps per
setup: GC Loader; PicoBoot or PicoLoader at power-on or from Swiss; FlippyDrive
at power-on or from Swiss; and something else, which works everywhere. Each
setup shows the card afterwards as a text tree, which norvitech.com draws as a
list; the site's two questions show one setup's section, keyed on these
headings (`ROUTES` in its `build_indigo.py`), so renaming a heading needs the
site changed with it.

Step 3 copies `ipl.dol` and `swiss/patches/apploader.img` instead of merging
the whole `swiss` folder. Finder offers Merge only when one folder holds items
the other lacks; with an existing `apploader.img` (every update) it offers only
Stop or Replace, and Replace deletes settings and saves. A sourced fact check
corrected the PicoBoot test (hold D-Pad Down with the card in and a wired
pad: gekkoboot's text screen means the chip reads `ipl.dol`), FlippyDrive's
menu versus boot mode, GC Loader's device name in Swiss, and where Apploader
reads `apploader.img` (copy it to every card used with Swiss).

Swiss creates its `swiss` folder hidden (`ensure_path(..., "swiss", ..., true)`
in files.c), so every backup step says how to show hidden folders, and the
poster and cheat packs are now copied file by file (README, posters.md,
cheats.md, the ZIP readme) for the same Finder Replace reason. The guide has
one whole-card map after the setups (where posters, stills, descriptions,
cheats, apps and folder pictures go), which the site draws under each setup
with that setup's own files merged in.

Documentation and package text only; the console source tree matches v2.3.0.

## 2026-10-06 — Memory Cards and Emulators as faces of the cube

Two faces after Apps, on no side until Settings gives them one: Memory
Cards opens the screen System's row opens (the cube's handover is generic,
from the face in front), and Emulators is the Apps screen over `/emulators`
(apps.c keeps one screen per folder: its heading, empty-folder line and
remembered program). Emulators leaves the ring while the source has no
program there, as Apps does; Home reads `/emulators` only while it is on a
side. Each new face has one icon, a memory card and a game pad, at
`face * UI_HOME_ICON_CHOICES`; the choices between are padding that draws
nothing.

Seven faces moved every per-face literal in the GX harnesses (the stroke
stream, frame budget, render pose) and the motif state's size bound
(512 to 640 bytes, two maps and an opacity per face). The Dolphin route
takes three RIGHTs from Apps to None now, past the two new values.

## 2026-10-06 — Choose the menu on each side of the cube

Setup › Console gains Up Face, Left Face, Right Face and Down Face in place
of Apps Face: each None, Source, Settings, System or Apps, by default the
cube of before. Each side's choices start at its default and go round the
faces (UIHome_SideFace), so a zeroed setting is that cube and main.c keeps
upstream's defaults. Their arms step only their own field, so the picker's
step-to-target commit and its own-field rule hold; a face named twice keeps
its first side. `Hide Apps Face=Yes` in an older file
empties Apps' side unless the file names the sides, and is no longer
written. Y on Home opens Settings from the ring in either cube, and the
hint says so while Settings has no side. Away from Home, an Infinite cube
with other sides shows Library in front of its own ring instead of the
authored four; the default sides keep the authored background.

The Dolphin route's Console offsets moved: Down Face is eleven DOWNs
(RIGHT from Apps is None, LEFT back), Cube twelve. The seeded keys
(`non-default.ini`) leave the sides at their defaults, because the
Classic walk expects the default cube.

## 2026-10-06 — Lay the Home cube's faces out by side

Home's faces now sit on Library's four sides, Up, Left, Right and Down,
through one list instead of their enum numbers. Infinite's ring is Library
followed by each face on a side that is there now, in that order; Classic
turns each side's own direction to it. The default list, Source, Settings,
System and Apps, gives exactly the cubes of before: the existing reducer,
motif and scene suites pass unchanged, except fixtures that set the ring by
hand, which now set the whole layout.

The capabilities carry the sides (zeroed capabilities are the default cube),
the state carries them with the faces on a side that are not there now (Apps
without apps), and the scene publishes both in one word of its snapshot. A
face named twice keeps its first side. With no source, A on Library turns to
Source when it is a turn away and otherwise opens the source picker; Library
alone does not turn. The Classic side table that the reducer and the glyph
placement each kept is now derived from the one list.

Nothing in Settings names other sides yet. New tests: an independent layout
oracle over twelve layouts in both cube styles, glyph placement for custom
layouts, and the scene receiving a change of sides alone.

## 2026-10-05 — Verify Memory Cards identity and save captions in native builds

Integrated the corrected Memory Cards folder controls, removed their Library
placement, and refreshed the guide with actual synthetic-data captures.
Native default and widescreen routes pass 170 and 169 checks, including the
visible Y hint, full path and contained save captions, cube colors, save,
cancel, reset and a second process that restores the saved color. Authored
animated icon textures change on screen; static and absent artwork retain
their separate meanings. RAW images, source saves and exported payloads are
unchanged after the route. No retries or fatal log lines occur.

All four Library layouts pass 299 checks and 11,432 presented frames with
no legacy-browser frames. Y on a Library folder keeps normal navigation;
there is no folder path/color dialog. The native product source is unchanged
by the subsequent host-harness and documentation corrections. The frame
budget retains its existing twenty-scene ceilings.

The format supplies a last-update date, not a separate creation date. Saves
without declared captions receive bounded, validated identity fallbacks;
missing icon data is not replaced by invented game animation. Protected CI
on the final head and release-candidate packaging remain separate gates.

## 2026-10-05 — Put folder identity on Memory Cards mini cubes

Memory Cards now offers Y Folder only for its SD folders and RAW image
cubes. Its native page shows the full device-prefixed path, direct counts
and two contained save names prepared through the bounded metadata helpers.
RAW images stay read-only. A saves a path color, B cancels, Y restores Default
and Up/Down scrolls the complete path. Saved colors mark only folder cube
rims and glyphs, independently of Menu Color, in both screen shapes.

The settings namespace is Memory Card Folder Colors. A failed settings save
keeps the page open and restores the in-memory map; old Library colors are
not imported. Saves with explicitly absent art retain their readable identity,
and checked captions use the shared bounded text normalization.

Validation: folder/cube unit tests, settings fault rollback, production
controller sequences in plain and sanitized builds, and 18 GX renderer
regressions pass. The settings reference names the new key. Native captures
and integrated target/CI checks remain pending.

## 2026-10-05 — Remove folder identity controls from the game Library

Folder paths and identity colors belong to the folders shown as mini cubes
in Memory Cards. Removed the Library's Y Folder page, controller route and
color frames. Library folder navigation and artwork remain intact; games
retain Y Settings. The Library guide now describes navigation only.

The four-layout GX regression confirms that selected game folders offer A
Open without Y Folder or the explicit folder color frame. Memory Cards
folder identity controls and save caption validation are separate changes
to be integrated before the next release candidate.

## 2026-10-05 — Cover the Library controls beneath the folder page

A native capture exposed the bottoms of the Library button glyphs below
the folder page. Its panel now covers the complete underlying control row.
The prepared content and dialog controls retain their positions.

## 2026-10-05 — Keep metadata evidence tied to current captures

The value-row regression uses the four refreshed save-details pictures.
The older themed picture remains useful for general panel detection only.
Clarification of the folder entry below: a readable settings recovery copy
is accepted; unreadable settings or recovery metadata cannot be mistaken
for an absent file when saving folder colors.

## 2026-10-05 — Integrate save explanations, folder controls and installation guides

Save details shows the recorded last-update date and the save icon's status.
Animated, static and absent icons are distinguished, with clear messages for
paused or unavailable previews. No creation date is invented: the save format
has no separate field. Original animation decoding remains unchanged.

Y on a Library folder opens its full path and color controls. A saves, B
cancels and Y resets the color. Path identity includes its device prefix;
settings writes preserve other preferences and restore prior colors on failure.
The native folder route covers save, reopen, cancel and reset in all layouts.

The README, walkthrough and package/release instructions now give exact loader
filenames and card destinations, preservation, folder merging, first launch,
updates and recovery. Focused host and sanitizer regressions pass. Fresh native
save-details captures match the revised panel in both screen shapes; full
integrated protected checks and console acceptance remain required.

# Indigo development journal

## 2026-10-05 — Bound the folder settings sanitizer regression

The Linux GCC contracts lane reached the new folder settings fault test but
stalled before main: its PIE sanitizer executable recursively emitted
AddressSanitizer DEADLYSIGNAL on a host with high ASLR entropy. Match every
other sanitizer harness with Linux non-PIE flags, and bound compilation and
execution so a startup failure cannot consume the whole job timeout. The
production folder save code and UI are unchanged.

## 2026-10-05 — Read native folder color words at their own size

Fresh Dolphin proof showed that Right correctly changed Indigo to Azure,
but the folder route rejected Azure: its 50 bright pixels are below the
Library-title reader's 60-pixel minimum. Use a scoped bounded color-word
reader and stable word comparisons for selection, revisit, cancel and
reset. Other routes retain their existing title thresholds. Native Indigo
and Azure text crops reproduce the old rejection; the new regression also
rejects unchanged labels and a single changed frame between stale samples.

## 2026-10-05 — Give Library folders a path and persistent color

Y on a selected folder opens a retained identity page with the full
device-prefixed path, wrapping and scrolling without ellipses. Left/Right
preview the folder color, Y restores Default, A saves and B cancels. Explicit
color frames remain visible on custom posters in every Library layout and
keep their chosen color when Menu Color changes.

A bounded 32-entry map is saved under Library Folder Colors in global.ini.
Paths escape separators and control bytes; display controls use printable
hex escapes. Saving changes only that key, refuses unreadable settings and
recovery copies, and restores the exact in-memory map on failure. A proven
absent file is created with only this key. The settings fuzzer exercises the
same parser. The guide and settings reference describe controls and limits.

Validation: strict C and ASan/UBSan codec/path tests, production save fault
regressions, real folder navigation, 32 settings parser/writer tests and all
23 GX renderer tests pass. The target DOL compiles and links. The folders
Dolphin route now records save, revisit, cancel and reset for all layouts;
its native captures remain to be recorded by CI on the integrated build.

## 2026-10-04 — Prepare the validated candidate for v2.3.0

The maintainer tested the final release candidate on a GameCube and approved
promotion to stable. Named the v2.3.0 changelog and corrected the download
README to describe empty game folders with Library Folders enabled.
The accepted release candidate's product source remains unchanged.

The stable release is rebuilt from its tagged main commit through the normal
release workflow. Required promotion checks and signed package provenance
remain release gates.

## 2026-10-04 — Coordinate the remaining beta compatibility branches

Memory Cards and motion are merged into beta. Metadata filtering and manual
rewind remain pending their own pull requests. The rewind branch incorporates
the pending metadata branch so the ordered merges retain the same combined
implementation. The metadata branch keeps its focused production change.
The shared journal preserves both branches' earlier entries exactly. CI for
the refreshed heads remains pending.

## 2026-10-04 — Keep the reviewed beta compatibility fixes coherent

Included the metadata filtering branch alongside the accepted beta renderer
and manual rewind fix. Both compatibility changes retain their production
regressions, and every earlier journal entry is preserved. This prepares the
same combined implementation for the ordered beta merges.

## 2026-10-04 — Preserve current beta and metadata filtering together

Incorporated the accepted Memory Cards and motion changes from beta into the
metadata filtering branch. The renderer and shared launch/tab harnesses match
the tested combined implementation. Source filtering and its production
regressions are retained, with no new filename policy or product behavior.
Both branches' previous journal entries are preserved below.

The sanitized production visibility regression, shared launch and Gameflow GX
suites, Settings tab assertions and CI tool tests pass.

## 2026-10-04 — Update the shared tab harness for the current save interface

The incorporated motion harness still tried to extract the old Memory Cards
row renderer. The current interface uses save cubes, so the extractor failed
before running any assertions. The harness now matches the tested combined
implementation: Settings tabs retain their real spring and edge checks;
current save controls and geometry remain covered by the production controller
and save-cube GX suites. The product renderer is unchanged.

Beta incorporation preserves both features and all previous journal entries.
The focused shared-tab regression and ownership gate pass. The complete
contract lane and refreshed branch CI remain pending.

## 2026-10-04 — Library folders return directly to Home

The retained Library treats `/games` as its navigation boundary. A on its
parent card and X return Home without scanning the device root; nested A,
X and B still go up one folder and focus the folder just left. Empty and
non-game-only roots stay in Indigo when Library Folders is enabled. B and X
retain the selected game or folder on reopening; the root parent card opens
on the first actual card, as the renderer normally does.

The regression executes the production file filters, directory scan,
classifier, card snapshots, navigation branches and menu handoff. Plain and
sanitized runs pass; replaying the preceding candidate reproduces the legacy
browser publication. Names such as `Racing.v1` and `Nintendo.GC`, dotted
ancestors and full matching PNG names already pass those filters. Leading
dots retain the existing hidden-file policy, and Mac metadata stays filtered.

The SD2SP2 folder route covers all four Library layouts and records presented
frames during parent and Home transitions. Its guard identifies Swiss's
path, device panel and file rows, with single-frame, indexing, registration
and delayed-tail regressions. The existing global scene budgets and pixel
thresholds are preserved.

## 2026-10-04 — Preserve motion and save details together on beta

Incorporated the accepted beta icon and motion changes into Memory Cards.
The renderer retains the save summary inset and its focus positions while
preserving the sliding detail focus. Product rendering and metadata sources
match the independently tested combined implementation. Both branches' prior
journal entries are preserved below.

The shared launch GX regression checks both the new inset and moving focus.
The global frame profiles match the combined renderer, including its opening
transition. Source checks, the upstream ownership gate, actual launch and
Gameflow GX tests and the frame budget checks pass.

## 2026-10-04 — Preserve icon and motion coverage while incorporating beta

Merged the current beta branch into the motion polish branch. The resolved
stroke GX harness retains the icon lift, shadow bounds and GX-state restoration
checks alongside the controller idle-release checks. The shared harness matches
the independently tested combined implementation.

The project journal belongs to Indigo. The upstream source gate now recognizes
its directory, with a regression that still rejects unlisted upstream edits and
a similarly named directory outside the journal namespace.

Validation: source checks, the upstream ownership check and the actual stroke GX
stream suite. Product changes from both accepted branches are retained.

## 2026-10-04 — Save panel hierarchy and native button hints

Save details now promotes the save name, separates block and KiB totals,
aligns source and date rows, and shows native A/B icons for Actions and Back.
The Library Saves inset uses larger left-aligned totals and a brighter update
line. Metadata, action handlers and Library focus positions are unchanged;
the popup owns its bounded metadata copy and prepares text fits before drawing.

The native build and package checks, full local host suite and focused footer
parser regression pass. Native default and widescreen save routes pass 84 and
83 checks without retries; the themed simulated-card route passes 144 checks.
The 55-test emulator suite passes, with two local mtools checks skipped. Its
regressions cover real captures, value-only probes, thin digits, the small
themed heading, missing fields and unsafe A retries. Global text thresholds
and steady-frame requirements remain unchanged. Only affected guide pictures
and native fixtures were refreshed.

Exact final-source CI, SD installation and console
acceptance remain pending. Release promotion still requires the maintainer's go.

## 2026-10-04 — Themed save details detection and popup input guard

The previous final-source CI gate failed only in the themed GC Loader smoke
route. Native captures from its exact binary show the size text correctly;
Jet Black desaturates the muted text below the detector's global threshold.
Only the size phrase now uses its measured threshold. Native regressions
reject erased size text and the browser beneath the popup. Popup detection
requires all four borders before accepting any fields, and A retries stop
after any context change, including a transient change that returns to the
browser. Failed checks retain their native screenshot.

The emulator harness's 52 tests pass, including an independent mutation
check for unsafe retries. Focused default and widescreen virtual-card routes
against the exact previous binary pass 76 and 75 checks, respectively. Product
sources and global frame budgets are unchanged. The themed smoke rerun, new
final-source CI, SD installation and console acceptance remain pending.

## 2026-10-04 — Save details test candidate and native captures

The console build, SD package checks and full host suite pass, including
plain, sanitizer and contract lanes. Actual Dolphin captures show the new
known and unknown date dialogs, simulated slot metadata and guarded actions,
plus Library totals changing from one copy/two blocks to two copies/four
blocks after export. The RAW icon plays two distinct authored texture frames.
The corrected widescreen route passes all 71 checks.

Widescreen detection restores authored coordinates from Dolphin's letterbox
without altering native screenshots. Actual capture regressions reject a
missing update field and distinguish the complete block/KiB phrase. Seeded
settings retention is checked separately from fresh-card Save & Exit writes.
Guide pictures now show the current cube interface and details, with an
actual lossless icon animation. Final source CI, SD installation and the
next console acceptance remain pending; release promotion is not authorized.

## 2026-10-04 — Feature branch validation

The upstream comparison recognizes the development journal as Indigo-owned,
matching the documented per-repository record. Production controller,
metadata and renderer regressions pass on the feature branch. The new
Detail inset measures 136 geometry vertices and five batches in both shapes;
its tests retain the branch's existing focus behavior.

## 2026-10-04 — Save details and Library save statistics

Selecting a save now opens its details before guarded actions, including
when both columns contain read-only images. Details show blocks, KiB, source
and the save's recorded update date. Creation dates are explicitly not
recorded; SD container dates are never substituted. Physical metadata must
match the selected slot, game, maker, filename and size. Calendar formatting
works beyond 2038 without relying on the target's time_t width.

Game Detail takes a read-only snapshot for the verified six-byte disc ID and
shows save copies, blocks and update date above Settings and Cheats. Scans
examine at most 256 folder entries and 16 RAW images; omitted subfolders,
unreadable sources and truncated scans are marked partial. Draw functions
perform no storage reads, and existing focus and launch actions are retained.

Focused production controller, collector and metadata regressions pass
plain and sanitized builds. Tests stream fragmented RAW saves through the
real adapter and decoder, including distinct animated frames. Detail GX
budgets separately measure panel geometry and medium text in both screen
shapes. Dolphin routes cover the details flow, read-only images and Library
totals before and after a verified export. Console acceptance remains the
next step before release promotion.


## 2026-10-04 — RAW adapter Linux sanitizer startup

The standalone RAW adapter harness now uses the Linux-only non-PIE sanitizer
flags already required by the main host suite. A separate minimal startup
probe reproduced high-entropy ASLR failures before main without Indigo code.
Sanitizer instrumentation and GameCube product build flags are unchanged.

## 2026-10-04 — Virtual card emulator coverage and guide captures

A dedicated CI route boots the SD package with both physical slots empty,
browses a public synthetic RAW image and exports a fragmented save into the
other SD column. FAT readback verifies the full save and unchanged image.
The named L/R menus preserve the selected source; both physical slot routes
and invalid image fallback have production-function regressions.

The no-card workflow passed 22 emulator preview checks. Updated SD, virtual
card and storage chooser guide pictures come from that actual Dolphin build;
older physical-card pictures still show the previous header controls. The
combined final console test is awaiting its frozen CI artifact.

## 2026-10-04 — SD virtual cards and storage selection

Console testing succeeded overall but found an empty SD save view and hard
to discover L/R switching. The explorer now recognizes Swiss RAW images,
validates bounded metadata and streams save blocks for artwork and verified
GCI exports. Images have no write, move, erase or format path. Physical cards
remain available, while absent slots default to independent SD columns.
Permanent L/R storage buttons open named choices instead of cycling blindly.

Regression coverage includes metadata redundancy and malformed chains,
read-only controller routing, independent SD destinations, storage defaults
and a dedicated RAW fuzzer. The coordinated test build also smooths cube
edges and refreshes the affected emulator route and guide.

## 2026-10-04 — Memory Cards cube outlines

Console feedback found jagged outlines around the small save cubes. Their
renderer now adds one native pixel of outward coverage only on silhouette
edges, with shared corners and the same fade as each face. Original fill
quads, save icon texels and banner drawing are unchanged. Both screen shapes,
small and turning cubes, partial fades, GX vertex counts and unchanged icon
geometry have host regressions. The Memory Cards guide pictures need a new
capture from the next combined test build.
## 2026-10-04 — Preserve hidden-file filtering with beta icon and motion updates

Incorporated the accepted beta icon and motion updates and preserved both
branches' development history. The Library sidecar and hidden-file filtering remains unchanged.
Source and ownership checks pass; CI will validate the combined branch.

## 2026-10-04 — Project journal ownership in the source gate

The upstream audit now recognizes the project-owned AGENTS directory,
including its development journal. This fixes the source gate rejecting the
journal added with this branch's sanitizer regression work. The product
sources and existing behavior are unchanged.

The real upstream-checker regression accepts the project journal while still
rejecting unlisted upstream code and similarly named directories. The
upstream audit and source checks pass.


## 2026-10-04 — Stabilize Linux folder-visibility sanitizer startup

The standalone folder-visibility regression harness now adds `-fno-pie`
and `-no-pie` to sanitizer builds on Linux, matching the host-test Makefile.
This avoids Clang sanitizer failures before `main` on affected Linux hosts
while retaining the address and undefined-behavior checks.

Validation: the plain and sanitized harness passes on macOS; the sanitized
harness passes with GCC and Clang in the pinned Linux build image.
## 2026-10-04 — Preserve accepted beta and manual rewind safeguards

Incorporated the accepted Memory Cards and motion changes from beta while
retaining the bounded manual rewind fix. Shared renderer and tab harnesses
match the tested combined implementation. All earlier journal entries from
both branches are preserved below.

## 2026-10-04 — Preserve seek bounds with beta icon and motion updates

Incorporated the accepted beta icon and motion updates and preserved both
branches' development history. Manual MP3 seek bounds remain unchanged.
Source and ownership checks pass; CI will validate the combined branch.

## 2026-10-04 — Stabilize Linux MP3 sanitizer startup

The standalone MP3 regression harness now adds `-fno-pie` and `-no-pie`
to sanitizer builds on Linux, matching the host-test Makefile. This avoids
Clang sanitizer failures before `main` on affected Linux hosts while
retaining the address and undefined-behavior checks.

Validation: the plain and sanitized harness passes on macOS; the sanitized
harness passes with GCC and Clang in the pinned Linux build image.
## 2026-10-05 — Normalize save captions and missing-title identities

Bounded save text now collapses control bytes and whitespace, preserves the
IPL font's Windows-1252 accents and symbols, and converts their common UTF-8
forms. Missing comments use conservative game-and-maker identity captions
for the supported fallback families, then a bounded readable directory name.
Valid comments retain precedence. This changes display text only; icon frames
and the recorded update date remain governed by their existing metadata.

Strict host and ASan/UBSan regressions cover absent comments, exact identity,
wrong makers, invalid entries, unterminated names, encoding and short buffers.
Controller integration and native screen verification remain separate gates.
