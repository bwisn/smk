#include "matrix.h"
#include "report.h"
#include "debug.h"
#include "layout.h"
#include "user_layout.h"
#include "kb.h"
#include "user_matrix.h"
#include "kbdef.h"
#include "host.h"
#include "delay.h"
#include "indicators.h"
#include "sleep.h"
#include <stdlib.h>
#include <stdbool.h>

typedef uint8_t matrix_col_t;

matrix_col_t matrix[MATRIX_COLS];
uint8_t     matrix_snapshot[MATRIX_COLS];

// Matrix events, filled by the tick and drained by the main loop. The queue
// keeps a press and release between two main-loop runs as two events.
#define MATRIX_EVENT_QUEUE_SIZE 64 // a power of two
#define MATRIX_EVENT_QUEUE_MASK (MATRIX_EVENT_QUEUE_SIZE - 1)

// The event: column in the low four bits, row in the next three bits, pressed
// flag in the top bit.
#define MATRIX_EVENT_COL(e)   ((uint8_t)((e) & 0x0F))
#define MATRIX_EVENT_ROW(e)   ((uint8_t)(((e) >> 4) & 0x07))
#define MATRIX_EVENT_DOWN(e) ((uint8_t)(((e) & 0x80) != 0))

static volatile uint8_t matrix_events[MATRIX_EVENT_QUEUE_SIZE];
static volatile uint8_t matrix_event_head; // the tick writes
static volatile uint8_t matrix_event_tail; // the main loop reads

// The state of the last scan, per column. The tick diffs against it.
static uint8_t matrix_sampled[MATRIX_COLS];

// The state the main loop has processed, per column.
static uint8_t matrix_processed[MATRIX_COLS];

// Set when an event could not be queued; the main loop then resyncs.
static volatile bool matrix_resync;

// Scans are counted in the tick.
static uint16_t matrix_scans;

uint8_t action_layer;

uint8_t default_layer;

void matrix_init()
{
    action_layer       = 0;
    default_layer      = 0;
    matrix_event_head  = 0;
    matrix_event_tail  = 0;
    matrix_resync      = false;
    matrix_scans       = 0;

    for (int i = 0; i < MATRIX_COLS; i++) {
        matrix[i]           = 0;
        matrix_sampled[i]   = 0;
        matrix_processed[i] = 0;
        matrix_snapshot[i]  = 0;
    }
}

void set_default_layer(uint8_t layer)
{
    default_layer = layer;
}

static uint16_t resolve_keycode(uint16_t base, uint8_t row, uint8_t col)
{
    if (!action_layer) {
        return base;
    }

    const uint16_t overlay = keymaps[action_layer][row][col];
    return (overlay == KC_TRANSPARENT) ? base : overlay;
}

static void send_keycode(uint16_t qcode, bool pressed)
{
    if (IS_MODIFIER_KEYCODE(qcode)) {
        if (pressed) {
            add_mods(MOD_BIT((uint8_t)(qcode & 0xFF)));
        } else {
            del_mods(MOD_BIT((uint8_t)(qcode & 0xFF)));
        }
        send_keyboard_report();
        return;
    }

    if (IS_BASIC_KEYCODE(qcode)) {
        if (pressed) {
            add_key((uint8_t)(qcode & 0xFF));
        } else {
            del_key((uint8_t)(qcode & 0xFF));
        }
        send_keyboard_report();
        return;
    }

    if (IS_SYSTEM_KEYCODE(qcode)) {
        host_system_send(pressed ? keycode_to_system(qcode) : 0);
        return;
    }

    if (IS_CONSUMER_KEYCODE(qcode)) {
        host_consumer_send(pressed ? keycode_to_consumer(qcode) : 0);
        return;
    }
}

