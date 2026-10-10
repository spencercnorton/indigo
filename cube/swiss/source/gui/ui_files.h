#ifndef UI_FILES_H
#define UI_FILES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The File Browser: two panes side by side, as Memory Cards shows two
 * stacks. The left pane is the Source (the listing Swiss scans, the one that
 * starts games); the right pane is another folder, on the same storage or
 * another. This half is pure: where everything sits in both screen shapes,
 * which row each pane focuses, what a row says, which actions a file allows
 * and why not, and the words on the boxes. swiss.c fills it in and
 * FrameBufferMagic.c draws it. Stage px throughout. */

#define UI_FILES_PANES 2
#define UI_FILES_LEFT 0
#define UI_FILES_RIGHT 1

/* ------------------------------------------------------------------------
 * Layout. Every y is the same in both screen shapes; Menu Widescreen moves
 * the panes' outer edges out to the wider stage and keeps the inner ones,
 * so the extra width goes to file names.
 * --------------------------------------------------------------------- */
#define UI_FILES_ROWS 8			/* rows a pane shows */
#define UI_FILES_ROW_TOP 114		/* row 0's top */
#define UI_FILES_ROW_PITCH 28
#define UI_FILES_ROW_HEIGHT 26
#define UI_FILES_MARGIN 40		/* the panes' outer edges from the stage's */
#define UI_FILES_INNER_LEFT 312		/* the left pane's inner edge */
#define UI_FILES_INNER_RIGHT 328	/* the right pane's: a 16 px gutter */
#define UI_FILES_BUTTON_Y 28		/* the storage buttons */
#define UI_FILES_BUTTON_WIDTH 224
#define UI_FILES_BUTTON_HEIGHT 27
#define UI_FILES_DEVICE_Y 75		/* text y values are middles */
#define UI_FILES_FREE_TOP 63		/* the free-space box, right-aligned */
#define UI_FILES_FREE_BOTTOM 86
#define UI_FILES_FREE_MIN_WIDTH 64
#define UI_FILES_PATH_Y 98		/* the path line and the counter */
#define UI_FILES_PANE_TOP 108
#define UI_FILES_PANE_BOTTOM 342
#define UI_FILES_TRACK_TOP 116		/* the scroll track, past 8 entries */
#define UI_FILES_TRACK_BOTTOM 334
#define UI_FILES_MESSAGE_Y 226		/* "This folder is empty." */
#define UI_FILES_INFO_TOP 362
#define UI_FILES_INFO_BOTTOM 433
#define UI_FILES_LINE1_Y 403
#define UI_FILES_LINE2_Y 418
#define UI_FILES_HINT_Y 454
#define UI_FILES_BOX_TOP 112		/* boxes stay between these */
#define UI_FILES_BOX_BOTTOM 336

typedef struct {
	int x0, y0, x1, y1;
} uiFilesRect_t;

typedef struct {
	uiFilesRect_t pane[UI_FILES_PANES];	/* the pane boxes */
	int mid[UI_FILES_PANES];		/* each pane's middle */
	uiFilesRect_t button[UI_FILES_PANES];	/* "Choose storage" */
	uiFilesRect_t track[UI_FILES_PANES];	/* 2 px, 9 px in from x1 */
	uiFilesRect_t info;			/* the info bar */
	uiFilesRect_t picture;			/* its 96x32 banner or cube */
	uiFilesRect_t sizeBox;			/* its size box; x1 set by the text */
	int infoTextX;				/* the info bar's name */
	int hintLeft, hintRight;		/* the hint groups' ends */
} uiFilesLayout_t;

/* Every rect for a stage from stageLeft to stageRight (UIStage_Left and
 * UIStage_Right): 0 and 640 give the 4:3 table exactly. */
void UIFiles_Layout(float stageLeft, float stageRight, uiFilesLayout_t *out);

/* Row i of a pane (0 .. UI_FILES_ROWS - 1), inside the box's edge. */
uiFilesRect_t UIFiles_RowRect(const uiFilesLayout_t *layout, int pane, int row);

/* Where a row's cube is centred, its name starts and its size ends; the size
 * moves in clear of the scroll track when the pane shows one. */
int UIFiles_CubeX(const uiFilesLayout_t *layout, int pane);
int UIFiles_NameX(const uiFilesLayout_t *layout, int pane);
int UIFiles_MetaRight(const uiFilesLayout_t *layout, int pane, bool track);

