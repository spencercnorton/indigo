#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "ui_home_safety.h"

static unsigned long checks;
static unsigned long failures;

#define CHECK(condition) do { \
	checks++; \
	if(!(condition)) { \
		failures++; \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
	} \
} while(0)

static void test_source_identity_and_availability(void)
{
	uiHomeSourceLifecycle_t lifecycle = {0};
	int sourceA = 0;
	int sourceB = 1;

	CHECK(!UIHomeSafety_SourceMounted(&lifecycle, &sourceA));
	CHECK(!UIHomeSafety_SourceReady(&lifecycle, &sourceA, true));
	CHECK(!UIHomeSafety_SourceReady(NULL, &sourceA, true));

	UIHomeSafety_RecordSource(&lifecycle, &sourceA,
		UI_HOME_SOURCE_MOUNT_UNMOUNTED);
	CHECK(!UIHomeSafety_SourceMounted(&lifecycle, &sourceA));
	CHECK(!UIHomeSafety_SourceReady(&lifecycle, &sourceA, true));

	UIHomeSafety_RecordSource(&lifecycle, &sourceA,
		UI_HOME_SOURCE_MOUNT_MOUNTED);
	CHECK(UIHomeSafety_SourceMounted(&lifecycle, &sourceA));
	CHECK(UIHomeSafety_SourceReady(&lifecycle, &sourceA, true));
	CHECK(!UIHomeSafety_SourceReady(&lifecycle, &sourceA, false));
	CHECK(!UIHomeSafety_SourceMounted(&lifecycle, &sourceB));
	CHECK(!UIHomeSafety_SourceReady(&lifecycle, &sourceB, true));

	UIHomeSafety_RecordSource(&lifecycle, &sourceB,
		UI_HOME_SOURCE_MOUNT_UNMOUNTED);
	CHECK(!UIHomeSafety_SourceMounted(&lifecycle, &sourceA));
	CHECK(!UIHomeSafety_SourceMounted(&lifecycle, &sourceB));

	UIHomeSafety_RecordSource(&lifecycle, NULL,
		UI_HOME_SOURCE_MOUNT_MOUNTED);
	CHECK(lifecycle.handler == NULL);
	CHECK(lifecycle.state == UI_HOME_SOURCE_MOUNT_ABSENT);
	CHECK(!UIHomeSafety_SourceReady(&lifecycle, NULL, true));
}

static void test_recent_reinit_policy(void)
{
	uiHomeSourceLifecycle_t lifecycle = {0};
	int current = 0;
	int different = 1;

	UIHomeSafety_RecordSource(&lifecycle, &current,
		UI_HOME_SOURCE_MOUNT_UNMOUNTED);
	CHECK(UIHomeSafety_ShouldForceRecentInit(&lifecycle, &current,
		&current, true));
	CHECK(UIHomeSafety_ShouldForceRecentInit(&lifecycle, &current,
		&current, false));

	UIHomeSafety_RecordSource(&lifecycle, &current,
		UI_HOME_SOURCE_MOUNT_MOUNTED);
	CHECK(!UIHomeSafety_ShouldForceRecentInit(&lifecycle, &current,
		&current, true));
	CHECK(UIHomeSafety_ShouldForceRecentInit(&lifecycle, &current,
		&current, false));
	CHECK(!UIHomeSafety_ShouldForceRecentInit(&lifecycle, &current,
		&different, true));
	CHECK(!UIHomeSafety_ShouldForceRecentInit(&lifecycle, &current,
		NULL, true));

	/* Startup failure clears DEVICE_CUR. Pointer inequality then forces the
	 * existing lookup path to call init for the remembered target. */
	UIHomeSafety_RecordSource(&lifecycle, NULL,
		UI_HOME_SOURCE_MOUNT_ABSENT);
	CHECK(!UIHomeSafety_ShouldForceRecentInit(&lifecycle, NULL,
		&current, false));
	CHECK(!UIHomeSafety_SourceReady(&lifecycle, NULL, true));
}

