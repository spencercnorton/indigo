#!/bin/sh
# check_ui_isolation.sh — guardrail #1 from docs/ui-redesign/DESIGN.md
#
# Fails if the diff against a base ref touches anything outside the UI
# allowlist. UI work must never reach device drivers, flash handlers
# (Wiikey .FZN), memcard I/O, the fragment/patch engine, or IPL code.
#
# usage: buildtools/check_ui_isolation.sh [base-ref]   (default: origin/master)

set -eu
BASE="${1:-origin/master}"

# Allowlist: UI/menu code, textures, nav, settings plumbing, docs, tooling.
# Everything else — notably cube/swiss/source/devices/**, patcher.c,
# patches/**, exi.c, aram/**, cube/ipl/**, wii/** — is out of bounds.
# deviceHandler.c has one mechanically checked reset-order exception: its reset
# priority may move from 0 to 1 so priority-0 UI teardown runs before mounted
# devices deinitialize. When the selected base contains the superseded Phase 4M
# reverse dependency, the same check also permits only deletion of that GUI
# include/callback. util.c likewise permits only deletion of its superseded GUI
# include/callback. No added dependency, callback, driver, or behavior change is
# accepted through either exception.
# main.c has one narrowly audited startup exception: a failed boot-device init
# clears DEVICE_CUR so Home cannot inherit a detected-but-unmounted handler.
# audit_routing_safety.py locks that hunk against the accepted base. No loader
# or driver behavior is allowed through this exception.
# video.c has one mechanical input exception: the existing post-retrace callback
# calls padsScan(), whose wrapper performs the same PAD_ScanPads call and
# atomically publishes its low-bit validity mask for menu navigation.
# cube/swiss/include/swiss.h allowed: it is SwissSettings' home (UI config
# flags live there); it contains no device/flash/write logic. wiiload.c is a
# narrow boot-handoff exception for stopping UI audio before sidestep resets
# ARAM; mp3.c/mp3.h expose only the UI player's AESND lifecycle state.
# Project-native CI and ignored host-test artifacts are validation tooling
# and cannot affect the target binary.
# .public-release.toml is the publication manifest: an allowlist of paths and
# hashes the exporter reads outside this repository. It is not compiled, not
# in DATA, and never read at runtime, so it cannot reach the target binary
# either — and every release that changes the published surface edits it.
# NOTICE and .github/ (issue templates, funding, code owners, the pull
# request template) are public-repository metadata the exporter publishes.
# Like the Markdown docs, none is compiled or read by the target binary.
# cheats.[ch] and cheat_policy.[ch] are narrow Game Detail/runtime safety
# exceptions. The latter is pure policy plus a bounded code-list writer; no
# device driver, patch engine, reserved-memory layout, or loader is admitted.
# input.h is admitted for the per-port menu-navigation policy (Phase 4O).
# cube/swiss/Makefile has one mechanically checked build exception: the tags.h
# rule may exclude the commit being built, so a release build stops embedding
# its own short hash twice (once as GIT_COMMIT, once in git_tags) and
# verify_dol.py keeps its exactly-one rule. No target, flag or source-list
# change passes through it.
# Makefile and cube/packer/Makefile have one mechanically checked build
# exception each, so the release zip can carry Indigo's own
# swiss/patches/apploader.img, the program In-Game Reset (Apploader) restarts:
# `make dev` also builds the existing packer, and the packer compresses with xz
# because the pinned libogc2 image has no 7z (same PowerPC BCJ + LZMA2 chain,
# CRC32 check). Neither changes swiss.dol, the packer's code, a driver, the
# patch engine or IPL code.
ALLOW='^(cube/swiss/source/gui/|cube/swiss/source/images/|cube/swiss/source/swiss\.[ch]$|cube/swiss/source/main\.c$|cube/swiss/include/swiss\.h$|cube/swiss/include/input\.h$|cube/swiss/include/mp3\.h$|cube/swiss/source/input\.c$|cube/swiss/source/mp3\.c$|cube/swiss/source/wiiload\.c$|cube/swiss/source/cheats/(cheats|cheat_policy)\.[ch]$|cube/swiss/source/config/|docs/|AGENTS/|buildtools/|\.gitlab-ci\.yml$|\.public-release\.toml$|\.gitignore$|NOTICE$|\.github/|[^/]*\.md$)'

