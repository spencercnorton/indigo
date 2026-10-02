# Dolphin for the emulator test

The emulator runner (`../emulator.Dockerfile`) builds Dolphin from source:
the commit the SD card adapter below was written on (master of 2026-03-18,
release 2603 and 81 commits), plus the patches here, applied in order. Only
the headless front end is built (`dolphin-emu-nogui`).

| Patch | What it does |
| --- | --- |
| [`0001-sd2sp2-adapter.patch`](0001-sd2sp2-adapter.patch) | An SD card adapter on the EXI bus, the emulator core of [dfederspiel/dolphin#1](https://github.com/dfederspiel/dolphin/pull/1) by CodeFly: SD2SP2 in Serial Port 2 (`SerialPort2 = 15`), or an SD Gecko in a memory card slot (`SlotB = 15`), reading and writing the card image `SP2SDCardImage` names. It speaks the SD protocol a byte at a time, SDHC, with CRCs, so libogc2's own driver, FatFs and Swiss's device code run as on a console. |
| [`0002-flush-sd-writes.patch`](0002-flush-sd-writes.patch) | Each write reaches the card image at once, so the test reads back what Indigo wrote once Dolphin stops. |

Both are under Dolphin's licence, the GNU General Public License version 2
or later.

Without them CI could boot Indigo only from a disc: Dolphin has no GameCube
SD adapter of its own, and the card is where every setting, save, poster and
game of a real installation lives.

## Moving to another Dolphin

Change `DOLPHIN_COMMIT` in `emulator.Dockerfile`, check that both patches
still apply (`git apply --check`), and let a pull request's CI run the
emulator test on the new image, bootstrapped as the runner README describes.

Release 2609 takes both patches, but its headless front end built this way
boots nothing ("Could not boot the specified file", for any DOL), so the
image stays on the adapter's own base until that is understood. An SD card
in Memory Card Slot A works but takes minutes to mount in this emulation;
the test uses SD2SP2.