/* The width a row's name may take beside a size metaWidth px wide (0: no
 * size). */
int UIFiles_NameWidth(const uiFilesLayout_t *layout, int pane, int metaWidth,
	bool track);

/* ------------------------------------------------------------------------
 * The two panes' focus. Up and Down move one row and wrap; a page moves
 * eight and stops at the ends; Left and Right cross between the panes, each
 * keeping its own row. Each pane shows a window of eight rows that keeps a
 * row of margin round the focus until the list ends.
 * --------------------------------------------------------------------- */
typedef enum {
	UI_FILES_INPUT_UP = 0,
	UI_FILES_INPUT_DOWN,
	UI_FILES_INPUT_LEFT,
	UI_FILES_INPUT_RIGHT,
	UI_FILES_INPUT_PAGE_UP,		/* C-stick up */
	UI_FILES_INPUT_PAGE_DOWN,	/* C-stick down */
	UI_FILES_INPUTS
} uiFilesInput_t;

typedef struct {
	int count;	/* entries listed, ".." included */
	int focus;	/* 0 .. count - 1, or 0 when empty */
	int first;	/* the window's first row */
} uiFilesPaneState_t;

typedef struct {
	uiFilesPaneState_t pane[UI_FILES_PANES];
	int active;	/* UI_FILES_LEFT or UI_FILES_RIGHT */
} uiFilesState_t;

void UIFiles_Init(uiFilesState_t *state);

/* A pane read again: its entries and the row found for the focused name.
 * The focus is clamped into the list and the window follows it. */
void UIFiles_SetPane(uiFilesState_t *state, int pane, int count, int focus);

/* One input to the active pane. True when anything moved; false is a bump
 * (Left in the left pane, a page at the end, an empty pane). */
bool UIFiles_Input(uiFilesState_t *state, uiFilesInput_t input);

/* The left pane's visible rows, start .. end - 1, whichever pane is active:
 * swiss.c's current_view_start and current_view_end, so the meta thread
 * never frees a visible left row's banner. */
void UIFiles_LeftView(const uiFilesState_t *state, int *start, int *end);

/* ------------------------------------------------------------------------
 * Rows: the kind of each entry, its name and its size.
 * --------------------------------------------------------------------- */
typedef enum {
	UI_FILES_KIND_PARENT = 0,	/* ".." */
	UI_FILES_KIND_FOLDER,
	UI_FILES_KIND_PROGRAM_FOLDER,	/* a folder Swiss starts by its DOL */
	UI_FILES_KIND_DISC,		/* .gcm .iso .tgc .fdi */
	UI_FILES_KIND_DISC_COMPRESSED,	/* .gcz .rvz: Swiss can't start them */
	UI_FILES_KIND_PROGRAM,		/* .dol .dol+cli .elf .bin */
	UI_FILES_KIND_FIRMWARE,		/* .fpkg .fzn */
	UI_FILES_KIND_MUSIC,		/* .mp3 */
	UI_FILES_KIND_PICTURE,
	UI_FILES_KIND_TEXT,
	UI_FILES_KIND_OTHER,
	UI_FILES_KINDS
} uiFilesKind_t;

/* fileType is main.h's: 1 a file, 2 a folder, 3 "..". */
#define UI_FILES_TYPE_FILE 1
#define UI_FILES_TYPE_DIR 2
#define UI_FILES_TYPE_PARENT 3

uiFilesKind_t UIFiles_Kind(const char *name, int fileType);

/* Kinds that A starts (a disc opens its details first). */
bool UIFiles_Loads(uiFilesKind_t kind);

/* Swiss's meta reading can turn a folder entry into the program inside it
 * (Foo/default.dol or Foo/Foo.dol): a file one folder below the open one,
 * ending .dol. A flattened folder never has one. */
bool UIFiles_IsProgramFolder(const char *entryName, int fileType,
	const char *curDirName, bool flattened);

/* The folder a program folder's entry stands for: the open folder and the
 * entry's first part below it. Its row shows this path's last part, and
 * focus comes back to it by this path. */
void UIFiles_ProgramFolderPath(char *out, size_t capacity, const char *entryName,
	const char *curDirName);

/* What a row says before fitting: a name with its extension; a program
 * folder's folder name; ".." as "Up to <parent>", "Up to <device>" one
 * below the root and "Other storage" at the root. */
