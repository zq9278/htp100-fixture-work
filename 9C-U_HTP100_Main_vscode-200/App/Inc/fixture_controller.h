#ifndef FIXTURE_CONTROLLER_H
#define FIXTURE_CONTROLLER_H
#include "fixture_wire.h"
typedef struct {
    uint32_t (*now)(void);
    void (*off)(void);
    /* eye probe must read the EEPROM identity afresh, without writing it. */
    bool (*probe)(void);
    bool (*heater)(float target,float *measured);
    uint8_t (*prepare)(bool demo);
    bool (*send)(const uint8_t *,uint16_t);
    /* Read-only consumption check, independent of the disabled write/fuse switch. */
    uint8_t (*check_usable)(void);
    void (*status_led)(uint8_t state);
    /* NULL resets current-item diagnostics; otherwise returns the latest sample. */
    void (*diagnostics)(FxDiagnostics *out);
} FixturePort;
void Fixture_Init(const FixturePort *port);
void Fixture_Feed(const uint8_t *data,size_t size);
void Fixture_Tick(void);
void Fixture_PowerLost(void);
#endif
