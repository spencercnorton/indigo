/*
 * Host test command (run from the repository root):
 * cc -std=c11 -Wall -Wextra -Werror -Wconversion -Wsign-conversion \
 *   -pedantic -Icube/swiss/source/gui \
 *   buildtools/ui/tests/test_ui_files.c cube/swiss/source/gui/ui_files.c \
 *   cube/swiss/source/gui/ui_hint.c -o /tmp/test_ui_files && /tmp/test_ui_files
 *
 * The File Browser's pure half: layout in both screen shapes, the two panes'
 * focus (checked against an independent model over every short input
 * sequence), rows, fitting real names, hints, storage, actions and their
 * reasons, boxes, landing, focus after an action and Swiss's questions.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_files.h"
#include "ui_hint.h"
#include "ui_motion.h"

static unsigned int checks;

#define CHECK(condition) do { \
	++checks; \
	if(!(condition)) { \
		fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, \
			#condition); \
		exit(EXIT_FAILURE); \
	} \
} while(0)

#define CHECK_TEXT(actual, expected) do { \
	const char *const checkedActual = (actual); \
	const char *const checkedExpected = (expected); \
	if(strcmp(checkedActual, checkedExpected) != 0) { \
		fprintf(stderr, "at %s:%d: got \"%s\", want \"%s\"\n", __FILE__, \
			__LINE__, checkedActual, checkedExpected); \
	} \
	CHECK(strcmp(checkedActual, checkedExpected) == 0); \
} while(0)

/* About the IPL font: 14 px a character at scale 1, so 0.46 holds about
 * 25 characters in a 4:3 name column beside a size. */
static int measure(const char *text)
{
	return 14 * (int)strlen(text);
}

static bool rectIs(uiFilesRect_t r, int x0, int y0, int x1, int y1)
{
	return r.x0 == x0 && r.y0 == y0 && r.x1 == x1 && r.y1 == y1;
}

/* ------------------------------------------------------------------------
 * Layout.
 * --------------------------------------------------------------------- */
static void testLayout(void)
{
	uiFilesLayout_t l, w;
	int p, row;

	/* 4:3: the layout table exactly. */
	UIFiles_Layout(0.0f, 640.0f, &l);
	CHECK(rectIs(l.pane[0], 40, 108, 312, 342));
	CHECK(rectIs(l.pane[1], 328, 108, 600, 342));
	CHECK(l.mid[0] == 176 && l.mid[1] == 464);
	CHECK(rectIs(l.button[0], 64, 28, 288, 55));
	CHECK(rectIs(l.button[1], 352, 28, 576, 55));
	CHECK(rectIs(l.track[0], 303, 116, 305, 334));
	CHECK(rectIs(l.track[1], 591, 116, 593, 334));
	CHECK(rectIs(l.info, 40, 362, 600, 433));
	CHECK(rectIs(l.picture, 56, 381, 152, 413));
	CHECK(l.infoTextX == 168);
	CHECK(rectIs(l.sizeBox, 168, 397, 168, 421));
	CHECK(l.hintLeft == 40 && l.hintRight == 600);
	for(row = 0; row < UI_FILES_ROWS; ++row) {
		uiFilesRect_t r = UIFiles_RowRect(&l, UI_FILES_LEFT, row);

		CHECK(r.y0 == 114 + 28 * row && r.y1 - r.y0 == 26);
		CHECK(r.x0 > l.pane[0].x0 && r.x1 < l.pane[0].x1);
	}
	CHECK(UIFiles_RowRect(&l, UI_FILES_LEFT, 7).y1 <= 342);
	CHECK(UIFiles_CubeX(&l, 0) == 61 && UIFiles_CubeX(&l, 1) == 349);
	CHECK(UIFiles_NameX(&l, 0) == 80 && UIFiles_NameX(&l, 1) == 368);
	CHECK(UIFiles_MetaRight(&l, 0, false) == 306 && UIFiles_MetaRight(&l, 1, false) == 594);
	/* Clear of the track. */
	CHECK(UIFiles_MetaRight(&l, 0, true) < l.track[0].x0);
	/* "1.35 GB" at 0.44 is about 44 px: about 174 px of name. */
	CHECK(UIFiles_NameWidth(&l, 0, 44, false) == 174);
	CHECK(UIFiles_NameWidth(&l, 0, 0, false) == 226);
	CHECK(UIFiles_NameWidth(&l, 0, 44, true) == 166);

	/* Menu Widescreen: an 853 px stage. Outer edges move, inner stay. */
	UIFiles_Layout(-106.5f, 746.5f, &w);
	CHECK(w.pane[0].x0 == -67 && w.pane[0].x1 == 312);
	CHECK(w.pane[1].x0 == 328 && w.pane[1].x1 == 707);
	CHECK(w.pane[0].x1 - w.pane[0].x0 == 379 && w.pane[1].x1 - w.pane[1].x0 == 379);
	CHECK(w.info.x0 == -67 && w.info.x1 == 707);
	CHECK(w.hintLeft == -67 && w.hintRight == 707);
	CHECK(w.picture.x0 == w.info.x0 + 16 && w.infoTextX == w.info.x0 + 128);
	for(p = 0; p < UI_FILES_PANES; ++p) {
		/* Every y and the rows are the same; mids and buttons follow. */
		CHECK(w.pane[p].y0 == l.pane[p].y0 && w.pane[p].y1 == l.pane[p].y1);
		CHECK(w.mid[p] == (w.pane[p].x0 + w.pane[p].x1) / 2);
		CHECK(w.button[p].x1 - w.button[p].x0 == 224);
		CHECK(w.button[p].x0 + 112 == w.mid[p]);
		CHECK(w.track[p].x0 == w.pane[p].x1 - 9);
		CHECK(UIFiles_RowRect(&w, p, 3).y0 == UIFiles_RowRect(&l, p, 3).y0);
		/* The extra width goes to names. */
		CHECK(UIFiles_NameWidth(&w, p, 44, false) ==
			UIFiles_NameWidth(&l, p, 44, false) + 107);
	}
}

/* ------------------------------------------------------------------------
 * The two panes, against an independent model.
 * --------------------------------------------------------------------- */
typedef struct {
	int count[2], focus[2], active;
} model_t;

static void modelInput(model_t *m, uiFilesInput_t input, bool *moved)
{
	int p = m->active, n = m->count[p], before = m->focus[p];

	*moved = false;
	if(input == UI_FILES_INPUT_LEFT || input == UI_FILES_INPUT_RIGHT) {
		int to = input == UI_FILES_INPUT_RIGHT;

		*moved = m->active != to;
		m->active = to;
		return;
	}
	if(n == 0) return;
	if(input == UI_FILES_INPUT_UP) m->focus[p] = before == 0 ? n - 1 : before - 1;
	if(input == UI_FILES_INPUT_DOWN) m->focus[p] = before == n - 1 ? 0 : before + 1;
	if(input == UI_FILES_INPUT_PAGE_UP) m->focus[p] = before - 8 < 0 ? 0 : before - 8;
	if(input == UI_FILES_INPUT_PAGE_DOWN) m->focus[p] = before + 8 > n - 1 ? n - 1 : before + 8;
	*moved = m->focus[p] != before;
}

/* The window's rules: inside the list, the focus inside it with a row of
 * margin unless the list ends there, and it moved only if it had to. */
static void checkWindow(const uiFilesPaneState_t *pane, int previousFirst)
{
	int last = pane->count > 8 ? pane->count - 8 : 0;
	bool marginOk;

	CHECK(pane->first >= 0 && pane->first <= last);
	if(pane->count == 0) {
		CHECK(pane->focus == 0 && pane->first == 0);
		return;
	}
	CHECK(pane->focus >= 0 && pane->focus < pane->count);
	CHECK(pane->focus >= pane->first && pane->focus < pane->first + 8);
	CHECK(pane->focus >= pane->first + 1 || pane->first == 0);
	CHECK(pane->focus <= pane->first + 6 || pane->first == last);
	marginOk = previousFirst >= 0 && previousFirst <= last &&
		(pane->focus >= previousFirst + 1 || previousFirst == 0) &&
		(pane->focus <= previousFirst + 6 || previousFirst == last) &&
		pane->focus >= previousFirst && pane->focus < previousFirst + 8;
	if(marginOk) CHECK(pane->first == previousFirst);
}

static void testReducerAuthored(void)
{
	uiFilesState_t s;
	int start, end;

	UIFiles_Init(&s);
	CHECK(s.active == UI_FILES_LEFT);
	UIFiles_SetPane(&s, UI_FILES_LEFT, 20, 0);
	UIFiles_SetPane(&s, UI_FILES_RIGHT, 5, 3);
	/* Up from the top wraps to the end; the window shows the last eight. */
	CHECK(UIFiles_Input(&s, UI_FILES_INPUT_UP));
	CHECK(s.pane[0].focus == 19 && s.pane[0].first == 12);
	UIFiles_LeftView(&s, &start, &end);
	CHECK(start == 12 && end == 20);
	/* Down from the end wraps back. */
	CHECK(UIFiles_Input(&s, UI_FILES_INPUT_DOWN));
	CHECK(s.pane[0].focus == 0 && s.pane[0].first == 0);
	/* Paging stops at the ends. */
	CHECK(!UIFiles_Input(&s, UI_FILES_INPUT_PAGE_UP));
	CHECK(UIFiles_Input(&s, UI_FILES_INPUT_PAGE_DOWN) && s.pane[0].focus == 8);
	CHECK(UIFiles_Input(&s, UI_FILES_INPUT_PAGE_DOWN) && s.pane[0].focus == 16);
	CHECK(UIFiles_Input(&s, UI_FILES_INPUT_PAGE_DOWN) && s.pane[0].focus == 19);
	CHECK(!UIFiles_Input(&s, UI_FILES_INPUT_PAGE_DOWN));
	/* Crossing keeps each pane's row; Left in the left pane bumps. */
	CHECK(!UIFiles_Input(&s, UI_FILES_INPUT_LEFT));
	CHECK(UIFiles_Input(&s, UI_FILES_INPUT_RIGHT) && s.active == UI_FILES_RIGHT);
	CHECK(s.pane[1].focus == 3 && s.pane[0].focus == 19);
	CHECK(!UIFiles_Input(&s, UI_FILES_INPUT_RIGHT));
	/* The right pane never moves the left window. */
	CHECK(UIFiles_Input(&s, UI_FILES_INPUT_DOWN) && s.pane[1].focus == 4);
	UIFiles_LeftView(&s, &start, &end);
	CHECK(start == 12 && end == 20);
	CHECK(UIFiles_Input(&s, UI_FILES_INPUT_LEFT) && s.active == UI_FILES_LEFT);
	CHECK(s.pane[0].focus == 19);
	/* Re-read: the focus is clamped, the window follows. */
	UIFiles_SetPane(&s, UI_FILES_LEFT, 10, 19);
	CHECK(s.pane[0].focus == 9 && s.pane[0].first == 2);
	/* A Delete or Move shrank it: the focus one past the end comes back. */
	UIFiles_SetPane(&s, UI_FILES_LEFT, 5, 5);
	CHECK(s.pane[0].focus == 4 && s.pane[0].count == 5);
	UIFiles_SetPane(&s, UI_FILES_LEFT, 0, 5);
	CHECK(s.pane[0].focus == 0 && s.pane[0].first == 0 && s.pane[0].count == 0);
	CHECK(!UIFiles_Input(&s, UI_FILES_INPUT_DOWN));
	UIFiles_LeftView(&s, &start, &end);
	CHECK(start == 0 && end == 0);
	/* A single entry can't move. */
	UIFiles_SetPane(&s, UI_FILES_LEFT, 1, 0);
	CHECK(!UIFiles_Input(&s, UI_FILES_INPUT_UP) && !UIFiles_Input(&s, UI_FILES_INPUT_DOWN));
	/* A one-row margin while scrolling down a long list. */
	UIFiles_SetPane(&s, UI_FILES_LEFT, 30, 0);
	{
		int i;

		for(i = 0; i < 6; ++i) CHECK(UIFiles_Input(&s, UI_FILES_INPUT_DOWN));
		CHECK(s.pane[0].focus == 6 && s.pane[0].first == 0);
		CHECK(UIFiles_Input(&s, UI_FILES_INPUT_DOWN));
		CHECK(s.pane[0].focus == 7 && s.pane[0].first == 1);
	}
}

static void testReducerExhaustive(void)
{
	static const int counts[] = {0, 1, 2, 3, 7, 8, 9, 10, 16, 17, 25};
	const int countCount = (int)(sizeof(counts) / sizeof(counts[0]));
	const int length = 5;
	int total = 1, i, l, r, start0;

	for(i = 0; i < length; ++i) total *= UI_FILES_INPUTS;
	for(l = 0; l < countCount; ++l) {
		for(r = 0; r < countCount; r += 4) {
			for(start0 = 0; start0 < 2; ++start0) {
				int sequence;

				for(sequence = 0; sequence < total; ++sequence) {
					uiFilesState_t s;
					model_t m;
					int code = sequence, step;

					UIFiles_Init(&s);
					m.count[0] = counts[l];
					m.count[1] = counts[r];
					m.focus[0] = counts[l] == 0 ? 0 : (start0 * 7) % counts[l];
					m.focus[1] = counts[r] == 0 ? 0 : counts[r] - 1;
					m.active = 0;
					UIFiles_SetPane(&s, 0, m.count[0], m.focus[0]);
					/* One past the end, as after a Delete: clamped to the last. */
					UIFiles_SetPane(&s, 1, m.count[1], m.count[1]);
					checkWindow(&s.pane[0], s.pane[0].first);
					checkWindow(&s.pane[1], s.pane[1].first);
					for(step = 0; step < length; ++step) {
						uiFilesInput_t input = (uiFilesInput_t)(code % UI_FILES_INPUTS);
						int firstBefore[2] = {s.pane[0].first, s.pane[1].first};
						int viewStart, viewEnd, startBefore, endBefore;
						bool moved, modelMoved;

						code /= UI_FILES_INPUTS;
						UIFiles_LeftView(&s, &startBefore, &endBefore);
						moved = UIFiles_Input(&s, input);
						modelInput(&m, input, &modelMoved);
						CHECK(moved == modelMoved);
						CHECK(s.active == m.active);
						CHECK(s.pane[0].focus == m.focus[0] && s.pane[1].focus == m.focus[1]);
						checkWindow(&s.pane[0], firstBefore[0]);
						checkWindow(&s.pane[1], firstBefore[1]);
						/* current_view_* is the left window, whatever is active. */
						UIFiles_LeftView(&s, &viewStart, &viewEnd);
						CHECK(viewStart == s.pane[0].first);
						CHECK(viewEnd == (s.pane[0].first + 8 < m.count[0] ?
							s.pane[0].first + 8 : m.count[0]));
						if(m.active == 1 && input != UI_FILES_INPUT_RIGHT) {
							CHECK(viewStart == startBefore && viewEnd == endBefore);
						}
					}
				}
			}
		}
	}
}

