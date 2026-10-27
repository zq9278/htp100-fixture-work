#include "fixture_ui.h"
#include "fixture_wire.h"
#include "fixture_buzzer.h"
#include "lvgl.h"
#include "main.h"
#include "usart.h"
#include <stdio.h>
LV_FONT_DECLARE(fixture_font28);
LV_FONT_DECLARE(fixture_font36);
LV_FONT_DECLARE(fixture_font48);
static lv_obj_t *home,*page,*title,*status,*details,*back_label;
static lv_obj_t *diagnostic_page,*diagnostic_text,*diagnostic_scroll,*diagnostic_button;
static uint8_t failure_frame[FX_FRAME_SIZE];
static FxParser parser;
static uint16_t seq;
static uint8_t mode,state;
static uint32_t last_rx,last_tx;
static bool accepted,cancelling,link_failed;
static const char *names[]={"耗材检测工装","眼盾初始化","制作演示耗材","眼盾出厂检测"};
static const char *errors[]={"", "未检测到耗材，请检查连接", "检测中耗材断开",
    "温度读取失败\n请检查传感器", "温度过高\n已停止加热", "加热超时\n未达到稳定条件",
    "耗材写入失败\n请检查连接", "耗材回读校验失败", "通讯中断\n已停止检测",
    "操作已取消", "功能指令错误", "加热输出启动失败", "供电中断，已停止检测",
    "耗材已报废", "耗材类型不匹配", "耗材标记读取失败\n请检查连接"};