CHANGED=$(git diff --name-only "$BASE"...HEAD --)
VIOLATIONS=$(printf '%s\n' "$CHANGED" | grep -Ev "$ALLOW" || true)

RESET_PATH='cube/swiss/source/devices/deviceHandler.c'
if printf '%s\n' "$VIOLATIONS" | grep -qx "$RESET_PATH"; then
	RESET_CHANGES=$(git diff --unified=0 "$BASE"...HEAD -- "$RESET_PATH" |
		sed -n '/^[+-]/p' | grep -Ev '^(---|\+\+\+)' || true)
	EXPECTED_RESET_PRIORITY=$(printf '%b\n' \
		'-\t{NULL, NULL}, onreset, 0' \
		'+\t{NULL, NULL}, onreset, 1')
	EXPECTED_RESET_CLEANUP=$(printf '%b\n' \
		'-#include "gui/FrameBufferMagic.h"' \
		'-\t\t/* A reset may bypass menu_loop and still deinitialize DEVICE_CUR. */' \
		'-\t\tDrawGameflowCancelPosters();' \
		'-\t{NULL, NULL}, onreset, 0' \
		'+\t{NULL, NULL}, onreset, 1')
	if [ "$RESET_CHANGES" = "$EXPECTED_RESET_PRIORITY" ] ||
		[ "$RESET_CHANGES" = "$EXPECTED_RESET_CLEANUP" ]; then
		VIOLATIONS=$(printf '%s\n' "$VIOLATIONS" |
			grep -vx "$RESET_PATH" || true)
	else
		echo "ISOLATION RESET EXCEPTION FAILED — unexpected deviceHandler.c diff:" >&2
		printf '%s\n' "$RESET_CHANGES" | sed 's/^/  /' >&2
	fi
fi

