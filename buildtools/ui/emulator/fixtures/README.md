These are untouched 640×480 Dolphin captures of commit dcfbdc5, using only
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
