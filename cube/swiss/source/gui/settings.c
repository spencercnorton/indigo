#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <gccore.h>
#include <ogc/exi.h>
#include <ogc/machine/processor.h>
#include "deviceHandler.h"
#include "FrameBufferMagic.h"
#include "IPLFontWrite.h"
#include "swiss.h"
#include "main.h"
#include "info.h"
#include "config.h"
#include "settings.h"
#include "menuaudio.h"
#include "exi.h"
#include "bba.h"
#include "sram.h"
#include "rt4k.h"
#include "input.h"
#include "ui_motion.h"
#include "ui_settings_layout.h"

/* Bind the pure layout module's page model to the real option enums: a
 * drifting row count or action index becomes a compile error here, not a
 * silently wrong highlight. */
_Static_assert(UI_SETLAYOUT_PAGE_COUNT == VIEW_GAME + 1, "view count drift");
_Static_assert(UI_SETLAYOUT_TAB_COUNT == VIEW_SETUP + 1, "tab count drift");
_Static_assert(UI_SETLAYOUT_TAB_SETUP == VIEW_SETUP, "Setup tab drift");
_Static_assert(UI_SETLAYOUT_ROWS_GAME_DEFAULTS == SET_DEFAULT_DEFAULTS + 1, "defaults rows drift");
_Static_assert(UI_SETLAYOUT_ROWS_NETWORK == SET_RT4K_PORT + 1, "network rows drift");
_Static_assert(UI_SETLAYOUT_ROWS_GAME == SET_DEFAULTS + 1, "game rows drift");
_Static_assert(UI_SETLAYOUT_ROWS_SETUP == VIEW_DEVELOPER - VIEW_DISPLAY + 1, "Setup sections drift");
_Static_assert((int)UI_SETLAYOUT_MOTION_FULL == (int)UI_MOTION_FULL,
	"Full motion enum drift");
_Static_assert((int)UI_SETLAYOUT_MOTION_REDUCED == (int)UI_MOTION_REDUCED,
	"Reduced motion enum drift");
_Static_assert((int)UI_SETLAYOUT_MOTION_OFF == (int)UI_MOTION_OFF,
	"Off motion enum drift");

/* Text sizes for the settings shell. */
#define set_title_size (0.92f)
#define set_subtitle_size (0.60f)
#define set_tab_size (0.60f)
#define set_row_size (0.72f)
#define set_row_sel_size (0.78f)
#define set_meta_size (0.6f)

ConfigEntry tempConfig;
SwissSettings tempSettings;
char *enableUSBGeckoStr[] = {"No", "Slot A", "Slot B", "Serial Port 2"};
char *simulatedMemSizeStr[] = {"None", "16 MiB", "24 MiB", "32 MiB", "48 MiB", "64 MiB"};
char *uiVModeStr[] = {"Auto", "480i", "480sf", "480p", "576i", "576sf", "576p"};
char *gameVModeStr[] = {"Auto", "480i", "480sf", "240p", "960i", "480p", "1080i60", "540p60", "576i", "576sf", "288p", "1152i", "576p", "1080i50", "540p50"};
char *forceHScaleStr[] = {"Auto", "1:1", "11:10", "9:8", "640px", "656px", "672px", "704px", "720px"};
char *forceVFilterStr[] = {"Auto", "0", "1", "2"};
char *forceVJitterStr[] = {"Auto", "On", "Off", "TAA"};
char *fixPixelCenterStr[] = {"No", "1/24", "1/12"};
char *forceWidescreenStr[] = {"No", "3D", "2D+3D"};
char *forcePollRateStr[] = {"No", "VSync", "1000Hz", "500Hz", "350Hz", "300Hz", "250Hz", "200Hz", "150Hz", "150Hz", "120Hz", "120Hz", "100Hz"};
char *invertCStickStr[] = {"No", "X", "Y", "X&Y"};
char *swapCStickStr[] = {"No", "X", "Y", "X&Y"};
char *configAudioBufferStr[] = {"Off", "Auto", "On"};
char *disableMCPGameIDStr[] = {"No", "Slot A", "Slot B", "Slot A&B"};
char *disableVideoPatchesStr[] = {"None", "Game", "All"};
char *forceDTVStatusStr[] = {"No", "Yes", "Region Switch"};
char *emulateAudioStreamStr[] = {"Off", "Auto", "On"};
char *emulateReadSpeedStr[] = {"No", "Yes", "Wii"};
char *disableMemoryCardStr[] = {"No", "Slot A", "Slot B"};
char *sramLanguageStr[] = {"English", "German", "French", "Spanish", "Italian", "Dutch", "Japanese", "English (US)", "Default"};
char *sramVideoStr[] = {"NTSC", "PAL", "PAL-M"};
char *igrTypeStr[] = {"Disabled", "Reboot", "Apploader"};
char *aveCompatStr[] = {"AVE N-DOL", "AVE P-DOL", "CMPV-DOL", "GCDigital", "GCVideo", "AVE-RVL"};
char *fileBrowserTypeStr[] = {"Standard", "Fullwidth", "Carousel"};
char *bs2BootStr[] = {"No", "Yes", "Sound 1", "Sound 2"};
char *recentListLevelStr[] = {"Off", "Lazy", "On"};
char *uiColorStr[] = {"Indigo", "Azure", "Emerald", "Gold", "Spice", "Crimson", "Rose", "Jet Black"};
_Static_assert(sizeof(uiColorStr) / sizeof(uiColorStr[0]) == UI_COLOR_MAX, "Menu Color names drift");
static const char *uiMotionModeStr[] = {"Full", "Reduced", "Off"};

const int simulatedMemSizeInt[] = {
	0,
	16 << 20,
	24 << 20,
	32 << 20,
	48 << 20,
	64 << 20
};

static char *tooltips_global[PAGE_GLOBAL_MAX+1] = {
	[SET_SYS_BOOTMODE] = "System Boot Mode:\n\nSets development or production mode on development hardware.\nOn retail hardware with GC Loader or PicoLoader, the default\nskips the GameCube logo screen.",
	[SET_SYS_SOUND] = "System Sound:\n\nSets the default audio output type used by most games",
	[SET_SYS_VIDEO] = "System Video:\n\nIntended to select between NTSC and PAL-M on DOL-002(BRA)",
	[SET_SCREEN_POS] = "Screen Position:\n\nAdjusts the horizontal screen position in games",
	[SET_SYS_LANG] = "System Language:\n\nSystem language used in games, primarily multi-5 PAL games",
	[SET_CONFIG_DEV] = "Configuration Device:\n\nThe device Indigo loads settings from and saves them to, in\nswiss/settings/global.ini. The choice is stored in SRAM, and it\nchanges only once Save & Exit has written the settings there.\nThe line under the Storage title says whether that file loaded.",
	[SET_INIT_DRIVE] = "Init DVD Drive at startup:\n\nDisabled - Leave it as-is (default)\nEnabled - Deassert reset signal when Swiss starts\n\nThis is necessary for the eject button to function on the\nPanasonic Q when Swiss is used as IPL replacement.",
	[SET_STOP_MOTOR] = "Stop DVD Drive motor:\n\nDisabled - Leave it as-is (default)\nEnabled - Stop the disc from spinning when Swiss starts\n\nThis option is mostly for users booting from game save exploits\nwhere the disc will already be spinning.",
	[SET_AUDIO_BUFFER] = "Configure Audio Buffer:\n\nOff - Disable audio streaming\nAuto - Enable audio streaming if the disc is known to use it\nOn - Enable audio streaming if the disc asks for it (default)\n\nThe audio buffer consumes a large portion of the GameCube\ndisc drive's read-ahead cache, lengthening load times.",
	[SET_EXI_SPEED] = "SD/IDE-EXI Speed:\n\nThe clock speed to try using on the EXI bus for SD cards and\nIDE-EXI devices. 27 MHz may not work with some SD cards or\nSD card adapters.",
	[SET_AVE_COMPAT] = "AVE Compatibility:\n\nSets the compatibility mode for the used audio/video encoder.\n\nAVE N-DOL - Output PAL as NTSC 50\nAVE P-DOL - Disable progressive scan mode\nCMPV-DOL - Enable 1080i & 540p\nGCDigital - Apply input filtering in OSD\nGCVideo - Apply general workarounds for GCVideo (default)\nAVE-RVL - Support 960i & 1152i without WiiVideo",
	[SET_FORCE_DTVSTATUS] = "Force DTV Status:\n\nDisabled - Use detect signal from the Digital AV Out (default)\nEnabled - Force detection in the case of a hardware fault\nRegion Switch - Ease transition between SD/ED TV setups",
	[SET_RT4K_OPTIM] = "RetroTINK-4K HDMI Input:\n\nFor GCDigital compatibility mode:\n Requires FX-Framework firmware version 3.9.46.178 or later\n and RetroTINK-4K firmware version 1.9.4 or later, and using\n DV1-Direct mode.\n\nFor GCVideo compatibility mode:\n Requires GCVideo-DVI firmware version 3.0 or later.",
	[SET_DISABLE_RECALIB] = "Controller Recalibration:\n\nOn - Controllers are recalibrated as usual (default)\nOff - Skips recalibration. This avoids problems with\nnon-compliant GameCube Controller implementations.",
	[SET_ENABLE_USBGECKO] = "Enable USB Gecko:\n\nIf a USB Gecko is present, messages output to the debug UART\nwill be redirected. When the USB host isn't actively reading from\nthe USB Gecko, it may cause the system to hang.\n\nwiiload is also made available for iterative development.",
	[SET_WAIT_USBGECKO] = "Wait for USB Gecko:\n\nWait for the transmit buffer to be read by the USB host when full.",
	[SET_SIMMEMSIZE] = "Simulated MRAM Size:\n\nLimits the amount of memory available on development hardware.",
	[SET_TAU_CALIB] = "CPU Temperature Calibration:\n\nOn a cold boot, adjust this value so that the CPU temperature\nreading in the title bar is around room temperature.\n\nThere is no factory calibration.",
	[SET_SWISS_VIDEOMODE] = "Swiss Video Mode:\n\nThe video mode Indigo's own screens use. Auto picks 480p with a\ndigital cable and otherwise follows the console's region.\n\nAfter a change, press A within 10 seconds to keep the new mode;\notherwise it changes back by itself.",
	[SET_DISABLE_RUMBLE] = "Controller Rumble:\n\nOn - Controllers can rumble in games (default)\nOff - Rumble is turned off in games"
};

static char *tooltips_interface[PAGE_INTERFACE_MAX+1] = {
	[SET_FILEBROWSER_TYPE] = "File Browser Type:\n\nStandard - Displays files with minimal detail (default)\n\nCarousel - Suited towards Game/DOL only use, consider combining\nthis option with the File Management setting turned off\nand Hide Unknown File Types turned on for a better experience.",
	[SET_APPSBROWSER_TYPE] = "File Browser Type for apps:\n\nApplicable to the /apps directory.",
	[SET_GAMEBROWSER_TYPE] = "File Browser Type for games:\n\nApplicable to the /games directory.",
	[SET_FILE_MGMT] = "File Management:\n\nWhen enabled, pressing Z on an entry in the file browser will\nallow it to be managed.",
	[SET_RECENT_LIST] = "Recent List:\n\n(On) - Press Start while browsing to show a recent list.\n(Lazy) - Same as On but list updates only for new entries.\n(Off) - Recent list is completely disabled.\n\nThe lazy/off options exist to minimise SD card writes.",
	[SET_HIDE_UNK] = "Hide unknown file types:\n\nDisabled - Show all files (default)\nEnabled - Hide unknown file types from being displayed\n\nKnown file types are:\n GameCube Executables (.bin/.dol/.elf)\n Disc images (.gcm/.iso/.nkit.iso/.tgc)\n MP3 Music (.mp3)\n WASP/WKF Flash files (.fzn)\n GameCube Memory Card files (.gci/.gcs/.sav)\n GameCube Executables with parameters appended (.dol+cli)",
	[SET_UI_ANIMS] = "UI Motion:\n\nFull - Calm spatial motion and ambient detail (default)\nReduced - Faster transitions without decorative travel\nOff - Menu elements move instantly\n\nMotion never delays input or changes the destination.",
	[SET_UI_COLOR] = "Menu Color:\n\nColors the cube, its light, the panels and the text.\nIndigo (default) is the GameCube's own color; Spice and\nJet Black are GameCube colors too.\n\nCover art, button icons, warnings and enabled cheats\nkeep their own colors.",
	[SET_PANEL_TRANSPARENCY] = "Panel Transparency:\n\nEnabled - Menu panels are translucent so the backdrop\nshows through, GameCube-menu style (default)\nDisabled - Panels are solid.",
	[SET_ANIMATED_BACKDROP] = "Animated Backdrop:\n\nEnabled - The backdrop gently drifts (default)\nDisabled - The backdrop is static.",
	[SET_MENU_MUSIC] = "Menu Music:\n\nEnabled - Play the bundled original ambient loop (default)\nDisabled - Silent.\n\nChanges take effect immediately.",
	[SET_MENU_SFX] = "Menu Sounds:\n\nEnabled - Soft blip/confirm sounds on navigation (default)\nDisabled - Silent.",
	[SET_FLATTEN_DIR] = "Flatten directory:\n\nFlattens a directory structure matching a glob pattern.",
	[SET_SHOW_HIDDEN] = "Show hidden files:\n\nLists files and folders marked hidden, such as the /swiss folder\nthat holds Indigo's settings.",
	[SET_AUTOBOOT] = "Boot without prompts:\n\nStarts a game as soon as you choose it, without its detail screen.\nHold B while choosing a game to see the screen instead; that turns\nthis off for the rest of the session."
};

static char *tooltips_network[PAGE_NETWORK_MAX+1] = {
	[SET_INIT_NET] = "Init network at startup:\n\nDisabled - Do not initialise the BBA even if present (default)\nEnabled - If a BBA is present, it will be initialised at startup\n\nIf initialised, navigate to the IP in a web browser to backup\nvarious data. wiiload is available for iterative development.",
	[SET_FSP_PMTU] = "FSP Path MTU:\n\nThe valid range is between 576 and 2030 bytes (default: 1500).\nIncreasing this value may provide a performance enhancement.\n\nThe maximum packet size on the server should be set to at least\nthis value, minus 40 bytes. The Layer 2 MTU on all involved\nnetwork interfaces must be set to at least this value.\n\nMisconfiguration will result in read errors. Users of USB Dolphin\nneed to observe limitations specific to their USB Ethernet adapter.",
	[SET_BBA_LOCALIP] = "IPv4 Address:\n\nThe console's address when DHCP is off. With DHCP on, the address\nyour router hands out is shown here instead.\n\nTakes effect the next time the network starts.",
	[SET_BBA_NETMASK] = "IPv4 Netmask:\n\nThe network's prefix length when DHCP is off, such as 24 on a\ntypical home network.",
	[SET_BBA_GATEWAY] = "IPv4 Gateway:\n\nYour router's address when DHCP is off.",
	[SET_BBA_DHCP] = "IPv4 uses DHCP:\n\nYes - Your router assigns the address, netmask and gateway (default)\nNo - Use the address, netmask and gateway set above",
	[SET_FSP_HOSTIP] = "FSP Host IP:\n\nAddress of the computer running the FSP server with your games.",
	[SET_FSP_PORT] = "FSP Port:\n\nPort of the FSP server (21 by default).",
	[SET_FSP_PASS] = "FSP Password:\n\nPassword for the FSP server, if it has one. It is stored as plain\ntext in global.ini.",
	[SET_FTP_HOSTIP] = "FTP Host IP:\n\nAddress of the FTP server with your games.",
	[SET_FTP_PORT] = "FTP Port:\n\nPort of the FTP server (21 by default).",
	[SET_FTP_USER] = "FTP Username:\n\nUser name for the FTP server.",
	[SET_FTP_PASS] = "FTP Password:\n\nPassword for the FTP server. It is stored as plain text in\nglobal.ini.",
	[SET_FTP_PASV] = "FTP PASV Mode:\n\nUse passive mode, where the console opens the data connection.\nTry it if listings stall behind a firewall or router.",
	[SET_SMB_HOSTIP] = "SMB Host IP:\n\nAddress of the computer sharing your games over SMB.",
	[SET_SMB_SHARE] = "SMB Share:\n\nName of the shared folder on that computer.",
	[SET_SMB_USER] = "SMB Username:\n\nUser name for the share.",
	[SET_SMB_PASS] = "SMB Password:\n\nPassword for the share. It is stored as plain text in global.ini.",
	[SET_RT4K_HOSTIP] = "RetroTINK-4K Host IP:\n\nAddress of a RetroTINK-4K on your network. Indigo switches it to\neach game's RetroTINK-4K profile.",
	[SET_RT4K_PORT] = "RetroTINK-4K Port:\n\nPort of the RetroTINK-4K's network control."
};

static char *tooltips_game_global[PAGE_GAME_GLOBAL_MAX+1] = {
	[SET_IGR] = "In-Game Reset: (A + Z + Start)\n\nReboot - Perform hot reset with a compatible device\nApploader - Requires /swiss/patches/apploader.img",
	[SET_BS2BOOT] = "Load GameCube Main Menu:\n\nWhen enabled, games will be booted with the GameCube logo\nscreen and Main Menu accessible with patches applied.\n\nRequires /swiss/patches/ipl.bin on Wii.",
	[SET_FORCE_VIDACTIVE] = "Force Video Active:\n\nA workaround for GCVideo-DVI firmware version series 3.0,\nrendered obsolete by 3.1 and later.",
	[SET_PAUSE_AVOUTPUT] = "Pause for resolution change:\n\nWhen enabled, a change in active video resolution will pause the\ngame for 2 seconds.",
	[SET_ALL_CHEATS] = "Auto-load cheats:\n\nIf enabled, and a cheats file for a particular game is found\ne.g. /swiss/cheats/GPOP8D.txt (on a compatible device)\nthen all previously enabled cheats will be re-enabled",
	[SET_WIIRDDBG] = "WiiRD debugging:\n\nDisabled - Boot as normal (default)\nEnabled - This will start a game with the WiiRD debugger enabled\n& paused\n\nThe WiiRD debugger takes up more memory and can cause issues.",
	[SET_EMULATE_MEMCARD] = "Emulate Memory Card:\n\nGames save to a memory card image on the device they start from\ninstead of a real memory card. Needs a device that supports it.",
	[SET_DISABLE_MCPGAMEID] = "Disable MemCard PRO GameID:\n\nStops Indigo telling a MemCard PRO or compatible card in that\nslot which game is starting, so it stays on its current card.",
	[SET_DISABLE_VIDPATCH] = "Disable Video Patches:\n\nNone - Apply Swiss's video patches, including fixes for\nparticular games (default)\nGame - Skip only the fixes for particular games\nAll - Patch no video at all. Forced video modes and the other\npicture settings then have no effect."
};

