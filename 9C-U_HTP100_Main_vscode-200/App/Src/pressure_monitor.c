#include "pressure_monitor.h"

#include <math.h>
#include <string.h>

#define PM_PASCAL_PER_MMHG 133.3224f
#define PM_GRAVITY 9.80665f
#define PM_FRESH_MS 100U
#define PM_REPORT_MS 50U
#define PM_COLLECT_MS 1000U

typedef struct {
    int32_t zero;
    float grams_per_count;
    float area;
    uint8_t flags; /* bit 0: calibrated; bit 1: zero established */
} Calibration;

static const PressureMonitorPort *io;
static Calibration cal;
static uint8_t rx_stream[2][PM_MAX_FRAME], rx_used[2], reply_channel, collect_channel;
static uint8_t active_slot, peak_state, collect_command;
static uint16_t tx_sequence, collect_sequence;
static uint32_t generation, last_sample, last_report;
static bool dirty, sample_valid, adc_ready, filter_valid, saturated;
static int32_t latest_raw;
static float pressure, filtered, peak, minimum;
static uint32_t peak_started, peak_duration, peak_elapsed, peak_count, peak_errors, peak_at;
static uint32_t collect_started, collect_count;
static int32_t collect_min, collect_max;
static double collect_mean, collect_m2;
static bool collect_error;
static uint16_t power_flags, battery_mv;
static uint8_t battery_soc;
/* A repeated transaction is acknowledged, never repeated (notably SAVE/START). */
static uint8_t previous_request[2][PM_MAX_FRAME], previous_length[2], previous_status[2];
static bool previous_complete[2];
static uint8_t previous_point[2][20];

static uint16_t u16(const uint8_t *p) { return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1]<<8); }
static uint32_t u32(const uint8_t *p) { return (uint32_t)u16(p) | ((uint32_t)u16(p+2)<<16); }
static void put16(uint8_t *p, uint16_t x) { p[0]=(uint8_t)x; p[1]=(uint8_t)(x>>8); }
static void put32(uint8_t *p, uint32_t x) { put16(p,(uint16_t)x); put16(p+2,(uint16_t)(x>>16)); }
static float getf(const uint8_t *p) { float x; memcpy(&x,p,4); return x; }
static void putf(uint8_t *p, float x) { memcpy(p,&x,4); }

uint16_t PressureMonitor_Crc(const uint8_t *p, size_t n)
{
    uint16_t crc=0xFFFFU;
    while(n--) {
        crc^=*p++;
        for(unsigned b=0;b<8;b++) crc=(crc&1U)?(uint16_t)((crc>>1)^0xA001U):(uint16_t)(crc>>1);
    }
    return crc;
}

static void send(uint8_t command, uint16_t sequence, const uint8_t *p, uint8_t n)
{
    uint8_t f[PM_MAX_FRAME];
    f[0]=0x7B; f[1]=0xB7; f[2]=1; f[3]=command; put16(f+4,sequence); f[6]=n;
    if(n) memcpy(f+7,p,n);
    put16(f+7+n,PressureMonitor_Crc(f,7U+n)); f[9+n]=0x0A;
    if(command==PM_STATUS || reply_channel==0) (void)io->send(f,(uint16_t)(10U+n));
    if(io->send_pc && (command==PM_STATUS || reply_channel==1)) (void)io->send_pc(f,(uint16_t)(10U+n));
}

static void ack(uint8_t command, uint16_t sequence, uint8_t status)
{
    uint8_t p[2]={command,status};
    send(PM_ACK,sequence,p,2);
}

static bool calibration_valid(const Calibration *c)
{
    return c->zero>=-8388608L && c->zero<=8388607L &&
        isfinite(c->grams_per_count) && fabsf(c->grams_per_count)>=1.0e-7f &&
        fabsf(c->grams_per_count)<=100.0f && isfinite(c->area) &&
        c->area>=1.0f && c->area<=100000.0f && c->flags==3;
}

static bool decode_record(const uint8_t *p, Calibration *c, uint32_t *gen)
{
    if(memcmp(p,"PM01",4) || u16(p+30)!=PressureMonitor_Crc(p,30)) return false;
    *gen=u32(p+4); c->zero=(int32_t)u32(p+8); c->grams_per_count=getf(p+12);
    c->area=getf(p+16); c->flags=p[20];
    return calibration_valid(c);
}