/* ------------------------------------------------------------------------
 * Rows.
 * --------------------------------------------------------------------- */
static void testKinds(void)
{
	/* util.c's knownExtensions, .fdi, and what Hide unknown file types
	 * hides. */
	CHECK(UIFiles_Kind("sd:/a.bin", 1) == UI_FILES_KIND_PROGRAM);
	CHECK(UIFiles_Kind("sd:/a.dol", 1) == UI_FILES_KIND_PROGRAM);
	CHECK(UIFiles_Kind("sd:/a.dol+cli", 1) == UI_FILES_KIND_PROGRAM);
	CHECK(UIFiles_Kind("sd:/a.elf", 1) == UI_FILES_KIND_PROGRAM);
	CHECK(UIFiles_Kind("sd:/a.fpkg", 1) == UI_FILES_KIND_FIRMWARE);
	CHECK(UIFiles_Kind("sd:/a.fzn", 1) == UI_FILES_KIND_FIRMWARE);
	CHECK(UIFiles_Kind("sd:/a.gcm", 1) == UI_FILES_KIND_DISC);
	CHECK(UIFiles_Kind("sd:/a.gcz", 1) == UI_FILES_KIND_DISC_COMPRESSED);
	CHECK(UIFiles_Kind("sd:/a.iso", 1) == UI_FILES_KIND_DISC);
	CHECK(UIFiles_Kind("sd:/a.mp3", 1) == UI_FILES_KIND_MUSIC);
	CHECK(UIFiles_Kind("sd:/a.rvz", 1) == UI_FILES_KIND_DISC_COMPRESSED);
	CHECK(UIFiles_Kind("sd:/a.tgc", 1) == UI_FILES_KIND_DISC);
	CHECK(UIFiles_Kind("sd:/a.fdi", 1) == UI_FILES_KIND_DISC);
	CHECK(UIFiles_Kind("sd:/a.png", 1) == UI_FILES_KIND_PICTURE);
	CHECK(UIFiles_Kind("sd:/a.txt", 1) == UI_FILES_KIND_TEXT);
	CHECK(UIFiles_Kind("sd:/a.gci", 1) == UI_FILES_KIND_OTHER);
	CHECK(UIFiles_Kind("sd:/noextension", 1) == UI_FILES_KIND_OTHER);
	CHECK(UIFiles_Kind("sd:/GAME.ISO", 1) == UI_FILES_KIND_DISC);
	CHECK(UIFiles_Kind("sd:/Game.nkit.iso", 1) == UI_FILES_KIND_DISC);
	CHECK(UIFiles_Kind("sd:/game.iso", UI_FILES_TYPE_DIR) == UI_FILES_KIND_FOLDER);
	CHECK(UIFiles_Kind("sd:/games/..", UI_FILES_TYPE_PARENT) == UI_FILES_KIND_PARENT);
	CHECK(UIFiles_Kind(NULL, 1) == UI_FILES_KIND_OTHER);

	CHECK(UIFiles_Loads(UI_FILES_KIND_DISC) && UIFiles_Loads(UI_FILES_KIND_PROGRAM) &&
		UIFiles_Loads(UI_FILES_KIND_PROGRAM_FOLDER) && UIFiles_Loads(UI_FILES_KIND_FIRMWARE) &&
		UIFiles_Loads(UI_FILES_KIND_MUSIC));
	CHECK(!UIFiles_Loads(UI_FILES_KIND_DISC_COMPRESSED) && !UIFiles_Loads(UI_FILES_KIND_FOLDER) &&
		!UIFiles_Loads(UI_FILES_KIND_PARENT) && !UIFiles_Loads(UI_FILES_KIND_TEXT) &&
		!UIFiles_Loads(UI_FILES_KIND_PICTURE) && !UIFiles_Loads(UI_FILES_KIND_OTHER));
}

static void testProgramFolders(void)
{
	char text[256];

	/* populate_meta's rewrites under /apps. */
	CHECK(UIFiles_IsProgramFolder("sd:/apps/Foo/Foo.dol", 1, "sd:/apps", false));
	CHECK(UIFiles_IsProgramFolder("sd:/apps/Foo/default.dol", 1, "sd:/apps", false));
	CHECK(UIFiles_IsProgramFolder("sd:/apps/Foo/default.dol", 1, "sd:/apps/", false));
	CHECK(UIFiles_IsProgramFolder("sd:/Foo/default.dol", 1, "sd:/", false));
	/* A plain file in the open folder isn't; nor anything flattened; nor
	 * a folder. */
	CHECK(!UIFiles_IsProgramFolder("sd:/apps/menu.dol", 1, "sd:/apps", false));
	CHECK(!UIFiles_IsProgramFolder("sd:/apps/menu.dol", 1, "sd:/apps/", false));
	CHECK(!UIFiles_IsProgramFolder("sd:/menu.dol", 1, "sd:/", false));
	CHECK(!UIFiles_IsProgramFolder("sd:/apps/Foo/Foo.dol", 1, "sd:/apps", true));
	CHECK(!UIFiles_IsProgramFolder("sd:/apps/Foo", UI_FILES_TYPE_DIR, "sd:/apps", false));
	CHECK(!UIFiles_IsProgramFolder("sd:/apps/..", UI_FILES_TYPE_PARENT, "sd:/apps", false));
	/* "sd:/applications" isn't inside "sd:/apps". */
	CHECK(UIFiles_IsProgramFolder("sd:/applications/x.dol", 1, "sd:/apps", false));

	UIFiles_ProgramFolderPath(text, sizeof(text), "sd:/apps/Foo/default.dol", "sd:/apps");
	CHECK_TEXT(text, "sd:/apps/Foo");
	UIFiles_ProgramFolderPath(text, sizeof(text), "sd:/apps/Foo/Foo.dol", "sd:/apps/");
	CHECK_TEXT(text, "sd:/apps/Foo");
	UIFiles_ProgramFolderPath(text, sizeof(text), "sd:/Foo/default.dol", "sd:/");
	CHECK_TEXT(text, "sd:/Foo");

	/* Its row is the folder's. */
	UIFiles_RowName(text, sizeof(text), "sd:/apps/Foo/default.dol",
		UI_FILES_KIND_PROGRAM_FOLDER, "sd:/apps", "SD Card");
	CHECK_TEXT(text, "Foo");
	UIFiles_RowName(text, sizeof(text), "sd:/apps/menu.dol", UI_FILES_KIND_PROGRAM,
		"sd:/apps", "SD Card");
	CHECK_TEXT(text, "menu.dol");
	UIFiles_RowName(text, sizeof(text), "dvd:/Copper Orchard [GCOE01].iso",
		UI_FILES_KIND_DISC, "dvd:/", "Game Disc");
	CHECK_TEXT(text, "Copper Orchard [GCOE01].iso");
	/* "..": the parent's name, the device one below the root, Other storage
	 * at the root. */
	UIFiles_RowName(text, sizeof(text), "sd:/games/GameCube/..", UI_FILES_KIND_PARENT,
		"sd:/games/GameCube", "SD Card");
	CHECK_TEXT(text, "Up to games");
	UIFiles_RowName(text, sizeof(text), "sd:/a/b/c/..", UI_FILES_KIND_PARENT,
		"sd:/a/b/c/", "SD Card");
	CHECK_TEXT(text, "Up to b");
	UIFiles_RowName(text, sizeof(text), "sd:/games/..", UI_FILES_KIND_PARENT,
		"sd:/games", "SD Card");
	CHECK_TEXT(text, "Up to SD Card");
	UIFiles_RowName(text, sizeof(text), "sd:/..", UI_FILES_KIND_PARENT, "sd:/", "SD Card");
	CHECK_TEXT(text, "Other storage");
	UIFiles_RowName(text, sizeof(text), "dvd:..", UI_FILES_KIND_PARENT, "dvd:", "Game Disc");
	CHECK_TEXT(text, "Other storage");
}

static void testSizes(void)
{
	char text[64];

	UIFiles_SizeText(text, sizeof(text), 1350000000u, 0u, true);
	CHECK_TEXT(text, "1.35 GB");
	UIFiles_SizeText(text, sizeof(text), 1459978240u, 0u, true);
	CHECK_TEXT(text, "1.46 GB");
	UIFiles_SizeText(text, sizeof(text), 1459978240u, 0u, false);
	CHECK_TEXT(text, "1.36 GiB");
	UIFiles_SizeText(text, sizeof(text), 2310000u, 0u, true);
	CHECK_TEXT(text, "2.31 MB");
	UIFiles_SizeText(text, sizeof(text), 1536u, 0u, false);
	CHECK_TEXT(text, "1.5 KiB");
	UIFiles_SizeText(text, sizeof(text), 999u, 0u, true);
	CHECK_TEXT(text, "999 bytes");
	UIFiles_SizeText(text, sizeof(text), 0u, 0u, true);
	CHECK_TEXT(text, "0 bytes");
	/* formatBytes rounds up to the next unit from half the one below. */
	UIFiles_SizeText(text, sizeof(text), 999500000u, 0u, true);
	CHECK_TEXT(text, "1 GB");
	/* Memory cards count 8 KB blocks, a Qoob 64 KB ones. */
	UIFiles_SizeText(text, sizeof(text), 8192u, 8192u, false);
	CHECK_TEXT(text, "1 block");
	UIFiles_SizeText(text, sizeof(text), 8193u, 8192u, false);
	CHECK_TEXT(text, "2 blocks");
	UIFiles_SizeText(text, sizeof(text), 3u * 65536u, 65536u, false);
	CHECK_TEXT(text, "3 blocks");
	UIFiles_PartitionText(text, sizeof(text), 1, 3);
	CHECK_TEXT(text, "Partition 1, ISO 3");

	UIFiles_RowMeta(text, sizeof(text), UI_FILES_KIND_DISC, "1.35 GB");
	CHECK_TEXT(text, "1.35 GB");
	UIFiles_RowMeta(text, sizeof(text), UI_FILES_KIND_DISC_COMPRESSED, "948 MB");
	CHECK_TEXT(text, "Can't start \267 948 MB");
	UIFiles_RowMeta(text, sizeof(text), UI_FILES_KIND_FOLDER, "4 kB");
	CHECK_TEXT(text, "");
	UIFiles_RowMeta(text, sizeof(text), UI_FILES_KIND_PROGRAM_FOLDER, "4 kB");
	CHECK_TEXT(text, "");
	UIFiles_RowMeta(text, sizeof(text), UI_FILES_KIND_PARENT, "");
	CHECK_TEXT(text, "");
	/* Small buffers are cut, never overrun. */
	UIFiles_SizeText(text, 4u, 1350000000u, 0u, true);
	CHECK(strlen(text) == 3u);
}

/* ------------------------------------------------------------------------
 * Fitting real names.
 * --------------------------------------------------------------------- */

/* True when text is whole UTF-8. */
static bool validUtf8(const char *text, size_t length)
{
	size_t i = 0u;

	while(i < length) {
		unsigned char c = (unsigned char)text[i];
		size_t more = c < 0x80u ? 0u : (c & 0xE0u) == 0xC0u ? 1u :
			(c & 0xF0u) == 0xE0u ? 2u : (c & 0xF8u) == 0xF0u ? 3u : 4u;
		size_t k;

		if(more == 4u || i + more >= length) return false;
		for(k = 1u; k <= more; ++k) {
			if(((unsigned char)text[i + k] & 0xC0u) != 0x80u) return false;
		}
		i += more + 1u;
	}
	return true;
}

/* A middle cut's rules: one ellipsis, the name's start before it, exactly
 * the expected tail after it, fits, and the longest start that fits. */
static void checkCut(const char *name, int width, const char *tail)
{
	char out[256], longer[256];
	const char *ellipsis;
	size_t head, tailLength = strlen(tail);
	float scale = UIFiles_FitName(out, sizeof(out), name, width, measure);

	CHECK(scale == UI_FILES_NAME_MIN_SCALE);
	CHECK((float)measure(out) * scale <= (float)width);
	ellipsis = strrchr(out, UI_FILES_ELLIPSIS);
	CHECK(ellipsis != NULL);
	head = (size_t)(ellipsis - out);
	CHECK(strcmp(ellipsis + 1, tail) == 0);
	CHECK(strlen(out) == head + 1u + tailLength);
	CHECK(strncmp(out, name, head) == 0);
	CHECK(head == 0u || out[head - 1u] != ' ');
	/* A tail never stands alone: the start comes first. */
	CHECK(tailLength == 0u || head >= UI_FILES_NAME_MIN_HEAD);
	CHECK(validUtf8(out, head));
	/* One more character of the name would not fit. */
	{
		size_t limit = strlen(name) - tailLength, next;

		for(next = head + 1u; next <= limit; ++next) {
			size_t kept = next;

			if(next < limit && ((unsigned char)name[next] & 0xC0u) == 0x80u) continue;
			while(kept > 0u && name[kept - 1u] == ' ') --kept;
			if(kept <= head) continue;
			memcpy(longer, name, kept);
			longer[kept] = UI_FILES_ELLIPSIS;
			strcpy(longer + kept + 1u, tail);
			CHECK((float)measure(longer) * scale > (float)width);
			break;
		}
	}
}

