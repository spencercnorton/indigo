# Settings files

Indigo keeps Swiss's settings files exactly as Swiss writes them, so a card
works with either. You can write them on a computer before you boot, which is
the quickest way to set up a new card or to change settings you only set once.

Every key and value on this page is checked automatically against the code
that reads and writes these files.

## Where the files go

Swiss keeps its settings on the **configuration device**, which is usually the
SD card (SD2SP2, SD Gecko) or a GC Loader or other drive replacement. Choose it
on the console in Settings › Setup › Storage › Configuration Device. The choice is
stored in the console's SRAM, not in a file. Until one is chosen, Swiss uses
the first device it finds that can hold settings. A new choice reaches SRAM
only after Save & Exit has written the settings to that device; if the save
fails, the console keeps the old one.

To check that your file loaded, open Settings › Setup › Storage. The line
under the title reads:
- "Settings are saved in swiss/settings/global.ini." when it loaded, or
  after a save has written it.
- "No swiss/settings/global.ini yet: using defaults." when there is no file
  there, or it couldn't be read.
- "No device to save settings to." when no configuration device was found.

| File | What it holds |
| --- | --- |
| `/swiss/settings/global.ini` | Every setting that isn't per game. Read once, when Swiss starts. |
| `/swiss/settings/game/<ID4>.ini` | One game's own settings. `<ID4>` is the first four characters of the game ID, such as `GALE` for Super Smash Bros. Melee (NTSC, `GALE01`). Read when you pick that game. |

Every disc and revision that shares those four characters shares one file.

## Writing a file

- One setting per line, as `Key=Value`. Lines starting with `#` are ignored.
  Windows and Unix line endings both work.
- Keys and values are case-sensitive and must match this page exactly. Nothing
  is trimmed, so `Key = Value` with spaces doesn't match anything.
- A key you leave out keeps its default. A key Swiss doesn't know is ignored.
- **On/off settings:** only `Yes` turns a setting on. Anything else, including
  `yes`, `1`, `true` or a typo, turns it off.
- **Settings with a list of values:** a value that isn't in the list is
  ignored, and the setting keeps its default.
- Numbers are whole numbers. Text is used as written, up to the field's length.
- When you save settings on the console, Indigo keeps what you wrote. Your
  comments, blank lines and keys it doesn't know stay where they are, and each
  known key gets its new value in place. Keys missing from `global.ini` are
  added before its `#!!Swiss Settings End!!` line, or at the end. Old key
  names are replaced by their current ones. A game's file keeps only the
  settings that differ from the defaults. Upstream Swiss still rewrites the
  whole file and drops comments if you boot it.
- Network passwords are stored as plain text.

## Examples

[`examples/global.ini`](examples/global.ini) sets a few common options, and
[`examples/game/GALE.ini`](examples/game/GALE.ini) gives one game its own
video mode and polling rate. Copy them to the paths above and edit them.

```ini
IGRType=Reboot
AutoCheats=Yes
GameBrowserType=Carousel
Disable Menu Music=Yes
```

## What Swiss uses when a setting is in more than one place

Swiss starts from its built-in defaults. Each source below overrides the ones
before it:

1. the console's SRAM (sound, video, screen position, language, boot mode);
2. the settings your loader passes to Swiss (video cable type, language);
3. `global.ini`;
4. arguments passed to `swiss.dol`, if your loader supports them. They use the
   same `Key=Value` form and last until you restart.

Swiss then writes the SRAM values back to the console.

## Keys in global.ini

The tables follow the console's layout: Quick, Game Defaults, and the six
Setup sections. The last column names the row on that screen.

