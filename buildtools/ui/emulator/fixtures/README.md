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

`themed-save-details.png` is an untouched capture of commit e6c3e865 with
the public Copper Orchard save on a simulated card. Menu Color is Jet
Black, Backdrop Color is Emerald, Wave Color is Gold, and animations are
reduced. The visible size/source line peaks at gray147 after the palette
and alpha blending, below the normal bright-label threshold160. Its
bounded muted-text check retains the same minimum coverage and steadiness;
the regression also rejects an erased size line. No real card was used.

`themed-save-browser.png` shows the same synthetic card before A opens
details. Its cube pixels can pass the old title-band check. The popup guard
requires the authored presentation frame, rejects this browser capture,
and stops repeating A as soon as a popup or static context change appears.
