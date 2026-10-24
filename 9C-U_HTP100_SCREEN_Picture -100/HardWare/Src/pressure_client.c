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
static bool wizard_internal;
static float check_min[PC_W_CHECK_POINTS],check_max[PC_W_CHECK_POINTS],cycle_up[PC_W_CAL_POINTS];
static void wizard_point(void);
static void wizard_interrupt(const char *message);
static bool same_calibration(const PcCalibration *a,const PcCalibration *b) {
    return a->zero==b->zero && a->flags==b->flags &&
        fabsf(a->scale-b->scale)<=fabsf(b->scale)*1e-5f && fabsf(a->area-b->area)<=0.01f;
}
bool PressureClient_WizardActive(void) { return s.wizard.phase>=PC_W_PRELOAD && s.wizard.phase<=PC_W_RESULT; }
static void wizard_interrupt(const char *message) {
    if(PressureClient_WizardActive()) { s.wizard.phase=PC_W_INTERRUPTED;s.wizard.collecting=false;s.wizard.passed=false;s.wizard.revision++; }
    s.message=message;
}
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
    wizard_interrupt(message);
    s.connected=false;s.config_valid=false;identified=false;s.pending=0;
    s.filtered=s.pressure=NAN;s.flags=0;s.point_count=0;s.fit_valid=false;
    s.message=message;have_clock=false;
}
void PressureClient_Init(const PressureClientPort *port) {
    io=port;memset(&s,0,sizeof(s));used=0;sequence=(uint16_t)io->now();
    wizard_internal=false;
    s.pressure=s.filtered=s.peak=s.minimum=s.sample_area=NAN;s.soc=255;s.message="正在连接G0下位机";
    identified=have_clock=false;last_rx=last_config=io->now();last_hello=io->now()-1000U;
}
const PressureClientState *PressureClient_Status(void) { return &s; }
float PressureClient_Mmhg(float grams,float area) { return grams*9.80665f*1000.0f/(area*133.3224f); }
float PressureClient_Force(float mmhg,float area) {
    if(!isfinite(mmhg) || !isfinite(area) || area<1 || area>100000) return NAN;
    return mmhg*133.3224f*area*1.0e-6f;
}
float PressureClient_Grams(float mmhg,float area) { return PressureClient_Force(mmhg,area)*1000.0f/9.80665f; }
static bool calibration(const uint8_t *p,PcCalibration *c) {
    c->zero=(int32_t)u32(p);c->scale=getf(p+4);c->area=getf(p+8);c->flags=p[12];c->generation=u32(p+13);c->dirty=p[17]!=0;
    return c->zero>=-8388608L && c->zero<=8388607L && isfinite(c->scale) &&
        fabsf(c->scale)>=1e-7f && fabsf(c->scale)<=100 && isfinite(c->area) &&
        c->area>=1 && c->area<=100000 && (c->flags==0 || c->flags==2 || c->flags==3);
}
static void received(const uint8_t *f) {
    uint8_t cmd=f[3],n=f[6];uint16_t seq=u16(f+4);const uint8_t *p=f+7;
    if(cmd==PM_INFO && n==4) {
        if(p[0]==1 && p[1]==1) identified=true;
        else s.message="请更新G0为持续峰值版本1.1";
        return;
    }
    if(cmd==PM_CONFIG && n==18) {
        PcCalibration config;
        if(!calibration(p,&config)) return;
        if(s.pending==PM_SET_CONFIG && seq==pending_sequence &&
           (config.zero!=s.fit.zero || fabsf(config.scale-s.fit.scale)>fabsf(s.fit.scale)*1e-5f ||
            fabsf(config.area-s.fit.area)>0.01f || config.flags!=3)) {
            s.pending=0;wizard_interrupt("参数回读不一致，请重新读取");return;
        }
        s.config=config;s.config_valid=true;last_config=io->now();
        if(PressureClient_WizardActive() && !same_calibration(&config,s.wizard.applied?&s.fit:&s.wizard.original) &&
           !(s.pending==PM_SET_CONFIG && seq==pending_sequence)) {
            wizard_interrupt("设备参数被修改，请重新开始校准");s.pending=0;return;
        }
        if(seq==pending_sequence) {
            got_config=true;
            if(s.pending==PM_GET_CONFIG) { s.pending=0;s.message="设备参数已读取"; }
        }
        return;
    }
    if(cmd==PM_STATUS && (n==52 || n==56)) {
        uint32_t clock=u32(p);
        if(have_clock && (int32_t)(clock-s.device_ms)<0) { disconnect("G0已重启，正在重新连接");return; }
        have_clock=true;s.device_ms=clock;
        bool was_connected=s.connected;
        last_rx=io->now();s.connected=identified;
        if(s.connected && !was_connected) s.message="下位机已连接，先去皮或校准";
        s.raw=(int32_t)u32(p+4);s.flags=u16(p+36);
        s.pressure=getf(p+8);s.filtered=getf(p+12);s.peak=getf(p+16);s.minimum=getf(p+20);
        s.elapsed=u32(p+24);s.duration=u32(p+28);s.samples=u32(p+32);
        /* Units must use the area from this same sample frame, never a stale
         * configuration read or an area still being edited on the screen. */
        s.sample_area=n==56?getf(p+52):NAN;
        if(!isfinite(s.sample_area) || s.sample_area<1 || s.sample_area>100000) s.sample_area=NAN;
        s.peak_state=p[38];s.soc=p[39];s.mv=u16(p+40);s.errors=u32(p+42);s.peak_at=u32(p+48);s.updates++;
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
        if(p[1]) { s.wizard.collecting=false;s.message=p[1]<8?errors[p[1]]:"设备返回未知错误";return; }
        if(pending==PM_POINT) {
            if(!got_point) { s.wizard.collecting=false;s.message="缺少采样结果，请重试";return; }
            if(s.wizard.collecting) { wizard_point();return; }
            if(s.point_count>=PC_POINT_MAX) { s.message="校准点已满";return; }
            s.points[s.point_count++]=result;s.fit_valid=false;s.message="已采集，请更换下一组砝码";
        } else if(pending==PM_SET_CONFIG || pending==PM_SAVE || pending==PM_TARE) {
            if(!got_config) { wizard_interrupt("缺少回读，结果未知，请重新读取");return; }
            if(PressureClient_WizardActive() && pending==PM_SET_CONFIG) {
                s.wizard.applied=true;s.wizard.phase=PC_W_VERIFY;s.wizard.step=s.wizard.repeat=s.wizard.checks=0;
                s.wizard.target=0;s.wizard.revision++;s.message="已应用RAM，请开始三轮加载卸载复核";return;
            }
            if(PressureClient_WizardActive() && pending==PM_SAVE) {
                if(s.config.dirty || !same_calibration(&s.config,&s.fit)) { wizard_interrupt("保存回读不一致，请检查设备参数");return; }
                s.wizard.phase=PC_W_SAVED;s.wizard.revision++;s.message="复核通过，保存成功，断电保留";return;
            }
            s.message=pending==PM_SAVE?"保存成功，断电保留":pending==PM_TARE?"去皮完成，复核后可保存":"已应用，请用独立砝码复核";
        } else if(pending==PM_CLEAR) { s.peak=s.minimum=NAN;s.samples=s.errors=s.elapsed=s.peak_at=0;s.message="峰值已清除，正在持续捕捉"; }
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
    if(s.pending && now-command_started>=4000U) { s.pending=0;wizard_interrupt("命令超时，结果未知，请重新读取"); }
    if(!identified && now-last_hello>=1000U) { last_hello=now;(void)transmit(PM_HELLO,NULL,0); }
    else if(s.connected && !s.pending && now-last_config>=2000U) {
        if(transmit(PM_GET_CONFIG,NULL,0)) { s.pending=PM_GET_CONFIG;got_config=false; }
    }
}
static bool ready(uint8_t cmd) {
    if(PressureClient_WizardActive() && !wizard_internal && cmd!=PM_GET_CONFIG && cmd!=PM_CLEAR) { s.message="请先完成或退出引导校准";return false; }
    if(!s.connected || !s.config_valid) { s.message="下位机未连接或参数未读取";return false; }
    if(s.pending) { s.message="请等待当前操作完成";return false; }
    if((cmd==PM_TARE || cmd==PM_POINT) && !(s.flags&PM_VALID)) { s.message="传感器当前无有效读数";return false; }
    return true;
}
bool PressureClient_Command(uint8_t cmd,uint32_t duration) {
    (void)duration; /* Retained API argument; capture no longer has a duration. */
    if(!ready(cmd)) return false;
    uint8_t p[13]={0},n=0;
    if(cmd==PM_CLEAR) {
        if(!(s.flags&PM_ZEROED)) { s.message="请先空载去皮或校准";return false; }
    } else if(cmd==PM_SET_CONFIG) {
        if(!s.fit_valid) { s.message="请先计算有效校准";return false; }
        put32(p,(uint32_t)s.fit.zero);putf(p+4,s.fit.scale);putf(p+8,s.fit.area);p[12]=3;n=13;
    } else if(cmd==PM_SAVE && s.config.flags!=3) { s.message="尚未完成校准，不能保存";return false; }
    else if(cmd!=PM_TARE && cmd!=PM_SAVE && cmd!=PM_GET_CONFIG) return false;
    if(!transmit(cmd,p,n)) return false;
    s.pending=cmd;got_point=got_config=false;s.message=(cmd==PM_TARE || cmd==PM_POINT)?"采样1秒，请保持静止":"等待下位机回应";return true;
}
bool PressureClient_Point(float grams) {
    if(!ready(PM_POINT)) return false;
    if(!isfinite(grams) || grams<0 || grams>100000 || (!wizard_internal && s.point_count>=PC_POINT_MAX)) { s.message="重量无效或校准点已满";return false; }
    if(!transmit(PM_POINT,NULL,0)) return false;
    requested_grams=grams;s.pending=PM_POINT;got_point=got_config=false;s.message="采样1秒，请保持静止";return true;
}
void PressureClient_RemovePoint(bool all) { if(s.pending || PressureClient_WizardActive()) return;if(all) s.point_count=0;else if(s.point_count) s.point_count--;s.fit_valid=false; }
void PressureClient_InvalidateFit(void) { if(!PressureClient_WizardActive()) s.fit_valid=false; }
bool PressureClient_Fit(float area) {
    if(PressureClient_WizardActive() && !wizard_internal) { s.message="请按引导完成校准";return false; }
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
    /* The guided force calibration uses the user's tolerance in grams below;
     * its acceptance must be independent of pressure conversion area. */
    if(!wizard_internal && maximum>fmaxf(2,PressureClient_Mmhg(ymax-ymin,area)*0.005f)) { s.message="校准残差过大，请检查偏载或录入";return false; }
    s.fit_valid=true;s.message=s.point_count==2?"两点已拟合，建议增加校准点":"计算完成，请应用并独立复核";return true;
}

static float wizard_target(unsigned step) { return s.wizard.capacity*(float)(step<=5?step:10-step)/5.0f; }
static float measured(const PcCalibration *c,float raw) { return c->flags&2?(raw-(float)c->zero)*c->scale:NAN; }
static void reset_checks(void) {
    PcWizard *w=&s.wizard;w->step=w->repeat=w->checks=0;w->target=0;w->passed=false;
    w->max_error=w->repeatability=w->hysteresis=w->zero_error=0;
    memset(w->verification,0,sizeof(w->verification));w->revision++;
}
bool PressureClient_WizardStart(float capacity,float area,float tolerance) {
    if(PressureClient_WizardActive()) { s.message="请先结束当前校准";return false; }
    if(!ready(PM_POINT)) return false;
    if(!isfinite(capacity) || capacity<5 || capacity>100000 || !isfinite(area) || area<1 || area>100000 ||
       !isfinite(tolerance) || tolerance<0.001f || tolerance>capacity*0.1f) {
        s.message="量程需5至100000g，误差不超过量程10%";return false;
    }
    uint32_t revision=s.wizard.revision+1;
    memset(&s.wizard,0,sizeof(s.wizard));s.wizard.phase=PC_W_PRELOAD;s.wizard.revision=revision;
    s.wizard.capacity=capacity;s.wizard.area=area;s.wizard.tolerance=tolerance;s.wizard.original=s.config;
    s.point_count=0;s.fit_valid=false;s.message="确认额定载荷后，预加载卸载三次";return true;
}
bool PressureClient_WizardNext(void) {
    PcWizard *w=&s.wizard;
    if(!PressureClient_WizardActive()) { s.message="请先开始引导校准";return false; }
    if(s.pending) { s.message="采样中，请保持静止";return false; }
    if(w->phase==PC_W_PRELOAD) { w->phase=PC_W_LOAD;w->target=0;w->revision++;s.message="保留加载盘，移除砝码，采集空载";return true; }
    wizard_internal=true;bool ok=false;
    if(w->phase==PC_W_LOAD || w->phase==PC_W_VERIFY) {
        ok=PressureClient_Point(w->target);w->collecting=ok;
    } else if(w->phase==PC_W_FIT) ok=PressureClient_Command(PM_SET_CONFIG,0);
    else if(w->phase==PC_W_RESULT) {
        if(w->passed) ok=PressureClient_Command(PM_SAVE,0);
        else s.message="复核未通过，请重测或重新校准";
    }
    wizard_internal=false;return ok;
}
bool PressureClient_WizardRetry(void) {
    PcWizard *w=&s.wizard;
    if(s.pending || w->phase!=PC_W_RESULT || !w->applied || !same_calibration(&s.config,&s.fit)) return false;
    reset_checks();w->phase=PC_W_VERIFY;s.message="请从空载开始，重新进行三轮复核";return true;
}
bool PressureClient_WizardCancel(void) {
    if(s.pending) { s.message="请等待采样结束再退出";return false; }
    bool applied=s.wizard.applied,saved=s.wizard.phase==PC_W_SAVED;
    uint32_t revision=s.wizard.revision+1;
    memset(&s.wizard,0,sizeof(s.wizard));s.wizard.revision=revision;
    s.point_count=0;s.fit_valid=false;
    s.message=saved?"已结束，已保存参数保留":applied?"已退出，RAM参数仍有效，未自动保存":"已退出，原设备参数保留";return true;
}
static void wizard_point(void) {
    PcWizard *w=&s.wizard;w->collecting=false;
    if(w->phase==PC_W_LOAD) {
        if(s.point_count>=PC_W_CAL_POINTS || w->step!=s.point_count) { wizard_interrupt("校准次序异常，请重新开始");return; }
        s.points[s.point_count++]=result;
        w->calibration[w->step]=(PcCheck){w->target,measured(&w->original,result.raw)-w->target,NAN,0};
        w->step++;w->revision++;
        if(w->step<PC_W_CAL_POINTS) { w->target=wizard_target(w->step);s.message="已采集，请按提示更换砝码";return; }
        wizard_internal=true;bool ok=PressureClient_Fit(w->area);wizard_internal=false;
        if(!ok) { w->phase=PC_W_INTERRUPTED;return; }
        float maximum=0;
        for(unsigned i=0;i<PC_W_CAL_POINTS;i++) {
            float error=measured(&s.fit,s.points[i].raw)-s.points[i].grams;
            w->calibration[i].after=error;maximum=fmaxf(maximum,fabsf(error));
        }
        if(maximum>w->tolerance) { s.fit_valid=false;wizard_interrupt("拟合误差超出设定值，请检查加载");return; }
        w->phase=PC_W_FIT;s.message="六点计算完成，应用后独立复核";return;
    }
    if(w->phase!=PC_W_VERIFY || !w->applied) return;
    unsigned i=w->step;float value=measured(&s.fit,result.raw),error=value-w->target;
    PcCheck *c=&w->verification[i];
    if(!w->repeat) { *c=(PcCheck){w->target,0,0,0};check_min[i]=check_max[i]=value; }
    check_min[i]=fminf(check_min[i],value);check_max[i]=fmaxf(check_max[i],value);
    c->before+=(measured(&w->original,result.raw)-w->target-c->before)/(float)(w->repeat+1);
    c->after+=(error-c->after)/(float)(w->repeat+1);c->spread=check_max[i]-check_min[i];
    w->max_error=fmaxf(w->max_error,fabsf(error));w->repeatability=fmaxf(w->repeatability,c->spread);
    if(i<=5) cycle_up[i]=value;
    else w->hysteresis=fmaxf(w->hysteresis,fabsf(value-cycle_up[10-i]));
    if(i==10) w->zero_error=fmaxf(w->zero_error,fabsf(value));
    if(w->checks<i+1) w->checks=(uint8_t)(i+1);
    w->step++;w->revision++;
    if(w->step==PC_W_CHECK_POINTS) {
        w->step=0;w->repeat++;
        if(w->repeat==PC_W_REPEATS) {
            w->phase=PC_W_RESULT;
            w->passed=w->max_error<=w->tolerance && w->repeatability<=w->tolerance && w->hysteresis<=w->tolerance && w->zero_error<=w->tolerance;
            s.message=w->passed?"三轮复核通过，请保存参数":"复核未通过，请检查偏载摩擦或重测";return;
        }
    }
    w->target=wizard_target(w->step);s.message="已记录，请按提示加载或卸载";
}
