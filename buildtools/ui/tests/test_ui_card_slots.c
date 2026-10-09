/* ui_card_slots against scripted memory card emulators: each behaviour the
 * indigo#102 lab model plays (lab model v2, see the investigation), stepped a
 * retrace at a time as Game Detail polls it. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_card_slots.h"

static unsigned long checks;

#define CHECK(condition) do { \
	checks++; \
	if(!(condition)) { \
		fprintf(stderr, "check failed at %s:%d: %s\n", \
			__FILE__, __LINE__, #condition); \
		exit(1); \
	} \
} while(0)

#define FRAME_MS 16u
#define NEVER UINT64_MAX

/* A card after it took an ID at sentMs: the old card for holdMs, then off
 * the bus for awayMs (0: it never leaves), then present but answering garbage
 * (BROKEN) for loadMs. blank: the new card stays unformatted. idGarbageMs: an
 * ID read that soon after it came back gets garbage, which libogc2 keeps
 * (NOT_A_CARD) until the probe is reset. */
typedef struct {
	uint64_t sentMs;
	uint64_t holdMs;
	uint64_t awayMs;
	uint64_t loadMs;
	bool blank;
	uint64_t idGarbageMs;
	bool poisoned;
	bool idRead;	/* the ID was read since it came back (or the reset) */
} card_t;

static uint64_t backAt(const card_t *card)
{
	return card->sentMs + card->holdMs + card->awayMs;
}

static bool cardPresent(const card_t *card, uint64_t now)
{
	if(card->awayMs == 0u || card->holdMs == NEVER) return true;
	return now < card->sentMs + card->holdMs || now >= backAt(card);
}

static uiCardReadResult_t cardRead(card_t *card, uint64_t now)
{
	if(!cardPresent(card, now)) return UI_CARD_READ_NO_CARD;
	if(card->holdMs == NEVER || now < card->sentMs + card->holdMs) return UI_CARD_READ_OK;
	if(card->awayMs != 0u && !card->idRead) {
		card->idRead = true;
		if(now < backAt(card) + card->idGarbageMs) card->poisoned = true;
	}
	if(card->poisoned) return UI_CARD_READ_NOT_A_CARD;
	if(card->blank || now < backAt(card) + card->loadMs) return UI_CARD_READ_BROKEN;
	return UI_CARD_READ_OK;
}

typedef struct {
	unsigned reads;
	unsigned resets;
	unsigned readsBeforeBack;
	uint64_t readyAt;
	uint64_t failedAt;
} run_t;

/* Polls from `from` to `to` as Game Detail does: the presence line each
 * retrace, a read when one is due. */
static void poll(uiCardSlot_t *slot, card_t *card, uint64_t from, uint64_t to,
	run_t *run)
{
	uint64_t now;

	for(now = from; now <= to; now += FRAME_MS) {
		(void)UICardSlot_Observe(slot, cardPresent(card, now), now);
		if(UICardSlot_ReadDue(slot, now)) {
			bool reset = false;

			run->reads++;
			if(card->awayMs != 0u && card->holdMs != NEVER &&
				now >= card->sentMs + card->holdMs && now < backAt(card)) {
				run->readsBeforeBack++;
			}
			(void)UICardSlot_Read(slot, cardRead(card, now), now, &reset);
			if(reset) {
				run->resets++;
				card->poisoned = false;
				card->idRead = false;
			}
		}
		if(slot->phase == UI_CARD_SLOT_READY && run->readyAt == 0u) run->readyAt = now;
		if(slot->phase == UI_CARD_SLOT_FAILED && run->failedAt == 0u) run->failedAt = now;
	}
}

static const char GAME_A[6] = {'G', 'A', 'C', 'Z', '0', '1'};
static const char GAME_B[6] = {'G', 'C', 'H', 'Z', '0', '1'};
static const char GAME_C[6] = {'G', 'D', 'R', 'Z', '0', '1'};

