#!/usr/bin/env python3
"""Run real browser Back arms and Home handoff against the real event queue.

Regression: /games -> X (device root fallback browser) -> B left the legacy
browser published above Home. Retained Gameflow hid itself by scene; ordinary
container children did not. These tests exercise all three browser Back arms,
release/re-entry ownership, and a deliberately independent source selector.
"""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
SWISS = (ROOT / 'cube/swiss/source/swiss.c').read_text()
FRAME = (ROOT / 'cube/swiss/source/gui/FrameBufferMagic.c').read_text()


def block(source, marker):
    start = source.index(marker)
    opening = source.index('{', start)
    depth = 0
    for index in range(opening, len(source)):
        depth += (source[index] == '{') - (source[index] == '}')
        if depth == 0:
            return source[start:index + 1]
    raise AssertionError(marker)


def require_wiring(source):
    menu = block(source, 'void menu_loop()')
    body = menu[menu.index('uiDrawObj_t *filePanel = NULL;'):]
    assert body.count('homePublishBrowserTransition(&filePanel);') == 2
    assert 'homePublish(curMenuLocation == ON_OPTIONS);' not in body
    assert body.index('homePublishBrowserTransition(&filePanel);') < body.index('if(devices[DEVICE_CUR] != NULL && needsRefresh)')
    assert body.index('homePublishBrowserTransition(&filePanel);', body.index('scanFiles();')) < body.index('if(homeLibraryEntryPending)')


