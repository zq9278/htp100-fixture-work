#include "pressure_monitor.h"
#include <math.h>
#include <string.h>

#define CHECK(x) do { if(!(x)) return __LINE__; } while(0)
static uint32_t clock_ms, writes;
static int fail_write;
static uint8_t eeprom[256], latest[PM_MAX_FRAME];
static uint16_t next_sequence;
static uint8_t last_ack;
/* Python decodes this trace with the production PC parser after the ARM run. */
uint8_t pressure_test_trace[65536];
uint32_t pressure_test_trace_length;
static uint32_t now(void) { return clock_ms; }
static bool output(const uint8_t *p,uint16_t n) {
    memcpy(latest,p,n);
    if(p[3]==PM_ACK) last_ack=p[8];
    if(pressure_test_trace_length+n<=sizeof(pressure_test_trace)) {
        memcpy(pressure_test_trace+pressure_test_trace_length,p,n); pressure_test_trace_length+=n;
    }
    return true;
}
static bool read_store(uint8_t a,uint8_t *p,size_t n) { memcpy(p,eeprom+a,n); return true; }
static bool write_store(uint8_t a,const uint8_t *p,size_t n) {
    writes++;
    if(fail_write && --fail_write==0) { memcpy(eeprom+a,p,n/2); return false; }
    memcpy(eeprom+a,p,n); return true;
}
static const PressureMonitorPort port={now,output,read_store,write_store};
static void put16(uint8_t *p,uint16_t x) { p[0]=(uint8_t)x; p[1]=(uint8_t)(x>>8); }
static void put32(uint8_t *p,uint32_t x) { put16(p,(uint16_t)x); put16(p+2,(uint16_t)(x>>16)); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)p[0]|(uint16_t)((uint16_t)p[1]<<8); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)get16(p)|((uint32_t)get16(p+2)<<16); }
static float getf(const uint8_t *p) { float x; memcpy(&x,p,4); return x; }
static void putf(uint8_t *p,float x) { memcpy(p,&x,4); }
static uint8_t request(uint8_t command,const uint8_t *payload,uint8_t n,uint16_t sequence) {
    uint8_t f[PM_MAX_FRAME]; f[0]=0x7B; f[1]=0xB7; f[2]=1; f[3]=command;
    put16(f+4,sequence); f[6]=n; if(n) memcpy(f+7,payload,n);
    put16(f+7+n,PressureMonitor_Crc(f,7U+n)); f[9+n]=10;
    /* Every request is fragmented, with leading noise and another protocol. */
    const uint8_t noise[]={0x5A,0xA5,0x7B,0x00};
    PressureMonitor_Feed(noise,sizeof(noise));
    for(unsigned i=0;i<10U+n;i++) PressureMonitor_Feed(f+i,1);
    return latest[3]==PM_ACK?latest[8]:255;
}
static uint8_t command(uint8_t c,const uint8_t *p,uint8_t n) { return request(c,p,n,++next_sequence); }
static void configuration(float scale) {
    uint8_t p[13]; put32(p,100000); putf(p+4,scale); putf(p+8,84.8f); p[12]=3;
    (void)command(PM_SET_CONFIG,p,13);
}
static void report(void) { clock_ms+=50; PressureMonitor_Tick(); }
static uint8_t *status(void) { report(); return latest+7; }
static void collect(int32_t raw,int noise) {
    uint32_t begin=clock_ms;
    for(unsigned i=0;i<1000;i++) {
        clock_ms=begin+i;
        PressureMonitor_Sample(true,raw+(i%2?noise:-noise));
        PressureMonitor_Tick();
    }
    clock_ms=begin+1000; PressureMonitor_Tick();
}

