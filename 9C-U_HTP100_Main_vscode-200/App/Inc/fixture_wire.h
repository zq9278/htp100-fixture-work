#ifndef FIXTURE_WIRE_H
#define FIXTURE_WIRE_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#define FX_FRAME_SIZE 48U
#define FX_VERSION 2U
#define FX_TEMP_INVALID ((int16_t)-32768)
enum { FX_START=1, FX_HEARTBEAT=2, FX_CANCEL=3, FX_STATUS=0x81 };
enum { FX_NONE=0, FX_INITIALIZE=1, FX_DEMO=2, FX_INSPECT=3, FX_CLEAR_MARKER=4 };
enum { FX_IDLE=0, FX_WAIT, FX_WRITE, FX_HEATING, FX_STABLE, FX_PASS, FX_FAIL };
enum { FX_OK=0, FX_NO_EYE, FX_REMOVED, FX_TEMP_COMM, FX_OVER_TEMP,
       FX_TIMEOUT, FX_WRITE_ERROR, FX_VERIFY_ERROR, FX_LINK_LOST,
       FX_CANCELLED, FX_BAD_COMMAND, FX_PWM_ERROR, FX_POWER_LOST,
       FX_USED_EYE, FX_DEMO_EYE, FX_EYE_READ_ERROR };
/* Byte 0..1 E7 7E, 2 version=2, 3 type, 4..5 transaction,
 * 6 mode, 7 state, 8 reason, 9 eye, 10..11 raw sensor centidegrees,
 * 12..13 raw target centidegrees, 14..15 stable ms.
 * 16 flags, 17 last I2C device (7-bit), 18 register, 19 HAL status,
 * 20..21 marker, 22..23 service, 24..25 TMP112 raw, 26 attempts,
 * 27 line levels (bit0 SCL/bit1 SDA), 28..31 HAL I2C error,
 * 32..35 elapsed ms, 36..37 overtemp ms, 38..39 best stable ms,
 * 40 operation (1 read/2 write), 41..45 reserved, 46..47 CRC16/MODBUS.
 * All multibyte fields little endian. Commands zero reserved/status fields. */
enum { FX_DIAG_MARK_VALID=1U, FX_DIAG_SERVICE_VALID=2U,
       FX_DIAG_TEMP_VALID=4U, FX_DIAG_IO_FAILED=8U };
typedef struct {
    uint8_t flags,device,reg,status,attempts,lines,operation;
    uint16_t marker,service,temperature_raw;
    uint32_t error;
} FxDiagnostics;
static inline uint16_t Fx_Get16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1]<<8)); }
static inline void Fx_Put16(uint8_t *p,uint16_t n) { p[0]=(uint8_t)n; p[1]=(uint8_t)(n>>8); }
static inline uint32_t Fx_Get32(const uint8_t *p) { return (uint32_t)Fx_Get16(p)|((uint32_t)Fx_Get16(p+2)<<16); }
static inline void Fx_Put32(uint8_t *p,uint32_t n) { Fx_Put16(p,(uint16_t)n); Fx_Put16(p+2,(uint16_t)(n>>16)); }
static inline void Fx_PutDiagnostics(uint8_t *p,const FxDiagnostics *d) {
    p[16]=d->flags; p[17]=d->device; p[18]=d->reg; p[19]=d->status;
    Fx_Put16(p+20,d->marker); Fx_Put16(p+22,d->service); Fx_Put16(p+24,d->temperature_raw);
    p[26]=d->attempts; p[27]=d->lines; Fx_Put32(p+28,d->error); p[40]=d->operation;
}
static inline uint16_t Fx_Crc(const uint8_t *p,size_t n) {
    uint16_t c=0xFFFFU;
    while(n--) { c^=*p++; for(unsigned b=0;b<8;b++) c=(uint16_t)((c>>1)^((c&1U)?0xA001U:0U)); }
    return c;
}
static inline void Fx_Frame(uint8_t *p,uint8_t type,uint16_t seq,uint8_t mode) {
    memset(p,0,FX_FRAME_SIZE); p[0]=0xE7; p[1]=0x7E; p[2]=FX_VERSION; p[3]=type;
    Fx_Put16(p+4,seq); p[6]=mode;
}
static inline void Fx_Seal(uint8_t *p) { Fx_Put16(p+FX_FRAME_SIZE-2,Fx_Crc(p,FX_FRAME_SIZE-2)); }
typedef struct { uint8_t bytes[FX_FRAME_SIZE]; size_t used; } FxParser;
/* Sliding window recovers from corruption, truncated frames and embedded headers. */
static inline bool Fx_Parse(FxParser *p,uint8_t byte,uint8_t *out) {
    p->bytes[p->used++]=byte;
    if(p->used<FX_FRAME_SIZE) return false;
    if(p->bytes[0]==0xE7 && p->bytes[1]==0x7E && p->bytes[2]==FX_VERSION &&
       Fx_Get16(p->bytes+FX_FRAME_SIZE-2)==Fx_Crc(p->bytes,FX_FRAME_SIZE-2)) {
        memcpy(out,p->bytes,FX_FRAME_SIZE); p->used=0; return true;
    }
    memmove(p->bytes,p->bytes+1,FX_FRAME_SIZE-1); p->used=FX_FRAME_SIZE-1; return false;
}
#endif