static void start(uiCardSlot_t *slot, const char id[6], uint64_t now)
{
	CHECK(UICardSlot_Request(slot, id));
	UICardSlot_Sent(slot, id, now);
	CHECK(slot->phase == UI_CARD_SLOT_SENT);
	CHECK(UICardSlot_WaitingFor(slot, id));
	CHECK(!UICardSlot_ReadableFor(slot, id));
	CHECK(!UICardSlot_WritableFor(slot, id, now));
}

/* The model that matches the reporter: off the bus for 0.8 s, back while it
 * loads for 5 s. Never read while away, a few reads, ready once loaded. */
static void checkShortDetach(void)
{
	uiCardSlot_t slot;
	card_t card = {0, 300u, 800u, 5000u, false, 0u, false, false};
	run_t run = {0};

	UICardSlot_Init(&slot);
	start(&slot, GAME_A, 0u);
	poll(&slot, &card, 0u, 20000u, &run);
	CHECK(run.readsBeforeBack == 0u);
	CHECK(run.readyAt >= backAt(&card) + card.loadMs);
	CHECK(run.readyAt <= backAt(&card) + card.loadMs + UI_CARD_SLOT_RETRY_MAX_MS + FRAME_MS);
	CHECK(run.reads >= 2u && run.reads <= 5u);
	CHECK(run.resets == 0u);
	CHECK(slot.sawAway);
	CHECK(UICardSlot_ReadableFor(&slot, GAME_A));
	CHECK(!UICardSlot_ReadableFor(&slot, GAME_B));
	CHECK(UICardSlot_WritableFor(&slot, GAME_A, 20000u));
	CHECK(!UICardSlot_Busy(&slot));
}

/* Off the bus for the whole 5 s switch: one read, after it settled. */
static void checkWholeDetach(void)
{
	uiCardSlot_t slot;
	card_t card = {0, 300u, 5000u, 0u, false, 0u, false, false};
	run_t run = {0};

	UICardSlot_Init(&slot);
	start(&slot, GAME_A, 0u);
	poll(&slot, &card, 0u, 20000u, &run);
	CHECK(run.reads == 1u);
	CHECK(run.readyAt >= backAt(&card) + UI_CARD_SLOT_SETTLE_MS);
	CHECK(run.readyAt <= backAt(&card) + UI_CARD_SLOT_SETTLE_MS + 2u * FRAME_MS);
}

/* Never off the bus, garbage while it loads: read after the detach wait, again
 * until it is loaded. */
static void checkNoDetach(void)
{
	uiCardSlot_t slot;
	card_t card = {0, 300u, 0u, 0u, false, 0u, false, false};
	run_t run = {0};
	uint64_t now;

	/* awayMs 0 means present throughout; loading is modelled by hand here:
	 * BROKEN until 5.3 s. */
	UICardSlot_Init(&slot);
	start(&slot, GAME_A, 0u);
	for(now = 0u; now <= 20000u; now += FRAME_MS) {
		(void)UICardSlot_Observe(&slot, true, now);
		if(UICardSlot_ReadDue(&slot, now)) {
			run.reads++;
			CHECK(now >= UI_CARD_SLOT_DETACH_MS);
			(void)UICardSlot_Read(&slot, now < 5300u ? UI_CARD_READ_BROKEN :
				UI_CARD_READ_OK, now, NULL);
		}
		if(slot.phase == UI_CARD_SLOT_READY && run.readyAt == 0u) run.readyAt = now;
	}
	(void)card;
	CHECK(run.readyAt >= 5300u && run.readyAt <= 5300u + UI_CARD_SLOT_RETRY_MAX_MS);
	CHECK(run.reads <= 4u);
	CHECK(!slot.sawAway);
	/* Never seen to leave: no write until the quiet time passed. */
	CHECK(!UICardSlot_WritableFor(&slot, GAME_A, UI_CARD_SLOT_QUIET_MS - 1u));
	CHECK(UICardSlot_WritableFor(&slot, GAME_A, UI_CARD_SLOT_QUIET_MS));
}

