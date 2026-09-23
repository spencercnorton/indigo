#include "ui_perf.h"

#if UI_PERF_CAPTURE

#include <string.h>
#include <ogc/lwp_watchdog.h>

#define UI_PERF_U32_MAX ((u32)-1)
#define UI_PERF_U64_MAX ((u64)-1)

static const u32 thresholds[UI_PERF_METRIC_COUNT] = {
	[UI_PERF_METRIC_FRAME_PERIOD] = UI_PERF_FRAME_PERIOD_THRESHOLD_US,
	[UI_PERF_METRIC_FRAME_WORK] = UI_PERF_FRAME_WORK_THRESHOLD_US,
	[UI_PERF_METRIC_BACKGROUND_CPU_SUBMIT] = UI_PERF_BACKGROUND_CPU_SUBMIT_THRESHOLD_US
};
static uiPerfSnapshot_t capture;

static void incrementSaturatedU64(u64 *value)
{
	if(*value != UI_PERF_U64_MAX) {
		(*value)++;
	}
}

static void addSaturatedU64(u64 *value, u64 increment)
{
	if(UI_PERF_U64_MAX - *value < increment) {
		*value = UI_PERF_U64_MAX;
	}
	else {
		*value += increment;
	}
}

void UIPerf_Reset(void)
{
	memset(&capture, 0, sizeof(capture));
}

u64 UIPerf_Now(void)
{
	return gettime();
}

void UIPerf_RecordTicks(uiPerfMetric_t metric, u64 startTicks, u64 endTicks)
{
	u64 elapsedTicks = diff_ticks(startTicks, endTicks);
	UIPerf_RecordMicroseconds(metric, ticks_to_microsecs(elapsedTicks));
}

void UIPerf_RecordMicroseconds(uiPerfMetric_t metric, u64 elapsedMicroseconds)
{
	uiPerfMetricSnapshot_t *result;
	u32 bin;

	if((u32)metric >= UI_PERF_METRIC_COUNT) {
		return;
	}

	result = &capture.metrics[metric];
	incrementSaturatedU64(&result->count);
	addSaturatedU64(&result->sumMicroseconds, elapsedMicroseconds);
	if(elapsedMicroseconds > result->maxMicroseconds) {
		result->maxMicroseconds = elapsedMicroseconds;
	}
	if(elapsedMicroseconds >= thresholds[metric]) {
		incrementSaturatedU64(&result->thresholdExceedances);
	}

	if(elapsedMicroseconds >= UI_PERF_HISTOGRAM_OVERFLOW_US) {
		bin = UI_PERF_HISTOGRAM_OVERFLOW_BIN;
	}
	else {
		bin = (u32)elapsedMicroseconds / UI_PERF_HISTOGRAM_BIN_WIDTH_US;
	}
	if(result->histogram[bin] != UI_PERF_U32_MAX) {
		result->histogram[bin]++;
	}
}

void UIPerf_Snapshot(uiPerfSnapshot_t *snapshot)
{
	if(snapshot) {
		memcpy(snapshot, &capture, sizeof(*snapshot));
		for(u32 i = 0; i < UI_PERF_METRIC_COUNT; i++) {
			snapshot->metrics[i].thresholdMicroseconds = thresholds[i];
		}
	}
}

u64 UIPerf_PercentileUs(const uiPerfMetricSnapshot_t *metric, u32 percentile)
{
	u64 histogramCount = 0;
	u64 rank;
	u64 cumulative = 0;
	u32 i;

	if(!metric || percentile == 0) {
		return 0;
	}
	if(percentile > 100) {
		percentile = 100;
	}

	for(i = 0; i < UI_PERF_HISTOGRAM_BIN_COUNT; i++) {
		histogramCount += metric->histogram[i];
	}
	if(histogramCount == 0) {
		return 0;
	}

	/* ceil(histogramCount * percentile / 100), arranged to avoid overflow. */
	rank = (histogramCount / 100u) * percentile;
	rank += (((histogramCount % 100u) * percentile) + 99u) / 100u;

	for(i = 0; i < UI_PERF_HISTOGRAM_BIN_COUNT; i++) {
		cumulative += metric->histogram[i];
		if(cumulative >= rank) {
			if(i == UI_PERF_HISTOGRAM_OVERFLOW_BIN) {
				return metric->maxMicroseconds > UI_PERF_HISTOGRAM_OVERFLOW_US ?
					metric->maxMicroseconds : UI_PERF_HISTOGRAM_OVERFLOW_US;
			}
			return (u64)(i + 1u) * UI_PERF_HISTOGRAM_BIN_WIDTH_US;
		}
	}

	return metric->maxMicroseconds;
}

#endif
