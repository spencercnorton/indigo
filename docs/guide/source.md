[Indigo guide](README.md) › Source

# Source

The source is the device Indigo reads your games from: the SD card, the disc
drive, a drive replacement such as GC Loader, a memory card slot or a
network share. Indigo picks one when it starts; the Source face lets you
change it.

## Change the source

1. On Home, turn the cube to **Source** and press A.
2. Choose **Change Source**.
3. Press **Left** or **Right** until the device you want is on the cube,
   then press **A**.
4. You're back on Home, now reading from that device. Turn the cube to
   **Library** and press A to see what's on it.

<p align="center">
  <img alt="On the Source face, A shows Change Source and Refresh Library. Change Source shows Game Disc, Boot + Stream, on the cube; Right shows Memory Card Slot A, Files Ready; Z shows every device, so Right reaches Memory Card Slot B, Not Detected; B goes back." src="images/source-picker.png" width="640">
</p>

Under each device's name, the picker says what it can do:

| It says | Meaning |
| --- | --- |
| Boot + Stream | Games start from it, with the disc audio some games stream. |
| Boot Ready | Games start from it. |
| Files Ready | It holds files you can browse and open. |
| Not Detected | Indigo can't find it right now. A tries again. |

- **Z** shows every device Indigo supports, not only the ones it found. Z
  again goes back to the ones it found.
- **Y** shows details about the device, where it has them.
- **X**, on an SD adapter or IDE-EXI device, shows its speed: Left and
  Right choose 13.5 or 27 MHz, and A keeps it. Some SD cards only work at
  the slower speed.
- **B** goes back.

The disc drive is called **Game Disc**.

## What the Library face opens

If the device has a `/games` folder holding only games, the Library face
opens the poster [Library](library.md). Otherwise it opens Swiss's file list
of the device, where you can browse folders and open games, homebrew
programs (`.dol`, `.elf`) and MP3s:

- **A** opens a file or folder, and **X** goes up a folder.
- With **File Management** on (Settings › Setup › Library), **Z** on a file
  offers to copy, move, rename, hide or delete it.
- Settings › Setup › Library › **File Browser Type** changes how the list
  looks.

## Refresh Library

The other choice on the Source face, **Refresh Library**, reads the current
device again. Use it when what's on it changes while Indigo is running, such
as a new disc in the drive.

---

<p align="center"><a href="personalize.md">← Make it yours</a> · <a href="system.md">System →</a></p>