static bool running(void) { return state>=FX_WAIT && state<=FX_STABLE; }
static void diagnostic_available(bool available) {
    lv_obj_t *back_button=lv_obj_get_parent(back_label);
    lv_obj_set_size(back_button,available?195:400,78);
    lv_obj_align(back_button,LV_ALIGN_TOP_LEFT,available?30:40,530);
    if(available) lv_obj_clear_flag(diagnostic_button,LV_OBJ_FLAG_HIDDEN);
    else {
        lv_obj_add_flag(diagnostic_button,LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(diagnostic_page,LV_OBJ_FLAG_HIDDEN);
    }
}
static void format_temperature(char *out,size_t size,int16_t centi) {
    if(centi==FX_TEMP_INVALID) snprintf(out,size,"无有效读值");
    else { int n=centi<0?-(int)centi:(int)centi;
        snprintf(out,size,"%s%d.%02d℃",centi<0?"-":"",n/100,n%100); }
}
static const char *communication_reason(const uint8_t *f) {
    uint32_t error=Fx_Get32(f+28);
    if(!(f[16]&FX_DIAG_IO_FAILED)) return "最近通信成功";
    if(error&4U) return "器件无应答(NACK)";
    if((error&32U) || f[19]==3) return "读取超时";
    if(f[19]==2) return "总线忙";
    if(error&1U) return "总线时序错误";
    if(error&2U) return "总线仲裁丢失";
    if(error&16U) return "I2C DMA错误";
    return "I2C通信异常";
}
static void failure_summary(const uint8_t *f,char *out,size_t size) {
    char temp[40]; format_temperature(temp,sizeof(temp),(int16_t)Fx_Get16(f+10));
    if(f[8]==FX_USED_EYE && (f[16]&FX_DIAG_MARK_VALID)) {
        snprintf(out,size,"标记地址 0x01-02\n读值 0x%04X\n要求 0xFFFF\n拔出后检测下一件",Fx_Get16(f+20));
    } else if(f[16]&FX_DIAG_IO_FAILED) {
        snprintf(out,size,"%s 0x%02X [0x%02X]\n%s\nHAL=%u ERR=%08lX\n点击诊断详情查看",
            f[17]==0x50?"EEPROM":f[17]==0x48?"TMP112":"I2C2",f[17],f[18],communication_reason(f),f[19],(unsigned long)Fx_Get32(f+28));
    } else if(f[8]==FX_OVER_TEMP) {
        snprintf(out,size,"TMP112 0x48 [00]\n实测 %s\n≥44℃ 连续 %u.%u秒\n拔出后检测下一件",temp,
            Fx_Get16(f+36)/1000,(Fx_Get16(f+36)%1000)/100);
    } else if(f[8]==FX_TEMP_COMM) {
        if(f[16]&FX_DIAG_TEMP_VALID)
            snprintf(out,size,"TMP112 0x48 [00]\n原始值 0x%04X\n有效范围 -40~125℃\n点击诊断详情查看",Fx_Get16(f+24));
        else snprintf(out,size,"TMP112 0x48 [00]\n本次无有效温度读值\n点击诊断详情查看");
    } else if(f[8]==FX_TIMEOUT) {
        snprintf(out,size,"目标43.00℃(传感器)\n末次读值 %s\n%lu秒内未稳定2秒\n点击诊断详情查看",temp,(unsigned long)(Fx_Get32(f+32)/1000));
    } else if(f[8]==FX_PWM_ERROR) snprintf(out,size,"加热输出 TIM14_CH1\nPWM启动失败\n末次温度 %s\n点击诊断详情查看",temp);
    else snprintf(out,size,"%s\n点击诊断详情查看",f[8]<sizeof(errors)/sizeof(errors[0])?errors[f[8]]:"未知故障");
}
static void diagnostic_back(lv_event_t *e) { (void)e; lv_obj_add_flag(diagnostic_page,LV_OBJ_FLAG_HIDDEN); }
static void diagnostic_open(lv_event_t *e) {
    (void)e;
    const uint8_t *f=failure_frame;
    char marker[40],service[40],raw[40],temp[40],text[1700],io_info[260];
    if(f[16]&FX_DIAG_MARK_VALID) snprintf(marker,sizeof(marker),"0x%04X",Fx_Get16(f+20));
    else snprintf(marker,sizeof(marker),"本次无有效读值");
    if(f[16]&FX_DIAG_SERVICE_VALID) snprintf(service,sizeof(service),"0x%04X",Fx_Get16(f+22));
    else snprintf(service,sizeof(service),"本次无有效读值");
    if(f[16]&FX_DIAG_TEMP_VALID) snprintf(raw,sizeof(raw),"0x%04X",Fx_Get16(f+24));
    else snprintf(raw,sizeof(raw),"本次无有效读值");
    format_temperature(temp,sizeof(temp),(int16_t)Fx_Get16(f+10));
    uint32_t error=Fx_Get32(f+28);
    const char *communication=communication_reason(f);
    if(f[26]) snprintf(io_info,sizeof(io_info),
        "最后器件 0x%02X\n寄存器/位置 0x%02X\n操作 %s，尝试 %u次\nHAL=%u，ERR=%08lX\n%s\nSCL=%u SDA=%u",
        f[17],f[18],f[40]==2?"写入":"读取",f[26],f[19],(unsigned long)error,communication,f[27]&1U,(f[27]>>1U)&1U);
    else snprintf(io_info,sizeof(io_info),"本次尚无通信记录\nSCL/SDA未采样");
    snprintf(text,sizeof(text),
        "失败原因\n%s\n\n"
        "失败时刻的数据快照\nI2C2 器件地址为7位\n"
        "%s\n\n"
        "EEPROM 器件 0x50\n使用标记 [0x01-02]\n读取 %s\n合格要求 0xFFFF\n"
        "其他值表示已报废\n服务字段 [0x03-04]\n读取 %s\n该字段不决定报废\n\n"
        "TMP112 器件 0x48\n温度寄存器 0x00\n原始数据 %s\n末次温度 %s\n"
        "此处温度未扣补偿\n有效范围 -40~125℃\n"
        "加热目标 43.00℃\n合格窗口 ±3℃\n连续稳定要求 2.00秒\n本次最长 %u.%02u秒\n"
        "本次检测 %lu.%u秒\n超温要求 ≥44℃ 5秒\n本次连续 %u.%02u秒\n\n"
        "PWM故障检查\nTIM14_CH1 加热输出\n非耗材寄存器错误\n\n"
        "通信失败不等于数值错\n无有效读值不会当作0\n检测全程不改写使用标记",
        f[8]<sizeof(errors)/sizeof(errors[0])?errors[f[8]]:"未知故障",io_info,marker,service,raw,temp,
        Fx_Get16(f+38)/1000,(Fx_Get16(f+38)%1000)/10,
        (unsigned long)(Fx_Get32(f+32)/1000),(unsigned)((Fx_Get32(f+32)%1000)/100),
        Fx_Get16(f+36)/1000,(Fx_Get16(f+36)%1000)/10);
    lv_label_set_text(diagnostic_text,text);
    lv_obj_scroll_to_y(diagnostic_scroll,0,LV_ANIM_OFF);
    lv_obj_clear_flag(diagnostic_page,LV_OBJ_FLAG_HIDDEN);
}
static void result_style(uint8_t result) {
    bool terminal=result==FX_PASS || result==FX_FAIL;
    lv_obj_set_style_bg_color(page,lv_color_hex(result==FX_PASS?0x087F3D:0xB71C1C),0);
    lv_obj_set_style_bg_opa(page,terminal?LV_OPA_COVER:LV_OPA_TRANSP,0);
    lv_obj_set_style_text_font(status,terminal?&fixture_font48:&fixture_font36,0);
}
static void transmit(uint8_t type) {
    uint8_t f[FX_FRAME_SIZE]; Fx_Frame(f,type,seq,mode); Fx_Seal(f);
    (void)HAL_UART_Transmit(&huart1,f,sizeof(f),20U); last_tx=HAL_GetTick();
}
static void show_home(void) {
    FixtureBuzzer_Stop();
    diagnostic_available(false);
    mode=FX_NONE; cancelling=false;
    lv_obj_add_flag(page,LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(home,LV_OBJ_FLAG_HIDDEN);
}
static void start(lv_event_t *e) {
    mode=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(++seq==0) ++seq;
    state=FX_WAIT; accepted=false; cancelling=false; link_failed=false;
    FixtureBuzzer_Stop(); result_style(FX_WAIT);
    diagnostic_available(false);
    last_rx=HAL_GetTick();
    lv_label_set_text(title,names[mode]); lv_label_set_text(status,"正在连接主机...");
    lv_label_set_text(details,mode==FX_INSPECT?"目标温度 42.5℃\n允许偏差 ±3℃\n连续稳定 2秒后合格":"请连接耗材，等待操作完成");
    lv_label_set_text(back_label,"取消并返回");
    lv_obj_add_flag(home,LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(page,LV_OBJ_FLAG_HIDDEN);
    transmit(FX_START);
}
static void back(lv_event_t *e) {
    (void)e;
    if(link_failed) { show_home(); return; }
    FixtureBuzzer_Stop();
    cancelling=true; last_rx=HAL_GetTick();
    lv_label_set_text(status,"正在取消\n等待主机关闭输出"); transmit(FX_CANCEL);
}
static lv_obj_t *label(lv_obj_t *parent,const char *text,int y,int height) {
    lv_obj_t *l=lv_label_create(parent); lv_obj_set_size(l,420,height);
    lv_obj_set_style_text_color(l,lv_color_hex(0xFFFFFF),0);
    lv_obj_align(l,LV_ALIGN_TOP_MID,0,y); lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
    lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP); lv_label_set_text(l,text); return l;
}
static lv_obj_t *button(lv_obj_t *parent,const char *text,int y,lv_event_cb_t cb,uintptr_t data) {
    lv_obj_t *b=lv_btn_create(parent); lv_obj_set_size(b,400,78);
    lv_obj_align(b,LV_ALIGN_TOP_MID,0,y); lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,(void *)data);
    lv_obj_t *l=lv_label_create(b); lv_label_set_text(l,text); lv_obj_center(l); return l;
}
void FixtureUi_Init(void) {
    FixtureBuzzer_Stop();
    parser.used=0; seq=(uint16_t)HAL_GetTick(); mode=FX_NONE;
    lv_obj_t *root=lv_scr_act();
    lv_obj_set_style_bg_color(root,lv_color_hex(0x101C2D),0);
    lv_obj_set_style_text_color(root,lv_color_hex(0xFFFFFF),0);
    lv_obj_set_style_text_font(root,&fixture_font28,0);
    home=lv_obj_create(root); page=lv_obj_create(root); diagnostic_page=lv_obj_create(root);
    lv_obj_t *panels[]={home,page,diagnostic_page};
    for(unsigned i=0;i<3;++i) {
        lv_obj_set_size(panels[i],480,640); lv_obj_center(panels[i]);
        lv_obj_clear_flag(panels[i],LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(panels[i],LV_OPA_TRANSP,0);
        lv_obj_set_style_border_width(panels[i],0,0); lv_obj_set_style_pad_all(panels[i],0,0);
    }
    label(home,names[0],48,48);
    label(home,"请选择需要执行的功能",110,42);
    button(home,names[1],200,start,FX_INITIALIZE);
    button(home,names[2],310,start,FX_DEMO);
    button(home,names[3],420,start,FX_INSPECT);
    title=label(page,"",35,60); status=label(page,"",125,125);
    details=label(page,"",270,230);
    lv_obj_set_style_text_font(details,&fixture_font36,0);
    lv_obj_set_style_text_line_space(details,5,0);
    back_label=button(page,"取消并返回",530,back,0);
    lv_obj_t *diagnostic_label=button(page,"诊断详情",530,diagnostic_open,0);
    diagnostic_button=lv_obj_get_parent(diagnostic_label);
    lv_obj_set_size(diagnostic_button,195,78); lv_obj_align(diagnostic_button,LV_ALIGN_TOP_LEFT,255,530);
    lv_obj_set_style_bg_color(diagnostic_page,lv_color_hex(0xB71C1C),0);
    lv_obj_set_style_bg_opa(diagnostic_page,LV_OPA_COVER,0);
    label(diagnostic_page,"失败诊断（可上下滑动）",25,50);
    diagnostic_scroll=lv_obj_create(diagnostic_page);
    lv_obj_set_size(diagnostic_scroll,450,425); lv_obj_align(diagnostic_scroll,LV_ALIGN_TOP_MID,0,85);
    lv_obj_set_style_bg_opa(diagnostic_scroll,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_width(diagnostic_scroll,0,0);
    lv_obj_set_style_pad_all(diagnostic_scroll,5,0);
    lv_obj_set_scroll_dir(diagnostic_scroll,LV_DIR_VER);
    diagnostic_text=lv_label_create(diagnostic_scroll);
    lv_obj_set_width(diagnostic_text,420); lv_obj_set_height(diagnostic_text,LV_SIZE_CONTENT);
    lv_obj_set_style_text_font(diagnostic_text,&fixture_font36,0);
    lv_obj_set_style_text_color(diagnostic_text,lv_color_hex(0xFFFFFF),0);
    lv_obj_set_style_text_line_space(diagnostic_text,5,0);
    button(diagnostic_page,"返回检测结果",530,diagnostic_back,0);
    show_home();
}
void FixtureUi_Feed(const uint8_t *data,size_t size) {
    uint8_t f[FX_FRAME_SIZE];
    while(size--) {
        if(!Fx_Parse(&parser,*data++,f) || f[3]!=FX_STATUS || mode==FX_NONE ||
           Fx_Get16(f+4)!=seq || f[6]!=mode || f[7]>FX_FAIL) continue;
        if(link_failed) continue; /* A late response must never replace a link failure with success. */
        uint8_t previous_state=state;
        accepted=true; last_rx=HAL_GetTick(); state=f[7];
        if(cancelling) {
            if(state==FX_FAIL && f[8]==FX_CANCELLED) show_home();
            continue;
        }
        result_style(state);
        diagnostic_available(mode==FX_INSPECT && state==FX_FAIL);
        if(state!=FX_PASS && previous_state==FX_PASS) FixtureBuzzer_Stop();
        if(state==FX_FAIL) {
            memcpy(failure_frame,f,sizeof(failure_frame));
            const char *headline=f[8]==FX_USED_EYE?"耗材已报废":"失败";
            if(mode==FX_INSPECT && (f[16]&FX_DIAG_IO_FAILED))
                headline=f[17]==0x48?"温度读取失败":"耗材通信失败";
            lv_label_set_text(status,headline);
            const char *why=f[8]<sizeof(errors)/sizeof(errors[0])?errors[f[8]]:"未知故障，请重新检测";
            bool auto_reset=f[8]!=FX_NO_EYE && f[8]!=FX_CANCELLED &&
                f[8]!=FX_LINK_LOST && f[8]!=FX_POWER_LOST && f[8]!=FX_BAD_COMMAND;
            char text[220]; snprintf(text,sizeof(text),"%s\n%s",why,
                auto_reset?"拔出耗材后等待下一件":"返回功能选择后重试");
            if(mode==FX_INSPECT) failure_summary(f,text,sizeof(text));
            lv_label_set_text(details,text);
        } else if(state==FX_PASS) {
            if(previous_state!=FX_PASS) FixtureBuzzer_Success();
            lv_label_set_text(status,mode==FX_INSPECT?"耗材正常": "成功");
            lv_label_set_text(details,mode==FX_INITIALIZE?"耗材初始化完成\n请拔出耗材\n自动等待下一件":mode==FX_DEMO?"演示耗材制作完成\n请拔出耗材\n自动等待下一件":"温度连续稳定2秒\n已停止加热，请拔出耗材\n自动等待下一件");
        } else {
            const char *states[]={"待机","等待插入耗材","正在写入并回读校验","正在加热","温度达标\n正在稳定计时"};
            lv_label_set_text(status,states[state]);
            if(previous_state==FX_PASS || previous_state==FX_FAIL) {
                lv_label_set_text(back_label,"取消并返回");
                if(mode!=FX_INSPECT) lv_label_set_text(details,"已复位，请插入下一件耗材");
            }
            if(mode==FX_INSPECT) {
                int16_t raw=(int16_t)Fx_Get16(f+10);
                char text[160];
                if(raw==FX_TEMP_INVALID) snprintf(text,sizeof(text),"目标温度 42.5℃\n等待温度数据\n稳定时间 0.0 / 2.0秒");
                else {
                    int display=(int)raw-50; /* Display excludes the 0.5C compensation. */
                    snprintf(text,sizeof(text),"目标温度 42.5℃\n当前温度 %s%d.%02d℃\n稳定时间 %u.%u / 2.0秒",
                        display<0?"-":"",(display<0?-display:display)/100,(display<0?-display:display)%100,
                        Fx_Get16(f+14)/1000,(Fx_Get16(f+14)%1000)/100);
                }
                lv_label_set_text(details,text);
            }
        }
        if(!running()) lv_label_set_text(back_label,mode==FX_INSPECT && state==FX_FAIL?"返回选择":"返回功能选择");
    }
}
void FixtureUi_Tick(void) {
    FixtureBuzzer_Tick();
    if(mode==FX_NONE || link_failed) return;
    uint32_t now=HAL_GetTick();
    if(now-last_rx>=4000U) {
        link_failed=true; transmit(FX_CANCEL);
        FixtureBuzzer_Stop(); result_style(FX_FAIL);
        lv_label_set_text(status,"失败");
        lv_label_set_text(details,"主机通讯超时\n请检查连接后重新进入功能");
        lv_label_set_text(back_label,"返回功能选择"); return;
    }
    if(now-last_tx>=250U) transmit(cancelling?FX_CANCEL:accepted?FX_HEARTBEAT:FX_START);
}
