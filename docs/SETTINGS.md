# Settings files

Indigo keeps Swiss's settings files exactly as Swiss writes them, so a card
works with either. You can write them on a computer before you boot, which is
the quickest way to set up a new card or to change settings you only set once.

Every key and value on this page is checked automatically against the code
that reads and writes these files.

## Where the files go

Swiss keeps its settings on the **configuration device**, which is usually the
SD card (SD2SP2, SD Gecko) or a GC Loader or other drive replacement. Choose it
on the console in Settings › System › Configuration Device. The choice is
stored in the console's SRAM, not in a file. Until one is chosen, Swiss uses
the first device it finds that can hold settings.

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
- When you save settings on the console, Swiss rewrites the whole of
  `global.ini` from memory. Comments and unknown keys are lost, and every key
  is written out. A game's file is rewritten with only the settings that
  differ from the defaults.
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

The last column says where the setting appears in Settings on the console.

### System

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `System Boot Mode` | `Default`, `Production`. Any other value means `Default`. | the console's SRAM | System › System Boot Mode |
| `System Sound` | `Mono`, `Stereo`. Any other value means `Mono`. | the console's SRAM | System › System Sound |
| `System Video` | `NTSC`, `PAL`, `PAL-M`. Ignored on a Wii (AVE-RVL). | the console's region | System › System Video |
| `Screen Position` | A signed number, such as `+0` or `-2`. | the console's SRAM | System › Screen Position |
| `System Language` | `English`, `German`, `French`, `Spanish`, `Italian`, `Dutch`, `Japanese`, `English (US)` | the console's SRAM | System › System Language |
| `Swiss Video Mode` | `Auto`, `480i`, `480sf`, `480p`, `576i`, `576sf`, `576p` | `Auto` | System › Swiss Video Mode |
| `Init DVD Drive at startup` | `Yes`, `No` | `No` | System › Init DVD Drive at startup |
| `Stop DVD Drive motor` | `Yes`, `No` | `No` | System › Stop DVD Drive motor |
| `Configure Audio Buffer` | `Off`, `Auto`, `On` | `On` | System › Configure Audio Buffer |
| `SD/IDE Speed` | `32MHz` (shown as 27 MHz), `16MHz` (shown as 13.5 MHz). Any other value, including `27MHz`, means `16MHz`. | `32MHz` | System › SD/IDE-EXI Speed |
| `AVECompat` | `AVE N-DOL`, `AVE P-DOL`, `CMPV-DOL`, `GCDigital`, `GCVideo`, `AVE-RVL` | `GCVideo`, or what your loader reports | System › AVE Compatibility |
| `Force DTV Status` | `No`, `Yes`, `Region Switch` | `No` | System › Force DTV Status |
| `RT4KOptim` | `Yes`, `No` | `No` | System › RetroTINK-4K HDMI Input |
| `Disable PAD Recalibration` | `Yes`, `No` | `No` | System › Disable Controller Recalibration |
| `Disable PAD Rumble` | `Yes`, `No` | `No` | System › Disable Controller Rumble |
| `Enable USB Gecko` | `No`, `Slot A`, `Slot B`, `Serial Port 2` | `No`; set automatically when a USB Gecko is connected | System › Enable USB Gecko |
| `Wait for USB Gecko` | `Yes`, `No` | `No` | System › Wait for USB Gecko |
| `Simulated MRAM Size` | `None`, `16 MiB`, `24 MiB`, `32 MiB`, `48 MiB`, `64 MiB`. A size larger than the console's memory is ignored. | `None` | System › Simulated MRAM Size |
| `Enable Debug` | Old name, still read: `Yes` means `Enable USB Gecko=Slot B`. | | |
| `USB Gecko debug output` | Old name for `Enable USB Gecko`, still read. | | |
| `Stop DVD Motor on startup` | Old name for `Stop DVD Drive motor`, still read. | | |

### Interface

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `FileBrowserType` | `Standard`, `Fullwidth`, `Carousel` | `Standard` | Interface › File Browser Type |
| `AppsBrowserType` | `Standard`, `Fullwidth`, `Carousel` | `Fullwidth` | Interface › File Browser Type for apps |
| `GameBrowserType` | `Standard`, `Fullwidth`, `Carousel` | `Fullwidth` | Interface › File Browser Type for games |
| `Enable File Management` | `Yes`, `No` | `No` | Interface › File Management |
| `RecentListLevel` | `Off`, `Lazy`, `On` | `On` | Interface › Recent List |
| `ShowHiddenFiles` | `Yes`, `No` | `No` | Interface › Show hidden files |
| `Hide Unknown file types` | `Yes`, `No` | `No` | Interface › Hide unknown file types |
| `Disable UI Animations` | `Yes`, `No`. `Yes` is UI Motion › Off. | `No` | Interface › UI Motion |
| `Reduce UI Animations` | `Yes`, `No`. `Yes` with `Disable UI Animations=No` is UI Motion › Reduced. | `No` | Interface › UI Motion |
| `Disable Panel Transparency` | `Yes`, `No`. `Yes` shows as Panel Transparency › No. | `No` | Interface › Panel Transparency |
| `Disable Animated Backdrop` | `Yes`, `No`. `Yes` shows as Animated Backdrop › No. | `No` | Interface › Animated Backdrop |
| `Disable Menu Music` | `Yes`, `No`. `Yes` shows as Menu Music › No. | `No` | Interface › Menu Music |
| `Disable Menu SFX` | `Yes`, `No`. `Yes` shows as Menu Sounds › No. | `No` | Interface › Menu Sounds |
| `AutoBoot` | `Yes`, `No` | `No` | Interface › Boot without prompts |
| `FlattenDir` | A folder pattern, such as `*/games`. | `*/games` | Interface › Flatten directory |

