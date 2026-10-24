#include "pressure_client.h"
#include <math.h>
#include <string.h>
static const PressureClientPort *io;
static PressureClientState s;
static uint8_t rx[PM_MAX_FRAME],used;
static uint16_t sequence,pending_sequence;
static uint32_t last_rx,last_hello,last_config,command_started;
static bool identified,have_clock,got_point,got_config;
static float requested_grams;
static PcPoint result;
static uint16_t u16(const uint8_t *p) { return (uint16_t)p[0]|(uint16_t)((uint16_t)p[1]<<8); }
static uint32_t u32(const uint8_t *p) { return (uint32_t)u16(p)|((uint32_t)u16(p+2)<<16); }
static void put16(uint8_t *p,uint16_t x) { p[0]=(uint8_t)x;p[1]=(uint8_t)(x>>8); }
static void put32(uint8_t *p,uint32_t x) { put16(p,(uint16_t)x);put16(p+2,(uint16_t)(x>>16)); }
static float getf(const uint8_t *p) { float x;memcpy(&x,p,4);return x; }
static void putf(uint8_t *p,float x) { memcpy(p,&x,4); }
static uint16_t crc(const uint8_t *p,size_t n) {
    uint16_t c=65535;
    while(n--) { c^=*p++;for(unsigned i=0;i<8;i++) c=(c&1)?(uint16_t)((c>>1)^0xA001):(uint16_t)(c>>1); }
    return c;
}
static bool transmit(uint8_t cmd,const uint8_t *p,uint8_t n) {
    uint8_t f[PM_MAX_FRAME];f[0]=0x7B;f[1]=0xB7;f[2]=1;f[3]=cmd;put16(f+4,++sequence);f[6]=n;
    if(n) memcpy(f+7,p,n);
    put16(f+7+n,crc(f,7U+n));f[9+n]=10;
    if(!io->send(f,(uint16_t)(n+10))) { s.message="发送失败，请检查连接";return false; }
    pending_sequence=sequence;command_started=io->now();return true;
}
static void disconnect(const char *message) {
    s.connected=false;s.config_valid=false;identified=false;s.pending=0;
    s.filtered=s.pressure=NAN;s.flags=0;s.point_count=0;s.fit_valid=false;
    s.message=message;have_clock=false;
}
void PressureClient_Init(const PressureClientPort *port) {
    io=port;memset(&s,0,sizeof(s));used=0;sequence=(uint16_t)io->now();
    s.pressure=s.filtered=s.peak=s.minimum=NAN;s.soc=255;s.message="正在连接G0下位机";
    identified=have_clock=false;last_rx=last_config=io->now();last_hello=io->now()-1000U;
}
const PressureClientState *PressureClient_Status(void) { return &s; }
float PressureClient_Mmhg(float grams,float area) { return grams*9.80665f*1000.0f/(area*133.3224f); }
static bool calibration(const uint8_t *p,PcCalibration *c) {
    c->zero=(int32_t)u32(p);c->scale=getf(p+4);c->area=getf(p+8);c->flags=p[12];c->generation=u32(p+13);c->dirty=p[17]!=0;
    return c->zero>=-8388608L && c->zero<=8388607L && isfinite(c->scale) &&
        fabsf(c->scale)>=1e-7f && fabsf(c->scale)<=100 && isfinite(c->area) &&
        c->area>=1 && c->area<=100000 && (c->flags==0 || c->flags==2 || c->flags==3);
}
static void received(const uint8_t *f) {
    uint8_t cmd=f[3],n=f[6];uint16_t seq=u16(f+4);const uint8_t *p=f+7;
    if(cmd==PM_INFO && n==4 && p[0]==1 && p[1]==0) { identified=true;return; }
    if(cmd==PM_CONFIG && n==18) {
        PcCalibration config;
        if(!calibration(p,&config)) return;
        if(s.pending==PM_SET_CONFIG && seq==pending_sequence &&
           (config.zero!=s.fit.zero || fabsf(config.scale-s.fit.scale)>fabsf(s.fit.scale)*1e-5f ||
            fabsf(config.area-s.fit.area)>0.01f || config.flags!=3)) {
            s.pending=0;s.message="参数回读不一致，请重新读取";return;
        }
        s.config=config;s.config_valid=true;last_config=io->now();
        if(seq==pending_sequence) {
            got_config=true;
            if(s.pending==PM_GET_CONFIG) { s.pending=0;s.message="设备参数已读取"; }
        }
        return;
    }
    if(cmd==PM_STATUS && n==52) {
        uint32_t clock=u32(p);
        if(have_clock && (int32_t)(clock-s.device_ms)<0) { disconnect("G0已重启，正在重新连接");return; }
        have_clock=true;s.device_ms=clock;
        bool was_connected=s.connected;uint8_t old_state=s.peak_state;
        last_rx=io->now();s.connected=identified;
        if(s.connected && !was_connected) s.message="下位机已连接，先去皮或校准";
        s.raw=(int32_t)u32(p+4);s.flags=u16(p+36);
        s.pressure=getf(p+8);s.filtered=getf(p+12);s.peak=getf(p+16);s.minimum=getf(p+20);
        s.elapsed=u32(p+24);s.duration=u32(p+28);s.samples=u32(p+32);
        s.peak_state=p[38];s.soc=p[39];s.mv=u16(p+40);s.errors=u32(p+42);s.peak_at=u32(p+48);s.updates++;
        if(old_state==PM_RUNNING && s.peak_state==PM_COMPLETE && !s.pending)
            s.message=s.errors?"捕捉完成，存在异常采样":"捕捉完成，结果已锁存";
        if(!(s.flags&PM_VALID)) s.pressure=s.filtered=NAN;
        return;
    }
    if(!s.pending || seq!=pending_sequence) return;
    if(cmd==PM_POINT_RESULT && s.pending==PM_POINT && n==20) {
        result=(PcPoint){requested_grams,getf(p),getf(p+4),u32(p+16)};
        got_point=isfinite(result.raw) && isfinite(result.sd) && result.sd>=0 &&
            result.count>=30 && result.raw>=-8388608.0f && result.raw<=8388607.0f;
    } else if(cmd==PM_ACK && n==2 && p[0]==s.pending) {
        uint8_t pending=s.pending;s.pending=0;
        static const char *errors[]={"操作完成","命令格式错误","设备忙，请先结束采集","传感器异常或采样缺失",
            "受力不稳定，请静置重试","保存失败，旧记录仍保留","请先空载去皮或校准","参数超出范围"};
        if(p[1]) { s.message=p[1]<8?errors[p[1]]:"设备返回未知错误";return; }
        if(pending==PM_POINT) {
            if(!got_point || s.point_count>=PC_POINT_MAX) { s.message="缺少采样结果，请重试";return; }
            s.points[s.point_count++]=result;s.fit_valid=false;s.message="已采集，请更换下一组砝码";
        } else if(pending==PM_SET_CONFIG || pending==PM_SAVE || pending==PM_TARE) {
            if(!got_config) { s.message="缺少回读，结果未知，请读取";return; }
            s.message=pending==PM_SAVE?"保存成功，断电保留":pending==PM_TARE?"去皮完成，复核后可保存":"已应用，请用独立砝码复核";
        } else s.message=pending==PM_START?"峰值捕捉中，以G0计时为准":"已结束，保留本次结果";
    }
}
void PressureClient_Feed(const uint8_t *p,size_t n) {
    while(n--) {
        if(used==sizeof(rx)) memmove(rx,rx+1,--used);
        rx[used++]=*p++;
        while(used>=2) {
            if(rx[0]!=0x7B || rx[1]!=0xB7) { memmove(rx,rx+1,--used);continue; }
            if(used<7) break;
            if(rx[2]!=1 || rx[6]>PM_MAX_PAYLOAD) { memmove(rx,rx+1,--used);continue; }
            uint8_t total=(uint8_t)(10U+rx[6]);if(used<total) break;
            if(rx[total-1]!=10 || u16(rx+total-3)!=crc(rx,total-3)) { memmove(rx,rx+1,--used);continue; }
            received(rx);used=(uint8_t)(used-total);memmove(rx,rx+total,used);
        }
    }
}
void PressureClient_Tick(void) {
    uint32_t now=io->now();
    if(s.connected && now-last_rx>1000U) {
        s.filtered=s.pressure=NAN;s.flags&=(uint16_t)~(PM_VALID|PM_POWER_VALID|PM_CHARGING|PM_FULL);
        s.soc=255;s.mv=0;
    }
    if(identified && now-last_rx>=4000U) disconnect("通讯超时，请检查两板连接");
    if(s.pending && now-command_started>=4000U) { s.pending=0;s.message="命令超时，结果未知，请读取"; }
    if(!identified && now-last_hello>=1000U) { last_hello=now;(void)transmit(PM_HELLO,NULL,0); }
    else if(s.connected && !s.pending && now-last_config>=2000U) {
        if(transmit(PM_GET_CONFIG,NULL,0)) { s.pending=PM_GET_CONFIG;got_config=false; }
    }
}
static bool ready(uint8_t cmd) {
    if(!s.connected || !s.config_valid) { s.message="下位机未连接或参数未读取";return false; }
    if(s.pending) { s.message="请等待当前操作完成";return false; }
    if(s.peak_state==PM_RUNNING && cmd!=PM_STOP && cmd!=PM_GET_CONFIG) { s.message="请先结束峰值捕捉";return false; }
    if((cmd==PM_START || cmd==PM_TARE || cmd==PM_POINT) && !(s.flags&PM_VALID)) { s.message="传感器当前无有效读数";return false; }
    return true;
}
bool PressureClient_Command(uint8_t cmd,uint32_t duration) {
    if(!ready(cmd)) return false;
    uint8_t p[13]={0},n=0;
    if(cmd==PM_START) {
        if(!(s.flags&PM_ZEROED) || duration<100 || duration>3600000) { s.message="请先去皮，并确认捕捉时长";return false; }
        put32(p,duration);n=4;
    } else if(cmd==PM_SET_CONFIG) {
        if(!s.fit_valid) { s.message="请先计算有效校准";return false; }
        put32(p,(uint32_t)s.fit.zero);putf(p+4,s.fit.scale);putf(p+8,s.fit.area);p[12]=3;n=13;
    } else if(cmd==PM_SAVE && s.config.flags!=3) { s.message="尚未完成校准，不能保存";return false; }
    else if(cmd!=PM_TARE && cmd!=PM_STOP && cmd!=PM_SAVE && cmd!=PM_GET_CONFIG) return false;
    if(!transmit(cmd,p,n)) return false;
    s.pending=cmd;got_point=got_config=false;s.message=(cmd==PM_TARE || cmd==PM_POINT)?"采样1秒，请保持静止":"等待下位机回应";return true;
}
bool PressureClient_Point(float grams) {
    if(!ready(PM_POINT)) return false;
    if(!isfinite(grams) || grams<0 || grams>100000 || s.point_count>=PC_POINT_MAX) { s.message="重量无效或校准点已满";return false; }
    if(!transmit(PM_POINT,NULL,0)) return false;
    requested_grams=grams;s.pending=PM_POINT;got_point=got_config=false;s.message="采样1秒，请保持静止";return true;
}
void PressureClient_RemovePoint(bool all) { if(s.pending) return;if(all) s.point_count=0;else if(s.point_count) s.point_count--;s.fit_valid=false; }
void PressureClient_InvalidateFit(void) { s.fit_valid=false; }
bool PressureClient_Fit(float area) {
    s.fit_valid=false;
    if(s.pending || !isfinite(area) || area<1 || area>100000 || s.point_count<2) { s.message="至少空载与一个砝码点，面积需有效";return false; }
    double xm=0,ym=0,xx=0,xy=0;bool zero=false;
    float xmin=8388607,xmax=-8388608,ymin=100000,ymax=0,sdmax=0;
    for(unsigned i=0;i<s.point_count;i++) {
        PcPoint *p=s.points+i;xm+=p->raw;ym+=p->grams;zero|=p->grams==0;
        xmin=fminf(xmin,p->raw);xmax=fmaxf(xmax,p->raw);ymin=fminf(ymin,p->grams);ymax=fmaxf(ymax,p->grams);sdmax=fmaxf(sdmax,p->sd);
    }
    if(!zero || ymax-ymin<1 || xmax-xmin<fmaxf(100,10*sdmax)) { s.message="需空载点，重量或ADC变化不足";return false; }
    xm/=s.point_count;ym/=s.point_count;
    for(unsigned i=0;i<s.point_count;i++) { double x=(double)s.points[i].raw-xm;xx+=x*x;xy+=x*((double)s.points[i].grams-ym); }
    double scale=xy/xx,z=xm-ym/scale;
    if(!isfinite(scale) || fabs(scale)<1e-7 || fabs(scale)>100 || !isfinite(z) || z< -8388608 || z>8388607) { s.message="拟合异常，请检查受力和录入";return false; }
    s.fit=(PcCalibration){(int32_t)lround(z),(float)scale,area,3,0,true};
    double squared=0;float maximum=0;
    for(unsigned i=0;i<s.point_count;i++) {
        PcPoint *p=s.points+i;float error=PressureClient_Mmhg((p->raw-(float)s.fit.zero)*s.fit.scale-p->grams,area);
        squared+=(double)error*error;maximum=fmaxf(maximum,fabsf(error));
    }
    s.fit_rms=(float)sqrt(squared/s.point_count);s.fit_max=maximum;
    if(maximum>fmaxf(2,PressureClient_Mmhg(ymax-ymin,area)*0.005f)) { s.message="校准残差过大，请检查偏载或录入";return false; }
    s.fit_valid=true;s.message=s.point_count==2?"两点已拟合，建议增加校准点":"计算完成，请应用并独立复核";return true;
}