static void config(uint16_t sequence)
{
    uint8_t p[18];
    put32(p,(uint32_t)cal.zero); putf(p+4,cal.grams_per_count); putf(p+8,cal.area);
    p[12]=cal.flags; put32(p+13,generation); p[17]=(uint8_t)dirty;
    send(PM_CONFIG,sequence,p,sizeof(p));
}

static bool save(void)
{
    uint8_t p[PM_STORE_SIZE]={0}, verify[PM_STORE_SIZE];
    uint8_t address=active_slot==PM_STORE_A?PM_STORE_B:PM_STORE_A;
    /* Invalidate only the inactive slot. Commit its magic last; a failed write
     * or loss of power leaves the previously verified calibration intact. */
    if(!io->write_store(address,p,4)) return false;
    memcpy(p,"PM01",4); put32(p+4,generation+1U); put32(p+8,(uint32_t)cal.zero);
    putf(p+12,cal.grams_per_count); putf(p+16,cal.area); p[20]=cal.flags;
    put16(p+30,PressureMonitor_Crc(p,30));
    if(!io->write_store((uint8_t)(address+4),p+4,PM_STORE_SIZE-4) ||
       !io->write_store(address,p,4) || !io->read_store(address,verify,sizeof(verify)) ||
       memcmp(p,verify,sizeof(p))) return false;
    generation++; active_slot=address; dirty=false;
    return true;
}

static float convert(int32_t raw)
{
    /* Keep signed readings: clamping to zero conceals offset and hysteresis. */
    return ((float)raw-(float)cal.zero)*cal.grams_per_count*PM_GRAVITY*1000.0f/
           (cal.area*PM_PASCAL_PER_MMHG);
}

static void reset_filter(void)
{
    filter_valid=false;
    /* A displayed result must never use a mixture of calibration scales. */
    peak_state=PM_IDLE; peak_count=peak_errors=peak_elapsed=peak_at=0;
    peak=minimum=NAN;
    pressure=sample_valid?convert(latest_raw):NAN;
    filtered=pressure;
}

void PressureMonitor_Init(const PressureMonitorPort *port)
{
    io=port;
    cal=(Calibration){0,-1000.0f/(8388607.0f*128.0f*0.000370f),84.8f,0};
    memset(rx_used,0,sizeof(rx_used)); reply_channel=collect_channel=0;
    active_slot=0; generation=0; dirty=false;
    tx_sequence=0; collect_command=0;
    memset(previous_length,0,sizeof(previous_length)); memset(previous_complete,0,sizeof(previous_complete));
    sample_valid=adc_ready=filter_valid=saturated=false;
    last_sample=last_report=io->now();
    peak_started=peak_duration=0; latest_raw=0; power_flags=0; battery_soc=255; battery_mv=0;
    uint8_t p[PM_STORE_SIZE]; Calibration candidate; uint32_t gen;
    if(io->read_store(PM_STORE_A,p,sizeof(p)) && decode_record(p,&candidate,&gen)) {
        cal=candidate; generation=gen; active_slot=PM_STORE_A;
    }
    if(io->read_store(PM_STORE_B,p,sizeof(p)) && decode_record(p,&candidate,&gen) &&
       (!active_slot || (int32_t)(gen-generation)>0)) {
        cal=candidate; generation=gen; active_slot=PM_STORE_B;
    }
    reset_filter();
}

static void point_result(uint16_t sequence)
{
    uint8_t p[20];
    putf(p,(float)collect_mean);
    putf(p+4,collect_count>1?(float)sqrt(collect_m2/(double)(collect_count-1U)):0.0f);
    put32(p+8,(uint32_t)collect_min); put32(p+12,(uint32_t)collect_max);
    put32(p+16,collect_count);memcpy(previous_point[reply_channel],p,sizeof(p));
    send(PM_POINT_RESULT,sequence,p,sizeof(p));
}

