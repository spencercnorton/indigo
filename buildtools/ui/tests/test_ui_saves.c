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


/* The art fields of an entry: the icon address, banner_fmt, each frame's
 * format and speed (frames past n have neither) and the comment address. */
static void setArt(uint8_t entry[UI_SAVES_ENTRY_SIZE], uint32_t iconAddr,
    uint8_t bannerFormat, const unsigned *formats, const unsigned *speeds,
    unsigned n, uint32_t commentAddr)
{
    unsigned format = 0u, speed = 0u, i;

    for(i = 0u; i < n; i++) {
        format |= formats[i] << (2u * i);
        speed |= speeds[i] << (2u * i);
    }
    entry[0x07] = bannerFormat;
    entry[0x2C] = (uint8_t)(iconAddr >> 24); entry[0x2D] = (uint8_t)(iconAddr >> 16);
    entry[0x2E] = (uint8_t)(iconAddr >> 8); entry[0x2F] = (uint8_t)iconAddr;
    entry[0x30] = (uint8_t)(format >> 8); entry[0x31] = (uint8_t)format;
    entry[0x32] = (uint8_t)(speed >> 8); entry[0x33] = (uint8_t)speed;
    entry[0x3C] = (uint8_t)(commentAddr >> 24); entry[0x3D] = (uint8_t)(commentAddr >> 16);
    entry[0x3E] = (uint8_t)(commentAddr >> 8); entry[0x3F] = (uint8_t)commentAddr;
}

static void expectSteps(const uiSavesArt_t *art, const char *frames,
    const char *holds)
{
    size_t i;

    assert(art->steps == strlen(frames));
    for(i = 0u; i < art->steps; i++) {
        assert(art->stepFrame[i] == (frames[i] == '-' ? UI_SAVES_ART_BLANK :
            (uint8_t)(frames[i] - '0')));
        assert(art->stepHold[i] == (uint8_t)(holds[i] - '0'));
    }
}

/* Decodes a picture the slow way, written apart from ui_saves.c: walk the
 * source in its own tile order (CI8 8x4, RGB5A3 4x4) into rows of 16-bit
 * texels, as Dolphin's DecodeCI8Image and Decode5A3Image do. */
static void untile(const uint8_t *pixels, const uint8_t *palette, int ci8,
    unsigned width, uint16_t *rows)
{
    unsigned tileW = ci8 ? 8u : 4u, x, y, ix, iy;

    for(y = 0u; y < 32u; y += 4u) {
        for(x = 0u; x < width; x += tileW) {
            for(iy = 0u; iy < 4u; iy++) {
                for(ix = 0u; ix < tileW; ix++) {
                    const uint8_t *texel = ci8 ? palette + 2u * *pixels++ : pixels;
                    rows[(y + iy) * width + x + ix] =
                        (uint16_t)(texel[0] << 8 | texel[1]);
                    if(!ci8) {
                        pixels += 2;
                    }
                }
            }
        }
    }
}

/* Pictures with every texel different, so a misplaced tile shows. */
static void fillPicture(uint8_t *bytes, size_t length, unsigned seed)
{
    size_t i;

    for(i = 0u; i < length; i++) {
        bytes[i] = (uint8_t)(i * 7u + i / 251u + seed);
    }
}

static void expectDecoded(const uint8_t *data, size_t length,
    const uiSavesArt_t *art, int frame, const uint8_t *pixels,
    const uint8_t *palette, int ci8)
{
    static uint8_t out[UI_SAVES_BANNER_BYTES];
    static uint16_t expected[96u * 32u], got[96u * 32u];
    unsigned width = frame == UI_SAVES_ART_BANNER ? 96u : 32u;

    memset(out, 0xEE, sizeof(out));
    assert(UISaves_ToRgb5a3(data, length, art, frame, out));
    untile(pixels, palette, ci8, width, expected);
    untile(out, NULL, 0, width, got);
    assert(memcmp(expected, got, width * 32u * sizeof(got[0])) == 0);
    /* It writes the picture and nothing past it. */
    assert(width == 96u || out[UI_SAVES_ICON_BYTES] == 0xEE);
}