static void testFitName(void)
{
	static const char *const redump[] = {
		"Signal Garden - The Lost Seasons (USA) (En,Fr,Es) (Disc 1).iso",
		"Signal Garden - The Lost Seasons (USA) (En,Fr,Es) (Disc 2).iso",
		"Moonlit Lake - Tales of the Hollow Shore (Europe) (En,Fr,De,Es,It) [GZLP01].iso",
		"Copper Orchard Deluxe Edition (Japan) (Rev 1) (Disc 10).gcm",
	};
	uiFilesLayout_t layouts[2];
	char out[256];
	float scale;
	int shape, meta;

	/* Short names draw whole at 0.56. */
	scale = UIFiles_FitName(out, sizeof(out), "menu.dol", 174, measure);
	CHECK(scale == UI_FILES_NAME_SCALE);
	CHECK_TEXT(out, "menu.dol");
	/* A little long: whole, shrunk to fit, not below 0.46. */
	scale = UIFiles_FitName(out, sizeof(out), "Copper Orchard [GCOE01].iso", 174, measure);
	CHECK_TEXT(out, "Copper Orchard [GCOE01].iso");
	CHECK(scale < UI_FILES_NAME_SCALE && scale >= UI_FILES_NAME_MIN_SCALE);
	CHECK((float)measure(out) * scale <= 174.0f + 0.01f);
	/* The largest scale that fits, not the smallest. */
	scale = UIFiles_FitName(out, sizeof(out), "Copper Orchard [GCOE01].iso", 200, measure);
	CHECK_TEXT(out, "Copper Orchard [GCOE01].iso");
	CHECK(scale > 0.52f && scale < 0.54f);
	CHECK((float)measure(out) * scale <= 200.0f + 0.01f);
	CHECK((float)measure(out) * scale >= 200.0f - 0.01f);
	/* Exactly at 0.56 and exactly at 0.46 (350 px of name): whole. */
	scale = UIFiles_FitName(out, sizeof(out), "Copper Orchard (GCOE).iso", 196, measure);
	CHECK_TEXT(out, "Copper Orchard (GCOE).iso");
	CHECK(scale == UI_FILES_NAME_SCALE);
	scale = UIFiles_FitName(out, sizeof(out), "Copper Orchard (GCOE).iso", 161, measure);
	CHECK_TEXT(out, "Copper Orchard (GCOE).iso");
	CHECK(scale >= UI_FILES_NAME_MIN_SCALE - 0.0001f && scale <= UI_FILES_NAME_MIN_SCALE + 0.0001f);
	scale = UIFiles_FitName(out, sizeof(out), "Copper Orchard (GCOE).iso", 160, measure);
	CHECK(strchr(out, UI_FILES_ELLIPSIS) != NULL && scale == UI_FILES_NAME_MIN_SCALE);

	/* Cuts at 4:3 beside a size, as the screen draws them. */
	checkCut(redump[0], 174, "(Disc 1).iso");
	UIFiles_FitName(out, sizeof(out), redump[0], 174, measure);
	CHECK_TEXT(out, "Signal Garden\205(Disc 1).iso");
	checkCut(redump[2], 174, ".iso");
	UIFiles_FitName(out, sizeof(out), redump[2], 174, measure);
	CHECK_TEXT(out, "Moonlit Lake - Tales o\205.iso");

	/* Both screen shapes, with and without a size, both panes. */
	UIFiles_Layout(0.0f, 640.0f, &layouts[0]);
	UIFiles_Layout(-106.5f, 746.5f, &layouts[1]);
	for(shape = 0; shape < 2; ++shape) {
		for(meta = 0; meta <= 44; meta += 44) {
			int p;

			for(p = 0; p < UI_FILES_PANES; ++p) {
				int width = UIFiles_NameWidth(&layouts[shape], p, meta, false);
				size_t i;

				for(i = 0u; i < sizeof(redump) / sizeof(redump[0]); ++i) {
					const char *name = redump[i];
					const char *tail = i == 2u ? ".iso" : i == 3u ? "(Disc 10).gcm" :
						i == 1u ? "(Disc 2).iso" : "(Disc 1).iso";

					if((float)measure(name) * UI_FILES_NAME_MIN_SCALE <= (float)width) {
						scale = UIFiles_FitName(out, sizeof(out), name, width, measure);
						CHECK_TEXT(out, name);
					}
					else {
						checkCut(name, width, tail);
					}
				}
			}
		}
	}
	/* Wide 4:3 rows gain characters. */
	{
		char narrow[256], wide[256];

		UIFiles_FitName(narrow, sizeof(narrow), redump[2],
			UIFiles_NameWidth(&layouts[0], 0, 44, false), measure);
		UIFiles_FitName(wide, sizeof(wide), redump[2],
			UIFiles_NameWidth(&layouts[1], 0, 44, false), measure);
		CHECK(strlen(wide) >= strlen(narrow) + 10u);
	}

	/* Only "(Disc N)" survives as a tail: region, language and ID tags are
	 * cut like the rest. Not a disc tag: "(Disco)", "(Disc 1234)". */
	checkCut("A very long name for a party game (Disco).iso", 120, ".iso");
	checkCut("A very long name for a party game (Disc 1234).iso", 120, ".iso");
	checkCut("A very long name for a party game (Disc A).iso", 160, "(Disc A).iso");
	/* Region and language tags right before the extension are cut too. */
	checkCut("Moonlit Lake - Tales of the Hollow Shore (Europe).iso", 174, ".iso");
	UIFiles_FitName(out, sizeof(out), "Moonlit Lake - Tales of the Hollow Shore (Europe).iso",
		174, measure);
	CHECK_TEXT(out, "Moonlit Lake - Tales o\205.iso");
	checkCut("Moonlit Lake - Tales of the Hollow Shore (Germany).iso", 174, ".iso");
	checkCut("Moonlit Lake - Tales of the Hollow Shore (En,Fr,De).iso", 174, ".iso");
	checkCut("Moonlit Lake - Tales of the Hollow Shore (Disc).iso", 174, ".iso");
	checkCut("Signal Garden - The Lost Seasons Collection (USA).iso", 174, ".iso");
	/* A narrow column beside "Can't start": the disc tail would leave no
	 * start, so the extension alone, never a bare "(Disc 1).rvz". */
	{
		const char *rvz = "Signal Garden - The Long Way Home (USA) (Disc 1).rvz";
		int width;

		checkCut(rvz, 87, ".rvz");
		UIFiles_FitName(out, sizeof(out), rvz, 87, measure);
		CHECK_TEXT(out, "Signal G\205.rvz");
		for(width = 60; width < 300; ++width) {
			const char *cut;

			UIFiles_FitName(out, sizeof(out), rvz, width, measure);
			cut = strchr(out, UI_FILES_ELLIPSIS);
			CHECK(cut != NULL);
			CHECK(strcmp(cut + 1, "(Disc 1).rvz") == 0 || strcmp(cut + 1, ".rvz") == 0 ||
				cut[1] == '\0');
			CHECK(cut[1] == '\0' || cut - out >= UI_FILES_NAME_MIN_HEAD);
		}
	}
	/* No extension, or a "dot" that isn't one: the start and an ellipsis. */
	checkCut("A very long name for a party game without any extension", 120, "");
	checkCut("Version 2.1 of a very long homebrew build name", 120, "");
	checkCut("homebrew-launcher-with-a-very-long-name.dol+cli", 160, ".dol+cli");
	/* Too narrow for the disc tail: the extension alone; then nothing. */
	checkCut("Signal Garden - The Lost Seasons (USA) (Disc 1).iso", 80, ".iso");
	checkCut("Signal Garden - The Lost Seasons (USA) (Disc 1).iso", 30, "");
	scale = UIFiles_FitName(out, sizeof(out), "Signal Garden.iso", 5, measure);
	CHECK_TEXT(out, "");

	/* UTF-8: never cut inside a sequence, at any width. (No 0x85 in it:
	 * that byte is the ellipsis.) */
	{
		const char *accented = "Caf\xc3\xa9 \xc3\xa9t\xc3\xa9 \xe2\x9c\x93\xe2\x9c\x93 "
			"\xf0\x9f\x8e\xae na\xc3\xafve r\xc3\xa9sum\xc3\xa9 (Disc 1).iso";
		int width;

		for(width = 40; width < 400; width += 3) {
			scale = UIFiles_FitName(out, sizeof(out), accented, width, measure);
			if(strchr(out, UI_FILES_ELLIPSIS) != NULL) {
				size_t head = (size_t)(strrchr(out, UI_FILES_ELLIPSIS) - out);

				CHECK(validUtf8(out, head));
				CHECK(strncmp(out, accented, head) == 0);
			}
			CHECK((float)measure(out) * scale <= (float)width + 0.01f);
		}
	}
	/* A tiny buffer is never overrun and still ends in the tail. */
	{
		char small[16];

		UIFiles_FitName(small, sizeof(small), redump[0], 400, measure);
		CHECK(strlen(small) < sizeof(small));
		CHECK(strcmp(small + strlen(small) - 4, ".iso") == 0);
	}
}

static void testFitPath(void)
{
	char out[256];

	UIFiles_FitPath(out, sizeof(out), "sd:/games", 200, 0.46f, measure);
	CHECK_TEXT(out, "/games");
	UIFiles_FitPath(out, sizeof(out), "sd:/games/", 200, 0.46f, measure);
	CHECK_TEXT(out, "/games");
	UIFiles_FitPath(out, sizeof(out), "sd:/", 200, 0.46f, measure);
	CHECK_TEXT(out, "/");
	UIFiles_FitPath(out, sizeof(out), "dvd:", 200, 0.46f, measure);
	CHECK_TEXT(out, "/");
	/* Cut from the left at a folder. */
	UIFiles_FitPath(out, sizeof(out), "sd:/backups/Old/Collection/GameCube", 150, 0.46f,
		measure);
	CHECK_TEXT(out, "\205/Collection/GameCube");
	CHECK((float)measure(out) * 0.46f <= 150.0f);
	/* One folder too long for that: cut inside it. */
	UIFiles_FitPath(out, sizeof(out), "sd:/a/An extremely long folder name for games", 150,
		0.46f, measure);
	CHECK(out[0] == '\205' && strchr(out, '/') == NULL);
	CHECK((float)measure(out) * 0.46f <= 150.0f);
	CHECK(strcmp(out + strlen(out) - 5, "games") == 0);
	/* Never inside a UTF-8 sequence. */
	{
		int width;

		for(width = 20; width < 200; width += 3) {
			UIFiles_FitPath(out, sizeof(out), "sd:/\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9/"
				"\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9", width, 0.46f, measure);
			if(out[0] == '\205') CHECK(validUtf8(out + 1, strlen(out + 1)));
			CHECK((float)measure(out) * 0.46f <= (float)width);
		}
	}
}

/* The device's name over a pane keeps clear of the SOURCE chip and the
 * free box, in both screen shapes, at about the IPL font's width. */
static void testFitDevice(void)
{
	static const char *const names[] = {
		"GC Loader", "SD Card - SD2SP2", "SD Card - Slot B", "Memory Card - Slot A",
		"Wiikey / Wasp Fusion", "USB Gecko - Slot B only"
	};
	uiFilesLayout_t layouts[2];
	char out[48];
	int l, n, p, read;

	UIFiles_Layout(0.0f, 640.0f, &layouts[0]);
	UIFiles_Layout(-106.5f, 746.5f, &layouts[1]);
	for(l = 0; l < 2; ++l) {
		for(n = 0; n < (int)(sizeof(names) / sizeof(names[0])); ++n) {
			for(p = 0; p < UI_FILES_PANES; ++p) {
				for(read = 0; read < 2; ++read) {
					const uiFilesRect_t *box = &layouts[l].pane[p];
					int freeWidth = read ? 92 : 72, word = read ? 0 : 23;
					float scale = UIFiles_FitDevice(out, sizeof(out), names[n], &layouts[l],
						p, p == UI_FILES_LEFT, freeWidth, word, measure);
					int right = box->x0 + 2 + (int)((float)measure(out) * scale + 0.5f) +
						(p == UI_FILES_LEFT ? 10 + UI_FILES_CHIP_W : 0);

					CHECK(out[0] != '\0');
					CHECK(scale >= UI_FILES_DEVICE_MIN_SCALE && scale <= UI_FILES_DEVICE_SCALE);
					CHECK(right + 8 <= box->x1 - freeWidth - (word ? word + 8 : 0));
				}
			}
		}
	}
	/* Short: whole at 0.92 (this measure is wider than the font, so in
	 * Menu Widescreen). Long in 4:3 beside the chip: cut at its end. */
	CHECK(UIFiles_FitDevice(out, sizeof(out), "GC Loader", &layouts[1], 0, true, 72, 23,
		measure) == UI_FILES_DEVICE_SCALE);
	CHECK_TEXT(out, "GC Loader");
	CHECK(UIFiles_FitDevice(out, sizeof(out), "Memory Card - Slot A", &layouts[0], 0, true,
		92, 0, measure) == UI_FILES_DEVICE_MIN_SCALE);
	CHECK(strncmp(out, "Memory", 6) == 0 && out[strlen(out) - 1] == '\205');
}

/* ------------------------------------------------------------------------
 * Hints.
 * --------------------------------------------------------------------- */
static int roundGlyphs(const char *text)
{
	uiHintItem_t items[UI_HINT_MAX_ITEMS];
	int count = UIHint_Parse(text, items, UI_HINT_MAX_ITEMS), i, j, round = 0;

	for(i = 0; i < count; ++i) {
		for(j = 0; j < items[i].glyphCount; ++j) {
			uiHintGlyph_t g = items[i].glyph[j];

			round += g == UI_HINT_GLYPH_A || g == UI_HINT_GLYPH_B || g == UI_HINT_GLYPH_X ||
				g == UI_HINT_GLYPH_Y || g == UI_HINT_GLYPH_Z;
		}
	}
	return round;
}

