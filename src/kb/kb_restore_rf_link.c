#include "kb.h"
#include "keyboard.h"
#include "settings.h"

#ifdef RF_ENABLED
#    include "rf_controller.h"
#endif

void kb_restore_rf_link()
{
#ifdef RF_ENABLED
    rf_set_link((rf_mode_t)user_settings.rf_link);

    keyboard_state.rf_link   = user_settings.rf_link;
    keyboard_state.connected = 1;
    keyboard_state.paired    = 1;
#endif
}