### Quick

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `Disable PAD Rumble` | `Yes`, `No`. `Yes` shows as Controller Rumble › Off. | `No` | Controller Rumble |
| `Disable UI Animations` | `Yes`, `No`. `Yes` is UI Motion › Off. | `No` | UI Motion |
| `Reduce UI Animations` | `Yes`, `No`. `Yes` with `Disable UI Animations=No` is UI Motion › Reduced. | `No` | UI Motion |
| `Disable Menu Music` | `Yes`, `No`. `Yes` shows as Menu Music › Off. | `No` | Menu Music |
| `Disable Menu SFX` | `Yes`, `No`. `Yes` shows as Menu Sounds › Off. | `No` | Menu Sounds |
| `AutoBoot` | `Yes`, `No` | `No` | Boot without prompts |
| `IGRType` | `Disabled`, `Reboot`, `Apploader` | `Disabled` | In-Game Reset |
| `BS2Boot` | `No`, `Yes`, `Sound 1`, `Sound 2` | `No` | Load GameCube Main Menu |
| `Emulate Memory Card` | `Yes`, `No`. Applies to every game; there is no per-game version. | `No` | Emulate Memory Card |
| `AutoCheats` | `Yes`, `No` | `No` | Auto-load cheats |

### Game Defaults

What every game starts with. A game's own file overrides each of them, and
most use the same key there.

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `Force NTSC Video Mode` | `Auto`, `480i`, `480sf`, `240p`, `960i`, `480p`, `1080i60`, `540p60`. For games that aren't PAL; 50 Hz modes are ignored here. | `Auto` | Force NTSC Video Mode |
| `Force PAL Video Mode` | `Auto`, `480i`, `480sf`, `240p`, `960i`, `480p`, `1080i60`, `540p60`, `576i`, `576sf`, `288p`, `1152i`, `576p`, `1080i50`, `540p50`. For PAL games. | `Auto` | Force PAL Video Mode |
| `Force Horizontal Scale` | `Auto`, `1:1`, `11:10`, `9:8`, `640px`, `656px`, `672px`, `704px`, `720px` | `Auto` | Force Horizontal Scale |
| `Force Vertical Offset` | A signed number. **This one doesn't reach games:** a game starts at `-3` with GCVideo or GCDigital and `+0` otherwise. Set it per game instead. | `+0` | Not shown, since it does nothing |
| `Force Vertical Filter` | `Auto`, `0`, `1`, `2` | `Auto` | Force Vertical Filter |
| `Force Field Rendering` | `Auto`, `On`, `Off`, `TAA` | `Auto` | Force Field Rendering |
| `Fix Pixel Center` | `No`, `1/24`, `1/12` | `No` | Fix Pixel Center |
| `Disable Alpha Dithering` | `Yes`, `No`. `Yes` shows as Alpha Dithering › Off. | `No` | Alpha Dithering |
| `Force Anisotropic Filter` | `Yes`, `No` | `No` | Force Anisotropic Filter |
| `Force Widescreen` | `No`, `3D`, `2D+3D` | `No` | Force Widescreen |
| `Force Polling Rate` | `No`, `VSync`, `1000Hz`, `500Hz`, `350Hz`, `300Hz`, `250Hz`, `200Hz`, `150Hz`, `120Hz`, `100Hz`. The console lists 150 Hz and 120 Hz twice; a file can only choose the first of each. | `No` | Force Polling Rate |
| `Invert Camera Stick` | `No`, `X`, `Y`, `X&Y` | `No` | Invert Camera Stick |
| `Swap Camera Stick` | `No`, `X`, `Y`, `X&Y` | `No` | Swap Camera Stick |
| `Digital Trigger Level` | `0` to `200`; the console steps by 10. | `0` | Digital Trigger Level |
| `Emulate Audio Streaming` | `Off`, `Auto`, `On` | `Auto` | Emulate Audio Streaming |
| `Emulate Read Speed` | `No`, `Yes`, `Wii` | `No` | Emulate Read Speed |
| `Emulate Broadband Adapter` | `Yes`, `No` | `No` | Emulate Broadband Adapter |
| `Disable Memory Card` | `No`, `Slot A`, `Slot B` | `No` | Disable Memory Card |
| `Disable Hypervisor` | `Yes`, `No`. `Yes` shows as Hypervisor › Off. | `No` | Hypervisor |
| `Prefer Clean Boot` | `Yes`, `No` | `No` | Prefer Clean Boot |
| `RetroTINK-4K Profile` | `0` to `12`. Also the profile used in the menus. | `0` | RetroTINK-4K Profile |