static void testArtLayout(void)
{
    static uint8_t data[64u * 1024u];
    uint8_t entry[UI_SAVES_ENTRY_SIZE];
    uiSavesArt_t art;
    const unsigned rgb[8] = {2, 2, 2, 2, 2, 2, 2, 2};
    const unsigned speed2[8] = {2, 2, 2, 2, 2, 2, 2, 2};

    /* A CI8 banner (3072 + its 512-byte palette), then three RGB5A3
     * frames of 2048: the frames start 3584, 5632 and 7680 in. */
    makeEntry(entry, "art", 2u);
    {
        const unsigned speeds[3] = {1, 2, 3};
        setArt(entry, 0u, 0x01, rgb, speeds, 3u, 0xFFFFFFFFu);
    }
    assert(UISaves_ArtLayout(entry, 2u * UI_SAVES_BLOCK_SIZE, &art));
    assert(art.bannerFormat == UI_SAVES_ART_CI8_OWN && art.bannerAt == 0u);
    assert(art.frames == 3u);
    assert(art.frameAt[0] == 3584u && art.frameAt[1] == 5632u &&
        art.frameAt[2] == 7680u);
    assert(art.end == 9728u && !art.comment);
    expectSteps(&art, "012", "123");
    assert(art.period == 6u);

    /* The same with the icon 0x40 in and the comment at 0: the comment is
     * part of what's read, and every offset moves with the icon. */
    {
        const unsigned speeds[3] = {1, 2, 3};
        setArt(entry, 0x40u, 0x02, rgb, speeds, 3u, 0u);
    }
    assert(UISaves_ArtLayout(entry, 2u * UI_SAVES_BLOCK_SIZE, &art));
    assert(art.bannerFormat == UI_SAVES_ART_RGB5A3 && art.bannerAt == 0x40u);
    assert(art.frameAt[0] == 0x40u + 6144u && art.frameAt[2] == 0x40u + 6144u + 4096u);
    assert(art.comment && art.commentAt == 0u);
    assert(art.end == 0x40u + 6144u + 3u * 2048u);

    /* CI8 frames on the shared palette: 1024 each, the palette after the
     * last frame; a frame with its own palette has it right after it. */
    {
        const unsigned formats[4] = {1, 3, 1, 2};
        const unsigned speeds[4] = {1, 1, 1, 1};
        setArt(entry, 0u, 0x00, formats, speeds, 4u, 0xFFFFFFFFu);
    }
    assert(UISaves_ArtLayout(entry, UI_SAVES_BLOCK_SIZE, &art));
    assert(art.bannerFormat == UI_SAVES_ART_NONE);
    assert(art.frameAt[0] == 0u && art.frameAt[1] == 1024u);
    assert(art.frameAt[2] == 1024u + 1536u && art.frameAt[3] == 1024u + 1536u + 1024u);
    assert(art.paletteAt == 1024u + 1536u + 1024u + 2048u);
    assert(art.end == art.paletteAt + 512u);
    /* No shared palette without a frame that uses it. */
    {
        const unsigned formats[2] = {3, 2};
        const unsigned speeds[2] = {1, 1};
        setArt(entry, 0u, 0x00, formats, speeds, 2u, 0xFFFFFFFFu);
    }
    assert(UISaves_ArtLayout(entry, UI_SAVES_BLOCK_SIZE, &art));
    assert(art.end == 1536u + 2048u);
    /* Frames that fit with a shared palette that doesn't: no icon. */
    {
        const unsigned formats[1] = {1};
        const unsigned speeds[1] = {1};
        setArt(entry, 0u, 0x00, formats, speeds, 1u, 0xFFFFFFFFu);
    }
    assert(!UISaves_ArtLayout(entry, 1024u + 511u, &art));
    assert(art.frames == 0u && art.steps == 0u);
    assert(UISaves_ArtLayout(entry, 1024u + 512u, &art));
    assert(art.frames == 1u && art.paletteAt == 1024u);

    /* A speed of 0 ends the frames, whatever follows it. */
    {
        const unsigned speeds[5] = {1, 3, 0, 2, 2};
        setArt(entry, 0u, 0x00, rgb, speeds, 5u, 0xFFFFFFFFu);
    }
    assert(UISaves_ArtLayout(entry, UI_SAVES_BLOCK_SIZE, &art));
    assert(art.frames == 2u && art.end == 4096u);
    expectSteps(&art, "01", "13");

    /* A frame with no pixels shows the next that has them, and takes no
     * bytes; one at the end, with nothing after it, shows nothing. */
    {
        const unsigned formats[5] = {2, 0, 0, 2, 0};
        const unsigned speeds[5] = {1, 2, 1, 3, 2};
        setArt(entry, 0u, 0x00, formats, speeds, 5u, 0xFFFFFFFFu);
    }
    assert(UISaves_ArtLayout(entry, UI_SAVES_BLOCK_SIZE, &art));
    assert(art.frames == 5u && art.frameAt[3] == 2048u && art.end == 4096u);
    expectSteps(&art, "0333-", "12132");
    /* A first frame with no pixels: no icon at all (Dolphin), the banner
     * still there. */
    {
        const unsigned formats[2] = {0, 2};
        const unsigned speeds[2] = {1, 1};
        setArt(entry, 0u, 0x02, formats, speeds, 2u, 0xFFFFFFFFu);
    }
    assert(UISaves_ArtLayout(entry, UI_SAVES_BLOCK_SIZE, &art));
    assert(art.frames == 0u && art.steps == 0u && art.period == 0u);
    assert(art.bannerFormat == UI_SAVES_ART_RGB5A3 && art.end == 6144u);
    /* banner_fmt 3 is no banner; the icon starts at the address. */
    {
        const unsigned speeds[1] = {1};
        setArt(entry, 0x40u, 0x03, rgb, speeds, 1u, 0xFFFFFFFFu);
    }
    assert(UISaves_ArtLayout(entry, UI_SAVES_BLOCK_SIZE, &art));
    assert(art.bannerFormat == UI_SAVES_ART_NONE && art.frameAt[0] == 0x40u);

    /* Bounce (banner_fmt 0x04) adds the frames between the last and the
     * first, backwards: 1, 2, 3 and 8 frames take 1, 2, 4 and 14 steps. */
    {
        const unsigned speeds[3] = {1, 2, 3};
        setArt(entry, 0u, 0x04, rgb, speeds, 1u, 0xFFFFFFFFu);
        assert(UISaves_ArtLayout(entry, UI_SAVES_BLOCK_SIZE, &art));
        expectSteps(&art, "0", "1");
        setArt(entry, 0u, 0x04, rgb, speeds, 2u, 0xFFFFFFFFu);
        assert(UISaves_ArtLayout(entry, UI_SAVES_BLOCK_SIZE, &art));
        expectSteps(&art, "01", "12");
        setArt(entry, 0u, 0x04, rgb, speeds, 3u, 0xFFFFFFFFu);
        assert(UISaves_ArtLayout(entry, UI_SAVES_BLOCK_SIZE, &art));
        expectSteps(&art, "0121", "1232");
        assert(art.period == 8u);
    }
    setArt(entry, 0u, 0x04 | 0x02, rgb, speed2, 8u, 0xFFFFFFFFu);
    assert(UISaves_ArtLayout(entry, 3u * UI_SAVES_BLOCK_SIZE, &art));
    assert(art.frames == 8u && art.steps == UI_SAVES_ART_STEPS);
    expectSteps(&art, "01234567654321", "22222222222222");
    assert(art.period == 28u);

    /* Icon address 0xFFFFFFFF: no banner and no icon; the comment alone. */
    setArt(entry, 0xFFFFFFFFu, 0x02, rgb, speed2, 2u, 0x10u);
    assert(UISaves_ArtLayout(entry, UI_SAVES_BLOCK_SIZE, &art));
    assert(art.frames == 0u && art.bannerFormat == UI_SAVES_ART_NONE);
    assert(art.comment && art.end == 0x10u + 64u);
    /* Nothing at all: nothing to read. */
    setArt(entry, 0xFFFFFFFFu, 0x02, rgb, speed2, 2u, 0xFFFFFFFFu);
    assert(!UISaves_ArtLayout(entry, UI_SAVES_BLOCK_SIZE, &art));
    assert(art.end == 0u);

    /* A part past the save's data is left out on its own: here the frames,
     * while the banner and comment fit. */
    setArt(entry, 0u, 0x02, rgb, speed2, 2u, 0u);
    assert(UISaves_ArtLayout(entry, 6144u + 2048u + 2047u, &art));
    assert(art.bannerFormat == UI_SAVES_ART_RGB5A3 && art.frames == 0u);
    assert(art.steps == 0u && art.comment && art.end == 6144u);
    assert(UISaves_ArtStep(&art, 0u) == -1);
    assert(UISaves_ArtLayout(entry, 6144u + 4096u, &art));
    assert(art.frames == 2u && art.end == 6144u + 4096u);
    /* ...and past UI_SAVES_ART_MAX_END, however long the save. */
    setArt(entry, UI_SAVES_ART_MAX_END - 6144u, 0x02, rgb, speed2, 2u, 0xFFFFFFFFu);
    assert(UISaves_ArtLayout(entry, 100u * UI_SAVES_BLOCK_SIZE, &art));
    assert(art.bannerFormat == UI_SAVES_ART_RGB5A3 && art.frames == 0u);
    assert(art.end == UI_SAVES_ART_MAX_END);
    setArt(entry, 0u, 0x00, rgb, speed2, 1u, UI_SAVES_ART_MAX_END - 63u);
    assert(UISaves_ArtLayout(entry, 100u * UI_SAVES_BLOCK_SIZE, &art));
    assert(!art.comment && art.end == 2048u);
    /* An icon address near 2^32 doesn't wrap round to the start. */
    setArt(entry, 0xFFFFFFF0u, 0x02, rgb, speed2, 8u, 0xFFFFFFC0u);
    assert(!UISaves_ArtLayout(entry, 0xFFFFFFFFu, &art));
    assert(art.end == 0u && art.frames == 0u && !art.comment);

    /* An Action Replay file's entry, swapped in pairs, lays out as the
     * .gci's does once FindEntry has undone the swap. */
    {
        uint8_t swapped[UI_SAVES_ENTRY_SIZE], found[UI_SAVES_ENTRY_SIZE];
        uiSavesArt_t gci;
        uint8_t *file;
        size_t length;
        const unsigned formats[3] = {1, 3, 2};
        const unsigned speeds[3] = {3, 1, 2};

        makeEntry(entry, "datel-art", 2u);
        setArt(entry, 0x40u, 0x01 | 0x04, formats, speeds, 3u, 0u);
        assert(UISaves_ArtLayout(entry, 2u * UI_SAVES_BLOCK_SIZE, &gci));
        memcpy(swapped, entry, sizeof(swapped));
        swapPairs(swapped + 6, 2u);
        swapPairs(swapped + 0x2C, 20u);
        file = makeFile(0x80u, swapped, 2u, &length);
        memcpy(file, "DATELGC_SAVE", 12);
        assert(UISaves_FindEntry(file, length, found) == 0xC0u);
        assert(UISaves_ArtLayout(found, length - 0xC0u, &art));
        assert(memcmp(&art, &gci, sizeof(art)) == 0);
        expectSteps(&art, "0121", "3121");
        free(file);
    }

    /* No entry, no art. */
    assert(!UISaves_ArtLayout(NULL, 100u, &art));
    assert(!UISaves_ArtLayout(entry, 100u, NULL));

    /* Decoding: an RGB5A3 frame comes out byte for byte; CI8 frames, on the
     * shared palette and on their own, and the CI8 banner come out as
     * the slow decode reads them. */
    {
        const unsigned formats[4] = {2, 1, 3, 1};
        const unsigned speeds[4] = {1, 1, 1, 1};
        static uint8_t out[UI_SAVES_BANNER_BYTES];

        makeEntry(entry, "decode", 3u);
        setArt(entry, 0x40u, 0x01, formats, speeds, 4u, 0u);
        assert(UISaves_ArtLayout(entry, sizeof(data), &art));
        fillPicture(data, sizeof(data), 3u);
        assert(UISaves_ToRgb5a3(data, art.end, &art, 0, out));
        assert(memcmp(out, data + art.frameAt[0], UI_SAVES_ICON_BYTES) == 0);
        expectDecoded(data, art.end, &art, 1, data + art.frameAt[1],
            data + art.paletteAt, 1);
        expectDecoded(data, art.end, &art, 2, data + art.frameAt[2],
            data + art.frameAt[2] + 1024u, 1);
        expectDecoded(data, art.end, &art, 3, data + art.frameAt[3],
            data + art.paletteAt, 1);
        expectDecoded(data, art.end, &art, UI_SAVES_ART_BANNER, data + 0x40u,
            data + 0x40u + 3072u, 1);
        /* An RGB5A3 banner, as it is. */
        setArt(entry, 0x40u, 0x02, formats, speeds, 4u, 0u);
        assert(UISaves_ArtLayout(entry, sizeof(data), &art));
        assert(UISaves_ToRgb5a3(data, art.end, &art, UI_SAVES_ART_BANNER, out));
        assert(memcmp(out, data + 0x40u, UI_SAVES_BANNER_BYTES) == 0);
        expectDecoded(data, art.end, &art, UI_SAVES_ART_BANNER, data + 0x40u,
            NULL, 0);
        /* Nothing outside what was read, no frame that isn't, no pixels. */
        assert(!UISaves_ToRgb5a3(data, art.end - 1u, &art, 3, out));
        assert(!UISaves_ToRgb5a3(data, art.frameAt[1] - 1u, &art, 0, out));
        assert(!UISaves_ToRgb5a3(data, art.frameAt[2] + 1535u, &art, 2, out));
        assert(!UISaves_ToRgb5a3(data, art.end, &art, 4, out));
        assert(!UISaves_ToRgb5a3(data, art.end, &art, -2, out));
        assert(!UISaves_ToRgb5a3(NULL, art.end, &art, 0, out));
        {
            const unsigned gap[2] = {2, 0};
            setArt(entry, 0u, 0x00, gap, speeds, 2u, 0u);
        }
        assert(UISaves_ArtLayout(entry, sizeof(data), &art));
        assert(!UISaves_ToRgb5a3(data, art.end, &art, 1, out));
        assert(!UISaves_ToRgb5a3(data, art.end, &art, UI_SAVES_ART_BANNER, out));
    }

    /* The timeline at 15 ticks a second: each frame held its speed in
     * ticks, round and round; a bounce comes back down before it wraps. */
    {
        const unsigned speeds[4] = {1, 3, 2, 1};
        const int loop[] = {0, 1, 1, 1, 2, 2, 3, 0, 1};
        const int bounce[] = {0, 1, 1, 1, 2, 2, 3, 2, 2, 1, 1, 1, 0, 1};
        uint32_t tick;

        setArt(entry, 0u, 0x00, rgb, speeds, 4u, 0xFFFFFFFFu);
        assert(UISaves_ArtLayout(entry, UI_SAVES_BLOCK_SIZE, &art));
        assert(art.period == 7u);
        for(tick = 0u; tick < sizeof(loop) / sizeof(loop[0]); tick++) {
            assert(UISaves_ArtStep(&art, tick) == loop[tick]);
        }
        assert(UISaves_ArtStep(&art, 7u * 1000u + 3u) == 1);
        assert(UISaves_ArtStep(&art, 0xFFFFFFFFu) == loop[0xFFFFFFFFu % 7u]);
        setArt(entry, 0u, 0x04, rgb, speeds, 4u, 0xFFFFFFFFu);
        assert(UISaves_ArtLayout(entry, UI_SAVES_BLOCK_SIZE, &art));
        assert(art.period == 12u);
        for(tick = 0u; tick < sizeof(bounce) / sizeof(bounce[0]); tick++) {
            assert(UISaves_ArtStep(&art, tick) == bounce[tick]);
        }
        /* A step that shows nothing, and no icon at all. */
        {
            const unsigned formats[2] = {2, 0};
            setArt(entry, 0u, 0x00, formats, speeds, 2u, 0xFFFFFFFFu);
        }
        assert(UISaves_ArtLayout(entry, UI_SAVES_BLOCK_SIZE, &art));
        assert(UISaves_ArtStep(&art, 0u) == 0 && UISaves_ArtStep(&art, 1u) == -1);
        assert(UISaves_ArtStep(&art, 3u) == -1 && UISaves_ArtStep(&art, 4u) == 0);
        memset(&art, 0, sizeof(art));
        assert(UISaves_ArtStep(&art, 5u) == -1);
        assert(UISaves_ArtStep(NULL, 5u) == -1);
    }
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

    /* The same from its first bytes: a prefix holding the entry is enough
     * when the file's whole length is right, and wrong when it isn't. */
    file = makeFile(0u, entry, 3u, &length);
    assert(UISaves_FindEntryPrefix(file, UI_SAVES_HEAD_SIZE, length, found) == 64u);
    assert(memcmp(found, entry, sizeof(entry)) == 0);
    assert(UISaves_FindEntryPrefix(file, 64u, length, found) == 64u);
    assert(UISaves_FindEntryPrefix(file, 63u, length, found) == 0u);
    assert(UISaves_FindEntryPrefix(file, 64u, length + 1u, found) == 0u);
    assert(UISaves_FindEntryPrefix(file, 64u, length - UI_SAVES_BLOCK_SIZE, found) == 0u);
    assert(UISaves_FindEntryPrefix(file, length, length - 1u, found) == 0u);
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
    assert(UISaves_FindEntryPrefix(file, UI_SAVES_HEAD_SIZE, length, found) == 0x150u);
    assert(UISaves_FindEntryPrefix(file, UI_SAVES_HEAD_SIZE - 1u, length, found) == 0u);
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
    memset(found, 0, sizeof(found));
    assert(UISaves_FindEntryPrefix(file, UI_SAVES_HEAD_SIZE, length, found) == 0xC0u);
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

    testArtLayout();
    return 0;
}
