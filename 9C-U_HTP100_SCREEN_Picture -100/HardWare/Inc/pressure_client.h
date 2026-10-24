#ifndef PRESSURE_CLIENT_H
#define PRESSURE_CLIENT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pressure_monitor.h"
#define PC_POINT_MAX 8U
typedef struct { uint32_t (*now)(void);bool (*send)(const uint8_t *,uint16_t); } PressureClientPort;
typedef struct { int32_t zero;float scale,area;uint8_t flags;uint32_t generation;bool dirty; } PcCalibration;
typedef struct { float grams,raw,sd;uint32_t count; } PcPoint;
typedef struct {
    bool connected,config_valid,fit_valid;
    uint8_t pending,point_count,peak_state;
    uint16_t flags;
    uint32_t device_ms,elapsed,duration,samples,errors,peak_at,updates;
    float pressure,filtered,peak,minimum,sample_area;
    int32_t raw;
    uint8_t soc;
    uint16_t mv;
    PcCalibration config,fit;
    PcPoint points[PC_POINT_MAX];
    float fit_rms,fit_max;
    const char *message;
} PressureClientState;
void PressureClient_Init(const PressureClientPort *);
void PressureClient_Feed(const uint8_t *,size_t);
void PressureClient_Tick(void);
const PressureClientState *PressureClient_Status(void);
bool PressureClient_Command(uint8_t command,uint32_t duration_ms);
bool PressureClient_Point(float grams);
bool PressureClient_Fit(float area);
void PressureClient_RemovePoint(bool all);
void PressureClient_InvalidateFit(void);
float PressureClient_Mmhg(float grams,float area);
float PressureClient_Force(float mmhg,float area);
float PressureClient_Grams(float mmhg,float area);
#endif
