#ifndef FIXTURE_WIRE_H
#define FIXTURE_WIRE_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#define FX_FRAME_SIZE 18U
#define FX_TEMP_INVALID ((int16_t)-32768)
enum { FX_START=1, FX_HEARTBEAT=2, FX_CANCEL=3, FX_STATUS=0x81 };
enum { FX_NONE=0, FX_INITIALIZE=1, FX_DEMO=2, FX_INSPECT=3 };
enum { FX_IDLE=0, FX_WAIT, FX_WRITE, FX_HEATING, FX_STABLE, FX_PASS, FX_FAIL };
enum { FX_OK=0, FX_NO_EYE, FX_REMOVED, FX_TEMP_COMM, FX_OVER_TEMP,
       FX_TIMEOUT, FX_WRITE_ERROR, FX_VERIFY_ERROR, FX_LINK_LOST,
       FX_CANCELLED, FX_BAD_COMMAND, FX_PWM_ERROR, FX_POWER_LOST,
       FX_USED_EYE, FX_DEMO_EYE, FX_EYE_READ_ERROR };
/* Byte 0..1 E7 7E, 2 version=1, 3 type, 4..5 transaction,
 * 6 mode, 7 state, 8 reason, 9 eye, 10..11 raw sensor centidegrees,
 * 12..13 raw target centidegrees, 14..15 stable ms, 16..17 CRC16/MODBUS.
 * All multibyte fields little endian. Commands zero reserved/status fields. */
static inline uint16_t Fx_Get16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1]<<8)); }
static inline void Fx_Put16(uint8_t *p,uint16_t n) { p[0]=(uint8_t)n; p[1]=(uint8_t)(n>>8); }
static inline uint16_t Fx_Crc(const uint8_t *p,size_t n) {
    uint16_t c=0xFFFFU;
    while(n--) { c^=*p++; for(unsigned b=0;b<8;b++) c=(uint16_t)((c>>1)^((c&1U)?0xA001U:0U)); }
    return c;
}
static inline void Fx_Frame(uint8_t *p,uint8_t type,uint16_t seq,uint8_t mode) {
    memset(p,0,FX_FRAME_SIZE); p[0]=0xE7; p[1]=0x7E; p[2]=1; p[3]=type;
    Fx_Put16(p+4,seq); p[6]=mode;
}
static inline void Fx_Seal(uint8_t *p) { Fx_Put16(p+16,Fx_Crc(p,16)); }
typedef struct { uint8_t bytes[FX_FRAME_SIZE]; size_t used; } FxParser;
/* Sliding window recovers from corruption, truncated frames and embedded headers. */
static inline bool Fx_Parse(FxParser *p,uint8_t byte,uint8_t *out) {
    p->bytes[p->used++]=byte;
    if(p->used<FX_FRAME_SIZE) return false;
    if(p->bytes[0]==0xE7 && p->bytes[1]==0x7E && p->bytes[2]==1 &&
       Fx_Get16(p->bytes+16)==Fx_Crc(p->bytes,16)) {
        memcpy(out,p->bytes,FX_FRAME_SIZE); p->used=0; return true;
    }
    memmove(p->bytes,p->bytes+1,FX_FRAME_SIZE-1); p->used=FX_FRAME_SIZE-1; return false;
}
#endif