void UIFiles_RowName(char *out, size_t capacity, const char *entryName,
	uiFilesKind_t kind, const char *curDirName, const char *deviceName);

/* Sizes as Swiss writes them: three significant figures, kB/MB/GB (metric)
 * or KiB/MiB/GiB. blockSize non-zero counts blocks instead: 8192 on a memory
 * card, 65536 on a Qoob. */
void UIFiles_SizeText(char *out, size_t capacity, uint64_t bytes,
	uint32_t blockSize, bool metric);

/* A WODE entry has no size, only where it is. */
void UIFiles_PartitionText(char *out, size_t capacity, int partition, int iso);

/* A row's size column: nothing for folders, "Can't start" before the size of
 * a compressed disc. sizeText is UIFiles_SizeText's. */
void UIFiles_RowMeta(char *out, size_t capacity, uiFilesKind_t kind,
	const char *sizeText);

/* ------------------------------------------------------------------------
 * Fitting text. measure is GetTextSizeInPixels: the width at scale 1.
 * --------------------------------------------------------------------- */
typedef int (*uiFilesMeasureFn)(const char *text);

#define UI_FILES_NAME_SCALE 0.56f
#define UI_FILES_NAME_MIN_SCALE 0.46f
#define UI_FILES_ELLIPSIS '\205'
#define UI_FILES_NAME_MIN_HEAD 6	/* characters kept before a cut's tail */

/* A row name into maxWidth: whole at the largest scale from 0.56 down to
 * 0.46 that fits, else cut in the middle at 0.46, keeping its extension and
 * a "(Disc N)" just before it and nothing else ("Signal Garden - Th\205(Disc
 * 1).iso"). A tail is kept only with UI_FILES_NAME_MIN_HEAD characters of
 * the start before it: else the extension alone, else only the start.
 * Never splits a UTF-8 sequence. Returns the scale to draw at. */
float UIFiles_FitName(char *out, size_t capacity, const char *name, int maxWidth,
	uiFilesMeasureFn measure);

#define UI_FILES_DEVICE_SCALE 0.92f
#define UI_FILES_DEVICE_MIN_SCALE 0.64f
#define UI_FILES_CHIP_W 54		/* SOURCE, 10 px after the device's name */

/* The device's name over pane, in what the SOURCE chip (when source) and the
 * free box leave: freeWidth wide, with freeWord px of "free" 8 px before it
 * (0 when read-only), 8 px clear of the name. At 0.92, smaller down to 0.64
 * to fit, else cut at its end at 0.64. Returns the scale to draw at. */
float UIFiles_FitDevice(char *out, size_t capacity, const char *name,
	const uiFilesLayout_t *layout, int pane, bool source, int freeWidth,
	int freeWord, uiFilesMeasureFn measure);

/* A folder's path below its device's root ("/games/GameCube"), cut from the
 * left at a folder when it is too wide ("\205/Collection/GameCube"). */
void UIFiles_FitPath(char *out, size_t capacity, const char *path, int maxWidth,
	float scale, uiFilesMeasureFn measure);

/* ------------------------------------------------------------------------
 * Hints, at most five round buttons on the line.
 * --------------------------------------------------------------------- */
typedef enum {
	UI_FILES_HINTS_LIST = 0,
	UI_FILES_HINTS_BOX,		/* Actions or storage */
	UI_FILES_HINTS_QUESTION,
	UI_FILES_HINTS_DELETE,
	UI_FILES_HINTS_MESSAGE,
	UI_FILES_HINTS_STORAGE		/* a pane with nothing to act on: A chooses its storage */
} uiFilesHintMode_t;

#define UI_FILES_HINT_CAPACITY 96

/* The left and right hint groups for the focused entry. loads: A starts it
 * here (UIFiles_Loads, less a FlippyDrive update on FlippyDrive).
 * fileManagement: Z works. autoloadOn: ".." is the Autoload folder. */
void UIFiles_Hints(uiFilesHintMode_t mode, int pane, uiFilesKind_t kind,
	bool loads, bool fileManagement, bool autoloadOn,
	char left[UI_FILES_HINT_CAPACITY], char right[UI_FILES_HINT_CAPACITY]);

/* ------------------------------------------------------------------------
 * Storage on each side.
 * --------------------------------------------------------------------- */
