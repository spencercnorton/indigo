# Host builds for the combined UI test suite (never part of the DOL build).
CC ?= cc
GUI := ../../../cube/swiss/source/gui
CHEATS := ../../../cube/swiss/source/cheats
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -g
SANFLAGS := -fsanitize=address,undefined -fno-sanitize-recover=all \
	-fno-omit-frame-pointer
# Hosts with vm.mmap_rnd_bits=32 (high ASLR entropy) break this: Debian PIE sanitizer executables can land
# inside ASan's shadow range and recursively fault before main; non-PIE keeps
# the sanitizer mapping deterministic. Darwin's linker does not use this flag.
ifeq ($(shell uname -s),Linux)
SANFLAGS += -fno-pie -no-pie
endif
GAMEFLOW_CFLAGS := -std=c11 -Wall -Wextra -Werror -Wconversion \
	-Wsign-conversion -pedantic

PLAIN_TARGETS := test_ui_cube_motif test_ui_game_history test_ui_cheats test_ui_saves test_ui_assets test_ui_assets_target_sync \
	test_ui_settings_layout test_ui_settings_focus test_ui_system_info \
	test_ui_menu_input test_ui_clock test_ui_hint \
	test_ui_home test_ui_presentation \
	test_ui_home_layout test_ui_home_safety test_ui_scene \
	test_ui_command_rail test_gameflow_state test_gameflow_library \
	test_gameflow_resolver test_gameflow_detail test_gameflow_ownership \
	test_cheat_policy

SANITIZED_TARGETS := test_ui_cube_motif_san test_ui_game_history_san test_ui_cheats_san test_ui_saves_san test_ui_assets_san test_ui_assets_target_sync_san \
	test_ui_settings_layout_san test_ui_settings_focus_san \
	test_ui_menu_input_san test_ui_clock_san test_ui_hint_san \
	test_ui_system_info_san test_ui_home_san test_ui_presentation_san \
	test_ui_home_layout_san test_ui_home_safety_san test_ui_scene_san \
	test_ui_command_rail_san test_gameflow_state_san \
	test_gameflow_library_san test_gameflow_resolver_san \
	test_gameflow_detail_san test_gameflow_ownership_san \
	test_cheat_policy_san

all: plain sanitized

plain: $(PLAIN_TARGETS)

sanitized: $(SANITIZED_TARGETS)

test_ui_cube_motif: test_ui_cube_motif.c $(GUI)/ui_cube_motif.c $(GUI)/ui_cube_motif.h $(GUI)/ui_home.c $(GUI)/ui_home.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_ui_cube_motif.c $(GUI)/ui_cube_motif.c $(GUI)/ui_home.c -lm

test_ui_cube_motif_san: test_ui_cube_motif.c $(GUI)/ui_cube_motif.c $(GUI)/ui_cube_motif.h $(GUI)/ui_home.c $(GUI)/ui_home.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ test_ui_cube_motif.c $(GUI)/ui_cube_motif.c $(GUI)/ui_home.c -lm

test_ui_game_history: test_ui_game_history.c $(GUI)/ui_game_history.c $(GUI)/ui_game_history.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_ui_game_history.c $(GUI)/ui_game_history.c

test_ui_game_history_san: test_ui_game_history.c $(GUI)/ui_game_history.c $(GUI)/ui_game_history.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ test_ui_game_history.c $(GUI)/ui_game_history.c

test_ui_cheats: test_ui_cheats.c $(GUI)/ui_cheats.c $(GUI)/ui_cheats.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_ui_cheats.c $(GUI)/ui_cheats.c

test_ui_cheats_san: test_ui_cheats.c $(GUI)/ui_cheats.c $(GUI)/ui_cheats.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ test_ui_cheats.c $(GUI)/ui_cheats.c

test_ui_saves: test_ui_saves.c $(GUI)/ui_saves.c $(GUI)/ui_saves.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_ui_saves.c $(GUI)/ui_saves.c