RECENT_PATH='cube/swiss/source/util.c'
if printf '%s\n' "$VIOLATIONS" | grep -qx "$RECENT_PATH"; then
	RECENT_CHANGES=$(git diff --unified=0 "$BASE"...HEAD -- "$RECENT_PATH" |
		sed -n '/^[+-]/p' | grep -Ev '^(---|\+\+\+)' || true)
	EXPECTED_RECENT_CLEANUP=$(printf '%b\n' \
		'-#include "gui/FrameBufferMagic.h"' \
		'-\t\t\t\t/* Recent-entry selection can switch devices without returning' \
		'-\t\t\t\t * through menu_loop'\''s normal device-change boundary. */' \
		'-\t\t\t\tDrawGameflowCancelPosters();')
	if [ "$RECENT_CHANGES" = "$EXPECTED_RECENT_CLEANUP" ]; then
		VIOLATIONS=$(printf '%s\n' "$VIOLATIONS" |
			grep -vx "$RECENT_PATH" || true)
	else
		echo "ISOLATION UTIL EXCEPTION FAILED — unexpected util.c diff:" >&2
		printf '%s\n' "$RECENT_CHANGES" | sed 's/^/  /' >&2
	fi
fi

TAGS_PATH='cube/swiss/Makefile'
if printf '%s\n' "$VIOLATIONS" | grep -qx "$TAGS_PATH"; then
	TAGS_CHANGES=$(git diff --unified=0 "$BASE"...HEAD -- "$TAGS_PATH" |
		sed -n '/^[+-]/p' | grep -Ev '^(---|\+\+\+)' || true)
	# A quoted here-document: every $ and quote below is literal.
	EXPECTED_TAGS_RULE=$(cat <<'TAGSEOF'
-	$(SILENTCMD)git for-each-ref --format='"%(objectname:short)",' refs/tags > $@
+	$(SILENTCMD)git for-each-ref --format='"%(objectname:short)",' refs/tags | grep -vx '"$(shell git rev-parse --short HEAD)",' > $@ || true
TAGSEOF
	)
	if [ "$TAGS_CHANGES" = "$EXPECTED_TAGS_RULE" ]; then
		VIOLATIONS=$(printf '%s\n' "$VIOLATIONS" |
			grep -vx "$TAGS_PATH" || true)
	else
		echo "ISOLATION TAGS EXCEPTION FAILED — unexpected cube/swiss/Makefile diff:" >&2
		printf '%s\n' "$TAGS_CHANGES" | sed 's/^/  /' >&2
	fi
fi

DEV_PATH='Makefile'
if printf '%s\n' "$VIOLATIONS" | grep -qx "$DEV_PATH"; then
	DEV_CHANGES=$(git diff --unified=0 "$BASE"...HEAD -- "$DEV_PATH" |
		sed -n '/^[+-]/p' | grep -Ev '^(---|\+\+\+)' || true)
	EXPECTED_DEV_TARGET=$(printf '%b\n' \
		'-dev: clean compile-patches compile' \
		'+dev: clean compile-patches compile compile-packer')
	if [ "$DEV_CHANGES" = "$EXPECTED_DEV_TARGET" ]; then
		VIOLATIONS=$(printf '%s\n' "$VIOLATIONS" |
			grep -vx "$DEV_PATH" || true)
	else
		echo "ISOLATION DEV EXCEPTION FAILED — unexpected Makefile diff:" >&2
		printf '%s\n' "$DEV_CHANGES" | sed 's/^/  /' >&2
	fi
fi

PACKER_PATH='cube/packer/Makefile'
if printf '%s\n' "$VIOLATIONS" | grep -qx "$PACKER_PATH"; then
	PACKER_CHANGES=$(git diff --unified=0 "$BASE"...HEAD -- "$PACKER_PATH" |
		sed -n '/^[+-]/p' | grep -Ev '^(---|\+\+\+)' || true)
	# A quoted here-document: every $ and quote below is literal.
	EXPECTED_PACKER_XZ=$(cat <<'PACKEREOF'
+# xz, not 7z: the pinned libogc2 image has no 7z. Same PowerPC BCJ + LZMA2 chain;
+# CRC32 because XZ Embedded here is built without CRC64.
-	$(SILENTCMD)7z a $@ $< -mf=ppc -mx=9
+	$(SILENTCMD)xz --format=xz --check=crc32 --powerpc --lzma2=preset=9e -T1 -c $< > $@
PACKEREOF
	)
	if [ "$PACKER_CHANGES" = "$EXPECTED_PACKER_XZ" ]; then
		VIOLATIONS=$(printf '%s\n' "$VIOLATIONS" |
			grep -vx "$PACKER_PATH" || true)
	else
		echo "ISOLATION PACKER EXCEPTION FAILED — unexpected cube/packer/Makefile diff:" >&2
		printf '%s\n' "$PACKER_CHANGES" | sed 's/^/  /' >&2
	fi
fi

VIDEO_PATH='cube/swiss/source/video.c'
if printf '%s\n' "$VIOLATIONS" | grep -qx "$VIDEO_PATH"; then
	VIDEO_CHANGES=$(git diff --unified=0 "$BASE"...HEAD -- "$VIDEO_PATH" |
		sed -n '/^[+-]/p' | grep -Ev '^(---|\+\+\+)' || true)
	EXPECTED_VIDEO_SCAN=$(printf '%b\n' \
		'-\tPAD_ScanPads();' \
		'+\tpadsScan();')
	if [ "$VIDEO_CHANGES" = "$EXPECTED_VIDEO_SCAN" ]; then
		VIOLATIONS=$(printf '%s\n' "$VIOLATIONS" |
			grep -vx "$VIDEO_PATH" || true)
	else
		echo "ISOLATION VIDEO EXCEPTION FAILED — unexpected video.c diff:" >&2
		printf '%s\n' "$VIDEO_CHANGES" | sed 's/^/  /' >&2
	fi
fi

if [ -n "$VIOLATIONS" ]; then
    echo "ISOLATION GATE FAILED — diff touches non-UI paths:" >&2
    echo "$VIOLATIONS" | sed 's/^/  /' >&2
    echo "See docs/ui-redesign/DESIGN.md §4. If a path genuinely belongs in" >&2
    echo "UI scope, update the allowlist in this script in the same MR and" >&2
    echo "justify it in the MR description." >&2
    exit 1
fi
if [ -n "$CHANGED" ]; then
	CHANGED_COUNT=$(printf '%s\n' "$CHANGED" | wc -l | tr -d ' ')
else
	CHANGED_COUNT=0
fi
echo "isolation gate OK ($CHANGED_COUNT files, all within UI scope)"