static char *tooltips_game[PAGE_GAME_DEFAULTS_MAX+1] = {
	[SET_VERT_OFFSET] = "Force Vertical Offset:\n\n+0 - Standard value\n-2 - GCVideo-DVI compatible (480i)\n-3 - GCVideo-DVI compatible (default)\n-4 - GCVideo-DVI compatible (240p)\n-12 - Datapath VisionRGB (480p)",
	[SET_VERT_FILTER] = "Force Vertical Filter:\n\nFor 480i & 576i:\n Auto - Do nothing (default)\n\nFor 240p & 288p:\n Auto - Equivalent to 0 (default)\n 0 - 50%/50% blend with lower lines\n 1 - 50%/50% blend with upper lines\n 2 - Discard even lines\n\nFor other video modes:\n Auto - Equivalent to 0 (default)\n 0 - 3\327MSAA resolve only\n 1 - 18.75%/62.5%/18.75% blend\n 2 - 25%/50%/25% blend (deflicker)",
	[SET_PIXEL_CENTER] = "Fix Pixel Center:\n\nNot to be confused with the \223480p Pixel Fix\224 on Wii.",
	[SET_ANISO_FILTER] = "Force Anisotropic Filter:\n\nThe GameCube's texture sampling hardware is optimised for the\ntrilinear filtering of 16 bpp textures and the bilinear filtering of\n32 bpp textures.\n\nIt is not unusual for the performance to randomly plummet once\nanisotropic filtering is enabled, so do so sparingly.",
	[SET_POLL_RATE] = "Force Polling Rate:\n\nVSync - Highest compatibility\n1000Hz - Lowest input latency",
	[SET_INVERT_CAMERA] = "Invert Camera Stick:\n\nNo - Leave C Stick as-is (default)\nX - Invert X-axis of the C Stick\nY - Invert Y-axis of the C Stick\nX&Y - Invert both axes of the C Stick",
	[SET_SWAP_CAMERA] = "Swap Camera Stick:\n\nNo - Leave C Stick as-is (default)\nX - Swap X-axis of the C Stick with the Control Stick\nY - Swap Y-axis of the C Stick with the Control Stick\nX&Y - Swap both axes of the C Stick with the Control Stick",
	[SET_TRIGGER_LEVEL] = "Digital Trigger Level:\n\nSets the threshold where the L/R Button is fully pressed.",
	[SET_AUDIO_STREAM] = "Emulate Audio Streaming:\n\nAudio streaming is a hardware feature that allows a compressed\naudio track to be played in the background by the disc drive.\n\nEmulation is necessary for devices not attached to the\nDVD Interface, or for those not implementing it regardless.",
	[SET_READ_SPEED] = "Emulate Read Speed:\n\nNo - Start transfer immediately (default)\nYes - Delay transfer to simulate the GameCube disc drive\nWii - Delay transfer to simulate the Wii disc drive\n\nThis is necessary to avoid programming mistakes obfuscated by\nthe original medium, or for speedrunning.",
	[SET_EMULATE_ETHERNET] = "Emulate Broadband Adapter:\n\nOnly available with the File Service Protocol or an initialised\nETH2GC/GCNET module, where memory constraints permit.\n\nPackets not destined for the hypervisor are forwarded to the\nvirtual MAC. The virtual MAC address is the same as the\nphysical MAC. The physical MAC/PHY retain their configuration\nfrom Swiss, including link speed.",
	[SET_DISABLE_MEMCARD] = "Disable Memory Card:\n\nSome games misbehave when unexpected devices are present in\nthe memory card slots. When selected, the device will be hidden\nfrom the game if present at boot time.",
	[SET_DISABLE_HYPERVISOR] = "Hypervisor:\n\nOn - Features and bugfixes that rely on the hypervisor work,\nalong with prepatching and patch persistence (default)\nOff - Turns all of them off\n\nOnly available to devices attached to the DVD Interface.",
	[SET_CLEAN_BOOT] = "Prefer Clean Boot:\n\nWhen enabled, the GameCube will be reset and the game booted\nthrough normal processes with no changes applied.\nRegion restrictions may be applicable.\n\nOnly available to devices attached to the DVD Interface.",
	[SET_RT4K_PROFILE] = "RetroTINK-4K Profile:\n\nPresses a profile button through a configured ser2net TCP\nconnection to the RetroTINK-4K's serial port.",
	[SET_GAME_LANG] = "Game Language:\n\nThe language this game uses. Default follows System Language.\nMostly matters for PAL games that include several languages.",
	[SET_FORCE_VIDEOMODE] = "Force Video Mode:\n\nThe video mode this game starts in. Auto keeps the game's own\nmode. Some modes only appear with a component or digital cable.",
	[SET_HORIZ_SCALE] = "Force Horizontal Scale:\n\nHow the video output scales the picture across.\nAuto - Keep the game's own scaling (default)\n1:1 - No horizontal scaling\n11:10, 9:8 - Fixed ratios\n640px to 720px - A fixed output width",
	[SET_FIELD_RENDER] = "Force Field Rendering:\n\nAuto - Keep the game's own choice (default)\nOn - Draw each field of an interlaced picture separately\nOff - Draw whole frames\nTAA - The jittered rendering some games use for anti-aliasing",
	[SET_ALPHA_DITHER] = "Alpha Dithering:\n\nOn - The dithering the GameCube adds to blended effects stays\n(default)\nOff - Turns it off. It can show as a fine dot pattern on digital\nvideo.",
	[SET_WIDESCREEN] = "Force Widescreen:\n\nStretches games made for 4:3 to fill a 16:9 screen.\n3D - Widen the 3D view only\n2D+3D - Also widen 2D menus and on-screen displays\nSet your TV to 16:9. Edges can look wrong in some games.",
	[SET_DEFAULTS] = "Reset to defaults:\n\nPuts every setting on this screen back to its default.\nIt asks first, and Discard & Exit still undoes it."
};

// Number of settings (including Back, Next, Save, Exit buttons) per page

char* getConfigDeviceName(SwissSettings *settings) {
	DEVICEHANDLER_INTERFACE *configDevice = getDeviceByUniqueId(settings->configDeviceId);
	return configDevice != NULL ? (char*)(configDevice->deviceName) : "None";
}

char* getGameVideoModeString(int gameVMode) {
	return gameVMode <= 0 ? getScanMode() == VI_PROGRESSIVE ? "Auto (Progressive)" : "Auto (Interlaced)" : gameVModeStr[gameVMode];
}

/* Game Defaults' first two rows are the NTSC and PAL video modes; the shared
 * table holds a game's language and video mode at those indices. */
static char *tooltips_game_defaults_video[SET_DEFAULT_PAL_VIDEOMODE + 1] = {
	[SET_DEFAULT_NTSC_VIDEOMODE] = "Force NTSC Video Mode:\n\nThe video mode games that aren't PAL start in, unless a game's\nown settings say otherwise. Auto keeps each game's own mode.",
	[SET_DEFAULT_PAL_VIDEOMODE] = "Force PAL Video Mode:\n\nThe video mode PAL games start in, unless a game's own settings\nsay otherwise. Auto keeps each game's own mode."
};

char* get_tooltip(int page_num, int option) {
	char *textPtr = NULL;
	if(page_num == PAGE_GLOBAL) {
		textPtr = tooltips_global[option];
	}
	else if(page_num == PAGE_INTERFACE) {
		textPtr = tooltips_interface[option];
	}
	else if(page_num == PAGE_NETWORK) {
		textPtr = tooltips_network[option];
	}
	else if(page_num == PAGE_GAME_GLOBAL) {
		textPtr = tooltips_game_global[option];
	}
	else if(page_num == PAGE_GAME_DEFAULTS) {
		textPtr = option <= SET_DEFAULT_PAL_VIDEOMODE ?
			tooltips_game_defaults_video[option] : tooltips_game[option];
	}
	else if(page_num == PAGE_GAME) {
		textPtr = tooltips_game[option];
	}
	return textPtr;
}

/*
 * Phase 4D presentation state. One static layout per published page:
 * bounded MEM1 (sizeof(uiSetLayout_t), well under 1 KB), no per-frame
 * allocation, and no retained pointers -- every string handed to a Draw*
 * primitive is strdup'd by FrameBufferMagic before this frame returns.
 */
static uiSetLayout_t setLayout;
static int setLayoutRowCursor;

static GXColor setPanelColor = {8, 8, 28, 198};       /* readable indigo glass */
static GXColor setSubtleColor = {205, 210, 234, 230}; /* quiet chrome */
static GXColor setInactiveColor = {198, 203, 228, 235};
static GXColor setDisabledColor = {132, 138, 164, 220};
static GXColor setCustomColor = {196, 177, 255, 255};  /* the emblems' glow */
#define SET_PANEL_SOLID_ALPHA 238

typedef enum {
	SET_ROWKIND_CYCLE = 0, /* Left/Right cycles the value */
	SET_ROWKIND_TEXT,      /* opens the on-screen text editor */
	SET_ROWKIND_ACTION,    /* row-level action (e.g. Reset to defaults) */
	SET_ROWKIND_LINK       /* Setup row that opens a section */
} uiSettingRowKind_t;

static int measureSettingText(const char *text)
{
	return GetTextSizeInPixels(text);
}

static size_t boundedSettingTextLength(const char *text)
{
	size_t length = 0u;

	while(length < UI_SETLAYOUT_VALUE_SOURCE_LIMIT && text[length] != '\0') {
		length++;
	}
	return length;
}

/* Preserve the native-grid type floor by shortening presentation copies to
 * their measured owner. The underlying label/value is never modified. */
static bool prepareSettingText(const char *source, size_t maxSourceBytes,
	uiSetLayoutEllipsizeMode_t mode, uiSetLayoutTextKind_t kind,
	bool selected, bool enabled, int width, float preferredScale,
	char *out, size_t outCapacity, float *scale)
{
	uiSetLayoutTextFit_t fit;
	size_t sourceLength = boundedSettingTextLength(source);

	if(!UISetLayout_PrepareText(source, sourceLength, maxSourceBytes, mode,
		kind, selected, enabled, width, preferredScale,
		UI_SETLAYOUT_ROW_TEXT_FLOOR, measureSettingText, out, outCapacity,
		&fit)) {
		out[0] = '\0';
		*scale = UI_SETLAYOUT_ROW_TEXT_FLOOR;
		return false;
	}
	*scale = fit.scale;
	return true;
}

/* A button hint fits by its drawn width, icons included. */
static bool prepareHintText(const char *hint, int width, float preferredScale,
	char *out, size_t outCapacity, float *scale)
{
	uiSetLayoutTextFit_t fit;
	size_t length = strlen(hint);

	if(!UISetLayout_PrepareText(hint, length, length,
		UI_SETLAYOUT_ELLIPSIZE_TAIL, UI_SETLAYOUT_TEXT_PLAIN, false, true,
		width, preferredScale, UI_SETLAYOUT_ROW_TEXT_FLOOR,
		GetHintSizeInPixels, out, outCapacity, &fit)) {
		out[0] = '\0';
		*scale = UI_SETLAYOUT_ROW_TEXT_FLOOR;
		return false;
	}
	*scale = fit.scale;
	return true;
}

static uiSetLayoutTextKind_t settingTextKind(uiSettingRowKind_t kind)
{
	if(kind == SET_ROWKIND_CYCLE) {
		return UI_SETLAYOUT_TEXT_CYCLE;
	}
	if(kind == SET_ROWKIND_TEXT) {
		return UI_SETLAYOUT_TEXT_EDITABLE;
	}
	return UI_SETLAYOUT_TEXT_PLAIN;
}

static void add_tooltip_label(uiDrawObj_t* page, int page_num, int option) {
	char display[UI_SETLAYOUT_TEXT_BUFFER_SIZE];
	float scale;

	if(get_tooltip(page_num, option) &&
		prepareHintText("Y  HELP", setLayout.helpHintMaxWidth,
			set_subtitle_size, display, sizeof(display), &scale)) {
		DrawAddChild(page, DrawHintLabel(setLayout.helpHintX,
			setLayout.helpHintY, display, scale, ALIGN_LEFT, setSubtleColor));
	}
}

/*
 * Emit one settable row if it falls inside the layout's visible window.
 * The selected row is structurally distinct (capsule border + larger
 * text + leading marker), never hue-only; disabled rows dim both label
 * and value and drop their affordance. Value affordances are truthful:
 * "< >" only on cycle rows, "\205" only on rows that open an editor.
 */
static void drawSettingRow(uiDrawObj_t* page, const char *label, const char *value, const char *tag, uiSettingRowKind_t kind, bool selected, bool enabled) {
	char boundedLabel[UI_SETLAYOUT_LABEL_BUFFER_SIZE];
	char displayValue[UI_SETLAYOUT_VALUE_BUFFER_SIZE + 8u];
	int row = setLayoutRowCursor++;
	int slot = row - setLayout.firstVisibleRow;
	int textY;
	float labelScale = selected ? set_row_sel_size : set_row_size;
	float valueScale = labelScale;
	GXColor labelColor = !enabled ? setDisabledColor :
		(selected ? defaultColor : setInactiveColor);
	GXColor valueColor = !enabled ? setDisabledColor :
		(selected ? defaultColor : setInactiveColor);

	if(slot < 0 || slot >= setLayout.visibleRowCount) {
		return;
	}
	/* drawString anchors text on its vertical CENTER. */
	textY = setLayout.rowTextY[slot];
	if(selected) {
		DrawAddChild(page, DrawStyledLabel(setLayout.rowRect[slot].x - 12, textY, "\225", set_row_size, ALIGN_LEFT, labelColor));
	}
	if(prepareSettingText(label, UI_SETLAYOUT_LABEL_BUFFER_SIZE - 1u,
		UI_SETLAYOUT_ELLIPSIZE_TAIL, UI_SETLAYOUT_TEXT_PLAIN, false,
		enabled, setLayout.rowLabelMaxWidth, labelScale, boundedLabel,
		sizeof(boundedLabel), &labelScale)) {
		DrawAddChild(page, DrawStyledLabel(setLayout.rowLabelX, textY,
			boundedLabel, labelScale, ALIGN_LEFT, labelColor));
	}
	if(tag != NULL && setLayout.rowTagWidth > 0) {
		char displayTag[UI_SETLAYOUT_VALUE_BUFFER_SIZE];
		float tagScale;
		if(prepareSettingText(tag, UI_SETLAYOUT_VALUE_TEXT_MAX,
			UI_SETLAYOUT_ELLIPSIZE_TAIL, UI_SETLAYOUT_TEXT_PLAIN, false,
			true, setLayout.rowTagWidth, set_meta_size, displayTag,
			sizeof(displayTag), &tagScale)) {
			DrawAddChild(page, DrawStyledLabel(setLayout.rowTagX, textY,
				displayTag, tagScale, ALIGN_RIGHT,
				enabled ? setCustomColor : setDisabledColor));
		}
	}
	if(kind == SET_ROWKIND_ACTION || value == NULL) {
		return;
	}
	if(prepareSettingText(value, UI_SETLAYOUT_VALUE_TEXT_MAX,
		kind == SET_ROWKIND_TEXT ? UI_SETLAYOUT_ELLIPSIZE_MIDDLE :
			UI_SETLAYOUT_ELLIPSIZE_TAIL,
		settingTextKind(kind), selected, enabled, setLayout.rowValueWidth,
		valueScale, displayValue, sizeof(displayValue), &valueScale)) {
		DrawAddChild(page, DrawStyledLabel(setLayout.rowValueX, textY,
			displayValue, valueScale, ALIGN_RIGHT, valueColor));
	}
}

