/**
*
* Gecko OS/WiiRD cheat engine (kenobigc)
* 
* Adapted to Swiss by emu_kidid 2012-2015
*
*/

#include <stdio.h>
#include <gccore.h>		/*** Wrapper to include common libogc headers ***/
#include <ogcsys.h>		/*** Needed for console support ***/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <malloc.h>
#include <ctype.h>
#include <limits.h>
#include "swiss.h"
#include "main.h"
#include "cheats.h"
#include "cheat_policy.h"
#include "patcher.h"
#include "deviceHandler.h"
#include "FrameBufferMagic.h"
#include "IPLFontWrite.h"
#include <xxhash.h>

static CheatEntries _cheats;
static CheatIdentity _cheatIdentity;
static CheatPolicyState _cheatPolicy;

static void disposeCheatEntries(void) {
	int i;

	for(i = 0; i < _cheats.num_cheats; i++) {
		free(_cheats.cheat[i].name);
		free(_cheats.cheat[i].codes);
	}
	free(_cheats.cheat);
	memset(&_cheats, 0, sizeof(_cheats));
}

static void disableAllCheats(void) {
	int i;

	for(i = 0; i < _cheats.num_cheats; i++) {
		_cheats.cheat[i].enabled = 0;
	}
}

static bool currentCheatIdentity(CheatIdentity *identity) {
	return CheatIdentity_Init(identity, (const char *)&GCMDisk,
		GCMDisk.Version, GCMDisk.DiscID);
}

static bool beginCheatDiscovery(void) {
	disposeCheatEntries();
	memset(&_cheatIdentity, 0, sizeof(_cheatIdentity));
	memset(&_cheatPolicy, 0, sizeof(_cheatPolicy));
	devices[DEVICE_CHEATS] = NULL;
	if(!currentCheatIdentity(&_cheatIdentity)) {
		return false;
	}
	CheatPolicy_BeginDiscovery(&_cheatPolicy, &_cheatIdentity);
	return true;
}

void printCheats(void) {
	int i = 0, j = 0;
	print_debug("There are %i cheats\n", _cheats.num_cheats);
	for(i = 0; i < _cheats.num_cheats; i++) {
		CheatEntry *cheat = &_cheats.cheat[i];
		print_debug("Cheat: (%i codes) %s\n", cheat->num_codes, cheat->name);
		for(j = 0; j < cheat->num_codes; j++) {
			print_debug("%08X %08X\n", cheat->codes[j][0], cheat->codes[j][1]);
		}
	}
}

int getEnabledCheatsSize(void) {
	int i = 0, size = 0;
	for(i = 0; i < _cheats.num_cheats; i++) {
		CheatEntry *cheat = &_cheats.cheat[i];
		if(cheat->enabled) {
			if(cheat->num_codes > (INT_MAX - size) / 8) {
				return INT_MAX;
			}
			size += ((cheat->num_codes*2)*4);
		}
	}
	//print_debug("Size of cheats: %i\n", size);
	return size;
}

int getEnabledCheatsCount(void) {
	int i = 0, enabled = 0;
	for(i = 0; i < _cheats.num_cheats; i++) {
		CheatEntry *cheat = &_cheats.cheat[i];
		if(cheat->enabled) {
			enabled++;
		}
	}
	//print_debug("Size of cheats: %i\n", size);
	return enabled;
}

// Checks that the line contains a valid code in the format of "01234567 89ABCDEF"
int isValidCode(char *line) {
	int len = 0;
	return sscanf(line, "%*8[0-9A-Fa-f] %*8[0-9A-Fa-f]%n", &len) == 0 && len > 16;
}

int isCheatCode(char *line) {
	int len = 0;
	return sscanf(line, "%*2[0-9A-Fa-f]%*6[0-9A-Za-z] %*8[0-9A-Za-z]%n", &len) == 0 && len > 16;
}

