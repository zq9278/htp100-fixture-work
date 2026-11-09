#include "pressure_ui.h"
#include "pressure_client.h"
#include "fixture_buzzer.h"
#include "lvgl.h"
#include "main.h"
#include "usart.h"
#include "combined_ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
LV_FONT_DECLARE(pressure_font18);
LV_FONT_DECLARE(pressure_font16);
LV_FONT_DECLARE(pressure_font20);
LV_FONT_DECLARE(pressure_font28);
LV_FONT_DECLARE(pressure_font48);
static lv_obj_t *live,*calibration_page,*keyboard,*connection_label,*power_label,*value_label,*raw_label;
static lv_obj_t *peak_label,*peak_detail,*message_label,*chart,*area_input,*capacity_input,*tolerance_input,*point_table,*fit_label;
static lv_obj_t *wizard_step_label,*wizard_guide_label,*pm_next_button,*pm_retry_button,*pm_cancel_button;
static lv_obj_t *load_label,*peak_load_label,*reference_table;
static lv_obj_t *pm_clear_button,*pm_tare_button;
static lv_obj_t *pm_polarity_button,*pm_persist_button;
static lv_obj_t *pm_cal_button,*pm_back_button;
static lv_chart_series_t *series;
static uint32_t rendered,chart_update;
static uint32_t displayed_revision;
static bool area_initialized;
static const char *ui_notice;
static uint32_t notice_started;
static void notice(const char *value) {
    ui_notice=value;notice_started=HAL_GetTick();lv_label_set_text(message_label,value);
}
static bool transmit(const uint8_t *p,uint16_t n) { return CombinedUi_Ready() && HAL_UART_Transmit(&huart1,(uint8_t *)p,n,20)==HAL_OK; }
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
    if(target==capacity_input && lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED && !PressureClient_WizardActive()) {
        char *end;float capacity=strtof(lv_textarea_get_text(capacity_input),&end);
        if(!*end && isfinite(capacity) && capacity>=5 && capacity<=100000) {
            char b[24];snprintf(b,sizeof(b),"%.3f",(double)(capacity*0.005f));lv_textarea_set_text(tolerance_input,b);
        }
    }
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
    const PressureClientState *s=PressureClient_Status();const PcWizard *w=&s->wizard;char b[64];
    if(displayed_revision==w->revision) return;
    displayed_revision=w->revision;
    unsigned count=w->phase==PC_W_OFF?0:s->point_count;
    lv_table_set_row_cnt(point_table,(uint16_t)(count+w->checks+1));
    for(unsigned i=0;i<count+w->checks;i++) {
        bool cal=i<count;unsigned k=cal?i:i-count;
        const PcCheck *p=cal?w->calibration+k:w->verification+k;
        snprintf(b,sizeof(b),"%s%u",cal?"标":k<=5?"升":"降",(unsigned)lroundf(p->target/w->capacity*100));
        lv_table_set_cell_value(point_table,i+1,0,b);
        snprintf(b,sizeof(b),"%.1f",(double)p->target);lv_table_set_cell_value(point_table,i+1,1,b);
        if(isfinite(p->before)) snprintf(b,sizeof(b),"%+.2f",(double)p->before);else snprintf(b,sizeof(b),"--");
        lv_table_set_cell_value(point_table,i+1,2,b);
        if(isfinite(p->after)) snprintf(b,sizeof(b),"%+.2f",(double)p->after);else snprintf(b,sizeof(b),"--");
        lv_table_set_cell_value(point_table,i+1,3,b);
    }
}
enum { UI_CLEAR=1,UI_TARE,UI_CAL,UI_BACK,UI_NEXT,UI_RETRY,UI_CANCEL,UI_POLARITY,UI_PERSIST,UI_HOME };
static void action(lv_event_t *e) {
    unsigned id=(unsigned)(uintptr_t)lv_event_get_user_data(e);
    ui_notice=NULL;
    switch(id) {
    case UI_HOME: CombinedUi_ExitPressure();break;
    case UI_POLARITY: (void)PressureClient_Command(PM_POLARITY,0);break;
    case UI_PERSIST: (void)PressureClient_Command(PM_SAVE,0);break;
    case UI_CLEAR: (void)PressureClient_Command(PM_CLEAR,0);break;
    case UI_TARE: (void)PressureClient_Command(PM_TARE,0);break;
    case UI_CAL: lv_obj_add_flag(live,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(calibration_page,LV_OBJ_FLAG_HIDDEN);break;
    case UI_BACK:
        lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);lv_keyboard_set_textarea(keyboard,NULL);
        lv_obj_add_flag(calibration_page,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(live,LV_OBJ_FLAG_HIDDEN);break;
    case UI_NEXT:
        if(!PressureClient_WizardActive()) {
            float capacity,area,tolerance;
            if(!numeric(capacity_input,5,100000,&capacity) || !numeric(area_input,1,100000,&area) ||
               !numeric(tolerance_input,0.001f,10000,&tolerance)) { notice("请检查量程、面积和允许误差");return; }
            (void)PressureClient_WizardStart(capacity,area,tolerance);
        } else (void)PressureClient_WizardNext();break;
    case UI_RETRY: (void)PressureClient_WizardRetry();break;
    case UI_CANCEL: (void)PressureClient_WizardCancel();break;
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
static void create_reference_table(lv_obj_t *parent) {
    /* Fixed transcription supplied by the user. It is not recomputed from the
     * editable effective area and never changes calibration or peak capture. */
    static const char *rows[][4]={
        {"档位mmHg","-50 / N","中心 / N","+50 / N"},
        {"150","1.14","1.70","2.27"},
        {"250","2.27","2.83","3.41"},
        {"350","3.41","3.96","4.53"},
        {"450","4.53","5.09","5.66"},
        {"550","5.66","6.22","6.79"}
    };
    text(parent,"档位及左右50 mmHg参考力值（600为推算）",18,334,444,24,&pressure_font16);
    reference_table=lv_table_create(parent);lv_obj_set_pos(reference_table,18,358);
    lv_obj_set_size(reference_table,444,124);
    lv_obj_clear_flag(reference_table,LV_OBJ_FLAG_SCROLLABLE);
    lv_table_set_col_cnt(reference_table,4);lv_table_set_row_cnt(reference_table,6);
    for(unsigned col=0;col<4;++col)lv_table_set_col_width(reference_table,col,110);
    lv_obj_set_style_pad_all(reference_table,0,0);lv_obj_set_style_border_width(reference_table,1,0);
    lv_obj_set_style_text_font(reference_table,&pressure_font16,LV_PART_ITEMS);
    lv_obj_set_style_pad_ver(reference_table,0,LV_PART_ITEMS);lv_obj_set_style_pad_hor(reference_table,6,LV_PART_ITEMS);
    lv_obj_set_style_border_width(reference_table,0,LV_PART_ITEMS);
    lv_obj_set_style_bg_color(reference_table,lv_color_hex(0x182B42),0);
    lv_obj_set_style_bg_color(reference_table,lv_color_hex(0x182B42),LV_PART_ITEMS);
    lv_obj_set_style_text_color(reference_table,lv_color_hex(0xDAE9F9),LV_PART_ITEMS);
    for(unsigned i=0;i<6;++i)for(unsigned col=0;col<4;++col)
        lv_table_set_cell_value(reference_table,i,col,rows[i][col]);
}
void PressureUi_Init(void) {
    PressureClient_Init(&port);FixtureBuzzer_Stop();
    rendered=HAL_GetTick()-100;chart_update=0;area_initialized=false;displayed_revision=UINT32_MAX;ui_notice=NULL;
    lv_obj_t *root=lv_scr_act();lv_obj_clear_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root,lv_color_hex(0x101C2D),0);lv_obj_set_style_text_color(root,lv_color_hex(0xE9F3FF),0);
    lv_obj_set_style_text_font(root,&pressure_font20,0);live=panel(root);calibration_page=panel(root);
    lv_obj_add_flag(calibration_page,LV_OBJ_FLAG_HIDDEN);
    text(live,"压力传感器检测",18,4,270,30,&pressure_font20);
    lv_obj_t *home_button=button(live,"主界面",342,0,120,UI_HOME);lv_obj_set_height(home_button,32);
    connection_label=text(live,"正在连接G0下位机",18,35,444,22,&pressure_font16);
    power_label=text(live,"供电未知  电量未知",18,57,444,22,&pressure_font16);
    text(live,"实时压力 mmHg",18,82,210,26,&pressure_font18);
    value_label=text(live,"--.-",18,108,210,58,&pressure_font48);
    lv_obj_set_style_text_color(value_label,lv_color_hex(0x68C8FF),0);
    load_label=text(live,"---.--- N\n---.- g",18,166,210,76,&pressure_font28);
    lv_obj_set_style_text_color(load_label,lv_color_hex(0x68C8FF),0);
    raw_label=text(live,"瞬时 --.-  ADC --",18,0,264,26,&pressure_font16);
    lv_obj_add_flag(raw_label,LV_OBJ_FLAG_HIDDEN);
    chart=lv_chart_create(live);lv_obj_set_pos(chart,18,246);lv_obj_set_size(chart,444,86);
    lv_obj_set_style_bg_color(chart,lv_color_hex(0x182B42),0);lv_obj_set_style_line_color(chart,lv_color_hex(0x30455F),LV_PART_MAIN);
    lv_obj_set_style_pad_all(chart,1,0);lv_obj_set_style_size(chart,0,LV_PART_INDICATOR);
    lv_chart_set_type(chart,LV_CHART_TYPE_LINE);lv_chart_set_range(chart,LV_CHART_AXIS_PRIMARY_Y,-20,500);
    lv_chart_set_point_count(chart,200);series=lv_chart_add_series(chart,lv_color_hex(0x68C8FF),LV_CHART_AXIS_PRIMARY_Y);
    text(live,"峰值压力 mmHg",242,82,220,26,&pressure_font18);
    peak_label=text(live,"--.-",242,108,220,58,&pressure_font48);
    lv_obj_set_style_text_color(peak_label,lv_color_hex(0x74E4C9),0);
    peak_load_label=text(live,"---.--- N\n---.- g",242,166,220,76,&pressure_font28);
    lv_obj_set_style_text_color(peak_load_label,lv_color_hex(0x74E4C9),0);
    create_reference_table(live);
    peak_detail=text(live,"空载去皮后\n自动持续捕捉",0,0,168,90,&pressure_font16);
    lv_obj_add_flag(peak_detail,LV_OBJ_FLAG_HIDDEN);
    pm_clear_button=button(live,"清除峰值，重新捕捉",18,485,280,UI_CLEAR);
    pm_persist_button=button(live,"保存参数",310,485,152,UI_PERSIST);
    pm_tare_button=button(live,"空载去皮",18,540,140,UI_TARE);
    pm_polarity_button=button(live,"极性反转",170,540,140,UI_POLARITY);
    pm_cal_button=button(live,"砝码校准",322,540,140,UI_CAL);
    text(calibration_page,"引导式砝码校准",18,8,330,42,&pressure_font28);pm_back_button=button(calibration_page,"返回",358,8,104,UI_BACK);
    text(calibration_page,"使用量程 g",18,64,155,33,&pressure_font20);
    text(calibration_page,"允许误差 g",18,111,155,33,&pressure_font20);tolerance_input=input(calibration_page,"2.500",176,105,155);
    capacity_input=input(calibration_page,"500",176,58,155);
    text(calibration_page,"换算面积 mm²",18,158,155,33,&pressure_font18);area_input=input(calibration_page,"84.8",176,152,155);
    wizard_step_label=text(calibration_page,"准备校准",18,205,444,60,&pressure_font20);
    wizard_guide_label=text(calibration_page,"使用量程不得超过传感器额定载荷\n误差默认量程0.5%，可按要求修改",18,273,444,57,&pressure_font18);
    point_table=lv_table_create(calibration_page);lv_obj_set_pos(point_table,18,334);lv_obj_set_size(point_table,444,139);
    lv_table_set_col_cnt(point_table,4);lv_table_set_col_width(point_table,0,82);lv_table_set_col_width(point_table,1,110);lv_table_set_col_width(point_table,2,116);lv_table_set_col_width(point_table,3,116);
    lv_obj_set_style_text_font(point_table,&pressure_font18,0);lv_obj_set_style_text_font(point_table,&pressure_font18,LV_PART_ITEMS);
    lv_obj_set_style_bg_color(point_table,lv_color_hex(0x182B42),LV_PART_ITEMS);lv_obj_set_style_bg_color(point_table,lv_color_hex(0x182B42),0);
    lv_obj_set_style_text_color(point_table,lv_color_hex(0xE9F3FF),LV_PART_ITEMS);
    lv_obj_set_style_pad_ver(point_table,1,LV_PART_ITEMS);lv_obj_set_style_pad_hor(point_table,8,LV_PART_ITEMS);
    lv_table_set_cell_value(point_table,0,0,"阶段%");lv_table_set_cell_value(point_table,0,1,"标准g");
    lv_table_set_cell_value(point_table,0,2,"前差g");lv_table_set_cell_value(point_table,0,3,"后差g");
    fit_label=text(calibration_page,"校准六点，复核三轮\n示值、重复性、迟滞、回零均需通过",18,482,444,56,&pressure_font18);
    pm_next_button=button(calibration_page,"开始引导",18,543,210,UI_NEXT);
    pm_retry_button=button(calibration_page,"重测",242,543,100,UI_RETRY);
    pm_cancel_button=button(calibration_page,"结束",354,543,108,UI_CANCEL);
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
    bool linked=CombinedUi_Ready() && s->connected && s->config_valid,free=linked && !s->pending;
    const PcWizard *w=&s->wizard;bool wizard_active=PressureClient_WizardActive();
    const char *quality=!(s->flags&PM_VALID)?"传感器无有效数据":!(s->flags&PM_ZEROED)?"请先空载去皮":
        s->flags&PM_CALIBRATED?(s->config.dirty?"RAM校准未保存":"已校准·EEPROM已保存"):"未校准，当前为估算值";
    snprintf(b,sizeof(b),"%s  %s",linked?"G0已连接":"G0未连接",linked?quality:"");lv_label_set_text(connection_label,b);
    const char *supply=!(s->flags&PM_POWER_VALID)?"供电未知":s->flags&PM_FULL?"已充满":s->flags&PM_CHARGING?"外部供电/充电":"电池供电";
    if(s->soc<=100 && linked) snprintf(b,sizeof(b),"%s  %u%%  %umV",supply,(unsigned)s->soc,(unsigned)s->mv);
    else snprintf(b,sizeof(b),"%s  电量未知",linked?supply:"供电未知");
    lv_label_set_text(power_label,b);
    if(linked && isfinite(s->filtered)) snprintf(b,sizeof(b),"%.1f",(double)s->filtered);else snprintf(b,sizeof(b),"--.-");lv_label_set_text(value_label,b);
    float force=PressureClient_Force(s->filtered,s->sample_area),grams=PressureClient_Grams(s->filtered,s->sample_area);
    if(linked && isfinite(force)) snprintf(b,sizeof(b),"%.3f N\n%.1f g",(double)force,(double)grams);
    else snprintf(b,sizeof(b),"---.--- N\n---.- g");
    lv_label_set_text(load_label,b);
    if(linked && isfinite(s->pressure)) snprintf(b,sizeof(b),"瞬时 %.1f  ADC %ld",(double)s->pressure,(long)s->raw);
    else snprintf(b,sizeof(b),"瞬时 --.-  ADC %s",linked && (s->flags&PM_VALID)?"有数据，待去皮":"--");
    lv_label_set_text(raw_label,b);
    if(isfinite(s->peak)) snprintf(b,sizeof(b),"%.1f",(double)s->peak);else snprintf(b,sizeof(b),"--.-");lv_label_set_text(peak_label,b);
    force=PressureClient_Force(s->peak,s->sample_area);grams=PressureClient_Grams(s->peak,s->sample_area);
    if(isfinite(force)) snprintf(b,sizeof(b),"%.3f N\n%.1f g",(double)force,(double)grams);
    else snprintf(b,sizeof(b),"---.--- N\n---.- g");
    lv_label_set_text(peak_load_label,b);
    snprintf(b,sizeof(b),"%s\n%lu样本\n峰时 %.2fs\n异常 %lu",!linked?"断线·G0仍捕捉":s->peak_state==PM_RUNNING?"持续捕捉":"等待去皮/校准",
        (unsigned long)s->samples,(double)s->peak_at/1000,(unsigned long)s->errors);lv_label_set_text(peak_detail,b);
    if(chart_update!=s->updates) {
        chart_update=s->updates;float p=s->pressure;
        if(isfinite(p) && fabsf(p)<30000) { lv_chart_set_range(chart,LV_CHART_AXIS_PRIMARY_Y,(lv_coord_t)fminf(-20,p-10),(lv_coord_t)fmaxf(500,p+50));lv_chart_set_next_value(chart,series,(lv_coord_t)lroundf(p)); }
        else lv_chart_set_next_value(chart,series,LV_CHART_POINT_NONE);
    }
    enable(pm_clear_button,free && (s->flags&PM_ZEROED));
    enable(pm_tare_button,free && !wizard_active && (s->flags&PM_VALID));
    enable(pm_polarity_button,free && !wizard_active);
    enable(pm_persist_button,free && !wizard_active && s->config.flags==3 && s->config.dirty);
    enable(area_input,!s->pending && !wizard_active);enable(capacity_input,!s->pending && !wizard_active);
    enable(tolerance_input,!s->pending && !wizard_active);
    enable(pm_next_button,free && (s->flags&PM_VALID) && (w->phase!=PC_W_RESULT || w->passed));
    enable(pm_retry_button,free && w->phase==PC_W_RESULT);enable(pm_cancel_button,!s->pending && w->phase!=PC_W_OFF);
    if(!area_initialized && s->config_valid) { snprintf(b,sizeof(b),"%.3f",(double)s->config.area);lv_textarea_set_text(area_input,b);area_initialized=true; }
    const char *next="开始引导",*guide="使用量程不得超过传感器额定载荷\n误差默认量程0.5%，可按要求修改";
    if(w->phase==PC_W_PRELOAD) {
        snprintf(b,sizeof(b),"步骤1  预加载\n在额定范围内加载卸载三次");next="预加载已完成";
        guide="固定仪器，保证轴向受力\n装好加载盘，勿超过额定载荷";
    } else if(w->phase==PC_W_LOAD || w->phase==PC_W_VERIFY) {
        snprintf(b,sizeof(b),w->phase==PC_W_LOAD?"步骤2  标定 %u/6\n请放置 %.1f g  / %.3f N":"步骤4  复核第%u/3轮\n请放置 %.1f g  / %.3f N",
            w->phase==PC_W_LOAD?(unsigned)w->step+1:(unsigned)w->repeat+1,(double)w->target,(double)(w->target*0.00980665f));
        next=s->pending==PM_POINT?"采样中，请静置":"稳定后采集";
        guide=w->target==0?"保留加载盘，移除全部砝码\n空载采样1秒，使用实时值":w->phase==PC_W_LOAD?"按提示逐级加载，静置后再采集\n每次采样1秒，不使用冲击峰值":w->step<=5?"复核：逐级加载，静置后采集\n每轮升到满量程，再逐级卸载":"复核：逐级卸载，静置后采集\n请勿去皮，保留真实回零误差";
    } else if(w->phase==PC_W_FIT) {
        snprintf(b,sizeof(b),"步骤3  六点计算完成\n允许误差 ±%.3f g",(double)w->tolerance);next="应用并复核";
        guide="应用后仅改变RAM参数\n随后三轮加载卸载，合格才能保存";
    } else if(w->phase==PC_W_RESULT || w->phase==PC_W_SAVED) {
        snprintf(b,sizeof(b),"%s\n允许误差 ±%.3f g",w->phase==PC_W_SAVED?"复核通过，参数已保存":w->passed?"复核通过，等待保存":"复核未通过，禁止保存",(double)w->tolerance);
        next=w->phase==PC_W_SAVED?"重新校准":"保存参数";
        guide=w->phase==PC_W_SAVED?"已写入外部EEPROM双备份\n断电和重新刷写固件均保留":w->passed?"三轮复核通过，点击保存参数\n写入EEPROM后断电和刷写均保留":"请检查偏载、摩擦、迟滞及稳定性\n可重测复核，或结束后重新校准";
    } else if(w->phase==PC_W_INTERRUPTED) {
        snprintf(b,sizeof(b),"校准已中断\n请读取状态并重新开始");next="重新校准";
        guide="断线或参数变化后不能继续旧流程\n已应用的RAM参数不会自动撤销";
    } else snprintf(b,sizeof(b),"准备校准  %s\n先确认量程及允许误差",s->config_valid?(s->config.dirty?"RAM未保存":"参数已保存"):"未连接");
    lv_label_set_text(wizard_step_label,b);lv_label_set_text(wizard_guide_label,guide);
    lv_label_set_text(lv_obj_get_child(pm_next_button,0),next);
    if(w->phase>=PC_W_VERIFY && w->phase!=PC_W_INTERRUPTED)
        snprintf(b,sizeof(b),"最大差 %.2fg  重复性 %.2fg\n迟滞 %.2fg  回零 %.2fg",(double)w->max_error,(double)w->repeatability,(double)w->hysteresis,(double)w->zero_error);
    else if(w->phase==PC_W_FIT)
        snprintf(b,sizeof(b),"六点线性拟合，未写入设备\n前差/后差均为测量值减标准值");
    else snprintf(b,sizeof(b),"标定六点，独立复核三轮\n表格可滚动；面积仅用于mmHg换算");
    lv_label_set_text(fit_label,b);
    show_points();lv_label_set_text(message_label,ui_notice && now-notice_started<4000U?ui_notice:s->message);
}
bool PressureUi_Leave(void) {
    if(PressureClient_Status()->pending) { notice("请等待当前操作完成再返回");return false; }
    if(PressureClient_WizardActive()) (void)PressureClient_WizardCancel();
    lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);lv_keyboard_set_textarea(keyboard,NULL);
    lv_obj_add_flag(calibration_page,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(live,LV_OBJ_FLAG_HIDDEN);
    return true;
}
