#include "ui_card_slots.h"

#include <string.h>

static bool sameId(const char a[UI_CARD_SLOT_ID_LENGTH],
	const char b[UI_CARD_SLOT_ID_LENGTH])
{
	return memcmp(a, b, UI_CARD_SLOT_ID_LENGTH) == 0;
}

static bool switching(const uiCardSlot_t *slot)
{
	return slot->phase == UI_CARD_SLOT_SENT || slot->phase == UI_CARD_SLOT_AWAY ||
		slot->phase == UI_CARD_SLOT_BACK || slot->phase == UI_CARD_SLOT_LOADING;
}

static uint64_t retryMs(unsigned tries)
{
	uint64_t wait = UI_CARD_SLOT_RETRY_MS;

	while(tries > 1u && wait < UI_CARD_SLOT_RETRY_MAX_MS) {
		wait *= 2u;
		tries--;
	}
	return wait < UI_CARD_SLOT_RETRY_MAX_MS ? wait : UI_CARD_SLOT_RETRY_MAX_MS;
}

void UICardSlot_Init(uiCardSlot_t *slot)
{
	if(slot != NULL) memset(slot, 0, sizeof(*slot));
}

bool UICardSlot_Request(uiCardSlot_t *slot, const char id[UI_CARD_SLOT_ID_LENGTH])
{
	if(slot == NULL || id == NULL) return false;
	if(!switching(slot)) return true;
	/* Its own switch is under way already: nothing waits behind it. */
	if(sameId(slot->id, id)) {
		slot->hasPending = false;
		return false;
	}
	memcpy(slot->pending, id, UI_CARD_SLOT_ID_LENGTH);
	slot->hasPending = true;
	return false;
}

void UICardSlot_Sent(uiCardSlot_t *slot, const char id[UI_CARD_SLOT_ID_LENGTH],
	uint64_t nowMs)
{
	if(slot == NULL || id == NULL) return;
	/* The latest ID went out: none waits behind it any more. */
	slot->hasPending = false;
	slot->mmce = true;
	/* The card has this game settled already: no switch comes. If it does
	 * (someone changed the card on it meanwhile), the presence line says so. */
	if(slot->phase == UI_CARD_SLOT_READY && sameId(slot->id, id)) return;
	memcpy(slot->id, id, UI_CARD_SLOT_ID_LENGTH);
	slot->phase = UI_CARD_SLOT_SENT;
	slot->sawAway = false;
	slot->broken = false;
	slot->sentMs = nowMs;
	slot->deadlineMs = nowMs + UI_CARD_SLOT_DEADLINE_MS;
	slot->tries = 0u;
	slot->sendTries = 0u;
	slot->answered = false;
	slot->pendingAfterMs = 0u;
	slot->unsent = false;
}

void UICardSlot_NotSent(uiCardSlot_t *slot)
{
	if(slot == NULL) return;
	slot->phase = UI_CARD_SLOT_IDLE;
	slot->hasPending = false;
	slot->sendTries = 0u;
	slot->answered = false;
}

void UICardSlot_SendFailed(uiCardSlot_t *slot, const char id[UI_CARD_SLOT_ID_LENGTH],
	bool emulator, uint64_t nowMs)
{
	if(slot == NULL || id == NULL) return;
	if(!emulator && !slot->mmce) {
		UICardSlot_NotSent(slot);
		return;
	}
	slot->mmce = true;
	slot->answered |= emulator;
	slot->sendTries++;
	/* Nothing in the slot answered as an emulator, five times over: what is
	 * there now (a plain card put in its place, or none) is read as it is.
	 * One that answered in between (out of the slot at the last try, or
	 * loading with a garbage ID) is still the emulator: given up on below. */
	if(slot->sendTries >= UI_CARD_SLOT_SEND_TRIES && !slot->answered) {
		slot->mmce = false;
		UICardSlot_NotSent(slot);
		return;
	}
	if(slot->sendTries >= UI_CARD_SLOT_SEND_TRIES) {
		/* Given up: not this game's card for sure, and not read or written
		 * as if it were a plain one. Opening the game again starts over. */
		memcpy(slot->id, id, UI_CARD_SLOT_ID_LENGTH);
		slot->hasPending = false;
		slot->sendTries = 0u;
		slot->answered = false;
		slot->phase = UI_CARD_SLOT_FAILED;
		slot->broken = false;
		slot->unsent = true;
		return;
	}
	/* Not followed while the ID waits: a card that took it and left the bus
	 * at once (libogc2 calls that a failed send) is switching to this ID,
	 * not leaving the game the slot had before. */
	slot->phase = UI_CARD_SLOT_IDLE;
	memcpy(slot->pending, id, UI_CARD_SLOT_ID_LENGTH);
	slot->hasPending = true;
	slot->pendingAfterMs = nowMs + retryMs(slot->sendTries);
}

/* Off the bus: a switch (ours, or one made on the card), or the card taken
 * out. Either way it is read only after it is back and settled. */
static void leave(uiCardSlot_t *slot, uint64_t nowMs)
{
	if(!switching(slot)) {
		slot->sentMs = nowMs;
		slot->deadlineMs = nowMs + UI_CARD_SLOT_DEADLINE_MS;
		slot->tries = 0u;
		slot->broken = false;
	}
	slot->phase = UI_CARD_SLOT_AWAY;
	slot->sawAway = true;
}