static void checkHints(uiFilesHintMode_t mode, int pane, uiFilesKind_t kind, bool loads,
	bool fm, bool autoloadOn, const char *wantLeft, const char *wantRight)
{
	char left[UI_FILES_HINT_CAPACITY], right[UI_FILES_HINT_CAPACITY];

	UIFiles_Hints(mode, pane, kind, loads, fm, autoloadOn, left, right);
	CHECK_TEXT(left, wantLeft);
	CHECK_TEXT(right, wantRight);
}

static void testHints(void)
{
	int mode, pane, kind, flags;

	checkHints(UI_FILES_HINTS_LIST, 0, UI_FILES_KIND_DISC, true, true, false,
		"A  Details   Z  Actions   \213  \233  Other side", "X  Up   B  Home");
	checkHints(UI_FILES_HINTS_LIST, 0, UI_FILES_KIND_PROGRAM, true, true, false,
		"A  Start   Z  Actions   \213  \233  Other side", "X  Up   B  Home");
	checkHints(UI_FILES_HINTS_LIST, 0, UI_FILES_KIND_PROGRAM_FOLDER, true, true, false,
		"A  Start   Z  Actions   \213  \233  Other side", "X  Up   B  Home");
	checkHints(UI_FILES_HINTS_LIST, 1, UI_FILES_KIND_PROGRAM_FOLDER, true, true, false,
		"A  Open   Z  Actions   \213  \233  Other side", "X  Up   B  Home");
	checkHints(UI_FILES_HINTS_LIST, 1, UI_FILES_KIND_FOLDER, false, true, false,
		"A  Open   Z  Actions   \213  \233  Other side", "X  Up   B  Home");
	checkHints(UI_FILES_HINTS_LIST, 0, UI_FILES_KIND_PARENT, false, true, false,
		"A  Open   Z  Autoload   \213  \233  Other side", "X  Up   B  Home");
	checkHints(UI_FILES_HINTS_LIST, 0, UI_FILES_KIND_PARENT, false, true, true,
		"A  Open   Z  Autoload off   \213  \233  Other side", "X  Up   B  Home");
	checkHints(UI_FILES_HINTS_LIST, 1, UI_FILES_KIND_DISC, true, true, false,
		"Z  Actions   Y  Swap sides   \213  \233  Other side", "X  Up   B  Home");
	checkHints(UI_FILES_HINTS_LIST, 0, UI_FILES_KIND_DISC_COMPRESSED, false, true, false,
		"A  Actions   \213  \233  Other side", "X  Up   B  Home");
	/* A FlippyDrive update on the FlippyDrive opens Actions. */
	checkHints(UI_FILES_HINTS_LIST, 0, UI_FILES_KIND_FIRMWARE, false, true, false,
		"A  Actions   \213  \233  Other side", "X  Up   B  Home");
	/* Without File Management, no Z. */
	checkHints(UI_FILES_HINTS_LIST, 0, UI_FILES_KIND_DISC, true, false, false,
		"A  Details   \213  \233  Other side", "X  Up   B  Home");
	checkHints(UI_FILES_HINTS_LIST, 0, UI_FILES_KIND_PARENT, false, false, true,
		"A  Open   \213  \233  Other side", "X  Up   B  Home");
	checkHints(UI_FILES_HINTS_LIST, 1, UI_FILES_KIND_PROGRAM, true, false, false,
		"Y  Swap sides   \213  \233  Other side", "X  Up   B  Home");
	checkHints(UI_FILES_HINTS_LIST, 1, UI_FILES_KIND_TEXT, false, false, false,
		"\213  \233  Other side", "X  Up   B  Home");
	checkHints(UI_FILES_HINTS_BOX, 0, UI_FILES_KIND_DISC, true, true, false,
		"A  Choose", "B  Back");
	checkHints(UI_FILES_HINTS_QUESTION, 0, UI_FILES_KIND_DISC, true, true, false,
		"A  Choose", "B  Cancel");
	checkHints(UI_FILES_HINTS_DELETE, 0, UI_FILES_KIND_DISC, true, true, false,
		"L+A  Delete", "B  Cancel");
	checkHints(UI_FILES_HINTS_MESSAGE, 0, UI_FILES_KIND_DISC, true, true, false, "A  OK", "");
	/* A right pane with nothing to act on: no Z, A chooses its storage. */
	checkHints(UI_FILES_HINTS_STORAGE, 1, UI_FILES_KIND_FOLDER, false, true, false,
		"A  Choose storage   \213  \233  Other side", "B  Home");

	/* Every context: at most five round buttons, and the D-pad drawn as
	 * the glyph, not text. */
	for(mode = UI_FILES_HINTS_LIST; mode <= UI_FILES_HINTS_STORAGE; ++mode) {
		for(pane = 0; pane < 2; ++pane) {
			for(kind = 0; kind < UI_FILES_KINDS; ++kind) {
				for(flags = 0; flags < 8; ++flags) {
					char left[UI_FILES_HINT_CAPACITY], right[UI_FILES_HINT_CAPACITY];
					uiHintItem_t items[UI_HINT_MAX_ITEMS];
					int count, i;

					UIFiles_Hints((uiFilesHintMode_t)mode, pane, (uiFilesKind_t)kind,
						(flags & 1) != 0, (flags & 2) != 0, (flags & 4) != 0, left, right);
					CHECK(roundGlyphs(left) + roundGlyphs(right) <= 5);
					CHECK(((flags & 2) != 0 || mode != UI_FILES_HINTS_LIST) ||
						strstr(left, "Z  ") == NULL);
					count = UIHint_Parse(left, items, UI_HINT_MAX_ITEMS);
					for(i = 0; i < count; ++i) CHECK(items[i].glyphCount > 0);
					count = UIHint_Parse(right, items, UI_HINT_MAX_ITEMS);
					for(i = 0; i < count; ++i) CHECK(items[i].glyphCount > 0);
				}
			}
		}
	}
}

/* ------------------------------------------------------------------------
 * Storage.
 * --------------------------------------------------------------------- */

/* deviceHandler.h's LOC_* bits and the handlers' own. */
enum {
	SLOT_A = 0x1, SLOT_B = 0x2, DVD = 0x4, SP1 = 0x8, SP2 = 0x10, HSP = 0x20, SYSTEM = 0x40
};

static int handlers[16];

static uiFilesDevice_t device(int id, const char *name, uint32_t location, bool network)
{
	uiFilesDevice_t d;

	memset(&d, 0, sizeof(d));
	d.handler = &handlers[id];
	d.name = name;
	d.location = location;
	d.network = network;
	return d;
}

static void testStorage(void)
{
	const uiFilesDevice_t sdA = device(0, "SD Card", SLOT_A, false);
	const uiFilesDevice_t sdB = device(1, "SD Card", SLOT_B, false);
	const uiFilesDevice_t sd2 = device(2, "SD Card", SP2, false);
	const uiFilesDevice_t gcl = device(3, "GC Loader", DVD, false);
	const uiFilesDevice_t dvd = device(4, "Game Disc", DVD, false);
	const uiFilesDevice_t flippy = device(5, "FlippyDrive", DVD, false);
	const uiFilesDevice_t flash = device(6, "FlippyDrive flash", DVD | SYSTEM, false);
	const uiFilesDevice_t smb = device(7, "SMB", SP1, true);
	const uiFilesDevice_t ftp = device(8, "FTP", SP1, true);
	const uiFilesDevice_t fsp = device(9, "FSP", SP1, true);
	const uiFilesDevice_t cardA = device(10, "Memory Card", SLOT_A, false);
	const uiFilesDevice_t cardB = device(11, "Memory Card", SLOT_B, false);
	const uiFilesDevice_t qoob = device(12, "Qoob", SYSTEM, false);
	const uiFilesDevice_t kunai = device(13, "KunaiGC", SYSTEM, false);
	const uiFilesDevice_t system = device(14, "System", SYSTEM, false);
	const uiFilesDevice_t wode = device(15, "WODE", DVD, false);
	const uiFilesDevice_t smbUnknown = device(7, "SMB", 0, true);
	const uiFilesDevice_t ftpUnknown = device(8, "FTP", 0, true);
	const uiFilesDevice_t *sds[] = {&sdA, &sdB, &sd2};
	const uiFilesDevice_t *all[] = {&sdA, &sdB, &sd2, &gcl, &dvd, &flippy, &flash, &smb,
		&ftp, &fsp, &cardA, &cardB, &qoob, &kunai, &system, &wode};
	char reason[UI_FILES_TEXT_CAPACITY];
	size_t i, j;

	/* SD with SD, and every disc-connector or network device with SD. */
	for(i = 0u; i < 3u; ++i) {
		for(j = 0u; j < 3u; ++j) CHECK(!UIFiles_StorageClash(sds[i], sds[j], reason, sizeof(reason)));
		CHECK(!UIFiles_StorageClash(&gcl, sds[i], reason, sizeof(reason)));
		CHECK(!UIFiles_StorageClash(&dvd, sds[i], reason, sizeof(reason)));
		CHECK(!UIFiles_StorageClash(&flippy, sds[i], reason, sizeof(reason)));
		CHECK(!UIFiles_StorageClash(&smb, sds[i], reason, sizeof(reason)));
		CHECK(!UIFiles_StorageClash(&ftp, sds[i], reason, sizeof(reason)));
		CHECK(!UIFiles_StorageClash(&fsp, sds[i], reason, sizeof(reason)));
		CHECK(!UIFiles_StorageClash(sds[i], &gcl, reason, sizeof(reason)));
		CHECK(reason[0] == '\0');
	}
	/* FlippyDrive and its flash share the disc connector. */
	CHECK(UIFiles_StorageClash(&flash, &flippy, reason, sizeof(reason)));
	CHECK_TEXT(reason, "FlippyDrive is open on the other side.");
	CHECK(UIFiles_StorageClash(&flippy, &flash, reason, sizeof(reason)));
	CHECK_TEXT(reason, "FlippyDrive flash is open on the other side.");
	/* One network adapter, even before its location is known. */
	CHECK(UIFiles_StorageClash(&ftp, &smb, reason, sizeof(reason)));
	CHECK_TEXT(reason, "The network adapter is in use on the other side.");
	CHECK(UIFiles_StorageClash(&fsp, &smb, NULL, 0u));
	CHECK(UIFiles_StorageClash(&ftpUnknown, &smbUnknown, reason, sizeof(reason)));
	CHECK_TEXT(reason, "The network adapter is in use on the other side.");
	/* The same handler on both sides is always allowed. */
	for(i = 0u; i < sizeof(all) / sizeof(all[0]); ++i) {
		CHECK(!UIFiles_StorageClash(all[i], all[i], reason, sizeof(reason)));
	}
	CHECK(!UIFiles_StorageClash(&smb, &smbUnknown, reason, sizeof(reason)));
	/* Disc-connector devices against each other, and the system ones. */
	CHECK(UIFiles_StorageClash(&gcl, &dvd, NULL, 0u));
	CHECK(UIFiles_StorageClash(&wode, &flippy, NULL, 0u));
	CHECK(UIFiles_StorageClash(&qoob, &kunai, NULL, 0u));
	CHECK(UIFiles_StorageClash(&system, &qoob, NULL, 0u));
	CHECK(UIFiles_StorageClash(&flash, &system, NULL, 0u));
	/* Memory card slots beside anything they can sit beside. */
	CHECK(!UIFiles_StorageClash(&cardA, &cardB, NULL, 0u));
	CHECK(!UIFiles_StorageClash(&cardA, &sdB, NULL, 0u));
	CHECK(!UIFiles_StorageClash(&cardB, &gcl, NULL, 0u));
	CHECK(!UIFiles_StorageClash(&cardA, &sd2, NULL, 0u));
	/* Symmetric, and nothing on the other side clashes with nothing. */
	for(i = 0u; i < sizeof(all) / sizeof(all[0]); ++i) {
		for(j = 0u; j < sizeof(all) / sizeof(all[0]); ++j) {
			CHECK(UIFiles_StorageClash(all[i], all[j], NULL, 0u) ==
				UIFiles_StorageClash(all[j], all[i], NULL, 0u));
		}
		CHECK(!UIFiles_StorageClash(all[i], NULL, NULL, 0u));
	}

	CHECK(!UIFiles_FreeKnown(false, 32000000000u, false));
	CHECK(!UIFiles_FreeKnown(true, 0u, false));
	CHECK(!UIFiles_FreeKnown(true, 32000000000u, true));
	CHECK(UIFiles_FreeKnown(true, 32000000000u, false));

	CHECK(!UIFiles_CanSwap(UI_FILES_UNMOUNTED, true) && !UIFiles_CanSwap(UI_FILES_UNMOUNTED, false));
	CHECK(UIFiles_CanSwap(UI_FILES_SHARED, true) && UIFiles_CanSwap(UI_FILES_SHARED, false));
	CHECK(UIFiles_CanSwap(UI_FILES_OWN, true) && !UIFiles_CanSwap(UI_FILES_OWN, false));
	CHECK(!UIFiles_CanSwap(UI_FILES_FAILED, true) && !UIFiles_CanSwap(UI_FILES_FAILED, false));
}

/* ------------------------------------------------------------------------
 * Actions.
 * --------------------------------------------------------------------- */
static uiFilesSide_t side(int id, const char *name, const char *folder, bool writable)
{
	uiFilesSide_t s;

	memset(&s, 0, sizeof(s));
	s.device = device(id, name, SLOT_A, false);
	s.device.metric = true;
	s.device.canWrite = writable;
	s.device.canRename = writable;
	s.device.canHide = writable;
	s.device.canDelete = writable;
	s.folder = folder;
	s.mount = UI_FILES_OWN;
	s.readOk = true;
	s.freeKnown = true;
	s.freeBytes = 29100000000u;
	return s;
}

static uiFilesEntry_t file(uint64_t size)
{
	uiFilesEntry_t e;

	memset(&e, 0, sizeof(e));
	e.pane = UI_FILES_LEFT;
	e.isFile = true;
	e.needed = size;
	return e;
}

/* The right pane's first storage, its "isn't ready" words, and the L and
 * R menus. */