/* The card had the game already: no switch, one read after the detach wait. */
static void checkNoSwitch(void)
{
	uiCardSlot_t slot;
	card_t card = {0, NEVER, 0u, 0u, false, 0u, false, false};
	run_t run = {0};

	UICardSlot_Init(&slot);
	start(&slot, GAME_A, 0u);
	poll(&slot, &card, 0u, 10000u, &run);
	CHECK(run.reads == 1u);
	CHECK(run.readyAt >= UI_CARD_SLOT_DETACH_MS &&
		run.readyAt <= UI_CARD_SLOT_DETACH_MS + FRAME_MS);
	/* The same game again: still READY, readable at once, no new wait. */
	CHECK(UICardSlot_Request(&slot, GAME_A));
	UICardSlot_Sent(&slot, GAME_A, 10000u);
	CHECK(slot.phase == UI_CARD_SLOT_READY);
	CHECK(UICardSlot_ReadableFor(&slot, GAME_A));
}

/* A new card that takes 30 s to make: a bounded number of reads. */
static void checkSlowNewCard(void)
{
	uiCardSlot_t slot;
	card_t card = {0, 300u, 800u, 30000u, false, 0u, false, false};
	run_t run = {0};

	UICardSlot_Init(&slot);
	start(&slot, GAME_B, 0u);
	poll(&slot, &card, 0u, 50000u, &run);
	CHECK(run.readyAt >= backAt(&card) + card.loadMs);
	CHECK(run.reads <= 12u);
	CHECK(slot.phase == UI_CARD_SLOT_READY);
}

/* A new card that stays unformatted: given up at the deadline, marked broken,
 * a bounded number of reads, none after. */
static void checkUnformattedNewCard(void)
{
	uiCardSlot_t slot;
	card_t card = {0, 300u, 800u, 0u, true, 0u, false, false};
	run_t run = {0};
	unsigned reads;

	UICardSlot_Init(&slot);
	start(&slot, GAME_B, 0u);
	poll(&slot, &card, 0u, 70000u, &run);
	CHECK(slot.phase == UI_CARD_SLOT_FAILED);
	CHECK(slot.broken);
	CHECK(run.failedAt >= UI_CARD_SLOT_DEADLINE_MS &&
		run.failedAt <= UI_CARD_SLOT_DEADLINE_MS + FRAME_MS);
	CHECK(run.reads <= 20u);
	reads = run.reads;
	poll(&slot, &card, 70016u, 90000u, &run);
	CHECK(run.reads == reads);
	CHECK(!UICardSlot_ReadableFor(&slot, GAME_B));
	CHECK(!UICardSlot_WaitingFor(&slot, GAME_B));
	/* Opening it again asks again. */
	CHECK(UICardSlot_Request(&slot, GAME_B));
}

/* indigo#102's "GameID stopped until re-insertion": the first ID read after
 * the card came back got garbage, which libogc2 keeps. The probe is reset and
 * the card is read again: ready, with no re-insertion. */
static void checkStaleId(void)
{
	uiCardSlot_t slot;
	card_t card = {0, 300u, 800u, 3000u, false, 1500u, false, false};
	run_t run = {0};

	UICardSlot_Init(&slot);
	start(&slot, GAME_A, 0u);
	poll(&slot, &card, 0u, 20000u, &run);
	CHECK(run.resets >= 1u);
	CHECK(slot.phase == UI_CARD_SLOT_READY);
	CHECK(run.readyAt <= backAt(&card) + card.loadMs + 2u * UI_CARD_SLOT_RETRY_MAX_MS);
}

