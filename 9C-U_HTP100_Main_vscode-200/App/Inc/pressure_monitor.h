#ifndef PRESSURE_MONITOR_H
#define PRESSURE_MONITOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Portable measurement engine. Hardware access is confined to this port. */
typedef struct {
    uint32_t (*now)(void);
    bool (*send)(const uint8_t *, uint16_t);
    bool (*read_store)(uint8_t, uint8_t *, size_t);
    bool (*write_store)(uint8_t, const uint8_t *, size_t);
    bool (*send_pc)(const uint8_t *, uint16_t); /* Optional independent USART1. */
} PressureMonitorPort;

enum { PM_HELLO=1, PM_GET_CONFIG=2, PM_TARE=3, PM_POINT=4,
       PM_SET_CONFIG=5, PM_SAVE=6, PM_START=7, PM_STOP=8,
       PM_INFO=0x81, PM_CONFIG=0x82, PM_ACK=0x83, PM_POINT_RESULT=0x84,
       PM_STATUS=0x90 };
enum { PM_OK=0, PM_BAD_REQUEST=1, PM_BUSY=2, PM_SENSOR_ERROR=3,
       PM_UNSTABLE=4, PM_STORAGE_ERROR=5, PM_NOT_ZEROED=6,
       PM_RANGE=7 };
enum { PM_VALID=1, PM_CALIBRATED=2, PM_ZEROED=4, PM_CHARGING=8,
       PM_FULL=16, PM_POWER_VALID=32, PM_DIRTY=64, PM_SATURATED=128,
       PM_ADC_READY=256 };
enum { PM_IDLE=0, PM_RUNNING=1, PM_COMPLETE=2, PM_ABORTED=3 };

#define PM_MAX_PAYLOAD 64U
#define PM_MAX_FRAME (PM_MAX_PAYLOAD + 10U)
#define PM_STORE_A 0x90U
#define PM_STORE_B 0xB0U
#define PM_STORE_SIZE 32U

void PressureMonitor_Init(const PressureMonitorPort *port);
void PressureMonitor_Feed(const uint8_t *data, size_t length);
void PressureMonitor_FeedPc(const uint8_t *data, size_t length);
void PressureMonitor_Sample(bool valid, int32_t raw);
void PressureMonitor_Power(bool valid, bool charging, bool full,
                           uint8_t soc, uint16_t millivolts);
void PressureMonitor_AdcReady(bool ready);
void PressureMonitor_Tick(void);
uint16_t PressureMonitor_Crc(const uint8_t *data, size_t length);

#endif
