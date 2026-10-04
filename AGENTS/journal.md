# Indigo development journal

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