static void testSecondDevice(void)
{
	const uiFilesDevice_t gcl = device(3, "GC Loader", DVD, false);
	const uiFilesDevice_t sdA = device(0, "SD Card - Slot A", SLOT_A, false);
	const uiFilesDevice_t sd2 = device(2, "SD Card - SD2SP2", SP2, false);
	const uiFilesDevice_t flippy = device(5, "FlippyDrive", DVD, false);
	const uiFilesDevice_t flash = device(6, "FlippyDrive Flash", DVD | SYSTEM, false);
	const uiFilesDevice_t smb = device(7, "SMB 1.0/CIFS", SP1, true);
	const uiFilesDevice_t ftp = device(8, "File Transfer Protocol", SP1, true);
	const uiFilesDevice_t cardA = device(10, "Memory Card - Slot A", SLOT_A, false);
	const uiFilesDevice_t many[] = {gcl, sdA, sd2, cardA, smb, ftp, flash};
	char lines[UI_FILES_MESSAGE_LINES][UI_FILES_MESSAGE_TEXT];
	uiFilesStorageMenu_t menu;
	float alpha, scale;
	int fm, have, detected, source, i;

	/* The Configuration Device only with File Management, and only when it
	 * is set, detected and not the Source. */
	for(fm = 0; fm < 2; ++fm) for(have = 0; have < 2; ++have)
	for(detected = 0; detected < 2; ++detected) for(source = 0; source < 2; ++source) {
		CHECK(UIFiles_RightOnConfig(fm != 0, have != 0, detected != 0, source != 0) ==
			(fm && have && detected && !source));
	}

	UIFiles_NotReady(lines, "SD Card - SD2SP2", "No card is inserted.", NULL);
	CHECK_TEXT(lines[0], "SD Card - SD2SP2 isn't ready.");
	CHECK_TEXT(lines[1], "(No card is inserted.)");
	CHECK_TEXT(lines[2], "Press R to choose storage.");
	UIFiles_NotReady(lines, "SD Card - SD2SP2", "No card is inserted.", "backups");
	CHECK_TEXT(lines[1], "Couldn't read backups.");
	UIFiles_NotReady(lines, "Game Disc", "", "");
	CHECK_TEXT(lines[1], "Couldn't read its top folder.");
	UIFiles_NotReady(lines, NULL, NULL, NULL);
	CHECK_TEXT(lines[0], "This storage isn't ready.");
	CHECK_TEXT(lines[1], "");

	/* Boxes open from 0.92x (0.97x Reduced) over 0.08 s and fade out over
	 * 0.10 s; Off is at once. */
	UIFiles_MenuMotion(0.0f, true, UI_MOTION_FULL, &alpha, &scale);
	CHECK(alpha == 0.0f && scale == 0.92f);
	UIFiles_MenuMotion(0.04f, true, UI_MOTION_FULL, &alpha, &scale);
	CHECK(alpha > 0.4f && alpha < 0.6f && scale > 0.95f && scale < 0.97f);
	UIFiles_MenuMotion(0.08f, true, UI_MOTION_FULL, &alpha, &scale);
	CHECK(alpha == 1.0f && scale == 1.0f);
	UIFiles_MenuMotion(0.0f, true, UI_MOTION_REDUCED, &alpha, &scale);
	CHECK(alpha == 0.0f && scale == 0.97f);
	UIFiles_MenuMotion(0.05f, false, UI_MOTION_FULL, &alpha, &scale);
	CHECK(alpha > 0.4f && alpha < 0.6f && scale == 1.0f);
	UIFiles_MenuMotion(0.10f, false, UI_MOTION_REDUCED, &alpha, &scale);
	CHECK(alpha == 0.0f);
	UIFiles_MenuMotion(0.0f, true, UI_MOTION_OFF, &alpha, &scale);
	CHECK(alpha == 1.0f && scale == 1.0f);
	UIFiles_MenuMotion(0.0f, false, UI_MOTION_OFF, &alpha, &scale);
	CHECK(alpha == 0.0f);

	/* L, GC Loader the Source and SMB on the right: FTP greyed for the
	 * network adapter, SMB itself offered, the Source focused. */
	{
		const uiFilesDevice_t listed[] = {gcl, sdA, sd2, smb, ftp};

		UIFiles_StorageMenu(UI_FILES_LEFT, listed, 5, &gcl, &smb, "/share/backups", &menu);
	}
	CHECK_TEXT(menu.box.title, "Left storage");
	CHECK(menu.box.count == 6 && menu.devices == 5 && menu.box.pane == 0 && menu.box.open);
	CHECK_TEXT(menu.box.item[0], "GC Loader");
	CHECK_TEXT(menu.box.item[5], "Other devices\205");
	CHECK(menu.box.dim == (1u << 4) && menu.box.focus == 0);
	/* A greyed device's reason is amber, and only its. */
	CHECK(menu.warn == menu.box.dim);
	CHECK_TEXT(menu.line[0], "Shown on the left now. Games start from it.");
	CHECK_TEXT(menu.line[1], "Becomes the Source: games start from it.");
	CHECK_TEXT(menu.line[3], "Also open on the right, in /share/backups. Both sides can show it, each in its own folder.");
	CHECK_TEXT(menu.line[4], "Can't open on the left while SMB 1.0/CIFS is open on the right.");
	CHECK_TEXT(menu.reason[4], "The network adapter is in use on the other side.");
	CHECK_TEXT(menu.line[5], "Every storage, with each one's settings.");
	for(i = 0; i < 4; ++i) CHECK(menu.reason[i][0] == '\0');
	CHECK(menu.reason[5][0] == '\0');

	/* R, the right pane on SD2SP2 beside FlippyDrive: the flash greyed,
	 * the Source not; past six devices the rest are in Other devices. */
	{
		const uiFilesDevice_t listed[] = {flippy, flash, sd2, sdA, cardA, smb, ftp, gcl};

		UIFiles_StorageMenu(UI_FILES_RIGHT, listed, 8, &sd2, &flippy, "/games", &menu);
	}
	CHECK_TEXT(menu.box.title, "Right storage");
	CHECK(menu.box.pane == 1 && menu.devices == UI_FILES_STORAGE_DEVICES);
	CHECK(menu.box.count == UI_FILES_STORAGE_DEVICES + 1 &&
		menu.box.count <= UI_FILES_MENU_ITEMS(true));
	CHECK(menu.box.dim == (1u << 1) && menu.box.focus == 2);
	CHECK_TEXT(menu.line[0], "Also open on the left, in /games. Both sides can show it, each in its own folder.");
	CHECK_TEXT(menu.reason[1], "FlippyDrive is open on the other side.");
	CHECK_TEXT(menu.line[2], "Shown on the right now.");
	CHECK_TEXT(menu.line[3], "Opens on the right.");
	CHECK_TEXT(menu.box.item[6], "Other devices\205");
	CHECK_TEXT(menu.line[6], "Every storage that can be written to.");

	/* The current device not listed (chosen with every device shown): the
	 * first one not greyed has the focus; nothing but the Source, the menu
	 * is the Source and Other devices. */
	UIFiles_StorageMenu(UI_FILES_RIGHT, many + 6, 1, &smb, &ftp, "/", &menu);
	CHECK(menu.box.count == 2 && menu.box.dim == 0u && menu.box.focus == 0);
	UIFiles_StorageMenu(UI_FILES_RIGHT, many, 7, &flippy, &smb, "/", &menu);
	CHECK(menu.box.dim == (1u << 5) && menu.box.focus == 0);
	{
		const uiFilesDevice_t listed[] = {ftp, flash, gcl};

		UIFiles_StorageMenu(UI_FILES_RIGHT, listed, 3, &sdA, &smb, "/", &menu);
	}
	CHECK(menu.box.dim == 1u && menu.box.focus == 1);
	UIFiles_StorageMenu(UI_FILES_LEFT, &gcl, 1, &gcl, &gcl, "/backups", &menu);
	CHECK(menu.box.count == 2 && menu.box.focus == 0);
	CHECK_TEXT(menu.line[0], "Shown on the left now. Games start from it.");
	UIFiles_StorageMenu(UI_FILES_LEFT, NULL, 0, &gcl, &sdA, "/", &menu);
	CHECK(menu.box.count == 1 && menu.devices == 0 && menu.box.focus == 0);
	CHECK_TEXT(menu.box.item[0], "Other devices\205");
}

