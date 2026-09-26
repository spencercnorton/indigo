#!/usr/bin/env python3
"""Structural and mutation gate for Phase 4N cheat runtime safety."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[3]
CHEATS = ROOT / "cube/swiss/source/cheats/cheats.c"
POLICY = ROOT / "cube/swiss/source/cheats/cheat_policy.c"
POLICY_HEADER = ROOT / "cube/swiss/source/cheats/cheat_policy.h"
SWISS = ROOT / "cube/swiss/source/swiss.c"
SELECTOR = ROOT / "cube/swiss/source/gui/FrameBufferMagic.c"
TEST = Path(__file__).resolve().parent / "test_cheat_policy.c"
BUG_BASE = "621a2687cc4039058a373b52e8ffc22a9b66dcb6"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"cheat safety audit failed: {message}")


def git_show(path: str) -> str:
    return subprocess.check_output(
        ["git", "show", f"{BUG_BASE}:{path}"], cwd=ROOT, text=True
    )


def assert_bug_provenance() -> None:
    if subprocess.run(["git", "cat-file", "-e", f"{BUG_BASE}^{{commit}}"],
                      cwd=ROOT, capture_output=True).returncode:
        print("cheat safety audit: bug provenance skipped "
              "(its base commit predates this repository's history)")
        return
    old_cheats = git_show("cube/swiss/source/cheats/cheats.c")
    old_swiss = git_show("cube/swiss/source/swiss.c")
    old_selector = git_show("cube/swiss/source/gui/FrameBufferMagic.c")
    require("void kenobi_install_engine()" in old_cheats,
            "accepted base no longer contains unchecked installer")
    require("swiss/cheats/%.6s.chtsel" in old_cheats,
            "accepted base no longer contains six-ID selection key")
    require("swissSettings.wiirdDebug || getEnabledCheatsSize() > 0" in
            old_swiss,
            "accepted base no longer contains raw launch predicate")
    require("swissSettings.wiirdDebug ^=1;" in old_selector,
            "accepted base no longer contains unchecked debug toggle")


def assert_integration() -> None:
    cheats = CHEATS.read_text()
    policy = POLICY.read_text()
    swiss = SWISS.read_text()
    selector = SELECTOR.read_text()

    begin = cheats.index("if(!beginCheatDiscovery())")
    allocation = cheats.index(
        "file_handle *cheatsFile = calloc", begin
    )
    require(begin < allocation,
            "discovery does not reset state before its first allocation")
    require("disposeCheatEntries();" in cheats[cheats.index(
        "static bool beginCheatDiscovery"):begin],
        "discovery reset does not dispose the previous game's definitions")
    require("%.6s.chtsel" not in cheats,
            "active selection storage still uses a six-ID-only path")
    require("CheatIdentity_SelectionName" in cheats and
            "CheatIdentity_AllowsLegacySelection" in cheats,
            "keyed selection plus bounded legacy compatibility is not wired")
    require("selectionFile->size == expectedSize" in cheats and
            "enabledFlags[i] > 1u" in cheats,
            "selection loader lacks exact length/flag validation")
    require("CheatPolicy_SavedSelectionLoaded" in cheats and
            "CheatPolicy_ManualSelectionCommitted" in cheats,
            "display-only and manual origins are not distinct")
    require("CheatInstallWriter_Begin" in cheats and
            cheats.index("CheatInstallWriter_Begin") <
            cheats.index("memcpy(CHEATS_ENGINE"),
            "bounded writer is not preflighted before engine memory is touched")
    require("CheatInstallWriter_Append" in cheats and
            "CheatInstallWriter_Finish" in cheats,
            "installer bypasses the bounded writer")
    require("bool kenobi_install_engine(void)" in cheats,
            "installer does not report final safety failure")
    require("if(!kenobi_install_engine())" in swiss,
            "launch ignores final installer failure")
    require("getRuntimeEnabledCheatsCount()" in swiss and
            "cheatsShouldInstallEngine()" in swiss,
            "launch still bypasses policy-derived runtime state")
    require("bool cheatsFound = findCheats(true) > 0;" in swiss,
            "AutoCheats launch does not perform a fresh runtime discovery")
    require("getEnabledCheatsSize() > 0" not in swiss,
            "raw global enabled flags can still arm launch")
    require("swissSettings.wiirdDebug ^=1" not in selector and
            "cheatsCanEnableDebug()" in selector,
            "selector still permits an unchecked capacity shrink")
    require("%s_v%u_d%u.txt" in policy and "%s_v%u.txt" in policy and
            "%s.txt" in policy,
            "definition fallback order is incomplete")


def run_mutant(name: str, old: str, new: str) -> None:
    source = POLICY.read_text()
    require(source.count(old) == 1,
            f"mutation anchor {name!r} is not unique")
    mutant = source.replace(old, new, 1)
    with tempfile.TemporaryDirectory(prefix="cheat-policy-mutant-") as temp:
        temp_path = Path(temp)
        (temp_path / "cheat_policy.c").write_text(mutant)
        (temp_path / "cheat_policy.h").write_text(POLICY_HEADER.read_text())
        binary = temp_path / "test"
        command = [
            os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra",
            "-Werror", "-Wconversion", "-Wsign-conversion", "-pedantic",
            "-I", str(temp_path), "-o", str(binary), str(TEST),
            str(temp_path / "cheat_policy.c"),
        ]
        compiled = subprocess.run(command, cwd=ROOT, capture_output=True,
                                  text=True)
        require(compiled.returncode == 0,
                f"mutation {name!r} did not compile:\n{compiled.stderr}")
        executed = subprocess.run([str(binary)], cwd=ROOT,
                                  capture_output=True, text=True)
        require(executed.returncode != 0,
                f"tests survived bug-restoring mutation {name!r}")


def assert_mutations_killed() -> None:
    mutants = (
        (
            "saved selection bypasses AutoCheats",
            "state->origin == CHEAT_ORIGIN_SAVED && autoCheats",
            "state->origin == CHEAT_ORIGIN_SAVED || (autoCheats && false)",
        ),
        (
            "discovery miss preserves stale manual origin",
            "state->definitionKind = CHEAT_DEFINITION_NONE;\n\t\tstate->origin = CHEAT_ORIGIN_NONE;",
            "state->definitionKind = CHEAT_DEFINITION_NONE;\n\t\tstate->origin = CHEAT_ORIGIN_MANUAL;",
        ),
        (
            "identity ignores revision",
            "left->revision == right->revision && left->discId == right->discId",
            "left->discId == right->discId",
        ),
        (
            "identity ignores disc",
            "left->revision == right->revision && left->discId == right->discId",
            "left->revision == right->revision",
        ),
        (
            "selection key collapses to six-ID",
            'return formatName(name, nameCapacity, "%s_v%u_d%u.chtsel", identity);',
            'return formatName(name, nameCapacity, "%s.chtsel", identity);',
        ),
        (
            "debug exact-capacity request rejected",
            "enabledBytes <= capacityBytes;",
            "enabledBytes < capacityBytes;",
        ),
        (
            "debug-only launch retains non-installable status",
            "decision.status = wiirdDebug ? CHEAT_DECISION_READY :\n\t\t\tCHEAT_DECISION_NONE;",
            "decision.status = CHEAT_DECISION_NONE;",
        ),
        (
            "writer ignores maximum cheat payload",
            "destination == NULL || enabledBytes > maxCheatBytes ||",
            "destination == NULL || false ||",
        ),
        (
            "writer ignores short destination",
            "destinationBytes < requiredBytes ||",
            "(destinationBytes < requiredBytes && false) ||",
        ),
        (
            "writer accepts incomplete pair count",
            "writer->writtenPairs != writer->expectedPairs ||",
            "false ||",
        ),
    )
    for name, old, new in mutants:
        run_mutant(name, old, new)


def main() -> None:
    assert_bug_provenance()
    assert_integration()
    assert_mutations_killed()
    print("cheat safety integration and mutation audit passed")


if __name__ == "__main__":
    main()
