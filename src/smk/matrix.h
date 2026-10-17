#pragma once

#include <stdint.h>

#include "kbdef.h"

void    matrix_init();
uint8_t matrix_task();

void matrix_scan_full();

// Number of matrix scans taken by the tick.
uint16_t matrix_scan_count(void);

// Sampling times in microseconds. Increase them only for boards with slow row
// lines.
#ifndef MATRIX_SETTLE_US
#    define MATRIX_SETTLE_US 10
#endif
#ifndef MATRIX_SAMPLE_GAP_US
#    define MATRIX_SAMPLE_GAP_US 10
#endif

// Current scan state: one byte per column, one bit per row. Use it to check
// whether a key is held.
#define MATRIX_ROW_BIT(row) ((uint8_t)(1u << (row)))
extern uint8_t matrix_snapshot[MATRIX_COLS];
