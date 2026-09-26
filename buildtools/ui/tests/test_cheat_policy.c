#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cheat_policy.h"

#define ENGINE_SPACE 0x2000u
#define NORMAL_ENGINE_SIZE 2736u
#define DEBUG_ENGINE_SIZE 4288u
#define NORMAL_CAPACITY 5448u
#define DEBUG_CAPACITY 3896u
#define CANARY 0xA5u

static int failures;

#define CHECK(condition) do { \
	if(!(condition)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, \
			#condition); \
		failures++; \
	} \
} while(0)

static int compareNames(const void *left, const void *right)
{
	return strcmp((const char *)left, (const char *)right);
}

static CheatIdentity identity(const char *gameId, uint8_t revision,
	uint8_t discId)
{
	CheatIdentity value;
	CHECK(CheatIdentity_Init(&value, gameId, revision, discId));
	return value;
}

static CheatPolicyState loadedState(const CheatIdentity *value,
	CheatSelectionOrigin origin)
{
	CheatPolicyState state;
	CheatPolicy_BeginDiscovery(&state, value);
	CheatPolicy_DefinitionLoaded(&state, CHEAT_DEFINITION_LEGACY);
	if(origin == CHEAT_ORIGIN_SAVED) {
		CHECK(CheatPolicy_SavedSelectionLoaded(&state, value));
	}
	else if(origin == CHEAT_ORIGIN_MANUAL) {
		CHECK(CheatPolicy_ManualSelectionCommitted(&state, value));
	}
	return state;
}

static void testIdentityPaths(void)
{
	CheatIdentity melee = identity("GALE01", 2u, 0u);
	CheatIdentity other;
	char candidates[CHEAT_DEFINITION_CANDIDATE_COUNT]
		[CHEAT_FILENAME_CAPACITY];
	char selection[CHEAT_FILENAME_CAPACITY];
	char legacy[CHEAT_FILENAME_CAPACITY];
	char tooSmall[8];
	char (*allNames)[CHEAT_FILENAME_CAPACITY];
	size_t index = 0u;
	unsigned int revision;
	unsigned int disc;

	CHECK(CheatIdentity_DefinitionCandidates(&melee, candidates) == 3u);
	CHECK(strcmp(candidates[0], "GALE01_v102_d1.txt") == 0);
	CHECK(strcmp(candidates[1], "GALE01_v102.txt") == 0);
	CHECK(strcmp(candidates[2], "GALE01.txt") == 0);
	CHECK(CheatIdentity_SelectionName(&melee, selection,
		sizeof(selection)));
	CHECK(strcmp(selection, "GALE01_v102_d1.chtsel") == 0);
	CHECK(CheatIdentity_LegacySelectionName(&melee, legacy,
		sizeof(legacy)));
	CHECK(strcmp(legacy, "GALE01.chtsel") == 0);
	memset(tooSmall, CANARY, sizeof(tooSmall));
	CHECK(!CheatIdentity_SelectionName(&melee, tooSmall,
		sizeof(tooSmall)));
	CHECK(tooSmall[0] == '\0');
	CHECK(CheatIdentity_AllowsLegacySelection(&melee,
		CHEAT_DEFINITION_LEGACY));
	CHECK(!CheatIdentity_AllowsLegacySelection(&melee,
		CHEAT_DEFINITION_REVISION));

	CHECK(!CheatIdentity_Init(&other, "gALE01", 0u, 0u));
	CHECK(!CheatIdentity_Init(&other, "GA/E01", 0u, 0u));
	CHECK(!CheatIdentity_Init(&other, "GALE", 0u, 0u));
	other = identity("GALE01", 2u, 1u);
	CHECK(!CheatIdentity_AllowsLegacySelection(&other,
		CHEAT_DEFINITION_LEGACY));
	CHECK(!CheatIdentity_Equals(&melee, &other));
	other = identity("GALE01", 1u, 0u);
	CHECK(!CheatIdentity_Equals(&melee, &other));

	allNames = calloc(256u * 256u, sizeof(*allNames));
	CHECK(allNames != NULL);
	if(allNames == NULL) {
		return;
	}
	for(revision = 0u; revision <= UINT8_MAX; ++revision) {
		for(disc = 0u; disc <= UINT8_MAX; ++disc) {
			CheatIdentity value = identity("GALE01", (uint8_t)revision,
				(uint8_t)disc);
			CHECK(CheatIdentity_SelectionName(&value, allNames[index],
				CHEAT_FILENAME_CAPACITY));
			index++;
		}
	}
	CHECK(index == 65536u);
	qsort(allNames, index, sizeof(*allNames), compareNames);
	for(index = 1u; index < 65536u; ++index) {
		CHECK(strcmp(allNames[index - 1u], allNames[index]) != 0);
	}
	free(allNames);
}

