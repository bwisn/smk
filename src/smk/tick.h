#pragma once

#include <stdint.h>

// The keyboard's realtime work, driven by the platform's periodic tick.
// The tick alternates between matrix scans and LED PWM subframes.

void tick_init(void);

void tick_dispatch(void);

void tick_pause(void);
void tick_resume(void);

// Milliseconds since tick start. The count uses slot lengths, so it follows
// wall time. The ISR writes the low byte; the reader tracks the high byte.
uint16_t tick_millis(void);

void tick_time_advance(uint16_t us);