/* A card that is off the bus for good: given up at the deadline, never read. */
static void checkGone(void)
{
	uiCardSlot_t slot;
	card_t card = {0, 300u, 1000000u, 0u, false, 0u, false, false};
	run_t run = {0};

	UICardSlot_Init(&slot);
	start(&slot, GAME_A, 0u);
	poll(&slot, &card, 0u, 70000u, &run);
	CHECK(run.reads == 0u);
	CHECK(slot.phase == UI_CARD_SLOT_FAILED);
	CHECK(!slot.broken);
	/* Put back in at last: tried again, read once settled. */
	card.awayMs = 80000u - 300u;
	run.readyAt = 0u;
	poll(&slot, &card, 70016u, 90000u, &run);
	CHECK(slot.phase == UI_CARD_SLOT_READY);
	CHECK(run.readyAt >= 80000u + UI_CARD_SLOT_SETTLE_MS);
	CHECK(run.reads == 1u);
}

/* Details opened during a switch: the latest ID waits, only it goes out after. */
static void checkLatestWins(void)
{
	uiCardSlot_t slot;
	card_t card = {0, 300u, 800u, 2000u, false, 0u, false, false};
	run_t run = {0};
	char id[6];

	UICardSlot_Init(&slot);
	start(&slot, GAME_A, 0u);
	poll(&slot, &card, 0u, 1000u, &run);
	CHECK(!UICardSlot_Request(&slot, GAME_B));
	CHECK(UICardSlot_WaitingFor(&slot, GAME_B));
	CHECK(!UICardSlot_WaitingFor(&slot, GAME_A));
	CHECK(!UICardSlot_TakePending(&slot, id, 1000u));
	CHECK(!UICardSlot_Request(&slot, GAME_C));
	CHECK(UICardSlot_WaitingFor(&slot, GAME_C));
	CHECK(!UICardSlot_WaitingFor(&slot, GAME_B));
	CHECK(!UICardSlot_ReadableFor(&slot, GAME_A));
	poll(&slot, &card, 1016u, 10000u, &run);
	CHECK(slot.phase == UI_CARD_SLOT_READY);
	CHECK(UICardSlot_Busy(&slot));
	CHECK(UICardSlot_TakePending(&slot, id, 10000u));
	CHECK(memcmp(id, GAME_C, 6) == 0);
	CHECK(!UICardSlot_TakePending(&slot, id, 10000u));
	UICardSlot_Sent(&slot, id, 10000u);
	CHECK(slot.phase == UI_CARD_SLOT_SENT);
	CHECK(UICardSlot_WaitingFor(&slot, GAME_C));
	/* Back to the game being switched to: nothing waits behind it. */
	CHECK(!UICardSlot_Request(&slot, GAME_B));
	CHECK(!UICardSlot_Request(&slot, GAME_C));
	CHECK(!slot.hasPending);
}

/* The card holds the old card past the detach wait, then switches: read early
 * (the old card), then again after the real switch. No write in between. */
static void checkLongHold(void)
{
	uiCardSlot_t slot;
	card_t card = {0, 4000u, 800u, 2000u, false, 0u, false, false};
	run_t run = {0};
	uint64_t now;
	bool earlyReady = false;

	UICardSlot_Init(&slot);
	start(&slot, GAME_A, 0u);
	for(now = 0u; now < 4000u; now += FRAME_MS) {
		(void)UICardSlot_Observe(&slot, cardPresent(&card, now), now);
		if(UICardSlot_ReadDue(&slot, now)) {
			run.reads++;
			(void)UICardSlot_Read(&slot, cardRead(&card, now), now, NULL);
		}
		if(slot.phase == UI_CARD_SLOT_READY) {
			earlyReady = true;
			CHECK(!UICardSlot_WritableFor(&slot, GAME_A, now));
		}
	}
	CHECK(earlyReady);
	poll(&slot, &card, 4000u, 20000u, &run);
	CHECK(slot.phase == UI_CARD_SLOT_READY);
	CHECK(slot.sawAway);
	CHECK(UICardSlot_WritableFor(&slot, GAME_A, 20000u));
}