test_ui_saves_san: test_ui_saves.c $(GUI)/ui_saves.c $(GUI)/ui_saves.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ test_ui_saves.c $(GUI)/ui_saves.c

test_cheat_policy: test_cheat_policy.c $(CHEATS)/cheat_policy.c \
		$(CHEATS)/cheat_policy.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(CHEATS) -o $@ test_cheat_policy.c \
		$(CHEATS)/cheat_policy.c

test_cheat_policy_san: test_cheat_policy.c $(CHEATS)/cheat_policy.c \
		$(CHEATS)/cheat_policy.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(CHEATS) -o $@ \
		test_cheat_policy.c $(CHEATS)/cheat_policy.c

test_ui_assets: test_ui_assets.c $(GUI)/ui_assets.c $(GUI)/ui_assets.h
	$(CC) $(CFLAGS) -DUI_ASSETS_HOST_BUILD -I$(GUI) -o $@ \
		test_ui_assets.c $(GUI)/ui_assets.c -lz

test_ui_assets_san: test_ui_assets.c $(GUI)/ui_assets.c $(GUI)/ui_assets.h
	$(CC) $(CFLAGS) $(SANFLAGS) -DUI_ASSETS_HOST_BUILD -I$(GUI) -o $@ \
		test_ui_assets.c $(GUI)/ui_assets.c -lz

# Host-compiled policy variant: exercises the target requirement that Init
# receive a complete external critical-section pair without needing libogc.
test_ui_assets_target_sync: test_ui_assets.c $(GUI)/ui_assets.c $(GUI)/ui_assets.h
	$(CC) $(CFLAGS) -DUI_ASSETS_HOST_BUILD -DUI_ASSETS_HOST_REQUIRE_SYNC \
		-I$(GUI) -o $@ test_ui_assets.c $(GUI)/ui_assets.c -lz

test_ui_assets_target_sync_san: test_ui_assets.c $(GUI)/ui_assets.c $(GUI)/ui_assets.h
	$(CC) $(CFLAGS) $(SANFLAGS) -DUI_ASSETS_HOST_BUILD \
		-DUI_ASSETS_HOST_REQUIRE_SYNC -I$(GUI) -o $@ \
		test_ui_assets.c $(GUI)/ui_assets.c -lz

# Settings layout module: strict C99. ui_cheats is the cheat browser whose
# window the pages share.
test_ui_settings_layout: test_ui_settings_layout.c $(GUI)/ui_settings_layout.c $(GUI)/ui_settings_layout.h \
		$(GUI)/ui_hint.c $(GUI)/ui_hint.h $(GUI)/ui_cheats.c $(GUI)/ui_cheats.h
	$(CC) -std=c99 -Wall -Wextra -Werror -pedantic -I$(GUI) -o $@ \
		test_ui_settings_layout.c $(GUI)/ui_settings_layout.c $(GUI)/ui_hint.c \
		$(GUI)/ui_cheats.c

test_ui_settings_layout_san: test_ui_settings_layout.c \
		$(GUI)/ui_settings_layout.c $(GUI)/ui_settings_layout.h \
		$(GUI)/ui_hint.c $(GUI)/ui_hint.h $(GUI)/ui_cheats.c $(GUI)/ui_cheats.h
	$(CC) -std=c99 -Wall -Wextra -Werror -pedantic $(SANFLAGS) \
		-I$(GUI) -o $@ test_ui_settings_layout.c \
		$(GUI)/ui_settings_layout.c $(GUI)/ui_hint.c $(GUI)/ui_cheats.c

test_ui_menu_input: test_ui_menu_input.c $(GUI)/ui_menu_input.c \
		$(GUI)/ui_menu_input.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_ui_menu_input.c \
		$(GUI)/ui_menu_input.c

test_ui_menu_input_san: test_ui_menu_input.c $(GUI)/ui_menu_input.c \
		$(GUI)/ui_menu_input.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ \
		test_ui_menu_input.c $(GUI)/ui_menu_input.c

