#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_launch.h"

/* The Swiss sources, seen from buildtools/ui/tests where run_tests.sh runs. */
#ifndef SWISS_SOURCE
#define SWISS_SOURCE "../../../cube/swiss/source"
#endif

static unsigned checks;

static void check(int condition, const char *message)
{
	checks++;
	if(!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		exit(1);
	}
}

static char *readSource(const char *name)
{
	char path[256];
	FILE *file;
	long size = 0;
	char *text;

	snprintf(path, sizeof(path), "%s/%s", SWISS_SOURCE, name);
	file = fopen(path, "rb");
	check(file != NULL, path);
	check(fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) > 0 &&
		fseek(file, 0, SEEK_SET) == 0, path);
	text = malloc((size_t)size + 1u);
	check(text != NULL && fread(text, 1u, (size_t)size, file) ==
		(size_t)size, path);
	text[size] = '\0';
	fclose(file);
	return text;
}

/* A string as its C literal is spelled in a source file. */
static void spell(char *out, size_t size, const char *text)
{
	size_t n = 0u;

	out[n++] = '"';
	for(; *text != '\0' && n + 6u < size; ++text) {
		unsigned char c = (unsigned char)*text;

		if(c == '\n') {
			out[n++] = '\\';
			out[n++] = 'n';
		}
		else if(c >= 0x80u) {
			n += (size_t)snprintf(out + n, size - n, "\\%03o", c);
		}
		else {
			out[n++] = (char)c;
		}
	}
	out[n++] = '"';
	out[n] = '\0';
}

/* What sprintf makes of format, each %i and %s taking the next argument. */
static void format(char *out, size_t size, const char *pattern,
	const char *const *args)
{
	size_t n = 0u;

	while(*pattern != '\0' && n + 1u < size) {
		if(pattern[0] == '%' && (pattern[1] == 'i' || pattern[1] == 's')) {
			const char *arg = *args++;

			while(*arg != '\0' && n + 1u < size) {
				out[n++] = *arg++;
			}
			pattern += 2;
		}
		else {
			out[n++] = *pattern++;
		}
	}
	out[n] = '\0';
}

/* Every message the launch screen recognises, as Swiss writes it. */
static const struct {
	const char *file;
	const char *format;
	const char *args[4];
	uiLaunchStep_t step;
	int index;
	int count;
	const char *warning;
} pins[] = {
	{"swiss.c", "Saving recent list\205", {NULL},
		UI_LAUNCH_STEP_RECENT, 0, 0, ""},
	{"swiss.c", "Video Mode: %s", {"NTSC 480p"},
		UI_LAUNCH_STEP_VIDEO, 0, 0, ""},
	{"swiss.c", "Video Mode: %s\n%s Mode selected.", {"PAL 576i", "50Hz"},
		UI_LAUNCH_STEP_VIDEO, 0, 0, ""},
	{"swiss.c", "Applied %i cheats", {"3"},
		UI_LAUNCH_STEP_CHEATS, 0, 0, ""},
	{"swiss.c", "Checking Game\205", {NULL},
		UI_LAUNCH_STEP_CHECK, 0, 0, ""},
	{"gcm.c", "Reading File %i/%i\n%s [%iKB]", {"2", "3", "main.dol", "3012"},
		UI_LAUNCH_STEP_READ, 2, 3, ""},
	{"gcm.c", "Patching File %i/%i\n%s [%iKB]", {"2", "3", "main.dol", "3012"},
		UI_LAUNCH_STEP_PATCH, 2, 3, ""},
	{"gcm.c", "Writing File %i/%i\n%s [%iKB]", {"2", "3", "main.dol", "3012"},
		UI_LAUNCH_STEP_WRITE, 2, 3, ""},
	{"devices/dvd/deviceHandler-DVD.c",
		"One moment, setting up audio streaming.", {NULL},
		UI_LAUNCH_STEP_AUDIO, 0, 0, ""},
	{"swiss.c", "Loading DOL\nDo not remove %s", {"SD Card - SD2SP2"},
		UI_LAUNCH_STEP_LOAD, 0, 0, "Do not remove SD Card - SD2SP2"},
	{"swiss.c", "Loading DOL", {NULL}, UI_LAUNCH_STEP_LOAD, 0, 0, ""},
	{"swiss.c", "Loading BS2\nDo not remove %s", {"SD Card - SD2SP2"},
		UI_LAUNCH_STEP_LOAD, 0, 0, "Do not remove SD Card - SD2SP2"},
	{"swiss.c", "Loading BS2", {NULL}, UI_LAUNCH_STEP_LOAD, 0, 0, ""}
};

