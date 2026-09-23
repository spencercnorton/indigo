#include "cheat_policy.h"

#include <stdio.h>
#include <string.h>

#define CHEAT_LIST_MARKER 0x00D0C0DEu
#define CHEAT_LIST_TERMINATOR 0xFF000000u
#define CHEAT_OLD_ENGINE_JUMP 0x800018A8u
#define CHEAT_ENGINE_JUMP_OFFSET 0xA8u
#define CHEAT_ENGINE_OVERHEAD 8u
#define CHEAT_PAIR_BYTES 8u
#define CHEAT_LIST_MARKER_BYTES 8u
#define CHEAT_LIST_TERMINATOR_BYTES 4u

static bool isGameIdCharacter(char value)
{
	return (value >= 'A' && value <= 'Z') ||
		(value >= '0' && value <= '9');
}

static bool formatName(char *name, size_t nameCapacity, const char *format,
	const CheatIdentity *identity)
{
	int length;
	unsigned int discNumber;
	unsigned int revisionNumber;

	if(name == NULL || nameCapacity == 0u || format == NULL ||
		identity == NULL) {
		return false;
	}
	discNumber = (unsigned int)identity->discId + 1u;
	revisionNumber = (unsigned int)identity->revision + 100u;
	length = snprintf(name, nameCapacity, format, identity->gameId,
		revisionNumber, discNumber);
	if(length < 0 || (size_t)length >= nameCapacity) {
		name[0] = '\0';
		return false;
	}
	return true;
}

bool CheatIdentity_Init(CheatIdentity *identity, const char *gameId,
	uint8_t revision, uint8_t discId)
{
	size_t i;

	if(identity == NULL || gameId == NULL) {
		return false;
	}
	memset(identity, 0, sizeof(*identity));
	for(i = 0u; i < CHEAT_GAME_ID_LENGTH; ++i) {
		if(!isGameIdCharacter(gameId[i])) {
			return false;
		}
	}
	memcpy(identity->gameId, gameId, CHEAT_GAME_ID_LENGTH);
	identity->gameId[CHEAT_GAME_ID_LENGTH] = '\0';
	identity->revision = revision;
	identity->discId = discId;
	return true;
}

bool CheatIdentity_Equals(const CheatIdentity *left,
	const CheatIdentity *right)
{
	return left != NULL && right != NULL &&
		left->revision == right->revision && left->discId == right->discId &&
		memcmp(left->gameId, right->gameId, CHEAT_GAME_ID_LENGTH) == 0;
}

size_t CheatIdentity_DefinitionCandidates(const CheatIdentity *identity,
	char names[CHEAT_DEFINITION_CANDIDATE_COUNT][CHEAT_FILENAME_CAPACITY])
{
	int length;
	unsigned int revisionNumber;

	if(identity == NULL || names == NULL) {
		return 0u;
	}
	memset(names, 0, CHEAT_DEFINITION_CANDIDATE_COUNT *
		CHEAT_FILENAME_CAPACITY);
	if(!formatName(names[CHEAT_DEFINITION_EXACT], CHEAT_FILENAME_CAPACITY,
		"%s_v%u_d%u.txt", identity)) {
		return 0u;
	}
	revisionNumber = (unsigned int)identity->revision + 100u;
	length = snprintf(names[CHEAT_DEFINITION_REVISION],
		CHEAT_FILENAME_CAPACITY, "%s_v%u.txt", identity->gameId,
		revisionNumber);
	if(length < 0 || (size_t)length >= CHEAT_FILENAME_CAPACITY) {
		return 0u;
	}
	length = snprintf(names[CHEAT_DEFINITION_LEGACY],
		CHEAT_FILENAME_CAPACITY, "%s.txt", identity->gameId);
	if(length < 0 || (size_t)length >= CHEAT_FILENAME_CAPACITY) {
		return 0u;
	}
	return CHEAT_DEFINITION_CANDIDATE_COUNT;
}

bool CheatIdentity_SelectionName(const CheatIdentity *identity, char *name,
	size_t nameCapacity)
{
	return formatName(name, nameCapacity, "%s_v%u_d%u.chtsel", identity);
}

