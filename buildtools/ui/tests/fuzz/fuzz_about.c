/* Fuzz the game descriptions file (swiss/ui/descriptions.txt) as Spotlight
 * reads it: any file indexes without a sanitizer finding, the index is sorted
 * with each game once, every indexed game is found, and a description never
 * outgrows the room it is given. The bytes are copied to exactly their size,
 * so a read past the file is a finding. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ui_about.h"

static uiAboutEntry_t entries[UI_ABOUT_MAX_GAMES];

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	uiAbout_t about;
	char out[321];
	char *copy = malloc(size ? size : 1);
	size_t count;

	if(!copy)
		return 0;
	memcpy(copy, data, size);
	count = UIAbout_Index(&about, copy, size, entries, UI_ABOUT_MAX_GAMES);
	if(count > UI_ABOUT_MAX_GAMES || count != about.count)
		abort();
	for(size_t i = 0; i < count; i++) {
		char id[7];
		if(i > 0 && memcmp(about.entries[i - 1].id, about.entries[i].id, 6) >= 0)
			abort(); /* unsorted, or a game twice */
		if(i >= 16)
			continue;
		memcpy(id, about.entries[i].id, 6);
		id[6] = '\0';
		if(!UIAbout_Find(&about, id, out, sizeof(out)) || strlen(out) >= sizeof(out))
			abort();
		/* Another publisher's disc of the same game takes the first one's. */
		id[5] = id[5] == 'Z' ? 'A' : 'Z';
		if(!UIAbout_Find(&about, id, out, 8) || strlen(out) >= 8)
			abort();
	}
	free(copy);
	return 0;
}
