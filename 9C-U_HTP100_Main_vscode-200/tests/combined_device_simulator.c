/* The real production measurement core exposed to Python's ARM emulator. */
#include "pressure_monitor.h"
#include <string.h>
#include "fixture_controller.h"
#include "combined_runtime.h"
uint8_t sim_rx[1024], sim_tx[65536];
uint32_t sim_tx_length;
static uint8_t store[256];
static uint32_t milliseconds;
static bool pc_port;
static uint32_t now(void) { return milliseconds; }
static bool output(const uint8_t *p,uint16_t n) {
    if(sim_tx_length+n>sizeof(sim_tx)) return false;
    memcpy(sim_tx+sim_tx_length,p,n); sim_tx_length+=n; return true;
}
static bool send(const uint8_t *p,uint16_t n) { return pc_port?true:output(p,n); }
static bool send_pc(const uint8_t *p,uint16_t n) { return pc_port?output(p,n):true; }
static bool read_store(uint8_t a,uint8_t *p,size_t n) { memcpy(p,store+a,n); return true; }
static bool write_store(uint8_t a,const uint8_t *p,size_t n) { memcpy(store+a,p,n); return true; }
static CombinedParser demux;
uint32_t sim_eye_present=1,sim_heater_on,sim_write_count,sim_clear_count;
static void off(void) { sim_heater_on=0; }
static bool probe(void) { return sim_eye_present!=0; }
static bool heater(float target,float *value) { *value=target;sim_heater_on=1;return true; }
static uint8_t prepare(bool demo) { (void)demo;sim_write_count++;return FX_OK; }
static uint8_t usable(void) { return FX_OK; }
static uint8_t clear_marker(void) { sim_clear_count++;return FX_OK; }
static void led(uint8_t state) { (void)state; }
static void diagnostics(FxDiagnostics *out) { if(out) memset(out,0,sizeof(*out)); }
static const FixturePort fixture={now,off,probe,heater,prepare,send,usable,led,diagnostics,clear_marker};
static const PressureMonitorPort port={now,send,read_store,write_store,send_pc,Fixture_PressureSelected};
static void route(bool pressure,const uint8_t *p,size_t n) {
    Combined_Dispatch(pressure,p,n);
}
void Sim_Reset(void) {
    milliseconds=0; sim_tx_length=0;demux.used=0;sim_heater_on=sim_write_count=sim_clear_count=0;Fixture_Init(&fixture);PressureMonitor_Init(&port);
    PressureMonitor_AdcReady(true); PressureMonitor_Power(true,true,false,80,3900);
}
void Sim_Init(void) { memset(store,255,sizeof(store)); Sim_Reset(); }
void Sim_Feed(uint32_t n) { Combined_Feed(&demux,sim_rx,n,route); }
void Sim_FeedPc(uint32_t n) { PressureMonitor_FeedPc(sim_rx,n); }
void Sim_PcPort(uint32_t enabled) { pc_port=enabled!=0; }
void Sim_Step(uint32_t ms,int32_t raw) { milliseconds=ms; PressureMonitor_Sample(true,raw); PressureMonitor_Tick();Fixture_Tick(); }
int main(void) { return 0; }