### Setup › Display

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `System Video` | `NTSC`, `PAL`, `PAL-M`. Ignored on a Wii (AVE-RVL). | the console's region | System Video |
| `Screen Position` | A signed number, such as `+0` or `-2`. | the console's SRAM | Screen Position |
| `Swiss Video Mode` | `Auto`, `480i`, `480sf`, `480p`, `576i`, `576sf`, `576p` | `Auto` | Swiss Video Mode |
| `AVECompat` | `AVE N-DOL`, `AVE P-DOL`, `CMPV-DOL`, `GCDigital`, `GCVideo`, `AVE-RVL` | `GCVideo`, or what your loader reports | AVE Compatibility |
| `Force DTV Status` | `No`, `Yes`, `Region Switch` | `No` | Force DTV Status |
| `RT4KOptim` | `Yes`, `No` | `No` | RetroTINK-4K HDMI Input |
| `Disable Video Patches` | `None`, `Game`, `All` | `None` | Disable Video Patches |
| `Force Video Active` | `Yes`, `No` | `No` | Force Video Active |
| `Pause for resolution change` | `Yes`, `No` | `No` | Pause for resolution change |

### Setup › Console

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `Menu Color` | `Indigo`, `Azure`, `Emerald`, `Gold`, `Spice`, `Crimson`, `Rose`, `Jet Black` | `Indigo` | Menu Color |
| `Library Icon` | `Controller`, `Books`, `Hub`, `Disc`, `Sliders`, `Gear`, `Clock`, `None`. The picture on the cube's Library face. Any face can show any of them. | `Controller` | Library Icon |
| `Source Icon` | `Controller`, `Books`, `Hub`, `Disc`, `Sliders`, `Gear`, `Clock`, `None`. The picture on the Source face. | `Hub` | Source Icon |
| `Settings Icon` | `Controller`, `Books`, `Hub`, `Disc`, `Sliders`, `Gear`, `Clock`, `None`. The picture on the Settings face. | `Sliders` | Settings Icon |
| `System Icon` | `Controller`, `Books`, `Hub`, `Disc`, `Sliders`, `Gear`, `Clock`, `None`. The picture on the System face. | `Clock` | System Icon |
| `System Boot Mode` | `Default`, `Production`. Any other value means `Default`. | the console's SRAM | System Boot Mode |
| `System Sound` | `Mono`, `Stereo`. Any other value means `Mono`. | the console's SRAM | System Sound |
| `System Language` | `English`, `German`, `French`, `Spanish`, `Italian`, `Dutch`, `Japanese`, `English (US)` | the console's SRAM | System Language |
| `Disable PAD Recalibration` | `Yes`, `No`. `Yes` shows as Controller Recalibration › Off. | `No` | Controller Recalibration |

### Setup › Storage

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `Init DVD Drive at startup` | `Yes`, `No` | `No` | Init DVD Drive at startup |
| `Stop DVD Drive motor` | `Yes`, `No` | `No` | Stop DVD Drive motor |
| `Configure Audio Buffer` | `Off`, `Auto`, `On` | `On` | Configure Audio Buffer |
| `SD/IDE Speed` | `32MHz` (shown as 27 MHz), `16MHz` (shown as 13.5 MHz). Any other value, including `27MHz`, means `16MHz`. | `32MHz` | SD/IDE-EXI Speed |
| `Disable MemCard PRO GameID` | `No`, `Slot A`, `Slot B`, `Slot A&B` | `No` | Disable MemCard PRO GameID |