/** 
	Given a char array with the contents of a .txt, 
	this method will allocate and return a populated Parameters struct 
*/
void parseCheats(char *filecontents) {
	char *line = NULL, *prevLine = NULL, *linectx = NULL;
	int numCheats = 0;
	line = strtok_r( filecontents, "\r\n", &linectx );

	// Free previous definitions without changing the discovery identity.
	disposeCheatEntries();
	
	CheatEntry *curCheat = NULL;	// The current one we're parsing
	while( line != NULL ) {
		//print_debug("Line [%s]\n", line);
		if(isCheatCode(line)) {		// The line looks like a valid code
			_cheats.cheat = reallocarray(_cheats.cheat, numCheats+1, sizeof(CheatEntry));
			curCheat = &_cheats.cheat[numCheats];
			memset(curCheat, 0, sizeof(CheatEntry));
			
			if(prevLine != NULL) {
				curCheat->name = strdup(prevLine);
				//print_debug("Cheat Name: [%s]\n", prevLine);
			}
			int numCodes = 0, unsupported = 0;
			if(isValidCode(line)) {
				// Add this valid code as the first code for this cheat
				curCheat->codes = reallocarray(curCheat->codes, numCodes+1, sizeof(*curCheat->codes));
				sscanf(line, "%x %x", &curCheat->codes[numCodes][0], &curCheat->codes[numCodes][1]);
				numCodes++;
			}
			else {
				// If a code contains "XX" in it, it is unsupported, discard it entirely
				unsupported = 1;
				numCodes++;
			}
			
			line = strtok_r( NULL, "\r\n", &linectx);
			// Keep going until we're out of codes for this cheat
			while( line != NULL ) {
				if(isCheatCode(line)) {
					if(isValidCode(line)) {
						// Add this valid code
						curCheat->codes = reallocarray(curCheat->codes, numCodes+1, sizeof(*curCheat->codes));
						sscanf(line, "%x %x", &curCheat->codes[numCodes][0], &curCheat->codes[numCodes][1]);
						numCodes++;
					}
					else {
						// If a code contains "XX" in it, it is unsupported, discard it entirely
						unsupported = 1;
						numCodes++;
					}
				}
				else {
					break;
				}
				line = strtok_r( NULL, "\r\n", &linectx);
			}
			
			if(unsupported) {
				free(curCheat->name);
				free(curCheat->codes);
			}
			else {
				curCheat->num_codes = numCodes;
				numCheats++;
			}
		}
		prevLine = line;
		// And round we go again
		line = strtok_r( NULL, "\r\n", &linectx);
	}
	_cheats.num_cheats = numCheats;
	//printCheats();
}

CheatEntries* getCheats(void) {
	return &_cheats;
}

static CheatLaunchDecision getCheatLaunchDecision(void) {
	CheatIdentity currentIdentity;
	u32 engineSize = swissSettings.wiirdDebug ? kenobigc_dbg_bin_size :
		kenobigc_bin_size;

	if(!currentCheatIdentity(&currentIdentity)) {
		CheatLaunchDecision invalidDecision;
		memset(&invalidDecision, 0, sizeof(invalidDecision));
		invalidDecision.status = CHEAT_DECISION_INVALID;
		return invalidDecision;
	}
	return CheatPolicy_Decide(&_cheatPolicy, &currentIdentity,
		swissSettings.autoCheats != 0, swissSettings.wiirdDebug != 0,
		(size_t)getEnabledCheatsSize(), WIIRD_ENGINE_SPACE, engineSize);
}

int getRuntimeEnabledCheatsSize(void) {
	CheatLaunchDecision decision = getCheatLaunchDecision();
	return decision.applyCodes ? getEnabledCheatsSize() : 0;
}

int getRuntimeEnabledCheatsCount(void) {
	CheatLaunchDecision decision = getCheatLaunchDecision();
	return decision.applyCodes ? getEnabledCheatsCount() : 0;
}

bool cheatsCanEnableDebug(void) {
	return CheatPolicy_RequestDebug((size_t)getEnabledCheatsSize(),
		WIIRD_ENGINE_SPACE, kenobigc_dbg_bin_size);
}

CheatDecisionStatus cheatsLaunchStatus(void) {
	return getCheatLaunchDecision().status;
}

bool cheatsShouldInstallEngine(void) {
	return getCheatLaunchDecision().installEngine;
}