/* Tabs, header, backing panel, scroll, and the persistent action rail. */
static void drawSettingsChrome(uiDrawObj_t* page, int page_num, ConfigEntry *gameConfig) {
	const uiSetLayoutPage_t *desc = UISetLayout_PageDesc(page_num);
	const uiSetLayoutRect_t *focusRect = NULL;
	char title[UI_SETLAYOUT_TEXT_BUFFER_SIZE];
	char subtitle[UI_SETLAYOUT_TEXT_BUFFER_SIZE];
	char progress[32];
	char progressDisplay[UI_SETLAYOUT_TEXT_BUFFER_SIZE];
	const char *subtitleText = desc->subtitle;
	GXColor panelColor = setPanelColor;
	float titleScale;
	float subtitleScale;
	float progressScale;
	int focusSlot;
	int i;

	panelColor.a = swissSettings.disablePanelTransparency ?
		SET_PANEL_SOLID_ALPHA : setPanelColor.a;
	DrawAddChild(page, DrawEmptyColouredBox(setLayout.panel.x, setLayout.panel.y,
		setLayout.panel.x + setLayout.panel.w, setLayout.panel.y + setLayout.panel.h,
		panelColor));
	if(setLayout.selectedRow >= 0) {
		focusSlot = setLayout.selectedRow - setLayout.firstVisibleRow;
		if(focusSlot >= 0 && focusSlot < setLayout.visibleRowCount) {
			focusRect = &setLayout.rowRect[focusSlot];
		}
	}
	else if(setLayout.selectedAction >= 0 &&
		setLayout.selectedAction < setLayout.actionCount) {
		focusRect = &setLayout.actionRect[setLayout.selectedAction];
	}
	if(focusRect != NULL) {
		DrawAddChild(page, DrawSettingsFocus(focusRect->x, focusRect->y,
			focusRect->x + focusRect->w, focusRect->y + focusRect->h));
	}

	for(i = 0; i < setLayout.tabCount; i++) {
		const uiSetLayoutPage_t *tab = UISetLayout_PageDesc(i);
		char tabDisplay[UI_SETLAYOUT_TEXT_BUFFER_SIZE];
		float tabScale;
		if(i == setLayout.currentTab) {
			DrawAddChild(page, DrawTransparentBox(setLayout.tabCell[i].x, setLayout.tabCell[i].y,
				setLayout.tabCell[i].x + setLayout.tabCell[i].w,
				setLayout.tabCell[i].y + setLayout.tabCell[i].h));
		}
		if(prepareSettingText(tab->tabLabel,
				UI_SETLAYOUT_LABEL_BUFFER_SIZE - 1u,
				UI_SETLAYOUT_ELLIPSIZE_TAIL, UI_SETLAYOUT_TEXT_PLAIN,
				false, true, setLayout.tabCell[i].w - 12, set_tab_size,
				tabDisplay, sizeof(tabDisplay), &tabScale)) {
			DrawAddChild(page, DrawStyledLabel(setLayout.tabLabelCenterX[i],
				setLayout.tabLabelY, tabDisplay, tabScale, ALIGN_CENTER,
				i == setLayout.currentTab ? defaultColor : setInactiveColor));
		}
	}

	if(desc->tab == UI_SETLAYOUT_NO_TAB) {
		snprintf(progress, sizeof(progress), "X  DEFAULT    B  DONE");
	}
	else if(page_num > VIEW_SETUP) {
		snprintf(progress, sizeof(progress), "B  BACK");
	}
	else {
		snprintf(progress, sizeof(progress), "L/R  %i OF %i", desc->tab + 1,
			UI_SETLAYOUT_TAB_COUNT);
	}
	if(prepareSettingText(page_num == VIEW_GAME && gameConfig != NULL &&
			gameConfig->game_name[0] != '\0' ? gameConfig->game_name :
			desc->title, UI_SETLAYOUT_LABEL_BUFFER_SIZE - 1u,
			UI_SETLAYOUT_ELLIPSIZE_TAIL, UI_SETLAYOUT_TEXT_PLAIN, false,
			true, setLayout.titleMaxWidth, set_title_size, title,
			sizeof(title), &titleScale)) {
		DrawAddChild(page, DrawStyledLabel(setLayout.titleX, setLayout.titleY,
			title, titleScale, ALIGN_LEFT, defaultColor));
	}
	if(page_num == VIEW_STORAGE) {
		subtitleText = UISetLayout_SettingsFileText(
			devices[DEVICE_CONFIG] == NULL ? UI_SETLAYOUT_SETTINGS_FILE_NO_DEVICE :
			config_global_file_loaded() ? UI_SETLAYOUT_SETTINGS_FILE_SAVED :
			UI_SETLAYOUT_SETTINGS_FILE_MISSING);
	}
	if(prepareSettingText(subtitleText,
			UI_SETLAYOUT_LABEL_BUFFER_SIZE - 1u,
			UI_SETLAYOUT_ELLIPSIZE_TAIL, UI_SETLAYOUT_TEXT_PLAIN, false,
			true, setLayout.subtitleMaxWidth, set_subtitle_size, subtitle,
			sizeof(subtitle), &subtitleScale)) {
		DrawAddChild(page, DrawStyledLabel(setLayout.subtitleX,
			setLayout.subtitleY, subtitle, subtitleScale, ALIGN_LEFT,
			setSubtleColor));
	}
	if(prepareHintText(progress, setLayout.progressMaxWidth, set_meta_size,
			progressDisplay, sizeof(progressDisplay), &progressScale)) {
		DrawAddChild(page, DrawHintLabel(setLayout.progressX,
			setLayout.progressY, progressDisplay, progressScale,
			ALIGN_RIGHT, setSubtleColor));
	}

	if(setLayout.scrollVisible) {
		DrawAddChild(page, DrawVertScrollBar(setLayout.scrollTrack.x, setLayout.scrollTrack.y,
			setLayout.scrollTrack.w, setLayout.scrollTrack.h,
			setLayout.scrollPercent, setLayout.scrollThumb.h));
	}

	for(i = 0; i < setLayout.actionCount; i++) {
		static const char *actionText[UI_SETLAYOUT_MAX_ACTIONS] = {
			"Save & Exit", "Discard & Exit"
		};
		DrawAddChild(page, DrawSelectableButton(setLayout.actionRect[i].x, setLayout.actionRect[i].y,
			setLayout.actionRect[i].x + setLayout.actionRect[i].w,
			setLayout.actionRect[i].y + setLayout.actionRect[i].h,
			actionText[setLayout.actionKind[i]], B_NOSELECT));
	}
}

/* One view row: the (page, option) setting it shows, or, when page is
 * SETTINGS_ROW_LINK, the Setup section it opens (option = view). */
typedef struct {
	u8 page;
	u8 option;
} settingsRowRef_t;

#define SETTINGS_ROW_LINK 0xFF

static const settingsRowRef_t quickRows[] = {
	{PAGE_INTERFACE, SET_MENU_MUSIC},
	{PAGE_INTERFACE, SET_MENU_SFX},
	{PAGE_INTERFACE, SET_UI_ANIMS},
	{PAGE_GLOBAL, SET_DISABLE_RUMBLE},
	{PAGE_GAME_GLOBAL, SET_IGR},
	{PAGE_GAME_GLOBAL, SET_BS2BOOT},
	{PAGE_GAME_GLOBAL, SET_EMULATE_MEMCARD},
	{PAGE_GAME_GLOBAL, SET_ALL_CHEATS},
	{PAGE_INTERFACE, SET_AUTOBOOT},
};

/* Game Defaults and a game's own settings share one order: what people
 * change most (video, widescreen, controls), then compatibility, picture
 * tuning and the RetroTINK-4K profile. */
static const settingsRowRef_t gameDefaultsRows[] = {
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_NTSC_VIDEOMODE},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_PAL_VIDEOMODE},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_WIDESCREEN},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_POLL_RATE},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_INVERT_CAMERA},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_SWAP_CAMERA},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_TRIGGER_LEVEL},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_CLEAN_BOOT},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_DISABLE_HYPERVISOR},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_READ_SPEED},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_AUDIO_STREAM},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_EMULATE_ETHERNET},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_DISABLE_MEMCARD},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_HORIZ_SCALE},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_VERT_OFFSET},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_VERT_FILTER},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_FIELD_RENDER},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_PIXEL_CENTER},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_ALPHA_DITHER},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_ANISO_FILTER},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_RT4K_PROFILE},
	{PAGE_GAME_DEFAULTS, SET_DEFAULT_DEFAULTS},
};

static const settingsRowRef_t setupRows[] = {
	{SETTINGS_ROW_LINK, VIEW_DISPLAY},
	{SETTINGS_ROW_LINK, VIEW_CONSOLE},
	{SETTINGS_ROW_LINK, VIEW_STORAGE},
	{SETTINGS_ROW_LINK, VIEW_NETWORK},
	{SETTINGS_ROW_LINK, VIEW_LIBRARY},
	{SETTINGS_ROW_LINK, VIEW_DEVELOPER},
};

static const settingsRowRef_t displayRows[] = {
	{PAGE_GLOBAL, SET_SWISS_VIDEOMODE},
	{PAGE_GLOBAL, SET_SYS_VIDEO},
	{PAGE_GLOBAL, SET_SCREEN_POS},
	{PAGE_GLOBAL, SET_AVE_COMPAT},
	{PAGE_GLOBAL, SET_FORCE_DTVSTATUS},
	{PAGE_GLOBAL, SET_RT4K_OPTIM},
	{PAGE_GAME_GLOBAL, SET_DISABLE_VIDPATCH},
	{PAGE_GAME_GLOBAL, SET_FORCE_VIDACTIVE},
	{PAGE_GAME_GLOBAL, SET_PAUSE_AVOUTPUT},
};

static const settingsRowRef_t consoleRows[] = {
	{PAGE_INTERFACE, SET_UI_COLOR},
	{PAGE_GLOBAL, SET_SYS_SOUND},
	{PAGE_GLOBAL, SET_SYS_LANG},
	{PAGE_GLOBAL, SET_SYS_BOOTMODE},
	{PAGE_GLOBAL, SET_DISABLE_RECALIB},
	{PAGE_GLOBAL, SET_TAU_CALIB},
};

static const settingsRowRef_t storageRows[] = {
	{PAGE_GLOBAL, SET_CONFIG_DEV},
	{PAGE_GLOBAL, SET_EXI_SPEED},
	{PAGE_GLOBAL, SET_INIT_DRIVE},
	{PAGE_GLOBAL, SET_STOP_MOTOR},
	{PAGE_GLOBAL, SET_AUDIO_BUFFER},
	{PAGE_GAME_GLOBAL, SET_DISABLE_MCPGAMEID},
};

static const settingsRowRef_t networkRows[] = {
	{PAGE_NETWORK, SET_INIT_NET},
	{PAGE_NETWORK, SET_BBA_LOCALIP},
	{PAGE_NETWORK, SET_BBA_NETMASK},
	{PAGE_NETWORK, SET_BBA_GATEWAY},
	{PAGE_NETWORK, SET_BBA_DHCP},
	{PAGE_NETWORK, SET_FSP_HOSTIP},
	{PAGE_NETWORK, SET_FSP_PORT},
	{PAGE_NETWORK, SET_FSP_PASS},
	{PAGE_NETWORK, SET_FSP_PMTU},
	{PAGE_NETWORK, SET_FTP_HOSTIP},
	{PAGE_NETWORK, SET_FTP_PORT},
	{PAGE_NETWORK, SET_FTP_USER},
	{PAGE_NETWORK, SET_FTP_PASS},
	{PAGE_NETWORK, SET_FTP_PASV},
	{PAGE_NETWORK, SET_SMB_HOSTIP},
	{PAGE_NETWORK, SET_SMB_SHARE},
	{PAGE_NETWORK, SET_SMB_USER},
	{PAGE_NETWORK, SET_SMB_PASS},
	{PAGE_NETWORK, SET_RT4K_HOSTIP},
	{PAGE_NETWORK, SET_RT4K_PORT},
};

static const settingsRowRef_t libraryRows[] = {
	{PAGE_INTERFACE, SET_GAMEBROWSER_TYPE},
	{PAGE_INTERFACE, SET_APPSBROWSER_TYPE},
	{PAGE_INTERFACE, SET_FILEBROWSER_TYPE},
	{PAGE_INTERFACE, SET_RECENT_LIST},
	{PAGE_INTERFACE, SET_FLATTEN_DIR},
	{PAGE_INTERFACE, SET_SHOW_HIDDEN},
	{PAGE_INTERFACE, SET_HIDE_UNK},
	{PAGE_INTERFACE, SET_FILE_MGMT},
	{PAGE_INTERFACE, SET_PANEL_TRANSPARENCY},
	{PAGE_INTERFACE, SET_ANIMATED_BACKDROP},
};

static const settingsRowRef_t developerRows[] = {
	{PAGE_GLOBAL, SET_ENABLE_USBGECKO},
	{PAGE_GLOBAL, SET_WAIT_USBGECKO},
	{PAGE_GLOBAL, SET_SIMMEMSIZE},
	{PAGE_GAME_GLOBAL, SET_WIIRDDBG},
};

static const settingsRowRef_t gameRows[] = {
	{PAGE_GAME, SET_FORCE_VIDEOMODE},
	{PAGE_GAME, SET_WIDESCREEN},
	{PAGE_GAME, SET_GAME_LANG},
	{PAGE_GAME, SET_POLL_RATE},
	{PAGE_GAME, SET_INVERT_CAMERA},
	{PAGE_GAME, SET_SWAP_CAMERA},
	{PAGE_GAME, SET_TRIGGER_LEVEL},
	{PAGE_GAME, SET_CLEAN_BOOT},
	{PAGE_GAME, SET_DISABLE_HYPERVISOR},
	{PAGE_GAME, SET_READ_SPEED},
	{PAGE_GAME, SET_AUDIO_STREAM},
	{PAGE_GAME, SET_EMULATE_ETHERNET},
	{PAGE_GAME, SET_DISABLE_MEMCARD},
	{PAGE_GAME, SET_HORIZ_SCALE},
	{PAGE_GAME, SET_VERT_OFFSET},
	{PAGE_GAME, SET_VERT_FILTER},
	{PAGE_GAME, SET_FIELD_RENDER},
	{PAGE_GAME, SET_PIXEL_CENTER},
	{PAGE_GAME, SET_ALPHA_DITHER},
	{PAGE_GAME, SET_ANISO_FILTER},
	{PAGE_GAME, SET_RT4K_PROFILE},
	{PAGE_GAME, SET_DEFAULTS},
};

/* Where each game row keeps its value, so it can be compared with the
 * default and put back. The game file stores exactly these fields. */
#define GAME_FIELD(field) {offsetof(ConfigEntry, field), sizeof(((ConfigEntry *)0)->field)}
static const struct {
	size_t offset;
	size_t size;
} gameRowFields[] = {
	[SET_GAME_LANG] = GAME_FIELD(gameLanguage),
	[SET_FORCE_VIDEOMODE] = GAME_FIELD(gameVMode),
	[SET_HORIZ_SCALE] = GAME_FIELD(forceHScale),
	[SET_VERT_OFFSET] = GAME_FIELD(forceVOffset),
	[SET_VERT_FILTER] = GAME_FIELD(forceVFilter),
	[SET_FIELD_RENDER] = GAME_FIELD(forceVJitter),
	[SET_PIXEL_CENTER] = GAME_FIELD(fixPixelCenter),
	[SET_ALPHA_DITHER] = GAME_FIELD(disableDithering),
	[SET_ANISO_FILTER] = GAME_FIELD(forceAnisotropy),
	[SET_WIDESCREEN] = GAME_FIELD(forceWidescreen),
	[SET_POLL_RATE] = GAME_FIELD(forcePollRate),
	[SET_INVERT_CAMERA] = GAME_FIELD(invertCStick),
	[SET_SWAP_CAMERA] = GAME_FIELD(swapCStick),
	[SET_TRIGGER_LEVEL] = GAME_FIELD(triggerLevel),
	[SET_AUDIO_STREAM] = GAME_FIELD(emulateAudioStream),
	[SET_READ_SPEED] = GAME_FIELD(emulateReadSpeed),
	[SET_EMULATE_ETHERNET] = GAME_FIELD(emulateEthernet),
	[SET_DISABLE_MEMCARD] = GAME_FIELD(disableMemoryCard),
	[SET_DISABLE_HYPERVISOR] = GAME_FIELD(disableHypervisor),
	[SET_CLEAN_BOOT] = GAME_FIELD(preferCleanBoot),
	[SET_RT4K_PROFILE] = GAME_FIELD(rt4kProfile),
};
#undef GAME_FIELD
_Static_assert(sizeof(gameRowFields) / sizeof(gameRowFields[0]) == SET_DEFAULTS, "game fields drift");

/* The defaults this game would get from the current Game Defaults. While a
 * game's settings are open the global settings cannot change, so these are
 * also what Save compares against. */
static void settingsGameDefaults(const ConfigEntry *game, ConfigEntry *defaults)
{
	memcpy(defaults, game, sizeof(ConfigEntry));
	config_defaults_from(defaults, &swissSettings);
}

static bool settingsGameRowCustom(const ConfigEntry *game,
	const ConfigEntry *defaults, int option)
{
	return option >= 0 && option < SET_DEFAULTS &&
		memcmp((const char *)game + gameRowFields[option].offset,
			(const char *)defaults + gameRowFields[option].offset,
			gameRowFields[option].size) != 0;
}

/* How many of a game's rows differ from Game Defaults (Game Detail shows it). */
int settings_game_custom_count(const ConfigEntry *game)
{
	static ConfigEntry defaults;
	int count = 0;
	int option;

	if(game == NULL) {
		return 0;
	}
	settingsGameDefaults(game, &defaults);
	for(option = 0; option < SET_DEFAULTS; option++) {
		if(settingsGameRowCustom(game, &defaults, option)) {
			count++;
		}
	}
	return count;
}

/* X in a game's settings: this row follows Game Defaults again. */
static void settingsUseDefault(ConfigEntry *game, int option)
{
	static ConfigEntry defaults;

	if(option < 0 || option >= SET_DEFAULTS) {
		return;
	}
	settingsGameDefaults(game, &defaults);
	memcpy((char *)game + gameRowFields[option].offset,
		(const char *)&defaults + gameRowFields[option].offset,
		gameRowFields[option].size);
}

/* Reset to defaults in a game's settings: every row follows Game Defaults
 * again. The game's Comment and Status aren't settings, so they stay. */
static void settingsResetGame(ConfigEntry *game)
{
	int option;

	for(option = 0; option < SET_DEFAULTS; option++) {
		settingsUseDefault(game, option);
	}
}

#define SETTINGS_ROWS(rows) {rows, sizeof(rows) / sizeof(rows[0])}
static const struct {
	const settingsRowRef_t *rows;
	int count;
} settingsViews[UI_SETLAYOUT_PAGE_COUNT] = {
	[VIEW_QUICK] = SETTINGS_ROWS(quickRows),
	[VIEW_GAME_DEFAULTS] = SETTINGS_ROWS(gameDefaultsRows),
	[VIEW_SETUP] = SETTINGS_ROWS(setupRows),
	[VIEW_DISPLAY] = SETTINGS_ROWS(displayRows),
	[VIEW_CONSOLE] = SETTINGS_ROWS(consoleRows),
	[VIEW_STORAGE] = SETTINGS_ROWS(storageRows),
	[VIEW_NETWORK] = SETTINGS_ROWS(networkRows),
	[VIEW_LIBRARY] = SETTINGS_ROWS(libraryRows),
	[VIEW_DEVELOPER] = SETTINGS_ROWS(developerRows),
	[VIEW_GAME] = SETTINGS_ROWS(gameRows),
};
#undef SETTINGS_ROWS