test_ui_settings_focus: test_ui_settings_focus.c $(GUI)/ui_settings_focus.c \
		$(GUI)/ui_settings_focus.h $(GUI)/ui_settings_layout.h \
		$(GUI)/ui_motion.c $(GUI)/ui_motion.h
	$(CC) -std=c11 -Wall -Wextra -Werror -Wconversion -Wsign-conversion \
		-pedantic -I$(GUI) -o $@ test_ui_settings_focus.c \
		$(GUI)/ui_settings_focus.c $(GUI)/ui_motion.c -lm

test_ui_settings_focus_san: test_ui_settings_focus.c $(GUI)/ui_settings_focus.c \
		$(GUI)/ui_settings_focus.h $(GUI)/ui_settings_layout.h \
		$(GUI)/ui_motion.c $(GUI)/ui_motion.h
	$(CC) -std=c11 -Wall -Wextra -Werror -Wconversion -Wsign-conversion \
		-pedantic $(SANFLAGS) -I$(GUI) -o $@ test_ui_settings_focus.c \
		$(GUI)/ui_settings_focus.c $(GUI)/ui_motion.c -lm

test_ui_system_info: test_ui_system_info.c $(GUI)/ui_system_info.c \
		$(GUI)/ui_system_info.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_ui_system_info.c \
		$(GUI)/ui_system_info.c

test_ui_system_info_san: test_ui_system_info.c $(GUI)/ui_system_info.c \
		$(GUI)/ui_system_info.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ \
		test_ui_system_info.c $(GUI)/ui_system_info.c

test_ui_hint: test_ui_hint.c $(GUI)/ui_hint.c $(GUI)/ui_hint.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_ui_hint.c $(GUI)/ui_hint.c

test_ui_hint_san: test_ui_hint.c $(GUI)/ui_hint.c $(GUI)/ui_hint.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ test_ui_hint.c $(GUI)/ui_hint.c

test_ui_clock: test_ui_clock.c $(GUI)/ui_clock.c $(GUI)/ui_clock.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_ui_clock.c \
		$(GUI)/ui_clock.c -lm

test_ui_clock_san: test_ui_clock.c $(GUI)/ui_clock.c $(GUI)/ui_clock.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ test_ui_clock.c \
		$(GUI)/ui_clock.c -lm

test_ui_home: test_ui_home.c $(GUI)/ui_home.c $(GUI)/ui_home.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_ui_home.c \
		$(GUI)/ui_home.c

test_ui_home_san: test_ui_home.c $(GUI)/ui_home.c $(GUI)/ui_home.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ test_ui_home.c \
		$(GUI)/ui_home.c

test_ui_home_safety: test_ui_home_safety.c $(GUI)/ui_home_safety.c \
		$(GUI)/ui_home_safety.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_ui_home_safety.c \
		$(GUI)/ui_home_safety.c

test_ui_home_safety_san: test_ui_home_safety.c $(GUI)/ui_home_safety.c \
		$(GUI)/ui_home_safety.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ \
		test_ui_home_safety.c $(GUI)/ui_home_safety.c

test_ui_home_layout: test_ui_home_layout.c $(GUI)/ui_home_layout.c \
		$(GUI)/ui_home_layout.h $(GUI)/ui_home_text.c \
		$(GUI)/ui_home_text.h $(GUI)/ui_home.c $(GUI)/ui_home.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_ui_home_layout.c \
		$(GUI)/ui_home_layout.c $(GUI)/ui_home_text.c $(GUI)/ui_home.c

test_ui_home_layout_san: test_ui_home_layout.c $(GUI)/ui_home_layout.c \
		$(GUI)/ui_home_layout.h $(GUI)/ui_home_text.c \
		$(GUI)/ui_home_text.h $(GUI)/ui_home.c $(GUI)/ui_home.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ \
		test_ui_home_layout.c $(GUI)/ui_home_layout.c \
		$(GUI)/ui_home_text.c $(GUI)/ui_home.c