static void test_remount_transitions(void)
{
	uiHomeSourceLifecycle_t lifecycle = {0};
	int source = 0;

	UIHomeSafety_RecordSource(&lifecycle, &source,
		UI_HOME_SOURCE_MOUNT_MOUNTED);
	CHECK(UIHomeSafety_SourceReady(&lifecycle, &source, true));

	/* Both WKF Refresh and Flippy update publish Unmounted before any scan. */
	UIHomeSafety_RecordSource(&lifecycle, &source,
		UI_HOME_SOURCE_MOUNT_UNMOUNTED);
	CHECK(!UIHomeSafety_SourceReady(&lifecycle, &source, true));

	/* Successful explicit init/remount. */
	UIHomeSafety_RecordSource(&lifecycle, &source,
		UI_HOME_SOURCE_MOUNT_MOUNTED);
	CHECK(UIHomeSafety_SourceReady(&lifecycle, &source, true));

	/* Failed remount removes the current identity. */
	UIHomeSafety_RecordSource(&lifecycle, NULL,
		UI_HOME_SOURCE_MOUNT_ABSENT);
	CHECK(!UIHomeSafety_SourceReady(&lifecycle, &source, true));
}

static void test_selector_release_union(void)
{
	const uint32_t selectorMask = 0x1FFu;

	CHECK(!UIHomeSafety_SelectorReleasePending(0u, selectorMask, 0, 0, 24));
	for(unsigned bit = 0u; bit < 9u; ++bit) {
		CHECK(UIHomeSafety_SelectorReleasePending(1u << bit, selectorMask,
			0, 0, 24));
	}
	CHECK(!UIHomeSafety_SelectorReleasePending(1u << 12, selectorMask,
		0, 0, 24));
	CHECK(!UIHomeSafety_SelectorReleasePending(0u, selectorMask, 23, -23, 24));
	CHECK(UIHomeSafety_SelectorReleasePending(0u, selectorMask, 24, 0, 24));
	CHECK(UIHomeSafety_SelectorReleasePending(0u, selectorMask, -24, 0, 24));
	CHECK(UIHomeSafety_SelectorReleasePending(0u, selectorMask, 0, 24, 24));
	CHECK(UIHomeSafety_SelectorReleasePending(0u, selectorMask, 0, -24, 24));
	CHECK(UIHomeSafety_SelectorReleasePending(0u, selectorMask,
		INT_MIN, INT_MAX, 24));
	CHECK(UIHomeSafety_SelectorReleasePending(0u, selectorMask, 0, 0, 0));
}

static void test_restart_digital_release_and_late_back(void)
{
	const uint32_t confirmationMask = 0x7Fu;
	const uint32_t back = 1u << 1;
	bool cancel = false;

	CHECK(!UIHomeSafety_RestartReleasePending(0u, confirmationMask));
	for(unsigned bit = 0u; bit < 7u; ++bit) {
		CHECK(UIHomeSafety_RestartReleasePending(1u << bit,
			confirmationMask));
	}
	CHECK(!UIHomeSafety_RestartReleasePending(1u << 12,
		confirmationMask));

	/* Analog state is deliberately absent: release depends on digital input. */
	cancel = UIHomeSafety_AccumulateRestartCancel(cancel, 1u << 0, back);
	CHECK(!cancel);
	cancel = UIHomeSafety_AccumulateRestartCancel(cancel, back, back);
	CHECK(cancel);
	CHECK(UIHomeSafety_AccumulateRestartCancel(true, 0u, back));
	CHECK(!UIHomeSafety_AccumulateRestartCancel(false, 0u, back));
}

int main(void)
{
	test_source_identity_and_availability();
	test_recent_reinit_policy();
	test_remount_transitions();
	test_selector_release_union();
	test_restart_digital_release_and_late_back();

	if(failures != 0u) {
		fprintf(stderr, "ui_home_safety: %lu/%lu checks failed\n",
			failures, checks);
		return 1;
	}
	printf("ui_home_safety: %lu checks passed\n", checks);
	return 0;
}