bool CheatIdentity_LegacySelectionName(const CheatIdentity *identity,
	char *name, size_t nameCapacity)
{
	int length;

	if(name == NULL || nameCapacity == 0u || identity == NULL) {
		return false;
	}
	length = snprintf(name, nameCapacity, "%s.chtsel", identity->gameId);
	if(length < 0 || (size_t)length >= nameCapacity) {
		name[0] = '\0';
		return false;
	}
	return true;
}

bool CheatIdentity_AllowsLegacySelection(const CheatIdentity *identity,
	CheatDefinitionKind definitionKind)
{
	return identity != NULL && identity->discId == 0u &&
		definitionKind == CHEAT_DEFINITION_LEGACY;
}

void CheatPolicy_BeginDiscovery(CheatPolicyState *state,
	const CheatIdentity *identity)
{
	if(state == NULL) {
		return;
	}
	memset(state, 0, sizeof(*state));
	state->definitionKind = CHEAT_DEFINITION_NONE;
	if(identity != NULL) {
		state->identity = *identity;
		state->identityValid = true;
	}
}

void CheatPolicy_DefinitionLoaded(CheatPolicyState *state,
	CheatDefinitionKind definitionKind)
{
	if(state != NULL && state->identityValid &&
		definitionKind <= CHEAT_DEFINITION_LEGACY) {
		state->definitionLoaded = true;
		state->definitionKind = definitionKind;
		state->origin = CHEAT_ORIGIN_NONE;
	}
}

void CheatPolicy_DiscoveryMiss(CheatPolicyState *state)
{
	if(state != NULL) {
		state->definitionLoaded = false;
		state->definitionKind = CHEAT_DEFINITION_NONE;
		state->origin = CHEAT_ORIGIN_NONE;
	}
}

bool CheatPolicy_SavedSelectionLoaded(CheatPolicyState *state,
	const CheatIdentity *identity)
{
	if(state == NULL || !state->identityValid || !state->definitionLoaded ||
		!CheatIdentity_Equals(&state->identity, identity)) {
		return false;
	}
	state->origin = CHEAT_ORIGIN_SAVED;
	return true;
}

bool CheatPolicy_ManualSelectionCommitted(CheatPolicyState *state,
	const CheatIdentity *identity)
{
	if(state == NULL || !state->identityValid || !state->definitionLoaded ||
		!CheatIdentity_Equals(&state->identity, identity)) {
		return false;
	}
	state->origin = CHEAT_ORIGIN_MANUAL;
	return true;
}

bool CheatPolicy_Capacity(size_t engineSpace, size_t engineSize,
	size_t *capacityBytes)
{
	if(capacityBytes == NULL || engineSize > engineSpace ||
		engineSpace - engineSize < CHEAT_ENGINE_OVERHEAD) {
		return false;
	}
	*capacityBytes = engineSpace - engineSize - CHEAT_ENGINE_OVERHEAD;
	return true;
}

bool CheatPolicy_RequestDebug(size_t enabledBytes, size_t engineSpace,
	size_t debugEngineSize)
{
	size_t capacityBytes;

	return CheatPolicy_Capacity(engineSpace, debugEngineSize,
		&capacityBytes) && enabledBytes <= capacityBytes;
}

CheatLaunchDecision CheatPolicy_Decide(const CheatPolicyState *state,
	const CheatIdentity *currentIdentity, bool autoCheats, bool wiirdDebug,
	size_t enabledBytes, size_t engineSpace, size_t engineSize)
{
	CheatLaunchDecision decision;
	bool selectionRequested;

	memset(&decision, 0, sizeof(decision));
	decision.status = CHEAT_DECISION_INVALID;
	if(!CheatPolicy_Capacity(engineSpace, engineSize,
		&decision.capacityBytes)) {
		return decision;
	}
	if(state == NULL || currentIdentity == NULL || !state->identityValid) {
		return decision;
	}
	if(!CheatIdentity_Equals(&state->identity, currentIdentity)) {
		decision.status = CHEAT_DECISION_IDENTITY_MISMATCH;
		return decision;
	}
	if(!state->definitionLoaded) {
		decision.installEngine = wiirdDebug;
		decision.status = wiirdDebug ? CHEAT_DECISION_READY :
			CHEAT_DECISION_NONE;
		return decision;
	}
	selectionRequested = enabledBytes > 0u &&
		(state->origin == CHEAT_ORIGIN_MANUAL ||
		(state->origin == CHEAT_ORIGIN_SAVED && autoCheats));
	if(selectionRequested && enabledBytes > decision.capacityBytes) {
		decision.status = CHEAT_DECISION_TOO_LARGE;
		return decision;
	}
	decision.applyCodes = selectionRequested;
	decision.installEngine = wiirdDebug || decision.applyCodes;
	decision.status = decision.installEngine ? CHEAT_DECISION_READY :
		CHEAT_DECISION_NONE;
	return decision;
}