/* A card whose status turns sane once loaded: read then, not at the next retry. */
static void checkHint(void)
{
	uiCardSlot_t slot;
	card_t card = {0, 300u, 800u, 5000u, false, 0u, false, false};
	run_t run = {0};
	uint64_t now, loaded = 300u + 800u + 5000u;

	UICardSlot_Init(&slot);
	start(&slot, GAME_A, 0u);
	for(now = 0u; now <= 20000u; now += FRAME_MS) {
		(void)UICardSlot_Observe(&slot, cardPresent(&card, now), now);
		/* Polled every 250 ms, as card_slots.c does. */
		if(now % 256u == 0u && now >= loaded) UICardSlot_Hint(&slot, now);
		if(UICardSlot_ReadDue(&slot, now)) {
			run.reads++;
			(void)UICardSlot_Read(&slot, cardRead(&card, now), now, NULL);
		}
		if(slot.phase == UI_CARD_SLOT_READY && run.readyAt == 0u) run.readyAt = now;
	}
	CHECK(run.readyAt >= loaded && run.readyAt <= loaded + 256u + FRAME_MS);
	CHECK(run.reads <= 5u);
	/* No hint outside LOADING. */
	UICardSlot_Init(&slot);
	start(&slot, GAME_A, 0u);
	UICardSlot_Hint(&slot, 0u);
	CHECK(!UICardSlot_ReadDue(&slot, 0u));
}

/* An emulator that didn't take the ID: the ID stays pending, is sent again
 * after 1 s, 2 s, ..., the slot is never read or written as a plain card
 * meanwhile, and it is given up on (FAILED) after the last try. */
static void checkSendFailed(void)
{
	uiCardSlot_t slot;
	char id[6];
	uint64_t now = 0u;
	unsigned sends = 0u;

	UICardSlot_Init(&slot);
	CHECK(UICardSlot_Request(&slot, GAME_A));
	UICardSlot_SendFailed(&slot, GAME_A, true, now);
	CHECK(slot.mmce && slot.hasPending && slot.phase == UI_CARD_SLOT_IDLE);
	CHECK(!UICardSlot_ReadableFor(&slot, GAME_A));
	CHECK(!UICardSlot_WritableFor(&slot, GAME_A, now));
	CHECK(UICardSlot_WaitingFor(&slot, GAME_A));
	CHECK(UICardSlot_Busy(&slot));
	CHECK(!UICardSlot_TakePending(&slot, id, now + UI_CARD_SLOT_RETRY_MS - 1u));
	for(now = 0u; now < 60000u && UICardSlot_Busy(&slot); now += FRAME_MS) {
		if(UICardSlot_TakePending(&slot, id, now)) {
			sends++;
			CHECK(memcmp(id, GAME_A, 6) == 0);
			UICardSlot_SendFailed(&slot, id, true, now);
		}
	}
	CHECK(sends == UI_CARD_SLOT_SEND_TRIES - 1u);
	CHECK(slot.phase == UI_CARD_SLOT_FAILED);
	CHECK(UICardSlot_FailedFor(&slot, GAME_A));
	CHECK(!UICardSlot_FailedFor(&slot, GAME_B));
	CHECK(!UICardSlot_ReadableFor(&slot, GAME_A));
	CHECK(!UICardSlot_Busy(&slot));
	/* Taken out: still given up on. Put back: it never took the ID, so it
	 * holds another game's card. It is sent the ID again once settled,
	 * never read meanwhile. */
	(void)UICardSlot_Observe(&slot, true, now);
	(void)UICardSlot_Observe(&slot, false, now + 100u);
	CHECK(UICardSlot_FailedFor(&slot, GAME_A));
	CHECK(UICardSlot_Observe(&slot, true, now + 200u));
	CHECK(!UICardSlot_FailedFor(&slot, GAME_A));
	CHECK(!UICardSlot_ReadableFor(&slot, GAME_A));
	CHECK(!UICardSlot_WritableFor(&slot, GAME_A, now + 200u));
	CHECK(UICardSlot_WaitingFor(&slot, GAME_A));
	CHECK(!UICardSlot_TakePending(&slot, id, now + 199u + UI_CARD_SLOT_SETTLE_MS));
	CHECK(UICardSlot_TakePending(&slot, id, now + 200u + UI_CARD_SLOT_SETTLE_MS));
	CHECK(memcmp(id, GAME_A, 6) == 0);
	/* Opening the game again asks again; a send that works starts over. */
	CHECK(UICardSlot_Request(&slot, GAME_A));
	UICardSlot_Sent(&slot, GAME_A, now);
	CHECK(slot.phase == UI_CARD_SLOT_SENT && slot.sendTries == 0u);

	/* A queued ID whose send fails after the switch ahead of it is not lost. */
	UICardSlot_Init(&slot);
	start(&slot, GAME_A, 0u);
	CHECK(!UICardSlot_Request(&slot, GAME_B));
	slot.phase = UI_CARD_SLOT_READY;
	CHECK(UICardSlot_TakePending(&slot, id, 5000u));
	UICardSlot_SendFailed(&slot, id, false, 5000u);
	CHECK(slot.hasPending && UICardSlot_WaitingFor(&slot, GAME_B));
	CHECK(!UICardSlot_ReadableFor(&slot, GAME_B));
	CHECK(UICardSlot_TakePending(&slot, id, 5000u + UI_CARD_SLOT_RETRY_MS));
	CHECK(memcmp(id, GAME_B, 6) == 0);
	UICardSlot_Sent(&slot, id, 6000u);
	CHECK(slot.phase == UI_CARD_SLOT_SENT && !slot.hasPending);

	/* Nothing ever took an ID there: a failed send is a plain card's. */
	UICardSlot_Init(&slot);
	CHECK(UICardSlot_Request(&slot, GAME_A));
	UICardSlot_SendFailed(&slot, GAME_A, false, 0u);
	CHECK(slot.phase == UI_CARD_SLOT_IDLE && !slot.hasPending && !slot.mmce);
	CHECK(UICardSlot_ReadableFor(&slot, GAME_A));
}

