#ifndef COMBINED_UI_H
#define COMBINED_UI_H
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
bool CombinedUi_Ready(void);
void CombinedUi_Init(void);
void CombinedUi_Feed(const uint8_t *,size_t);
void CombinedUi_Tick(void);
void CombinedUi_OpenPressure(void);
void CombinedUi_ExitPressure(void);
#endif