// Installs the GeckoOS (kenobiGC) cheats engine and sets up variables/copies cheats
bool kenobi_install_engine(void) {
	int isDebug = swissSettings.wiirdDebug;
	// If high memory is in use, we'll use low, otherwise high.
	const u8 *ptr = isDebug ? kenobigc_dbg_bin : kenobigc_bin;
	u32 size = isDebug ? kenobigc_dbg_bin_size : kenobigc_bin_size;
	CheatLaunchDecision decision = getCheatLaunchDecision();
	CheatInstallWriter writer;
	size_t enabledBytes = decision.applyCodes ?
		(size_t)getEnabledCheatsSize() : 0u;
	size_t destinationBytes = WIIRD_ENGINE_SPACE - (size_t)size + 8u;
	size_t bytesWritten = 0u;
	CheatDecisionStatus writerStatus;

	if(decision.status != CHEAT_DECISION_READY || !decision.installEngine) {
		print_debug("Refusing unsafe or unrequested cheat engine install (%i)\n",
			decision.status);
		return false;
	}
	writerStatus = CheatInstallWriter_Begin(&writer, CHEATS_LOCATION(size),
		destinationBytes, enabledBytes, decision.capacityBytes);
	if(writerStatus != CHEAT_DECISION_READY) {
		print_debug("Refusing out-of-bounds cheat list (%i)\n", writerStatus);
		return false;
	}
	
	print_debug("Copying kenobi%s to %08X\n", (isDebug?"_dbg":""),(u32)CHEATS_ENGINE);
	memcpy(CHEATS_ENGINE, ptr, size);
	memcpy(CHEATS_GAMEID, VAR_AREA, CHEATS_GAMEID_LEN);
	if(!isDebug && decision.applyCodes) {
		CHEATS_ENABLE_CHEATS = CHEATS_TRUE;
	}
	CHEATS_START_PAUSED = isDebug ? CHEATS_TRUE : CHEATS_FALSE;
	memset(CHEATS_LOCATION(size), 0, destinationBytes);
	print_debug("Copying %i bytes of cheats to %08X\n",
		(int)enabledBytes, (u32)CHEATS_LOCATION(size));
	
	int i = 0, j = 0;
	for(i = 0; decision.applyCodes && i < _cheats.num_cheats; i++) {
		CheatEntry *cheat = &_cheats.cheat[i];
		if(cheat->enabled) {
			for(j = 0; j < cheat->num_codes; j++) {
				writerStatus = CheatInstallWriter_Append(&writer,
					cheat->codes[j][0], cheat->codes[j][1],
					(uintptr_t)CHEATS_ENGINE);
				if(writerStatus != CHEAT_DECISION_READY) {
					print_debug("Cheat writer rejected code pair (%i)\n",
						writerStatus);
					return false;
				}
			}
		}
	}
	writerStatus = CheatInstallWriter_Finish(&writer, &bytesWritten);
	if(writerStatus != CHEAT_DECISION_READY) {
		print_debug("Cheat writer rejected final list (%i)\n", writerStatus);
		return false;
	}
	print_debug("Prepared %u bytes of bounded cheat list\n", (u32)bytesWritten);
	DCFlushRange((void*)CHEATS_ENGINE, WIIRD_ENGINE_SPACE);
	ICInvalidateRange((void*)CHEATS_ENGINE, WIIRD_ENGINE_SPACE);
	return true;
}

int kenobi_get_maxsize_for_debug(bool debugEnabled) {
	size_t capacityBytes = 0u;
	u32 engineSize = debugEnabled ? kenobigc_dbg_bin_size : kenobigc_bin_size;
	if(!CheatPolicy_Capacity(WIIRD_ENGINE_SPACE, engineSize,
		&capacityBytes) || capacityBytes > INT_MAX) {
		return 0;
	}
	return (int)capacityBytes;
}

int kenobi_get_maxsize(void) {
	return kenobi_get_maxsize_for_debug(swissSettings.wiirdDebug != 0);
}

static bool probeCheatsFile(DEVICEHANDLER_INTERFACE *device,
	file_handle *cheatsFile, char testBuffer[8]) {
	if(device->readFile(cheatsFile, testBuffer, 8) == 8) {
		return true;
	}
	if(device->closeFile != NULL) {
		device->closeFile(cheatsFile);
	}
	cheatsFile->size = 0;
	return false;
}

static bool probeCheatsOnDevice(DEVICEHANDLER_INTERFACE *device,
	file_handle *cheatsFile, char testBuffer[8],
	CheatDefinitionKind *definitionKind) {
	char names[CHEAT_DEFINITION_CANDIDATE_COUNT][CHEAT_FILENAME_CAPACITY];
	size_t candidateCount;
	size_t i;

	if(device == NULL || cheatsFile == NULL || definitionKind == NULL) {
		return false;
	}
	candidateCount = CheatIdentity_DefinitionCandidates(&_cheatIdentity,
		names);
	for(i = 0u; i < candidateCount; ++i) {
		memset(cheatsFile, 0, sizeof(*cheatsFile));
		if(concatf_path(cheatsFile->name, device->initial->name,
			"swiss/cheats/%s", names[i]) >= PATHNAME_MAX) {
			continue;
		}
		print_debug("Looking for cheats file @ [%s]\n", cheatsFile->name);
		if(probeCheatsFile(device, cheatsFile, testBuffer)) {
			*definitionKind = (CheatDefinitionKind)i;
			return true;
		}
	}
	return false;
}