### Setup › Network

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `InitNetwork` | `Yes`, `No` | `No` | Init network at startup |
| `BBAUseDHCP` | `Yes`, `No` | `Yes` | IPv4 uses DHCP |
| `BBALocalIP` | An IPv4 address. | empty | IPv4 Address |
| `BBANetmask` | The prefix length, such as `24`. | `0` | IPv4 Netmask |
| `BBAGateway` | An IPv4 address. | empty | IPv4 Gateway |
| `FSPHostIP` | An IPv4 address. | empty | FSP Host IP |
| `FSPPort` | A port number. | `21` | FSP Port |
| `FSPPassword` | Text, stored as written. | empty | FSP Password |
| `FSPPathMTU` | `576` to `2030`; anything else uses `1500`. | `1500` | FSP Path MTU |
| `FTPHostIP` | An IPv4 address. | empty | FTP Host IP |
| `FTPPort` | A port number. | `21` | FTP Port |
| `FTPUserName` | Text. | empty | FTP Username |
| `FTPPassword` | Text, stored as written. | empty | FTP Password |
| `FTPUsePasv` | `Yes`, `No` | `No` | FTP PASV Mode |
| `SMBHostIP` | An IPv4 address. | empty | SMB Host IP |
| `SMBShareName` | Text. | empty | SMB Share |
| `SMBUserName` | Text. | empty | SMB Username |
| `SMBPassword` | Text, stored as written. | empty | SMB Password |
| `RT4KHostIP` | An IPv4 address. | empty | RetroTINK-4K Host IP |
| `RT4KPort` | A port number. | `0` | RetroTINK-4K Port |

### Setup › Library

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `FileBrowserType` | `Standard`, `Fullwidth`, `Carousel` | `Standard` | File Browser Type |
| `AppsBrowserType` | `Standard`, `Fullwidth`, `Carousel` | `Fullwidth` | File Browser Type for apps |
| `GameBrowserType` | `Standard`, `Fullwidth`, `Carousel` | `Fullwidth` | File Browser Type for games |
| `Enable File Management` | `Yes`, `No` | `No` | File Management |
| `RecentListLevel` | `Off`, `Lazy`, `On` | `On` | Recent List |
| `ShowHiddenFiles` | `Yes`, `No` | `No` | Show hidden files |
| `Hide Unknown file types` | `Yes`, `No` | `No` | Hide unknown file types |
| `Disable Panel Transparency` | `Yes`, `No`. `Yes` shows as Panel Transparency › Off. | `No` | Panel Transparency |
| `Disable Animated Backdrop` | `Yes`, `No`. `Yes` shows as Animated Backdrop › Off. | `No` | Animated Backdrop |
| `FlattenDir` | A folder pattern, such as `*/games`. | `*/games` | Flatten directory |

### Setup › Developer

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `Enable USB Gecko` | `No`, `Slot A`, `Slot B`, `Serial Port 2` | `No`; set automatically when a USB Gecko is connected | Enable USB Gecko |
| `Wait for USB Gecko` | `Yes`, `No` | `No` | Wait for USB Gecko |
| `Simulated MRAM Size` | `None`, `16 MiB`, `24 MiB`, `32 MiB`, `48 MiB`, `64 MiB`. A size larger than the console's memory is ignored. | `None` | Simulated MRAM Size |
| `Enable WiiRD debug` | `Yes`, `No` | `No` | WiiRD debugging |

### Old names Swiss still reads

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `Enable Debug` | Old name, still read: `Yes` means `Enable USB Gecko=Slot B`. |  |  |
| `USB Gecko debug output` | Old name for `Enable USB Gecko`, still read. |  |  |
| `Stop DVD Motor on startup` | Old name for `Stop DVD Drive motor`, still read. |  |  |

