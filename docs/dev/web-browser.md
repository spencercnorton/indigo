# Web browser cube face (in progress)

Adds a **Web** face to the Home cube that opens a GameCube web browser, the way
the Apps and Emulators faces open a program. Target release: the first minor
version **after 2.4** (this touches `swiss.c`, which resets the pending MemCard
PRO stable qualification 2.4 is finishing, so it must not merge into the 2.4
line).

## Shape: launch a bundled DOL (not compiled into ipl.dol)

The browser ships as its own `cubebrowser.dol` inside the Indigo zip; the Web
face launches it through `boot_dol`, exactly like an app in `/apps`. Chosen over
compiling it into `ipl.dol` because:

- It keeps Indigo's DOL under the 6 MiB tripwire. The browser links mbedTLS and
  would add ~1.18 MB; Indigo has ~0.86 MB of headroom. The in-process version is
  a later, separate effort that first needs the tripwire raised or mbedTLS
  trimmed (a console-tested boot-path change).
- The browser keeps its own renderer, audio, network and crash isolation, so a
  browser fault cannot corrupt Indigo.

Trade-off: the browser renders itself rather than through Indigo's GX/IPL-font
path, so the match to the rest of the UI is close but not pixel-identical. Its
themes already mirror Indigo's (Indigo / Jet Black / Platinum / Spice).

## The browser

Upstream: `Arial-Bold/Cubebrowser`. We maintain a hardened fork (the creator gave
permission to integrate). The fork already carries:

- a clean Mozilla CA bundle (the upstream one shipped four TLS-interception
  roots) plus a guard that fails the build if one returns;
- pinned, checksum-verified mbedTLS 3.6.7 and stb_image, replacing unpinned
  downloads;
- fixes for the page-reachable parser memory-safety bugs found by fuzzing
  (CSS value underflow, `:is`/`:not` recursion, a `<title>` overrun);
- a parser fuzz suite (the upstream project had no tests);
- `Settings > Exit to Indigo`, which returns from the browser's main loop so the
  loader hands control back to Indigo;
- the SD card in a GC Loader as well as an SD2SP2 or SD Gecko: with more than
  one card it takes the one whose `gcbrowser/` holds `cacert.pem`;
- rectangles cut to the clip box before drawing, which stopped a grey wedge
  drawn over some pages.

## Integration checklist (Indigo side)

1. **Widen the 8-face cap first.** `gui/ui_home.c` asserts `UI_HOME_FACE_COUNT
   <= 8` because the `absent` mask is a `uint8_t`; widen it, and raise the
   640-byte motif-state bound in `test_ui_cube_motif.c`.
2. Append `UI_HOME_FACE_WEB` after `FILES`, an icon, and
   `UI_HOME_EFFECT_OPEN_WEB`; update the side-choice name arrays in
   `gui/settings.c` (modulo order) and the ~dozen tests/audits that pin the face
   list (`audit_four_face_home.py`, `test_ui_home.c`, the `*_gx_stream.py`
   per-face arrays, `frame_budget.c`, …).
3. Dispatch the effect in `swiss.c` `homeDispatchEffect` to `boot_dol` on the
   bundled browser, after `menuaudio_shutdown()` + `DrawShutdown()`. Pass the
   current theme to the browser via argv so it opens in the matching colour.
4. Draw a web/globe icon in `gui/indigo_background.c`, fixed primitive count.
5. Ship `cubebrowser.dol` in the zip (`buildtools/sd_package.sh`) at a known
   path, and add a build step that produces it (see Open decisions).
6. Use a distinct internal name (e.g. `WEB`): "browser" already means the File
   Browser in this codebase and its tests.

## Packaging

`sd_package.sh` bundles the browser when `browser/cubebrowser.dol` (and the
`browser/gcbrowser/` CA folder) are present: the DOL goes to
`swiss/indigo/cubebrowser.dol` (where `launchWebBrowser` looks, on the config
device, which it mounts first: with games on another device the config device
is mounted only while something reads it) and the CA folder to `gcbrowser/` at
the card root. Both paths are overridable with `BROWSER_DOL` /
`BROWSER_GCBROWSER`. The browser is optional in the zip (`check_package.py`
`OPTIONAL`), so a plain build still packages; a release ships it. The zip
tripwire was raised to 9 MiB for the ~1 MiB the browser adds.

The browser is **used with permission from Arial-Bold**
(https://github.com/Arial-Bold/Cubebrowser); recorded in `NOTICE`. It ships as a
separate DOL (an aggregate), so its licence stays its own and does not touch
Indigo's GPL.

## How CI builds it

The Build jobs in `ci.yml` and `release.yml` clone the hardened fork at a pinned
commit, fetch its checksum-verified dependencies and build it, then copy the DOL
and CA bundle into `browser/` for `sd_package.sh`:

- fork: https://github.com/spencernorton/cubebrowser (maintained by us, used
  with permission; its own README/NOTICE credit Arial-Bold)
- pinned commit: `cfe550b255eb432377e1d3abe12a8b92006cd0ae` — bump it in both
  workflows' `CUBEBROWSER_REF` to take browser changes.

It builds with the same libogc2 toolchain the runner already carries, and
`get-mbedtls.sh` fetches mbedTLS and stb from GitHub, which the CI egress proxy
allows.

## Open decisions

- **Theme to the browser via argv** — the browser ignores argv today, so it
  opens in its own saved theme (Indigo by default). Add argv parsing to sync it.
- **Return-path console test.** Returning from the browser to Indigo via the
  loader stub is the standard homebrew mechanism but is unverified on the real
  console; test before release.