PREFIX = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr, "failed %d: %s\n", __LINE__, #c); exit(73); } } while(0)
typedef struct uiDrawObj { int type; bool disposed; struct uiDrawObj *child; void *data; } uiDrawObj_t;
typedef struct uiQueue { uiDrawObj_t *event; struct uiQueue *next; } uiDrawObjQueue_t;
typedef struct { uiDrawObj_t *uiObj; } file_handle;
enum { ON_FILLIST, ON_OPTIONS, B_SELECTED, B_NOSELECT, BUTTON_B = 16 };
static uiDrawObjQueue_t *videoEventQueue;
static int curMenuLocation, curSelection, needsDeviceChange, publications, disposals;
static const char *curDir = "dvd:/";
/* DrawDispose lets go of a Settings page's Menu Color; no route here pins one. */
uiDrawObj_t *menuColorPage; int menuColorPinned = -1, menuColorPreview = -1;
#define LWP_MutexLock(unused) ((void)0)
#define LWP_MutexUnlock(unused) ((void)0)
static void clearNestedEvent(uiDrawObj_t *event) { free(event); }
static unsigned activeType(int type)
{
    unsigned result = 0;
    for(uiDrawObjQueue_t *q = videoEventQueue; q; q = q->next)
        result += !q->event->disposed && q->event->type == type;
    return result;
}
static void homePublish(bool visible)
{
    ++publications;
    if(visible) CHECK(activeType(1) == 0 && activeType(3) == 0);
}
static void DrawUpdateFileBrowserButton(uiDrawObj_t *event, int mode)
{ CHECK(event != NULL && !event->disposed); CHECK(mode == B_NOSELECT); }
'''

SUFFIX = r'''
static uiDrawObj_t *panel(int type)
{
    uiDrawObj_t *result = calloc(1, sizeof(*result)); CHECK(result); result->type = type;
    return DrawPublish(result);
}
static void retrace(void)
{
    uiDrawObjQueue_t *q = videoEventQueue->next;
    while(q) {
        if(q->event->disposed) { disposeEvent(q->event); ++disposals; q = videoEventQueue->next; }
        else q = q->next;
    }
}
int main(void)
{
    panel(0); /* permanent background/Home root */
    for(int browser = 0; browser < 4; ++browser) {
        uiDrawObj_t *filePanel = panel(browser == 3 ? 3 : 1);
        for(int visit = 0; visit < 100; ++visit) {
            curMenuLocation = ON_FILLIST; curSelection = 2; needsDeviceChange = 0;
            file_handle entries[3] = {{filePanel},{filePanel},{filePanel}};
            file_handle *directory[3] = {&entries[0],&entries[1],&entries[2]};
            uiDrawObj_t *before = filePanel;
            homePublishBrowserTransition(&filePanel);
            CHECK(filePanel == before && !before->disposed); /* Library stays live. */
            if(browser == 0) back_list(directory, false);
            else if(browser == 1 || browser == 3) back_carousel(directory, browser == 3);
            else back_fullwidth(directory, false);
            CHECK(curMenuLocation == ON_OPTIONS);
            homePublishBrowserTransition(&filePanel);
            CHECK(filePanel == NULL && before->disposed);
            CHECK(curSelection == 2 && needsDeviceChange == 0 && !strcmp(curDir,"dvd:/"));
            retrace();
            CHECK(activeType(1) == 0 && activeType(3) == 0);
            homePublishBrowserTransition(&filePanel); /* Safe repeated Home frame. */
            /* Intentional Source is independent and must not be canceled by browser cleanup. */
            uiDrawObj_t *selector = panel(2);
            homePublishBrowserTransition(&filePanel);
            CHECK(activeType(2) == 1 && !selector->disposed);
            DrawDispose(selector); retrace();
            CHECK(activeType(2) == 0);
            curMenuLocation = ON_FILLIST;
            uiDrawObj_t *fresh = calloc(1,sizeof(*fresh)); CHECK(fresh);
            fresh->type = browser == 3 ? 3 : 1;
            filePanel = DrawRepublish(filePanel, fresh);
            retrace();
            CHECK(!filePanel->disposed && activeType(fresh->type) == 1);
        }
        curMenuLocation = ON_OPTIONS; homePublishBrowserTransition(&filePanel); retrace();
    }
    homePublishBrowserTransition(NULL);
    CHECK(disposals == 804 && publications == 1605);
    CHECK(videoEventQueue->next == NULL);
    free(videoEventQueue->event); free(videoEventQueue);
    puts("browser/Home lifecycle: 400 legacy and retained returns passed");
    return 0;
}
'''


class BrowserHomeLifecycle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.helper = block(SWISS, 'static void homePublishBrowserTransition(')
        cls.queue = '\n'.join(block(FRAME, marker) for marker in (
            'static uiDrawObj_t* addVideoEvent(', 'static void disposeEvent(',
            'uiDrawObj_t* DrawPublish(', 'uiDrawObj_t* DrawRepublish(', 'void DrawDispose('))
        cls.arms = ''
        for name, marker in (('list', 'uiDrawObj_t* renderFileBrowser('),
                             ('carousel', 'uiDrawObj_t* renderFileCarousel('),
                             ('fullwidth', 'uiDrawObj_t* renderFileFullwidth(')):
            arm = block(block(SWISS, marker), 'if(browserButtons & BUTTON_B)')
            cls.arms += f'''static void back_{name}(file_handle **directory, bool useGameflow)
{{ const unsigned browserButtons = BUTTON_B; (void)useGameflow; do {{ {arm} }} while(0); }}\n'''

    def run_harness(self, helper, sanitized=False):
        with tempfile.TemporaryDirectory() as tmp:
            source = Path(tmp) / 'lifecycle.c'; binary = Path(tmp) / 'lifecycle'
            source.write_text(PREFIX + self.queue + helper + self.arms + SUFFIX)
            cmd = shlex.split(os.environ.get('CC', 'cc')) + ['-std=c11', '-Wall', '-Wextra', '-Werror']
            if sanitized:
                cmd += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                        '-fno-omit-frame-pointer']
                # Match the host suite: a high-ASLR Linux configuration
                # can place PIE executables in ASan's reserved shadow range.
                if sys.platform.startswith('linux'):
                    cmd += ['-fno-pie', '-no-pie']
            subprocess.run(cmd + [str(source), '-o', str(binary)], check=True,
                           capture_output=True, text=True, timeout=30)
            return subprocess.run([str(binary)], capture_output=True, text=True,
                                  timeout=10)

    def test_real_back_routes_and_event_ownership(self):
        for sanitized in (False, True):
            result = self.run_harness(self.helper, sanitized)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_regression_mutants_fail(self):
        for old, new in (('DrawDispose(*filePanel);', ''), ('*filePanel = NULL;', ''),
                         ('if(visible &&', 'if(!visible &&')):
            with self.subTest(mutation=old):
                result = self.run_harness(self.helper.replace(old, new), True)
                self.assertNotEqual(result.returncode, 0)
        # Publishing Home before ownership ends is observable even before the next retrace.
        reordered = self.helper.replace('homePublish(visible);', '').replace(
            'bool visible = curMenuLocation == ON_OPTIONS;',
            'bool visible = curMenuLocation == ON_OPTIONS; homePublish(visible);')
        self.assertNotEqual(self.run_harness(reordered).returncode, 0)

    def test_actual_menu_wiring_and_mutants(self):
        require_wiring(SWISS)
        for occurrence in (0, 1):
            marker = 'homePublishBrowserTransition(&filePanel);'
            positions = [i for i in range(len(SWISS)) if SWISS.startswith(marker, i)]
            position = positions[occurrence]
            mutant = SWISS[:position] + SWISS[position:].replace(marker,
                'homePublish(curMenuLocation == ON_OPTIONS);', 1)
            with self.assertRaises(AssertionError): require_wiring(mutant)


if __name__ == '__main__': unittest.main(verbosity=2)