static int findCheatsInternal(bool silent, bool allowPathMutation,
	bool allowFallbackInit) {
	char testBuffer[8];
	bool cheatsFileOpen = false;
	bool fallbackStatDisabled = false;
	CheatDefinitionKind definitionKind = CHEAT_DEFINITION_NONE;
	DEVICEHANDLER_INTERFACE *searchDevices[4];
	size_t searchCount;
	size_t i;
	bool found = false;

	if(!beginCheatDiscovery()) {
		return 0;
	}
	file_handle *cheatsFile = calloc(1, sizeof(file_handle));
	if(cheatsFile == NULL) {
		CheatPolicy_DiscoveryMiss(&_cheatPolicy);
		return 0;
	}
	searchDevices[0] = devices[DEVICE_CUR];
	searchDevices[1] = &__device_sd_a;
	searchDevices[2] = &__device_sd_b;
	searchDevices[3] = &__device_sd_c;
	searchCount = allowFallbackInit ? 4u : 1u;

	for(i = 0u; i < searchCount && !found; ++i) {
		size_t previous;
		bool duplicate = false;
		for(previous = 0u; previous < i; ++previous) {
			if(searchDevices[previous] == searchDevices[i]) {
				duplicate = true;
				break;
			}
		}
		if(duplicate || searchDevices[i] == NULL) {
			continue;
		}
		devices[DEVICE_CHEATS] = searchDevices[i];
		if(i > 0u) {
			if(!fallbackStatDisabled) {
				deviceHandler_setStatEnabled(0);
				fallbackStatDisabled = true;
			}
			devices[DEVICE_CHEATS]->init(devices[DEVICE_CHEATS]->initial);
		}
		if(allowPathMutation) {
			ensure_path(DEVICE_CHEATS, "swiss", NULL, true);
			ensure_path(DEVICE_CHEATS, "swiss/cheats", "cheats", false);
		}
		cheatsFileOpen = probeCheatsOnDevice(devices[DEVICE_CHEATS],
			cheatsFile, testBuffer, &definitionKind);
		found = cheatsFileOpen;
	}
	if(fallbackStatDisabled) {
		deviceHandler_setStatEnabled(1);
	}
	if(!found) {
		devices[DEVICE_CHEATS] = NULL;
	}
	// Still fail?
	if(devices[DEVICE_CHEATS] == NULL || cheatsFile->size == 0) {
		if(cheatsFileOpen && devices[DEVICE_CHEATS] != NULL &&
			devices[DEVICE_CHEATS]->closeFile != NULL) {
			devices[DEVICE_CHEATS]->closeFile(cheatsFile);
		}
		if(!silent) {
			uiDrawObj_t *msgBox = DrawMessageBox(D_INFO,"No cheats file found.\nPress A to continue.");
			DrawPublish(msgBox);
			wait_press_A();
			DrawDispose(msgBox);
		}
		CheatPolicy_DiscoveryMiss(&_cheatPolicy);
		free(cheatsFile);
		return 0;
	}
	print_debug("Cheats file found with size %i\n", cheatsFile->size);
	bool readComplete = false;
	char *cheats_buffer = calloc(1, cheatsFile->size + 1);
	if(cheats_buffer) {
		devices[DEVICE_CHEATS]->seekFile(cheatsFile, 0, DEVICE_HANDLER_SEEK_SET);
		s32 bytesRead = devices[DEVICE_CHEATS]->readFile(cheatsFile,
			cheats_buffer, cheatsFile->size);
		readComplete = bytesRead == (s32)cheatsFile->size;
		if(readComplete) {
			parseCheats(cheats_buffer);
		}
		free(cheats_buffer);
	}
	devices[DEVICE_CHEATS]->closeFile(cheatsFile);
	free(cheatsFile);
	if(!readComplete) {
		CheatPolicy_DiscoveryMiss(&_cheatPolicy);
		devices[DEVICE_CHEATS] = NULL;
		return 0;
	}
	if(_cheats.num_cheats > 0) {
		CheatPolicy_DefinitionLoaded(&_cheatPolicy, definitionKind);
	}
	else {
		CheatPolicy_DiscoveryMiss(&_cheatPolicy);
	}

	if(!silent && _cheats.num_cheats == 0) {
		uiDrawObj_t *msgBox = DrawMessageBox(D_INFO,"Empty or unreadable cheats file found.\nPress A to continue.");
		DrawPublish(msgBox);
		wait_press_A();
		DrawDispose(msgBox);
	}
	return _cheats.num_cheats;
}