### Network

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `InitNetwork` | `Yes`, `No` | `No` | Network › Init network at startup |
| `BBAUseDHCP` | `Yes`, `No` | `Yes` | Network › IPv4 uses DHCP |
| `BBALocalIP` | An IPv4 address. | empty | Network › IPv4 Address |
| `BBANetmask` | The prefix length, such as `24`. | `0` | Network › IPv4 Netmask |
| `BBAGateway` | An IPv4 address. | empty | Network › IPv4 Gateway |
| `FSPHostIP` | An IPv4 address. | empty | Network › FSP Host IP |
| `FSPPort` | A port number. | `21` | Network › FSP Port |
| `FSPPassword` | Text, stored as written. | empty | Network › FSP Password |
| `FSPPathMTU` | `576` to `2030`; anything else uses `1500`. | `1500` | Network › FSP Path MTU |
| `FTPHostIP` | An IPv4 address. | empty | Network › FTP Host IP |
| `FTPPort` | A port number. | `21` | Network › FTP Port |
| `FTPUserName` | Text. | empty | Network › FTP Username |
| `FTPPassword` | Text, stored as written. | empty | Network › FTP Password |
| `FTPUsePasv` | `Yes`, `No` | `No` | Network › FTP PASV Mode |
| `SMBHostIP` | An IPv4 address. | empty | Network › SMB Host IP |
| `SMBShareName` | Text. | empty | Network › SMB Share |
| `SMBUserName` | Text. | empty | Network › SMB Username |
| `SMBPassword` | Text, stored as written. | empty | Network › SMB Password |
| `RT4KHostIP` | An IPv4 address. | empty | Network › RetroTINK-4K Host IP |
| `RT4KPort` | A port number. | `0` | Network › RetroTINK-4K Port |

### Game

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `IGRType` | `Disabled`, `Reboot`, `Apploader` | `Disabled` | Game › In-Game Reset |
| `BS2Boot` | `No`, `Yes`, `Sound 1`, `Sound 2` | `No` | Game › Load GameCube Main Menu |
| `Emulate Memory Card` | `Yes`, `No`. Applies to every game; there is no per-game version. | `No` | Game › Emulate Memory Card |
| `Disable MemCard PRO GameID` | `No`, `Slot A`, `Slot B`, `Slot A&B` | `No` | Game › Disable MemCard PRO GameID |
| `Disable Video Patches` | `None`, `Game`, `All` | `None` | Game › Disable Video Patches |
| `Force Video Active` | `Yes`, `No` | `No` | Game › Force Video Active |
| `Pause for resolution change` | `Yes`, `No` | `No` | Game › Pause for resolution change |
| `AutoCheats` | `Yes`, `No` | `No` | Game › Auto-load cheats |
| `Enable WiiRD debug` | `Yes`, `No` | `No` | Game › WiiRD debugging |

### Game defaults