typedef enum {
	UI_FILES_UNMOUNTED = 0,
	UI_FILES_SHARED,	/* the Source's own mount */
	UI_FILES_OWN,		/* mounted for the pane */
	UI_FILES_FAILED		/* init or a read failed */
} uiFilesMount_t;

typedef struct {
	const void *handler;	/* DEVICEHANDLER_INTERFACE */
	const char *name;	/* DeviceDisplayName */
	uint32_t location;	/* LOC_* bits */
	bool network;		/* SMB, FTP, FSP: one network adapter */
	bool metric;		/* sizes in kB rather than KiB */
	bool canWrite;		/* FEAT_WRITE */
	bool canRename, canHide, canDelete;	/* the handler has them */
	bool card;		/* a memory card slot: Memory Cards copies saves */
} uiFilesDevice_t;

/* Two different handlers on the same connector can't be open together; the
 * same handler on both sides can (each side its own folder). reason, when
 * not NULL, gets why: "<other> is open on the other side." or "The network
 * adapter is in use on the other side." */
bool UIFiles_StorageClash(const uiFilesDevice_t *choice,
	const uiFilesDevice_t *other, char *reason, size_t capacity);

/* Free space is only worth showing when info() gave it, gave a total, and
 * isn't a network share's (filled once at connect, 0 when unknown). */
bool UIFiles_FreeKnown(bool haveInfo, uint64_t totalSpace, bool network);

/* Y swaps the sides only onto a pane that is ready: the Source's own mount,
 * or its own with its last read good. */
bool UIFiles_CanSwap(uiFilesMount_t mount, bool readOk);

/* Where the right pane opens the first time: the Configuration Device when
 * it is set, detected and not the Source, else the Source itself. Without
 * File Management nothing can be done with a second device, so it browses
 * the Source. */
bool UIFiles_RightOnConfig(bool fileManagement, bool haveConfig,
	bool configDetected, bool configIsSource);

/* The right pane when its storage can't be used: what failed, the device's
 * own words for it in brackets (status), or, when a read failed after it
 * mounted, which folder (folder: its name, NULL for init), and how to
 * choose another. */
#define UI_FILES_MESSAGE_LINES 3
#define UI_FILES_MESSAGE_TEXT 64
void UIFiles_NotReady(char out[UI_FILES_MESSAGE_LINES][UI_FILES_MESSAGE_TEXT],
	const char *name, const char *status, const char *folder);

/* ------------------------------------------------------------------------
 * Actions (Z). Greyed items stay listed, with why.
 * --------------------------------------------------------------------- */
typedef enum {
	UI_FILES_ACTION_COPY = 0,	/* X */
	UI_FILES_ACTION_MOVE,		/* Y */
	UI_FILES_ACTION_RENAME,		/* R */
	UI_FILES_ACTION_HIDE,		/* L: Hide or Unhide */
	UI_FILES_ACTION_DELETE,		/* Z */
	UI_FILES_ACTIONS
} uiFilesAction_t;

typedef struct {
	uiFilesDevice_t device;
	const char *folder;	/* the open folder's path */
	uint8_t mount;		/* uiFilesMount_t: the Source is SHARED */
	bool readOk;		/* its last read worked */
	bool freeKnown;		/* UIFiles_FreeKnown */
	uint64_t freeBytes;
} uiFilesSide_t;

typedef struct {
	int pane;		/* the side it is on */
	bool isFile;
	bool programFolder;	/* acts as its folder */
	bool hidden;
	uint64_t needed;	/* its size, plus a GCI header off a memory card */
	bool exists;		/* a same-named entry in the other folder */
	bool existsFolder;	/* and that entry is a folder */
	uint64_t existingSize;	/* that entry's size */
} uiFilesEntry_t;

#define UI_FILES_TEXT_CAPACITY 128

typedef struct {
	bool enabled[UI_FILES_ACTIONS];
	/* Line 2 of the info bar for each item: why it is greyed, or Copy's and
	 * Move's fit; amber when warn. */
	char line[UI_FILES_ACTIONS][UI_FILES_TEXT_CAPACITY];
	bool warn[UI_FILES_ACTIONS];
	/* Copy's and Move's: it fits only by replacing the file there. Never
	 * a same-device Move, which renames and writes nothing. */
	bool replaceOnly[UI_FILES_ACTIONS];
	char label[UI_FILES_ACTIONS][12];	/* "Copy" .. "Delete" */
	char letter[UI_FILES_ACTIONS];		/* X Y R L Z */
} uiFilesAvailability_t;