static void request(const uint8_t *f, uint8_t length)
{
    uint8_t command=f[3], n=f[6], status=PM_OK;
    uint16_t sequence=u16(f+4); const uint8_t *p=f+7;
    uint8_t channel=reply_channel;
    if(command==PM_HELLO && n==0) {
        if(!collect_command) previous_length[channel]=0; /* A new host session. */
        const uint8_t info[]={1,0,0x4A,0x01}; /* firmware 1.0, nominal 330 SPS */
        send(PM_INFO,sequence,info,sizeof(info)); config(sequence); return;
    }
    if(command==PM_GET_CONFIG && n==0) { config(sequence); return; }
    if(previous_length[channel]==length && !memcmp(f,previous_request[channel],length)) {
        if(previous_complete[channel]) {
            if(command==PM_POINT && previous_status[channel]==PM_OK)
                send(PM_POINT_RESULT,sequence,previous_point[channel],20);
            ack(command,sequence,previous_status[channel]);
        }
        return;
    }
    if(collect_command) { ack(command,sequence,PM_BUSY); return; }
    memcpy(previous_request[channel],f,length); previous_length[channel]=length; previous_complete[channel]=false;
    if((command==PM_TARE || command==PM_POINT) && n==0) {
        if(peak_state==PM_RUNNING) status=PM_BUSY;
        else if(!sample_valid || io->now()-last_sample>PM_FRESH_MS) status=PM_SENSOR_ERROR;
        else {
            collect_command=command; collect_sequence=sequence; collect_channel=channel; collect_started=io->now();
            collect_count=0; collect_mean=collect_m2=0; collect_error=false;
            collect_min=8388607; collect_max=-8388608;
            return; /* Exactly one final acknowledgement after the sample window. */
        }
    } else if(command==PM_SET_CONFIG && n==13) {
        Calibration candidate={(int32_t)u32(p),getf(p+4),getf(p+8),p[12]};
        if(peak_state==PM_RUNNING) status=PM_BUSY;
        else if(!calibration_valid(&candidate)) status=PM_RANGE;
        else { cal=candidate; dirty=true; reset_filter(); config(sequence); }
    } else if(command==PM_SAVE && n==0) {
        if(peak_state==PM_RUNNING) status=PM_BUSY;
        else if(!calibration_valid(&cal)) status=PM_NOT_ZEROED;
        else if(!save()) status=PM_STORAGE_ERROR;
        else config(sequence);
    } else if(command==PM_START && n==4) {
        uint32_t duration=u32(p);
        if(peak_state==PM_RUNNING) status=PM_BUSY;
        else if(!(cal.flags&2)) status=PM_NOT_ZEROED;
        else if(!sample_valid || io->now()-last_sample>PM_FRESH_MS) status=PM_SENSOR_ERROR;
        else if(duration<100U || duration>3600000U) status=PM_RANGE;
        else {
            peak_duration=duration; peak_started=io->now(); peak_elapsed=peak_count=peak_errors=peak_at=0;
            peak=minimum=NAN; peak_state=PM_RUNNING;
        }
    } else if(command==PM_STOP && n==0) {
        if(peak_state==PM_RUNNING) {
            peak_elapsed=io->now()-peak_started;
            if(peak_elapsed>peak_duration) peak_elapsed=peak_duration;
            peak_state=PM_ABORTED;
        }
    } else status=PM_BAD_REQUEST;
    previous_status[channel]=status; previous_complete[channel]=true; ack(command,sequence,status);
}

static void feed_channel(const uint8_t *p, size_t length, uint8_t channel)
{
    uint8_t *rx=rx_stream[channel], used=rx_used[channel];
    reply_channel=channel;
    while(length--) {
        if(used==PM_MAX_FRAME) { memmove(rx,rx+1,--used); }
        rx[used++]=*p++;
        while(used>=2) {
            if(rx[0]!=0x7B || rx[1]!=0xB7) { memmove(rx,rx+1,--used); continue; }
            if(used<7) break;
            if(rx[2]!=1 || rx[6]>PM_MAX_PAYLOAD) { memmove(rx,rx+1,--used); continue; }
            uint8_t total=(uint8_t)(10U+rx[6]);
            if(used<total) break;
            if(rx[total-1]!=0x0A || u16(rx+total-3)!=PressureMonitor_Crc(rx,total-3)) {
                memmove(rx,rx+1,--used); continue;
            }
            request(rx,total); used=(uint8_t)(used-total); memmove(rx,rx+total,used);
        }
    }
    rx_used[channel]=used;
}

void PressureMonitor_Feed(const uint8_t *p,size_t n) { feed_channel(p,n,0); }
void PressureMonitor_FeedPc(const uint8_t *p,size_t n) { feed_channel(p,n,1); }