_Static_assert(sizeof(quickRows) / sizeof(quickRows[0]) == UI_SETLAYOUT_ROWS_QUICK, "quick rows drift");
_Static_assert(sizeof(gameDefaultsRows) / sizeof(gameDefaultsRows[0]) == UI_SETLAYOUT_ROWS_GAME_DEFAULTS, "defaults rows drift");
_Static_assert(sizeof(setupRows) / sizeof(setupRows[0]) == UI_SETLAYOUT_ROWS_SETUP, "setup rows drift");
_Static_assert(sizeof(displayRows) / sizeof(displayRows[0]) == UI_SETLAYOUT_ROWS_DISPLAY, "display rows drift");
_Static_assert(sizeof(consoleRows) / sizeof(consoleRows[0]) == UI_SETLAYOUT_ROWS_CONSOLE, "console rows drift");
_Static_assert(sizeof(storageRows) / sizeof(storageRows[0]) == UI_SETLAYOUT_ROWS_STORAGE, "storage rows drift");
_Static_assert(sizeof(networkRows) / sizeof(networkRows[0]) == UI_SETLAYOUT_ROWS_NETWORK, "network rows drift");
_Static_assert(sizeof(libraryRows) / sizeof(libraryRows[0]) == UI_SETLAYOUT_ROWS_LIBRARY, "library rows drift");
_Static_assert(sizeof(developerRows) / sizeof(developerRows[0]) == UI_SETLAYOUT_ROWS_DEVELOPER, "developer rows drift");
_Static_assert(sizeof(gameRows) / sizeof(gameRows[0]) == UI_SETLAYOUT_ROWS_GAME, "game rows drift");

/* Short summaries shown beside each Setup section; each fits the value
 * column's UI_SETLAYOUT_VALUE_TEXT_MAX without an ellipsis. */
static const char *setupSummaries[] = {
	"Video mode, cable, TV",
	"Color, sound, language",
	"Settings file, SD, disc",
	"Adapter, file servers",
	"Browser, recent, look",
	"USB Gecko, memory, debug",
};
_Static_assert(sizeof(setupSummaries) / sizeof(setupSummaries[0]) == UI_SETLAYOUT_ROWS_SETUP, "setup summaries drift");

static const settingsRowRef_t *settingsViewRow(int view, int option)
{
	if(view < 0 || view >= UI_SETLAYOUT_PAGE_COUNT ||
		option < 0 || option >= settingsViews[view].count) {
		return NULL;
	}
	return &settingsViews[view].rows[option];
}

/* What one row shows. `text` holds formatted values. */
typedef struct {
	const char *label;
	const char *value;
	uiSettingRowKind_t kind;
	bool enabled;
	char text[32];
} settingRowView_t;

static void rowShow(settingRowView_t *row, uiSettingRowKind_t kind,
	const char *label, const char *value, bool enabled)
{
	row->kind = kind;
	row->label = label;
	row->value = value;
	row->enabled = enabled;
}

static void rowCycle(settingRowView_t *row, const char *label,
	const char *value, bool enabled)
{
	rowShow(row, SET_ROWKIND_CYCLE, label, value, enabled);
}

static void rowYesNo(settingRowView_t *row, const char *label, bool value,
	bool enabled)
{
	rowShow(row, SET_ROWKIND_CYCLE, label, value ? "Yes" : "No", enabled);
}

/* A setting stored as "Disable X" reads the positive way round. */
static void rowOnOff(settingRowView_t *row, const char *label, bool on,
	bool enabled)
{
	rowShow(row, SET_ROWKIND_CYCLE, label, on ? "On" : "Off", enabled);
}

static void rowNumber(settingRowView_t *row, const char *label,
	const char *format, int value, bool enabled)
{
	snprintf(row->text, sizeof(row->text), format, value);
	rowShow(row, SET_ROWKIND_CYCLE, label, row->text, enabled);
}

static void rowText(settingRowView_t *row, const char *label,
	const char *value, bool enabled)
{
	rowShow(row, SET_ROWKIND_TEXT, label, value, enabled);
}

static void rowTextNumber(settingRowView_t *row, const char *label,
	int value, bool enabled)
{
	snprintf(row->text, sizeof(row->text), "%i", value);
	rowShow(row, SET_ROWKIND_TEXT, label, row->text, enabled);
}

static void rowAction(settingRowView_t *row, const char *label, bool enabled)
{
	rowShow(row, SET_ROWKIND_ACTION, label, NULL, enabled);
}

/* Label, value and enabled state of one setting, as the six legacy pages
 * drew them. */
static void settingsDescribeRow(int page, int option, ConfigEntry *gameConfig,
	settingRowView_t *row)
{
	bool enabledVideoPatches = swissSettings.disableVideoPatches < 2;
	bool enabledHypervisor = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->features & FEAT_HYPERVISOR);
	bool emulatedAudioStream = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_AUDIO_STREAMING);
	bool emulatedReadSpeed = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_READ_SPEED);
	bool emulatedEthernet = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_ETHERNET);
	bool enabledCleanBoot = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->location & LOC_DVD_CONNECTOR);

	rowAction(row, "", false);
	if(page == PAGE_GLOBAL) {
		bool tvEnable = swissSettings.aveCompat != AVE_RVL_COMPAT;
		bool dvdEnable = deviceHandler_getDeviceAvailable(&__device_dvd);
		bool dtvEnable = !in_range(swissSettings.aveCompat, AVE_N_DOL_COMPAT, AVE_P_DOL_COMPAT);
		bool rt4kEnable = in_range(swissSettings.aveCompat, GCDIGITAL_COMPAT, GCVIDEO_COMPAT);
		switch(option) {
			case SET_SYS_BOOTMODE: rowCycle(row, "System Boot Mode:", swissSettings.sramBoot ? "Production" : "Default", true); break;
			case SET_SYS_SOUND: rowCycle(row, "System Sound:", swissSettings.sramStereo ? "Stereo" : "Mono", true); break;
			case SET_SYS_VIDEO: rowCycle(row, "System Video:", sramVideoStr[swissSettings.sramVideo], tvEnable); break;
			case SET_SCREEN_POS: rowNumber(row, "Screen Position:", "%+i", swissSettings.sramHOffset, true); break;
			case SET_SYS_LANG: rowCycle(row, "System Language:", sramLanguageStr[swissSettings.sramLanguage], true); break;
			case SET_CONFIG_DEV: rowCycle(row, "Configuration Device:", getConfigDeviceName(&swissSettings), true); break;
			case SET_SWISS_VIDEOMODE:
				snprintf(row->text, sizeof(row->text), "%s%s", getVideoModeString(getVideoModeFromSwissSetting(swissSettings.uiVMode)), swissSettings.uiVMode == 0 ? " (Auto) " : "");
				rowCycle(row, "Swiss Video Mode:", row->text, true);
			break;
			case SET_INIT_DRIVE: rowYesNo(row, "Init DVD Drive at startup:", swissSettings.initDVDDriveAtStart, dvdEnable); break;
			case SET_STOP_MOTOR: rowYesNo(row, "Stop DVD Drive motor:", swissSettings.stopMotor, dvdEnable); break;
			case SET_AUDIO_BUFFER: rowCycle(row, "Configure Audio Buffer:", configAudioBufferStr[swissSettings.configAudioBuffer], dvdEnable); break;
			case SET_EXI_SPEED: rowCycle(row, "SD/IDE-EXI Speed:", swissSettings.exiSpeed ? "27 MHz" : "13.5 MHz", true); break;
			case SET_AVE_COMPAT: rowCycle(row, "AVE Compatibility:", aveCompatStr[swissSettings.aveCompat], true); break;
			case SET_FORCE_DTVSTATUS: rowCycle(row, "Force DTV Status:", forceDTVStatusStr[swissSettings.forceDTVStatus], dtvEnable); break;
			case SET_RT4K_OPTIM: rowYesNo(row, "RetroTINK-4K HDMI Input:", swissSettings.rt4kOptim, rt4kEnable); break;
			case SET_DISABLE_RECALIB: rowOnOff(row, "Controller Recalibration:", !swissSettings.disableRecalibration, true); break;
			/* Shown the positive way round; the file keeps Disable PAD Rumble. */
			case SET_DISABLE_RUMBLE: rowOnOff(row, "Controller Rumble:", !swissSettings.disableRumble, true); break;
			case SET_ENABLE_USBGECKO: rowCycle(row, "Enable USB Gecko:", enableUSBGeckoStr[swissSettings.enableUSBGecko], true); break;
			case SET_WAIT_USBGECKO: rowYesNo(row, "Wait for USB Gecko:", swissSettings.waitForUSBGecko, true); break;
			case SET_SIMMEMSIZE: rowCycle(row, "Simulated MRAM Size:", simulatedMemSizeStr[swissSettings.simulatedMemSize], true); break;
			case SET_TAU_CALIB: rowNumber(row, "CPU Temperature Calibration:", "%+i\260C", swissSettings.sramTemperature, is_gamecube()); break;
		}
	}
	else if(page == PAGE_INTERFACE) {
		switch(option) {
			case SET_FILEBROWSER_TYPE: rowCycle(row, "File Browser Type:", fileBrowserTypeStr[swissSettings.fileBrowserType], true); break;
			case SET_APPSBROWSER_TYPE: rowCycle(row, "File Browser Type for apps:", fileBrowserTypeStr[swissSettings.appsBrowserType], true); break;
			case SET_GAMEBROWSER_TYPE: rowCycle(row, "File Browser Type for games:", fileBrowserTypeStr[swissSettings.gameBrowserType], true); break;
			case SET_FILE_MGMT: rowYesNo(row, "File Management:", swissSettings.enableFileManagement, true); break;
			case SET_RECENT_LIST: rowCycle(row, "Recent List:", recentListLevelStr[swissSettings.recentListLevel], true); break;
			case SET_SHOW_HIDDEN: rowYesNo(row, "Show hidden files:", swissSettings.showHiddenFiles, true); break;
			case SET_HIDE_UNK: rowYesNo(row, "Hide unknown file types:", swissSettings.hideUnknownFileTypes, true); break;
			case SET_UI_ANIMS:
				rowCycle(row, "UI Motion:", uiMotionModeStr[UIMotion_ModeFromFlags(
					swissSettings.disableUIAnimations, swissSettings.reduceUIAnimations)], true);
			break;
			case SET_UI_COLOR: rowCycle(row, "Menu Color:", uiColorStr[swissSettings.uiColor], true); break;
			case SET_PANEL_TRANSPARENCY: rowYesNo(row, "Panel Transparency:", !swissSettings.disablePanelTransparency, true); break;
			case SET_ANIMATED_BACKDROP: rowYesNo(row, "Animated Backdrop:", !swissSettings.disableAnimatedBackdrop, true); break;
			case SET_MENU_MUSIC: rowYesNo(row, "Menu Music:", !swissSettings.disableMenuMusic, true); break;
			case SET_MENU_SFX: rowYesNo(row, "Menu Sounds:", !swissSettings.disableMenuSFX, true); break;
			case SET_AUTOBOOT: rowYesNo(row, "Boot without prompts:", swissSettings.autoBoot, true); break;
			case SET_FLATTEN_DIR: rowText(row, "Flatten directory:", swissSettings.flattenDir, true); break;
		}
	}
	else if(page == PAGE_NETWORK) {
		bool netEnable = net_initialized || bba_exists(LOC_ANY);
		switch(option) {
			case SET_INIT_NET: rowYesNo(row, "Init network at startup:", swissSettings.initNetworkAtStart, !bba_requires_init()); break;
			case SET_BBA_LOCALIP: rowText(row, "IPv4 Address:", swissSettings.bbaLocalIp, netEnable); break;
			case SET_BBA_NETMASK: rowTextNumber(row, "IPv4 Netmask:", swissSettings.bbaNetmask, netEnable); break;
			case SET_BBA_GATEWAY: rowText(row, "IPv4 Gateway:", swissSettings.bbaGateway, netEnable); break;
			case SET_BBA_DHCP: rowYesNo(row, "IPv4 uses DHCP:", swissSettings.bbaUseDhcp, netEnable); break;
			case SET_FSP_HOSTIP: rowText(row, "FSP Host IP:", swissSettings.fspHostIp, netEnable); break;
			case SET_FSP_PORT: rowTextNumber(row, "FSP Port:", swissSettings.fspPort, netEnable); break;
			case SET_FSP_PASS: rowText(row, "FSP Password:", "*****", netEnable); break;
			case SET_FSP_PMTU: rowTextNumber(row, "FSP Path MTU:", swissSettings.fspPathMtu, netEnable); break;
			case SET_FTP_HOSTIP: rowText(row, "FTP Host IP:", swissSettings.ftpHostIp, netEnable); break;
			case SET_FTP_PORT: rowTextNumber(row, "FTP Port:", swissSettings.ftpPort, netEnable); break;
			case SET_FTP_USER: rowText(row, "FTP Username:", swissSettings.ftpUserName, netEnable); break;
			case SET_FTP_PASS: rowText(row, "FTP Password:", "*****", netEnable); break;
			case SET_FTP_PASV: rowYesNo(row, "FTP PASV Mode:", swissSettings.ftpUsePasv, netEnable); break;
			case SET_SMB_HOSTIP: rowText(row, "SMB Host IP:", swissSettings.smbServerIp, netEnable); break;
			case SET_SMB_SHARE: rowText(row, "SMB Share:", swissSettings.smbShare, netEnable); break;
			case SET_SMB_USER: rowText(row, "SMB Username:", swissSettings.smbUser, netEnable); break;
			case SET_SMB_PASS: rowText(row, "SMB Password:", "*****", netEnable); break;
			case SET_RT4K_HOSTIP: rowText(row, "RetroTINK-4K Host IP:", swissSettings.rt4kHostIp, netEnable); break;
			case SET_RT4K_PORT: rowTextNumber(row, "RetroTINK-4K Port:", swissSettings.rt4kPort, netEnable); break;
		}
	}
	else if(page == PAGE_GAME_GLOBAL) {
		bool emulatedMemoryCard = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_MEMCARD);
		bool dbgEnable = devices[DEVICE_CUR] != &__device_usbgecko && deviceHandler_getDeviceAvailable(&__device_usbgecko);
		switch(option) {
			case SET_IGR: rowCycle(row, "In-Game Reset:", igrTypeStr[swissSettings.igrType], enabledHypervisor); break;
			case SET_BS2BOOT: rowCycle(row, "Load GameCube Main Menu:", bs2BootStr[swissSettings.bs2Boot], true); break;
			case SET_EMULATE_MEMCARD: rowYesNo(row, "Emulate Memory Card:", swissSettings.emulateMemoryCard, emulatedMemoryCard); break;
			case SET_DISABLE_MCPGAMEID: rowCycle(row, "Disable MemCard PRO GameID:", disableMCPGameIDStr[swissSettings.disableMCPGameID], true); break;
			case SET_DISABLE_VIDPATCH: rowCycle(row, "Disable Video Patches:", disableVideoPatchesStr[swissSettings.disableVideoPatches], true); break;
			case SET_FORCE_VIDACTIVE: rowYesNo(row, "Force Video Active:", swissSettings.forceVideoActive, enabledVideoPatches); break;
			case SET_PAUSE_AVOUTPUT: rowYesNo(row, "Pause for resolution change:", swissSettings.pauseAVOutput, enabledHypervisor); break;
			case SET_ALL_CHEATS: rowYesNo(row, "Auto-load cheats:", swissSettings.autoCheats, true); break;
			case SET_WIIRDDBG: rowYesNo(row, "WiiRD debugging:", swissSettings.wiirdDebug, dbgEnable); break;
			case SET_GLOBAL_DEFAULTS: rowAction(row, "Reset to defaults", true); break;
		}
	}
	else if(page == PAGE_GAME_DEFAULTS) {
		switch(option) {
			case SET_DEFAULT_NTSC_VIDEOMODE: rowCycle(row, "Force NTSC Video Mode:", getGameVideoModeString(swissSettings.gameVModeNtsc), enabledVideoPatches); break;
			case SET_DEFAULT_PAL_VIDEOMODE: rowCycle(row, "Force PAL Video Mode:", getGameVideoModeString(swissSettings.gameVModePal), enabledVideoPatches); break;
			case SET_DEFAULT_HORIZ_SCALE: rowCycle(row, "Force Horizontal Scale:", forceHScaleStr[swissSettings.forceHScale], enabledVideoPatches); break;
			case SET_DEFAULT_VERT_OFFSET: rowNumber(row, "Force Vertical Offset:", "%+i", swissSettings.forceVOffset, enabledVideoPatches); break;
			case SET_DEFAULT_VERT_FILTER: rowCycle(row, "Force Vertical Filter:", forceVFilterStr[swissSettings.forceVFilter], enabledVideoPatches); break;
			case SET_DEFAULT_FIELD_RENDER: rowCycle(row, "Force Field Rendering:", forceVJitterStr[swissSettings.forceVJitter], enabledVideoPatches); break;
			case SET_DEFAULT_PIXEL_CENTER: rowCycle(row, "Fix Pixel Center:", fixPixelCenterStr[swissSettings.fixPixelCenter], enabledVideoPatches); break;
			case SET_DEFAULT_ALPHA_DITHER: rowOnOff(row, "Alpha Dithering:", !swissSettings.disableDithering, enabledVideoPatches); break;
			case SET_DEFAULT_ANISO_FILTER: rowYesNo(row, "Force Anisotropic Filter:", swissSettings.forceAnisotropy, true); break;
			case SET_DEFAULT_WIDESCREEN: rowCycle(row, "Force Widescreen:", forceWidescreenStr[swissSettings.forceWidescreen], true); break;
			case SET_DEFAULT_POLL_RATE: rowCycle(row, "Force Polling Rate:", forcePollRateStr[swissSettings.forcePollRate], true); break;
			case SET_DEFAULT_INVERT_CAMERA: rowCycle(row, "Invert Camera Stick:", invertCStickStr[swissSettings.invertCStick], true); break;
			case SET_DEFAULT_SWAP_CAMERA: rowCycle(row, "Swap Camera Stick:", swapCStickStr[swissSettings.swapCStick], true); break;
			case SET_DEFAULT_TRIGGER_LEVEL: rowNumber(row, "Digital Trigger Level:", "%i", swissSettings.triggerLevel, true); break;
			case SET_DEFAULT_AUDIO_STREAM: rowCycle(row, "Emulate Audio Streaming:", emulateAudioStreamStr[swissSettings.emulateAudioStream], emulatedAudioStream); break;
			case SET_DEFAULT_READ_SPEED: rowCycle(row, "Emulate Read Speed:", emulateReadSpeedStr[swissSettings.emulateReadSpeed], emulatedReadSpeed); break;
			case SET_DEFAULT_EMULATE_ETHERNET: rowYesNo(row, "Emulate Broadband Adapter:", swissSettings.emulateEthernet, emulatedEthernet); break;
			case SET_DEFAULT_DISABLE_MEMCARD: rowCycle(row, "Disable Memory Card:", disableMemoryCardStr[swissSettings.disableMemoryCard], enabledHypervisor); break;
			case SET_DEFAULT_DISABLE_HYPERVISOR: rowOnOff(row, "Hypervisor:", !swissSettings.disableHypervisor, enabledCleanBoot); break;
			case SET_DEFAULT_CLEAN_BOOT: rowYesNo(row, "Prefer Clean Boot:", swissSettings.preferCleanBoot, enabledCleanBoot); break;
			case SET_DEFAULT_RT4K_PROFILE: rowNumber(row, "RetroTINK-4K Profile:", "%i", swissSettings.rt4kProfile, is_rt4k_alive()); break;
			case SET_DEFAULT_DEFAULTS: rowAction(row, "Reset to defaults", true); break;
		}
	}
	else if(page == PAGE_GAME) {
		/* Without a game (or one forced to clean boot) the rows are dimmed
		 * and show what a game would start with. */
		static ConfigEntry fallback;
		bool enabled = gameConfig != NULL && !gameConfig->forceCleanBoot;
		const ConfigEntry *game = gameConfig;
		if(!enabled) {
			memset(&fallback, 0, sizeof(fallback));
			config_defaults(&fallback);
			game = &fallback;
		}
		switch(option) {
			case SET_GAME_LANG: rowCycle(row, "Game Language:", sramLanguageStr[game->gameLanguage], enabled); break;
			case SET_FORCE_VIDEOMODE: rowCycle(row, "Force Video Mode:", getGameVideoModeString(game->gameVMode), enabled && enabledVideoPatches); break;
			case SET_HORIZ_SCALE: rowCycle(row, "Force Horizontal Scale:", forceHScaleStr[game->forceHScale], enabled && enabledVideoPatches); break;
			case SET_VERT_OFFSET: rowNumber(row, "Force Vertical Offset:", "%+i", game->forceVOffset, enabled && enabledVideoPatches); break;
			case SET_VERT_FILTER: rowCycle(row, "Force Vertical Filter:", forceVFilterStr[game->forceVFilter], enabled && enabledVideoPatches); break;
			case SET_FIELD_RENDER: rowCycle(row, "Force Field Rendering:", forceVJitterStr[game->forceVJitter], enabled && enabledVideoPatches); break;
			case SET_PIXEL_CENTER: rowCycle(row, "Fix Pixel Center:", fixPixelCenterStr[game->fixPixelCenter], enabled && enabledVideoPatches); break;
			case SET_ALPHA_DITHER: rowOnOff(row, "Alpha Dithering:", !game->disableDithering, enabled && enabledVideoPatches); break;
			case SET_ANISO_FILTER: rowYesNo(row, "Force Anisotropic Filter:", game->forceAnisotropy, enabled); break;
			case SET_WIDESCREEN: rowCycle(row, "Force Widescreen:", forceWidescreenStr[game->forceWidescreen], enabled); break;
			case SET_POLL_RATE: rowCycle(row, "Force Polling Rate:", forcePollRateStr[game->forcePollRate], enabled); break;
			case SET_INVERT_CAMERA: rowCycle(row, "Invert Camera Stick:", invertCStickStr[game->invertCStick], enabled); break;
			case SET_SWAP_CAMERA: rowCycle(row, "Swap Camera Stick:", swapCStickStr[game->swapCStick], enabled); break;
			case SET_TRIGGER_LEVEL: rowNumber(row, "Digital Trigger Level:", "%i", game->triggerLevel, enabled); break;
			case SET_AUDIO_STREAM: rowCycle(row, "Emulate Audio Streaming:", emulateAudioStreamStr[game->emulateAudioStream], enabled && emulatedAudioStream); break;
			case SET_READ_SPEED: rowCycle(row, "Emulate Read Speed:", emulateReadSpeedStr[game->emulateReadSpeed], enabled && emulatedReadSpeed); break;
			case SET_EMULATE_ETHERNET: rowYesNo(row, "Emulate Broadband Adapter:", game->emulateEthernet, enabled && emulatedEthernet); break;
			case SET_DISABLE_MEMCARD: rowCycle(row, "Disable Memory Card:", disableMemoryCardStr[game->disableMemoryCard], enabled && enabledHypervisor); break;
			case SET_DISABLE_HYPERVISOR: rowOnOff(row, "Hypervisor:", !game->disableHypervisor, enabled && enabledCleanBoot); break;
			case SET_CLEAN_BOOT: rowYesNo(row, "Prefer Clean Boot:", game->preferCleanBoot, enabled && enabledCleanBoot); break;
			case SET_RT4K_PROFILE: rowNumber(row, "RetroTINK-4K Profile:", "%i", game->rt4kProfile, enabled && is_rt4k_alive()); break;
			case SET_DEFAULTS: rowAction(row, "Reset to defaults", enabled); break;
		}
	}
}

