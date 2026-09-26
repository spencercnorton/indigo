#include "ui_cheats.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static bool enabled(int index, const void *context)
{
    const unsigned int *mask = context;
    assert(index >= 0 && index < 8);
    return (*mask & (1u << (unsigned int)index)) != 0u;
}

static int measure(const char *text)
{
    return (int)strlen(text) * 10;
}

static void expectName(const char *source, const char *expected)
{
    char result[128];
    UICheats_Name(result, sizeof(result), source);
    assert(strcmp(result, expected) == 0);
}

int main(void)
{
    char result[128];
    char lines[3][UI_CHEATS_TEXT_CAPACITY];
    char longName[UI_CHEATS_SOURCE_LIMIT + 2u];
    unsigned int mask;
    int total;
    int selected;
    size_t capacity;
    UICheats_GameTitle(result, sizeof(result), "gcldr:/games/Super Mario Sunshine [GMSE01].iso");
    assert(strcmp(result, "Super Mario Sunshine") == 0);
    UICheats_GameTitle(result, sizeof(result), "C:\\games\\Game [GALE01].NKIT.ISO");
    assert(strcmp(result, "Game") == 0);
    UICheats_GameTitle(result, sizeof(result), "gcldr:/games/Super Mario Sunshine [GMSE01]/game.iso");
    assert(strcmp(result, "Super Mario Sunshine") == 0);
    UICheats_GameTitle(result, sizeof(result), "gcldr:/games/Resident Evil [GBIE08]/DISC2.GCM");
    assert(strcmp(result, "Resident Evil") == 0);
    UICheats_GameTitle(result, sizeof(result), "gcldr:/games/Other folder/Actual title.iso");
    assert(strcmp(result, "Actual title") == 0);
    UICheats_GameTitle(result, sizeof(result), "gcldr:/game.iso");
    assert(strcmp(result, "game") == 0);
    UICheats_GameTitle(result, sizeof(result), "Game [PAL only].gcm");
    assert(strcmp(result, "Game [PAL only]") == 0);
    UICheats_GameTitle(result, sizeof(result), NULL);
    assert(result[0] == '\0');
    expectName("Invincible [Ralf]", "Invincible");
    expectName("Mario Opens Yoshi Eggs [hawkeye2777 & Ralf]", "Mario Opens Yoshi Eggs");
    expectName("Mode [PAL only]", "Mode [PAL only]");
    expectName("Mode [Requires master code]", "Mode [Requires master code]");
    expectName("Mode [v1.02 only]", "Mode [v1.02 only]");
    expectName("Mode [WARNING: save corruption]", "Mode [WARNING: save corruption]");
    expectName("Mode [Unknown author]", "Mode [Unknown author]");
    expectName("[Ralf]", "[Ralf]");
    expectName(NULL, "");

    memset(longName, 'X', sizeof(longName));
    for(capacity = 0u; capacity <= sizeof(result); ++capacity) {
        char guarded[130];
        memset(guarded, '?', sizeof(guarded));
        UICheats_Fit(guarded + 1, capacity, longName, 130, 1.0f, measure);
        assert(guarded[0] == '?');
        assert(guarded[capacity + 1u] == '?');
        if(capacity != 0u) {
            assert(memchr(guarded + 1, '\0', capacity) != NULL);
            assert(measure(guarded + 1) <= 130);
        }
    }
    UICheats_Fit(result, sizeof(result), "A long title", 80, 1.0f, measure);
    assert(strcmp(result, "A lon...") == 0);
    UICheats_Fit(result, sizeof(result), "short", 20, 1.0f, measure);
    assert(result[0] == '\0');
    UICheats_Wrap(lines, "One two three four five six", 100, 1.0f, measure);
    assert(strcmp(lines[0], "One two") == 0);
    assert(strcmp(lines[1], "three four") == 0);
    assert(strcmp(lines[2], "five six") == 0);
    UICheats_Wrap(lines, longName, 100, 1.0f, measure);
    for(total = 0; total < 3; ++total) assert(measure(lines[total]) <= 100);

    for(mask = 0u; mask < 256u; ++mask) {
        for(total = 0; total <= 8; ++total) {
            int expected[8];
            int count = 0;
            int index;
            for(index = 0; index < total; ++index) {
                if(enabled(index, &mask)) expected[count++] = index;
            }
            assert(UICheats_Count(total, false, enabled, &mask) == total);
            assert(UICheats_Count(total, true, enabled, &mask) == count);
            assert(UICheats_Index(total, true, -1, enabled, &mask) == -1);
            assert(UICheats_Index(total, true, count, enabled, &mask) == -1);
            for(index = 0; index < count; ++index) {
                assert(UICheats_Index(total, true, index, enabled, &mask) == expected[index]);
                assert(UICheats_Preserve(total, true, expected[index], enabled, &mask) == index);
            }
            for(index = 0; index < total; ++index) {
                int wanted = 0;
                while(wanted < count && expected[wanted] < index) ++wanted;
                if(wanted >= count) wanted = count > 0 ? count - 1 : 0;
                assert(UICheats_Preserve(total, true, index, enabled, &mask) == wanted);
            }
        }
    }
    for(total = 0; total <= 250; ++total) {
        for(selected = 0; selected <= total; ++selected) {
            int first = UICheats_WindowStart(selected, total);
            int moved;
            assert(first >= 0);
            assert(first + UI_CHEATS_VISIBLE_ROWS <= total || first == 0);
            if(selected < total) assert(selected >= first && selected < first + UI_CHEATS_VISIBLE_ROWS);
            moved = UICheats_Move(selected, total, -1, false);
            assert(moved >= 0 && (moved < total || total == 0));
            moved = UICheats_Move(selected, total, 1, true);
            assert(moved >= 0 && (moved < total || total == 0));
        }
    }
    assert(UICheats_Move(INT_MAX - 2, INT_MAX, 1, true) == INT_MAX - 1);
    assert(UICheats_Move(0, INT_MAX, -1, true) == INT_MAX - 1);
    assert(UICheats_Move(INT_MIN, 0, INT_MIN, true) == 0);
    assert(UICheats_Move(6, 12, -1, true) == 0);
    assert(UICheats_Move(11, 12, 1, true) == 0);
    puts("ui_cheats: labels, bounds, filters, identity preservation and navigation passed");
    return 0;
}