int findCheats(bool silent) {
	return findCheatsInternal(silent, true, true);
}

int findCheatsReadOnly(void) {
	/* Automatic dashboard discovery is current-device-only. It must never
	 * create/migrate paths or remount fallback slots merely because a game
	 * was inspected; explicit legacy flows retain that behavior above. */
	return findCheatsInternal(true, false, false);
}

XXH64_hash_t calcCheatsHash() {
	XXH64_state_t* const state = XXH64_createState();
	XXH64_hash_t const seed = 0;
	XXH64_reset(state, seed);
	
	int i = 0, j = 0;
	for(i = 0; i < _cheats.num_cheats; i++) {
		CheatEntry *cheat = &_cheats.cheat[i];
		for(j = 0; j < cheat->num_codes; j++) {
			XXH64_update(state, &cheat->codes[j][0], 8);
		}
	}
	XXH64_hash_t const hash = XXH64_digest(state);
	XXH64_freeState(state);
	print_debug("Cheats file hash is: [%08X], %i cheats.\n", (u32)(hash&0xFFFFFFFF), _cheats.num_cheats);
	return hash;
}

typedef enum {
	CHEAT_SELECTION_NOT_FOUND = 0,
	CHEAT_SELECTION_VALID,
	CHEAT_SELECTION_INVALID
} CheatSelectionLoadResult;

static bool buildCheatSelectionPath(file_handle *selectionFile,
	const char *baseName) {
	return selectionFile != NULL && baseName != NULL &&
		devices[DEVICE_CHEATS] != NULL &&
		concatf_path(selectionFile->name,
			devices[DEVICE_CHEATS]->initial->name,
			"swiss/cheats/%s", baseName) < PATHNAME_MAX;
}

static CheatSelectionLoadResult loadCheatSelectionFile(
	const char *baseName, bool deleteMismatch) {
	XXH64_hash_t oldHash = 0;
	XXH64_hash_t currentHash;
	file_handle *selectionFile;
	u8 *enabledFlags;
	size_t expectedSize;
	bool valid = false;
	int i;

	if(_cheats.num_cheats <= 0 ||
		(size_t)_cheats.num_cheats > UINT32_MAX - sizeof(oldHash)) {
		return CHEAT_SELECTION_INVALID;
	}
	selectionFile = calloc(1, sizeof(*selectionFile));
	enabledFlags = calloc((size_t)_cheats.num_cheats, sizeof(*enabledFlags));
	if(selectionFile == NULL || enabledFlags == NULL ||
		!buildCheatSelectionPath(selectionFile, baseName)) {
		free(selectionFile);
		free(enabledFlags);
		return CHEAT_SELECTION_INVALID;
	}
	print_debug("Looking for previous cheat selection file [%s].\n",
		selectionFile->name);
	if(devices[DEVICE_CHEATS]->statFile(selectionFile)) {
		devices[DEVICE_CHEATS]->closeFile(selectionFile);
		free(selectionFile);
		free(enabledFlags);
		return CHEAT_SELECTION_NOT_FOUND;
	}
	expectedSize = (size_t)_cheats.num_cheats + sizeof(oldHash);
	currentHash = calcCheatsHash();
	if(selectionFile->size == expectedSize &&
		devices[DEVICE_CHEATS]->seekFile(selectionFile,
			_cheats.num_cheats, DEVICE_HANDLER_SEEK_SET) ==
			_cheats.num_cheats &&
		devices[DEVICE_CHEATS]->readFile(selectionFile, &oldHash,
			sizeof(oldHash)) == sizeof(oldHash) && oldHash == currentHash &&
		devices[DEVICE_CHEATS]->seekFile(selectionFile, 0,
			DEVICE_HANDLER_SEEK_SET) == 0 &&
		devices[DEVICE_CHEATS]->readFile(selectionFile, enabledFlags,
			_cheats.num_cheats) == _cheats.num_cheats) {
		valid = true;
		for(i = 0; i < _cheats.num_cheats; ++i) {
			if(enabledFlags[i] > 1u) {
				valid = false;
				break;
			}
		}
	}
	devices[DEVICE_CHEATS]->closeFile(selectionFile);
	if(valid) {
		for(i = 0; i < _cheats.num_cheats; ++i) {
			_cheats.cheat[i].enabled = enabledFlags[i];
		}
		print_debug("Hash and length matched, loading cheat selections.\n");
	}
	else if(deleteMismatch) {
		devices[DEVICE_CHEATS]->deleteFile(selectionFile);
		print_debug("Invalid keyed cheat selection; deleting it.\n");
	}
	else {
		print_debug("Invalid cheat selection; ignoring it.\n");
	}
	free(selectionFile);
	free(enabledFlags);
	return valid ? CHEAT_SELECTION_VALID : CHEAT_SELECTION_INVALID;
}