uiDrawObj_t* settings_draw_page(int view, int option, ConfigEntry *gameConfig) {
	uiDrawObj_t* page = DrawContainer();
	const settingsRowRef_t *focus = settingsViewRow(view, option);
	bool focusSetting = focus != NULL && focus->page != SETTINGS_ROW_LINK;
	/* Mirrors _CurrentMotionMode() (FrameBufferMagic): Animations off snaps.
	 * Backdrop animation is decorative and cannot weaken primary focus travel. */
	int motionMode = (int)UIMotion_ModeFromFlags(
		swissSettings.disableUIAnimations, swissSettings.reduceUIAnimations);
	int i;

	static ConfigEntry gameDefaults;
	bool tagged = view == VIEW_GAME && gameConfig != NULL;

	UISetLayout_Compute(view, option, focusSetting &&
		get_tooltip(focus->page, focus->option) != NULL, motionMode, &setLayout);
	setLayoutRowCursor = 0;
	drawSettingsChrome(page, view, gameConfig);
	if(tagged) {
		settingsGameDefaults(gameConfig, &gameDefaults);
	}

	for(i = 0; i < settingsViews[view].count; i++) {
		const settingsRowRef_t *ref = &settingsViews[view].rows[i];
		const char *tag = NULL;
		settingRowView_t row;

		if(ref->page == SETTINGS_ROW_LINK) {
			rowShow(&row, SET_ROWKIND_LINK, UISetLayout_PageDesc(ref->option)->title,
				setupSummaries[ref->option - VIEW_DISPLAY], true);
		}
		else {
			settingsDescribeRow(ref->page, ref->option, gameConfig, &row);
		}
		/* A game's own values are marked; the rest follow Game Defaults. */
		if(tagged && settingsGameRowCustom(gameConfig, &gameDefaults, ref->option)) {
			tag = "Custom";
		}
		drawSettingRow(page, row.label, row.value, tag, row.kind, option == i, row.enabled);
	}
	// If we have a tooltip for this row, tell the user to press Y for help
	if(focusSetting) {
		add_tooltip_label(page, focus->page, focus->option);
	}

	DrawPublish(page);
	DrawPinMenuColor(page, swissSettings.uiColor);
	return page;
}

