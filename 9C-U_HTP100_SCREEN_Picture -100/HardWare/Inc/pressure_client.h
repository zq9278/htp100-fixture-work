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
/* A calibration run covers 0/20/40/60/80/100 percent, then an independent
 * ascending/descending verification over three complete loading cycles. */
enum { PC_W_OFF,PC_W_PRELOAD,PC_W_LOAD,PC_W_FIT,PC_W_VERIFY,PC_W_RESULT,PC_W_SAVED,PC_W_INTERRUPTED };
#define PC_W_CAL_POINTS 6U
#define PC_W_CHECK_POINTS 11U
#define PC_W_REPEATS 3U
typedef struct { float target,before,after,spread; } PcCheck;
typedef struct {
    uint8_t phase,step,repeat,checks;
    bool collecting,applied,passed;
    uint32_t revision;
    float capacity,area,tolerance,target,max_error,repeatability,hysteresis,zero_error;
    PcCalibration original;
    PcCheck calibration[PC_W_CAL_POINTS],verification[PC_W_CHECK_POINTS];
} PcWizard;
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
    PcWizard wizard;
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
bool PressureClient_WizardStart(float capacity_g,float area_mm2,float tolerance_g);
bool PressureClient_WizardNext(void);
bool PressureClient_WizardRetry(void);
bool PressureClient_WizardCancel(void);
bool PressureClient_WizardActive(void);
#endif
