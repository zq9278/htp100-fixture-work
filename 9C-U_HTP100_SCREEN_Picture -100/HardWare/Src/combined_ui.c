#include "combined_ui.h"
#include "combined_wire.h"
#include "fixture_ui.h"
#include "pressure_ui.h"
#include "lvgl.h"
#include "main.h"
#include "usart.h"
LV_FONT_DECLARE(pressure_font18);
static lv_obj_t *fixture_screen,*pressure_screen,*handover,*handover_text;
static CombinedParser wire;
static bool switching;
static uint8_t selected;
static uint16_t selection_sequence;
static uint32_t last_selection,selection_started;
static void send_selection(void) {
    uint8_t f[FX_FRAME_SIZE];Fx_Frame(f,FX_SELECT,selection_sequence,selected);Fx_Seal(f);
    (void)HAL_UART_Transmit(&huart1,f,sizeof(f),20);last_selection=HAL_GetTick();
}
static void select_mode(uint8_t mode) {
    if(switching) return;
    selected=mode;selection_sequence++;switching=true;selection_started=HAL_GetTick();
    lv_label_set_text(handover_text,"正在切换功能\n等待G0关闭加热并确认");
    lv_obj_clear_flag(handover,LV_OBJ_FLAG_HIDDEN);send_selection();
}
void CombinedUi_OpenPressure(void) { select_mode(FX_PRESSURE); }
void CombinedUi_ExitPressure(void) { if(PressureUi_Leave()) select_mode(FX_NONE); }
static void receive(bool pressure,const uint8_t *f,size_t n) {
    if(pressure) PressureUi_Feed(f,n);
    else {
        if(switching && f[3]==FX_STATUS && Fx_Get16(f+4)==selection_sequence &&
           f[6]==selected && f[7]==FX_IDLE && f[8]==FX_OK) {
            switching=false;lv_obj_add_flag(handover,LV_OBJ_FLAG_HIDDEN);
            lv_scr_load(selected==FX_PRESSURE?pressure_screen:fixture_screen);
        }
        FixtureUi_Feed(f,n);
    }
}
void CombinedUi_Init(void) {
    wire.used=0;switching=false;selection_sequence=(uint16_t)(HAL_GetTick()^0xA500);
    fixture_screen=lv_obj_create(NULL);lv_scr_load(fixture_screen);FixtureUi_Init();
    pressure_screen=lv_obj_create(NULL);lv_scr_load(pressure_screen);PressureUi_Init();
    lv_scr_load(fixture_screen);
    handover=lv_obj_create(lv_layer_top());lv_obj_set_size(handover,480,640);lv_obj_center(handover);
    lv_obj_set_style_bg_color(handover,lv_color_hex(0x101C2D),0);lv_obj_set_style_bg_opa(handover,LV_OPA_COVER,0);
    handover_text=lv_label_create(handover);lv_obj_set_width(handover_text,430);
    lv_obj_set_style_text_font(handover_text,&pressure_font18,0);
    lv_obj_set_style_text_color(handover_text,lv_color_hex(0xFFFFFF),0);lv_obj_center(handover_text);
    lv_obj_add_flag(handover,LV_OBJ_FLAG_HIDDEN);
    select_mode(FX_NONE); /* Screen reboot disarms an earlier fixture transaction. */
}
void CombinedUi_Feed(const uint8_t *p,size_t n) { Combined_Feed(&wire,p,n,receive); }
void CombinedUi_Tick(void) {
    FixtureUi_Tick();PressureUi_Tick();
    if(switching && HAL_GetTick()-last_selection>=250U) {
        if(HAL_GetTick()-selection_started>=4000U)
            lv_label_set_text(handover_text,"G0未确认，功能尚未切换\n请检查两板连接和固件\n恢复连接后自动重试");
        send_selection();
    }
}
