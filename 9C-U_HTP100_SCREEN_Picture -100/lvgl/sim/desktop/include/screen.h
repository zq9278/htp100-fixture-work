#ifndef SCREEN_H
#define SCREEN_H

#include "lvgl.h"
#include <stdint.h>

extern lv_obj_t *global_panel;
extern uint8_t EYE_status;
void screen_refresh_prepare_ui(void);
void screen_refresh_runtime_ui(void);

#endif