int main(void) {
    memset(eeprom,255,sizeof(eeprom)); clock_ms=100; PressureMonitor_Init(&port);
    uint8_t *p=status(); CHECK(!(get16(p+36)&PM_ZEROED)); CHECK(isnan(getf(p+8)));
    uint8_t duration[4]; put32(duration,1000);
    CHECK(command(PM_START,duration,4)==PM_NOT_ZEROED);
    configuration(-0.01f); CHECK(latest[8]==PM_OK);
    PressureMonitor_AdcReady(true); PressureMonitor_Power(true,true,false,75,3900);
    PressureMonitor_Sample(true,99900); p=status();
    CHECK(fabsf(getf(p+8)-0.86736f)<0.001f);
    CHECK((get16(p+36)&(PM_VALID|PM_CALIBRATED|PM_CHARGING|PM_ADC_READY))==
          (PM_VALID|PM_CALIBRATED|PM_CHARGING|PM_ADC_READY));
    CHECK(p[39]==75 && get16(p+40)==3900);
    CHECK(fabsf(getf(p+52)-84.8f)<0.001f); /* Area travels with its pressure sample. */
    configuration(-0.02f); p=status();
    CHECK(fabsf(getf(p+8)-getf(p+12))<0.0001f); /* No old-scale filtered display after config. */
    configuration(-0.01f); PressureMonitor_Sample(true,99900);
    CHECK(command(PM_START,duration,4)==PM_OK);
    uint16_t start_seq=next_sequence;
    clock_ms+=100; PressureMonitor_Sample(true,95000); /* 50g maximum */
    CHECK(request(PM_START,duration,4,start_seq)==PM_OK); /* Must not reset timer/peak. */
    clock_ms+=100; PressureMonitor_Sample(true,99000);
    CHECK(command(PM_TARE,0,0)==PM_BUSY);
    configuration(-0.02f); CHECK(latest[8]==PM_BUSY);
    CHECK(command(PM_SAVE,0,0)==PM_BUSY);
    clock_ms+=800; PressureMonitor_Sample(true,50000); /* Exactly on deadline: excluded. */
    p=status(); CHECK(p[38]==PM_COMPLETE && get32(p+24)==1000 && get32(p+32)==2);
    CHECK(fabsf(getf(p+16)-43.368f)<0.01f && get32(p+48)==100);
    CHECK(command(PM_START,duration,4)==PM_OK);
    clock_ms+=100; PressureMonitor_Sample(false,0);
    clock_ms+=100; PressureMonitor_Sample(true,98000);
    CHECK(command(PM_STOP,0,0)==PM_OK);
    p=status(); CHECK(p[38]==PM_ABORTED && get32(p+42)==1 && get32(p+32)==1);
    clock_ms+=200; p=status(); CHECK(!(get16(p+36)&PM_VALID) && isnan(getf(p+8)));
    PressureMonitor_Sample(true,8388607); p=status();
    CHECK((get16(p+36)&PM_SATURATED) && !(get16(p+36)&PM_VALID));

    /* Stable point capture and tare use a one-second, non-blocking window. */
    PressureMonitor_Sample(true,100000);
    (void)command(PM_POINT,0,0); uint16_t point_seq=next_sequence;
    CHECK(command(PM_TARE,0,0)==PM_BUSY);
    collect(100000,2); CHECK(last_ack==PM_OK);
    CHECK(request(PM_POINT,0,0,point_seq)==PM_OK);
    (void)command(PM_TARE,0,0); collect(100500,2); CHECK(last_ack==PM_OK);
    (void)command(PM_GET_CONFIG,0,0); CHECK(latest[3]==PM_CONFIG && get32(latest+7)==100500);
    (void)command(PM_TARE,0,0); collect(100000,5000); CHECK(last_ack==PM_UNSTABLE);
    (void)command(PM_GET_CONFIG,0,0); CHECK(get32(latest+7)==100500); /* Failed tare preserves zero. */
    PressureMonitor_Sample(true,100000); (void)command(PM_POINT,0,0);
    PressureMonitor_Sample(false,0); clock_ms+=1100; PressureMonitor_Tick(); CHECK(last_ack==PM_SENSOR_ERROR);

    /* Save, idempotent retry, power-cut rollback, corrupt-new-slot fallback. */
    configuration(-0.01f); CHECK(command(PM_SAVE,0,0)==PM_OK);
    uint16_t saved_sequence=next_sequence; uint32_t saved_writes=writes;
    CHECK(request(PM_SAVE,0,0,saved_sequence)==PM_OK && writes==saved_writes);
    PressureMonitor_Init(&port); (void)command(PM_GET_CONFIG,0,0);
    CHECK(get32(latest+20)==1 && latest[19]==3 && latest[24]==0);
    CHECK(fabsf(getf(latest+11)+0.01f)<0.000001f);
    configuration(-0.02f); fail_write=2; CHECK(command(PM_SAVE,0,0)==PM_STORAGE_ERROR);
    PressureMonitor_Init(&port); (void)command(PM_GET_CONFIG,0,0);
    CHECK(fabsf(getf(latest+11)+0.01f)<0.000001f);
    configuration(-0.02f); CHECK(command(PM_SAVE,0,0)==PM_OK);
    PressureMonitor_Init(&port); (void)command(PM_GET_CONFIG,0,0);
    CHECK(fabsf(getf(latest+11)+0.02f)<0.000001f && get32(latest+20)==2);
    eeprom[PM_STORE_B+12]^=1;
    PressureMonitor_Init(&port); (void)command(PM_GET_CONFIG,0,0);
    CHECK(fabsf(getf(latest+11)+0.01f)<0.000001f);

    /* Time and generation wrap, invalid config, parser recovery. */
    put32(eeprom+PM_STORE_A+4,0xFFFFFFFFU);
    put16(eeprom+PM_STORE_A+30,PressureMonitor_Crc(eeprom+PM_STORE_A,30));
    PressureMonitor_Init(&port); configuration(-0.03f); CHECK(command(PM_SAVE,0,0)==PM_OK);
    PressureMonitor_Init(&port); (void)command(PM_GET_CONFIG,0,0);
    CHECK(get32(latest+20)==0 && fabsf(getf(latest+11)+0.03f)<0.000001f);
    clock_ms=0xFFFFFF00U; PressureMonitor_Sample(true,100000);
    put32(duration,500); CHECK(command(PM_START,duration,4)==PM_OK);
    clock_ms+=100; PressureMonitor_Sample(true,99000);
    clock_ms+=500; p=status(); CHECK(p[38]==PM_COMPLETE && get32(p+24)==500);
    uint8_t bad[13]={0}; putf(bad+4,NAN); putf(bad+8,84.8f); bad[12]=3;
    CHECK(command(PM_SET_CONFIG,bad,sizeof(bad))==PM_RANGE);
    const uint8_t garbage[]={0x7B,0xB7,1,8,1,0,0,0,0,10,0x7B,0xB7,1,8,0,0,255};
    PressureMonitor_Feed(garbage,sizeof(garbage));
    CHECK(command(PM_STOP,0,0)==PM_OK);
    eeprom[PM_STORE_A]^=1; eeprom[PM_STORE_B]^=1;
    PressureMonitor_Init(&port); (void)command(PM_GET_CONFIG,0,0); CHECK(latest[19]==0);
    return 0;
}