static void testCapacityAndDecisions(void)
{
	CheatIdentity game = identity("GM4E01", 0u, 0u);
	CheatIdentity otherRevision = identity("GM4E01", 1u, 0u);
	CheatIdentity otherDisc = identity("GM4E01", 0u, 1u);
	CheatPolicyState state;
	CheatLaunchDecision decision;
	size_t capacity = 0u;
	int origin;
	int autoCheats;
	int debug;
	size_t sizes[] = {0u, 8u, DEBUG_CAPACITY, DEBUG_CAPACITY + 8u};
	size_t sizeIndex;

	CHECK(CheatPolicy_Capacity(ENGINE_SPACE, NORMAL_ENGINE_SIZE,
		&capacity));
	CHECK(capacity == NORMAL_CAPACITY);
	CHECK(CheatPolicy_Capacity(ENGINE_SPACE, DEBUG_ENGINE_SIZE,
		&capacity));
	CHECK(capacity == DEBUG_CAPACITY);
	CHECK(!CheatPolicy_Capacity(8u, 1u, &capacity));
	CHECK(!CheatPolicy_Capacity(8u, 9u, &capacity));
	CHECK(!CheatPolicy_Capacity(ENGINE_SPACE, NORMAL_ENGINE_SIZE, NULL));
	CHECK(CheatPolicy_RequestDebug(DEBUG_CAPACITY, ENGINE_SPACE,
		DEBUG_ENGINE_SIZE));
	CHECK(!CheatPolicy_RequestDebug(DEBUG_CAPACITY + 8u, ENGINE_SPACE,
		DEBUG_ENGINE_SIZE));

	for(origin = CHEAT_ORIGIN_NONE; origin <= CHEAT_ORIGIN_MANUAL;
		++origin) {
		for(autoCheats = 0; autoCheats <= 1; ++autoCheats) {
			for(debug = 0; debug <= 1; ++debug) {
				for(sizeIndex = 0u;
					sizeIndex < sizeof(sizes) / sizeof(sizes[0]);
					++sizeIndex) {
					bool expectedApply;
					size_t engineSize = debug ? DEBUG_ENGINE_SIZE :
						NORMAL_ENGINE_SIZE;
					state = loadedState(&game,
						(CheatSelectionOrigin)origin);
					decision = CheatPolicy_Decide(&state, &game,
						autoCheats != 0, debug != 0, sizes[sizeIndex],
						ENGINE_SPACE, engineSize);
					expectedApply = sizes[sizeIndex] > 0u &&
						(origin == CHEAT_ORIGIN_MANUAL ||
						(origin == CHEAT_ORIGIN_SAVED && autoCheats));
					if(expectedApply && sizes[sizeIndex] >
						decision.capacityBytes) {
						CHECK(decision.status == CHEAT_DECISION_TOO_LARGE);
						CHECK(!decision.applyCodes);
						CHECK(!decision.installEngine);
					}
					else {
						CHECK(decision.applyCodes == expectedApply);
						CHECK(decision.installEngine ==
							(debug != 0 || expectedApply));
					}
				}
			}
		}
	}

	state = loadedState(&game, CHEAT_ORIGIN_MANUAL);
	decision = CheatPolicy_Decide(&state, &otherRevision, true, false,
		8u, ENGINE_SPACE, NORMAL_ENGINE_SIZE);
	CHECK(decision.status == CHEAT_DECISION_IDENTITY_MISMATCH);
	CHECK(!decision.applyCodes && !decision.installEngine);
	decision = CheatPolicy_Decide(&state, &otherDisc, true, true,
		8u, ENGINE_SPACE, DEBUG_ENGINE_SIZE);
	CHECK(decision.status == CHEAT_DECISION_IDENTITY_MISMATCH);
	CHECK(!decision.applyCodes && !decision.installEngine);

	CheatPolicy_BeginDiscovery(&state, &game);
	CheatPolicy_DefinitionLoaded(&state, CHEAT_DEFINITION_LEGACY);
	CHECK(CheatPolicy_ManualSelectionCommitted(&state, &game));
	CheatPolicy_BeginDiscovery(&state, &otherRevision);
	CheatPolicy_DiscoveryMiss(&state);
	decision = CheatPolicy_Decide(&state, &otherRevision, true, false,
		24u, ENGINE_SPACE, NORMAL_ENGINE_SIZE);
	CHECK(decision.status == CHEAT_DECISION_NONE);
	CHECK(state.origin == CHEAT_ORIGIN_NONE);
	CHECK(!state.definitionLoaded);
	decision = CheatPolicy_Decide(&state, &otherRevision, false, true,
		24u, ENGINE_SPACE, DEBUG_ENGINE_SIZE);
	CHECK(decision.status == CHEAT_DECISION_READY);
	CHECK(!decision.applyCodes);
	CHECK(decision.installEngine);
}

static void appendPairs(CheatInstallWriter *writer, size_t pairCount)
{
	size_t i;
	for(i = 0u; i < pairCount; ++i) {
		CHECK(CheatInstallWriter_Append(writer, (uint32_t)i,
			(uint32_t)(i + 1u), (uintptr_t)0x817FE000u) ==
			CHEAT_DECISION_READY);
	}
}

