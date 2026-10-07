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
