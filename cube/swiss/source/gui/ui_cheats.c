#include "ui_cheats.h"

#include <string.h>

static size_t boundedLength(const char *source)
{
    size_t length = 0u;
    if(source != NULL) {
        while(length < UI_CHEATS_SOURCE_LIMIT && source[length] != '\0') {
            ++length;
        }
    }
    return length;
}

static void copyRange(char *out, size_t capacity, const char *source,
    size_t start, size_t end)
{
    size_t length;
    if(out == NULL || capacity == 0u) return;
    while(start < end && source[start] == ' ') ++start;
    while(end > start && source[end - 1u] == ' ') --end;
    length = end - start;
    if(length >= capacity) length = capacity - 1u;
    if(length != 0u) memcpy(out, source + start, length);
    out[length] = '\0';
}

static bool endsWith(const char *source, size_t length, const char *suffix)
{
    size_t suffixLength = strlen(suffix);
    size_t i;
    if(length < suffixLength) return false;
    for(i = 0u; i < suffixLength; ++i) {
        char value = source[length - suffixLength + i];
        if(value >= 'A' && value <= 'Z') value = (char)(value + ('a' - 'A'));
        if(value != suffix[i]) return false;
    }
    return true;
}

void UICheats_GameTitle(char *out, size_t capacity, const char *source)
{
    size_t end = boundedLength(source);
    size_t start = 0u;
    size_t i;
    if(source == NULL) source = "";
    for(i = 0u; i < end; ++i) {
        if(source[i] == '/' || source[i] == '\\') start = i + 1u;
    }
    while(end > start && source[end - 1u] == ' ') --end;
    if(endsWith(source, end, ".iso") || endsWith(source, end, ".gcm")) end -= 4u;
    if(endsWith(source, end, ".nkit")) end -= 5u;
    /* Strict-folder libraries commonly use game.iso or disc1/2.iso. The
     * descriptive parent is display context, not a new game identity. */
    if(start > 0u && ((end - start == 4u && endsWith(source, end, "game")) ||
        (end - start == 5u && (endsWith(source, end, "disc1") ||
            endsWith(source, end, "disc2"))))) {
        size_t parentEnd = start - 1u;
        size_t parentStart = parentEnd;
        while(parentStart > 0u && source[parentStart - 1u] != '/' &&
            source[parentStart - 1u] != '\\') --parentStart;
        if(parentEnd > parentStart && source[parentEnd - 1u] != ':') {
            start = parentStart;
            end = parentEnd;
        }
    }
    if(end >= start + 8u && source[end - 1u] == ']' && source[end - 8u] == '[') {
        bool gameId = true;
        for(i = end - 7u; i < end - 1u; ++i) {
            char value = source[i];
            if(!((value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9'))) {
                gameId = false;
            }
        }
        if(gameId) end -= 8u;
    }
    copyRange(out, capacity, source, start, end);
}

void UICheats_Name(char *out, size_t capacity, const char *source)
{
    size_t end = boundedLength(source);
    size_t author;
    if(source == NULL) source = "";
    while(end > 0u && source[end - 1u] == ' ') --end;
    if(end > 0u && source[end - 1u] == ']') {
        author = end - 1u;
        while(author > 0u && source[author] != '[') --author;
        if(author > 1u && source[author] == '[' && source[author - 1u] == ' ') {
            /* Catalogs also put compatibility requirements in brackets.
             * Remove only credits known from our catalog, never a qualifier. */
            static const char *const credits[] = {
                "[Ralf]", "[hawkeye2777 & Ralf]"
            };
            size_t i;
            for(i = 0u; i < sizeof(credits) / sizeof(credits[0]); ++i) {
                size_t creditLength = strlen(credits[i]);
                if(end - author == creditLength &&
                    memcmp(source + author, credits[i], creditLength) == 0) {
                    end = author - 1u;
                    break;
                }
            }
        }
    }
    copyRange(out, capacity, source, 0u, end);
}

void UICheats_Fit(char *out, size_t capacity, const char *source,
    int maxWidth, float scale, uiCheatsMeasureFn measure)
{
    size_t sourceLength = boundedLength(source);
    size_t length;
    bool clipped;
    if(out == NULL || capacity == 0u) return;
    out[0] = '\0';
    if(source == NULL || measure == NULL || maxWidth <= 0 || !(scale > 0.0f)) return;
    copyRange(out, capacity, source, 0u, sourceLength);
    length = strlen(out);
    clipped = sourceLength >= capacity;
    if(!clipped && (float)measure(out) * scale <= (float)maxWidth) return;
    if(capacity < 4u) { out[0] = '\0'; return; }
    if(length > capacity - 4u) length = capacity - 4u;
    while(1) {
        memcpy(out + length, "...", 4u);
        if((float)measure(out) * scale <= (float)maxWidth) return;
        if(length == 0u) { out[0] = '\0'; return; }
        --length;
    }
}

void UICheats_Wrap(char lines[3][UI_CHEATS_TEXT_CAPACITY],
    const char *source, int maxWidth, float scale, uiCheatsMeasureFn measure)
{
    size_t length = boundedLength(source);
    size_t offset = 0u;
    int row;
    memset(lines, 0, 3u * UI_CHEATS_TEXT_CAPACITY);
    if(source == NULL || measure == NULL || maxWidth <= 0 || !(scale > 0.0f)) return;
    for(row = 0; row < 3 && offset < length; ++row) {
        size_t used = 0u;
        size_t wordBreak = 0u;
        while(offset < length && source[offset] == ' ') ++offset;
        if(row == 2) {
            char tail[UI_CHEATS_SOURCE_LIMIT + 1u];
            copyRange(tail, sizeof(tail), source, offset, length);
            UICheats_Fit(lines[row], UI_CHEATS_TEXT_CAPACITY, tail,
                maxWidth, scale, measure);
            break;
        }
        while(offset + used < length && used + 1u < UI_CHEATS_TEXT_CAPACITY) {
            lines[row][used] = source[offset + used];
            lines[row][used + 1u] = '\0';
            if((float)measure(lines[row]) * scale > (float)maxWidth) {
                lines[row][used] = '\0';
                break;
            }
            if(source[offset + used] == ' ') wordBreak = used;
            ++used;
        }
        if(used == 0u) break;
        if(offset + used < length && source[offset + used] != ' ' && wordBreak > 0u) {
            used = wordBreak;
            lines[row][used] = '\0';
        }
        offset += used;
    }
}

static bool visible(int index, bool enabledOnly, uiCheatsEnabledFn enabled,
    const void *context)
{
    return !enabledOnly || (enabled != NULL && enabled(index, context));
}

int UICheats_Count(int total, bool enabledOnly, uiCheatsEnabledFn enabled,
    const void *context)
{
    int count = 0;
    int i;
    for(i = 0; i < total; ++i) if(visible(i, enabledOnly, enabled, context)) ++count;
    return count;
}

int UICheats_Index(int total, bool enabledOnly, int visibleIndex,
    uiCheatsEnabledFn enabled, const void *context)
{
    int i;
    if(visibleIndex < 0) return -1;
    for(i = 0; i < total; ++i) {
        if(visible(i, enabledOnly, enabled, context) && visibleIndex-- == 0) return i;
    }
    return -1;
}

int UICheats_Preserve(int total, bool enabledOnly, int underlyingIndex,
    uiCheatsEnabledFn enabled, const void *context)
{
    int count = 0;
    int i;
    for(i = 0; i < total; ++i) {
        if(visible(i, enabledOnly, enabled, context)) {
            if(i >= underlyingIndex) return count;
            ++count;
        }
    }
    return count > 0 ? count - 1 : 0;
}

int UICheats_Move(int selected, int count, int direction, bool page)
{
    int step = page ? UI_CHEATS_VISIBLE_ROWS : 1;
    if(count <= 0) return 0;
    if(selected < 0 || selected >= count) selected = 0;
    if(direction < 0) {
        if(selected == 0) return count - 1;
        return selected < step ? 0 : selected - step;
    }
    if(direction > 0) {
        if(selected == count - 1) return 0;
        return count - 1 - selected < step ? count - 1 : selected + step;
    }
    return selected;
}

int UICheats_WindowStart(int selected, int count)
{
    int start;
    if(count <= UI_CHEATS_VISIBLE_ROWS) return 0;
    if(selected < 0 || selected >= count) selected = 0;
    start = selected - UI_CHEATS_VISIBLE_ROWS / 2;
    if(start < 0) return 0;
    return start > count - UI_CHEATS_VISIBLE_ROWS ? count - UI_CHEATS_VISIBLE_ROWS : start;
}