test_ui_scene: test_ui_scene.c $(GUI)/ui_scene.c $(GUI)/ui_scene.h \
		$(GUI)/ui_motion.c $(GUI)/ui_motion.h $(GUI)/ui_home.c $(GUI)/ui_home.h \
		$(GUI)/ui_cube_motif.c $(GUI)/ui_cube_motif.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_ui_scene.c \
		$(GUI)/ui_scene.c $(GUI)/ui_motion.c $(GUI)/ui_home.c $(GUI)/ui_cube_motif.c -lm

test_ui_scene_san: test_ui_scene.c $(GUI)/ui_scene.c $(GUI)/ui_scene.h \
		$(GUI)/ui_motion.c $(GUI)/ui_motion.h $(GUI)/ui_home.c $(GUI)/ui_home.h \
		$(GUI)/ui_cube_motif.c $(GUI)/ui_cube_motif.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ \
		test_ui_scene.c $(GUI)/ui_scene.c $(GUI)/ui_motion.c $(GUI)/ui_home.c $(GUI)/ui_cube_motif.c -lm

test_ui_command_rail: test_ui_command_rail.c $(GUI)/ui_command_rail.c \
		$(GUI)/ui_command_rail.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_ui_command_rail.c \
		$(GUI)/ui_command_rail.c -lm

test_ui_command_rail_san: test_ui_command_rail.c $(GUI)/ui_command_rail.c \
		$(GUI)/ui_command_rail.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ \
		test_ui_command_rail.c $(GUI)/ui_command_rail.c -lm

test_ui_presentation: test_ui_presentation.c $(GUI)/ui_presentation.c \
		$(GUI)/ui_presentation.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_ui_presentation.c \
		$(GUI)/ui_presentation.c

test_ui_presentation_san: test_ui_presentation.c $(GUI)/ui_presentation.c \
		$(GUI)/ui_presentation.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ \
		test_ui_presentation.c $(GUI)/ui_presentation.c

# Retained Gameflow suites: strict C11, each plain and sanitized.
test_gameflow_state: test_gameflow_state.c $(GUI)/ui_gameflow.c \
		$(GUI)/ui_gameflow.h $(GUI)/ui_motion.c $(GUI)/ui_motion.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_gameflow_state.c \
		$(GUI)/ui_gameflow.c $(GUI)/ui_motion.c -lm

test_gameflow_state_san: test_gameflow_state.c $(GUI)/ui_gameflow.c \
		$(GUI)/ui_gameflow.h $(GUI)/ui_motion.c $(GUI)/ui_motion.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ \
		test_gameflow_state.c $(GUI)/ui_gameflow.c $(GUI)/ui_motion.c -lm

test_gameflow_library: test_gameflow_library.c \
		$(GUI)/ui_gameflow_library.c $(GUI)/ui_gameflow_library.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_gameflow_library.c \
		$(GUI)/ui_gameflow_library.c

test_gameflow_library_san: test_gameflow_library.c \
		$(GUI)/ui_gameflow_library.c $(GUI)/ui_gameflow_library.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ \
		test_gameflow_library.c $(GUI)/ui_gameflow_library.c

test_gameflow_resolver: test_gameflow_resolver.c \
		$(GUI)/ui_gameflow_resolver.c $(GUI)/ui_gameflow_resolver.h \
		$(GUI)/ui_gameflow_library.c $(GUI)/ui_gameflow_library.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_gameflow_resolver.c \
		$(GUI)/ui_gameflow_resolver.c $(GUI)/ui_gameflow_library.c

test_gameflow_resolver_san: test_gameflow_resolver.c \
		$(GUI)/ui_gameflow_resolver.c $(GUI)/ui_gameflow_resolver.h \
		$(GUI)/ui_gameflow_library.c $(GUI)/ui_gameflow_library.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ \
		test_gameflow_resolver.c $(GUI)/ui_gameflow_resolver.c \
		$(GUI)/ui_gameflow_library.c

