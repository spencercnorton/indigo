#include "ui_saves.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* A save entry as a card holds it: game GALE, maker 01, the given name and
 * block count, and recognisable values in the fields Action Replay swaps. */
static void makeEntry(uint8_t entry[UI_SAVES_ENTRY_SIZE], const char *name,
    unsigned blocks)
{
    memset(entry, 0, UI_SAVES_ENTRY_SIZE);
    memcpy(entry, "GALE01", 6);
    entry[6] = 0xFF;
    entry[7] = 0x02;
    memcpy(entry + 8, name, strlen(name));
    entry[0x2C] = 0x11; entry[0x2D] = 0x22; entry[0x2E] = 0x33; entry[0x2F] = 0x44;
    entry[0x34] = 0x04;
    entry[0x38] = (uint8_t)(blocks >> 8);
    entry[0x39] = (uint8_t)blocks;
    entry[0x3A] = 0xFF; entry[0x3B] = 0xFF;
    entry[0x3F] = 0x40;
}

/* A file of header bytes, then the entry, then the blocks. */
static uint8_t *makeFile(size_t header, const uint8_t entry[UI_SAVES_ENTRY_SIZE],
    unsigned blocks, size_t *length)
{
    uint8_t *file;
    size_t i;

    *length = header + UI_SAVES_ENTRY_SIZE + (size_t)blocks * UI_SAVES_BLOCK_SIZE;
    file = calloc(1u, *length);
    assert(file != NULL);
    memcpy(file + header, entry, UI_SAVES_ENTRY_SIZE);
    for(i = header + UI_SAVES_ENTRY_SIZE; i < *length; i++) {
        file[i] = (uint8_t)i;
    }
    return file;
}

static void swapPairs(uint8_t *bytes, size_t length)
{
    size_t i;
    for(i = 0u; i + 1u < length; i += 2u) {
        uint8_t first = bytes[i];
        bytes[i] = bytes[i + 1u];
        bytes[i + 1u] = first;
    }
}

static void expectFileName(const char *name, const char *expected)
{
    uint8_t entry[UI_SAVES_ENTRY_SIZE];
    char out[96];

    makeEntry(entry, name, 1u);
    UISaves_FileName(out, sizeof(out), entry);
    assert(strcmp(out, expected) == 0);
}

static void expectNumbered(const char *name, int attempt, const char *expected)
{
    char out[64];

    UISaves_NumberedName(out, sizeof(out), name, attempt);
    assert(strcmp(out, expected) == 0);
}

