#!/usr/bin/env python3
"""The Library layouts' host tests catch real mistakes.

Each mutant breaks one rule of the navigation, the grid window or the ring
and grid state in a copy of the source; test_gameflow_library.c or
test_gameflow_state.c must then fail.
"""

import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[3]
TESTS = Path(__file__).resolve().parent
GUI = ROOT / "cube/swiss/source/gui"
FLAGS = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-Wconversion",
         "-Wsign-conversion", "-pedantic"]
LIBRARY = ("test_gameflow_library.c", ["ui_gameflow_library.c"])
STATE = ("test_gameflow_state.c", ["ui_gameflow.c", "ui_motion.c"])

MUTANTS = {
    "a short last row runs past the end": (LIBRARY, "ui_gameflow_library.c",
        "\t\tif(target > last) {\n\t\t\ttarget = last;\n\t\t}\n", ""),
    "up from the first row stays there": (LIBRARY, "ui_gameflow_library.c",
        "\t\t\t\trow = row == 0u ? rows - 1u : row - 1u;\n\t\t\t\tbreak;\n\t\t\tcase UI_GAMEFLOW_LIBRARY_MOVE_DOWN:",
        "\t\t\t\trow = row == 0u ? 0u : row - 1u;\n\t\t\t\tbreak;\n\t\t\tcase UI_GAMEFLOW_LIBRARY_MOVE_DOWN:"),
    "down from the last row stays there": (LIBRARY, "ui_gameflow_library.c",
        "\t\t\t\trow = row == rows - 1u ? 0u : row + 1u;",
        "\t\t\t\trow = row == rows - 1u ? row : row + 1u;"),
    "the carousel's page stops short": (LIBRARY, "ui_gameflow_library.c",
        "(selectedIndex > page ? selectedIndex - page : 0u);",
        "(selectedIndex > page ? selectedIndex - page + 1u : 0u);"),
    "the grid pages in cards": (LIBRARY, "ui_gameflow_library.c",
        "(page > rows - 1u - row ? rows - 1u : row + page);",
        "(page > rows - 1u - row ? rows - 1u : row + 1u);"),
    "up reads as a move on": (LIBRARY, "ui_gameflow_library.c",
        "\t\tmove == UI_GAMEFLOW_LIBRARY_MOVE_UP ||\n", ""),
    "two rows ignore the last move": (LIBRARY, "ui_gameflow_library.c",
        "int8_t side = rowDirection == UI_GAMEFLOW_DIRECTION_NEXT ? -1 :",
        "int8_t side = rowDirection == UI_GAMEFLOW_DIRECTION_PREVIOUS ? -1 :"),
    "a small grid repeats its rows": (LIBRARY, "ui_gameflow_library.c",
        "\t\tif(j != placedCount) {\n\t\t\tcontinue;\n\t\t}\n\t\tplaced[placedCount++] = row;",
        "\t\tplaced[placedCount++] = row;"),
    "a row starts at its left end": (LIBRARY, "ui_gameflow_library.c",
        "\t\t\tuint32_t offset = (step + 1u) / 2u;", "\t\t\tuint32_t offset = step;"),
    "the grid travels in cards": (STATE, "ui_gameflow.c",
        "\tif(state->columns == 0u) {\n\t\treturn ringDelta(",
        "\tif(state->columns != 99u) {\n\t\treturn ringDelta("),
    "the highlight never moves": (STATE, "ui_gameflow.c",
        "\t\tUIMotion_SpringRetarget(&state->columnSpring,\n\t\t\tgridColumn(state, newSelection), motionMode);",
        ""),
    "a page slides the highlight": (STATE, "ui_gameflow.c",
        "\tif(snapshot->snapTransition) {\n\t\tUIMotion_SpringSnap(&state->columnSpring,\n\t\t\tgridColumn(state, newSelection));\n\t}",
        "\tif(false) {\n\t}"),
    "Off leaves the highlight sliding": (STATE, "ui_gameflow.c",
        "\t\tUIMotion_SpringSnap(&state->columnSpring,\n\t\t\tstate->columnSpring.target);\n", ""),
}


class LayoutMutants(unittest.TestCase):
    def run_test(self, gui: Path, test: tuple, work: Path) -> int:
        source, units = test
        binary = work / "test"
        result = subprocess.run(shlex.split(os.environ.get("CC", "cc")) + FLAGS +
                                ["-I" + str(gui), str(TESTS / source)] +
                                [str(gui / unit) for unit in units] + ["-lm", "-o", str(binary)],
                                capture_output=True, text=True, timeout=120)
        if result.returncode:
            return -1   # a mutant that doesn't even build is caught too
        return subprocess.run([str(binary)], capture_output=True, timeout=60).returncode

    def test_the_real_sources_pass(self):
        with tempfile.TemporaryDirectory(prefix="swiss-layout-mutants-") as tmp:
            for test in (LIBRARY, STATE):
                self.assertEqual(self.run_test(GUI, test, Path(tmp)), 0, test[0])

    def test_every_mutant_is_caught(self):
        for name, (test, unit, old, new) in MUTANTS.items():
            with self.subTest(mutant=name), tempfile.TemporaryDirectory(
                    prefix="swiss-layout-mutants-") as tmp:
                gui = Path(tmp) / "gui"
                shutil.copytree(GUI, gui)
                text = (gui / unit).read_text()
                self.assertEqual(text.count(old), 1, f"mutation anchor missing: {name}")
                (gui / unit).write_text(text.replace(old, new))
                self.assertNotEqual(self.run_test(gui, test, Path(tmp)), 0,
                                    f"mutant escaped: {name}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