static void testWriterBoundary(size_t capacityBytes)
{
	size_t listBytes = 8u + capacityBytes + 4u;
	size_t allocationBytes = listBytes + 8u;
	uint8_t *allocation = malloc(allocationBytes);
	void *destination;
	CheatInstallWriter writer;
	size_t written = 0u;

	CHECK(allocation != NULL);
	if(allocation == NULL) {
		return;
	}
	memset(allocation, CANARY, allocationBytes);
	destination = allocation + 4u;
	CHECK(CheatInstallWriter_Begin(&writer, destination, listBytes,
		capacityBytes, capacityBytes) == CHEAT_DECISION_READY);
	CHECK(allocation[4] == CANARY);
	appendPairs(&writer, capacityBytes / 8u);
	CHECK(CheatInstallWriter_Finish(&writer, &written) ==
		CHEAT_DECISION_READY);
	CHECK(written == listBytes);
	CHECK(allocation[0] == CANARY && allocation[3] == CANARY);
	CHECK(allocation[allocationBytes - 4u] == CANARY &&
		allocation[allocationBytes - 1u] == CANARY);
	free(allocation);
}

static void testInstallWriter(void)
{
	uint8_t buffer[64];
	uint8_t before[64];
	CheatInstallWriter writer;
	size_t written = 0u;
	uint32_t *words = (uint32_t *)(void *)buffer;

	testWriterBoundary(NORMAL_CAPACITY);
	testWriterBoundary(DEBUG_CAPACITY);

	memset(buffer, CANARY, sizeof(buffer));
	memcpy(before, buffer, sizeof(buffer));
	CHECK(CheatInstallWriter_Begin(&writer, buffer, sizeof(buffer),
		DEBUG_CAPACITY + 8u, DEBUG_CAPACITY) == CHEAT_DECISION_TOO_LARGE);
	CHECK(memcmp(buffer, before, sizeof(buffer)) == 0);
	CHECK(CheatInstallWriter_Begin(&writer, buffer, sizeof(buffer), 16u, 8u) ==
		CHEAT_DECISION_TOO_LARGE);
	CHECK(memcmp(buffer, before, sizeof(buffer)) == 0);
	CHECK(CheatInstallWriter_Begin(&writer, buffer, 16u, 8u, 8u) ==
		CHEAT_DECISION_TOO_LARGE);
	CHECK(memcmp(buffer, before, sizeof(buffer)) == 0);
	CHECK(CheatInstallWriter_Begin(&writer, buffer, sizeof(buffer), 7u,
		8u) == CHEAT_DECISION_INVALID);
	CHECK(memcmp(buffer, before, sizeof(buffer)) == 0);
	CHECK(CheatInstallWriter_Begin(&writer, buffer, sizeof(buffer),
		SIZE_MAX, SIZE_MAX) == CHEAT_DECISION_INVALID);
	CHECK(memcmp(buffer, before, sizeof(buffer)) == 0);

	CHECK(CheatInstallWriter_Begin(&writer, buffer, sizeof(buffer), 8u,
		8u) == CHEAT_DECISION_READY);
	CHECK(memcmp(buffer, before, sizeof(buffer)) == 0);
	CHECK(CheatInstallWriter_Append(&writer, 0x12345678u, 0x800018A8u,
		(uintptr_t)0x817FE000u) == CHEAT_DECISION_READY);
	CHECK(CheatInstallWriter_Finish(&writer, &written) ==
		CHEAT_DECISION_READY);
	CHECK(words[0] == 0x00D0C0DEu && words[1] == 0x00D0C0DEu);
	CHECK(words[2] == 0x12345678u);
	CHECK(words[3] == 0x817FE0A8u);
	CHECK(words[4] == 0xFF000000u);
	CHECK(written == 20u);

	CHECK(CheatInstallWriter_Begin(&writer, buffer, sizeof(buffer), 16u,
		16u) == CHEAT_DECISION_READY);
	CHECK(CheatInstallWriter_Append(&writer, 1u, 2u,
		(uintptr_t)0x817FE000u) == CHEAT_DECISION_READY);
	CHECK(CheatInstallWriter_Finish(&writer, &written) ==
		CHEAT_DECISION_INVALID);
	CHECK(CheatInstallWriter_Append(&writer, 3u, 4u,
		(uintptr_t)0x817FE000u) == CHEAT_DECISION_READY);
	CHECK(CheatInstallWriter_Append(&writer, 5u, 6u,
		(uintptr_t)0x817FE000u) == CHEAT_DECISION_INVALID);
	CHECK(CheatInstallWriter_Finish(&writer, &written) ==
		CHEAT_DECISION_READY);

	CHECK(CheatInstallWriter_Begin(&writer, buffer, sizeof(buffer), 0u,
		0u) == CHEAT_DECISION_READY);
	CHECK(CheatInstallWriter_Finish(&writer, &written) ==
		CHEAT_DECISION_READY);
	CHECK(written == 12u);
}

int main(void)
{
	testIdentityPaths();
	testCapacityAndDecisions();
	testInstallWriter();
	if(failures != 0) {
		fprintf(stderr, "%d cheat policy checks failed\n", failures);
		return 1;
	}
	puts("cheat policy checks passed");
	return 0;
}
