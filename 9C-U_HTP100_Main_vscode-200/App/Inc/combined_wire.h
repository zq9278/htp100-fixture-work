#ifndef COMBINED_WIRE_H
#define COMBINED_WIRE_H
#include "fixture_wire.h"
#include "pressure_monitor.h"
/* Frame-level demultiplexing: never interpret a second protocol embedded in
 * a valid frame's payload. Corruption uses sliding resynchronization. */
typedef struct { uint8_t bytes[PM_MAX_FRAME];size_t used; } CombinedParser;
typedef void (*CombinedReceiver)(bool pressure,const uint8_t *frame,size_t length);
static inline void Combined_Feed(CombinedParser *s,const uint8_t *data,size_t n,CombinedReceiver receive) {
    while(n--) {
        s->bytes[s->used++]=*data++;
        while(s->used>=2) {
            uint8_t *p=s->bytes;bool pressure=p[0]==0x7B && p[1]==0xB7;
            bool fixture=p[0]==0xE7 && p[1]==0x7E;
            size_t length=FX_FRAME_SIZE;
            if(!pressure && !fixture) goto discard;
            if(s->used<3) break;
            if(p[2]!=(pressure?1:FX_VERSION)) goto discard;
            if(pressure) {
                if(s->used<7) break;
                if(p[6]>PM_MAX_PAYLOAD) goto discard;
                length=10U+p[6];
            }
            if(s->used<length) break;
            size_t crc_at=length-(pressure?3U:2U);
            if(Fx_Get16(p+crc_at)!=Fx_Crc(p,crc_at) || (pressure && p[length-1]!=10)) goto discard;
            receive(pressure,p,length);s->used-=length;memmove(p,p+length,s->used);continue;
        discard:
            memmove(p,p+1,--s->used);
        }
    }
}
#endif
