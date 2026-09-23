#ifndef UI_PERF_H
#define UI_PERF_H

#include <gctypes.h>

/* Diagnostic builds opt in with -DUI_PERF_CAPTURE=1. Normal builds retain
 * no capture state and the hot-path instrumentation macros evaluate nothing. */
#ifndef UI_PERF_CAPTURE
#define UI_PERF_CAPTURE 0
#endif

#define UI_PERF_HISTOGRAM_BIN_COUNT 256u
#define UI_PERF_HISTOGRAM_BIN_WIDTH_US 250u
#define UI_PERF_HISTOGRAM_OVERFLOW_BIN (UI_PERF_HISTOGRAM_BIN_COUNT - 1u)
#define UI_PERF_HISTOGRAM_OVERFLOW_US \
	(UI_PERF_HISTOGRAM_OVERFLOW_BIN * UI_PERF_HISTOGRAM_BIN_WIDTH_US)

#define UI_PERF_FRAME_PERIOD_THRESHOLD_US 25000u
#define UI_PERF_FRAME_WORK_THRESHOLD_US 12000u
#define UI_PERF_BACKGROUND_CPU_SUBMIT_THRESHOLD_US 2000u

typedef enum uiPerfMetric {
	UI_PERF_METRIC_FRAME_PERIOD = 0,
	UI_PERF_METRIC_FRAME_WORK,
	UI_PERF_METRIC_BACKGROUND_CPU_SUBMIT,
	UI_PERF_METRIC_COUNT
} uiPerfMetric_t;

typedef struct uiPerfMetricSnapshot {
	u64 count;
	u64 sumMicroseconds;
	u64 maxMicroseconds;
	u64 thresholdExceedances;
	u32 thresholdMicroseconds;
	u32 histogram[UI_PERF_HISTOGRAM_BIN_COUNT];
} uiPerfMetricSnapshot_t;

typedef struct uiPerfSnapshot {
	uiPerfMetricSnapshot_t metrics[UI_PERF_METRIC_COUNT];
} uiPerfSnapshot_t;

#if UI_PERF_CAPTURE

/* Capture has one writer: the video thread. Reset and Snapshot must run on
 * that thread, or while it is quiescent, so the copied aggregates are stable. */
void UIPerf_Reset(void);
u64 UIPerf_Now(void);
void UIPerf_RecordTicks(uiPerfMetric_t metric, u64 startTicks, u64 endTicks);
void UIPerf_RecordMicroseconds(uiPerfMetric_t metric, u64 elapsedMicroseconds);
void UIPerf_Snapshot(uiPerfSnapshot_t *snapshot);

/* Returns the selected histogram bin's upper edge in microseconds. If the
 * rank lands in the overflow bin, the observed maximum is returned instead. */
u64 UIPerf_PercentileUs(const uiPerfMetricSnapshot_t *metric, u32 percentile);

#define UI_PERF_BEGIN(name) u64 name = UIPerf_Now()
#define UI_PERF_END(metric, name) \
	UIPerf_RecordTicks((metric), (name), UIPerf_Now())

#else

#define UIPerf_Reset() ((void)0)
#define UIPerf_Now() ((u64)0)
#define UIPerf_RecordTicks(metric, startTicks, endTicks) ((void)0)
#define UIPerf_RecordMicroseconds(metric, elapsedMicroseconds) ((void)0)
#define UIPerf_Snapshot(snapshot) ((void)(snapshot))
#define UIPerf_PercentileUs(metric, percentile) ((u64)0)

#define UI_PERF_BEGIN(name) ((void)0)
#define UI_PERF_END(metric, name) ((void)0)

#endif

#endif
