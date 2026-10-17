#include "nvm.h"
#include "flash.h"
#include <stdbool.h>
#include <stdint.h>

/* Settings storage:
 *
 *   legacy record  [0x5A][0xA5][len][payload(len)][chk]
 *   journal slot   [0x3C][len][payload(len)][chk]
 *
 * The journal appends one slot per change; the page is erased and rewritten
 * once when no slots are left. A legacy page still loads and new writes chain
 * after its record. */

#define CFG_MAGIC0 0x5Au
#define CFG_MAGIC1 0xA5u
#define CFG_HDR    3u

#define SLOT_MAGIC 0x3Cu
#define SLOT_HDR   2u // magic and length

#define SLOT_BYTES(len)   ((uint16_t)(SLOT_HDR + 1u + (len)))
#define RECORD_AT(offset) (uint16_t)(NVM_BASE + (offset))

#define NVM_LEN_CAN_OVERRUN (NVM_CAPACITY < 255u)

_Static_assert(CFG_HDR + 1u <= NVM_SECTOR_SIZE, "the sector is too small for a record");
_Static_assert(NVM_CAPACITY + SLOT_HDR + 1u <= NVM_SECTOR_SIZE, "a record does not fit one slot");

static uint8_t checksum(const __xdata uint8_t *src, uint8_t len)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += src[i];
    }
    return sum;
}

static __xdata uint8_t page_probe[1];
static __xdata uint8_t frame[CFG_HDR];

static uint8_t page_byte(uint16_t off)
{
    flash_read_into(NVM_WINDOW, RECORD_AT(off), page_probe, 1);
    return page_probe[0];
}

static uint8_t page_checksum(uint16_t off, uint8_t len)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += page_byte(off + i);
    }
    return sum;
}

static void page_read(uint16_t off, __xdata uint8_t *dst, uint8_t len)
{
    flash_read_into(NVM_WINDOW, RECORD_AT(off), dst, len);
}

static bool page_matches(uint16_t off, const __xdata uint8_t *src, uint8_t len)
{
    return flash_matches(NVM_WINDOW, RECORD_AT(off), src, len);
}

static void page_program(uint16_t off, const __xdata uint8_t *src, uint8_t len)
{
    flash_program_from(NVM_WINDOW, RECORD_AT(off), src, len);
}

static void page_erase(void)
{
    flash_erase(NVM_WINDOW, RECORD_AT(0));
}

struct nvm_scan {
    uint16_t rec_off;
    uint8_t  rec_len;
    uint16_t append_off;
    bool     needs_defrag;
};

// Scan the chain and keep the newest record with a valid checksum. A bad slot
// is skipped; the scan falls back to the last good record.
static void nvm_scan(uint8_t len, struct nvm_scan *s)
{
    uint16_t off = 0;

    s->rec_off      = 0;
    s->rec_len      = 0;
    s->append_off   = 0;
    s->needs_defrag = false;

    while (off < NVM_SECTOR_SIZE) {
        const uint8_t m0 = page_byte(off);

        if (m0 == 0xFFu) {
            break;
        }

        if (m0 == CFG_MAGIC0) {
            page_read(off, frame, CFG_HDR);
            if (frame[1] != CFG_MAGIC1 || frame[2] == 0u) {
                s->needs_defrag = true;
                break;
            }
            const uint16_t total = CFG_HDR + (uint16_t)frame[2] + 1u;
            if (total > NVM_SECTOR_SIZE - off) {
                break;
            }
            const uint8_t stored = page_byte(off + CFG_HDR + frame[2]);
            if (page_checksum(off + CFG_HDR, frame[2]) != stored) {
                s->needs_defrag = true;
                break;
            }
            s->rec_off = off + CFG_HDR;
            s->rec_len = frame[2];
            off += total;
            s->append_off = off;
            continue;
        }

        if (m0 == SLOT_MAGIC) {
            page_read(off, frame, SLOT_HDR);
            const uint8_t slot_len = frame[1];
            if (slot_len == 0u || SLOT_BYTES(slot_len) > NVM_SECTOR_SIZE - off) {
                break;
            }
            const uint8_t stored = page_byte(off + SLOT_HDR + slot_len);
            if (page_checksum(off + SLOT_HDR, slot_len) != stored) {
                s->needs_defrag = true;
                break;
            }
            if (slot_len == len) {
                s->rec_off = off + SLOT_HDR;
                s->rec_len = slot_len;
            }
            off += SLOT_BYTES(slot_len);
            s->append_off = off;
            continue;
        }

        // Bad data or a partial write: erase before writing again.
        s->needs_defrag = true;
        break;
    }
}

static void nvm_write_slot(uint16_t off, const __xdata uint8_t *src, uint8_t len)
{
    frame[0] = SLOT_MAGIC;
    frame[1] = len;
    page_program(off, frame, SLOT_HDR);

    page_program(off + SLOT_HDR, src, len);

    frame[0] = checksum(src, len);
    page_program(off + SLOT_HDR + len, frame, 1);
}

static struct nvm_scan nvm_page; // cached page state
static bool            nvm_page_primed;

// Erase the page and write the record again. The ~15 ms stall happens once per
// page or after corruption; at boot the LED scan is stopped.
static void nvm_defrag(const __xdata uint8_t *src, uint8_t len)
{
    page_erase();
    nvm_write_slot(0, src, len);
    nvm_page.rec_off      = SLOT_HDR;
    nvm_page.rec_len      = len;
    nvm_page.append_off   = SLOT_BYTES(len);
    nvm_page.needs_defrag = false;
}

bool nvm_load(__xdata uint8_t *dst, uint8_t len)
{
#if NVM_LEN_CAN_OVERRUN
    if (len > NVM_CAPACITY) {
        return false;
    }
#endif

    nvm_scan(len, &nvm_page);
    nvm_page_primed = true;

    if (nvm_page.rec_len != len) {
        return false;
    }

    if (page_checksum(nvm_page.rec_off, len) != page_byte(nvm_page.rec_off + len)) {
        return false;
    }

    page_read(nvm_page.rec_off, dst, len);
    if (checksum(dst, len) != page_byte(nvm_page.rec_off + len)) {
        for (uint8_t i = 0; i < len; i++) {
            dst[i] = 0;
        }
        return false;
    }

    // Keep room for one more slot; at boot the LED scan is stopped.
    if (nvm_page.append_off + SLOT_BYTES(len) > NVM_SECTOR_SIZE) {
        nvm_defrag(dst, len);
    }

    return true;
}

void nvm_save(const __xdata uint8_t *src, uint8_t len)
{
#if NVM_LEN_CAN_OVERRUN
    if (len > NVM_CAPACITY) {
        return;
    }
#endif

    if (!nvm_page_primed) {
        nvm_scan(len, &nvm_page);
        nvm_page_primed = true;
    }

    if (nvm_page.rec_len == len && page_matches(nvm_page.rec_off, src, len)) {
        return;
    }

    if (nvm_page.append_off + SLOT_BYTES(len) > NVM_SECTOR_SIZE || nvm_page.needs_defrag) {
        // No room, or a bad slot stops the chain.
        nvm_defrag(src, len);
        return;
    }

    nvm_write_slot(nvm_page.append_off, src, len);
    nvm_page.rec_off = nvm_page.append_off + SLOT_HDR;
    nvm_page.rec_len = len;
    nvm_page.append_off += SLOT_BYTES(len);
}
