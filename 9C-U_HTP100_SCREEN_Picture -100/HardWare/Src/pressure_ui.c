#include "pressure_ui.h"
#include "pressure_client.h"
#include "fixture_buzzer.h"
#include "lvgl.h"
#include "main.h"
#include "usart.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
LV_FONT_DECLARE(pressure_font18);
LV_FONT_DECLARE(pressure_font20);
LV_FONT_DECLARE(pressure_font28);
LV_FONT_DECLARE(pressure_font48);
static lv_obj_t *live,*calibration_page,*keyboard,*connection_label,*power_label,*value_label,*raw_label;
static lv_obj_t *peak_label,*peak_detail,*message_label,*chart,*time_input,*area_input,*mass_input,*point_table,*fit_label,*config_label;
static lv_obj_t *load_label,*peak_load_label;
static lv_obj_t *pm_start_button,*pm_stop_button,*pm_tare_button,*pm_point_button,*pm_apply_button,*pm_save_button,*pm_fit_button;
static lv_obj_t *pm_cal_button,*pm_back_button;
static lv_chart_series_t *series;
static uint32_t rendered,chart_update;
static uint8_t previous_state;
static uint8_t displayed_points;
static bool area_initialized;
static const char *ui_notice;
static uint32_t notice_started;
static void notice(const char *value) {
    ui_notice=value;notice_started=HAL_GetTick();lv_label_set_text(message_label,value);
}
static bool transmit(const uint8_t *p,uint16_t n) { return HAL_UART_Transmit(&huart1,(uint8_t *)p,n,20)==HAL_OK; }
static const PressureClientPort port={HAL_GetTick,transmit};
static lv_obj_t *text(lv_obj_t *parent,const char *value,int x,int y,int w,int h,const lv_font_t *font) {
    lv_obj_t *o=lv_label_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(0xE9F3FF),0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP);lv_label_set_text(o,value);return o;
}
static void enable(lv_obj_t *o,bool enabled) { if(enabled) lv_obj_clear_state(o,LV_STATE_DISABLED);else lv_obj_add_state(o,LV_STATE_DISABLED); }
static void keyboard_event(lv_event_t *e) {
    if(lv_event_get_code(e)==LV_EVENT_READY || lv_event_get_code(e)==LV_EVENT_CANCEL) {
        lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);lv_keyboard_set_textarea(keyboard,NULL);
    }
}
static void input_event(lv_event_t *e) {
    lv_obj_t *target=lv_event_get_target(e);
    if(lv_event_get_code(e)==LV_EVENT_FOCUSED || lv_event_get_code(e)==LV_EVENT_CLICKED) {
        lv_keyboard_set_textarea(keyboard,target);lv_obj_clear_flag(keyboard,LV_OBJ_FLAG_HIDDEN);
    }
    if(target==area_input && lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED) PressureClient_InvalidateFit();
}
static lv_obj_t *input(lv_obj_t *parent,const char *value,int x,int y,int w) {
    lv_obj_t *o=lv_textarea_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,42);
    lv_textarea_set_one_line(o,true);lv_textarea_set_accepted_chars(o,"0123456789.");lv_textarea_set_max_length(o,10);
    lv_obj_set_style_pad_all(o,5,0);lv_obj_set_style_bg_color(o,lv_color_hex(0x23354D),0);
    lv_obj_set_style_text_color(o,lv_color_hex(0xFFFFFF),0);lv_textarea_set_text(o,value);
    lv_obj_add_event_cb(o,input_event,LV_EVENT_ALL,NULL);return o;
}
static bool numeric(lv_obj_t *o,float low,float high,float *value) {
    const char *p=lv_textarea_get_text(o);char *end;float x=strtof(p,&end);
    if(!*p || *end || !isfinite(x) || x<low || x>high) return false;
    *value=x;return true;
}
static void show_points(void) {
    const PressureClientState *s=PressureClient_Status();char b[64];
    if(displayed_points==s->point_count) return;
    displayed_points=s->point_count;
    lv_table_set_row_cnt(point_table,(uint16_t)(s->point_count+1));
    for(unsigned i=0;i<s->point_count;i++) {
        const PcPoint *p=s->points+i;
        snprintf(b,sizeof(b),"%.1f",(double)p->grams);lv_table_set_cell_value(point_table,i+1,0,b);
        snprintf(b,sizeof(b),"%.0f",(double)p->raw);lv_table_set_cell_value(point_table,i+1,1,b);
        snprintf(b,sizeof(b),"%.2f",(double)p->sd);lv_table_set_cell_value(point_table,i+1,2,b);
    }
}
enum { UI_START=1,UI_STOP,UI_TARE,UI_CAL,UI_BACK,UI_POINT,UI_REMOVE,UI_CLEAR,UI_FIT,UI_APPLY,UI_SAVE,UI_READ };
static void action(lv_event_t *e) {
    unsigned id=(unsigned)(uintptr_t)lv_event_get_user_data(e);float value;
    ui_notice=NULL;
    switch(id) {
    case UI_START:
        if(numeric(time_input,0.1f,3600,&value)) (void)PressureClient_Command(PM_START,(uint32_t)lroundf(value*1000));
        else { notice("时长需为0.1至3600秒");return; } break;
    case UI_STOP: (void)PressureClient_Command(PM_STOP,0);break;
    case UI_TARE: (void)PressureClient_Command(PM_TARE,0);break;
    case UI_CAL: lv_obj_add_flag(live,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(calibration_page,LV_OBJ_FLAG_HIDDEN);break;
    case UI_BACK: lv_obj_add_flag(calibration_page,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(live,LV_OBJ_FLAG_HIDDEN);break;
    case UI_POINT:
        if(numeric(mass_input,0,100000,&value)) (void)PressureClient_Point(value);
        else { notice("请输入有效砝码质量");return; } break;
    case UI_REMOVE: PressureClient_RemovePoint(false);break;
    case UI_CLEAR: PressureClient_RemovePoint(true);break;
    case UI_FIT:
        if(numeric(area_input,1,100000,&value)) (void)PressureClient_Fit(value);
        else { notice("有效面积需为1至100000");return; } break;
    case UI_APPLY: (void)PressureClient_Command(PM_SET_CONFIG,0);break;
    case UI_SAVE: (void)PressureClient_Command(PM_SAVE,0);break;
    case UI_READ: (void)PressureClient_Command(PM_GET_CONFIG,0);break;
    }
    show_points();lv_label_set_text(message_label,PressureClient_Status()->message);rendered=HAL_GetTick()-100U;
}
static lv_obj_t *button(lv_obj_t *parent,const char *label,int x,int y,int w,unsigned id) {
    lv_obj_t *o=lv_btn_create(parent);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,46);
    lv_obj_set_style_bg_color(o,lv_color_hex(0x205A92),0);lv_obj_set_style_radius(o,7,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(0x23354D),LV_STATE_DISABLED);
    lv_obj_set_style_text_color(o,lv_color_hex(0x8294AA),LV_STATE_DISABLED);
    lv_obj_add_event_cb(o,action,LV_EVENT_CLICKED,(void *)(uintptr_t)id);
    lv_obj_t *l=lv_label_create(o);lv_label_set_text(l,label);lv_obj_center(l);return o;
}
static lv_obj_t *panel(lv_obj_t *root) {
    lv_obj_t *o=lv_obj_create(root);lv_obj_set_size(o,480,600);lv_obj_set_pos(o,0,0);
    lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_style_pad_all(o,0,0);
    lv_obj_set_style_border_width(o,0,0);lv_obj_set_style_bg_opa(o,LV_OPA_TRANSP,0);return o;
}
void PressureUi_Init(void) {
    PressureClient_Init(&port);FixtureBuzzer_Stop();
    rendered=HAL_GetTick()-100;chart_update=0;previous_state=0;area_initialized=false;displayed_points=255;ui_notice=NULL;
    lv_obj_t *root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root,lv_color_hex(0x101C2D),0);lv_obj_set_style_text_color(root,lv_color_hex(0xE9F3FF),0);
    lv_obj_set_style_text_font(root,&pressure_font20,0);live=panel(root);calibration_page=panel(root);
    lv_obj_add_flag(calibration_page,LV_OBJ_FLAG_HIDDEN);
    text(live,"压力传感器检测",18,8,440,42,&pressure_font28);
    connection_label=text(live,"正在连接G0下位机",18,51,444,29,&pressure_font18);
    power_label=text(live,"供电未知  电量未知",18,80,444,29,&pressure_font18);
    value_label=text(live,"--.- mmHg",18,108,444,62,&pressure_font48);
    lv_obj_set_style_text_color(value_label,lv_color_hex(0x68C8FF),0);
    load_label=text(live,"力值 ---.--- N  等效 ---.- g",18,174,444,30,&pressure_font20);
    lv_obj_set_style_text_color(load_label,lv_color_hex(0x68C8FF),0);
    raw_label=text(live,"瞬时 --.-  ADC --",18,207,444,28,&pressure_font18);
    chart=lv_chart_create(live);lv_obj_set_pos(chart,18,241);lv_obj_set_size(chart,444,94);
    lv_obj_set_style_bg_color(chart,lv_color_hex(0x182B42),0);lv_obj_set_style_line_color(chart,lv_color_hex(0x30455F),LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart,6,0);lv_obj_set_style_size(chart,0,LV_PART_INDICATOR);
    lv_chart_set_type(chart,LV_CHART_TYPE_LINE);lv_chart_set_range(chart,LV_CHART_AXIS_PRIMARY_Y,-20,500);
    lv_chart_set_point_count(chart,200);series=lv_chart_add_series(chart,lv_color_hex(0x68C8FF),LV_CHART_AXIS_PRIMARY_Y);
    peak_label=text(live,"峰值 --.- mmHg",18,344,444,40,&pressure_font28);
    lv_obj_set_style_text_color(peak_label,lv_color_hex(0x74E4C9),0);
    peak_load_label=text(live,"峰值力 ---.--- N  ---.- g",18,387,444,28,&pressure_font18);
    lv_obj_set_style_text_color(peak_load_label,lv_color_hex(0x74E4C9),0);
    peak_detail=text(live,"设置时长后开始捕捉",18,420,444,52,&pressure_font18);
    text(live,"秒",155,490,30,35,&pressure_font20);time_input=input(live,"10.0",18,480,128);
    pm_start_button=button(live,"开始捕捉",198,480,128,UI_START);
    pm_stop_button=button(live,"提前结束",338,480,124,UI_STOP);
    pm_tare_button=button(live,"空载去皮",18,540,210,UI_TARE);
    pm_cal_button=button(live,"砝码校准",242,540,220,UI_CAL);
    text(calibration_page,"砝码校准",18,8,295,42,&pressure_font28);pm_back_button=button(calibration_page,"返回",358,8,104,UI_BACK);
    text(calibration_page,"先采空载0g，再放已知砝码\n每点静置采样1秒，最多8点",18,61,444,57,&pressure_font18);
    text(calibration_page,"面积 mm²",18,127,130,33,&pressure_font20);area_input=input(calibration_page,"84.8",153,121,160);
    text(calibration_page,"砝码 g",18,181,130,33,&pressure_font20);mass_input=input(calibration_page,"0",153,175,122);
    pm_point_button=button(calibration_page,"采集此点",291,173,171,UI_POINT);
    point_table=lv_table_create(calibration_page);lv_obj_set_pos(point_table,18,234);lv_obj_set_size(point_table,444,155);
    lv_table_set_col_cnt(point_table,3);lv_table_set_col_width(point_table,0,106);lv_table_set_col_width(point_table,1,194);lv_table_set_col_width(point_table,2,130);
    lv_obj_set_style_text_font(point_table,&pressure_font18,0);lv_obj_set_style_text_font(point_table,&pressure_font18,LV_PART_ITEMS);
    lv_obj_set_style_bg_color(point_table,lv_color_hex(0x182B42),LV_PART_ITEMS);lv_obj_set_style_bg_color(point_table,lv_color_hex(0x182B42),0);
    lv_obj_set_style_text_color(point_table,lv_color_hex(0xE9F3FF),LV_PART_ITEMS);
    lv_obj_set_style_pad_ver(point_table,1,LV_PART_ITEMS);lv_obj_set_style_pad_hor(point_table,8,LV_PART_ITEMS);
    lv_table_set_cell_value(point_table,0,0,"质量 g");lv_table_set_cell_value(point_table,0,1,"ADC均值");lv_table_set_cell_value(point_table,0,2,"标准差");
    button(calibration_page,"删除末点",18,401,132,UI_REMOVE);button(calibration_page,"清空",162,401,100,UI_CLEAR);
    pm_fit_button=button(calibration_page,"计算校准",274,401,188,UI_FIT);
    pm_apply_button=button(calibration_page,"应用校准",18,459,210,UI_APPLY);
    pm_save_button=button(calibration_page,"保存参数",242,459,220,UI_SAVE);
    fit_label=text(calibration_page,"至少采集空载和一个砝码点",18,515,444,56,&pressure_font18);
    config_label=text(calibration_page,"参数未读取",18,568,325,28,&pressure_font18);button(calibration_page,"读取",358,558,104,UI_READ);
    message_label=text(root,"正在连接G0下位机",18,605,444,31,&pressure_font18);
    keyboard=lv_keyboard_create(lv_layer_top());lv_obj_set_size(keyboard,480,255);lv_obj_align(keyboard,LV_ALIGN_BOTTOM_MID,0,0);
    lv_keyboard_set_mode(keyboard,LV_KEYBOARD_MODE_NUMBER);lv_obj_set_style_text_font(keyboard,&lv_font_montserrat_18,LV_PART_ITEMS);
    lv_obj_add_event_cb(keyboard,keyboard_event,LV_EVENT_ALL,NULL);lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);
}
void PressureUi_Feed(const uint8_t *p,size_t n) { PressureClient_Feed(p,n); }
void PressureUi_Tick(void) {
    PressureClient_Tick();FixtureBuzzer_Tick();uint32_t now=HAL_GetTick();
    if(now-rendered<100U) return;
    rendered=now;
    const PressureClientState *s=PressureClient_Status();char b[160];
    bool linked=s->connected && s->config_valid,free=linked && !s->pending,active=s->peak_state==PM_RUNNING;
    const char *quality=!(s->flags&PM_VALID)?"传感器无有效数据":!(s->flags&PM_ZEROED)?"请先空载去皮":s->flags&PM_CALIBRATED?"已校准":"未校准，当前为估算值";
    snprintf(b,sizeof(b),"%s  %s",linked?"G0已连接":"G0未连接",linked?quality:"");lv_label_set_text(connection_label,b);
    const char *supply=!(s->flags&PM_POWER_VALID)?"供电未知":s->flags&PM_FULL?"已充满":s->flags&PM_CHARGING?"外部供电/充电":"电池供电";
    if(s->soc<=100 && linked) snprintf(b,sizeof(b),"%s  %u%%  %umV",supply,(unsigned)s->soc,(unsigned)s->mv);
    else snprintf(b,sizeof(b),"%s  电量未知",linked?supply:"供电未知");
    lv_label_set_text(power_label,b);
    if(linked && isfinite(s->filtered)) snprintf(b,sizeof(b),"%.1f mmHg",(double)s->filtered);else snprintf(b,sizeof(b),"--.- mmHg");lv_label_set_text(value_label,b);
    float force=PressureClient_Force(s->filtered,s->sample_area),grams=PressureClient_Grams(s->filtered,s->sample_area);
    if(linked && isfinite(force)) snprintf(b,sizeof(b),"力值 %.3f N  等效 %.1f g",(double)force,(double)grams);
    else snprintf(b,sizeof(b),"力值 ---.--- N  等效 ---.- g");
    lv_label_set_text(load_label,b);
    if(linked && isfinite(s->pressure)) snprintf(b,sizeof(b),"瞬时 %.1f  ADC %ld",(double)s->pressure,(long)s->raw);
    else snprintf(b,sizeof(b),"瞬时 --.-  ADC %s",linked && (s->flags&PM_VALID)?"有数据，待去皮":"--");
    lv_label_set_text(raw_label,b);
    if(isfinite(s->peak)) snprintf(b,sizeof(b),"峰值 %.1f mmHg",(double)s->peak);else snprintf(b,sizeof(b),"峰值 --.- mmHg");lv_label_set_text(peak_label,b);
    force=PressureClient_Force(s->peak,s->sample_area);grams=PressureClient_Grams(s->peak,s->sample_area);
    if(isfinite(force)) snprintf(b,sizeof(b),"峰值力 %.3f N  %.1f g",(double)force,(double)grams);
    else snprintf(b,sizeof(b),"峰值力 ---.--- N  ---.- g");
    lv_label_set_text(peak_load_label,b);
    static const char *states[]={"待开始","捕捉中","已完成","提前结束"};
    snprintf(b,sizeof(b),"%s %.1f/%.1f秒  %lu样本\n峰值时刻 %.2f秒  异常 %lu%s",linked && s->peak_state<4?states[s->peak_state]:"通讯中断",
        (double)s->elapsed/1000,(double)s->duration/1000,(unsigned long)s->samples,(double)s->peak_at/1000,(unsigned long)s->errors,s->errors?" 结果不完整":"");lv_label_set_text(peak_detail,b);
    if(chart_update!=s->updates) {
        chart_update=s->updates;float p=s->pressure;
        if(isfinite(p) && fabsf(p)<30000) { lv_chart_set_range(chart,LV_CHART_AXIS_PRIMARY_Y,(lv_coord_t)fminf(-20,p-10),(lv_coord_t)fmaxf(500,p+50));lv_chart_set_next_value(chart,series,(lv_coord_t)lroundf(p)); }
        else lv_chart_set_next_value(chart,series,LV_CHART_POINT_NONE);
    }
    if(linked && s->peak_state==PM_COMPLETE && previous_state==PM_RUNNING) FixtureBuzzer_Success();
    previous_state=s->peak_state;
    enable(pm_start_button,free && !active && (s->flags&PM_VALID) && (s->flags&PM_ZEROED));
    enable(pm_stop_button,free && active);enable(pm_tare_button,free && !active && (s->flags&PM_VALID));
    enable(pm_point_button,free && !active && (s->flags&PM_VALID) && s->point_count<PC_POINT_MAX);
    enable(pm_apply_button,free && !active && s->fit_valid);enable(pm_save_button,free && !active && s->config.flags==3);
    enable(pm_fit_button,!s->pending && s->point_count>=2);enable(time_input,!active && !s->pending);enable(area_input,!s->pending && !active);enable(mass_input,!s->pending);
    if(!area_initialized && s->config_valid) { snprintf(b,sizeof(b),"%.3f",(double)s->config.area);lv_textarea_set_text(area_input,b);area_initialized=true; }
    if(s->fit_valid) snprintf(b,sizeof(b),"零点 %ld  g/count %.7f\n残差 RMS %.2f  最大 %.2f mmHg",(long)s->fit.zero,(double)s->fit.scale,(double)s->fit_rms,(double)s->fit_max);
    else snprintf(b,sizeof(b),"已采集 %u/8 点\n先计算，再应用，复核后保存",(unsigned)s->point_count);
    lv_label_set_text(fit_label,b);
    snprintf(b,sizeof(b),"%s  %s",s->config_valid?(s->config.flags==3?"设备已校准":"设备未校准"):"未读取",s->config_valid?(s->config.dirty?"RAM未保存":"已保存"):"");lv_label_set_text(config_label,b);
    show_points();lv_label_set_text(message_label,ui_notice && now-notice_started<4000U?ui_notice:s->message);
}
