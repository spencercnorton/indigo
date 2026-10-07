[Indigo guide](README.md) › System

# System

The System face tells you about your console, manages the saves on your
memory cards, opens the File Browser and restarts Indigo. On Home, turn
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

**File Browser** opens at the top of your source, even when your games show
in the Library, for the files the Library and Apps don't show: settings,
cheats, saves, music and anything else on the card. It can have a side of
the Home cube of its own too: Settings › Setup › Console › **Up Face** (or
Left, Right or Down Face) › File Browser; see
[Choose the sides](home.md#choose-the-sides). The same screen shows wherever
Indigo lists files outside the Library: from the Library face when there are
no games to show, from Recent, and for an Autoload folder.

<p align="center">
  <img alt="The File Browser: two panes over graph paper. The left one, marked SOURCE, is open at the disc's games folder with a game focused; the right one shows the top of the disc. The info bar below names the focused game and its size." src="images/files.png" width="640">
</p>

It shows two panes side by side, as Memory Cards shows two stacks. The left
pane, marked **SOURCE**, is your source: games, programs and music start
from it. The right pane shows another folder, on the same device or on
another one. Above each pane are its device, the device's free space (or
**read-only**; nothing when the device can't say, as on a network share),
the open folder and where you are in it. Each row has a cube for what the entry is
(folders violet, disc images silver, programs teal, firmware updates amber,
music pink), the file's real name with its extension, and its size. A name
too long for the row is cut in the middle, keeping its extension and a
"(Disc 1)" before it; the info bar below shows the whole name, and for a
game its banner and title.

- **Left** and **Right** move between the panes; each keeps its place.
- **Up** and **Down** move a row, and keep going while held. The C-stick
  goes a page up or down.
- **A** opens a folder or starts a file in the left pane, and **X** or `..`
  goes up a folder. A on a disc image opens Swiss's own game screen rather
  than Indigo's [Game details](game-details.md). In the right pane, A opens
  folders; games start from the left.
- **L** and **R** choose the storage on each side, as in Memory Cards: **L**
  the left pane's, **R** the right pane's. **X** or `..` at the top of a
  pane does the same.
- **Y** swaps the two sides: the right pane's device and folder become your
  source, and your source moves to the right. Y again swaps them back.
- **B** goes back to the System face, or to the File Browser face when you
  opened it from there. The Library face opens the Library again as usual.
- **START** shows recently played games; choosing one leaves the File
  Browser.

### Storage on each side

<p align="center">
  <img alt="The File Browser with its Right storage menu open over the right pane: Game Disc, Memory Card - Slot A, Memory Card - Slot B and System, then Other devices. Memory Card - Slot A is focused, and the info bar says it opens on the right." src="images/files-storage.png" width="640">
</p>

The storage menu lists every device Indigo found that it can read, then
**Other devices…**. Up and Down move, **A** chooses and **B** closes it.
The info bar says what choosing the focused device does.

- On the **left**, the device you choose becomes your source, as from
  Source › Change Source; Other devices… opens that picker, with every
  device and each one's settings.
- On the **right**, the pane opens the device at its top. Other devices…
  opens Swiss's list of devices that can be written to.
- Both sides can show the same device, each in its own folder. Two devices
  that share a connector can't be open together, such as a FlippyDrive and
  its flash, or two network shares: the one the other side holds is greyed,
  with why.
- A device that won't open says so in its pane, with the reason, and **R**
  chooses another. Y doesn't swap onto it.
- The first time, the right pane opens on your settings' device, or on your
  source when the settings live there too or when the File Browser was
  opened from the Library face, Recent or an Autoload folder without
  **File Management**. After that it keeps its device and folder until
  Indigo restarts.

**Z** on a file or folder, in either pane, opens Swiss's actions for it:
**X** Copy, **Y** Move, **Z** Delete, **R** Rename and **L** Hide, as far as
the device allows. Opened from System or its face, the File Browser always
has them; opened any other way, they need **File Management** on (Settings ›
Setup › Library). File Browser leaves that setting as it is. A program
folder, which starts its program when you press A on it, is acted on as the
folder.

- **Copy** asks for a device, then a folder: **X** picks the folder you are
  in. Copying from the right pane copies from the right pane's device. If the file is already there, **A** keeps both by numbering the copy
  and **Z** replaces it.
- **Move** asks first, and removes the original once it is copied.
- **Delete** asks for **L** and **A** together. Deleting a folder deletes
  everything in it.
- **Hide** asks first. A hidden file or folder shows again with **Show hidden
  files** on (Settings › Setup › Library).

Indigo also asks before the File Browser changes how your console starts:

- **Z** on `..` sets **Autoload**: that folder opens every time Indigo
  starts, instead of Home. Z there again turns it off. On Swiss's game
  screen, Z does the same for the game.
- **A** on a `.fzn` file writes it to a WiiKey Fusion's flash, and on a
  `.fpkg` file updates a FlippyDrive's firmware.

What it can't do:

- Copy or move a folder: only files.
- Show `/games` folder by folder in the left pane. Swiss lists the disc
  images in `/games`'s folders as one list there (**Flatten directory**,
  Settings › Setup › Library); the right pane shows the folders.
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