static void testAvailability(void)
{
	uiFilesSide_t gcl = side(3, "GC Loader", "gcl:/games", true);
	uiFilesSide_t sd = side(0, "SD Card", "sd:/backups", true);
	uiFilesEntry_t e = file(1350000000u);
	uiFilesAvailability_t a;
	int i;

	gcl.mount = UI_FILES_SHARED;
	/* Everything allowed, and the fit on line 2. */
	UIFiles_Availability(&gcl, &sd, &e, &a);
	for(i = 0; i < UI_FILES_ACTIONS; ++i) CHECK(a.enabled[i]);
	CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "Needs 1.35 GB. It fits.");
	CHECK_TEXT(a.line[UI_FILES_ACTION_MOVE], "Needs 1.35 GB. It fits.");
	CHECK(!a.warn[UI_FILES_ACTION_COPY] && !a.replaceOnly[0] && !a.replaceOnly[1]);
	CHECK_TEXT(a.line[UI_FILES_ACTION_RENAME], "");
	CHECK_TEXT(a.label[UI_FILES_ACTION_COPY], "Copy");
	CHECK_TEXT(a.label[UI_FILES_ACTION_HIDE], "Hide");
	CHECK(a.letter[0] == 'X' && a.letter[1] == 'Y' && a.letter[2] == 'R' &&
		a.letter[3] == 'L' && a.letter[4] == 'Z');
	CHECK(UIFiles_FirstEnabled(a.enabled, UI_FILES_ACTIONS) == 0);
	e.hidden = true;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK_TEXT(a.label[UI_FILES_ACTION_HIDE], "Unhide");
	e.hidden = false;

	/* Folders, and program folders, which act as theirs. */
	e.isFile = false;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK(!a.enabled[0] && !a.enabled[1] && a.enabled[2] && a.enabled[3] && a.enabled[4]);
	CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "Folders can't be copied yet.");
	CHECK_TEXT(a.line[UI_FILES_ACTION_MOVE], "Folders can't be moved yet.");
	CHECK(a.warn[UI_FILES_ACTION_COPY]);
	CHECK(UIFiles_FirstEnabled(a.enabled, UI_FILES_ACTIONS) == UI_FILES_ACTION_RENAME);
	e.isFile = true;
	e.programFolder = true;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK(!a.enabled[0] && !a.enabled[1] && a.enabled[2] && a.enabled[3] && a.enabled[4]);
	CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "Folders can't be copied yet.");
	e.programFolder = false;

	/* The other side unmounted, failed, or its read failed: the button to
	 * fix it is the other pane's. */
	sd.mount = UI_FILES_UNMOUNTED;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK(!a.enabled[0] && !a.enabled[1] && a.enabled[2]);
	CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "SD Card isn't ready. Choose storage with R.");
	CHECK_TEXT(a.line[UI_FILES_ACTION_MOVE], "SD Card isn't ready. Choose storage with R.");
	sd.mount = UI_FILES_FAILED;
	e.pane = UI_FILES_RIGHT;
	UIFiles_Availability(&sd, &gcl, &e, &a);
	CHECK(a.enabled[UI_FILES_ACTION_COPY]);
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "SD Card isn't ready. Choose storage with L.");
	e.pane = UI_FILES_LEFT;
	sd.mount = UI_FILES_OWN;
	sd.readOk = false;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK(!a.enabled[0] && !a.enabled[1]);
	sd.readOk = true;

	/* The same folder on the same device; another folder there is a
	 * rename for Move, which needs no room. */
	{
		uiFilesSide_t same = gcl;

		UIFiles_Availability(&gcl, &same, &e, &a);
		CHECK(!a.enabled[0] && !a.enabled[1]);
		CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "It's already in this folder.");
		CHECK_TEXT(a.line[UI_FILES_ACTION_MOVE], "It's already in this folder.");
		same.folder = "gcl:/backups";
		same.freeBytes = 10u;
		UIFiles_Availability(&gcl, &same, &e, &a);
		CHECK(!a.enabled[UI_FILES_ACTION_COPY] && a.enabled[UI_FILES_ACTION_MOVE]);
		CHECK_TEXT(a.line[UI_FILES_ACTION_MOVE], "");
		/* Without rename, Swiss copies then deletes: it needs the room. */
		gcl.device.canRename = false;
		same.device.canRename = false;
		UIFiles_Availability(&gcl, &same, &e, &a);
		CHECK(!a.enabled[UI_FILES_ACTION_MOVE]);
		gcl.device.canRename = true;
		same.device.canRename = true;
		/* Only replacing makes room for Copy; Move renames, so Keep both
		 * stays open to it. */
		same.freeBytes = 1000000000u;
		e.exists = true;
		e.existingSize = 1000000000u;
		UIFiles_Availability(&gcl, &same, &e, &a);
		CHECK(a.enabled[UI_FILES_ACTION_COPY] && a.replaceOnly[UI_FILES_ACTION_COPY]);
		CHECK(a.enabled[UI_FILES_ACTION_MOVE] && !a.replaceOnly[UI_FILES_ACTION_MOVE]);
		CHECK_TEXT(a.line[UI_FILES_ACTION_MOVE], "");
		e.exists = false;
		e.existingSize = 0u;
		/* The same folder but not ready: the fix comes first. */
		same.folder = gcl.folder;
		same.readOk = false;
		UIFiles_Availability(&gcl, &same, &e, &a);
		CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "GC Loader isn't ready. Choose storage with R.");
		CHECK_TEXT(a.line[UI_FILES_ACTION_MOVE], "GC Loader isn't ready. Choose storage with R.");
		same.readOk = true;
		same.mount = UI_FILES_UNMOUNTED;
		UIFiles_Availability(&gcl, &same, &e, &a);
		CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "GC Loader isn't ready. Choose storage with R.");
	}
	/* Onto a memory card: Memory Cards copies saves; off one is fine. */
	sd.device.card = true;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK(!a.enabled[0] && !a.enabled[1] && a.enabled[2] && a.warn[0]);
	CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "Use Memory Cards to copy saves.");
	CHECK_TEXT(a.line[UI_FILES_ACTION_MOVE], "Use Memory Cards to copy saves.");
	UIFiles_Availability(&sd, &gcl, &e, &a);
	CHECK(a.enabled[0] && a.enabled[1]);
	sd.device.card = false;
	/* A folder there with the file's name: neither, room or not. */
	e.exists = true;
	e.existsFolder = true;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK(!a.enabled[0] && !a.enabled[1] && a.warn[1]);
	CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "A folder there has the same name.");
	e.exists = false;
	e.existsFolder = false;
	/* Exactly the room it needs: it fits. */
	sd.freeBytes = e.needed;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK(a.enabled[UI_FILES_ACTION_COPY] && a.enabled[UI_FILES_ACTION_MOVE]);
	CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "Needs 1.35 GB. It fits.");
	sd.freeBytes = 29100000000u;
	/* Game Disc both sides, as in the emulator: same folder first. */
	{
		uiFilesSide_t disc = side(4, "Game Disc", "dvd:/", false);
		uiFilesSide_t disc2 = disc;

		disc.mount = UI_FILES_SHARED;
		disc2.mount = UI_FILES_SHARED;
		UIFiles_Availability(&disc, &disc2, &e, &a);
		CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "It's already in this folder.");
		CHECK_TEXT(a.line[UI_FILES_ACTION_RENAME], "Game Disc can't rename files.");
		CHECK_TEXT(a.line[UI_FILES_ACTION_HIDE], "Game Disc can't hide files.");
		CHECK_TEXT(a.line[UI_FILES_ACTION_DELETE], "Game Disc can't delete files.");
		for(i = 0; i < UI_FILES_ACTIONS; ++i) CHECK(!a.enabled[i] && a.warn[i]);
		/* Off a read-only disc: Copy yes, Move no. */
		UIFiles_Availability(&disc, &sd, &e, &a);
		CHECK(a.enabled[UI_FILES_ACTION_COPY] && !a.enabled[UI_FILES_ACTION_MOVE]);
		CHECK_TEXT(a.line[UI_FILES_ACTION_MOVE],
			"Game Disc is read-only, so it can't be moved off it.");
		/* Onto it: neither. */
		UIFiles_Availability(&sd, &disc, &e, &a);
		CHECK(!a.enabled[UI_FILES_ACTION_COPY] && !a.enabled[UI_FILES_ACTION_MOVE]);
		CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "Game Disc is read-only.");
		CHECK_TEXT(a.line[UI_FILES_ACTION_MOVE], "Game Disc is read-only.");
	}
	/* Room: greyed with both amounts in one unit. */
	sd.freeBytes = 800000000u;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK(!a.enabled[0] && !a.enabled[1] && a.enabled[2]);
	CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "SD Card has 0.80 GB free; this needs 1.35 GB.");
	CHECK_TEXT(a.line[UI_FILES_ACTION_MOVE], "SD Card has 0.80 GB free; this needs 1.35 GB.");
	/* A same-named copy there only counts when replacing makes room. */
	e.exists = true;
	e.existingSize = 500000000u;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK(!a.enabled[0] && !a.replaceOnly[0]);
	e.existingSize = 550000000u;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK(a.enabled[0] && a.enabled[1] && a.replaceOnly[0] && a.replaceOnly[1] && a.warn[0]);
	CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "It fits only by replacing the copy already there.");
	/* It fits without replacing: not replace-only. */
	sd.freeBytes = 2000000000u;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK(a.enabled[0] && !a.replaceOnly[0] && !a.replaceOnly[1]);
	/* Unknown free space never greys. */
	sd.freeKnown = false;
	sd.freeBytes = 0u;
	e.exists = false;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK(a.enabled[0] && a.enabled[1] && a.warn[0]);
	CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "Free space on SD Card is unknown.");
	sd.freeKnown = true;
	/* Small amounts keep three figures in one unit. */
	sd.freeBytes = 4000u;
	e.needed = 24000u;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "SD Card has 4.0 kB free; this needs 24.0 kB.");
	sd.freeBytes = 10u;
	e.needed = 700u;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK_TEXT(a.line[UI_FILES_ACTION_COPY], "SD Card has 10 bytes free; this needs 700 bytes.");
	sd.freeBytes = 29100000000u;
	e.needed = 1350000000u;
	/* A source missing one operation greys that one alone. */
	gcl.device.canHide = false;
	UIFiles_Availability(&gcl, &sd, &e, &a);
	CHECK(a.enabled[0] && a.enabled[1] && a.enabled[2] && !a.enabled[3] && a.enabled[4]);
	CHECK_TEXT(a.line[UI_FILES_ACTION_HIDE], "GC Loader can't hide files.");
	gcl.device.canHide = true;

	/* Against manage_file's own flags, over every combination. */
	{
		int bits;

		for(bits = 0; bits < (1 << 12); ++bits) {
			uiFilesSide_t from = side(3, "A", "a:/x", (bits & 1) != 0);
			uiFilesSide_t to = (bits & 2048) != 0 ? from : side(0, "B", "b:/y", (bits & 2) != 0);
			uiFilesEntry_t entry = file((bits & 4) != 0 ? 5000u : 50u);
			bool canWrite, isFile, canMove, canCopy, canDelete, canRename, canHide;

			from.device.canRename = (bits & 8) != 0;
			from.device.canHide = (bits & 16) != 0;
			from.device.canDelete = (bits & 32) != 0;
			entry.isFile = (bits & 64) != 0;
			to.mount = (uint8_t)((bits >> 7) & 3);
			to.readOk = (bits & 512) != 0;
			to.freeKnown = (bits & 1024) != 0;
			to.freeBytes = 1000u;
			if((bits & 2048) != 0) {
				to.folder = "a:/z";
				to.mount = (uint8_t)((bits >> 7) & 3);
				to.readOk = (bits & 512) != 0;
			}
			UIFiles_Availability(&from, &to, &entry, &a);
			canWrite = from.device.canWrite;
			isFile = entry.isFile;
			canMove = canWrite && isFile;
			canCopy = isFile;
			canDelete = canWrite && from.device.canDelete;
			canRename = canWrite && from.device.canRename;
			canHide = canWrite && from.device.canHide;
			CHECK(!a.enabled[UI_FILES_ACTION_COPY] || canCopy);
			CHECK(!a.enabled[UI_FILES_ACTION_MOVE] || canMove);
			CHECK(a.enabled[UI_FILES_ACTION_RENAME] == canRename);
			CHECK(a.enabled[UI_FILES_ACTION_HIDE] == canHide);
			CHECK(a.enabled[UI_FILES_ACTION_DELETE] == canDelete);
			/* A Move ends with the original gone: a rename or a delete. */
			CHECK(!a.enabled[UI_FILES_ACTION_MOVE] || canDelete ||
				((bits & 2048) != 0 && canRename));
			for(i = 0; i < UI_FILES_ACTIONS; ++i) {
				/* Every greyed item says why, in amber. */
				CHECK(a.enabled[i] || (a.line[i][0] != '\0' && a.warn[i]));
			}
			/* Unknown free space never greys; known greys only short. */
			if(!to.freeKnown || entry.needed <= to.freeBytes) {
				bool ready = (to.mount == UI_FILES_SHARED || to.mount == UI_FILES_OWN) &&
					to.readOk;

				CHECK(a.enabled[UI_FILES_ACTION_COPY] ==
					(isFile && ready && to.device.canWrite));
			}
		}
	}
}

static void testExistsChoices(void)
{
	uiFilesChoices_t c;

	UIFiles_ExistsChoices(true, &c);
	CHECK_TEXT(c.item[0], "Keep both");
	CHECK_TEXT(c.item[1], "Replace it");
	CHECK_TEXT(c.item[2], "Cancel");
	CHECK(c.dim == 0u && c.focus == 0);
	CHECK_TEXT(c.reason, "");
	UIFiles_ExistsChoices(false, &c);
	CHECK(c.dim == 1u && c.focus == 1);
	CHECK_TEXT(c.reason, "There isn't room for both.");
}

static void testMenuBox(void)
{
	uiFilesLayout_t layouts[2];
	int shape, pane, items, titled, row;

	UIFiles_Layout(0.0f, 640.0f, &layouts[0]);
	UIFiles_Layout(-106.5f, 746.5f, &layouts[1]);
	for(shape = 0; shape < 2; ++shape) {
		for(pane = 0; pane < 2; ++pane) {
			for(items = 2; items <= UI_FILES_MENU_ITEMS(false); ++items) {
				for(titled = 0; titled < 2; ++titled) {
					if(items > UI_FILES_MENU_ITEMS(titled != 0)) continue;
					for(row = 0; row < UI_FILES_ROWS; ++row) {
						const uiFilesLayout_t *l = &layouts[shape];
						uiFilesRect_t r = UIFiles_RowRect(l, pane, row);
						uiFilesBox_t box;
						int title = titled ? UI_FILES_MENU_TITLE + 4 : 0;
						int total = title + 16 + 24 * items;

						UIFiles_MenuBox(l, &r, pane, items, titled != 0, 0, &box);
						CHECK(box.width == 124);
						CHECK(box.height == 16 + 24 * items);
						/* Over its own pane's size column, never the other. */
						CHECK(box.x + box.width == l->pane[pane].x1 - 6);
						CHECK(box.x > l->pane[pane].x0);
						CHECK(box.y - box.titleY == title);
						/* Every box a caller may ask for stays in the window. */
						CHECK(total <= UI_FILES_BOX_BOTTOM - UI_FILES_BOX_TOP);
						CHECK(box.titleY >= UI_FILES_BOX_TOP);
						CHECK(box.y + box.height <= UI_FILES_BOX_BOTTOM);
						/* Level with the row unless it would leave the window. */
						if(r.y0 >= UI_FILES_BOX_TOP && r.y0 + total <= UI_FILES_BOX_BOTTOM) {
							CHECK(box.titleY == r.y0);
						}
					}
				}
			}
		}
	}
	/* The 4:3 Actions box on the left sits at x 306, on the right at 594. */
	{
		uiFilesRect_t r = UIFiles_RowRect(&layouts[0], 0, 2);
		uiFilesBox_t box;

		UIFiles_MenuBox(&layouts[0], &r, 0, 5, false, 0, &box);
		CHECK(box.x == 182 && box.y == 170 && box.titleY == 170);
		r = UIFiles_RowRect(&layouts[0], 1, 7);
		UIFiles_MenuBox(&layouts[0], &r, 1, 5, false, 0, &box);
		CHECK(box.x + box.width == 594 && box.y == 336 - 136);
		/* A question's title grows the box; nothing narrows it. */
		UIFiles_MenuBox(&layouts[0], &r, 1, 2, true, 180, &box);
		CHECK(box.width == 180 && box.x == 414);
		UIFiles_MenuBox(&layouts[0], &r, 1, 2, true, 80, &box);
		CHECK(box.width == 124);
		UIFiles_MenuBox(&layouts[0], &r, 1, 2, true, 124, &box);
		CHECK(box.width == 124);
		/* A box asked wider than its pane stays in it. */
		for(shape = 0; shape < 2; ++shape) {
			for(pane = 0; pane < 2; ++pane) {
				const uiFilesRect_t *edge = &layouts[shape].pane[pane];

				r = UIFiles_RowRect(&layouts[shape], pane, 3);
				UIFiles_MenuBox(&layouts[shape], &r, pane, 3, true, 900, &box);
				CHECK(box.width == edge->x1 - edge->x0 - 12);
				CHECK(box.x == edge->x0 + 6 && box.x + box.width == edge->x1 - 6);
			}
		}
		/* A storage menu: its devices and "Other devices..." under a title. */
		CHECK(UI_FILES_STORAGE_DEVICES + 1 <= UI_FILES_MENU_ITEMS(true));
		/* A storage menu opening below its button. */
		r.y0 = 59;
		UIFiles_MenuBox(&layouts[0], &r, 0, 5, true, 0, &box);
		CHECK(box.titleY == 112 && box.y == 146);
	}
}

/* ------------------------------------------------------------------------
 * Landing, against a straight scan of a sorted listing.
 * --------------------------------------------------------------------- */
typedef struct {
	const char *name;
	int type;
} entry_t;

