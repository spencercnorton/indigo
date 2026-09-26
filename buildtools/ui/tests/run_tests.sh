#!/bin/sh
# run_tests.sh -- deterministic host suite with independently schedulable lanes.
#
# Usage: run_tests.sh [all|plain|sanitized|contracts]
# Needs: Python 3 + Pillow, a C compiler, zlib, and (when strict) gxtexconv.
set -eu
cd "$(dirname "$0")"

SUITE="${1:-all}"
case "$SUITE" in
	all|plain|sanitized|contracts) ;;
	*)
		echo "usage: $0 [all|plain|sanitized|contracts]" >&2
		exit 2
		;;
esac

# The structural audits intentionally use Python assertions today. Never let
# PYTHONOPTIMIZE silently turn those checks into no-ops.
python3 - <<'PY'
import sys

if not __debug__:
    print("host suite refuses optimized Python; assertions would be disabled", file=sys.stderr)
    raise SystemExit(2)
PY

detect_jobs() {
	if command -v nproc >/dev/null 2>&1; then
		nproc
	elif command -v getconf >/dev/null 2>&1; then
		getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2
	else
		echo 2
	fi
}

JOBS="${UI_TEST_JOBS:-$(detect_jobs)}"
if ! printf '%s\n' "$JOBS" | grep -Eqx '0*[1-9][0-9]*'; then
	echo "UI_TEST_JOBS must be a positive integer (got: $JOBS)" >&2
	exit 2
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

build_binaries() {
	target=$1
	echo "== clean parallel host build: $target ($JOBS jobs) =="
	make -s clean
	make -s -j "$JOBS" "$target"
}

make_fixture() {
	if python3 fixture_pack.py "$TMP/fixture.pak"; then
		return 0
	else
		status=$?
	fi
	if [ "$status" -ne 3 ]; then
		return "$status"
	fi
	if [ "${UI_TEST_REQUIRE_GXTEXCONV:-0}" = "1" ]; then
		echo "real-pak pass required, but gxtexconv is unavailable" >&2
		return 3
	fi
	echo "real-pak pass skipped (gxtexconv unavailable; set UI_TEST_REQUIRE_GXTEXCONV=1 to fail)"
	return 3
}

run_plain() {
	echo "== cheat menu presentation and navigation (plain) =="
	./test_ui_cheats
	echo "== Memory Cards save files and destinations (plain) =="
	./test_ui_saves
	echo "== ui_assets runtime (plain + target-sync policy) =="
	if make_fixture; then
		./test_ui_assets "$TMP/fixture.pak"
		./test_ui_assets_target_sync "$TMP/fixture.pak"
	else
		status=$?
		if [ "$status" -ne 3 ] || [ "${UI_TEST_REQUIRE_GXTEXCONV:-0}" = "1" ]; then
			return "$status"
		fi
		./test_ui_assets
		./test_ui_assets_target_sync
	fi

	echo "== settings layout and focus (plain) =="
	./test_ui_settings_layout
	./test_ui_settings_focus

	echo "== System Information presentation (plain) =="
	./test_ui_system_info

	echo "== per-port menu input policy and live clock geometry (plain) =="
	./test_ui_menu_input
	./test_ui_clock
	./test_ui_hint

	echo "== Home reducer, lifecycle, layout, scene, and command rail (plain) =="
	./test_ui_home
	./test_ui_cube_motif
	./test_ui_home_safety
	./test_ui_home_layout
	./test_ui_scene
	./test_ui_command_rail

	echo "== shared retained presentation states (plain) =="
	./test_ui_presentation

	echo "== retained Gameflow (plain) =="
	./test_gameflow_state
	./test_gameflow_library
	./test_gameflow_resolver
	./test_gameflow_detail
	./test_ui_game_history
	./test_gameflow_ownership

	echo "== cheat identity, launch policy, and bounded writer (plain) =="
	./test_cheat_policy
}

