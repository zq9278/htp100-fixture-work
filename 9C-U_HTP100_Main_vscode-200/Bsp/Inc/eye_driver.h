#ifndef EYE_DRIVER_H
#define EYE_DRIVER_H

#include <stdbool.h>

#include "app_types.h"
#include "fixture_wire.h"

void EyeDriver_Init(void);
void EyeDriver_DiagnosticsTick(void);
bool EyeDriver_ReadTemperature(float *temperature_c);
bool EyeDriver_ReadTemperatureTelemetry(float *temperature_c);
AppEyeState EyeDriver_ReadState(void);
bool EyeDriver_InitialStateConfirmed(void);
bool EyeDriver_LastProbeValid(void);
bool EyeDriver_MarkConsumed(void);
void EyeDriver_Erase(void);
bool EyeDriver_FixtureProbe(void);
uint8_t EyeDriver_FixtureCheckUsable(void);
uint8_t EyeDriver_FixtureClearMarker(void);
void EyeDriver_FixtureDiagnostics(FxDiagnostics *out);
uint8_t EyeDriver_FixturePrepare(bool demo);

#endif