### Written by Swiss, not shown in Settings

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `Last DTV Status` | `Yes`, `No`. Swiss writes the cable it detected. Leave it out: if it doesn't match the cable at boot, Swiss opens Settings at AVE Compatibility. |  |  |
| `Autoload` | The path of the game to start automatically. Set with Z on a game's detail screen, which saves only this key. | empty |  |
| `GCLoaderHWVersion` | A number Swiss uses to remind you about GC Loader firmware updates. | `0` |  |
| `GCLoaderTopVersion` | Text Swiss uses to remind you about GC Loader firmware updates. | empty |  |

## Keys in a game's file

A game's file only needs the settings that should differ from the game
defaults above. The game-defaults keys that aren't listed here work the same
way in a game's file.

| Key | Values | Default |
| --- | --- | --- |
| `ID` | Written by Swiss, not read: the file name decides the game. | |
| `Name` | The game's name. | |
| `Comment` | Text, not shown anywhere. Reset to defaults keeps it. | `No Comment` |
| `Status` | Text, not shown anywhere. Reset to defaults keeps it. | `Unknown` |
| `Game Language` | `English`, `German`, `French`, `Spanish`, `Italian`, `Dutch`, `Japanese`, `English (US)`, `Default`. `Default` uses the System Language. | `Default` |
| `Force Video Mode` | `Auto`, `480i`, `480sf`, `240p`, `960i`, `480p`, `1080i60`, `540p60`, `576i`, `576sf`, `288p`, `1152i`, `576p`, `1080i50`, `540p50` | `Force NTSC Video Mode` or `Force PAL Video Mode`, by the game's region |
| `Force Horizontal Scale` | `Auto`, `1:1`, `11:10`, `9:8`, `640px`, `656px`, `672px`, `704px`, `720px` | from global.ini |
| `Force Vertical Offset` | A signed number, such as `-3` or `+2`. | `-3` with GCVideo or GCDigital, otherwise `+0` |
| `Force Vertical Filter` | `Auto`, `0`, `1`, `2` | from global.ini |
| `Force Field Rendering` | `Auto`, `On`, `Off`, `TAA` | from global.ini |
| `Fix Pixel Center` | `No`, `1/24`, `1/12` | from global.ini |
| `Disable Alpha Dithering` | `Yes`, `No`. `Yes` shows as Alpha Dithering › Off. | from global.ini |
| `Force Anisotropic Filter` | `Yes`, `No` | from global.ini |
| `Force Widescreen` | `No`, `3D`, `2D+3D` | from global.ini |
| `Force Polling Rate` | `No`, `VSync`, `1000Hz`, `500Hz`, `350Hz`, `300Hz`, `250Hz`, `200Hz`, `150Hz`, `120Hz`, `100Hz` | from global.ini |
| `Invert Camera Stick` | `No`, `X`, `Y`, `X&Y` | from global.ini |
| `Swap Camera Stick` | `No`, `X`, `Y`, `X&Y` | from global.ini |
| `Digital Trigger Level` | `0` to `200` | from global.ini |
| `Emulate Audio Streaming` | `Off`, `Auto`, `On` | from global.ini |
| `Emulate Read Speed` | `No`, `Yes`, `Wii` | from global.ini |
| `Emulate Broadband Adapter` | `Yes`, `No` | from global.ini |
| `Disable Memory Card` | `No`, `Slot A`, `Slot B` | from global.ini |
| `Disable Hypervisor` | `Yes`, `No`. `Yes` shows as Hypervisor › Off. | from global.ini |
| `Prefer Clean Boot` | `Yes`, `No` | from global.ini |
| `RetroTINK-4K Profile` | `0` to `12` | from global.ini |

For some games, Swiss starts from its own fix instead of global.ini:
Fix Pixel Center, Digital Trigger Level, Emulate Audio Streaming, Emulate Read
Speed or Emulate Broadband Adapter. A game's file still overrides the fix.

## Settings a file can't set

- **Configuration Device** and **CPU Temperature Calibration** live only in
  the console's SRAM. Set them on the console.
- The **Reset to defaults** rows are actions, not settings.