These are what every game starts with. A game's own file overrides each of
them. Most have the same key as in a game's file.

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `Force NTSC Video Mode` | `Auto`, `480i`, `480sf`, `240p`, `960i`, `480p`, `1080i60`, `540p60`. For games that aren't PAL; 50 Hz modes are ignored here. | `Auto` | Defaults › Force NTSC Video Mode |
| `Force PAL Video Mode` | `Auto`, `480i`, `480sf`, `240p`, `960i`, `480p`, `1080i60`, `540p60`, `576i`, `576sf`, `288p`, `1152i`, `576p`, `1080i50`, `540p50`. For PAL games. | `Auto` | Defaults › Force PAL Video Mode |
| `Force Horizontal Scale` | `Auto`, `1:1`, `11:10`, `9:8`, `640px`, `656px`, `672px`, `704px`, `720px` | `Auto` | Defaults › Force Horizontal Scale |
| `Force Vertical Offset` | A signed number. **This one doesn't reach games:** a game starts at `-3` with GCVideo or GCDigital and `+0` otherwise. Set it per game instead. | `+0` | Defaults › Force Vertical Offset |
| `Force Vertical Filter` | `Auto`, `0`, `1`, `2` | `Auto` | Defaults › Force Vertical Filter |
| `Force Field Rendering` | `Auto`, `On`, `Off`, `TAA` | `Auto` | Defaults › Force Field Rendering |
| `Fix Pixel Center` | `No`, `1/24`, `1/12` | `No` | Defaults › Fix Pixel Center |
| `Disable Alpha Dithering` | `Yes`, `No` | `No` | Defaults › Disable Alpha Dithering |
| `Force Anisotropic Filter` | `Yes`, `No` | `No` | Defaults › Force Anisotropic Filter |
| `Force Widescreen` | `No`, `3D`, `2D+3D` | `No` | Defaults › Force Widescreen |
| `Force Polling Rate` | `No`, `VSync`, `1000Hz`, `500Hz`, `350Hz`, `300Hz`, `250Hz`, `200Hz`, `150Hz`, `120Hz`, `100Hz`. The console lists 150 Hz and 120 Hz twice; a file can only choose the first of each. | `No` | Defaults › Force Polling Rate |
| `Invert Camera Stick` | `No`, `X`, `Y`, `X&Y` | `No` | Defaults › Invert Camera Stick |
| `Swap Camera Stick` | `No`, `X`, `Y`, `X&Y` | `No` | Defaults › Swap Camera Stick |
| `Digital Trigger Level` | `0` to `200`; the console steps by 10. | `0` | Defaults › Digital Trigger Level |
| `Emulate Audio Streaming` | `Off`, `Auto`, `On` | `Auto` | Defaults › Emulate Audio Streaming |
| `Emulate Read Speed` | `No`, `Yes`, `Wii` | `No` | Defaults › Emulate Read Speed |
| `Emulate Broadband Adapter` | `Yes`, `No` | `No` | Defaults › Emulate Broadband Adapter |
| `Disable Memory Card` | `No`, `Slot A`, `Slot B` | `No` | Defaults › Disable Memory Card |
| `Disable Hypervisor` | `Yes`, `No` | `No` | Defaults › Disable Hypervisor |
| `Prefer Clean Boot` | `Yes`, `No` | `No` | Defaults › Prefer Clean Boot |
| `RetroTINK-4K Profile` | `0` to `12`. Also the profile used in the menus. | `0` | Defaults › RetroTINK-4K Profile |

### Written by Swiss, not shown in Settings

| Key | Values | Default | On screen |
| --- | --- | --- | --- |
| `Last DTV Status` | `Yes`, `No`. Swiss writes the cable it detected. Leave it out: if it doesn't match the cable at boot, Swiss opens Settings at AVE Compatibility. | | |
| `Autoload` | The path of the game to start automatically. Set with Z on a game's detail screen. | empty | |
| `GCLoaderHWVersion` | A number Swiss uses to remind you about GC Loader firmware updates. | `0` | |
| `GCLoaderTopVersion` | Text Swiss uses to remind you about GC Loader firmware updates. | empty | |

## Keys in a game's file

A game's file only needs the settings that should differ from the game
defaults above. The game-defaults keys that aren't listed here work the same
way in a game's file.

| Key | Values | Default |
| --- | --- | --- |
| `ID` | Written by Swiss, not read: the file name decides the game. | |
| `Name` | The game's name. | |
| `Comment` | Text, not shown anywhere. | `No Comment` |
| `Status` | Text, not shown anywhere. | `Unknown` |
| `Game Language` | `English`, `German`, `French`, `Spanish`, `Italian`, `Dutch`, `Japanese`, `English (US)`, `Default`. `Default` uses the System Language. | `Default` |
| `Force Video Mode` | `Auto`, `480i`, `480sf`, `240p`, `960i`, `480p`, `1080i60`, `540p60`, `576i`, `576sf`, `288p`, `1152i`, `576p`, `1080i50`, `540p50` | `Force NTSC Video Mode` or `Force PAL Video Mode`, by the game's region |
| `Force Horizontal Scale` | `Auto`, `1:1`, `11:10`, `9:8`, `640px`, `656px`, `672px`, `704px`, `720px` | from global.ini |
| `Force Vertical Offset` | A signed number, such as `-3` or `+2`. | `-3` with GCVideo or GCDigital, otherwise `+0` |
| `Force Vertical Filter` | `Auto`, `0`, `1`, `2` | from global.ini |
| `Force Field Rendering` | `Auto`, `On`, `Off`, `TAA` | from global.ini |
| `Fix Pixel Center` | `No`, `1/24`, `1/12` | from global.ini |
| `Disable Alpha Dithering` | `Yes`, `No` | from global.ini |
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
| `Disable Hypervisor` | `Yes`, `No` | from global.ini |
| `Prefer Clean Boot` | `Yes`, `No` | from global.ini |
| `RetroTINK-4K Profile` | `0` to `12` | from global.ini |

For some games, Swiss starts from its own fix instead of global.ini:
Fix Pixel Center, Digital Trigger Level, Emulate Audio Streaming, Emulate Read
Speed or Emulate Broadband Adapter. A game's file still overrides the fix.

## Settings a file can't set

- **Configuration Device** and **CPU Temperature Calibration** live only in
  the console's SRAM. Set them on the console.
- The **Reset to defaults** rows are actions, not settings.