test_gameflow_detail: test_gameflow_detail.c \
		$(GUI)/ui_gameflow_detail.c $(GUI)/ui_gameflow_detail.h $(GUI)/ui_game_history.c $(GUI)/ui_game_history.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_gameflow_detail.c \
		$(GUI)/ui_gameflow_detail.c $(GUI)/ui_game_history.c

test_gameflow_detail_san: test_gameflow_detail.c \
		$(GUI)/ui_gameflow_detail.c $(GUI)/ui_gameflow_detail.h $(GUI)/ui_game_history.c $(GUI)/ui_game_history.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ \
		test_gameflow_detail.c $(GUI)/ui_gameflow_detail.c $(GUI)/ui_game_history.c

test_gameflow_ownership: test_gameflow_ownership.c \
		$(GUI)/ui_gameflow_ownership.c $(GUI)/ui_gameflow_ownership.h
	$(CC) $(GAMEFLOW_CFLAGS) -I$(GUI) -o $@ test_gameflow_ownership.c \
		$(GUI)/ui_gameflow_ownership.c

test_gameflow_ownership_san: test_gameflow_ownership.c \
		$(GUI)/ui_gameflow_ownership.c $(GUI)/ui_gameflow_ownership.h
	$(CC) $(GAMEFLOW_CFLAGS) $(SANFLAGS) -I$(GUI) -o $@ \
		test_gameflow_ownership.c $(GUI)/ui_gameflow_ownership.c

clean:
	rm -f test_ui_game_history test_ui_game_history_san
	rm -rf test_ui_game_history*.dSYM
	rm -f test_ui_cheats test_ui_cheats_san
	rm -rf test_ui_cheats*.dSYM
	rm -f test_ui_saves test_ui_saves_san
	rm -rf test_ui_saves*.dSYM
	rm -f test_ui_assets test_ui_assets_san test_ui_assets_target_sync \
		test_ui_assets_target_sync_san test_ui_settings_layout \
		test_ui_settings_layout_san \
		test_ui_settings_focus test_ui_settings_focus_san \
		test_ui_system_info test_ui_system_info_san \
		test_ui_home test_ui_home_san test_ui_home_layout \
	test_ui_assets_target_sync_san test_ui_settings_layout \
	test_ui_settings_layout_san \
	test_ui_menu_input test_ui_menu_input_san \
	test_ui_settings_focus test_ui_settings_focus_san \
	test_ui_system_info test_ui_system_info_san \
	test_ui_clock test_ui_clock_san \
	test_ui_hint test_ui_hint_san \
	test_ui_home test_ui_home_san test_ui_home_layout \
		test_ui_home_layout_san test_ui_home_safety test_ui_home_safety_san \
		test_ui_scene test_ui_scene_san \
		test_ui_command_rail test_ui_command_rail_san \
		test_ui_presentation test_ui_presentation_san \
		test_gameflow_state \
		test_gameflow_state_san test_gameflow_library \
		test_gameflow_library_san test_gameflow_resolver \
		test_gameflow_resolver_san test_gameflow_detail \
		test_gameflow_detail_san test_gameflow_ownership \
		test_gameflow_ownership_san test_cheat_policy test_cheat_policy_san
	rm -rf test_ui_assets*.dSYM test_ui_settings_layout*.dSYM \
		test_gameflow_ownership_san
	rm -rf test_ui_assets*.dSYM test_ui_settings_layout*.dSYM \
		test_ui_menu_input*.dSYM \
		test_ui_settings_focus*.dSYM test_ui_home*.dSYM test_ui_scene*.dSYM \
		test_ui_system_info*.dSYM \
		test_ui_clock*.dSYM test_ui_hint*.dSYM \
		test_ui_command_rail*.dSYM test_ui_presentation*.dSYM \
		test_gameflow_*.dSYM

.PHONY: all plain sanitized clean
