#ifndef UI_CARD_SLOTS_H
#define UI_CARD_SLOTS_H

/* A memory card slot around a GameID switch.
 *
 * A memory card emulator (the MemCard PRO GC, or another card speaking MMCE)
 * changes to a game's own virtual card when it gets the game's ID, and nothing
 * says when it is done. It leaves the bus for a moment, comes back, and can
 * still be loading the card for some seconds after (longer when it makes a new
 * one); until then a read gets the card it had, or nothing usable. libogc2
 * also answers the first probe after the card comes back with BUSY, and caches
 * the card's ID until the card leaves the bus again.
 *
 * This is one slot's state through that, from what the console can see: the
 * slot's presence line, the clock, and what a read of the card gave. No
 * hardware here; card_slots.c feeds it. Times are milliseconds. */

#include <stdbool.h>
#include <stdint.h>

#define UI_CARD_SLOT_ID_LENGTH 6u

/* A switch shows on the presence line within this, or none is coming (the
 * card had the game already, or doesn't leave the bus): then it is read. */
#define UI_CARD_SLOT_DETACH_MS 2500u
/* Back on the bus to the first read: past libogc2's 300 ms, and past the
 * moment a card that just came back may not answer its ID yet. */
#define UI_CARD_SLOT_SETTLE_MS 1000u
/* A read that found it still loading is tried again after 1 s, 2 s, then
 * every 4 s: a few reads per switch, never one a second. */
#define UI_CARD_SLOT_RETRY_MS 1000u
#define UI_CARD_SLOT_RETRY_MAX_MS 4000u
/* From the ID going out: a switch not done by then is given up on. */
#define UI_CARD_SLOT_DEADLINE_MS 60000u
/* A card that never left the bus after the ID is only written this long
 * after it, so a write can't land on the card it is still switching from. */
#define UI_CARD_SLOT_QUIET_MS 10000u
/* An emulator that didn't take the ID is sent it again after 1 s, 2 s, ...
 * this many times in all, then the slot is given up on (FAILED, never read
 * as a plain card). */
#define UI_CARD_SLOT_SEND_TRIES 5u

typedef enum {
	UI_CARD_SLOT_IDLE = 0,	/* no switch of ours: read it as it is */
	UI_CARD_SLOT_SENT,	/* the ID went out; the card hasn't left the bus */
	UI_CARD_SLOT_AWAY,	/* off the bus */
	UI_CARD_SLOT_BACK,	/* back on the bus, settling before the first read */
	UI_CARD_SLOT_LOADING,	/* read, not ready yet; read again at nextMs */
	UI_CARD_SLOT_READY,	/* read fine after the switch */
	UI_CARD_SLOT_FAILED	/* not ready by the deadline */
} uiCardSlotPhase_t;

/* What a read of the card gave, as card_slots.c sorts libogc2's results. */
typedef enum {
	UI_CARD_READ_OK = 0,	/* mounted */
	UI_CARD_READ_BUSY,	/* busy, an I/O error, or gone in the middle */
	UI_CARD_READ_BROKEN,	/* no valid directory: still loading, or unformatted */
	UI_CARD_READ_NO_CARD,	/* the slot is empty */
	UI_CARD_READ_NOT_A_CARD	/* answers as something else */
} uiCardReadResult_t;

typedef struct {
	uiCardSlotPhase_t phase;
	char id[UI_CARD_SLOT_ID_LENGTH];	/* the game the card was asked for */
	char pending[UI_CARD_SLOT_ID_LENGTH];	/* asked during a switch: sent after */
	bool hasPending;
	bool mmce;	/* took a GameID: an emulator, not a plain card */
	bool sawAway;	/* this switch left the bus */
	bool broken;	/* the last read was BROKEN (FAILED: an unformatted card) */
	bool unsent;	/* FAILED: the ID never went out (else: the card never loaded) */
	bool present;	/* the presence line when last seen */
	bool seen;	/* present holds a reading */
	uint64_t pendingAfterMs;	/* a failed send is tried again from then */
	unsigned sendTries;	/* failed sends of the pending ID */
	bool answered;	/* one of them reached an emulator (it said so) */
	uint64_t sentMs;
	uint64_t backMs;
	uint64_t nextMs;
	uint64_t deadlineMs;
	unsigned tries;
} uiCardSlot_t;

void UICardSlot_Init(uiCardSlot_t *slot);

/* A game's details open. True: send its ID now, then Sent or NotSent. False:
 * the card is switching to another game; this ID waits for it, the latest
 * one only, and goes out when TakePending gives it. */
bool UICardSlot_Request(uiCardSlot_t *slot, const char id[UI_CARD_SLOT_ID_LENGTH]);

/* The card took the ID at nowMs. */
void UICardSlot_Sent(uiCardSlot_t *slot, const char id[UI_CARD_SLOT_ID_LENGTH],
	uint64_t nowMs);

/* It couldn't take it: no emulator in the slot (a plain card, or nothing). */
void UICardSlot_NotSent(uiCardSlot_t *slot);

/* The ID didn't go out to what is (emulator) or was (mmce) an emulator: it
 * stays the pending ID and is sent again later, a few times, before the slot
 * is given up on. Never a plain card's IDLE: the card may be switching. With
 * no emulator ever seen there, it is NotSent. */
void UICardSlot_SendFailed(uiCardSlot_t *slot, const char id[UI_CARD_SLOT_ID_LENGTH],
	bool emulator, uint64_t nowMs);

/* The presence line at nowMs. True when the phase changed. */
bool UICardSlot_Observe(uiCardSlot_t *slot, bool present, uint64_t nowMs);

/* A read is due: LOADING, and its time came. */
bool UICardSlot_ReadDue(const uiCardSlot_t *slot, uint64_t nowMs);

/* The card says it is ready (its status byte turned sane): read it now,
 * once, instead of at the next retry. A card that says nothing is read on
 * the retries alone. */
void UICardSlot_Hint(uiCardSlot_t *slot, uint64_t nowMs);

/* What that read gave. True when the phase changed. *resetProbe: the card
 * answered as something else after a switch, which is libogc2's cached ID
 * from a read while it was switching: reset the probe before the next read. */
bool UICardSlot_Read(uiCardSlot_t *slot, uiCardReadResult_t result,
	uint64_t nowMs, bool *resetProbe);

/* The ID that waited, once the slot can take it (not switching, and past a
 * failed send's wait). */
bool UICardSlot_TakePending(uiCardSlot_t *slot, char id[UI_CARD_SLOT_ID_LENGTH],
	uint64_t nowMs);

/* A switch is under way, or an ID waits. */
bool UICardSlot_Busy(const uiCardSlot_t *slot);

/* Game id's details may read this slot now: no switch of ours (whatever card
 * is there), or READY for that game. */
bool UICardSlot_ReadableFor(const uiCardSlot_t *slot,
	const char id[UI_CARD_SLOT_ID_LENGTH]);

/* The slot is switching to game id, or id waits for it: its saves come later. */
bool UICardSlot_WaitingFor(const uiCardSlot_t *slot,
	const char id[UI_CARD_SLOT_ID_LENGTH]);

/* The slot was given up on while switching to game id. */
bool UICardSlot_FailedFor(const uiCardSlot_t *slot,
	const char id[UI_CARD_SLOT_ID_LENGTH]);

/* A save for game id may be written to this slot now. */
bool UICardSlot_WritableFor(const uiCardSlot_t *slot,
	const char id[UI_CARD_SLOT_ID_LENGTH], uint64_t nowMs);

#endif