/* What manage_file allows on this entry (its canCopy .. canDelete), and
 * where Copy and Move can go: the other side ready, writable, another
 * folder, no folder of the same name, not a memory card, with room.
 * Unknown free space never greys anything. */
void UIFiles_Availability(const uiFilesSide_t *source, const uiFilesSide_t *other,
	const uiFilesEntry_t *entry, uiFilesAvailability_t *out);

/* The first item focus starts on: the first enabled one, else 0. */
int UIFiles_FirstEnabled(const bool *enabled, int count);

/* The file-exists box: Keep both, Replace it, Cancel. Keep both is greyed
 * when only replacing makes room. */
typedef struct {
	char item[3][16];
	uint32_t dim;		/* bit per greyed item */
	int focus;
	const char *reason;	/* why Keep both is greyed, or "" */
} uiFilesChoices_t;

void UIFiles_ExistsChoices(bool fitsBoth, uiFilesChoices_t *out);

/* A box beside a row (Actions, a question, a storage menu): right-aligned
 * 6 px in from its own pane's right edge, so the other pane stays clear,
 * and never wider than the pane less 6 px each side; level with the row; kept between y 112 and 336. That holds
 * UI_FILES_MENU_ITEMS(titled) items; callers list no more, so a storage
 * menu shows UI_FILES_STORAGE_DEVICES devices and "Other devices..." reaches
 * the rest. */
#define UI_FILES_MENU_WIDTH 124
#define UI_FILES_MENU_PITCH 24
#define UI_FILES_MENU_TITLE 30
#define UI_FILES_MENU_ITEMS(titled) ((titled) ? 7 : 8)
#define UI_FILES_STORAGE_DEVICES 6
typedef struct {
	int x, y, width, height;	/* the items' box */
	int titleY;			/* the title box's top, or y untitled */
} uiFilesBox_t;

void UIFiles_MenuBox(const uiFilesLayout_t *layout, const uiFilesRect_t *row,
	int pane, int items, bool titled, int width, uiFilesBox_t *out);

/* A box as the frame draws it: the items, which are greyed and which has
 * the focus. serial changes for each box opened, so it opens again. */
#define UI_FILES_MENU_MAX 9
typedef struct {
	char title[UI_FILES_TEXT_CAPACITY];
	char item[UI_FILES_MENU_MAX][40];
	uint16_t dim;		/* a bit per greyed item */
	uint16_t serial;
	int16_t width;		/* measured on the menu thread */
	uint8_t count, focus;
	uint8_t pane;		/* the pane it sits in */
	uint8_t open;		/* 0: closing, its items kept to fade */
	uint8_t row;		/* the row it sits beside: a storage menu's is 0 */
	uint8_t rose;		/* Delete's: a rose edge */
	char letter[UI_FILES_MENU_MAX];	/* each item's letter chip, or 0 */
} uiFilesMenu_t;

/* A box opening (open) or closing, since seconds ago: from 0.92x (0.97x on
 * Reduced) over 0.08 s, out over 0.10 s, at once with UI Motion Off. mode
 * is ui_motion.h's uiMotionMode_t. */
void UIFiles_MenuMotion(float since, bool open, int mode, float *alpha, float *scale);

/* L and R: the storage menu for pane, as Memory Cards' chooseStorage. Every
 * detected device given (allDevices order) up to UI_FILES_STORAGE_DEVICES,
 * then "Other devices...". A device that can't be open beside the other
 * side's is greyed with why; the other side's own device isn't. Focus starts
 * on current, else the first one not greyed. For each item, the info bar's
 * two lines: what choosing it does, and (amber) why it is greyed. */
typedef struct {
	uiFilesMenu_t box;
	int devices;		/* items before "Other devices...", or -1: not storage */
	char line[UI_FILES_MENU_MAX][UI_FILES_TEXT_CAPACITY];
	char reason[UI_FILES_MENU_MAX][UI_FILES_TEXT_CAPACITY];
	uint16_t warn;		/* a bit per item whose reason is amber */
} uiFilesStorageMenu_t;