run_sanitized() {
	echo "== cheat menu presentation and navigation (ASan/UBSan) =="
	./test_ui_cheats_san
	echo "== Memory Cards save files and destinations (ASan/UBSan) =="
	./test_ui_saves_san
	echo "== ui_assets runtime (ASan/UBSan + target-sync policy) =="
	if make_fixture; then
		./test_ui_assets_san "$TMP/fixture.pak"
		./test_ui_assets_target_sync_san "$TMP/fixture.pak"
	else
		status=$?
		if [ "$status" -ne 3 ] || [ "${UI_TEST_REQUIRE_GXTEXCONV:-0}" = "1" ]; then
			return "$status"
		fi
		./test_ui_assets_san
		./test_ui_assets_target_sync_san
	fi

	echo "== settings layout and focus (ASan/UBSan) =="
	./test_ui_settings_layout_san
	./test_ui_settings_focus_san

	echo "== System Information presentation (ASan/UBSan) =="
	./test_ui_system_info_san

	echo "== per-port menu input policy and live clock geometry (ASan/UBSan) =="
	./test_ui_menu_input_san
	./test_ui_clock_san
	./test_ui_hint_san

	echo "== Home reducer, lifecycle, layout, scene, and command rail (ASan/UBSan) =="
	./test_ui_home_san
	./test_ui_cube_motif_san
	./test_ui_home_safety_san
	./test_ui_home_layout_san
	./test_ui_scene_san
	./test_ui_command_rail_san

	echo "== shared retained presentation states (ASan/UBSan) =="
	./test_ui_presentation_san

	echo "== retained Gameflow (ASan/UBSan) =="
	./test_gameflow_state_san
	./test_gameflow_library_san
	./test_gameflow_resolver_san
	./test_gameflow_detail_san
	./test_ui_game_history_san
	./test_gameflow_ownership_san

	echo "== cheat identity, launch policy, and bounded writer (ASan/UBSan) =="
	./test_cheat_policy_san
}

run_contracts() {
	echo "== play-history device faults and handoff boundaries =="
	python3 ./test_history_persistence.py
	echo "== cheat panel GX vertex stream and geometry =="
	python3 ./test_cheats_gx_stream.py
	echo "== native stroke GX stream and perspective coverage =="
	python3 ./test_stroke_gx_stream.py
	echo "== wave and grid native coverage =="
	python3 ./test_background_gx_stream.py
	echo "== cube glass light: refraction, dispersion, bloom, rim, glint =="
	python3 ./test_glass_light.py
	echo "== display copy clear and poster retreat =="
	python3 ./test_frame_copy_clear.py
	echo "== Library layouts: GX stream, poses, motion, Horizontal unchanged =="
	python3 ./test_gameflow_gx_stream.py
	echo "== Library layouts: navigation and state mutants =="
	python3 ./test_gameflow_layout_mutants.py
	echo "== cube orientation and visible face binding =="
	python3 ./test_cube_render_pose.py
	echo "== settings files: real parser/writer vs docs/SETTINGS.md =="
	python3 ./test_settings_file.py
	echo "== settings views: every setting in exactly one view =="
	python3 ./test_settings_views.py
	echo "== Menu Color: Indigo's colors turn, meanings and neutrals stay =="
	python3 ./test_ui_color.py
	echo "== settings semantics audit =="
	./audit_settings_semantics.sh

	echo "== Game Detail safety audit =="
	python3 ./audit_game_detail_safety.py

	echo "== retained Detail presentation audit =="
	python3 ./audit_detail_presentation.py

	echo "== shared retained presentation audit =="
	python3 ./audit_shared_presentation.py

	echo "== retained System Information presentation audit =="
	python3 ./audit_system_info_presentation.py

	echo "== cheat runtime integration and mutation audit =="
	python3 ./audit_cheat_safety.py

	echo "== Memory Cards copy, move and delete safety audit =="
	python3 ./audit_saves_safety.py

	echo "== strict Game Library dispatch audit =="
	python3 ./audit_gameflow_dispatch.py

	echo "== hardware Alpha V1 Home polish audit =="
	python3 ./audit_home_polish.py

	echo "== Settings hardware presentation/input audit =="
	python3 ./audit_settings_hardware_followup.py
	python3 -O ./audit_settings_hardware_followup.py

	echo "== Home hardware follow-up audit =="
	python3 ./audit_home_hardware_followup.py

	echo "== semantic four-face Home integration audit =="
	python3 ./audit_four_face_home.py

	echo "== browser to Home event ownership =="
	python3 ./test_browser_home_lifecycle.py

	echo "== routing lifecycle mutation audit =="
	python3 ./audit_routing_safety.py
}

case "$SUITE" in
	all)
		echo "== poster_pack generator tests =="
		python3 -m unittest -v test_poster_pack
		build_binaries all
		run_plain
		run_sanitized
		run_contracts
		;;
	plain)
		echo "== poster_pack generator tests =="
		python3 -m unittest -v test_poster_pack
		build_binaries plain
		run_plain
		;;
	sanitized)
		build_binaries sanitized
		run_sanitized
		;;
	contracts)
		run_contracts
		;;
esac

echo "$SUITE host suite passed"