CheatDecisionStatus CheatInstallWriter_Begin(CheatInstallWriter *writer,
	void *destination, size_t destinationBytes, size_t enabledBytes,
	size_t maxCheatBytes)
{
	size_t requiredBytes;

	if(writer == NULL) {
		return CHEAT_DECISION_INVALID;
	}
	memset(writer, 0, sizeof(*writer));
	if(destination == NULL || enabledBytes > maxCheatBytes ||
		enabledBytes % CHEAT_PAIR_BYTES != 0u ||
		enabledBytes > SIZE_MAX - CHEAT_LIST_MARKER_BYTES -
			CHEAT_LIST_TERMINATOR_BYTES) {
		return enabledBytes > maxCheatBytes ? CHEAT_DECISION_TOO_LARGE :
			CHEAT_DECISION_INVALID;
	}
	requiredBytes = CHEAT_LIST_MARKER_BYTES + enabledBytes +
		CHEAT_LIST_TERMINATOR_BYTES;
	if(destinationBytes < requiredBytes || destinationBytes % sizeof(uint32_t) != 0u) {
		return CHEAT_DECISION_TOO_LARGE;
	}
	writer->begin = (uint32_t *)destination;
	writer->cursor = writer->begin + 2;
	writer->end = writer->begin + (destinationBytes / sizeof(uint32_t));
	writer->expectedPairs = enabledBytes / CHEAT_PAIR_BYTES;
	writer->active = true;
	return CHEAT_DECISION_READY;
}

CheatDecisionStatus CheatInstallWriter_Append(CheatInstallWriter *writer,
	uint32_t address, uint32_t value, uintptr_t engineStart)
{
	if(writer == NULL || !writer->active ||
		writer->writtenPairs >= writer->expectedPairs ||
		writer->cursor == NULL || writer->end == NULL ||
		writer->cursor > writer->end ||
		(size_t)(writer->end - writer->cursor) < 3u ||
		engineStart > (uintptr_t)UINT32_MAX - CHEAT_ENGINE_JUMP_OFFSET) {
		return CHEAT_DECISION_INVALID;
	}
	if(writer->writtenPairs == 0u) {
		writer->begin[0] = CHEAT_LIST_MARKER;
		writer->begin[1] = CHEAT_LIST_MARKER;
	}
	writer->cursor[0] = address;
	writer->cursor[1] = value == CHEAT_OLD_ENGINE_JUMP ?
		(uint32_t)(engineStart + CHEAT_ENGINE_JUMP_OFFSET) : value;
	writer->cursor += 2;
	writer->writtenPairs++;
	return CHEAT_DECISION_READY;
}

CheatDecisionStatus CheatInstallWriter_Finish(CheatInstallWriter *writer,
	size_t *bytesWritten)
{
	if(writer == NULL || bytesWritten == NULL || !writer->active ||
		writer->writtenPairs != writer->expectedPairs ||
		writer->cursor == NULL || writer->end == NULL ||
		writer->cursor >= writer->end) {
		return CHEAT_DECISION_INVALID;
	}
	if(writer->writtenPairs == 0u) {
		writer->begin[0] = CHEAT_LIST_MARKER;
		writer->begin[1] = CHEAT_LIST_MARKER;
	}
	writer->cursor[0] = CHEAT_LIST_TERMINATOR;
	*bytesWritten = CHEAT_LIST_MARKER_BYTES +
		(writer->writtenPairs * CHEAT_PAIR_BYTES) +
		CHEAT_LIST_TERMINATOR_BYTES;
	writer->active = false;
	return CHEAT_DECISION_READY;
}
