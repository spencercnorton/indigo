#ifndef __CHEAT_POLICY_H
#define __CHEAT_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CHEAT_GAME_ID_LENGTH 6u
#define CHEAT_DEFINITION_CANDIDATE_COUNT 3u
#define CHEAT_FILENAME_CAPACITY 32u

typedef struct {
	char gameId[CHEAT_GAME_ID_LENGTH + 1u];
	uint8_t revision;
	uint8_t discId;
} CheatIdentity;

typedef enum {
	CHEAT_DEFINITION_EXACT = 0,
	CHEAT_DEFINITION_REVISION = 1,
	CHEAT_DEFINITION_LEGACY = 2,
	CHEAT_DEFINITION_NONE = 3
} CheatDefinitionKind;

typedef enum {
	CHEAT_ORIGIN_NONE = 0,
	CHEAT_ORIGIN_SAVED,
	CHEAT_ORIGIN_MANUAL
} CheatSelectionOrigin;

typedef struct {
	CheatIdentity identity;
	bool identityValid;
	bool definitionLoaded;
	CheatDefinitionKind definitionKind;
	CheatSelectionOrigin origin;
} CheatPolicyState;

typedef enum {
	CHEAT_DECISION_NONE = 0,
	CHEAT_DECISION_READY,
	CHEAT_DECISION_IDENTITY_MISMATCH,
	CHEAT_DECISION_TOO_LARGE,
	CHEAT_DECISION_INVALID
} CheatDecisionStatus;

typedef struct {
	CheatDecisionStatus status;
	bool applyCodes;
	bool installEngine;
	size_t capacityBytes;
} CheatLaunchDecision;

typedef struct {
	uint32_t *begin;
	uint32_t *cursor;
	uint32_t *end;
	size_t expectedPairs;
	size_t writtenPairs;
	bool active;
} CheatInstallWriter;

bool CheatIdentity_Init(CheatIdentity *identity, const char *gameId,
	uint8_t revision, uint8_t discId);
bool CheatIdentity_Equals(const CheatIdentity *left,
	const CheatIdentity *right);
size_t CheatIdentity_DefinitionCandidates(const CheatIdentity *identity,
	char names[CHEAT_DEFINITION_CANDIDATE_COUNT][CHEAT_FILENAME_CAPACITY]);
bool CheatIdentity_SelectionName(const CheatIdentity *identity, char *name,
	size_t nameCapacity);
bool CheatIdentity_LegacySelectionName(const CheatIdentity *identity,
	char *name, size_t nameCapacity);
bool CheatIdentity_AllowsLegacySelection(const CheatIdentity *identity,
	CheatDefinitionKind definitionKind);

void CheatPolicy_BeginDiscovery(CheatPolicyState *state,
	const CheatIdentity *identity);
void CheatPolicy_DefinitionLoaded(CheatPolicyState *state,
	CheatDefinitionKind definitionKind);
void CheatPolicy_DiscoveryMiss(CheatPolicyState *state);
bool CheatPolicy_SavedSelectionLoaded(CheatPolicyState *state,
	const CheatIdentity *identity);
bool CheatPolicy_ManualSelectionCommitted(CheatPolicyState *state,
	const CheatIdentity *identity);
bool CheatPolicy_Capacity(size_t engineSpace, size_t engineSize,
	size_t *capacityBytes);
bool CheatPolicy_RequestDebug(size_t enabledBytes, size_t engineSpace,
	size_t debugEngineSize);
CheatLaunchDecision CheatPolicy_Decide(const CheatPolicyState *state,
	const CheatIdentity *currentIdentity, bool autoCheats, bool wiirdDebug,
	size_t enabledBytes, size_t engineSpace, size_t engineSize);

CheatDecisionStatus CheatInstallWriter_Begin(CheatInstallWriter *writer,
	void *destination, size_t destinationBytes, size_t enabledBytes,
	size_t maxCheatBytes);
CheatDecisionStatus CheatInstallWriter_Append(CheatInstallWriter *writer,
	uint32_t address, uint32_t value, uintptr_t engineStart);
CheatDecisionStatus CheatInstallWriter_Finish(CheatInstallWriter *writer,
	size_t *bytesWritten);

#endif
