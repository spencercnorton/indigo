The save-related pictures are untouched 640×480 Dolphin captures of commit dcfbdc5, using only
public demonstration games and synthetic saves. No user's save or physical
memory card was involved.

`default-save-details-known.png` and `default-save-details-unknown.png`
show the two-block Copper Archive save and one-block Moonlit Lake save in
4:3. `wide-save-details-known.png` and `wide-save-details-unknown.png` show
the same saves in 16:9. The known save records 2024-02-29 12:34; the other
has no recorded update. Both show Created: Not recorded.

Dolphin letterboxes the 16:9 menu at y60..420. Detection restores authored
coordinates with nearest-neighbor sampling; these native screenshots remain
unchanged. At the unchanged bright-text threshold 160, the wide block values
1 and 2 contain 30 and 58 pixels. The bounded numeric predicate accepts their
narrow glyphs while rejecting pixels, bars, thin lines and dense patches.
The actual masks distinguish 1 from 2 and 8 KiB from 16 KiB in both shapes.

`wide-save-stats.png` shows Astral Circuit's SAVES inset before exporting:
one copy, two blocks and the recorded update date. Its date probe excludes
the Updated prefix. A regression removes the date while retaining that
prefix and rejects the missing value.

`themed-save-details.png` shows Copper Orchard on a simulated physical card
with Menu Color Jet Black, Backdrop Color Emerald, Wave Color Gold and
reduced animations. `themed-save-browser.png` shows the same card before A
opens details. The popup guard requires all four panel borders and the
fixed SAVE DETAILS eyebrow; it rejects the browser and does not depend on
save-name length. Its small themed marker has 49 bright pixels at threshold
160, so only that fixed marker uses a bounded 40-pixel minimum; value-word
checks keep their 60-pixel minimum. A retries stop after any context change, including a
transient change that returns to the browser.

Value-only probes exclude Blocks/KiB captions and Source/Created/Last
updated labels. Regressions erase each value and retain its caption or
label; labels alone cannot satisfy a missing value. Positive captures are
never altered. Mutations are made only to in-memory copies during tests.

`folder-horizontal.png` and `folder-legacy-browser.png` are untouched
640×480 captures of exact commit 08e536aa from the public demonstration SD
image. The latter shows the actual defect: X from the `/games` Library
opens Swiss's list at the device root. The folder transition classifier
requires its separate left device card and long, regularly spaced file-row
borders. Controls dim these native pixels down to 10%, remove either
geometry component and inject a single legacy frame into an otherwise
valid sequence, including a delayed encoder tail. Blank frames are allowed
as fades and cannot satisfy the eventual Library/Home title check.

`folder-vertical.png`, `folder-grid.png`, `folder-spotlight.png` and
`folder-home.png` are untouched 640×480 captures of the repaired product
at commit 1645d023, built with the pinned SDK. They use the same public
demonstration games and dotted-folder SD image. Every Library layout and
Home rejects the legacy predicate, including faded copies made in memory.
Grid and Spotlight were captured in separate, explicitly seeded layout
controls; Vertical and Home came from the repaired folder route.

## Folder color words

`folder-color-indigo.png` and `folder-color-azure.png` are unscaled native
Dolphin RGB crops of the folder page's value box (240, 328, 570, 351) from
commit `ea0946fd7d740a958e78dedf486bdd17d4734876`, NTSC component video with
SD2SP2 and a fresh demonstration card. Indigo has 73 pixels at the reader's
brightness threshold; Azure has 50, below the ordinary Library-title minimum
of 60. The crops preserve the original pixel values and reproduce that
rejection without launching Dolphin. Their test also exercises stable changed
words, rejects unchanged values, and rejects a single changed frame.

`memory-folder-path-root.png` and `memory-folder-path-open.png` are the
actual widescreen header path values before and after opening the synthetic
Backups folder, captured from build `738bb96`. They retain the nearest-neighbor
authored-coordinate detection pixels. The native captures showed that reading
the whole header lets its unchanged large SD glyph hide the path change.
`memory-folder-header-root.png` and `memory-folder-header-open.png` preserve
those actual wider header crops to reproduce the previous rejection.

`files-screen.png`, `files-screen-right.png`, `files-screen-wide.png` and
`files-screen-themed.png` are untouched 640×480 Dolphin captures of the File
Browser from the public demonstration disc and SD card: the left pane in
`/games` with a game focused, the right pane focused after opening `/apps`,
Menu Widescreen on a GC Loader card, and the GC Loader card's settings
(`settings/non-default.ini`: Menu Color Jet Black, an Emerald backdrop), whose
grey edges count as edges too. `files_screen` finds both pane boxes, both storage
buttons, the gutter between the panes and the info bar by their edges, over
the spans that stay put when Menu Widescreen widens the panes outward;
`active_pane` compares the two panes' top edges, the focused one lit in full.
The tests blank each part in an in-memory copy, bridge the gutter, and check
the legacy list, the Library layouts, Home and Memory Cards never pass.

`files-screen-z.png` is the same screen with Swiss's Manage File box open over
the rows (Z on a game), from the same run as `files-screen.png`: the route's
Z step tells the box from the rows by the text in the band it covers, and
`files_text` reads each pane's path line in both screen shapes.