int main(void)
{
    uint8_t entry[UI_SAVES_ENTRY_SIZE];
    uint8_t found[UI_SAVES_ENTRY_SIZE];
    uint8_t *file;
    size_t length;
    char out[96];
    uiSavesPlace_t places[UI_SAVES_PLACE_COUNT];

    /* A plain .gci: the entry comes first, the blocks start at 64. */
    makeEntry(entry, "SuperSmashBros0110290334", 3u);
    assert(UISaves_Blocks(entry) == 3u);
    file = makeFile(0u, entry, 3u, &length);
    memset(found, 0xAA, sizeof(found));
    assert(UISaves_FindEntry(file, length, found) == 64u);
    assert(memcmp(found, entry, sizeof(entry)) == 0);
    /* One byte more or less is not a whole save. */
    assert(UISaves_FindEntry(file, length - 1u, found) == 0u);
    free(file);
    file = makeFile(0u, entry, 4u, &length);
    assert(UISaves_FindEntry(file, length, found) == 0u);
    free(file);

    /* A save the entry says has no blocks is no save to write. */
    makeEntry(entry, "empty", 0u);
    file = makeFile(0u, entry, 0u, &length);
    assert(UISaves_FindEntry(file, length, found) == 0u);
    free(file);

    /* GameShark: "GCSAVE", the entry at 0x110. */
    makeEntry(entry, "gcs", 2u);
    file = makeFile(0x110u, entry, 2u, &length);
    memcpy(file, "GCSAVE", 6);
    assert(UISaves_FindEntry(file, length, found) == 0x150u);
    assert(memcmp(found, entry, sizeof(entry)) == 0);
    free(file);

    /* Action Replay: "DATELGC_SAVE", the entry at 0x80 with bytes 6-7 and
     * the 20 from 0x2C swapped in pairs; the block count is among them. */
    makeEntry(entry, "datel", 1u);
    {
        uint8_t swapped[UI_SAVES_ENTRY_SIZE];
        memcpy(swapped, entry, sizeof(swapped));
        swapPairs(swapped + 6, 2u);
        swapPairs(swapped + 0x2C, 20u);
        file = makeFile(0x80u, swapped, 1u, &length);
    }
    memcpy(file, "DATELGC_SAVE", 12);
    assert(UISaves_FindEntry(file, length, found) == 0xC0u);
    assert(memcmp(found, entry, sizeof(entry)) == 0);
    free(file);

    /* Too short for any entry, or nothing at all. */
    assert(UISaves_FindEntry((const uint8_t *)"GCSAVE", 6u, found) == 0u);
    assert(UISaves_FindEntry(NULL, 100u, found) == 0u);

    /* Dolphin's GCI folder names: maker, game, card name. */
    expectFileName("SuperSmashBros0110290334", "01-GALE-SuperSmashBros0110290334.gci");
    expectFileName("a/b:c*d?e\"f<g>h|i\\j", "01-GALE-a_b_c_d_e_f_g_h_i_j.gci");
    expectFileName("name. .", "01-GALE-name.gci");
    expectFileName("", "01-GALE-save.gci");
    expectFileName("\x82\xA0jp", "01-GALE-__jp.gci");
    /* A card name fills all 32 bytes with no terminator. */
    expectFileName("ABCDEFGHIJKLMNOPQRSTUVWXYZ012345", "01-GALE-ABCDEFGHIJKLMNOPQRSTUVWXYZ012345.gci");
    makeEntry(entry, "long name", 1u);
    UISaves_FileName(out, 8u, entry);
    assert(strlen(out) == 7u);

    expectNumbered("01-GALE-save.gci", 1, "01-GALE-save.gci");
    expectNumbered("01-GALE-save.gci", 2, "01-GALE-save_2.gci");
    expectNumbered("noextension", 3, "noextension_3");
    expectNumbered(".gci", 2, ".gci_2");

    /* A Slot A save goes to Slot B or a folder, never back to Slot A. */
    assert(UISaves_Destinations(UI_SAVES_PLACE_SLOT_A, true, true, true, false, places) == 3);
    assert(places[0] == UI_SAVES_PLACE_SLOT_B);
    assert(places[1] == UI_SAVES_PLACE_FOLDER);
    assert(places[2] == UI_SAVES_PLACE_CHOOSE);
    /* No SD card: only the other slot. */
    assert(UISaves_Destinations(UI_SAVES_PLACE_SLOT_B, true, true, false, false, places) == 1);
    assert(places[0] == UI_SAVES_PLACE_SLOT_A);
    /* One card and no SD card: nowhere to go. */
    assert(UISaves_Destinations(UI_SAVES_PLACE_SLOT_A, true, false, false, false, places) == 0);
    /* A save already in the Save Folder isn't offered it again. */
    assert(UISaves_Destinations(UI_SAVES_PLACE_FOLDER, true, false, true, true, places) == 2);
    assert(places[0] == UI_SAVES_PLACE_SLOT_A);
    assert(places[1] == UI_SAVES_PLACE_CHOOSE);
    assert(UISaves_Destinations(UI_SAVES_PLACE_FOLDER, true, true, true, false, places) == 4);
    assert(UISaves_Destinations(UI_SAVES_PLACE_FOLDER, false, false, true, false, NULL) == 0);
    return 0;
}