static void process_key_state(uint8_t row, uint8_t col, bool pressed)
{
    const uint16_t base = keymaps[default_layer][row][col];

    if (IS_QK_MOMENTARY(base)) {
        if (pressed) {
            action_layer = QK_MOMENTARY_GET_LAYER(base);
        } else {
            clear_keys();
            action_layer = 0;
        }
        return;
    }

    const uint16_t qcode = resolve_keycode(base, row, col);

    if (!kb_process_record(qcode, pressed)) {
        return;
    }

    if (!layout_process_record(qcode, pressed)) {
        return;
    }

    send_keycode(qcode, pressed);
}

static void matrix_push_event(uint8_t col, uint8_t row, uint8_t pressed)
{
    const uint8_t head = matrix_event_head;
    const uint8_t next = (uint8_t)((head + 1) & MATRIX_EVENT_QUEUE_MASK);

    if (next == matrix_event_tail) {
        // Queue full: resync after drain repairs the state; only this
        // keystroke is lost.
        matrix_resync = true;
        return;
    }

    // The event is written before the head moves, so the main loop never reads
    // a half written one.
    matrix_events[head] = (uint8_t)(col | (uint8_t)(row << 4) | (pressed ? 0x80 : 0x00));
    matrix_event_head = next;
}

void matrix_scan_full(void)
{
    indicators_pwm_disable();

    user_matrix_sinks_off();

    user_matrix_scan_pre();
    user_matrix_cols_deselect_all();

    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        user_matrix_col_select(col);

        delay_us(MATRIX_SETTLE_US);
        const uint8_t sample1 = user_matrix_read_rows();
        delay_us(MATRIX_SAMPLE_GAP_US);
        const uint8_t sample2 = user_matrix_read_rows();

        if (sample1 == sample2) {
            matrix[col] = ~sample1;
        }

        user_matrix_col_deselect(col);
    }

    user_matrix_scan_post();

    indicators_pwm_enable();

    matrix_scans++;

    // Create events here, where every change is visible. The main loop can be
    // behind and would miss a fast tap.
    for (uint8_t col = 0; col < MATRIX_COLS; col++) {
        const uint8_t changes = matrix[col] ^ matrix_sampled[col];
        if (!changes) {
            continue;
        }

        matrix_sampled[col]  = matrix[col];
        matrix_snapshot[col] = matrix[col];

        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            const uint8_t bit = (uint8_t)(1u << row);
            if (changes & bit) {
                // A set bit in matrix[] means the key is down: the rows are
                // active low, so the scan inverts the sample.
                matrix_push_event(col, row, (matrix[col] & bit) != 0);
            }
        }
    }
}

uint8_t matrix_task()
{
    if (matrix_event_head == matrix_event_tail && !matrix_resync) {
        return false;
    }

    bool matrix_changed = false;

    while (matrix_event_head != matrix_event_tail) {
        const uint8_t event = matrix_events[matrix_event_tail];
        matrix_event_tail = (uint8_t)((matrix_event_tail + 1) & MATRIX_EVENT_QUEUE_MASK);

        const uint8_t col = MATRIX_EVENT_COL(event);
        const uint8_t row = MATRIX_EVENT_ROW(event);
        const uint8_t bit = (uint8_t)(1u << row);
        const bool pressed = MATRIX_EVENT_DOWN(event);

        if (pressed) {
            matrix_processed[col] |= bit;
        } else {
            matrix_processed[col] &= (uint8_t)~bit;
        }

        matrix_changed = true;
        sleep_note_activity();

        process_key_state(row, col, pressed);
    }

    if (matrix_resync) {
        // The queue overflowed: send the state the main loop missed, so no key
        // stays down after it was released.
        matrix_resync = false;

        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            const uint8_t changes = matrix[col] ^ matrix_processed[col];
            if (!changes) {
                continue;
            }

            matrix_processed[col] = matrix[col];

            for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
                const uint8_t bit = (uint8_t)(1u << row);
                if (changes & bit) {
                    matrix_changed = true;
                    sleep_note_activity();

                    process_key_state(row, col, (matrix[col] & bit) != 0);
                }
            }
        }
    }

    return matrix_changed;
}

uint16_t matrix_scan_count(void)
{
    return matrix_scans;
}
