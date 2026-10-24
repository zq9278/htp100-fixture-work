#ifndef UI_LANG_H
#define UI_LANG_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_LANG_ZH 0
#define UI_LANG_EN 1

void ui_lang_set(uint16_t lang);
void ui_lang_preselect(uint16_t lang);
uint16_t ui_lang_get(void);
void ui_lang_apply_all(void);
void ui_lang_apply_bee_state(void);
void ui_lang_apply_language_state(void);
void ui_lang_apply_setting_state(void);
void ui_lang_apply_invalid_shield_popup_state(void);

void ui_debug_init_screen(const char *screen_name);
void ui_debug_load_screen(const char *screen_name);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