void UIFiles_StorageMenu(int pane, const uiFilesDevice_t *devices, int count,
	const uiFilesDevice_t *current, const uiFilesDevice_t *other,
	const char *otherPath, uiFilesStorageMenu_t *out);

/* Where a new entry would sort in a listing sorted as Swiss's
 * fileComparator sorts: higher fileType first, then the name, ignoring
 * case. at gives entry i's name and fileType. */
typedef void (*uiFilesEntryAtFn)(const void *context, int index,
	const char **name, int *fileType);
int UIFiles_LandingIndex(const void *context, int count, uiFilesEntryAtFn at,
	const char *name, int fileType);

/* Which entry each pane focuses after an action, picked before the panes
 * are read again. sourceIndex is a row of the old listing whose name to
 * find again, or UI_FILES_FOCUS_NEW_NAME (Rename: the new name). */
#define UI_FILES_FOCUS_NEW_NAME (-2)
typedef struct {
	int sourceIndex;
	bool otherToNew;	/* the other pane focuses the new file */
	bool flash;		/* and flashes it */
} uiFilesFocusAfter_t;

void UIFiles_FocusAfter(uiFilesAction_t action, bool done, int focus, int count,
	bool showHidden, bool wasHidden, uiFilesFocusAfter_t *out);

/* Line 1 of the info bar for an action in the Actions box: what it does.
 * here: the entry's storage; there and folder: the other side's storage
 * and folder (as the path line shows it). */
void UIFiles_ActionLine(uiFilesAction_t action, bool hidden, const char *here,
	const char *there, const char *folder, char *out, size_t capacity);

/* How a copy or a move ended, in the maroon message's two lines. */
typedef enum {
	UI_FILES_RESULT_DONE = 0,
	UI_FILES_RESULT_STOPPED,	/* B */
	UI_FILES_RESULT_WRITE_FAILED,
	UI_FILES_RESULT_READ_FAILED,
	UI_FILES_RESULT_KEPT		/* moved, but the original couldn't be deleted */
} uiFilesResult_t;

/* name: the file's; here: its storage; there and folder: where it went;
 * code: the device's error; removed: a stopped or failed copy's unfinished
 * file was deleted; replaced: Replace it took the file there off first. */
void UIFiles_Result(uiFilesResult_t result, bool move, const char *name, const char *here,
	const char *there, const char *folder, int code, bool removed, bool replaced,
	char out[2][UI_FILES_TEXT_CAPACITY]);

/* ------------------------------------------------------------------------
 * Swiss's confirmAction texts ("Move this file?\nIt is removed from here
 * once copied.\nA  MOVE    B  CANCEL"), read for an IPL box: line 1 the
 * title, line 2 the info bar's, the hint the two items.
 * --------------------------------------------------------------------- */
typedef struct {
	char title[UI_FILES_TEXT_CAPACITY];
	char detail[UI_FILES_TEXT_CAPACITY];
	char verb[24];		/* "Move" */
	char cancel[24];	/* "Cancel" */
} uiFilesQuestion_t;

/* False, leaving out empty, when text doesn't end in "A  VERB    B  NAME". */
bool UIFiles_ParseQuestion(const char *text, uiFilesQuestion_t *out);

/* ------------------------------------------------------------------------
 * A frame of the screen, as swiss.c writes it on the menu thread with every
 * string already fitted; FrameBufferMagic.c only draws it.
 * --------------------------------------------------------------------- */
#define UI_FILES_ROW_TEXT 128
#define UI_FILES_BANNER_BYTES 6144	/* a 96x32 RGB5A3 banner */

#define UI_FILES_ROW_FOCUS 1u
#define UI_FILES_ROW_HIDDEN 2u
#define UI_FILES_ROW_GHOST 4u	/* where a copy will land, before it does */
#define UI_FILES_ROW_FLASH 8u	/* a copy that just landed, under the message */

typedef struct {
	char name[UI_FILES_ROW_TEXT];
	char meta[24];
	float scale;		/* the name's */
	uint8_t kind;		/* uiFilesKind_t */
	uint8_t flags;
	uint8_t reserved[2];
} uiFilesRowSnapshot_t;

