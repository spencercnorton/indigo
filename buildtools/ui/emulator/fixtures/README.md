`wide-save-stats.png` is an untouched 640×480 Dolphin capture of commit
9a24d4c, using only the public demonstration game Astral Circuit and the
synthetic Demo Card.raw. It shows the known two-block save and recorded
2024-02-29 12:34 date. No user's saves or memory card were involved.

Dolphin auto-aspect letterboxes the 16:9 menu at y60..420. The native date
has 52 bright pixels, below the unchanged 60-pixel authored-stage threshold.
The host regression restores authored coordinates for detection and also
rejects a copy with that date field removed. Screenshots remain untouched.

`wide-save-details-known.png` and `wide-save-details-unknown.png` are
untouched captures of the same public fixture. They show two blocks/16 KiB
and one block/8 KiB. The single-digit masks alias at native resolution;
the regression compares the bounded block/KiB phrase instead.
