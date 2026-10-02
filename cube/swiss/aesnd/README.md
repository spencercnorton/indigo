# AESND, with Indigo's fix

Indigo's menu music and sounds play through AESND, libogc2's audio library.
Swiss is built with AESND from this directory rather than the toolchain's
`-laesnd`, because one function in it needed a fix libogc2 does not have.

| Path | What it is |
| --- | --- |
| `libogc2/` | libogc2's AESND, byte for byte, at the commit `UPSTREAM` names: the toolchain's own |
| `0001-reset-waits-for-the-dsp.patch` | Indigo's change, applied when Swiss is built |
| `UPSTREAM` | That commit, and the toolchain version that must still match it |

## The fix

`AESND_Reset` stops AESND: it turns interrupts off, stops the audio DMA and
mails the DSP to stop (`0xfacedead`), then waits for the DSP to take that
mail. But AESND keeps the DSP busy every audio period, with or without
voices, and the mixer (`libogc2/libaesnd/dspcode/dspmixer.s`) takes no mail
until the CPU has read the one it sent last: its command loop runs
`wait_mail_sent` before `wait_mail_recv`. The CPU reads the DSP's mail in the
DSP interrupt, which `AESND_Reset` has just turned off. When a frame was
still being mixed, the DSP's answer was never read, the DSP never took the
stop, and both waited for ever. Indigo's menu stops AESND before every launch,
so a launch froze until the console was switched off.

Everywhere else AESND mails the DSP only once `__aesnddspcomplete` says the
last frame is done. The patch makes `AESND_Reset` wait for that too, and for
the task to have started (`__aesnddspinit`), letting the DSP interrupt in
while it waits. The DSP then waits for mail, and takes the stop at once.

How it was found and shown:

- The emulator test caught a launch hung in `DSP_CheckMailTo` under
  `AESND_Reset`. A Dolphin build that logs both mailboxes showed the stop
  unread in the CPU's (`face…`) and the DSP's last answer unread in its own
  (`dcd1…`) for the whole hang, the DSP's code in `wait_mail_sent`.
- A program that starts AESND with a playing voice and stops it a varying part
  of an audio period later froze within 50 rounds with libogc2's
  `AESND_Reset`, and runs every round with this one.
- The earlier `AESND_Reset` hang that libogc2 (38edc9db, 2022) and devkitPro's
  libogc (#143, 2023) fixed is another bug: the mixer did not recognise the
  stop mail at all, so every reset hung. This copy has that fix.

## How it is built

`cube/swiss/Makefile` copies `libogc2/libaesnd/aesndlib.c` into its build
directory and applies the patches here in order (`patch -F0`: one that no
longer applies exactly fails the build), assembles the mixer from
`dspmixer.s` with the toolchain's `gcdsptool`, and compiles AESND with Swiss's
flags. The copy's headers come before the toolchain's on the include path.
Without the patches this is the library the toolchain ships: the same
sources, the same headers, and a mixer byte-identical to the one in its
`libaesnd.a`.

## How it is checked

- `buildtools/ci/check_aesnd.py`, in CI's Source checks: every file in
  `libogc2/` is byte-identical to the commit `UPSTREAM` names, and the
  toolchain's libogc2 (its `ogc/libversion.h`) is still that commit.
- The emulator test stops AESND 300 times in Dolphin with the `aesndlib.o`
  Swiss built (`buildtools/ui/emulator/aesnd/`).

## When the toolchain moves

`check_aesnd.py` fails until this copy follows. In a libogc2 clone, compare
`libaesnd/`, `include/aesndlib.h` and `include/mp3player.h` between the commit
`UPSTREAM` names and the toolchain's:

- unchanged: put the toolchain's commit and version in `UPSTREAM`;
- changed: copy the new files over these with a plain `cp`, update `UPSTREAM`,
  and build and run the emulator test: a patch that no longer applies fails
  the build.

Don't edit the files in `libogc2/`: change them with a patch here.
