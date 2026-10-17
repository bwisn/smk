#include "extint.h"
#include "sh68f90.h"

// INT4 edge and trigger-mode values. The datasheet does not document these two
// registers; these values are known to work.
#define IENC_INT4_CONFIG  0xF3
#define EXF0_INT4_TRIGGER 0x40

void extint_wake_clear(void)
{
    EXF1 = 0;
}

void extint_wake_arm(void)
{
    extint_wake_clear();
    IENC = IENC_INT4_CONFIG;
    EXF0 = EXF0_INT4_TRIGGER;
    EX4  = 1;
    // Enable the interrupt at the CPU as well (IEN0 |= 2) when arming the
    // wake source, so the wake reaches the handler.
    IEN0 |= 2;
}

void extint_wake_disable(void)
{
    EX4 = 0;
    extint_wake_clear();
}
