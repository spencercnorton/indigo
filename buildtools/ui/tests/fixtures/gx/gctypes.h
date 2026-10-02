/* libogc's integer types, for test_frame_budget.py's host build. */
#ifndef STUB_GCTYPES_H
#define STUB_GCTYPES_H
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
typedef int8_t s8; typedef int16_t s16; typedef int32_t s32;
#define ATTRIBUTE_ALIGN(x) __attribute__((aligned(x)))
#endif
