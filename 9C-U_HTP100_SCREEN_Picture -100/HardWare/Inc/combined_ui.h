#ifndef COMBINED_UI_H
#define COMBINED_UI_H
#include <stddef.h>
#include <stdint.h>
void CombinedUi_Init(void);
void CombinedUi_Feed(const uint8_t *,size_t);
void CombinedUi_Tick(void);
void CombinedUi_OpenPressure(void);
void CombinedUi_ExitPressure(void);
#endif