static void checkPins(void)
{
	size_t i;

	for(i = 0u; i < sizeof(pins) / sizeof(pins[0]); ++i) {
		char *source = readSource(pins[i].file);
		char literal[160];
		char message[160];
		int index;
		int count;
		uiLaunch_t launch;

		/* A Swiss merge that rewords a message fails here, before the
		 * launch screen stops recognising it. */
		spell(literal, sizeof(literal), pins[i].format);
		if(strstr(source, literal) == NULL) {
			fprintf(stderr, "%s no longer says %s\n", pins[i].file, literal);
		}
		check(strstr(source, literal) != NULL, "a recognised message is pinned");
		free(source);

		format(message, sizeof(message), pins[i].format, pins[i].args);
		check(UILaunch_Classify(message, &index, &count) == pins[i].step,
			message);
		check(index == pins[i].index && count == pins[i].count,
			"a file message gives its number and count");
		UILaunch_Begin(&launch);
		check(UILaunch_Message(&launch, message), "recognised");
		check(launch.step == pins[i].step, "the launch moves to the step");
		check(strcmp(launch.warning, pins[i].warning) == 0,
			"the Do not remove line, and only it, is kept");
		/* One short line instead of file names and sizes. */
		check(strlen(UILaunch_Caption(launch.step)) > 0u &&
			strlen(UILaunch_Caption(launch.step)) <= 26u &&
			strstr(UILaunch_Caption(launch.step), "main.dol") == NULL &&
			strstr(UILaunch_Caption(launch.step), "KB") == NULL,
			"a short caption");
	}
}

static void checkUnknown(void)
{
	static const char *const others[] = {
		/* Shown before Game Detail, never during a launch. */
		"Reading\205", "Setup base offset please Wait\205",
		"Invalid or Corrupt File!", "", "Load", "Loading"
	};
	uiLaunch_t launch;
	uiLaunch_t before;
	size_t i;
	int index = 7;
	int count = 7;

	UILaunch_Begin(&launch);
	check(UILaunch_Message(&launch, "Checking Game\205"), "known");
	before = launch;
	for(i = 0u; i < sizeof(others) / sizeof(others[0]); ++i) {
		check(UILaunch_Classify(others[i], &index, &count) ==
			UI_LAUNCH_STEP_NONE && index == 0 && count == 0, others[i]);
		check(!UILaunch_Message(&launch, others[i]), others[i]);
		check(memcmp(&launch, &before, sizeof(launch)) == 0,
			"an unknown message changes nothing");
	}
	check(UILaunch_Classify(NULL, NULL, NULL) == UI_LAUNCH_STEP_NONE,
		"no message");
	check(!UILaunch_Message(NULL, "Checking Game\205"), "no launch");
	/* A file message without its numbers is still a file step, at the
	 * start of the files' share. */
	check(UILaunch_Classify("Reading File x/y", &index, &count) ==
		UI_LAUNCH_STEP_READ && index == 0 && count == 0, "no numbers");
	check(UILaunch_Classify("Reading File 4/3", &index, &count) ==
		UI_LAUNCH_STEP_READ && index == 0 && count == 0, "past the count");
	check(UILaunch_Target(UI_LAUNCH_STEP_READ, 0, 0) ==
		UILaunch_Target(UI_LAUNCH_STEP_READ, 1, 5), "files start");
	check(strcmp(UILaunch_Caption(UI_LAUNCH_STEP_NONE), "") == 0 &&
		strcmp(UILaunch_Caption(UI_LAUNCH_STEP_COUNT), "") == 0 &&
		strcmp(UILaunch_Caption((uiLaunchStep_t)99), "") == 0,
		"no caption for no step");
	/* An app from Apps goes through boot_dol's steps only (the start, the
	 * recent list and "Loading DOL"), and each says app. */
	check(strcmp(UILaunch_AppCaption(UI_LAUNCH_STEP_START), "Starting app") == 0 &&
		strcmp(UILaunch_AppCaption(UI_LAUNCH_STEP_RECENT), "Saving recent list") == 0 &&
		strcmp(UILaunch_AppCaption(UI_LAUNCH_STEP_LOAD), "Loading app") == 0,
		"an app's launch says app");
	check(UILaunch_Classify("Loading DOL", NULL, NULL) == UI_LAUNCH_STEP_LOAD &&
		UILaunch_Classify("Saving recent list\205", NULL, NULL) ==
			UI_LAUNCH_STEP_RECENT, "boot_dol's messages are an app's steps");
	check(strcmp(UILaunch_AppCaption(UI_LAUNCH_STEP_COUNT), "") == 0 &&
		strcmp(UILaunch_AppCaption((uiLaunchStep_t)99), "") == 0,
		"no caption for no step");
}