static int lowerOf(int c)
{
	return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static int oracleCompare(const entry_t *a, const entry_t *b)
{
	const unsigned char *x = (const unsigned char *)a->name, *y = (const unsigned char *)b->name;

	if(a->type != b->type) return b->type - a->type;
	while(*x != '\0' && lowerOf(*x) == lowerOf(*y)) {
		++x;
		++y;
	}
	return lowerOf(*x) - lowerOf(*y);
}

static int qsortCompare(const void *a, const void *b)
{
	return oracleCompare((const entry_t *)a, (const entry_t *)b);
}

static void entryAt(const void *context, int index, const char **name, int *type)
{
	const entry_t *entries = (const entry_t *)context;

	*name = entries[index].name;
	*type = entries[index].type;
}

static void testLanding(void)
{
	entry_t listing[] = {
		{"sd:/backups/..", 3}, {"sd:/backups/Old saves", 2}, {"sd:/backups/zeta", 2},
		{"sd:/backups/Moonlit Lake.iso", 1}, {"sd:/backups/swiss_r2400.dol", 1},
		{"sd:/backups/flippy-update.fpkg", 1}, {"sd:/backups/notes.txt", 1},
		{"sd:/backups/B.iso", 1}, {"sd:/backups/b.iso", 1}, {"sd:/backups/_x", 1},
		{"sd:/backups/[GCOE01].iso", 1}, {"sd:/backups/Copper Orchard [GCOE01].iso", 1},
	};
	const int count = (int)(sizeof(listing) / sizeof(listing[0]));
	static const entry_t probes[] = {
		{"sd:/backups/Copper Orchard [GCOE01].iso", 1}, {"sd:/backups/aaa.iso", 1},
		{"sd:/backups/ZZZ.iso", 1}, {"sd:/backups/~", 1}, {"sd:/backups/0.iso", 1},
		{"sd:/backups/New folder", 2}, {"sd:/backups/b.ISO", 1}, {"sd:/backups/", 1},
		{"sd:/backups/notes.txt", 1}, {"sd:/backups/!", 2},
	};
	size_t p;
	int n;

	qsort(listing, (size_t)count, sizeof(listing[0]), qsortCompare);
	for(n = 0; n <= count; ++n) {
		for(p = 0u; p < sizeof(probes) / sizeof(probes[0]); ++p) {
			int want = 0;

			while(want < n && oracleCompare(&listing[want], &probes[p]) < 0) ++want;
			CHECK(UIFiles_LandingIndex(listing, n, entryAt, probes[p].name, probes[p].type) == want);
		}
	}
	/* A file lands after "..", the folders, and before a larger name. */
	CHECK(UIFiles_LandingIndex(listing, count, entryAt, "sd:/backups/aaa.iso", 1) >= 3);
	CHECK(UIFiles_LandingIndex(NULL, 0, entryAt, "x", 1) == 0);
}

/* ------------------------------------------------------------------------
 * Focus after an action, through a re-read and the reducer.
 * --------------------------------------------------------------------- */

/* Finds name in a listing as scanFiles does; -1 when it's gone. */
static int findName(const char *const *names, int count, const char *name)
{
	int i;

	for(i = 0; i < count; ++i) {
		if(strcmp(names[i], name) == 0) return i;
	}
	return -1;
}

static void testFocusAfter(void)
{
	static const char *const before[] = {"..", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j"};
	const int count = 11;
	uiFilesFocusAfter_t f;
	uiFilesState_t s;
	int focus;

	/* Copy: the same entry here, the new one there, flashing. */
	UIFiles_FocusAfter(UI_FILES_ACTION_COPY, true, 4, count, false, false, &f);
	CHECK(f.sourceIndex == 4 && f.otherToNew && f.flash);
	/* Move and Delete: the next row, or the one before at the end. */
	UIFiles_FocusAfter(UI_FILES_ACTION_MOVE, true, 4, count, false, false, &f);
	CHECK(f.sourceIndex == 5 && f.otherToNew && !f.flash);
	UIFiles_FocusAfter(UI_FILES_ACTION_DELETE, true, 10, count, false, false, &f);
	CHECK(f.sourceIndex == 9 && !f.otherToNew);
	UIFiles_FocusAfter(UI_FILES_ACTION_DELETE, true, 1, 2, false, false, &f);
	CHECK(f.sourceIndex == 0);
	UIFiles_FocusAfter(UI_FILES_ACTION_DELETE, true, 0, 1, false, false, &f);
	CHECK(f.sourceIndex == -1);
	/* Rename: the new name. */
	UIFiles_FocusAfter(UI_FILES_ACTION_RENAME, true, 3, count, false, false, &f);
	CHECK(f.sourceIndex == UI_FILES_FOCUS_NEW_NAME && !f.otherToNew);
	/* Hide: the same entry while hidden files show, else its neighbour;
	 * Unhide keeps it. */
	UIFiles_FocusAfter(UI_FILES_ACTION_HIDE, true, 3, count, true, false, &f);
	CHECK(f.sourceIndex == 3);
	UIFiles_FocusAfter(UI_FILES_ACTION_HIDE, true, 3, count, false, false, &f);
	CHECK(f.sourceIndex == 4);
	UIFiles_FocusAfter(UI_FILES_ACTION_HIDE, true, 10, count, false, false, &f);
	CHECK(f.sourceIndex == 9);
	UIFiles_FocusAfter(UI_FILES_ACTION_HIDE, true, 3, count, true, true, &f);
	CHECK(f.sourceIndex == 3);
	/* Stopped or failed: nothing moves. */
	for(focus = 0; focus < UI_FILES_ACTIONS; ++focus) {
		UIFiles_FocusAfter((uiFilesAction_t)focus, false, 6, count, false, false, &f);
		CHECK(f.sourceIndex == 6 && !f.otherToNew && !f.flash);
	}

	/* Delete at every row, the listing read again, the reducer re-selecting
	 * by name: the neighbour is focused and the window is sound. */
	for(focus = 1; focus < count; ++focus) {
		const char *after[11];
		int n = 0, i, found;

		UIFiles_Init(&s);
		UIFiles_SetPane(&s, UI_FILES_LEFT, count, focus);
		UIFiles_FocusAfter(UI_FILES_ACTION_DELETE, true, s.pane[0].focus, count, false, false, &f);
		for(i = 0; i < count; ++i) {
			if(i != focus) after[n++] = before[i];
		}
		found = f.sourceIndex >= 0 ? findName(after, n, before[f.sourceIndex]) : -1;
		CHECK(found >= 0);
		UIFiles_SetPane(&s, UI_FILES_LEFT, n, found);
		CHECK(s.pane[0].focus == (focus == count - 1 ? n - 1 : focus));
		checkWindow(&s.pane[0], s.pane[0].first);
	}
	/* Rename at the last row: the new name sorts first; focus follows it. */
	{
		const char *after[] = {"..", "0-renamed", "a", "b", "c", "d", "e", "f", "g", "h", "i"};

		UIFiles_Init(&s);
		UIFiles_SetPane(&s, UI_FILES_LEFT, count, count - 1);
		CHECK(s.pane[0].first == 3);
		UIFiles_FocusAfter(UI_FILES_ACTION_RENAME, true, count - 1, count, false, false, &f);
		CHECK(f.sourceIndex == UI_FILES_FOCUS_NEW_NAME);
		UIFiles_SetPane(&s, UI_FILES_LEFT, count, findName(after, count, "0-renamed"));
		CHECK(s.pane[0].focus == 1 && s.pane[0].first == 0);
	}
	/* Deleting the last entry of the right pane leaves ".." focused there,
	 * and the left window is untouched. */
	{
		int start, end;

		UIFiles_Init(&s);
		UIFiles_SetPane(&s, UI_FILES_LEFT, 30, 20);
		UIFiles_SetPane(&s, UI_FILES_RIGHT, 2, 1);
		UIFiles_Input(&s, UI_FILES_INPUT_RIGHT);
		UIFiles_FocusAfter(UI_FILES_ACTION_DELETE, true, 1, 2, false, false, &f);
		UIFiles_SetPane(&s, UI_FILES_RIGHT, 1, f.sourceIndex);
		CHECK(s.pane[1].focus == 0 && s.active == UI_FILES_RIGHT);
		UIFiles_LeftView(&s, &start, &end);
		CHECK(start == 14 && end == 22);
	}
}

/* ------------------------------------------------------------------------
 * Swiss's questions.
 * --------------------------------------------------------------------- */
static void checkQuestion(const char *text, const char *title, const char *detail,
	const char *verb)
{
	uiFilesQuestion_t q;

	CHECK(UIFiles_ParseQuestion(text, &q));
	CHECK_TEXT(q.title, title);
	CHECK_TEXT(q.detail, detail);
	CHECK_TEXT(q.verb, verb);
	CHECK_TEXT(q.cancel, "Cancel");
}

static void testQuestions(void)
{
	uiFilesQuestion_t q;

	/* swiss.c's confirmAction texts, byte for byte. */
	checkQuestion("Move this file?\nIt is removed from here once copied.\nA  MOVE    B  CANCEL",
		"Move this file?", "It is removed from here once copied.", "Move");
	checkQuestion("Hide this file?\nIt shows only with Show hidden files on.\nA  HIDE    B  CANCEL",
		"Hide this file?", "It shows only with Show hidden files on.", "Hide");
	checkQuestion("Hide this folder?\nIt shows only with Show hidden files on.\nA  HIDE    B  CANCEL",
		"Hide this folder?", "It shows only with Show hidden files on.", "Hide");
	checkQuestion("Open this folder at every start?\nHome is skipped until you turn it off.\n"
		"A  AUTOLOAD    B  CANCEL", "Open this folder at every start?",
		"Home is skipped until you turn it off.", "Autoload");
	checkQuestion("Open this game at every start?\nHome is skipped until you turn it off.\n"
		"A  AUTOLOAD    B  CANCEL", "Open this game at every start?",
		"Home is skipped until you turn it off.", "Autoload");
	checkQuestion("Update the FlippyDrive with this file?\nA  UPDATE    B  CANCEL",
		"Update the FlippyDrive with this file?", "", "Update");
	checkQuestion("Write this file to the WiiKey's flash?\nA  FLASH    B  CANCEL",
		"Write this file to the WiiKey's flash?", "", "Flash");
	/* More lines join on line 2; a verb may be two words. */
	checkQuestion("Title\nOne.\nTwo.\nA  TURN OFF    B  CANCEL  ", "Title", "One. Two.",
		"Turn off");
	CHECK(UIFiles_ParseQuestion("T\nA  GO   B  STOP", &q) && strcmp(q.cancel, "Stop") == 0);

	/* Delete's chord and anything else stay Swiss's boxes. */
	CHECK(!UIFiles_ParseQuestion("Delete this file?\n \nPress L + A to continue, or B to cancel.", &q));
	CHECK(q.title[0] == '\0' && q.verb[0] == '\0');
	CHECK(!UIFiles_ParseQuestion("No hint at all", &q));
	CHECK(!UIFiles_ParseQuestion("Title\nA  MOVE", &q));
	CHECK(!UIFiles_ParseQuestion("Title\nA  MOVE    X  CANCEL", &q));
	CHECK(!UIFiles_ParseQuestion("Title\nA     B  CANCEL", &q));
	CHECK(!UIFiles_ParseQuestion("Title\nA  MOVE    B  ", &q));
	CHECK(!UIFiles_ParseQuestion(NULL, &q));
	CHECK(!UIFiles_ParseQuestion("x", NULL));
}

/* manage_file_ex's permissions, its own lines (audit_files_contract.py
 * holds them to swiss.c's), on a made-up Source and entry: Swiss's device
 * slots, its entry and its constants as far as those lines read them. */
enum { IS_FILE = 2, IS_DIR = 1, ATTRIB_HIDDEN = 0x02, FEAT_WRITE = 0x200 };
enum { DEVICE_CUR = 0 };
typedef struct {
	int features;
	void (*deleteFile)(void), (*renameFile)(void), (*hideFile)(void);
} fakeDevice_t;
typedef struct {
	int fileType, fileAttrib;
} fakeFile_t;
typedef struct {
	bool canMove, canCopy, canDelete, canRename, canHide;
} manageFlags_t;

static void fakeOperation(void) {}

static manageFlags_t manageFlags(fakeDevice_t *source, fakeFile_t entry)
{
	fakeDevice_t *devices[1] = {source};
	fakeFile_t curFile = entry;
	manageFlags_t out;

	bool isFile = curFile.fileType == IS_FILE;
	bool isHidden = curFile.fileAttrib & ATTRIB_HIDDEN;
	bool canWrite = devices[DEVICE_CUR]->features & FEAT_WRITE;
	bool canMove = canWrite && isFile;
	bool canCopy = isFile;
	bool canDelete = canWrite && devices[DEVICE_CUR]->deleteFile;
	bool canRename = canWrite && devices[DEVICE_CUR]->renameFile;
	bool canHide = canWrite && devices[DEVICE_CUR]->hideFile;
	(void)isHidden;
	out.canMove = canMove;
	out.canCopy = canCopy;
	out.canDelete = canDelete;
	out.canRename = canRename;
	out.canHide = canHide;
	return out;
}

/* What the Actions box allows is what manage_file_ex does, for every
 * device and entry: Rename, Hide and Delete exactly its permissions; Copy
 * and Move only where it allows them, and a Move only where Swiss either
 * renames it across the same device or deletes the original after copying,
 * so a Move never ends as a copy. */
static void testAvailabilityTable(void)
{
	int bits;

	for(bits = 0; bits < 1 << 9; ++bits) {
		bool write = bits & 1, del = bits & 2, ren = bits & 4, hide = bits & 8;
		bool isFile = bits & 16, sameDevice = bits & 32, otherWrite = bits & 64;
		bool program = bits & 128, hidden = bits & 256;
		fakeDevice_t source = {write ? FEAT_WRITE : 0, del ? fakeOperation : NULL,
			ren ? fakeOperation : NULL, hide ? fakeOperation : NULL};
		fakeFile_t entry = {isFile ? IS_FILE : IS_DIR, hidden ? ATTRIB_HIDDEN : 0};
		manageFlags_t swiss = manageFlags(&source, entry);
		uiFilesSide_t from = side(1, "SD Card", "sd:/games", write);
		uiFilesSide_t to = side(sameDevice ? 1 : 2, "Other", "other:/backups", otherWrite);
		uiFilesEntry_t e = file(1000u);
		uiFilesAvailability_t a;

		from.device.canRename = ren;
		from.device.canHide = hide;
		from.device.canDelete = del;
		from.mount = UI_FILES_SHARED;
		if(sameDevice) {
			to.device = from.device;
			to.device.canWrite = write;
		}
		e.isFile = isFile;
		e.programFolder = program && isFile;
		e.hidden = hidden;
		UIFiles_Availability(&from, &to, &e, &a);
		CHECK(a.enabled[UI_FILES_ACTION_RENAME] == swiss.canRename);
		CHECK(a.enabled[UI_FILES_ACTION_HIDE] == swiss.canHide);
		CHECK(a.enabled[UI_FILES_ACTION_DELETE] == swiss.canDelete);
		if(a.enabled[UI_FILES_ACTION_COPY]) {
			CHECK(swiss.canCopy && !e.programFolder);
		}
		if(a.enabled[UI_FILES_ACTION_MOVE]) {
			CHECK(swiss.canMove && !e.programFolder);
			CHECK((sameDevice && swiss.canRename) || swiss.canDelete);
		}
		/* A file Swiss could copy is greyed only for the other side. */
		if(swiss.canCopy && !e.programFolder && (sameDevice ? write : otherWrite)) {
			CHECK(a.enabled[UI_FILES_ACTION_COPY]);
		}
	}
}

/* Line 1 under each action, and how a copy ended in the message. */
static void testActionWords(void)
{
	char line[UI_FILES_TEXT_CAPACITY], out[2][UI_FILES_TEXT_CAPACITY];

	UIFiles_ActionLine(UI_FILES_ACTION_COPY, false, "GC Loader", "SD Card", "/backups", line,
		sizeof(line));
	CHECK_TEXT(line, "Copy puts a copy in SD Card  \233  /backups. It stays here.");
	UIFiles_ActionLine(UI_FILES_ACTION_MOVE, false, "GC Loader", "SD Card", "", line,
		sizeof(line));
	CHECK_TEXT(line, "Move puts it in SD Card  \233  / and takes it off GC Loader.");
	UIFiles_ActionLine(UI_FILES_ACTION_RENAME, false, "GC Loader", "SD Card", "/", line,
		sizeof(line));
	CHECK_TEXT(line, "Rename gives it a new name.");
	UIFiles_ActionLine(UI_FILES_ACTION_HIDE, true, "GC Loader", "SD Card", "/", line,
		sizeof(line));
	CHECK_TEXT(line, "Unhide shows it again.");
	UIFiles_ActionLine(UI_FILES_ACTION_HIDE, false, "GC Loader", "SD Card", "/", line,
		sizeof(line));
	CHECK_TEXT(line, "Hide keeps it out of sight until Show hidden files is on.");
	UIFiles_ActionLine(UI_FILES_ACTION_DELETE, false, NULL, "SD Card", "/", line, sizeof(line));
	CHECK_TEXT(line, "Delete takes it off This storage for good.");

	UIFiles_Result(UI_FILES_RESULT_DONE, false, "Copper Orchard", "GC Loader", "SD Card",
		"backups", 0, false, false, out);
	CHECK_TEXT(out[0], "Finished copying.");
	CHECK_TEXT(out[1], "");
	UIFiles_Result(UI_FILES_RESULT_DONE, true, "Copper Orchard", "GC Loader", "SD Card",
		"backups", 0, false, false, out);
	CHECK_TEXT(out[0], "Finished moving.");
	UIFiles_Result(UI_FILES_RESULT_STOPPED, false, "Copper Orchard", "GC Loader", "SD Card",
		"backups", 0, true, false, out);
	CHECK_TEXT(out[0], "Copying was stopped.");
	CHECK_TEXT(out[1], "The unfinished copy was removed from SD Card.");
	UIFiles_Result(UI_FILES_RESULT_STOPPED, true, "Copper Orchard", "GC Loader", "SD Card",
		"backups", 0, false, false, out);
	CHECK_TEXT(out[0], "Moving was stopped.");
	CHECK_TEXT(out[1], "Part of it is left in backups.");
	UIFiles_Result(UI_FILES_RESULT_WRITE_FAILED, true, "Copper Orchard", "GC Loader", "SD Card",
		"", -5, true, false, out);
	CHECK_TEXT(out[0], "Couldn't write to SD Card. (-5)");
	CHECK_TEXT(out[1], "The unfinished copy was removed. Nothing was moved.");
	UIFiles_Result(UI_FILES_RESULT_READ_FAILED, false, "Copper Orchard", "GC Loader", "SD Card",
		"", 3, false, false, out);
	CHECK_TEXT(out[0], "Couldn't read Copper Orchard. (3)");
	CHECK_TEXT(out[1], "Part of it is left in SD Card.");
	UIFiles_Result(UI_FILES_RESULT_KEPT, true, "Copper Orchard", "GC Loader", "SD Card",
		"backups", 0, false, false, out);
	CHECK_TEXT(out[0], "Copied, but couldn't take it off GC Loader.");
	CHECK_TEXT(out[1], "It is in both places now.");
	/* Replace it took the file there off first: the message says so. */
	UIFiles_Result(UI_FILES_RESULT_STOPPED, false, "Copper Orchard", "GC Loader", "SD Card",
		"backups", 0, true, true, out);
	CHECK_TEXT(out[1], "The unfinished copy and the file it replaced are gone.");
	UIFiles_Result(UI_FILES_RESULT_STOPPED, false, "Copper Orchard", "GC Loader", "SD Card",
		"backups", 0, false, true, out);
	CHECK_TEXT(out[1], "Part of it is left in backups; the old file is gone.");
	UIFiles_Result(UI_FILES_RESULT_WRITE_FAILED, true, "Copper Orchard", "GC Loader", "SD Card",
		"", -5, true, true, out);
	CHECK_TEXT(out[1], "The unfinished copy and the file it replaced are gone. Nothing was moved.");
	UIFiles_Result(UI_FILES_RESULT_DONE, false, "Copper Orchard", "GC Loader", "SD Card",
		"backups", 0, false, true, out);
	CHECK_TEXT(out[0], "Finished copying.");
	CHECK_TEXT(out[1], "");
}

/* The ghost row opens where the copy lands in the window: the rows after
 * it move down, the last out of view, the focus with its row; the window
 * holds it even when it lands above or below. */
static void testGhost(void)
{
	uiFilesPaneSnapshot_t pane;
	uiFilesRowSnapshot_t ghost;
	int i;

	memset(&ghost, 0, sizeof(ghost));
	strcpy(ghost.name, "new.iso");
	ghost.flags = UI_FILES_ROW_FOCUS;
	memset(&pane, 0, sizeof(pane));
	pane.rows = 3;
	pane.count = 3;
	pane.focusRow = 1;
	for(i = 0; i < 3; i++) snprintf(pane.row[i].name, sizeof(pane.row[i].name), "%d", i);
	pane.row[1].flags = UI_FILES_ROW_FOCUS;
	UIFiles_InsertGhost(&pane, 1, &ghost);
	CHECK(pane.rows == 4 && pane.count == 4 && pane.focusRow == 2);
	CHECK_TEXT(pane.row[0].name, "0");
	CHECK_TEXT(pane.row[1].name, "new.iso");
	CHECK(pane.row[1].flags == UI_FILES_ROW_GHOST);
	CHECK_TEXT(pane.row[2].name, "1");
	CHECK(pane.row[2].flags == UI_FILES_ROW_FOCUS);
	CHECK_TEXT(pane.row[3].name, "2");

	/* A full window from row 10: before it, it opens the window; after it,
	 * it takes the last row; the focus on the last row falls out. */
	memset(&pane, 0, sizeof(pane));
	pane.rows = UI_FILES_ROWS;
	pane.count = 40;
	pane.first = 10;
	pane.focusRow = UI_FILES_ROWS - 1;
	for(i = 0; i < UI_FILES_ROWS; i++) snprintf(pane.row[i].name, sizeof(pane.row[i].name), "%d", 10 + i);
	UIFiles_InsertGhost(&pane, 2, &ghost);
	CHECK(pane.rows == UI_FILES_ROWS && pane.focusRow == -1);
	CHECK_TEXT(pane.row[0].name, "new.iso");
	CHECK_TEXT(pane.row[1].name, "10");
	CHECK_TEXT(pane.row[UI_FILES_ROWS - 1].name, "16");
	UIFiles_InsertGhost(&pane, 90, &ghost);
	CHECK_TEXT(pane.row[UI_FILES_ROWS - 1].name, "new.iso");
	CHECK_TEXT(pane.row[UI_FILES_ROWS - 2].name, "15");

	/* An empty folder: the ghost is its only row. */
	memset(&pane, 0, sizeof(pane));
	pane.focusRow = -1;
	UIFiles_InsertGhost(&pane, 0, &ghost);
	CHECK(pane.rows == 1 && pane.focusRow == -1 && pane.count == 1);
}

/* A copy's new row flashes as its message comes: two pulses on Full, one on
 * Reduced, none Off, each from nothing to full and back. */
static void testFlash(void)
{
	CHECK(UIFiles_Flash(0.0f, UI_MOTION_FULL) == 0.0f);
	CHECK(UIFiles_Flash(0.15f, UI_MOTION_FULL) > 0.99f);
	CHECK(UIFiles_Flash(0.30f, UI_MOTION_FULL) < 0.01f);
	CHECK(UIFiles_Flash(0.45f, UI_MOTION_FULL) > 0.99f);
	CHECK(UIFiles_Flash(0.60f, UI_MOTION_FULL) == 0.0f);
	CHECK(UIFiles_Flash(5.0f, UI_MOTION_FULL) == 0.0f);
	CHECK(UIFiles_Flash(0.15f, UI_MOTION_REDUCED) > 0.99f);
	CHECK(UIFiles_Flash(0.45f, UI_MOTION_REDUCED) == 0.0f);
	CHECK(UIFiles_Flash(0.15f, UI_MOTION_OFF) == 0.0f);
	CHECK(UIFiles_Flash(-1.0f, UI_MOTION_FULL) == 0.0f);
}

/* The page comes as Memory Cards' does and goes before Home shows: the Home
 * cube going back, the paper, then the words; on B the words first, the
 * paper, and the cube back by the time leaving ends. */
static void testStage(void)
{
	const float leave[3] = {0.45f, 0.2f, 0.0f};
	uiFilesStage_t stage;
	int mode;

	UIFiles_Stage(0.0f, -1.0f, UI_MOTION_FULL, &stage);
	CHECK(stage.handover == 1.0f && stage.paper == 0.0f && stage.chrome == 0.0f);
	UIFiles_Stage(0.2f, -1.0f, UI_MOTION_FULL, &stage);
	CHECK(stage.handover > 0.0f && stage.handover < 1.0f);
	CHECK(stage.paper > 0.0f && stage.chrome == 0.0f);
	UIFiles_Stage(0.4f, -1.0f, UI_MOTION_FULL, &stage);
	CHECK(stage.handover == 0.0f && stage.chrome > 0.0f && stage.chrome < 1.0f);
	UIFiles_Stage(0.6f, -1.0f, UI_MOTION_FULL, &stage);
	CHECK(stage.handover == 0.0f && stage.paper == 1.0f && stage.chrome == 1.0f);
	UIFiles_Stage(0.1f, -1.0f, UI_MOTION_REDUCED, &stage);
	CHECK(stage.handover == 0.0f && stage.paper == stage.chrome &&
		stage.paper > 0.0f && stage.paper < 1.0f);
	UIFiles_Stage(0.0f, -1.0f, UI_MOTION_OFF, &stage);
	CHECK(stage.handover == 0.0f && stage.paper == 1.0f && stage.chrome == 1.0f);
	for(mode = UI_MOTION_FULL; mode <= UI_MOTION_OFF; ++mode) {
		CHECK(UIFiles_LeaveSeconds(mode) == leave[mode]);
		/* Fully open, then all the way out: nothing of the page left. */
		UIFiles_Stage(5.0f, UIFiles_LeaveSeconds(mode), mode, &stage);
		CHECK(stage.paper == 0.0f && stage.chrome == 0.0f);
		if(mode == UI_MOTION_FULL) {
			CHECK(stage.handover == 1.0f);
		}
	}
	/* Full: the words go first, the cube comes back only once they have. */
	UIFiles_Stage(5.0f, 0.12f, UI_MOTION_FULL, &stage);
	CHECK(stage.chrome == 0.0f && stage.paper > 0.0f && stage.handover == 0.0f);
	UIFiles_Stage(5.0f, 0.35f, UI_MOTION_FULL, &stage);
	CHECK(stage.handover > 0.0f && stage.handover < 1.0f);
}

/* Y: the panes' contents cross-fade, out while the sides change places and
 * in once they have, half of 0.15 s each way (0.10 s on Reduced), from
 * wherever they were; with UI Motion Off they stay. */
static void testSwapFade(void)
{
	float content = 1.0f;
	int frames;

	CHECK(UIFiles_SwapStep(1.0f, false, 1.0f / 60.0f, UI_MOTION_FULL) == 1.0f);
	CHECK(UIFiles_SwapStep(1.0f, true, 0.0375f, UI_MOTION_FULL) == 0.5f);
	CHECK(UIFiles_SwapStep(1.0f, true, 0.075f, UI_MOTION_FULL) == 0.0f);
	CHECK(UIFiles_SwapStep(0.0f, true, 1.0f, UI_MOTION_FULL) == 0.0f);
	CHECK(UIFiles_SwapStep(0.0f, false, 0.075f, UI_MOTION_FULL) == 1.0f);
	CHECK(UIFiles_SwapStep(1.0f, true, 0.05f, UI_MOTION_REDUCED) == 0.0f);
	CHECK(UIFiles_SwapStep(0.0f, false, 0.025f, UI_MOTION_REDUCED) == 0.5f);
	CHECK(UIFiles_SwapStep(1.0f, true, 1.0f, UI_MOTION_OFF) == 1.0f);
	CHECK(UIFiles_SwapStep(0.0f, false, 0.0f, UI_MOTION_OFF) == 1.0f);
	/* A clock that stepped back moves nothing. */
	CHECK(UIFiles_SwapStep(0.5f, true, -1.0f, UI_MOTION_FULL) == 0.5f);
	/* The new sides came half way out: back in from there, no jump. */
	content = UIFiles_SwapStep(content, true, 0.03f, UI_MOTION_FULL);
	CHECK(content > 0.5f && content < 0.7f);
	for(frames = 0; content < 1.0f && frames < 60; ++frames) {
		float next = UIFiles_SwapStep(content, false, 1.0f / 60.0f, UI_MOTION_FULL);

		CHECK(next > content && next - content < 0.25f);
		content = next;
	}
	CHECK(content == 1.0f && frames == 2);
}

int main(void)
{
	testLayout();
	testReducerAuthored();
	testReducerExhaustive();
	testKinds();
	testProgramFolders();
	testSizes();
	testFitName();
	testFitPath();
	testFitDevice();
	testHints();
	testStorage();
	testSecondDevice();
	testAvailability();
	testExistsChoices();
	testMenuBox();
	testLanding();
	testFocusAfter();
	testQuestions();
	testAvailabilityTable();
	testActionWords();
	testGhost();
	testFlash();
	testStage();
	testSwapFade();
	printf("test_ui_files: %u checks passed\n", checks);
	return 0;
}
