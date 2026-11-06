#ifndef PRESSURE_UI_H
#define PRESSURE_UI_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
void PressureUi_Init(void);
void PressureUi_Feed(const uint8_t *,size_t);
void PressureUi_Tick(void);
bool PressureUi_Leave(void);
#endif