static bool loadCheatsSelectionInternal(bool deleteMismatch) {
	char exactName[CHEAT_FILENAME_CAPACITY];
	char legacyName[CHEAT_FILENAME_CAPACITY];
	CheatSelectionLoadResult result;

	disableAllCheats();
	if(devices[DEVICE_CHEATS] == NULL || !_cheatPolicy.definitionLoaded ||
		!CheatIdentity_SelectionName(&_cheatIdentity, exactName,
			sizeof(exactName))) {
		return false;
	}
	result = loadCheatSelectionFile(exactName, deleteMismatch);
	if(result == CHEAT_SELECTION_NOT_FOUND &&
		CheatIdentity_AllowsLegacySelection(&_cheatIdentity,
			_cheatPolicy.definitionKind) &&
		CheatIdentity_LegacySelectionName(&_cheatIdentity, legacyName,
			sizeof(legacyName))) {
		/* Legacy six-ID selections are compatibility input only. Never delete,
		 * rename, or mirror them automatically. Explicit saves use the exact
		 * revision/disc key below. */
		result = loadCheatSelectionFile(legacyName, false);
	}
	if(result == CHEAT_SELECTION_VALID) {
		return CheatPolicy_SavedSelectionLoaded(&_cheatPolicy,
			&_cheatIdentity);
	}
	return false;
}

bool loadCheatsSelection(void) {
	return loadCheatsSelectionInternal(true);
}

bool loadCheatsSelectionReadOnly(void) {
	return loadCheatsSelectionInternal(false);
}

bool saveCheatsSelection(void) {
	XXH64_hash_t cheatsHash;
	file_handle *selectionFile;
	char exactName[CHEAT_FILENAME_CAPACITY];
	int i;
	bool writeComplete = true;

	/* Cheats read from a read-only device (a data disc) cannot be saved;
	 * its handler has no writeFile, and calling it crashed on Done. */
	if(devices[DEVICE_CHEATS] == NULL ||
		devices[DEVICE_CHEATS]->writeFile == NULL ||
		!_cheatPolicy.definitionLoaded ||
		!CheatIdentity_SelectionName(&_cheatIdentity, exactName,
			sizeof(exactName)) || getEnabledCheatsSize() > kenobi_get_maxsize()) {
		return false;
	}
	selectionFile = calloc(1, sizeof(*selectionFile));
	if(selectionFile == NULL ||
		!buildCheatSelectionPath(selectionFile, exactName)) {
		free(selectionFile);
		return false;
	}
	print_debug("Looking to update exact cheat selection file [%s].\n",
		selectionFile->name);
	cheatsHash = calcCheatsHash();
	for(i = 0; i < _cheats.num_cheats; i++) {
		u8 enabled = _cheats.cheat[i].enabled != 0;
		if(devices[DEVICE_CHEATS]->writeFile(selectionFile, &enabled,
			1) != 1) {
			writeComplete = false;
			break;
		}
	}
	if(writeComplete && devices[DEVICE_CHEATS]->writeFile(selectionFile,
		&cheatsHash, sizeof(cheatsHash)) != sizeof(cheatsHash)) {
		writeComplete = false;
	}
	if(!writeComplete) {
		devices[DEVICE_CHEATS]->closeFile(selectionFile);
		devices[DEVICE_CHEATS]->deleteFile(selectionFile);
		free(selectionFile);
		return false;
	}
	/* Keep an exact all-disabled record as a compatibility tombstone. Without
	 * it, disabling every imported legacy selection would re-import the old
	 * six-ID flags on the next visit. */
	print_debug("Finished exact cheat selection file, hash [%08X].\n",
		(u32)(cheatsHash & 0xFFFFFFFF));
	devices[DEVICE_CHEATS]->closeFile(selectionFile);
	free(selectionFile);
	return CheatPolicy_ManualSelectionCommitted(&_cheatPolicy,
		&_cheatIdentity);
}