static void checkOrder(void)
{
	static const uiLaunchStep_t order[] = {
		UI_LAUNCH_STEP_START, UI_LAUNCH_STEP_RECENT, UI_LAUNCH_STEP_VIDEO,
		UI_LAUNCH_STEP_CHEATS, UI_LAUNCH_STEP_CHECK
	};
	float last = -1.0f;
	size_t i;
	int count;

	for(i = 0u; i < sizeof(order) / sizeof(order[0]); ++i) {
		check(UILaunch_Target(order[i], 0, 0) > last,
			"each step starts after the one before");
		last = UILaunch_Target(order[i], 0, 0);
	}
	for(count = 1; count <= 512; ++count) {
		float files = last;
		int index;
		int phase;

		for(index = 1; index <= count; ++index) {
			for(phase = 0; phase < 3; ++phase) {
				float target = UILaunch_Target((uiLaunchStep_t)(
					UI_LAUNCH_STEP_READ + phase), index, count);

				check(target > files, "every file and phase moves on");
				files = target;
			}
		}
		check(files < UILaunch_Target(UI_LAUNCH_STEP_AUDIO, 0, 0),
			"the files end before the drive's audio is set up");
	}
	check(UILaunch_Target(UI_LAUNCH_STEP_AUDIO, 0, 0) <
		UILaunch_Target(UI_LAUNCH_STEP_LOAD, 0, 0) &&
		UILaunch_Target(UI_LAUNCH_STEP_LOAD, 0, 0) < 1.0f,
		"loading the game comes last");
}

/* A launch as Swiss runs one, with time passing between the messages. */
static void checkLaunch(void)
{
	static const char *const messages[] = {
		"Saving recent list\205", "Video Mode: NTSC 480p", "Applied 2 cheats",
		"Checking Game\205",
		"Reading File 1/2\nmain.dol [3012KB]",
		"Patching File 1/2\nmain.dol [3012KB]",
		"Writing File 1/2\nmain.dol [3012KB]",
		/* A box shown again, or out of order, never moves the ring back. */
		"Reading File 1/2\nmain.dol [3012KB]",
		"Reading File 2/2\nstage.rel [220KB]",
		"Patching File 2/2\nstage.rel [220KB]",
		"Checking Game\205",
		"Loading DOL\nDo not remove SD Card - SD2SP2",
		"Loading DOL"
	};
	uiLaunch_t launch;
	float fill = 0.0f;
	float target = 0.0f;
	float ceiling;
	size_t i;
	int frame;

	UILaunch_Begin(&launch);
	check(launch.step == UI_LAUNCH_STEP_START && launch.fill == 0.0f &&
		launch.warning[0] == '\0', "a launch starts empty");
	check(strcmp(UILaunch_Caption(launch.step), "Starting game") == 0,
		"and says so");
	for(i = 0u; i < sizeof(messages) / sizeof(messages[0]); ++i) {
		check(UILaunch_Message(&launch, messages[i]), messages[i]);
		check(launch.target >= target, "the target never falls");
		check(launch.ceiling > launch.target, "a step has room to creep");
		target = launch.target;
		for(frame = 0; frame < 12; ++frame) {
			float next = UILaunch_Update(&launch, 1.0f / 60.0f, true);

			check(next >= fill, "the fill never falls");
			check(next <= launch.ceiling, "nor passes the next step");
			fill = next;
		}
	}
	check(launch.step == UI_LAUNCH_STEP_LOAD, "the last step is loading");
	check(strcmp(UILaunch_Caption(launch.step), "Loading game") == 0,
		"loading the game");
	check(strcmp(launch.warning, "Do not remove SD Card - SD2SP2") == 0,
		"the warning stays until the hand-off");
	/* A long load: the ring closes on the step, then creeps on. */
	for(frame = 0; frame < 60; ++frame) {
		UILaunch_Update(&launch, 1.0f / 60.0f, true);
	}
	check(launch.fill > launch.target - 0.01f, "closes on the step in a second");
	ceiling = launch.ceiling;
	fill = launch.fill;
	for(frame = 0; frame < 20 * 60; ++frame) {
		float next = UILaunch_Update(&launch, 1.0f / 60.0f, true);

		check(next > fill, "a long step never stalls");
		fill = next;
	}
	check(fill < ceiling, "nor reaches the next step");
	/* The hand-off: the ring completes inside the quarter-second fade. */
	UILaunch_Finish(&launch);
	for(frame = 0; frame < 15; ++frame) {
		float next = UILaunch_Update(&launch, 1.0f / 60.0f, true);

		check(next >= fill, "the fill never falls");
		fill = next;
	}
	check(fill >= 0.99f && fill <= 1.0f, "complete for the hand-off");
}

