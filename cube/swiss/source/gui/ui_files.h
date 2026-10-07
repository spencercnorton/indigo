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
	UI_FILES_INPUT_PAGE_UP,		/* C-stick up (L until storage menus) */
	UI_FILES_INPUT_PAGE_DOWN,	/* C-stick down (R) */
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
 * (Foo/default.dol or Foo/Foo.dol). Such an entry is a file whose parent
 * isn't the open folder; a flattened folder never has one. */
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
	UI_FILES_HINTS_MESSAGE
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
 * folder, with room. Unknown free space never greys anything. */
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
 * 6 px in from its own pane's right edge, so the other pane stays clear;
 * level with the row; kept between y 112 and 336. That holds
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

#endif