/* A plain memory card: the ID doesn't go out; read as it is, written as it is. */
static void checkPlainCard(void)
{
	uiCardSlot_t slot;

	UICardSlot_Init(&slot);
	CHECK(UICardSlot_Request(&slot, GAME_A));
	UICardSlot_NotSent(&slot);
	CHECK(slot.phase == UI_CARD_SLOT_IDLE);
	CHECK(!slot.mmce);
	CHECK(UICardSlot_ReadableFor(&slot, GAME_A));
	CHECK(UICardSlot_WritableFor(&slot, GAME_A, 0u));
	CHECK(!UICardSlot_WaitingFor(&slot, GAME_A));
	/* Taking a plain card out is not a switch. */
	CHECK(!UICardSlot_Observe(&slot, false, 100u));
	CHECK(slot.phase == UI_CARD_SLOT_IDLE);
	CHECK(!UICardSlot_ReadDue(&slot, 100u));
}

/* The card changed on the emulator by hand after it settled: switching again. */
static void checkManualChange(void)
{
	uiCardSlot_t slot;
	card_t card = {0, 300u, 800u, 1000u, false, 0u, false, false};
	card_t later = {20000u, 0u, 800u, 1000u, false, 0u, false, false};
	run_t run = {0};

	UICardSlot_Init(&slot);
	start(&slot, GAME_A, 0u);
	poll(&slot, &card, 0u, 20000u - FRAME_MS, &run);
	CHECK(slot.phase == UI_CARD_SLOT_READY);
	run.readyAt = 0u;
	poll(&slot, &later, 20000u, 40000u, &run);
	CHECK(slot.phase == UI_CARD_SLOT_READY);
	CHECK(run.readyAt >= backAt(&later) + later.loadMs);
}

int main(void)
{
	checkShortDetach();
	checkWholeDetach();
	checkNoDetach();
	checkNoSwitch();
	checkSlowNewCard();
	checkUnformattedNewCard();
	checkStaleId();
	checkGone();
	checkLatestWins();
	checkLongHold();
	checkHint();
	checkSendFailed();
	checkPlainCard();
	checkManualChange();
	printf("test_ui_card_slots: %lu checks passed\n", checks);
	return 0;
}
