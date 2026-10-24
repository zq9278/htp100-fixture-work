/* The real production measurement core exposed to Python's ARM emulator. */
#include "pressure_monitor.h"
#include <string.h>
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
static const PressureMonitorPort port={now,send,read_store,write_store,send_pc};
void Sim_Reset(void) {
    milliseconds=0; sim_tx_length=0; PressureMonitor_Init(&port);
    PressureMonitor_AdcReady(true); PressureMonitor_Power(true,true,false,80,3900);
}
void Sim_Init(void) { memset(store,255,sizeof(store)); Sim_Reset(); }
void Sim_Feed(uint32_t n) { PressureMonitor_Feed(sim_rx,n); }
void Sim_FeedPc(uint32_t n) { PressureMonitor_FeedPc(sim_rx,n); }
void Sim_PcPort(uint32_t enabled) { pc_port=enabled!=0; }
void Sim_Step(uint32_t ms,int32_t raw) { milliseconds=ms; PressureMonitor_Sample(true,raw); PressureMonitor_Tick(); }
int main(void) { return 0; }
