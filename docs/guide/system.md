[Indigo guide](README.md) › System

# System

The System face tells you about your console, manages the saves on your
memory cards, opens Swiss's own file list and restarts Indigo. On Home, turn
the cube to **System** and press A.

<p align="center">
  <img alt="The System face opened: the cube shows a clock, with System Information, Memory Cards, File Browser and Restart Indigo listed below it." src="../screenshots/system.png" width="640">
</p>

## System Information

**System Information** is six pages about the console and the session. **L**
and **R** move between them, and **B** goes back.

<p align="center">
  <img alt="System Information's six pages in turn: Overview, Console, Connections, Input / Output, About Indigo and Credits." src="images/system-info.png" width="640">
</p>

| Page | Shows |
| --- | --- |
| **Overview** | The time and date, the CPU temperature and its calibration, and the session: the current source and whether it's ready, the video mode, the region and the build. |
| **Console** | The console model, IPL version, CPU, graphics chip and the CPU's unique ID. |
| **Connections** | What's in the memory card slots, serial ports, the disc drive interface and the high-speed port, plus a live look at the slots, the current source and the configuration device. |
| **Input / Output** | What's plugged into the four controller sockets, and the video mode, progressive scan, region, audio and language. |
| **About Indigo** | The Swiss version Indigo is built on, the exact commit and revision, where the source lives, and where to report a bug or ask a question: Indigo's GitHub issues and discussions. |
| **Credits** | The people who made Swiss and Indigo possible, and where the Spotlight layout's design comes from: Gameplay Spotlight by mvizensk. |

Connections and Input / Output are read when you open the page; open it again
to see a change.

The CPU temperature has no factory calibration. If Overview's reading is off
on a cold console, adjust Settings › Setup › Console › **CPU Temperature
Calibration** until it reads about room temperature.

## Memory Cards

**Memory Cards** shows the saves on the memory cards in Slot A and Slot B and
in folders on your SD card as two stacks of cubes, each with its game's
icon, the way the GameCube's own Memory Card screen does, and moves, copies
and erases them from one stack to the other. See
[Memory Cards](memory-cards.md).

<p align="center">
  <img alt="The System face opened with Memory Cards highlighted, the second of its four rows." src="images/memory-cards-system.png" width="640">
</p>

## File Browser

**File Browser** opens Swiss's own file list at the top of your source, even
when your games show in the Library. It is Swiss's list as Swiss has it, for
the files the Library and Apps don't show: settings, cheats, saves, music and
anything else on the card.

<p align="center">
  <img alt="Swiss's plain file list, one entry per row with its banner, name and region flag." src="images/library-file-list.png" width="640">
</p>

- **A** opens a folder or starts a file, and **X** or `..` goes up a
  folder. A on a disc image opens Swiss's own game screen rather than
  Indigo's [Game details](game-details.md).
- **B** goes back to the System face. The Library face opens the Library
  again as usual.
- **START** shows recently played games; choosing one leaves the File
  Browser.
- Settings › Setup › Library › **File Browser Type** changes how the list
  looks.

**Z** on a file or folder opens Swiss's actions for it: **X** Copy, **Y**
Move, **Z** Delete, **R** Rename and **L** Hide, as far as the device
allows. In File Browser they are always there; wherever else Swiss's list
shows, they need **File Management** on (Settings › Setup › Library). File
Browser leaves that setting as it is.

- **Copy** asks for a device, then a folder: **X** picks the folder you are
  in. If the file is already there, **A** keeps both by numbering the copy
  and **Z** replaces it.
- **Move** asks first, and removes the original once it is copied.
- **Delete** asks for **L** and **A** together. Deleting a folder deletes
  everything in it.
- **Hide** asks first. A hidden file or folder shows again with **Show hidden
  files** on (Settings › Setup › Library).

Indigo also asks before the file list changes how your console starts:

- **Z** on `..` sets **Autoload**: that folder opens every time Indigo
  starts, instead of Home. Z there again turns it off. On Swiss's game
  screen, Z does the same for the game.
- **A** on a `.fzn` file writes it to a WiiKey Fusion's flash, and on a
  `.fpkg` file updates a FlippyDrive's firmware.

What it can't do:

- Copy or move a folder: only files.
- Show `/games` folder by folder. Swiss lists the disc images in `/games`'s
  folders as one list (**Flatten directory**, Settings › Setup › Library).
- Show what **Hide unknown file types** and **Show hidden files** hide.

## Restart Indigo

**Restart Indigo** resets the console, which then starts the way it does
after you press its Reset button. If your loader boots Indigo, as PicoBoot
does with `ipl.dol`, you're back in Indigo with a fresh session; a loader that
boots stock Swiss first starts that instead.

Indigo asks first, with **Cancel** selected, so an extra A can't restart it
by mistake. Choose **Restart** with Left or Right, then press A.

<p align="center">
  <img alt="Restart Indigo? over the cube, with Reloads Indigo and ends this session, and the buttons Cancel, selected, and Restart." src="images/system-restart.png" width="640">
</p>

---

<p align="center"><a href="source.md">← Source</a> · <a href="memory-cards.md">Memory Cards →</a></p>