static void checkEase(void)
{
	uiLaunch_t launch;
	float fill;
	int frame;

	/* A jump from the start to loading closes within half a second. */
	UILaunch_Begin(&launch);
	check(UILaunch_Message(&launch, "Loading DOL"), "loading");
	for(frame = 0; frame < 30; ++frame) {
		UILaunch_Update(&launch, 1.0f / 60.0f, true);
	}
	check(launch.fill >= launch.target * 0.95f, "half a second");
	/* Without animation the ring steps: no ease and no creep. */
	UILaunch_Begin(&launch);
	check(UILaunch_Message(&launch, "Checking Game\205"), "checking");
	check(UILaunch_Update(&launch, 1.0f / 60.0f, false) == launch.target,
		"steps to the target");
	check(UILaunch_Update(&launch, 10.0f, false) == launch.target,
		"and stays there");
	/* Frames that took no time, ran backwards or were lost move nothing;
	 * a long one moves as far as a twentieth of a second. */
	fill = launch.fill;
	check(UILaunch_Update(&launch, 0.0f, true) == fill &&
		UILaunch_Update(&launch, -1.0f, true) == fill &&
		UILaunch_Update(&launch, NAN, true) == fill, "no time, no move");
	UILaunch_Begin(&launch);
	check(UILaunch_Message(&launch, "Loading DOL"), "loading");
	UILaunch_Update(&launch, 5.0f, true);
	fill = launch.fill;
	UILaunch_Begin(&launch);
	check(UILaunch_Message(&launch, "Loading DOL"), "loading");
	UILaunch_Update(&launch, 0.05f, true);
	check(fill == launch.fill, "a long frame is one twentieth of a second");
	check(UILaunch_Update(NULL, 0.1f, true) == 0.0f, "no launch");
}

static void checkWarning(void)
{
	char message[200];
	uiLaunch_t launch;

	/* A long device name is cut to the line's room. */
	snprintf(message, sizeof(message), "Loading DOL\nDo not remove %s",
		"a device whose name runs on far longer than any real one does, and on");
	UILaunch_Begin(&launch);
	check(UILaunch_Message(&launch, message), "loading");
	check(strlen(launch.warning) == UI_LAUNCH_WARNING_LENGTH - 1u &&
		strncmp(launch.warning, "Do not remove a device", 22u) == 0,
		"cut, and terminated");
	/* Another second line is not a warning. */
	UILaunch_Begin(&launch);
	check(UILaunch_Message(&launch, "Video Mode: PAL 576i\n50Hz Mode selected."),
		"video");
	check(launch.warning[0] == '\0', "not a warning");
}

int main(void)
{
	checkPins();
	checkUnknown();
	checkOrder();
	checkLaunch();
	checkEase();
	checkWarning();
	printf("ui_launch: %u checks passed\n", checks);
	return 0;
}