bool UICardSlot_Observe(uiCardSlot_t *slot, bool present, uint64_t nowMs)
{
	uiCardSlotPhase_t was;
	bool fell, rose;

	if(slot == NULL) return false;
	was = slot->phase;
	fell = slot->seen && slot->present && !present;
	rose = slot->seen && !slot->present && present;
	slot->present = present;
	slot->seen = true;
	if(switching(slot) && nowMs >= slot->deadlineMs) {
		slot->phase = UI_CARD_SLOT_FAILED;
		return true;
	}
	switch(slot->phase) {
	case UI_CARD_SLOT_SENT:
		if(!present) leave(slot, nowMs);
		else if(nowMs >= slot->sentMs + UI_CARD_SLOT_DETACH_MS) {
			slot->phase = UI_CARD_SLOT_LOADING;
			slot->nextMs = nowMs;
		}
		break;
	case UI_CARD_SLOT_AWAY:
		if(present) {
			slot->phase = UI_CARD_SLOT_BACK;
			slot->backMs = nowMs;
		}
		break;
	case UI_CARD_SLOT_BACK:
		if(!present) leave(slot, nowMs);
		else if(nowMs >= slot->backMs + UI_CARD_SLOT_SETTLE_MS) {
			slot->phase = UI_CARD_SLOT_LOADING;
			slot->nextMs = nowMs;
		}
		break;
	case UI_CARD_SLOT_LOADING:
		if(!present) leave(slot, nowMs);
		break;
	case UI_CARD_SLOT_READY:
		/* A card that took a GameID left the bus: switching again. */
		if(fell && slot->mmce) leave(slot, nowMs);
		break;
	case UI_CARD_SLOT_FAILED:
		/* Never sent: put back, it is sent the ID again once settled (read,
		 * it would be another game's card). Never loaded: gone for good
		 * stays given up; back again, or changed, is tried again from the
		 * start. */
		if(slot->unsent) {
			if(rose) {
				slot->phase = UI_CARD_SLOT_IDLE;
				slot->unsent = false;
				memcpy(slot->pending, slot->id, UI_CARD_SLOT_ID_LENGTH);
				slot->hasPending = true;
				slot->pendingAfterMs = nowMs + UI_CARD_SLOT_SETTLE_MS;
			}
		}
		else if(fell && slot->mmce) leave(slot, nowMs);
		else if(rose && slot->mmce) {
			leave(slot, nowMs);
			slot->phase = UI_CARD_SLOT_BACK;
			slot->backMs = nowMs;
		}
		break;
	case UI_CARD_SLOT_IDLE:
	default:
		break;
	}
	return slot->phase != was;
}

bool UICardSlot_ReadDue(const uiCardSlot_t *slot, uint64_t nowMs)
{
	return slot != NULL && slot->phase == UI_CARD_SLOT_LOADING &&
		nowMs >= slot->nextMs;
}

void UICardSlot_Hint(uiCardSlot_t *slot, uint64_t nowMs)
{
	if(slot != NULL && slot->phase == UI_CARD_SLOT_LOADING && slot->nextMs > nowMs) {
		slot->nextMs = nowMs;
	}
}

bool UICardSlot_Read(uiCardSlot_t *slot, uiCardReadResult_t result,
	uint64_t nowMs, bool *resetProbe)
{
	if(resetProbe != NULL) *resetProbe = false;
	if(slot == NULL || slot->phase != UI_CARD_SLOT_LOADING) return false;
	if(result == UI_CARD_READ_OK) {
		slot->phase = UI_CARD_SLOT_READY;
		slot->broken = false;
		return true;
	}
	slot->tries++;
	slot->broken = result == UI_CARD_READ_BROKEN;
	slot->nextMs = nowMs + retryMs(slot->tries);
	/* An emulator that took our ID doesn't turn into another device: this is
	 * the ID libogc2 read while it was switching, kept until it leaves the
	 * bus again. Reset the probe; the next read reads the ID afresh. */
	if(result == UI_CARD_READ_NOT_A_CARD && resetProbe != NULL) *resetProbe = true;
	return false;
}

bool UICardSlot_TakePending(uiCardSlot_t *slot, char id[UI_CARD_SLOT_ID_LENGTH],
	uint64_t nowMs)
{
	if(slot == NULL || id == NULL || !slot->hasPending || switching(slot) ||
		nowMs < slot->pendingAfterMs) {
		return false;
	}
	memcpy(id, slot->pending, UI_CARD_SLOT_ID_LENGTH);
	slot->hasPending = false;
	return true;
}

bool UICardSlot_Busy(const uiCardSlot_t *slot)
{
	return slot != NULL && (switching(slot) || slot->hasPending);
}

bool UICardSlot_ReadableFor(const uiCardSlot_t *slot,
	const char id[UI_CARD_SLOT_ID_LENGTH])
{
	if(slot == NULL || id == NULL || slot->hasPending) return false;
	return slot->phase == UI_CARD_SLOT_IDLE ||
		(slot->phase == UI_CARD_SLOT_READY && sameId(slot->id, id));
}

bool UICardSlot_WaitingFor(const uiCardSlot_t *slot,
	const char id[UI_CARD_SLOT_ID_LENGTH])
{
	if(slot == NULL || id == NULL) return false;
	if(slot->hasPending) return sameId(slot->pending, id);
	return switching(slot) && sameId(slot->id, id);
}

bool UICardSlot_FailedFor(const uiCardSlot_t *slot,
	const char id[UI_CARD_SLOT_ID_LENGTH])
{
	return slot != NULL && id != NULL && !slot->hasPending &&
		slot->phase == UI_CARD_SLOT_FAILED && sameId(slot->id, id);
}

bool UICardSlot_WritableFor(const uiCardSlot_t *slot,
	const char id[UI_CARD_SLOT_ID_LENGTH], uint64_t nowMs)
{
	if(!UICardSlot_ReadableFor(slot, id)) return false;
	if(slot->phase == UI_CARD_SLOT_IDLE) return true;
	return slot->sawAway || nowMs >= slot->sentMs + UI_CARD_SLOT_QUIET_MS;
}
