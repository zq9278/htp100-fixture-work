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
    mode=FX_NONE; cancelling=false;
    lv_obj_add_flag(page,LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(home,LV_OBJ_FLAG_HIDDEN);
}
static void start(lv_event_t *e) {
    mode=(uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if(++seq==0) ++seq;
    state=FX_WAIT; accepted=false; cancelling=false; link_failed=false;
    FixtureBuzzer_Stop(); result_style(FX_WAIT);
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
    home=lv_obj_create(root); page=lv_obj_create(root);
    lv_obj_t *panels[]={home,page};
    for(unsigned i=0;i<2;++i) {
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
        if(state!=FX_PASS && previous_state==FX_PASS) FixtureBuzzer_Stop();
        if(state==FX_FAIL) {
            lv_label_set_text(status,f[8]==FX_USED_EYE?"耗材已报废":"失败");
            const char *why=f[8]<sizeof(errors)/sizeof(errors[0])?errors[f[8]]:"未知故障，请重新检测";
            bool auto_reset=f[8]!=FX_NO_EYE && f[8]!=FX_CANCELLED &&
                f[8]!=FX_LINK_LOST && f[8]!=FX_POWER_LOST && f[8]!=FX_BAD_COMMAND;
            char text[220]; snprintf(text,sizeof(text),"%s\n%s",why,
                auto_reset?"拔出耗材后等待下一件":"返回功能选择后重试");
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
        if(!running()) lv_label_set_text(back_label,"返回功能选择");
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