void PressureMonitor_Sample(bool valid, int32_t raw)
{
    uint32_t now=io->now();
    saturated=valid && (raw<=-8388600L || raw>=8388600L);
    valid=valid && !saturated && raw>=-8388608L && raw<=8388607L;
    sample_valid=valid;
    if(peak_state==PM_RUNNING && now-peak_started>=peak_duration) {
        peak_state=PM_COMPLETE; peak_elapsed=peak_duration;
    }
    if(!valid) {
        filter_valid=false;
        if(peak_state==PM_RUNNING) peak_errors++;
        if(collect_command) collect_error=true;
        return;
    }
    uint32_t dt=now-last_sample;
    latest_raw=raw; last_sample=now; pressure=convert(raw);
    /* 40ms time constant for readability. Peak always uses unfiltered samples. */
    float alpha=(float)dt/(40.0f+(float)dt);
    filtered=filter_valid?filtered+alpha*(pressure-filtered):pressure;
    filter_valid=true;
    if(peak_state==PM_RUNNING) {
        if(!peak_count || pressure>peak) { peak=pressure; peak_at=now-peak_started; }
        if(!peak_count || pressure<minimum) minimum=pressure;
        peak_count++;
    }
    if(collect_command && now-collect_started<PM_COLLECT_MS) {
        collect_count++;
        double delta=(double)raw-collect_mean;
        collect_mean+=delta/(double)collect_count;
        collect_m2+=delta*((double)raw-collect_mean);
        if(raw<collect_min) collect_min=raw;
        if(raw>collect_max) collect_max=raw;
    }
}

void PressureMonitor_Power(bool valid, bool charging, bool full, uint8_t soc, uint16_t mv)
{
    power_flags=valid?(uint16_t)(PM_POWER_VALID|(charging?PM_CHARGING:0)|(full?PM_FULL:0)):0;
    battery_soc=valid?soc:255; battery_mv=valid?mv:0;
}

void PressureMonitor_AdcReady(bool ready) { adc_ready=ready; }

void PressureMonitor_Tick(void)
{
    uint32_t now=io->now();
    if(peak_state==PM_RUNNING) {
        peak_elapsed=now-peak_started;
        if(peak_elapsed>=peak_duration) { peak_elapsed=peak_duration; peak_state=PM_COMPLETE; }
    }
    if(collect_command && now-collect_started>=PM_COLLECT_MS) {
        reply_channel=collect_channel;
        uint8_t status=PM_OK;
        if(collect_error || collect_count<30 || !sample_valid || now-last_sample>PM_FRESH_MS) status=PM_SENSOR_ERROR;
        /* Noise limit is in equivalent mmHg under the active/provisional scale.
         * Both signs are checked; no one-sided zero-instability bug. */
        else if(fabsf((float)(collect_max-collect_min)*cal.grams_per_count*PM_GRAVITY*1000.0f/
                     (cal.area*PM_PASCAL_PER_MMHG))>10.0f) status=PM_UNSTABLE;
        else if(collect_command==PM_TARE) {
            cal.zero=(int32_t)lround(collect_mean); cal.flags|=2; dirty=true; reset_filter(); config(collect_sequence);
        } else point_result(collect_sequence);
        previous_status[collect_channel]=status; previous_complete[collect_channel]=true;
        ack(collect_command,collect_sequence,status); collect_command=0;
    }
    if(now-last_report>=PM_REPORT_MS) {
        uint8_t p[56]; uint32_t age=now-last_sample;
        bool fresh=sample_valid && age<=PM_FRESH_MS;
        uint16_t flags=(uint16_t)(power_flags|(fresh?PM_VALID:0)|(dirty?PM_DIRTY:0)|
            ((cal.flags&1)?PM_CALIBRATED:0)|((cal.flags&2)?PM_ZEROED:0)|
            (saturated?PM_SATURATED:0)|(adc_ready?PM_ADC_READY:0));
        put32(p,now); put32(p+4,(uint32_t)latest_raw);
        putf(p+8,fresh&&(cal.flags&2)?pressure:NAN);
        putf(p+12,fresh&&(cal.flags&2)?filtered:NAN);
        putf(p+16,peak); putf(p+20,minimum); put32(p+24,peak_elapsed);
        put32(p+28,peak_duration); put32(p+32,peak_count); put16(p+36,flags);
        p[38]=peak_state; p[39]=battery_soc; put16(p+40,battery_mv);
        put32(p+42,peak_errors); put16(p+46,(uint16_t)(age>65535U?65535U:age));
        put32(p+48,peak_at);putf(p+52,cal.area);
        send(PM_STATUS,++tx_sequence,p,sizeof(p)); last_report=now;
    }
}