void settings_toggle(int page, int option, int direction, ConfigEntry *gameConfig) {
	if(page == PAGE_GLOBAL) {
		switch(option) {
			case SET_SYS_BOOTMODE:
				swissSettings.sramBoot ^= SYS_BOOT_PRODUCTION;
			break;
			case SET_SYS_SOUND:
				swissSettings.sramStereo ^= SYS_SOUND_STEREO;
			break;
			case SET_SYS_VIDEO:
				if(swissSettings.aveCompat != AVE_RVL_COMPAT) {
					swissSettings.sramVideo += direction;
					swissSettings.sramVideo = ((s8)swissSettings.sramVideo + 3) % 3;
				}
			break;
			case SET_SCREEN_POS:
				if(in_range(swissSettings.aveCompat, GCDIGITAL_COMPAT, GCVIDEO_COMPAT)) {
					swissSettings.sramHOffset /= 2;
					swissSettings.sramHOffset += direction;
					swissSettings.sramHOffset *= 2;
				}
				else {
					swissSettings.sramHOffset += direction;
				}
				VIDEO_SetAdjustingValues(swissSettings.sramHOffset, 0);
			break;
			case SET_SYS_LANG:
				swissSettings.sramLanguage += direction;
				swissSettings.sramLanguage = ((s8)swissSettings.sramLanguage + SRAM_LANGUAGE_MAX) % SRAM_LANGUAGE_MAX;
			break;
			case SET_CONFIG_DEV:
			{
				int curDevicePos = -1;

				// Set it to the first writable device available
				if(swissSettings.configDeviceId == DEVICE_ID_UNK) {
					for(int i = 0; i < MAX_DEVICES; i++) {
						if(allDevices[i] != NULL && (allDevices[i]->features & FEAT_CONFIG_DEVICE)) {
							swissSettings.configDeviceId = allDevices[i]->deviceUniqueId;
							return;
						}
					}
				}

				// get position in allDevices for current save device
				for(int i = 0; i < MAX_DEVICES; i++) {
					if(allDevices[i] != NULL && allDevices[i]->deviceUniqueId == swissSettings.configDeviceId) {
						curDevicePos = i;
						break;
					}
				}

				if(curDevicePos >= 0) {
					if(direction > 0) {
						curDevicePos = allDevices[curDevicePos+1] == NULL ? 0 : curDevicePos+1;
					}
					else {
						curDevicePos = curDevicePos > 0 ? curDevicePos-1 : 0;
					}
					// Go to next writable device
					while((allDevices[curDevicePos] == NULL) || !(allDevices[curDevicePos]->features & FEAT_CONFIG_DEVICE)) {
						curDevicePos += direction;
						curDevicePos = (curDevicePos + MAX_DEVICES) % MAX_DEVICES;
					}
					if(allDevices[curDevicePos] != NULL) {
						swissSettings.configDeviceId = allDevices[curDevicePos]->deviceUniqueId;
					}
				}
			}
			break;
			case SET_SWISS_VIDEOMODE:
				swissSettings.uiVMode += direction;
				swissSettings.uiVMode = (swissSettings.uiVMode + 7) % 7;
			break;
			case SET_INIT_DRIVE:
				if(deviceHandler_getDeviceAvailable(&__device_dvd))
					swissSettings.initDVDDriveAtStart ^= 1;
			break;
			case SET_STOP_MOTOR:
				if(deviceHandler_getDeviceAvailable(&__device_dvd))
					swissSettings.stopMotor ^= 1;
			break;
			case SET_AUDIO_BUFFER:
				if(deviceHandler_getDeviceAvailable(&__device_dvd)) {
					swissSettings.configAudioBuffer += direction;
					swissSettings.configAudioBuffer = (swissSettings.configAudioBuffer + 3) % 3;
				}
			break;
			case SET_EXI_SPEED:
				swissSettings.exiSpeed ^= 1;
			break;
			case SET_AVE_COMPAT:
				swissSettings.aveCompat += direction;
				swissSettings.aveCompat = (swissSettings.aveCompat + AVE_COMPAT_MAX) % AVE_COMPAT_MAX;
			break;
			case SET_FORCE_DTVSTATUS:
				if(!in_range(swissSettings.aveCompat, AVE_N_DOL_COMPAT, AVE_P_DOL_COMPAT)) {
					swissSettings.forceDTVStatus += direction;
					swissSettings.forceDTVStatus = (swissSettings.forceDTVStatus + 3) % 3;
				}
			break;
			case SET_RT4K_OPTIM:
				if(in_range(swissSettings.aveCompat, GCDIGITAL_COMPAT, GCVIDEO_COMPAT)) {
					swissSettings.rt4kOptim ^= 1;
					switch(swissSettings.aveCompat) {
						case GCDIGITAL_COMPAT:
							sprintf(txtbuffer, "In the \223GCDigital Settings\224 menu,\nset \223Use console DE\224 to %s.", swissSettings.rt4kOptim ? "on (4:3)" : "off");
						break;
						case GCVIDEO_COMPAT:
							sprintf(txtbuffer, "In the GCVideo \223Advanced Settings\224\nmenu, set \223Fix Resolution\224 to %s.", swissSettings.rt4kOptim ? "Off" : "On");
						break;
					}
					uiDrawObj_t *msgBox = DrawPublish(DrawMessageBox(D_INFO, txtbuffer));
					wait_press_A();
					DrawDispose(msgBox);
				}
			break;
			case SET_DISABLE_RECALIB:
				swissSettings.disableRecalibration ^= 1;
			break;
			case SET_DISABLE_RUMBLE:
				swissSettings.disableRumble ^= 1;
			break;
			case SET_ENABLE_USBGECKO:
				swissSettings.enableUSBGecko += direction;
				swissSettings.enableUSBGecko = (swissSettings.enableUSBGecko + USBGECKO_MAX) % USBGECKO_MAX;
			break;
			case SET_WAIT_USBGECKO:
				swissSettings.waitForUSBGecko ^= 1;
			break;
			case SET_SIMMEMSIZE:
				do {
					swissSettings.simulatedMemSize += direction;
					swissSettings.simulatedMemSize = (swissSettings.simulatedMemSize + 6) % 6;
				} while(simulatedMemSizeInt[swissSettings.simulatedMemSize] > SYS_GetPhysicalMemSize());
			break;
			case SET_TAU_CALIB:
				if(is_gamecube()) {
					swissSettings.sramTemperature += direction * 4;
					if(swissSettings.sramTemperature < -80) swissSettings.sramTemperature = -80;
					if(swissSettings.sramTemperature > +80) swissSettings.sramTemperature = +80;
					__SYS_SetTAUCalibration(swissSettings.sramTemperature);
				}
			break;
		}
		switch(option) {
			case SET_SYS_VIDEO:
			case SET_SWISS_VIDEOMODE:
			case SET_AVE_COMPAT:
			case SET_FORCE_DTVSTATUS:
			case SET_RT4K_OPTIM:
			{
				// Change Swiss video mode if it was modified.
				GXRModeObj *forcedMode = getVideoModeFromSwissSetting(swissSettings.uiVMode);
				DrawVideoMode(forcedMode);
			}
			break;
		}
	}
	else if(page == PAGE_INTERFACE) {
		switch(option) {
			case SET_FILEBROWSER_TYPE:
				swissSettings.fileBrowserType += direction;
				swissSettings.fileBrowserType = (swissSettings.fileBrowserType + BROWSER_MAX) % BROWSER_MAX;
			break;
			case SET_APPSBROWSER_TYPE:
				swissSettings.appsBrowserType += direction;
				swissSettings.appsBrowserType = (swissSettings.appsBrowserType + BROWSER_MAX) % BROWSER_MAX;
			break;
			case SET_GAMEBROWSER_TYPE:
				swissSettings.gameBrowserType += direction;
				swissSettings.gameBrowserType = (swissSettings.gameBrowserType + BROWSER_MAX) % BROWSER_MAX;
			break;
			case SET_FILE_MGMT:
				swissSettings.enableFileManagement ^= 1;
			break;
			case SET_RECENT_LIST:
				swissSettings.recentListLevel += direction;
				swissSettings.recentListLevel = (swissSettings.recentListLevel + 3) % 3;
			break;
			case SET_SHOW_HIDDEN:
				swissSettings.showHiddenFiles ^= 1;
			break;
			case SET_HIDE_UNK:
				swissSettings.hideUnknownFileTypes ^= 1;
			break;
			case SET_UI_ANIMS:
			{
				uiMotionMode_t mode = UIMotion_CycleMode(
					UIMotion_ModeFromFlags(swissSettings.disableUIAnimations,
						swissSettings.reduceUIAnimations), direction);
				swissSettings.disableUIAnimations = mode == UI_MOTION_OFF;
				swissSettings.reduceUIAnimations = mode == UI_MOTION_REDUCED;
			}
			break;
			case SET_UI_COLOR:
				swissSettings.uiColor += direction;
				swissSettings.uiColor = (swissSettings.uiColor + UI_COLOR_MAX) % UI_COLOR_MAX;
			break;
			case SET_PANEL_TRANSPARENCY:
				swissSettings.disablePanelTransparency ^= 1;
			break;
			case SET_ANIMATED_BACKDROP:
				swissSettings.disableAnimatedBackdrop ^= 1;
			break;
			case SET_MENU_MUSIC:
				swissSettings.disableMenuMusic ^= 1;
				menuaudio_apply_settings();
			break;
			case SET_MENU_SFX:
				swissSettings.disableMenuSFX ^= 1;
				menuaudio_apply_settings();
			break;
			case SET_AUTOBOOT:
				swissSettings.autoBoot ^= 1;
			break;
			case SET_FLATTEN_DIR:
				DrawGetTextEntry(ENTRYMODE_NUMERIC|ENTRYMODE_ALPHA, "Flatten directory", &swissSettings.flattenDir, sizeof(swissSettings.flattenDir) - 1);
			break;
		}
	}
	else if(page == PAGE_NETWORK) {
		switch(option) {
			case SET_INIT_NET:
				if(!bba_requires_init())
					swissSettings.initNetworkAtStart ^= 1;
			break;
			case SET_BBA_LOCALIP:
				DrawGetTextEntry(ENTRYMODE_IP, "IPv4 Address", &swissSettings.bbaLocalIp, sizeof(swissSettings.bbaLocalIp) - 1);
			break;
			case SET_BBA_NETMASK:
				DrawGetTextEntry(ENTRYMODE_NUMERIC, "IPv4 Netmask", &swissSettings.bbaNetmask, 2);
			break;
			case SET_BBA_GATEWAY:
				DrawGetTextEntry(ENTRYMODE_IP, "IPv4 Gateway", &swissSettings.bbaGateway, sizeof(swissSettings.bbaGateway) - 1);
			break;
			case SET_BBA_DHCP:
				swissSettings.bbaUseDhcp ^= 1;
			break;
			case SET_FSP_HOSTIP:
				DrawGetTextEntry(ENTRYMODE_IP, "FSP Host IP", &swissSettings.fspHostIp, sizeof(swissSettings.fspHostIp) - 1);
			break;
			case SET_FSP_PORT:
				DrawGetTextEntry(ENTRYMODE_NUMERIC, "FSP Port", &swissSettings.fspPort, 5);
			break;
			case SET_FSP_PASS:
				DrawGetTextEntry(ENTRYMODE_NUMERIC|ENTRYMODE_ALPHA|ENTRYMODE_MASKED, "FSP Password", &swissSettings.fspPassword, sizeof(swissSettings.fspPassword) - 1);
			break;
			case SET_FSP_PMTU:
				DrawGetTextEntry(ENTRYMODE_NUMERIC, "FSP Path MTU", &swissSettings.fspPathMtu, 4);
			break;
			case SET_FTP_HOSTIP:
				DrawGetTextEntry(ENTRYMODE_IP, "FTP Host IP", &swissSettings.ftpHostIp, sizeof(swissSettings.ftpHostIp) - 1);
			break;
			case SET_FTP_PORT:
				DrawGetTextEntry(ENTRYMODE_NUMERIC, "FTP Port", &swissSettings.ftpPort, 5);
			break;
			case SET_FTP_USER:
				DrawGetTextEntry(ENTRYMODE_NUMERIC|ENTRYMODE_ALPHA, "FTP Username", &swissSettings.ftpUserName, sizeof(swissSettings.ftpUserName) - 1);
			break;
			case SET_FTP_PASS:
				DrawGetTextEntry(ENTRYMODE_NUMERIC|ENTRYMODE_ALPHA|ENTRYMODE_MASKED, "FTP Password", &swissSettings.ftpPassword, sizeof(swissSettings.ftpPassword) - 1);
			break;
			case SET_FTP_PASV:
				swissSettings.ftpUsePasv ^= 1;
			break;
			case SET_SMB_HOSTIP:
				DrawGetTextEntry(ENTRYMODE_IP, "SMB Host IP", &swissSettings.smbServerIp, sizeof(swissSettings.smbServerIp) - 1);
			break;
			case SET_SMB_SHARE:
				DrawGetTextEntry(ENTRYMODE_NUMERIC|ENTRYMODE_ALPHA, "SMB Share", &swissSettings.smbShare, sizeof(swissSettings.smbShare) - 1);
			break;
			case SET_SMB_USER:
				DrawGetTextEntry(ENTRYMODE_NUMERIC|ENTRYMODE_ALPHA, "SMB Username", &swissSettings.smbUser, sizeof(swissSettings.smbUser) - 1);
			break;
			case SET_SMB_PASS:
				DrawGetTextEntry(ENTRYMODE_NUMERIC|ENTRYMODE_ALPHA|ENTRYMODE_MASKED, "SMB Password", &swissSettings.smbPassword, sizeof(swissSettings.smbPassword) - 1);
			break;
			case SET_RT4K_HOSTIP:
				DrawGetTextEntry(ENTRYMODE_IP, "RetroTINK-4K Host IP", &swissSettings.rt4kHostIp, sizeof(swissSettings.rt4kHostIp) - 1);
			break;
			case SET_RT4K_PORT:
				DrawGetTextEntry(ENTRYMODE_NUMERIC, "RetroTINK-4K Port", &swissSettings.rt4kPort, 5);
			break;
		}
	}
	else if(page == PAGE_GAME_GLOBAL) {
		switch(option) {
			case SET_IGR:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->features & FEAT_HYPERVISOR)) {
					swissSettings.igrType += direction;
					swissSettings.igrType = (swissSettings.igrType + 3) % 3;
				}
			break;
			case SET_BS2BOOT:
				swissSettings.bs2Boot += direction;
				swissSettings.bs2Boot = (swissSettings.bs2Boot + 4) % 4;
			break;
			case SET_EMULATE_MEMCARD:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_MEMCARD))
					swissSettings.emulateMemoryCard ^= 1;
			break;
			case SET_DISABLE_MCPGAMEID:
				swissSettings.disableMCPGameID += direction;
				swissSettings.disableMCPGameID = (swissSettings.disableMCPGameID + 4) % 4;
			break;
			case SET_DISABLE_VIDPATCH:
				swissSettings.disableVideoPatches += direction;
				swissSettings.disableVideoPatches = (swissSettings.disableVideoPatches + 3) % 3;
			break;
			case SET_FORCE_VIDACTIVE:
				if(swissSettings.disableVideoPatches < 2)
					swissSettings.forceVideoActive ^= 1;
			break;
			case SET_PAUSE_AVOUTPUT:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->features & FEAT_HYPERVISOR))
					swissSettings.pauseAVOutput ^=1;
			break;
			case SET_ALL_CHEATS:
				swissSettings.autoCheats ^=1;
			break;
			case SET_WIIRDDBG:
				if(devices[DEVICE_CUR] != &__device_usbgecko && deviceHandler_getDeviceAvailable(&__device_usbgecko))
					swissSettings.wiirdDebug ^=1;
			break;
			case SET_GLOBAL_DEFAULTS:
				if(direction == 0) {
					swissSettings.igrType = 0;
					swissSettings.bs2Boot = 0;
					swissSettings.emulateMemoryCard = 0;
					swissSettings.disableMCPGameID = 0;
					swissSettings.disableVideoPatches = 0;
					swissSettings.forceVideoActive = 0;
					swissSettings.pauseAVOutput = 0;
					swissSettings.autoCheats = 0;
					swissSettings.wiirdDebug = 0;
				}
			break;
		}
	}
	else if(page == PAGE_GAME_DEFAULTS) {
		switch(option) {
			case SET_DEFAULT_NTSC_VIDEOMODE:
				if(swissSettings.disableVideoPatches < 2) {
					swissSettings.gameVModeNtsc += direction;
					swissSettings.gameVModeNtsc = (swissSettings.gameVModeNtsc + 8) % 8;
					if(!getDTVStatus()) {
						while(in_range(swissSettings.gameVModeNtsc, 4, 7)) {
							swissSettings.gameVModeNtsc += direction;
							swissSettings.gameVModeNtsc = (swissSettings.gameVModeNtsc + 8) % 8;
						}
					}
					else if(swissSettings.aveCompat != CMPV_DOL_COMPAT) {
						while(in_range(swissSettings.gameVModeNtsc, 6, 7)) {
							swissSettings.gameVModeNtsc += direction;
							swissSettings.gameVModeNtsc = (swissSettings.gameVModeNtsc + 8) % 8;
						}
					}
				}
			break;
			case SET_DEFAULT_PAL_VIDEOMODE:
				if(swissSettings.disableVideoPatches < 2) {
					swissSettings.gameVModePal += direction;
					swissSettings.gameVModePal = (swissSettings.gameVModePal + 15) % 15;
					if(!getDTVStatus()) {
						while(in_range(swissSettings.gameVModePal, 4, 7) || in_range(swissSettings.gameVModePal, 11, 14)) {
							swissSettings.gameVModePal += direction;
							swissSettings.gameVModePal = (swissSettings.gameVModePal + 15) % 15;
						}
					}
					else if(swissSettings.aveCompat != CMPV_DOL_COMPAT) {
						while(in_range(swissSettings.gameVModePal, 6, 7) || in_range(swissSettings.gameVModePal, 13, 14)) {
							swissSettings.gameVModePal += direction;
							swissSettings.gameVModePal = (swissSettings.gameVModePal + 15) % 15;
						}
					}
				}
			break;
			case SET_DEFAULT_HORIZ_SCALE:
				if(swissSettings.disableVideoPatches < 2) {
					swissSettings.forceHScale += direction;
					swissSettings.forceHScale = (swissSettings.forceHScale + 9) % 9;
				}
			break;
			case SET_DEFAULT_VERT_OFFSET:
				if(swissSettings.disableVideoPatches < 2)
					swissSettings.forceVOffset += direction;
			break;
			case SET_DEFAULT_VERT_FILTER:
				if(swissSettings.disableVideoPatches < 2) {
					swissSettings.forceVFilter += direction;
					swissSettings.forceVFilter = (swissSettings.forceVFilter + 4) % 4;
				}
			break;
			case SET_DEFAULT_FIELD_RENDER:
				if(swissSettings.disableVideoPatches < 2) {
					swissSettings.forceVJitter += direction;
					swissSettings.forceVJitter = (swissSettings.forceVJitter + 4) % 4;
				}
			break;
			case SET_DEFAULT_PIXEL_CENTER:
				if(swissSettings.disableVideoPatches < 2) {
					swissSettings.fixPixelCenter += direction;
					swissSettings.fixPixelCenter = (swissSettings.fixPixelCenter + 3) % 3;
				}
			break;
			case SET_DEFAULT_ALPHA_DITHER:
				if(swissSettings.disableVideoPatches < 2)
					swissSettings.disableDithering ^= 1;
			break;
			case SET_DEFAULT_ANISO_FILTER:
				swissSettings.forceAnisotropy ^= 1;
			break;
			case SET_DEFAULT_WIDESCREEN:
				swissSettings.forceWidescreen += direction;
				swissSettings.forceWidescreen = (swissSettings.forceWidescreen + 3) % 3;
			break;
			case SET_DEFAULT_POLL_RATE:
				swissSettings.forcePollRate += direction;
				swissSettings.forcePollRate = (swissSettings.forcePollRate + 13) % 13;
			break;
			case SET_DEFAULT_INVERT_CAMERA:
				swissSettings.invertCStick += direction;
				swissSettings.invertCStick = (swissSettings.invertCStick + 4) % 4;
			break;
			case SET_DEFAULT_SWAP_CAMERA:
				swissSettings.swapCStick += direction;
				swissSettings.swapCStick = (swissSettings.swapCStick + 4) % 4;
			break;
			case SET_DEFAULT_TRIGGER_LEVEL:
				swissSettings.triggerLevel += direction * 10;
				swissSettings.triggerLevel = (swissSettings.triggerLevel + 210) % 210;
			break;
			case SET_DEFAULT_AUDIO_STREAM:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_AUDIO_STREAMING)) {
					swissSettings.emulateAudioStream += direction;
					swissSettings.emulateAudioStream = (swissSettings.emulateAudioStream + 3) % 3;
				}
			break;
			case SET_DEFAULT_READ_SPEED:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_READ_SPEED)) {
					swissSettings.emulateReadSpeed += direction;
					swissSettings.emulateReadSpeed = (swissSettings.emulateReadSpeed + 3) % 3;
				}
			break;
			case SET_DEFAULT_EMULATE_ETHERNET:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_ETHERNET))
					swissSettings.emulateEthernet ^= 1;
			break;
			case SET_DEFAULT_DISABLE_MEMCARD:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->features & FEAT_HYPERVISOR)) {
					swissSettings.disableMemoryCard += direction;
					swissSettings.disableMemoryCard = (swissSettings.disableMemoryCard + 3) % 3;
				}
			break;
			case SET_DEFAULT_DISABLE_HYPERVISOR:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->location & LOC_DVD_CONNECTOR))
					swissSettings.disableHypervisor ^= 1;
			break;
			case SET_DEFAULT_CLEAN_BOOT:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->location & LOC_DVD_CONNECTOR))
					swissSettings.preferCleanBoot ^= 1;
			break;
			case SET_DEFAULT_RT4K_PROFILE:
				if(is_rt4k_alive()) {
					swissSettings.rt4kProfile += direction;
					swissSettings.rt4kProfile = (swissSettings.rt4kProfile + 13) % 13;
				}
			break;
			case SET_DEFAULT_DEFAULTS:
				if(direction == 0) {
					swissSettings.gameVModeNtsc = 0;
					swissSettings.gameVModePal = 0;
					swissSettings.forceHScale = 0;
					swissSettings.forceVOffset = 0;
					swissSettings.forceVFilter = 0;
					swissSettings.forceVJitter = 0;
					swissSettings.fixPixelCenter = 0;
					swissSettings.disableDithering = 0;
					swissSettings.forceAnisotropy = 0;
					swissSettings.forceWidescreen = 0;
					swissSettings.forcePollRate = 0;
					swissSettings.invertCStick = 0;
					swissSettings.swapCStick = 0;
					swissSettings.triggerLevel = 0;
					swissSettings.emulateAudioStream = 1;
					swissSettings.emulateReadSpeed = 0;
					swissSettings.emulateEthernet = 0;
					swissSettings.disableMemoryCard = 0;
					swissSettings.disableHypervisor = 0;
					swissSettings.preferCleanBoot = 0;
					swissSettings.rt4kProfile = 0;
				}
			break;
		}
		if ((option == SET_DEFAULT_AUDIO_STREAM && swissSettings.emulateAudioStream > 1) ||
			(option == SET_DEFAULT_EMULATE_ETHERNET && swissSettings.emulateEthernet)) {
			uiDrawObj_t *msgBox = DrawPublish(DrawMessageBox(D_WARN, "Turning this on globally may cause certain\ngames to crash from resource exhaustion."));
			wait_press_A();
			DrawDispose(msgBox);
		}
	}
	else if(page == PAGE_GAME && gameConfig != NULL && !gameConfig->forceCleanBoot) {
		switch(option) {
			case SET_GAME_LANG:
				gameConfig->gameLanguage += direction;
				gameConfig->gameLanguage = (gameConfig->gameLanguage + 9) % 9;
			break;
			case SET_FORCE_VIDEOMODE:
				if(swissSettings.disableVideoPatches < 2) {
					gameConfig->gameVMode += direction;
					gameConfig->gameVMode = (gameConfig->gameVMode + 15) % 15;
					if(!getDTVStatus()) {
						while(in_range(gameConfig->gameVMode, 4, 7) || in_range(gameConfig->gameVMode, 11, 14)) {
							gameConfig->gameVMode += direction;
							gameConfig->gameVMode = (gameConfig->gameVMode + 15) % 15;
						}
					}
					else if(swissSettings.aveCompat != CMPV_DOL_COMPAT) {
						while(in_range(gameConfig->gameVMode, 6, 7) || in_range(gameConfig->gameVMode, 13, 14)) {
							gameConfig->gameVMode += direction;
							gameConfig->gameVMode = (gameConfig->gameVMode + 15) % 15;
						}
					}
				}
			break;
			case SET_HORIZ_SCALE:
				if(swissSettings.disableVideoPatches < 2) {
					gameConfig->forceHScale += direction;
					gameConfig->forceHScale = (gameConfig->forceHScale + 9) % 9;
				}
			break;
			case SET_VERT_OFFSET:
				if(swissSettings.disableVideoPatches < 2)
					gameConfig->forceVOffset += direction;
			break;
			case SET_VERT_FILTER:
				if(swissSettings.disableVideoPatches < 2) {
					gameConfig->forceVFilter += direction;
					gameConfig->forceVFilter = (gameConfig->forceVFilter + 4) % 4;
				}
			break;
			case SET_FIELD_RENDER:
				if(swissSettings.disableVideoPatches < 2) {
					gameConfig->forceVJitter += direction;
					gameConfig->forceVJitter = (gameConfig->forceVJitter + 4) % 4;
				}
			break;
			case SET_PIXEL_CENTER:
				if(swissSettings.disableVideoPatches < 2) {
					gameConfig->fixPixelCenter += direction;
					gameConfig->fixPixelCenter = (gameConfig->fixPixelCenter + 3) % 3;
				}
			break;
			case SET_ALPHA_DITHER:
				if(swissSettings.disableVideoPatches < 2)
					gameConfig->disableDithering ^= 1;
			break;
			case SET_ANISO_FILTER:
				gameConfig->forceAnisotropy ^= 1;
			break;
			case SET_WIDESCREEN:
				gameConfig->forceWidescreen += direction;
				gameConfig->forceWidescreen = (gameConfig->forceWidescreen + 3) % 3;
			break;
			case SET_POLL_RATE:
				gameConfig->forcePollRate += direction;
				gameConfig->forcePollRate = (gameConfig->forcePollRate + 13) % 13;
			break;
			case SET_INVERT_CAMERA:
				gameConfig->invertCStick += direction;
				gameConfig->invertCStick = (gameConfig->invertCStick + 4) % 4;
			break;
			case SET_SWAP_CAMERA:
				gameConfig->swapCStick += direction;
				gameConfig->swapCStick = (gameConfig->swapCStick + 4) % 4;
			break;
			case SET_TRIGGER_LEVEL:
				gameConfig->triggerLevel += direction * 10;
				gameConfig->triggerLevel = (gameConfig->triggerLevel + 210) % 210;
			break;
			case SET_AUDIO_STREAM:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_AUDIO_STREAMING)) {
					gameConfig->emulateAudioStream += direction;
					gameConfig->emulateAudioStream = (gameConfig->emulateAudioStream + 3) % 3;
				}
			break;
			case SET_READ_SPEED:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_READ_SPEED)) {
					gameConfig->emulateReadSpeed += direction;
					gameConfig->emulateReadSpeed = (gameConfig->emulateReadSpeed + 3) % 3;
				}
			break;
			case SET_EMULATE_ETHERNET:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_ETHERNET))
					gameConfig->emulateEthernet ^= 1;
			break;
			case SET_DISABLE_MEMCARD:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->features & FEAT_HYPERVISOR)) {
					gameConfig->disableMemoryCard += direction;
					gameConfig->disableMemoryCard = (gameConfig->disableMemoryCard + 3) % 3;
				}
			break;
			case SET_DISABLE_HYPERVISOR:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->location & LOC_DVD_CONNECTOR))
					gameConfig->disableHypervisor ^= 1;
			break;
			case SET_CLEAN_BOOT:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->location & LOC_DVD_CONNECTOR))
					gameConfig->preferCleanBoot ^= 1;
			break;
			case SET_RT4K_PROFILE:
				if(is_rt4k_alive()) {
					gameConfig->rt4kProfile += direction;
					gameConfig->rt4kProfile = (gameConfig->rt4kProfile + 13) % 13;
				}
			break;
			case SET_DEFAULTS:
				if(direction == 0)
					settingsResetGame(gameConfig);
			break;
		}
	}
}

