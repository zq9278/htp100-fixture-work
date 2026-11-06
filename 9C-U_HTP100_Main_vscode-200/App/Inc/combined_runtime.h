#ifndef COMBINED_RUNTIME_H
#define COMBINED_RUNTIME_H
#include "combined_wire.h"
#include "fixture_controller.h"
static inline void Combined_Dispatch(bool pressure,const uint8_t *frame,size_t length) {
    if(pressure) PressureMonitor_Feed(frame,length);
    else {
        /* An auxiliary PC may own a one-second collection when H7 exits.
         * Cancel it before handing the hardware back to the consumable tool. */
        if(frame[3]==FX_SELECT && frame[6]==FX_NONE) PressureMonitor_CancelCollection();
        Fixture_Feed(frame,length);
    }
}
#endif
