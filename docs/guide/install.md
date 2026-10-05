[Indigo guide](README.md) › Install

# Install

Start with a GameCube that already runs Swiss. This walkthrough puts Indigo
on your card, shows how to launch it with your particular loader, and keeps
your existing games, settings and recovery option available.

**You need:** your working Swiss setup, its SD card, a computer with a card
reader, and the [Indigo SD card ZIP](https://github.com/spencercnorton/indigo/releases/latest).
Download `Indigo-vX.Y.Z.zip` from **Assets**. GitHub's **Source code** ZIP is
for building Indigo and does not contain the ready-to-run program.

Already using Indigo? Go to [Update Indigo](#update-indigo).

## 1. Find the right card and boot method

A **boot method** starts a program when you turn the console on. An
**SD2SP2 or SD Gecko** is a card adapter: it holds files, but does not start
Indigo by itself. Your boot card and the card holding your games can be
different cards.

| How you start Swiss now | Card for the boot file | Indigo launch file |
| --- | --- | --- |
| [PicoBoot / PicoLoader using gekkoboot](#picoboot-or-picoloader-using-gekkoboot) | The SD2SP2, SD Gecko or other adapter your bootloader reads | `/ipl.dol` |
| [FlippyDrive with a root `boot.dol`](#flippydrive-booting-bootdol) | FlippyDrive's microSD card | `/boot.dol` (a renamed copy of Indigo's `ipl.dol`) |
| [GC Loader](#launch-from-swiss-gc-loader-and-other-setups) | GC Loader's SD card | Keep the existing `/boot.iso` or `/boot.gcm`; launch `/apps/indigo.dol` from Swiss |
| [Swiss in flash, cubeboot, or another setup](#launch-from-swiss-gc-loader-and-other-setups) | A card Swiss can browse | Launch `/apps/indigo.dol` from Swiss |

**If you are unsure, use [Launch from Swiss](#launch-from-swiss-gc-loader-and-other-setups).**
That route lets you try Indigo without changing the program that starts your
console. A missing `ipl.dol` does not by itself mean you need to flash
firmware. With PicoBoot, its bootloader does not read GC Loader's own SD
slot: use the adapter it already boots from.

## 2. Back up, then unzip on your computer

1. Turn the console off, remove the card and open it on your computer.
2. Copy the card's contents to a backup folder on your computer. Keep your
   existing boot files and the whole `swiss` folder: it can hold settings,
   cheats, artwork and saves. You do not need to format a working card.
3. Extract `Indigo-vX.Y.Z.zip` **on your computer**. Open the extracted
   folder. You should see these items directly inside it:

```text
Extracted Indigo download/
├── Indigo-README.txt
├── ipl.dol                   Indigo's executable
└── swiss/
    ├── patches/
    │   └── apploader.img     return to Indigo after a game
    ├── ui/                   optional artwork goes here
    └── indigo/
        ├── LICENSE.txt
        └── NOTICE.txt
```

The **card root** means the first level you see when you open the card,
beside its existing `swiss` or `games` folders. A path such as `/ipl.dol`
means a file at that first level, not inside a folder named `root`.

Do not copy the unopened ZIP or its enclosing `Indigo-vX.Y.Z` folder to the
card. `SD card/Indigo-vX.Y.Z/ipl.dol` is too deep for a loader looking for
`SD card/ipl.dol`.

## 3. Copy for your setup

Choose **one** launch route below. These instructions name both the file in
the extracted download and its destination on the card.

### Copy the shared files without replacing folders

Every route uses the download's `swiss` files. **Merge the contents** into
your existing `swiss` folder; keep its other files and subfolders.

| From the extracted download | To the card |
| --- | --- |
| `swiss/patches/apploader.img` | `/swiss/patches/apploader.img` |
| `swiss/indigo/LICENSE.txt` | `/swiss/indigo/LICENSE.txt` |
| `swiss/indigo/NOTICE.txt` | `/swiss/indigo/NOTICE.txt` |
| Empty `swiss/ui` folder | `/swiss/ui` if it does not already exist |

**On a Mac:** hold Option while dragging the `swiss` folder onto the card
and choose **Merge** if offered. Do not choose **Replace** for the whole
folder: that removes its existing contents. If Merge is unavailable, open
both `swiss` folders and copy the files to the destinations above one by
one. On Windows or Linux, merge folders and replace only the matching
files listed above. The empty `ui` folder must not replace your poster pack.

`apploader.img` changes the **Apploader** In-Game Reset destination to
Indigo, including for games started in stock Swiss. Keep its old copy in
your backup if you want to restore that destination later. With two cards,
put the patch on the device used for game patches; check **Settings › Setup
› Storage › Configuration Device** for the device holding your settings.

### PicoBoot or PicoLoader using gekkoboot

Use this route when the installed firmware already reads `/ipl.dol` from
an SD adapter. PicoLoader kits can instead start Swiss from flash; those
can use the next route without a firmware change.

1. Identify the existing `/ipl.dol` before replacing it. It may be Swiss,
   an older Indigo, or cubeboot. If it is cubeboot or you are unsure, use
   **Launch from Swiss** below.
2. If it is **stock Swiss**, keep a recovery copy. With gekkoboot, copying
   that Swiss file to `/z.dol` gives you a Z-button shortcut at power-on.
   Do this only if `z.dol` is unused; if it already exists, preserve it and
   choose another unused shortcut from the
   [gekkoboot button table](https://github.com/redolution/gekkoboot#usage).
   Do not overwrite someone else's shortcut. If `ipl.dol` is already
   **Indigo**, keep the computer backup and update it in place.
3. Copy the extracted download's `ipl.dol` to `/ipl.dol` on the boot card.
   Merge the shared files as described above.

```text
SD card root/
├── ipl.dol                   Indigo, copied from the download
├── z.dol                     your preserved Swiss, if this shortcut was free
├── swiss/                    existing files + Indigo's shared files
└── games/                    existing games, kept in place
```

Safely eject the card, return it to its adapter, and power on without
holding a shortcut button. Indigo should start. If you made the Swiss
shortcut, also test holding **Z** at power-on.

For an optional change from flash-based Swiss to gekkoboot, follow your
hardware's own instructions:
[PicoBoot](https://support.webhdx.dev/gc/picoboot/installation-guide) uses
`picoboot_full_pico.uf2` for Pico/Pico W or `picoboot_full_pico2.uf2` for
Pico 2/Pico 2 W; [PicoLoader](https://makeo.github.io/PicoLoader/) uses
`picoloader_gekkoboot.uf2`. These are different devices and firmware files.
You do not need this change to launch Indigo from Swiss.

### FlippyDrive booting `boot.dol`

1. Back up the current `/boot.dol` if there is one. Keep your other boot
   and cubeboot files.
2. Make a copy of the extracted Indigo `ipl.dol` on your computer and
   rename that copy to **`boot.dol`**. Make sure the filename does not
   become `boot.dol.dol` when file extensions are hidden.
3. Copy that `boot.dol` to the root of **FlippyDrive's microSD card**.
   Merge the shared `swiss` files as above. You do not also need to put
   Indigo at `/ipl.dol` for this route.

```text
FlippyDrive microSD root/
├── boot.dol                  Indigo: the download's ipl.dol, renamed
├── swiss/                    existing files + Indigo's shared files
└── games/                    your games
```

Safely eject, reinstall the card, and power on in **normal mode**, without
holding shortcut buttons. The [FlippyDrive startup guide](https://docs.flippydrive.com/usage.html#normal-mode)
explains the root `boot.dol`; if your configuration uses bypass mode, see
its [boot mode settings](https://docs.flippydrive.com/configuration.html#boot-mode). If you prefer to keep cubeboot or your
current boot program, use **Launch from Swiss** instead.

### Launch from Swiss: GC Loader and other setups

This route keeps the console's current startup. It works for GC Loader,
Swiss in flash, cubeboot and other setups where Swiss can browse a card.
It also works as a first try before making Indigo your default.

1. Leave existing `/boot.iso`, `/boot.gcm`, `/boot.dol`, `/ipl.dol` and
   button shortcuts as they are. The Indigo ZIP contains **no `boot.iso`**;
   renaming `ipl.dol` to `boot.iso` does not make it a disc image.
2. Create `/apps` on a card Swiss can read if it is not already there.
   Copy the download's `ipl.dol` into it, renaming the copy to
   **`indigo.dol`**. Do not copy the download's `ipl.dol` to the card root
   with this route.
3. Merge the shared `swiss` files as above. Start stock Swiss normally,
   choose the card, open **apps**, select **indigo.dol**, and launch it as
   a DOL program.

```text
GC Loader SD card root/      (example)
├── boot.iso                 your existing Swiss disc image, unchanged
├── apps/
│   └── indigo.dol            the download's ipl.dol, renamed
├── swiss/                   existing files + Indigo's shared files
└── games/                   your games
```

The filename `indigo.dol` keeps it apart from Swiss's startup autoload
filenames. On the next power-on, stock Swiss starts again; open
`/apps/indigo.dol` to return to Indigo.

On a Wii, use a model with GameCube ports, start Swiss in GameCube mode,
and put the card in an **SD Gecko**. Swiss does not read the Wii's own SD
slot in GameCube mode. Keep your existing Homebrew Channel setup.

## 4. Confirm the first launch

Indigo opens on Home, with a cube and the face name underneath. Open
**System › System Information › About Indigo** and check the version or
build commit. Seeing stock Swiss's file list immediately after power-on
can be expected with the **Launch from Swiss** route: launch
`/apps/indigo.dol` first.

<p align="center">
  <img alt="Home on the Library face: the glass cube shows a GameCube controller, LIBRARY is written underneath, and the hint line reads Turn and A Open." src="images/home-library-face.png" width="640">
</p>

1. Turn to **Source** and choose the device **holding your games**. This
   may be a different device from the one that loaded Indigo.
2. Open **Library**. It looks for `/games` on that source by default.
   Keep existing games; for a new card, either of these layouts works:

```text
SD card root/
└── games/
    ├── Super Mario Sunshine.iso
    └── The Wind Waker/
        └── game.iso
```

Supported disc-image extensions are `.iso`, `.gcm`, `.tgc` and `.fdi`.
`.rvz`, `.gcz` and ZIP files must be converted or extracted first. Apps go
in `/apps`, outside `/games`. Artwork and cheats are optional; install
those after you have launched Indigo and found your games.

Next: [library setup](library.md#set-up-the-games-folder),
[controls](controls.md), [posters](posters.md) and [cheats](cheats.md).

## If it does not start or games are missing

| What you see | Check next |
| --- | --- |
| Your old Swiss or cubeboot still starts | With **Launch from Swiss**, this is expected. Otherwise check the boot filename and the physical card in the table above. |
| GameCube menu, bootloader error, or no launch | Check that the ZIP was extracted, the DOL is at the card root for direct boot, and its extension is not doubled. Reinsert the card with the console off; restore your backed-up boot file if needed. |
| Indigo starts but the Library is empty or a plain file list appears | Choose the source holding the games. Check for `/games` at that card's root and supported disc images; with Library Folders off, no images means the plain file list. |
| In-Game Reset returns to stock Swiss | Check the copied `apploader.img` and the reset mode below. **Reboot** starts your original boot route again. |

For further help, see [Troubleshooting](troubleshooting.md). When asking
for help, include your Indigo version, boot method, which adapter holds
each card, and the destination path you copied to.

## Update Indigo

1. Back up the card, then extract the new release on your computer.
2. Replace **the Indigo executable you actually launch**, using the same
   destination as your installation: `/ipl.dol`, `/boot.dol`, or
   `/apps/indigo.dol`. For either renamed route, rename the new download's
   `ipl.dol` again. Updating an unused root `ipl.dol` will not update your
   FlippyDrive `boot.dol` or your Apps copy.
3. Merge the new shared files, including `swiss/patches/apploader.img`,
   without replacing the whole `swiss` folder. Keep games, settings,
   posters, cheats, saves and recovery shortcuts.
4. Safely eject, launch Indigo by your usual route, and check About Indigo.

This applies to both Indigo 1.x and 2.x. Older 1.25.0 instructions renamed
stock Swiss to `swiss.dol` to avoid startup autoload. Indigo 2.0 and later
no longer autoload it ([issue 3](https://github.com/spencercnorton/indigo/issues/3)).
There is no need to rename your backup again; a `z.dol` shortcut is useful
only when your installed bootloader supports it and the name is free.

## Go back to stock Swiss

- **Direct DOL boot:** restore your backed-up boot file under its original
  filename. On gekkoboot, use your preserved button shortcut if you made
  one; hold its button while switching on.
- **Launch from Swiss:** start Swiss as before and skip launching Indigo.
- **Apploader In-Game Reset:** restore the backed-up
  `/swiss/patches/apploader.img`, or use stock Swiss's copy from
  `Apploader/EXTRACT_TO_ROOT.zip` in a
  [Swiss release](https://github.com/emukidid/swiss-gc/releases/latest).

Both programs read the same settings file. Stock Swiss drops Indigo-only
settings when it saves, such as Menu Color and Library Layout; keep your
settings backup if you want to preserve those choices.

## Leaving a game

With **In-Game Reset** on (Settings › Quick), hold **A + Z + START** to
leave a game, or **R + Z + START** to restart it. **Apploader** returns to
the program in `/swiss/patches/apploader.img`; the Indigo download puts
Indigo there. **Reboot** resets the console and follows your normal boot
route, which can start stock Swiss rather than Indigo.

## Build it yourself

Most users can use the ready-made ZIP above. For a source build, run the
same container image as CI:

```bash
git clone https://github.com/spencercnorton/indigo.git
cd indigo
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/work" -w /work \
  ghcr.io/extremscorner/libogc2@sha256:e6531ecaa458d0b5d8c9ba57cee1facc5fb9120808c6d9eebb2c05e4ffaf6f0f make dev
buildtools/sd_package.sh dev
```

The build writes `cube/swiss/swiss.dol` **on your computer**; that is not a
card destination. `sd_package.sh` creates `Indigo-dev.zip` with the same
layout as a release. Follow the launch route above using its `ipl.dol`.
Upstream's `make all` and `make package` targets need device firmware and
prebuilt tools this fork does not redistribute; use `make dev` here.

---

<p align="center"><a href="README.md">← Guide</a> · <a href="controls.md">Controls →</a></p>
