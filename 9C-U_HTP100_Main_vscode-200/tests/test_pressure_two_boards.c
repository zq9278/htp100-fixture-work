#include "pressure_monitor.h"
#include "pressure_client.h"
#include <math.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) return __LINE__; } while(0)
static uint32_t clock_ms;
static uint8_t ee[256],to_g0[2048],to_h7[2048],to_pc[4096],last_point[PM_MAX_FRAME];
static size_t ng0,nh7,npc,last_point_n;
static bool drop_h7;
static uint32_t now(void) { return clock_ms; }
static bool g0_send(const uint8_t *p,uint16_t n) { if(!drop_h7) { memcpy(to_h7+nh7,p,n);nh7+=n; }return true; }
static bool pc_send(const uint8_t *p,uint16_t n) { if(npc+n<sizeof(to_pc)) { memcpy(to_pc+npc,p,n);npc+=n; }return true; }
static bool h7_send(const uint8_t *p,uint16_t n) { memcpy(to_g0+ng0,p,n);ng0+=n;if(p[3]==PM_POINT) { memcpy(last_point,p,n);last_point_n=n; }return true; }
static bool read_ee(uint8_t a,uint8_t *p,size_t n) { memcpy(p,ee+a,n);return true; }
static bool write_ee(uint8_t a,const uint8_t *p,size_t n) { memcpy(ee+a,p,n);return true; }
static const PressureMonitorPort g0={now,g0_send,read_ee,write_ee,pc_send};
static const PressureClientPort h7={now,h7_send};
static void pump(void) {
    for(unsigned i=0;i<4;i++) {
        if(ng0) { size_t n=ng0;ng0=0;for(size_t k=0;k<n;k++) PressureMonitor_Feed(to_g0+k,1); }
        if(nh7) { size_t n=nh7;nh7=0;PressureClient_Feed(to_h7,n); }
    }
}
static void advance(unsigned ms,float grams) {
    for(unsigned i=0;i<ms/5;i++) {
        clock_ms+=5;PressureMonitor_Sample(true,(int32_t)lroundf(1000000-grams*400));
        PressureMonitor_Tick();PressureClient_Tick();pump();
    }
}
static void pc_request(uint8_t cmd,uint16_t seq,const uint8_t *p,uint8_t n) {
    uint8_t f[PM_MAX_FRAME]={0x7B,0xB7,1,0,0,0,0};f[3]=cmd;f[4]=(uint8_t)seq;f[5]=(uint8_t)(seq>>8);f[6]=n;
    if(n) memcpy(f+7,p,n);uint16_t crc=PressureMonitor_Crc(f,n+7);f[n+7]=(uint8_t)crc;f[n+8]=(uint8_t)(crc>>8);f[n+9]=10;
    for(unsigned i=0;i<n+10U;i++) PressureMonitor_FeedPc(f+i,1);
}
static bool pc_ack(uint8_t status) { return npc>=12 && to_pc[npc-9]==PM_ACK && to_pc[npc-4]==status; }
int main(void) {
    memset(ee,255,sizeof(ee));clock_ms=100;PressureMonitor_Init(&g0);PressureClient_Init(&h7);
    advance(100,0);const PressureClientState *s=PressureClient_Status();
    CHECK(s->connected && s->config_valid && s->config.flags==0);
    CHECK(PressureClient_Point(0));pump();
    npc=0;uint8_t duration[4]={200,0,0,0};pc_request(PM_START,12,duration,4);CHECK(pc_ack(PM_BUSY));
    advance(1100,0);CHECK(s->point_count==1 && !s->pending);
    for(unsigned i=1;i<=3;i++) {
        CHECK(PressureClient_Point((float)(i==1?100:i==2?300:500)));pump();
        advance(1100,(float)(i==1?100:i==2?300:500));CHECK(s->point_count==i+1 && !s->pending);
    }
    /* Same sequence on another serial port must start a separate point capture.
     * Asynchronous result must return only to PC; H7's old retry uses its cache. */
    npc=0;pc_request(PM_POINT,(uint16_t)(last_point[4]|(last_point[5]<<8)),NULL,0);
    advance(1100,375);CHECK(npc>0);
    nh7=0;PressureMonitor_Feed(last_point,last_point_n);
    CHECK(nh7>=30 && to_h7[3]==PM_POINT_RESULT);
    float old_mean;memcpy(&old_mean,to_h7+7,4);CHECK(old_mean==800000);pump();
    CHECK(PressureClient_Fit(84.8f) && s->fit_valid && s->fit.zero==1000000);
    CHECK(fabsf(s->fit.scale+0.0025f)<0.000001f);
    CHECK(PressureClient_Command(PM_SET_CONFIG,0));pump();CHECK(s->config.flags==3 && !s->pending);
    CHECK(PressureClient_Command(PM_SAVE,0));pump();CHECK(s->config.generation==1 && !s->config.dirty);
    advance(50,0); /* The next telemetry publishes the newly calibrated flags. */
    CHECK(PressureClient_Command(PM_START,200));pump();
    advance(50,100);advance(5,500);advance(195,100);
    CHECK(s->peak_state==PM_COMPLETE && s->peak_at==55 && s->errors==0);
    CHECK(fabsf(s->peak-433.7023f)<0.01f && fabsf(s->pressure-86.7405f)<0.01f);
    CHECK(fabsf(PressureClient_Force(s->peak,s->sample_area)-4.903325f)<0.00001f);
    CHECK(fabsf(PressureClient_Grams(s->peak,s->sample_area)-500.0f)<0.001f);
    CHECK(fabsf(PressureClient_Grams(s->pressure,s->sample_area)-100.0f)<0.001f);
    CHECK(fabsf(PressureClient_Force(s->pressure,s->sample_area)+PressureClient_Force(-s->pressure,s->sample_area))<0.0001f);
    CHECK(isnan(PressureClient_Force(s->pressure,NAN)));
    CHECK(!PressureClient_Command(PM_START,0));
    CHECK(PressureClient_Command(PM_START,10000));pump();advance(50,100);
    PressureClient_Init(&h7);advance(100,100);CHECK(s->connected && s->peak_state==PM_RUNNING);
    CHECK(!PressureClient_Point(100));
    CHECK(PressureClient_Command(PM_STOP,0));pump();advance(100,100);CHECK(s->peak_state==PM_ABORTED);
    drop_h7=true;advance(4200,100);CHECK(!s->connected && !s->config_valid && isnan(s->pressure));
    drop_h7=false;advance(1200,0);CHECK(s->connected && s->config.flags==3 && s->config.generation==1);
    return 0;
}