#define SETTINGS_DIGITAL_INPUT_MASK (BUTTON_RIGHT | BUTTON_LEFT | \
	BUTTON_UP | BUTTON_DOWN | BUTTON_B | BUTTON_A | BUTTON_Y | \
	BUTTON_R | BUTTON_L | BUTTON_X)
#define SETTINGS_MENU_INPUT_POLICY (UI_MENU_INPUT_AXIS_BOTH | \
	UI_MENU_INPUT_REPEAT)

static u32 settingsMenuInputElapsedMicroseconds(u32 *lastRetrace)
{
	u32 currentRetrace;
	u32 elapsedRetraces;
	float retraceRate;
	float elapsed;

	if(lastRetrace == NULL) {
		return 0u;
	}
	currentRetrace = VIDEO_GetRetraceCount();
	elapsedRetraces = currentRetrace - *lastRetrace;
	*lastRetrace = currentRetrace;
	if(elapsedRetraces == 0u) {
		return 0u;
	}
	retraceRate = VIDEO_GetRetraceRate();
	if(!isfinite(retraceRate) || retraceRate < 1.0f) {
		retraceRate = 60.0f;
	}
	elapsed = (float)elapsedRetraces * 1000000.0f / retraceRate;
	/* The mapper intentionally consumes no more than 50 ms per poll. Clamp
	 * before lrintf so a long menu pause cannot overflow target 32-bit long. */
	if(elapsed >= (float)UI_MENU_INPUT_MAX_ELAPSED_US) {
		return UI_MENU_INPUT_MAX_ELAPSED_US;
	}
	return elapsed <= 0.0f ? 0u : (u32)lrintf(elapsed);
}

static u32 settingsButtonForMenuDirection(uiMenuInputDirection_t direction)
{
	switch(direction) {
		case UI_MENU_INPUT_LEFT:
			return BUTTON_LEFT;
		case UI_MENU_INPUT_RIGHT:
			return BUTTON_RIGHT;
		case UI_MENU_INPUT_UP:
			return BUTTON_UP;
		case UI_MENU_INPUT_DOWN:
			return BUTTON_DOWN;
		case UI_MENU_INPUT_NONE:
		default:
			return 0u;
	}
}

static u32 settingsWaitForInput(uiMenuInputState_t *menuInput,
	u32 *lastRetrace, bool *wasDigital)
{
	while(1) {
		u32 buttons = padsButtonsHeld() & SETTINGS_DIGITAL_INPUT_MASK;
		uiMenuInputDirection_t analog = padsMenuInputPoll(menuInput,
			settingsMenuInputElapsedMicroseconds(lastRetrace),
			SETTINGS_MENU_INPUT_POLICY, buttons != 0u);

		if(buttons != 0u) {
			*wasDigital = true;
			return buttons;
		}
		buttons = settingsButtonForMenuDirection(analog);
		if(buttons != 0u) {
			*wasDigital = false;
			return buttons;
		}
		VIDEO_WaitVSync();
	}
}

/* Keep sampling inhibited through the release edge. This consumes a stick
 * moved during a digital hold instead of leaking a deferred analog action. */
static void settingsInhibitThroughDigitalRelease(
	uiMenuInputState_t *menuInput, u32 *lastRetrace)
{
	while(1) {
		u32 buttons = padsButtonsHeld() & SETTINGS_DIGITAL_INPUT_MASK;
		(void)padsMenuInputPoll(menuInput,
			settingsMenuInputElapsedMicroseconds(lastRetrace),
			SETTINGS_MENU_INPUT_POLICY, true);
		if(buttons == 0u) {
			return;
		}
		VIDEO_WaitVSync();
	}
}

/* A held D-pad direction repeats on the stick's schedule: once after
 * UI_MENU_INPUT_INITIAL_REPEAT_US, then every UI_MENU_INPUT_REPEAT_US. Returns
 * true when the same directions are still held at that point; any other
 * button, or letting go, ends the repeat. */
static bool settingsHoldToRepeat(uiMenuInputState_t *menuInput,
	u32 *lastRetrace, u32 held, bool *repeating)
{
	const u32 directions = BUTTON_UP | BUTTON_DOWN | BUTTON_LEFT |
		BUTTON_RIGHT;
	u32 due = *repeating ? UI_MENU_INPUT_REPEAT_US :
		UI_MENU_INPUT_INITIAL_REPEAT_US;
	u32 waited = 0u;

	*repeating = false;
	if(held == 0u || (held & ~directions) != 0u) {
		return false;
	}
	while((padsButtonsHeld() & SETTINGS_DIGITAL_INPUT_MASK) == held) {
		u32 elapsed = settingsMenuInputElapsedMicroseconds(lastRetrace);

		(void)padsMenuInputPoll(menuInput, elapsed,
			SETTINGS_MENU_INPUT_POLICY, true);
		waited += elapsed;
		if(waited >= due) {
			*repeating = true;
			return true;
		}
		VIDEO_WaitVSync();
	}
	return false;
}

/* Rows whose change re-applies the Swiss video mode immediately. */
static bool settingsIsLiveVideoRow(int page, int option)
{
	return page == PAGE_GLOBAL && (option == SET_SYS_VIDEO ||
		option == SET_SWISS_VIDEOMODE || option == SET_AVE_COMPAT ||
		option == SET_FORCE_DTVSTATUS || option == SET_RT4K_OPTIM);
}

/* Rows A activates as an action (text entry, reset) instead of changing a
 * value; every other row is a choice that A advances like Right. */
static bool settingsRowIsAction(int page, int option)
{
	switch(page) {
		case PAGE_INTERFACE:
			return option == SET_FLATTEN_DIR;
		case PAGE_NETWORK:
			return in_range(option, SET_BBA_LOCALIP, SET_BBA_GATEWAY) ||
				in_range(option, SET_FSP_HOSTIP, SET_FTP_PASS) ||
				in_range(option, SET_SMB_HOSTIP, SET_RT4K_PORT);
		case PAGE_GAME_GLOBAL:
			return option == SET_GLOBAL_DEFAULTS;
		case PAGE_GAME_DEFAULTS:
			return option == SET_DEFAULT_DEFAULTS;
		case PAGE_GAME:
			return option == SET_DEFAULTS;
	}
	return false;
}

/* Rows that put a whole screen back to its defaults. */
static bool settingsRowIsReset(int page, int option)
{
	return (page == PAGE_GAME_GLOBAL && option == SET_GLOBAL_DEFAULTS) ||
		(page == PAGE_GAME_DEFAULTS && option == SET_DEFAULT_DEFAULTS) ||
		(page == PAGE_GAME && option == SET_DEFAULTS);
}

/* A reset undoes every row on the screen at once, so it asks first. */
static bool settingsConfirmReset(void)
{
	bool released = false;
	bool confirmed = false;
	uiDrawObj_t *box = DrawPublish(DrawMessageBox(D_WARN,
		"Reset everything on this screen to its default?\n"
		"A  RESET    B  KEEP"));

	while(1) {
		u32 held = padsButtonsHeld() & SETTINGS_DIGITAL_INPUT_MASK;

		/* The press that chose the row must be let go first. */
		if(!released) {
			released = held == 0u;
		}
		else if(held & BUTTON_A) {
			confirmed = true;
			break;
		}
		else if(held & BUTTON_B) {
			break;
		}
		VIDEO_WaitVSync();
	}
	DrawDispose(box);
	return confirmed;
}

/* True once anything differs from the snapshot taken when Settings opened. */
static bool settingsChanged(const ConfigEntry *config)
{
	return memcmp(&swissSettings, &tempSettings, sizeof(SwissSettings)) != 0 ||
		(config != NULL && memcmp(config, &tempConfig, sizeof(ConfigEntry)) != 0);
}

#define SETTINGS_VIDEO_KEEP_US 10000000u

/* A live video change can leave the TV without a picture, so it only stays
 * if A is pressed within ten seconds. B, or no answer, changes it back. */
static bool settingsKeepVideoMode(const settingRowView_t *row)
{
	u32 retrace = VIDEO_GetRetraceCount();
	u32 waited = 0u;
	int shown = -1;
	bool released = false;
	bool keep = false;
	uiDrawObj_t *box = NULL;

	while(waited < SETTINGS_VIDEO_KEEP_US) {
		int seconds = (int)((SETTINGS_VIDEO_KEEP_US - waited + 999999u) /
			1000000u);
		u32 held = padsButtonsHeld() & SETTINGS_DIGITAL_INPUT_MASK;

		if(seconds != shown) {
			/* DrawMessageBox copies into txtbuffer, so format elsewhere. */
			char message[128];

			/* Three lines: a fourth runs past the 125 px box. The first names
			 * the new value, which the page behind still shows as the old
			 * one until the prompt closes. The last is the buttons, which
			 * DrawMessageBox draws as icons. */
			snprintf(message, sizeof(message), "Keep %s %s?\n"
				"It changes back by itself in %d s.\n"
				"A  KEEP    B  CHANGE BACK", row->label, row->value, seconds);
			box = box == NULL ?
				DrawPublish(DrawMessageBox(D_WARN, message)) :
				DrawRepublish(box, DrawMessageBox(D_WARN, message));
			shown = seconds;
		}
		/* The press that changed the mode must be let go first. */
		if(!released) {
			released = held == 0u;
		}
		else if(held & BUTTON_A) {
			keep = true;
			break;
		}
		else if(held & BUTTON_B) {
			break;
		}
		VIDEO_WaitVSync();
		waited += settingsMenuInputElapsedMicroseconds(&retrace);
	}
	DrawDispose(box);
	return keep;
}

/* Changes one row's value. Video rows ask to keep the new mode and restore
 * the fields that choose it when the answer is no. */
static void settingsChangeValue(int page, int option, int direction,
	ConfigEntry *config)
{
	if(!settingsIsLiveVideoRow(page, option)) {
		settings_toggle(page, option, direction, config);
		return;
	}
	GXRModeObj *before = getVideoMode();
	u8 sramVideo = swissSettings.sramVideo;
	int uiVMode = swissSettings.uiVMode;
	int aveCompat = swissSettings.aveCompat;
	int forceDTVStatus = swissSettings.forceDTVStatus;
	bool rt4kOptim = swissSettings.rt4kOptim;

	settings_toggle(page, option, direction, config);
	if(getVideoMode() == before) {
		return;
	}
	settingRowView_t row;
	settingsDescribeRow(page, option, config, &row);
	if(!settingsKeepVideoMode(&row)) {
		swissSettings.sramVideo = sramVideo;
		swissSettings.uiVMode = uiVMode;
		swissSettings.aveCompat = aveCompat;
		swissSettings.forceDTVStatus = forceDTVStatus;
		swissSettings.rt4kOptim = rt4kOptim;
		DrawVideoMode(before);
	}
}

/* Choice rows with four or more values: A lists them all to pick from.
 * Left and Right still step one value at a time. The field lets the list
 * show the values in order with the current one in place. */
typedef struct {
	u8 page;
	u8 option;
	bool game;	/* the field is in the game's ConfigEntry */
	u16 offset;
	u8 size;
} settingsPickerRow_t;

#define PICK_SETTING(page, option, field) {page, option, false, \
	offsetof(SwissSettings, field), sizeof(((SwissSettings *)0)->field)}
#define PICK_GAME(option, field) {PAGE_GAME, option, true, \
	offsetof(ConfigEntry, field), sizeof(((ConfigEntry *)0)->field)}
static const settingsPickerRow_t settingsPickerRows[] = {
	PICK_SETTING(PAGE_GLOBAL, SET_SYS_LANG, sramLanguage),
	PICK_SETTING(PAGE_GLOBAL, SET_ENABLE_USBGECKO, enableUSBGecko),
	PICK_SETTING(PAGE_GLOBAL, SET_SIMMEMSIZE, simulatedMemSize),
	PICK_SETTING(PAGE_INTERFACE, SET_UI_COLOR, uiColor),
	PICK_SETTING(PAGE_GAME_GLOBAL, SET_BS2BOOT, bs2Boot),
	PICK_SETTING(PAGE_GAME_GLOBAL, SET_DISABLE_MCPGAMEID, disableMCPGameID),
	PICK_SETTING(PAGE_GAME_DEFAULTS, SET_DEFAULT_NTSC_VIDEOMODE, gameVModeNtsc),
	PICK_SETTING(PAGE_GAME_DEFAULTS, SET_DEFAULT_PAL_VIDEOMODE, gameVModePal),
	PICK_SETTING(PAGE_GAME_DEFAULTS, SET_DEFAULT_HORIZ_SCALE, forceHScale),
	PICK_SETTING(PAGE_GAME_DEFAULTS, SET_DEFAULT_VERT_FILTER, forceVFilter),
	PICK_SETTING(PAGE_GAME_DEFAULTS, SET_DEFAULT_FIELD_RENDER, forceVJitter),
	PICK_SETTING(PAGE_GAME_DEFAULTS, SET_DEFAULT_POLL_RATE, forcePollRate),
	PICK_SETTING(PAGE_GAME_DEFAULTS, SET_DEFAULT_INVERT_CAMERA, invertCStick),
	PICK_SETTING(PAGE_GAME_DEFAULTS, SET_DEFAULT_SWAP_CAMERA, swapCStick),
	PICK_SETTING(PAGE_GAME_DEFAULTS, SET_DEFAULT_RT4K_PROFILE, rt4kProfile),
	PICK_GAME(SET_GAME_LANG, gameLanguage),
	PICK_GAME(SET_FORCE_VIDEOMODE, gameVMode),
	PICK_GAME(SET_HORIZ_SCALE, forceHScale),
	PICK_GAME(SET_VERT_FILTER, forceVFilter),
	PICK_GAME(SET_FIELD_RENDER, forceVJitter),
	PICK_GAME(SET_POLL_RATE, forcePollRate),
	PICK_GAME(SET_INVERT_CAMERA, invertCStick),
	PICK_GAME(SET_SWAP_CAMERA, swapCStick),
	PICK_GAME(SET_RT4K_PROFILE, rt4kProfile),
};
#undef PICK_SETTING
#undef PICK_GAME

static const settingsPickerRow_t *settingsPickerFor(int page, int option)
{
	size_t i;

	for(i = 0; i < sizeof(settingsPickerRows) / sizeof(settingsPickerRows[0]); i++) {
		if(settingsPickerRows[i].page == page &&
			settingsPickerRows[i].option == option) {
			return &settingsPickerRows[i];
		}
	}
	return NULL;
}

static u32 settingsPickerValue(const settingsPickerRow_t *pick,
	const ConfigEntry *config)
{
	const u8 *field = (pick->game ? (const u8 *)config :
		(const u8 *)&swissSettings) + pick->offset;
	u8 byte;
	u16 half;
	u32 word;

	switch(pick->size) {
		case 1: memcpy(&byte, field, 1); return byte;
		case 2: memcpy(&half, field, 2); return half;
		default: memcpy(&word, field, 4); return word;
	}
}

#define SETTINGS_PICKER_MAX 16
typedef struct {
	int count;
	int current;	/* the entry set when the list opened */
	u32 value[SETTINGS_PICKER_MAX];
	char text[SETTINGS_PICKER_MAX][32];
} settingsPicker_t;

/* Lists a row's values by stepping the settings with the same code Right
 * uses, so modes Right skips stay out, then puts the settings back. Values
 * with the same name (the two 150 Hz rates) are listed once. */
static int settingsPickerLoad(const settingsPickerRow_t *pick,
	ConfigEntry *config, settingsPicker_t *list)
{
	static SwissSettings savedSettings;
	static ConfigEntry savedConfig;
	u32 start = settingsPickerValue(pick, config);
	int step, i, j;

	memcpy(&savedSettings, &swissSettings, sizeof(SwissSettings));
	if(config != NULL) {
		memcpy(&savedConfig, config, sizeof(ConfigEntry));
	}
	list->count = 0;
	for(step = 0; step < SETTINGS_PICKER_MAX; step++) {
		settingRowView_t row;
		u32 value = settingsPickerValue(pick, config);
		bool named = false;

		if(step > 0 && value == start) {
			break;
		}
		settingsDescribeRow(pick->page, pick->option, config, &row);
		for(i = 0; i < list->count; i++) {
			named = named || !strcmp(list->text[i], row.value);
		}
		if(!named) {
			list->value[list->count] = value;
			snprintf(list->text[list->count], sizeof(list->text[0]), "%s", row.value);
			list->count++;
		}
		settings_toggle(pick->page, pick->option, 1, config);
		if(settingsPickerValue(pick, config) == value) {
			break;
		}
	}
	memcpy(&swissSettings, &savedSettings, sizeof(SwissSettings));
	if(config != NULL) {
		memcpy(config, &savedConfig, sizeof(ConfigEntry));
	}
	/* The walk began at the current value; list in value order. */
	for(i = 1; i < list->count; i++) {
		for(j = i; j > 0 && list->value[j - 1] > list->value[j]; j--) {
			u32 value = list->value[j];
			char text[sizeof(list->text[0])];

			memcpy(text, list->text[j], sizeof(text));
			list->value[j] = list->value[j - 1];
			memcpy(list->text[j], list->text[j - 1], sizeof(text));
			list->value[j - 1] = value;
			memcpy(list->text[j - 1], text, sizeof(text));
		}
	}
	list->current = 0;
	for(i = 0; i < list->count; i++) {
		if(list->value[i] == start) {
			list->current = i;
		}
	}
	return list->count;
}