typedef struct {
	uiFilesRowSnapshot_t row[UI_FILES_ROWS];
	int16_t rows;		/* rows listed, 0 .. UI_FILES_ROWS */
	int16_t focusRow;	/* the focused one among them, or -1 */
	int16_t count, first;	/* entries, and the window's first: the track */
	char button[32];	/* the storage button's words */
	char device[48];
	char free[24];		/* "8.57 GB", "read-only", or "" */
	char path[UI_FILES_ROW_TEXT];
	char counter[16];	/* "3 / 10" */
	/* across the pane: "This folder is empty.", or why it can't be read */
	char message[UI_FILES_MESSAGE_LINES][UI_FILES_MESSAGE_TEXT];
	/* Widths at their scales, measured on the menu thread, so the chips and
	 * boxes after them sit without the draw measuring anything. */
	int16_t deviceWidth, pathWidth, freeWidth;
	float deviceScale;	/* the device's name: 0.92, less when long */
	uint16_t listing;	/* another listing: the focus bar snaps */
	uint8_t source;		/* the SOURCE chip after the device */
	uint8_t autoload;	/* the AUTOLOAD chip after the path */
	uint8_t reading;	/* "Reading..." over dimmed rows */
	uint8_t readOnly;	/* the free box says so, without "free" */
} uiFilesPaneSnapshot_t;

typedef struct {
	uint8_t banner[UI_FILES_BANNER_BYTES];	/* first, so 32-byte aligned */
	uiFilesPaneSnapshot_t pane[UI_FILES_PANES];
	char title[UI_FILES_ROW_TEXT];		/* the info bar's name */
	float titleScale;
	char chip[12];				/* HIDDEN or AUTOLOAD after it */
	char size[24];				/* in its box; none when empty */
	int16_t titleWidth, sizeWidth;		/* as the panes' widths */
	char line[2][UI_FILES_TEXT_CAPACITY];
	char hint[2][UI_FILES_HINT_CAPACITY];
	uint8_t active;		/* the focused pane */
	uint8_t hasBanner;
	uint8_t infoKind;	/* uiFilesKind_t: the info bar's cube */
	uint8_t warn;		/* line 2 in amber */
	uint8_t leaving;	/* B: the page goes, the Home cube comes back */
	uint8_t messageLeaving;	/* the message fades out */
	uint8_t swapping;	/* Y: the panes' contents fade out until the next */
	uint8_t reserved;
	uiFilesMenu_t menu;	/* a box: a storage menu, Actions, a question */
	/* The maroon message over the page, and its second line; serial
	 * changes for each one said, so it fades in again. */
	char message[2][UI_FILES_TEXT_CAPACITY];
	uint16_t messageSerial;
	int16_t messageWidth;	/* its box's, measured on the menu thread */
} uiFilesSnapshot_t;

/* The Copy question's ghost row: ghost, where a copy will land (landing,
 * in the listing's order), shown in pane's window as a row of its own; the
 * rows after it move down one, the last falling out of view. */
void UIFiles_InsertGhost(uiFilesPaneSnapshot_t *pane, int landing,
	const uiFilesRowSnapshot_t *ghost);

/* A copy's new row flashing under the message, since seconds after it
 * came: two pulses over 0.6 s, one over 0.3 s on Reduced, none with UI
 * Motion Off. 0 .. 1. mode is ui_motion.h's uiMotionMode_t. */
float UIFiles_Flash(float since, int mode);

/* The page coming and going, as Memory Cards' does: the Home cube going back
 * into the distance (handover 1 where it stood, 0 gone), the graph paper,
 * then the words (chrome). seconds since it opened; leave seconds since B,
 * or below 0. mode is ui_motion.h's uiMotionMode_t. */
typedef struct {
	float paper, chrome, handover;
} uiFilesStage_t;

void UIFiles_Stage(float seconds, float leave, int mode, uiFilesStage_t *out);

/* How long leaving takes, so B can wait for it before Home shows. */
float UIFiles_LeaveSeconds(int mode);

/* Y swapping the sides: what the panes and the info bar hold (their words,
 * rows and cubes) after delta seconds more, from content (0 .. 1): going
 * while swapping, back once the new sides are shown, each way in half of
 * a 0.15 s cross-fade (0.10 s on Reduced); always 1 with UI Motion Off.
 * mode is ui_motion.h's uiMotionMode_t. */
float UIFiles_SwapStep(float content, bool swapping, float delta, int mode);

/* Half of that cross-fade: how long Y lets the old sides go before it
 * changes them, so they do go however quickly the new ones are read. */
float UIFiles_SwapHalfSeconds(int mode);

#endif