#define SETTINGS_PICKER_ROWS 8
#define SETTINGS_PICKER_PITCH 28

static uiDrawObj_t *settingsDrawPicker(const char *label,
	const settingsPicker_t *list, int focus)
{
	int visible = MIN(list->count, SETTINGS_PICKER_ROWS);
	int first = MIN(MAX(0, focus - visible / 2), MAX(0, list->count - visible));
	bool scroll = list->count > visible;
	int x0 = 150, x1 = 490;
	int height = 44 + visible * SETTINGS_PICKER_PITCH + 34;
	int y0 = (480 - height) / 2, y1 = y0 + height;
	int rowsY = y0 + 44;
	int textRight = x1 - (scroll ? 34 : 20);
	GXColor panel = setPanelColor;
	char text[UI_SETLAYOUT_TEXT_BUFFER_SIZE];
	float scale;
	int i;
	uiDrawObj_t *box = DrawContainer();

	/* Solid, so the list reads over the rows behind it. */
	panel.a = SET_PANEL_SOLID_ALPHA;
	DrawAddChild(box, DrawEmptyColouredBox(x0, y0, x1, y1, panel));
	if(prepareSettingText(label, UI_SETLAYOUT_LABEL_BUFFER_SIZE - 1u,
		UI_SETLAYOUT_ELLIPSIZE_TAIL, UI_SETLAYOUT_TEXT_PLAIN, false, true,
		x1 - x0 - 40, set_row_size, text, sizeof(text), &scale)) {
		DrawAddChild(box, DrawStyledLabel(x0 + 20, y0 + 24, text, scale,
			ALIGN_LEFT, setSubtleColor));
	}
	for(i = first; i < first + visible; i++) {
		int y = rowsY + (i - first) * SETTINGS_PICKER_PITCH;
		bool focused = i == focus;

		if(focused) {
			/* Not DrawSettingsFocus: that is one animated focus shared with
			 * the page behind, and two targets would pull it between rows. */
			DrawAddChild(box, DrawTransparentBox(x0 + 12, y, textRight + 6,
				y + SETTINGS_PICKER_PITCH - 2));
			DrawAddChild(box, DrawStyledLabel(x0 + 20, y + 13, "\225",
				set_row_size, ALIGN_LEFT, defaultColor));
		}
		if(prepareSettingText(list->text[i], UI_SETLAYOUT_VALUE_TEXT_MAX,
			UI_SETLAYOUT_ELLIPSIZE_TAIL, UI_SETLAYOUT_TEXT_PLAIN, false, true,
			textRight - x0 - 110, focused ? set_row_sel_size : set_row_size,
			text, sizeof(text), &scale)) {
			DrawAddChild(box, DrawStyledLabel(x0 + 36, y + 13, text, scale,
				ALIGN_LEFT, focused ? defaultColor : setInactiveColor));
		}
		if(i == list->current) {
			DrawAddChild(box, DrawStyledLabel(textRight, y + 13, "Current",
				set_meta_size, ALIGN_RIGHT, setCustomColor));
		}
	}
	if(scroll) {
		DrawAddChild(box, DrawVertScrollBar(x1 - 24, rowsY, 8,
			visible * SETTINGS_PICKER_PITCH,
			(float)focus / (float)(list->count - 1),
			MAX(16, visible * SETTINGS_PICKER_PITCH * visible / list->count)));
	}
	DrawAddChild(box, DrawHintLabel((x0 + x1) / 2, y1 - 18,
		"A  CHOOSE    B  CANCEL", set_meta_size, ALIGN_CENTER, setSubtleColor));
	return box;
}

/* A on a picker row: every value in a list. A picks the focused one, B
 * keeps the current value. Returns whether the value changed. */
static bool settingsPick(const settingsPickerRow_t *pick, ConfigEntry *config,
	uiMenuInputState_t *menuInput, u32 *lastRetrace)
{
	static settingsPicker_t list;
	settingRowView_t row;
	uiDrawObj_t *box = NULL;
	bool repeating = false;
	int focus, step, chosen = -1;
	u32 target;

	settingsDescribeRow(pick->page, pick->option, config, &row);
	if(!row.enabled || settingsPickerLoad(pick, config, &list) < 2) {
		return false;
	}
	focus = list.current;
	/* The A that opened the list must not also choose from it. */
	settingsInhibitThroughDigitalRelease(menuInput, lastRetrace);
	while(1) {
		bool wasDigital;
		u32 btns;
		uiDrawObj_t *next = settingsDrawPicker(row.label, &list, focus);

		/* Menu Color's list shows each color as the focus reaches it. */
		if(pick->page == PAGE_INTERFACE && pick->option == SET_UI_COLOR) {
			DrawPreviewMenuColor((int)list.value[focus]);
		}
		box = box == NULL ? DrawPublish(next) : DrawRepublish(box, next);
		btns = settingsWaitForInput(menuInput, lastRetrace, &wasDigital);
		if(btns & BUTTON_UP) {
			focus = MAX(0, focus - 1);
		}
		if(btns & BUTTON_DOWN) {
			focus = MIN(list.count - 1, focus + 1);
		}
		if(btns & (BUTTON_A | BUTTON_B)) {
			chosen = (btns & BUTTON_A) ? focus : -1;
			settingsInhibitThroughDigitalRelease(menuInput, lastRetrace);
			break;
		}
		if(!wasDigital) {
			repeating = false;
		}
		else if(!settingsHoldToRepeat(menuInput, lastRetrace, btns, &repeating)) {
			settingsInhibitThroughDigitalRelease(menuInput, lastRetrace);
		}
	}
	DrawDispose(box);
	if(chosen < 0 || chosen == list.current) {
		return false;
	}
	/* Step to the chosen value with Right's code, as Right would. */
	target = list.value[chosen];
	for(step = 0; step < SETTINGS_PICKER_MAX &&
		settingsPickerValue(pick, config) != target; step++) {
		settings_toggle(pick->page, pick->option, 1, config);
	}
	return true;
}

/* These legacy setting arms open a child input/message loop. Reinitialize the
 * analog policy after they return so movement inside that child cannot become
 * a surprise Settings action. */
static bool settingsInputMayBlock(int page, int option, u32 buttons)
{
	bool horizontal = (buttons & (BUTTON_LEFT | BUTTON_RIGHT)) != 0u;
	bool activate = (buttons & BUTTON_A) != 0u;

	if(activate && settingsPickerFor(page, option) != NULL) {
		return true;
	}

	if(page == PAGE_GLOBAL) {
		return (horizontal || activate) && (option == SET_RT4K_OPTIM ||
			settingsIsLiveVideoRow(page, option));
	}
	if(page == PAGE_INTERFACE) {
		return (horizontal || activate) && option == SET_FLATTEN_DIR;
	}
	if(page == PAGE_NETWORK && (horizontal || activate)) {
		return in_range(option, SET_BBA_LOCALIP, SET_BBA_GATEWAY) ||
			in_range(option, SET_FSP_HOSTIP, SET_FTP_PASS) ||
			in_range(option, SET_SMB_HOSTIP, SET_RT4K_PORT);
	}
	if(page == PAGE_GAME_DEFAULTS && (horizontal || activate)) {
		return option == SET_DEFAULT_AUDIO_STREAM ||
			option == SET_DEFAULT_EMULATE_ETHERNET ||
			(activate && option == SET_DEFAULT_DEFAULTS);
	}
	if(page == PAGE_GAME) {
		return activate && option == SET_DEFAULTS;
	}
	return false;
}

/* Legacy entry points name a (page, option) setting: open the view that
 * shows it. A game's settings open on their own. */
int show_settings(int page, int option, ConfigEntry *config) {
	int view;
	int row;

	for(view = VIEW_QUICK; view <= VIEW_GAME; view++) {
		/* A game's rows live only in its own view. */
		if((view == VIEW_GAME) != (page == PAGE_GAME)) {
			continue;
		}
		for(row = 0; row < settingsViews[view].count; row++) {
			const settingsRowRef_t *ref = &settingsViews[view].rows[row];
			if(ref->page == page && ref->option == option) {
				return show_settings_view(view, row, config);
			}
		}
	}
	return show_settings_view(page == PAGE_GAME ? VIEW_GAME : VIEW_QUICK, 0, config);
}

int show_settings_view(int view, int option, ConfigEntry *config) {
	uiMenuInputState_t menuInput;
	u32 menuInputRetrace;
	bool digitalRepeat = false;

	if(view < 0 || view >= UI_SETLAYOUT_PAGE_COUNT) {
		view = VIEW_QUICK;
	}
	wait_network();
	// Copy current settings to a temp copy in case the user cancels out
	if(config != NULL) {
		memcpy(&tempConfig, config, sizeof(ConfigEntry));
	}
	memcpy(&tempSettings, &swissSettings, sizeof(SwissSettings));

	GXRModeObj *oldmode = getVideoMode();
	menuInputRetrace = VIDEO_GetRetraceCount();
	UIMenuInput_Init(&menuInput);
	/* The press that opened Settings (A on Home, X on Game Detail) must not
	 * also act here: a held X would put a game's first row back to its
	 * default. */
	if(padsButtonsHeld() & SETTINGS_DIGITAL_INPUT_MASK) {
		settingsInhibitThroughDigitalRelease(&menuInput,
			&menuInputRetrace);
	}
	while(1) {
		bool wasDigital;
		bool inputMayBlock;
		int inputView;
		int inputOption;
		int rows = settingsViews[view].count;
		const settingsRowRef_t *ref = settingsViewRow(view, option);
		bool onSetting = ref != NULL && ref->page != SETTINGS_ROW_LINK;
		uiDrawObj_t* settingsPage = settings_draw_page(view, option, config);
		u32 btns = settingsWaitForInput(&menuInput, &menuInputRetrace,
			&wasDigital);
		inputView = view;
		inputOption = option;
		inputMayBlock = onSetting &&
			settingsInputMayBlock(ref->page, ref->option, btns);
		if(btns & BUTTON_Y) {
			char *tooltip = onSetting ? get_tooltip(ref->page, ref->option) : NULL;
			if(tooltip) {
				uiDrawObj_t* tooltipBox = DrawPublish(DrawTooltip(tooltip));
				while(padsButtonsHeld() & BUTTON_Y) {
					(void)padsMenuInputPoll(&menuInput,
						settingsMenuInputElapsedMicroseconds(
							&menuInputRetrace),
						SETTINGS_MENU_INPUT_POLICY, true);
					VIDEO_WaitVSync();
				}
				while(!((padsButtonsHeld() & BUTTON_Y) ||
					(padsButtonsHeld() & BUTTON_B))) {
					(void)padsMenuInputPoll(&menuInput,
						settingsMenuInputElapsedMicroseconds(
							&menuInputRetrace),
						SETTINGS_MENU_INPUT_POLICY, true);
					VIDEO_WaitVSync();
				}
				(void)padsMenuInputPoll(&menuInput,
					settingsMenuInputElapsedMicroseconds(
						&menuInputRetrace),
					SETTINGS_MENU_INPUT_POLICY, true);
				DrawDispose(tooltipBox);
				UIMenuInput_Init(&menuInput);
			}
		}
		// Right/Left change a setting, or move between Save and Discard
		if(btns & BUTTON_RIGHT) {
			if(option >= rows) {
				if(option < rows + 1) option++;
			}
			else if(onSetting) {
				settingsChangeValue(ref->page, ref->option, 1, config);
			}
		}
		if(btns & BUTTON_LEFT) {
			if(option > rows) {
				option--;
			}
			else if(option < rows && onSetting) {
				settingsChangeValue(ref->page, ref->option, -1, config);
			}
		}
		// X in a game's settings puts an enabled row back to its Game
		// Defaults value
		if((btns & BUTTON_X) && view == VIEW_GAME && onSetting &&
			config != NULL) {
			settingRowView_t row;
			settingsDescribeRow(ref->page, ref->option, config, &row);
			if(row.enabled) {
				settingsUseDefault(config, ref->option);
			}
		}
		if((btns & BUTTON_DOWN) && option < rows + 1)
			option++;
		if((btns & BUTTON_UP) && option > 0)
			option--;
		// L and R move between the three tabs and wrap; a game's own
		// settings have none
		if(UISetLayout_PageDesc(view)->tab != UI_SETLAYOUT_NO_TAB) {
			int tab = UISetLayout_PageDesc(view)->tab;
			if(btns & BUTTON_R) {
				view = (tab + 1) % UI_SETLAYOUT_TAB_COUNT; option = 0;
			}
			if(btns & BUTTON_L) {
				view = (tab + UI_SETLAYOUT_TAB_COUNT - 1) % UI_SETLAYOUT_TAB_COUNT; option = 0;
			}
		}
		// B backs out of a Setup section. Anywhere else it leaves: Save &
		// Exit when something changed, otherwise just close. Discard & Exit
		// stays on the rail to undo.
		if(btns & BUTTON_B) {
			if(view > VIEW_SETUP && view < VIEW_GAME) {
				option = view - VIEW_DISPLAY;
				view = VIEW_SETUP;
				btns &= ~BUTTON_A;
			}
			else {
				option = settingsChanged(config) ?
					settingsViews[view].count : settingsViews[view].count + 1;
				btns |= BUTTON_A;
			}
		}
		// Handle all options/buttons here
		if((btns & BUTTON_A)) {
			if(option == settingsViews[view].count) {
				uiDrawObj_t *msgBox = DrawPublish(DrawProgressBar(true, 0, "Saving changes\205"));
				// Save settings to SRAM
				swissSettings.sram60Hz = getTVFormat() == VI_EURGB60;
				swissSettings.sramProgressive = getScanMode() == VI_PROGRESSIVE;
				if(in_range(swissSettings.aveCompat, GCDIGITAL_COMPAT, GCVIDEO_COMPAT)) {
					swissSettings.sramHOffset &= ~1;
				}
				VIDEO_SetAdjustingValues(swissSettings.sramHOffset, 0);
				// SRAM keeps naming the Configuration Device that Settings
				// opened with (tempSettings) until the settings are saved on
				// the one chosen here.
				u8 chosenConfigDevice = swissSettings.configDeviceId;
				swissSettings.configDeviceId = tempSettings.configDeviceId;
				updateSRAM(&swissSettings, true);
				swissSettings.configDeviceId = chosenConfigDevice;
				// Update environment
				config_update_environ();
				// Update our .ini (in memory)
				if(config != NULL) {
					config_defaults_from(&tempConfig, &tempSettings);
					config_update_game(config, &tempConfig, true);
				}
				// flush settings to .ini
				if(config_update_global(true)) {
					// Saved on the chosen device: SRAM may name it now.
					updateSRAM(&swissSettings, true);
					rt4k_init();
					msgBox = DrawRepublish(msgBox, DrawMessageBox(D_INFO, "Successfully saved configuration!"));
					sleep(1);
					DrawDispose(msgBox);
				}
				else {
					// Not saved on the chosen device: go back to the one
					// Settings opened with, which SRAM still names.
					swissSettings.configDeviceId = tempSettings.configDeviceId;
					msgBox = DrawRepublish(msgBox, DrawMessageBox(D_INFO, "Failed to save configuration!"));
					sleep(1);
					DrawDispose(msgBox);
				}
				DrawDispose(settingsPage);
				settingsInhibitThroughDigitalRelease(&menuInput,
					&menuInputRetrace);
				return 1;
			}
			if(option == settingsViews[view].count + 1) {
				// Exit without saving (revert)
				if(config != NULL) {
					memcpy(config, &tempConfig, sizeof(ConfigEntry));
				}
				memcpy(&swissSettings, &tempSettings, sizeof(SwissSettings));
				menuaudio_apply_settings();
				VIDEO_SetAdjustingValues(swissSettings.sramHOffset, 0);
				__SYS_SetTAUCalibration(swissSettings.sramTemperature);
				DrawDispose(settingsPage);
				DrawVideoMode(oldmode);
				settingsInhibitThroughDigitalRelease(&menuInput,
					&menuInputRetrace);
				return 0;
			}
			// A opens a Setup section, runs a text or reset row, and
			// changes a choice row the way Right does
			if(view == inputView && option == inputOption && ref != NULL) {
				if(ref->page == SETTINGS_ROW_LINK) {
					view = ref->option; option = 0;
				}
				else if(settingsRowIsAction(ref->page, ref->option)) {
					if(!settingsRowIsReset(ref->page, ref->option) ||
						settingsConfirmReset()) {
						settings_toggle(ref->page, ref->option, 0, config);
					}
				}
				else if(settingsPickerFor(ref->page, ref->option) != NULL) {
					settingsPick(settingsPickerFor(ref->page, ref->option), config,
						&menuInput, &menuInputRetrace);
				}
				else {
					settingsChangeValue(ref->page, ref->option, 1, config);
				}
			}
		}
		if(view != inputView || inputMayBlock) {
			UIMenuInput_Init(&menuInput);
		}
		/* A child prompt may return with its dismissing A press held even when
		 * analog opened it. Consume that release just like a digital action.
		 * A held D-pad direction repeats instead, on the stick's schedule. */
		if(inputMayBlock || !wasDigital) {
			digitalRepeat = false;
		}
		if(inputMayBlock || (wasDigital && !settingsHoldToRepeat(&menuInput,
			&menuInputRetrace, btns, &digitalRepeat))) {
			settingsInhibitThroughDigitalRelease(&menuInput,
				&menuInputRetrace);
		}
		DrawDispose(settingsPage);
	}
}
