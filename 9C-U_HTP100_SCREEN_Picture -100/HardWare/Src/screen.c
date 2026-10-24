#include "screen.h"
#include "ui.h"
#include "ui-myself.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
uint8_t EYE_status;
extern uint8_t work_mode;
extern TIM_HandleTypeDef htim14;
// char buffer[64];  // 假�?�数字加上百分号不会超过 64 �??字�??
extern lv_obj_t *ui_Label30; // 倒�?�时显示的标�??
extern lv_obj_t *ui_Label32; // 倒�?�时显示的标�??
extern lv_obj_t *ui_Label39; // 倒�?�时显示的标�??
extern lv_obj_t *ui_Label46; // 倒�?�时显示的标�??
extern lv_obj_t *ui_Slider1; // 电池电量
// extern lv_obj_t *ui_Label58; // 倒�?�时显示的标�??
extern lv_obj_t *ui_Slider2; // 电池电量
extern lv_obj_t *ui_Slider3; // 电池电量
extern lv_obj_t *ui_Slider9; // 电池电量
extern lv_obj_t *ui_Slider5; // 电池电量
extern lv_obj_t *ui_Slider6; // 电池电量
extern lv_obj_t *ui_Slider7; // 电池电量
extern lv_obj_t *ui_Slider8; // 电池电量
extern lv_obj_t *ui_Image53;
extern lv_obj_t *ui_Image56;
extern lv_obj_t *ui_Image57;
extern lv_obj_t *ui_Label5; // 电池电量数字
extern lv_obj_t *ui_Container10;
extern lv_obj_t *ui_Container4;
extern lv_obj_t *ui_Container7;

extern lv_obj_t *ui_Label2; // �??敷�?�数
extern lv_obj_t *ui_Label3; // 脉动次数
extern lv_obj_t *ui_Label4; // �??�?�??�数
extern lv_obj_t *ui_Label6; // 设置压力�??
extern lv_obj_t *ui_Label8; // 设置时间�??
extern lv_obj_t *ui_Image100;
extern lv_obj_t *ui_Image99;
extern lv_obj_t *ui_Image113;
extern lv_obj_t *ui_Image95;
extern lv_obj_t *ui_TextArea42;

uint16_t prepare_temprature, prepare_crimp, prepare_time, bee,
    prepare_set; // 准�?�温度，准�?�挤压�?�数，准备时间，蜂鸣�??，准备�?�置123
uint8_t work_finish = 0; // 0 没结束，1 工作完成
extern uint8_t usart1_tx;
extern UART_HandleTypeDef huart1;

static int16_t hot_temperature_tenths = 0;
static int16_t auto_temperature_tenths = 0;
static uint16_t pulsation_pressure = 0;
static uint16_t auto_pressure = 0;
static uint8_t battery_percent = 0;
static uint8_t battery_percent_valid = 0;
static uint32_t software_version = 20241226UL;
static uint8_t screen_start_ack = 0;
static volatile uint8_t startup_language_received = 0U;
static volatile uint8_t startup_language_preload_enabled = 1U;

#define SCREEN_FAULT_STATUS_ID       0x2F01U
#define SCREEN_FAULT_REPORT_TIMEOUT  5000U
#define SCREEN_HEARTBEAT_TIMEOUT     2000U
#define SCREEN_COMMUNICATION_FAULT   0x0501U
#define SCREEN_TREATMENT_ACK_ID       0x00AEU
#define SCREEN_TREATMENT_ACK_TIMEOUT  200U
#define SCREEN_TREATMENT_ACK_SLOTS    4U
#define SCREEN_PROTOCOL_MAX_FRAME     13U

LV_FONT_DECLARE(ui_font_popup40);

typedef enum {
  SCREEN_POPUP_NONE = 0,
  SCREEN_POPUP_EYE_OFFLINE,
  SCREEN_POPUP_FAULT,
  SCREEN_POPUP_HOST_LOST
} screen_popup_kind_t;

static uint32_t active_fault_code = 0U;
static uint32_t last_fault_report_ms = 0U;
static uint32_t last_battery_heartbeat_ms = 0U;
static uint8_t heartbeat_timer_started = 0U;
static uint8_t eye_state_received = 0U;
static screen_popup_kind_t displayed_popup = SCREEN_POPUP_NONE;
static uint32_t displayed_fault_code = 0U;
static uint8_t displayed_popup_language = 0xFFU;

typedef struct {
  uint8_t active;
  uint8_t retry_count;
  uint16_t command;
  uint16_t frame_size;
  uint32_t sent_at_ms;
  uint8_t frame[SCREEN_PROTOCOL_MAX_FRAME];
} screen_treatment_pending_t;

static screen_treatment_pending_t
    treatment_pending[SCREEN_TREATMENT_ACK_SLOTS];
static uint32_t treatment_ack_timeout_count;
static uint32_t treatment_ack_queue_full_count;

static HAL_StatusTypeDef uart_transmit_raw(UART_HandleTypeDef *huart,
                                           uint8_t *data, uint16_t size);

static uint8_t treatment_command_requires_ack(uint16_t command)
{
  switch (command) {
  case 0x1041U: /* prepare heating */
  case 0x1005U: /* prepare pressure */
  case 0x1037U: /* prepare automatic */
  case 0x1040U: /* start heating */
  case 0x1006U: /* start pressure */
  case 0x1036U: /* start automatic */
  case 0x1030U: /* stop heating */
  case 0x1034U: /* stop pressure */
  case 0x1038U: /* stop automatic */
  case 0x8900U: /* natural treatment finish */
    return 1U;
  default:
    return 0U;
  }
}

static void treatment_ack_arm(uint16_t command, const uint8_t *frame,
                              uint16_t frame_size)
{
  screen_treatment_pending_t *slot = NULL;

  if (frame == NULL || frame_size > SCREEN_PROTOCOL_MAX_FRAME) {
    return;
  }
  for (uint8_t index = 0U; index < SCREEN_TREATMENT_ACK_SLOTS; ++index) {
    if (treatment_pending[index].active &&
        treatment_pending[index].command == command) {
      slot = &treatment_pending[index];
      break;
    }
    if (slot == NULL && !treatment_pending[index].active) {
      slot = &treatment_pending[index];
    }
  }
  if (slot == NULL) {
    treatment_ack_queue_full_count++;
    return;
  }

  memcpy(slot->frame, frame, frame_size);
  slot->active = 1U;
  slot->retry_count = 0U;
  slot->command = command;
  slot->frame_size = frame_size;
  slot->sent_at_ms = HAL_GetTick();
}

static void treatment_ack_received(uint16_t command)
{
  for (uint8_t index = 0U; index < SCREEN_TREATMENT_ACK_SLOTS; ++index) {
    if (treatment_pending[index].active &&
        treatment_pending[index].command == command) {
      treatment_pending[index].active = 0U;
      return;
    }
  }
}

void screen_treatment_ack_tick(void)
{
  uint32_t now = HAL_GetTick();

  for (uint8_t index = 0U; index < SCREEN_TREATMENT_ACK_SLOTS; ++index) {
    screen_treatment_pending_t *slot = &treatment_pending[index];

    if (!slot->active ||
        now - slot->sent_at_ms < SCREEN_TREATMENT_ACK_TIMEOUT) {
      continue;
    }
    if (slot->retry_count == 0U) {
      (void)uart_transmit_raw(&huart1, slot->frame, slot->frame_size);
      slot->retry_count = 1U;
      slot->sent_at_ms = now;
    } else {
      slot->active = 0U;
      treatment_ack_timeout_count++;
    }
  }
}

void screen_preload_language_from_uart(const uint8_t *data, uint16_t size) {
  const uint16_t frame_size = (uint16_t)sizeof(prepare_data);

  if (!startup_language_preload_enabled || data == NULL || size < frame_size) {
    return;
  }

  for (uint16_t i = 0U; i + frame_size <= size; ++i) {
    const uint8_t *frame = &data[i];
    uint16_t received_crc;
    uint16_t language_value;

    if (frame[0] != 0x6AU || frame[1] != 0xA6U ||
        frame[2] != frame_size || frame[3] != 0x00U || frame[4] != 0xABU ||
        frame[frame_size - 2U] != 0xFFU || frame[frame_size - 1U] != 0xFFU) {
      continue;
    }

    received_crc = (uint16_t)frame[frame_size - 4U] |
                   (uint16_t)((uint16_t)frame[frame_size - 3U] << 8U);
    if (Calculate_CRC((uint8_t *)frame, frame_size - 4U) != received_crc) {
      continue;
    }

    language_value = (uint16_t)frame[5] | (uint16_t)((uint16_t)frame[6] << 8U);
    if (language_value <= UI_LANG_EN) {
      ui_lang_preselect(language_value);
      startup_language_received = 1U;
    }
  }
}

void screen_finish_startup_language_preload(void) {
  startup_language_preload_enabled = 0U;
}

void screen_fault_popup_tick(void) {
  uint32_t now = HAL_GetTick();
  uint8_t language = (uint8_t)ui_lang_get();
  uint8_t host_lost;
  screen_popup_kind_t wanted;
  char text[64];

  if (!heartbeat_timer_started) {
    heartbeat_timer_started = 1U;
    last_battery_heartbeat_ms = now;
  }

  if (active_fault_code != 0U &&
      now - last_fault_report_ms >= SCREEN_FAULT_REPORT_TIMEOUT) {
    active_fault_code = 0U;
  }

  host_lost = (now - last_battery_heartbeat_ms >= SCREEN_HEARTBEAT_TIMEOUT);
  if (eye_state_received && EYE_status == 0U) {
    wanted = SCREEN_POPUP_EYE_OFFLINE;
  } else if (active_fault_code == SCREEN_COMMUNICATION_FAULT) {
    /* The controller's screen-timeout fault and the local heartbeat timeout
     * describe the same problem. Present one consistent popup. */
    wanted = SCREEN_POPUP_HOST_LOST;
  } else if (active_fault_code != 0U) {
    wanted = SCREEN_POPUP_FAULT;
  } else if (host_lost) {
    wanted = SCREEN_POPUP_HOST_LOST;
  } else {
    wanted = SCREEN_POPUP_NONE;
  }

  if (wanted == SCREEN_POPUP_NONE) {
    if (global_panel != NULL &&
        !lv_obj_has_flag(global_panel, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_add_flag(global_panel, LV_OBJ_FLAG_HIDDEN);
    }
    displayed_popup = SCREEN_POPUP_NONE;
    return;
  }

  create_global_panel();
  if (global_panel == NULL) {
    return;
  }

  if (wanted != displayed_popup || language != displayed_popup_language ||
      (wanted == SCREEN_POPUP_FAULT && active_fault_code != displayed_fault_code)) {
    if (wanted == SCREEN_POPUP_EYE_OFFLINE) {
      global_panel_set_content(ui_lang_get() == UI_LANG_EN ?
                               "Shield invalid\nReplace shield" :
                               "检测眼盾无效\n请更换眼盾",
                               ui_lang_get() == UI_LANG_EN ?
                               "Return to home" : "返回首页");
    } else if (wanted == SCREEN_POPUP_FAULT) {
      if (ui_lang_get() == UI_LANG_EN) {
        snprintf(text, sizeof(text), "Fault code: 0x%04lX",
                 (unsigned long)active_fault_code);
      } else {
        snprintf(text, sizeof(text), "故障码: 0x%04lX",
                 (unsigned long)active_fault_code);
      }
      global_panel_set_content(text, ui_lang_get() == UI_LANG_EN ?
                               "Return to home" : "返回首页");
    } else {
      global_panel_set_content(ui_lang_get() == UI_LANG_EN ?
                               "Communication failed" : "通讯失败",
                               ui_lang_get() == UI_LANG_EN ?
                               "Return to home" : "返回首页");
    }
    displayed_popup = wanted;
    displayed_fault_code = active_fault_code;
    displayed_popup_language = language;
  }

  /* The Return Home button may hide the panel. Reassert it while the
   * corresponding condition is still active. */
  if (lv_obj_has_flag(global_panel, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_clear_flag(global_panel, LV_OBJ_FLAG_HIDDEN);
  }
}

typedef struct {
  lv_coord_t width;
  lv_coord_t height;
  lv_coord_t y;
  lv_coord_t pad_top;
  lv_coord_t pad_bottom;
} version_text_layout_t;

static version_text_layout_t version_text_layout(void) {
  version_text_layout_t layout;

  if (ui_lang_get() == UI_LANG_EN) {
    layout.width = 124;
    layout.height = 28;
    layout.y = 289;
    layout.pad_top = 1;
    layout.pad_bottom = 0;
  } else {
    layout.width = 124;
    layout.height = 27;
    layout.y = 282;
    layout.pad_top = 0;
    layout.pad_bottom = 0;
  }

  return layout;
}

static int16_t temp_to_tenths(float_t value) {
  float_t scaled = value * 10.0f;
  return (int16_t)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

static void set_temperature_label(lv_obj_t *label, int16_t tenths) {
  int16_t abs_value = tenths < 0 ? -tenths : tenths;
  int16_t whole = abs_value / 10;
  int16_t fraction = abs_value % 10;

  if (label == NULL) {
    return;
  }

  if (tenths < 0) {
    lv_label_set_text_fmt(label, "-%02d.%d", whole, fraction);
  } else {
    lv_label_set_text_fmt(label, "%02d.%d", whole, fraction);
  }
}

static void set_pressure_label(lv_obj_t *label, uint16_t value) {
  if (label == NULL) {
    return;
  }

  lv_label_set_text_fmt(label, "%03d", (int)value);
  if (value >= 247U) {
    lv_obj_set_style_text_color(label, lv_color_make(255, 0, 0), 0);
  } else {
    lv_obj_set_style_text_color(label, lv_color_make(255, 255, 255), 0);
  }
}

static void set_version_text(void) {
  char version_text[16];
  const char *current_text;
  version_text_layout_t layout;

  if (ui_TextArea42 == NULL) {
    return;
  }

  snprintf(version_text, sizeof(version_text), "%lu",
           (unsigned long)software_version);
  current_text = lv_textarea_get_text(ui_TextArea42);
  layout = version_text_layout();

  lv_textarea_set_one_line(ui_TextArea42, true);
  lv_textarea_set_cursor_click_pos(ui_TextArea42, false);
  lv_obj_clear_flag(ui_TextArea42, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(ui_TextArea42, layout.width);
  lv_obj_set_height(ui_TextArea42, layout.height);
  lv_obj_set_y(ui_TextArea42, layout.y);
  lv_obj_set_style_pad_top(ui_TextArea42, layout.pad_top,
                           LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(ui_TextArea42, layout.pad_bottom,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_line_space(ui_TextArea42, 0,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_letter_space(ui_TextArea42, 0,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
  if (current_text != NULL && strcmp(current_text, version_text) == 0) {
    return;
  }
  lv_textarea_set_text(ui_TextArea42, version_text);
  lv_textarea_set_cursor_pos(ui_TextArea42, 0);
  lv_obj_scroll_to_x(ui_TextArea42, 0, LV_ANIM_OFF);
  lv_obj_scroll_to_y(ui_TextArea42, 0, LV_ANIM_OFF);
}

void screen_refresh_prepare_ui(void) {
  const char *target_text;
  const char *current_text;

  if (ui_TextArea29 == NULL) {
    return;
  }

  switch (prepare_set) {
  case 1:
    target_text = ui_lang_get() == UI_LANG_EN ? "Setting 1" : "设定一";
    break;
  case 2:
    target_text = ui_lang_get() == UI_LANG_EN ? "Setting 2" : "设定二";
    break;
  case 3:
    target_text = ui_lang_get() == UI_LANG_EN ? "Setting 3" : "设定三";
    break;
  default:
    target_text = ui_lang_get() == UI_LANG_EN ? "Setting 1" : "设定一";
    break;
  }

  current_text = lv_textarea_get_text(ui_TextArea29);
  if (current_text != NULL && strcmp(current_text, target_text) == 0) {
    return;
  }

  lv_textarea_set_text(ui_TextArea29, target_text);
}

void screen_refresh_runtime_ui(void) {
  screen_refresh_prepare_ui();
  set_temperature_label(ui_Label30, hot_temperature_tenths);
  set_temperature_label(ui_Label39, auto_temperature_tenths);
  set_pressure_label(ui_Label32, pulsation_pressure);
  set_pressure_label(ui_Label46, auto_pressure);
  set_version_text();

  if (!battery_percent_valid) {
    return;
  }

  if (ui_Slider1 != NULL) lv_slider_set_value(ui_Slider1, battery_percent, LV_ANIM_ON);
  if (ui_Label5 != NULL) {
    char text[8];
    snprintf(text, sizeof(text), "%u%%", (unsigned)battery_percent);
    if (strcmp(lv_label_get_text(ui_Label5), text) != 0) {
      lv_label_set_text(ui_Label5, text);
    }
  }
  if (ui_Slider2 != NULL) lv_slider_set_value(ui_Slider2, battery_percent, LV_ANIM_ON);
  if (ui_Slider3 != NULL) lv_slider_set_value(ui_Slider3, battery_percent, LV_ANIM_ON);
  if (ui_Slider9 != NULL) lv_slider_set_value(ui_Slider9, battery_percent, LV_ANIM_ON);
  if (ui_Slider5 != NULL) lv_slider_set_value(ui_Slider5, battery_percent, LV_ANIM_ON);
  if (ui_Slider6 != NULL) lv_slider_set_value(ui_Slider6, battery_percent, LV_ANIM_ON);
  if (ui_Slider7 != NULL) lv_slider_set_value(ui_Slider7, battery_percent, LV_ANIM_ON);
  if (ui_Slider8 != NULL) lv_slider_set_value(ui_Slider8, battery_percent, LV_ANIM_ON);
}

void screen_reset_hotting_runtime_ui(void) {
  hot_temperature_tenths = 0;
  set_temperature_label(ui_Label30, hot_temperature_tenths);
}

void screen_reset_pulsationning_runtime_ui(void) {
  pulsation_pressure = 0;
  set_pressure_label(ui_Label32, pulsation_pressure);
}

void screen_reset_autoing_runtime_ui(void) {
  auto_temperature_tenths = 0;
  auto_pressure = 0;
  set_temperature_label(ui_Label39, auto_temperature_tenths);
  set_pressure_label(ui_Label46, auto_pressure);
}

void work_flinsh(void) {
  if (work_finish) {
	work_finish=0;
    HAL_TIM_Base_Stop_IT(&htim14);
    if (work_mode != 0) {
      static AXI_SRAM CTRL_MSG pData_time_finish;
      pData_time_finish.cmd_head_high = 0x5A;
      pData_time_finish.cmd_head_low = 0xA5;
      pData_time_finish.frame_length = 0x0d;
      pData_time_finish.cmd_type_high = 0x89;
      pData_time_finish.cmd_type_low = 0x00;
      pData_time_finish.data = 1.0;
      pData_time_finish.crc = Calculate_CRC((uint8_t *)&pData_time_finish,
                                            sizeof(pData_time_finish) - 4);
      pData_time_finish.end_high = 0xFF;
      pData_time_finish.end_low = 0xFF;
      UART_TransmitWithRetry(&huart1, (uint8_t *)&pData_time_finish,
                             sizeof(pData_time_finish));
      static AXI_SRAM CTRL_MSG pData;
      pData.cmd_head_high = 0x5A;
      pData.cmd_head_low = 0xA5;
      pData.frame_length = 0x0d;
      pData.cmd_type_high = 0x10;
      // 根据工作模式发送不同的命令
      switch (work_mode) {
      case 1:
        lv_obj_clear_flag(ui_Image53,
                          LV_OBJ_FLAG_HIDDEN); // 显示工作模式1相关UI元素
        lv_obj_clear_flag(ui_Container3, LV_OBJ_FLAG_HIDDEN);
        pData.cmd_type_low = 0x30;
        break;
      case 2:
        lv_obj_clear_flag(ui_Image56,
                          LV_OBJ_FLAG_HIDDEN); // 显示工作模式2相关UI元素
        lv_obj_clear_flag(ui_Container5, LV_OBJ_FLAG_HIDDEN);
        pData.cmd_type_low = 0x34;
        break;
      case 3:
        lv_obj_clear_flag(ui_Image57,
                          LV_OBJ_FLAG_HIDDEN); // 显示工作模式3相关UI元素
        lv_obj_clear_flag(ui_Container8, LV_OBJ_FLAG_HIDDEN);
        pData.cmd_type_low = 0x38;
        break;
      default:
        break;
      }
      pData.crc = Calculate_CRC((uint8_t *)&pData, sizeof(pData) - 4);
      pData.end_high = 0xFF;
      pData.end_low = 0xFF;
      UART_TransmitWithRetry(&huart1, (uint8_t *)&pData, sizeof(pData));
    }
  }
}

void command_parsing(
    const uint8_t *received_data) { // 区分调试命令和屏幕工作命�??
  // �??认帧尾是否合�??
  // if (received_data->buffer[received_data->length - 2] != 0xFF ||
  //     received_data->buffer[received_data->length - 1] != 0xFF) {
  //   return; // 非法帧，直接返回
  // }
  // 提取命令类型（高字节和低字节�??
  uint16_t cmd_type =
      (received_data[0] << 8) |
      received_data[1]; // �??一位和�??二位判断�??不是调试参数
  // 解析命令
  switch (cmd_type) {
  case 0x5aa5: { // 命令类型 0x9000 - 处理 UART1 数据
    uint16_t message_type = ((uint16_t)received_data[3] << 8) | received_data[4];
    if (message_type == 0x2060U || message_type == SCREEN_FAULT_STATUS_ID) {
      processData_u32((PCTRL_MSG_U32)received_data);
    } else {
      processData((PCTRL_MSG)received_data); // process data from uart
    }
    break;
  }
  case 0x6aa6: { // 命令类型 0x6aa6 - 处理 UART1 数据
    processData_prepare(
        (prepare_data_p)received_data); // process data from uart
    HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_15);
    break;
  }
  default:
    // �??知命令，忽略
    break;
  }
}

void processData(PCTRL_MSG msg) {
  uint16_t cmd_head = ((msg->cmd_head_high) << 8) |
                      (msg->cmd_head_low); // 将两�??8位值合并成一�??16位�?
  uint16_t cmd_type = ((msg->cmd_type_high) << 8) |
                      (msg->cmd_type_low); // 将两�??8位值合并成一�??16位�?
  // uint16_t data = ((msg->data_high) << 8) | (msg->data_low); // 指令类型
  float_t data = msg->data;
  switch (cmd_type) {

  case 0x2041:                                       // 开始热�??
    hot_temperature_tenths = temp_to_tenths(data);
    screen_refresh_runtime_ui();
    break;
  case 0x2005: // 开始脉�??
    pulsation_pressure = (uint16_t)data;
    screen_refresh_runtime_ui();
    break;
  case 0x2037:                                       // �??动开始热�??
    auto_temperature_tenths = temp_to_tenths(data);
    screen_refresh_runtime_ui();
    break;
  case 0x2047: // �??动开始脉�??
    auto_pressure = (uint16_t)data;
    screen_refresh_runtime_ui();
    break;
  case 0x2050: // 跟新电池
    battery_percent = (uint8_t)data;
    battery_percent_valid = 1;
    last_battery_heartbeat_ms = HAL_GetTick();
    heartbeat_timer_started = 1U;
    screen_refresh_runtime_ui();
    break;
  case 0x2051: // 主机发送停�??            
    HAL_TIM_Base_Stop_IT(&htim14);
    switch (work_mode) {
    case 1: // �??敷状态下点击的停�??
      lv_obj_clear_flag(ui_Image53, LV_OBJ_FLAG_HIDDEN);     // 打开阴影
      lv_obj_clear_flag(ui_Container10, LV_OBJ_FLAG_HIDDEN); // 打开阴影//3
      break;
    case 2: // 脉动状态下点击的停�??
      lv_obj_clear_flag(ui_Image56, LV_OBJ_FLAG_HIDDEN);    // 打开阴影
      lv_obj_clear_flag(ui_Container4, LV_OBJ_FLAG_HIDDEN); // 打开阴影//5
      break;
    case 3: // �??动状态下点击的停�??
      lv_obj_clear_flag(ui_Image57, LV_OBJ_FLAG_HIDDEN);    // 打开阴影
      lv_obj_clear_flag(ui_Container7, LV_OBJ_FLAG_HIDDEN); // 打开阴影//8
      break;

    default:
      break;
    }
    work_mode = 0;
    break;
		case 0x2056: // 眼盾拔出主机发送停          
    HAL_TIM_Base_Stop_IT(&htim14);
		break;
		case 0x2057: // 检测新眼盾 
		{lv_obj_t * current_screen1 = lv_scr_act();
    // 这些界面不创建 global_panel
    if (!(current_screen1 == ui_home_page ) )
		{
			_ui_screen_change(&ui_home_page, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_home_page_screen_init);
    }		}
		break;
  case 0x2052: // 主机发送开�??
    HAL_TIM_Base_Start_IT(&htim14);
    switch (work_mode) {
    case 1: // �??敷状态下点击的停�??
      lv_obj_add_flag(ui_Image70,
                      LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST); /// 隐藏
      lv_obj_add_flag(ui_Image5, LV_OBJ_FLAG_CLICKABLE);
      break;
    case 2: // 脉动状态下点击的停�??
      lv_obj_add_flag(ui_Image74,
                      LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST); /// 隐藏
      lv_obj_add_flag(ui_Image22, LV_OBJ_FLAG_CLICKABLE);
      break;
    case 3: // �??动状态下点击的停�??
      lv_obj_add_flag(ui_Image75,
                      LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST); /// 隐藏
      lv_obj_add_flag(ui_Image55, LV_OBJ_FLAG_CLICKABLE);
      break;

    default:
      break;
    }

    break;
  case 0x2053: // 在充�??
    lv_obj_set_style_bg_img_recolor(ui_Slider1, lv_color_hex(0x4FCD51),
                                    LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_recolor_opa(ui_Slider1, 255,
                                        LV_PART_INDICATOR | LV_STATE_DEFAULT);
    break;
  case 0x2054: // 不在充电
               // lv_obj_set_style_bg_img_recolor(ui_Slider1,
               // lv_color_hex(0x4FCD51), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_recolor_opa(ui_Slider1, 00,
                                        LV_PART_INDICATOR | LV_STATE_DEFAULT);
    break;
  case 0x2055:
    EYE_status = (data == 1.0f) ? 1U : 0U;
    eye_state_received = 1U;
    if (EYE_status == 0U) {
      if (lv_scr_act() != ui_home_page) {
        _ui_screen_change(&ui_home_page, LV_SCR_LOAD_ANIM_NONE, 0, 0,
                          &ui_home_page_screen_init);
      }
      work_mode = 0;
    }
    screen_fault_popup_tick();
    break;
  default:
    break;
  }
}

void processData_u32(PCTRL_MSG_U32 msg) {
  uint16_t cmd_type = ((msg->cmd_type_high) << 8) | (msg->cmd_type_low);

  switch (cmd_type) {
  case 0x2060:
    software_version = msg->value;
    screen_refresh_runtime_ui();
    break;
  case SCREEN_FAULT_STATUS_ID:
    active_fault_code = msg->value;
    last_fault_report_ms = HAL_GetTick();
    screen_fault_popup_tick();
    break;
  default:
    break;
  }
}

void processData_prepare(prepare_data_p msg) {
  uint16_t cmd_head = ((msg->cmd_head_high) << 8) |
                      (msg->cmd_head_low); // 将两�??8位值合并成一�??16位�?
  uint16_t cmd_type = ((msg->cmd_type_high) << 8) |
                      (msg->cmd_type_low); // 将两�??8位值合并成一�??16位�?
  // uint16_t data = ((msg->data_high) << 8) | (msg->data_low); // 指令类型
  uint16_t data = msg->value;
  switch (cmd_type) {

  case 0x00A0:                                           // 加热次数
    lv_label_set_text_fmt(ui_Label2, "%05d", (int)data); // 显示加热次数
    break;
  case 0x00A1:                                           // 挤压次数
    lv_label_set_text_fmt(ui_Label3, "%05d", (int)data); //
    break;
  case 0x00A2:                                           // �??�?�??�数
    lv_label_set_text_fmt(ui_Label4, "%05d", (int)data); //
    break;
    case 0x00A3: // 选择预设
    prepare_set = data;
    switch (prepare_set) {
    case 0: {
      // 保持原有的面板透明度设置
      lv_obj_set_style_bg_opa(ui_Panel16, 220, LV_PART_MAIN); // 设置背景透明度
      lv_obj_set_style_bg_opa(ui_Panel17, 220, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel18, 220, LV_PART_MAIN);

      lv_obj_add_flag(ui_Image24, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST); /// Flags
      prepare_time = 1;
      prepare_crimp = 150;
    } break;
    case 1: {
      lv_obj_set_style_bg_opa(ui_Panel16, 0, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel17, 220, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel18, 220, LV_PART_MAIN);

      lv_obj_clear_flag(ui_Image24, LV_OBJ_FLAG_HIDDEN); /// Flags
      screen_refresh_prepare_ui();
    } break;
    case 2: {
      lv_obj_set_style_bg_opa(ui_Panel16, 220, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel17, 0, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel18, 220, LV_PART_MAIN);

      lv_obj_clear_flag(ui_Image24, LV_OBJ_FLAG_HIDDEN); /// Flags
      screen_refresh_prepare_ui();
    } break;
    case 3: {
      lv_obj_set_style_bg_opa(ui_Panel16, 220, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel17, 220, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel18, 0, LV_PART_MAIN);

      lv_obj_clear_flag(ui_Image24, LV_OBJ_FLAG_HIDDEN); /// Flags
      screen_refresh_prepare_ui();
    } break;
    }
    break;
  case 0x00A4: // 预�?�温�??
    prepare_temprature = data;

    break;
  case 0x00A5: // 预�?�压�??
    prepare_crimp = data;

    break;
  case 0x00A6: // 预�?�时�??
    prepare_time = data;
    break;
  case 0x00A8: // 预�?�温�??
               // prepare_temprature= data;

    break;
  case 0x00A9: // 预�?�压�??
               // prepare_crimp= data;

    lv_label_set_text_fmt(ui_Label6, "%03d", (int)data); // 显示加热温度次数
    break;
  case 0x00AA: // 预�?�时�??
    // prepare_time= data;
    lv_label_set_text_fmt(ui_Label8, "%02d", (int)data); // 显示加热温度次数
    break;
  case 0x00AC: // 型号区分
    if (data == 0U) {
      if (ui_Image24 != NULL) {
        lv_obj_add_flag(ui_Image24, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
      }
      if (ui_Panel2 != NULL) {
        lv_obj_add_flag(ui_Panel2, LV_OBJ_FLAG_HIDDEN);
      }
    }
    break;
	case 0x00b0: // 
    lv_label_set_text_fmt(ui_Label1, "%d", (int)data); // 显示加热温度次数
    break;
  case 0x00A7: // bee
    bee = data;
    HAL_GPIO_WritePin(Bee_vcc_GPIO_Port, Bee_vcc_Pin,
                      bee == 1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    ui_lang_apply_bee_state();
    break;
  case 0x00AB: // language
    /* A startup frame may already have preselected the language before
     * ui_init(). Runtime frames reach here without ISR-side UI state changes,
     * so a changed value refreshes the current page immediately. */
    if ((data <= 1U) && (ui_lang_get() != data)) {
      ui_lang_set(data);
      screen_refresh_runtime_ui();
    }
    break;
  case 0x00AD: // screen power-on ACK
    if (data == 1U) {
      screen_start_ack = 1U;
    }
    break;
  case SCREEN_TREATMENT_ACK_ID:
    treatment_ack_received(data);
    break;
  default:
    break;
  }
}

uint8_t screen_start_ack_received(void) {
  return screen_start_ack;
}

uint8_t screen_start_language_received(void) {
  return startup_language_received;
}

/* 环形队列入队操作 */
#if 0
int RingQueue_Push(RingQueue *queue, UART_RX_TypeDef *data) {
  if (queue->count >= UART_QUEUE_SIZE) {
    return -1; // 队列已满，无法入队
  }
  queue->buffer[queue->head] = data;                 // 存入队列
  queue->head = (queue->head + 1) % UART_QUEUE_SIZE; // 头部索引循环递增
  queue->count++;                                    // 更新计数
  return 0;                                          // 入队成功
}

/* 环形队列出队操作 */
UART_RX_TypeDef *RingQueue_Pop(RingQueue *queue) {
  if (queue->count == 0) {
    return NULL; // 队列为空，返回NULL
  }
  UART_RX_TypeDef *data = queue->buffer[queue->tail]; // 取出数据
  queue->tail = (queue->tail + 1) % UART_QUEUE_SIZE;  // 尾部索引循环递增
  queue->count--;                                     // 更新计数
  return data;                                        // 返回数据指针
}
#endif
/* 计算 CRC 校验值 */
uint16_t Calculate_CRC(uint8_t *data, uint16_t length) {
  uint16_t crc = 0xFFFF; // 初始 CRC 值
  for (uint16_t i = 0; i < length; i++) {
    crc ^= data[i]; // 进行 CRC 计算
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x01) {
        crc = (crc >> 1) ^ 0xA001; // 右移并异或
      } else {
        crc >>= 1; // 右移
      }
    }
  }
  return crc; // 返回计算结果
}

#if 0
void Serial_data_stream_parsing(UART_RX_TypeDef *frameData) {
  if (frameData == NULL) {
    // printf("Error: Invalid frameData pointer or size\n");
    return;
  }
  for (uint16_t i = 0; i < frameData->size - 1; i++) {
    if ((frameData->buffer[i] == FRAME_HEADER_BYTE1 &&
         i + 1 < frameData->size &&
         frameData->buffer[i + 1] == FRAME_HEADER_BYTE2) ||
        (frameData->buffer[i] == 0x6a && i + 1 < frameData->size &&
         frameData->buffer[i + 1] == 0xa6)) {
      for (uint16_t j = i + 2; j < frameData->size - 1; j++) {
        if (frameData->buffer[j] == FRAME_TAIL_BYTE1 &&
            j + 1 < frameData->size &&
            frameData->buffer[j + 1] == FRAME_TAIL_BYTE2) {
          uint16_t frame_size = j - i + 2;
          if (frame_size > UART_RX_BUFFER_SIZE) {
            // printf("Error: Frame size exceeds buffer limit\n");
            break;
          }
          // 计算CRC并校验
          uint16_t received_crc =
              (frameData->buffer[j - 2] | (frameData->buffer[j - 1] << 8));
          uint16_t calculated_crc = Calculate_CRC(
              &frameData->buffer[i],
              frame_size - 4); // 不包含CRC和帧尾
                               // HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_14);
          if (calculated_crc == received_crc) {
            // 													for
            // (uint16_t i = 0; i < frameData->size; i++) {
            //     printf("%02X ", frameData->buffer[i]);
            // }
            // printf("\n");
             //screenack();
            command_parsing(&frameData->buffer[i]);
          } else {
            // printf("Error: CRC mismatch\n");
          }
          // 跳过已解析的帧数据，避免重复解析
          i = j + 1;
          break;
        }
      }
    }
  }
}
#endif

#define UART_PROTOCOL_STREAM_SIZE 512U

static uint8_t protocol_rx_stream[UART_PROTOCOL_STREAM_SIZE];
static size_t protocol_rx_length;

static uint8_t protocol_header_valid(const uint8_t *frame)
{
  return (uint8_t)(((frame[0] == 0x5AU) && (frame[1] == 0xA5U)) ||
                   ((frame[0] == 0x6AU) && (frame[1] == 0xA6U)));
}

static size_t protocol_expected_frame_size(const uint8_t *frame)
{
  return (frame[0] == 0x5AU) ? sizeof(CTRL_MSG) : sizeof(prepare_data);
}

static uint8_t protocol_frame_valid(const uint8_t *frame, size_t frame_size)
{
  uint16_t received_crc;
  uint16_t calculated_crc;

  if (!protocol_header_valid(frame) || frame[2] != frame_size ||
      frame[frame_size - 2U] != FRAME_TAIL_BYTE1 ||
      frame[frame_size - 1U] != FRAME_TAIL_BYTE2) {
    return 0U;
  }

  received_crc = (uint16_t)frame[frame_size - 4U] |
                 (uint16_t)((uint16_t)frame[frame_size - 3U] << 8U);
  calculated_crc = Calculate_CRC((uint8_t *)frame,
                                 (uint16_t)(frame_size - 4U));
  return (uint8_t)(received_crc == calculated_crc);
}

void Serial_data_stream_parsing(const uint8_t *data, size_t length)
{
  if (data == NULL || length == 0U) {
    return;
  }

  if (length > sizeof(protocol_rx_stream) - protocol_rx_length) {
    protocol_rx_length = 0U;
    if (length > sizeof(protocol_rx_stream)) {
      data += length - sizeof(protocol_rx_stream);
      length = sizeof(protocol_rx_stream);
    }
  }
  memcpy(&protocol_rx_stream[protocol_rx_length], data, length);
  protocol_rx_length += length;

  while (protocol_rx_length >= 3U) {
    size_t start = 0U;
    size_t frame_size;

    while (start + 1U < protocol_rx_length &&
           !protocol_header_valid(&protocol_rx_stream[start])) {
      ++start;
    }
    if (start > 0U) {
      memmove(protocol_rx_stream, &protocol_rx_stream[start],
              protocol_rx_length - start);
      protocol_rx_length -= start;
    }
    if (protocol_rx_length < 3U) {
      return;
    }

    frame_size = protocol_expected_frame_size(protocol_rx_stream);
    if (protocol_rx_stream[2] != frame_size) {
      memmove(protocol_rx_stream, &protocol_rx_stream[1],
              protocol_rx_length - 1U);
      protocol_rx_length--;
      continue;
    }
    if (protocol_rx_length < frame_size) {
      return;
    }

    if (protocol_frame_valid(protocol_rx_stream, frame_size)) {
      command_parsing(protocol_rx_stream);
      memmove(protocol_rx_stream, &protocol_rx_stream[frame_size],
              protocol_rx_length - frame_size);
      protocol_rx_length -= frame_size;
    } else {
      /* Skip one byte after a damaged frame so the next valid header can be
       * recovered even if it began inside the declared frame length. */
      memmove(protocol_rx_stream, &protocol_rx_stream[1],
              protocol_rx_length - 1U);
      protocol_rx_length--;
    }
  }
}

#define MAX_RETRIES 3 // 最大重试次数
#define TIMEOUT 1000  // 发送超时时间，单位为毫秒
#if 0
volatile uint8_t uart_tx_done = 1;

// 封装的函数
HAL_StatusTypeDef UART_TransmitWithRetry(UART_HandleTypeDef *huart,
                                         uint8_t *data, uint16_t size) {
		uart_tx_done = 0;  // 标志：开始发送
    SCB_CleanInvalidateDCache();
    HAL_Delay(20);
    HAL_StatusTypeDef status=HAL_UART_Transmit_DMA(huart, data, size);
		if (status != HAL_OK) {
        return status;  // 发送启动失败
    }
		 uint32_t tickstart = HAL_GetTick();
    while (uart_tx_done == 0) {
        if ((HAL_GetTick() - tickstart) > TIMEOUT) {
            return HAL_TIMEOUT;  // 超时未完成
        }
    }

    return HAL_OK;

}
#endif

#define UART_TX_TIMEOUT_MS 20U

static HAL_StatusTypeDef uart_transmit_raw(UART_HandleTypeDef *huart,
                                           uint8_t *data, uint16_t size)
{
  HAL_StatusTypeDef status = HAL_ERROR;

  if (huart == NULL || data == NULL || size == 0U) {
    return HAL_ERROR;
  }

  /* Protocol frames are only 11 or 13 bytes. Polling transmission completes
   * in about 1.1 ms at 115200 baud and avoids DMA buffer lifetime/cache
   * problems. A short retry only applies when the peripheral is temporarily
   * busy or a real timeout/error is reported. */
  for (uint8_t attempt = 0U; attempt < MAX_RETRIES; ++attempt) {
    status = HAL_UART_Transmit(huart, data, size, UART_TX_TIMEOUT_MS);
    if (status == HAL_OK) {
      return HAL_OK;
    }
    if (attempt + 1U < MAX_RETRIES) {
      HAL_Delay(1U);
    }
  }
  return status;
}

HAL_StatusTypeDef UART_TransmitWithRetry(UART_HandleTypeDef *huart,
                                         uint8_t *data, uint16_t size)
{
  HAL_StatusTypeDef status = uart_transmit_raw(huart, data, size);

  if (status == HAL_OK && size >= 5U && data[0] == 0x5AU &&
      data[1] == 0xA5U) {
    uint16_t command = (uint16_t)((uint16_t)data[3] << 8U) | data[4];
    if (treatment_command_requires_ack(command)) {
      treatment_ack_arm(command, data, size);
    }
  }
  return status;
}

void screenstart(void) {
  static AXI_SRAM CTRL_MSG pData;
  pData.cmd_head_high = 0x5A;
  pData.cmd_head_low = 0xA5;
  pData.frame_length = 0x0d;
  pData.cmd_type_high = 0x10;
  pData.cmd_type_low = 0x50;
  pData.data = 1.0;
  pData.crc = Calculate_CRC((uint8_t *)&pData, sizeof(pData) - 4);
  pData.end_high = 0xFF;
  pData.end_low = 0xFF;
  (void)UART_TransmitWithRetry(&huart1, (uint8_t *)&pData, sizeof(pData));
}
void screenack(void) {
  static AXI_SRAM CTRL_MSG pData;
  pData.cmd_head_high = 0x5A;
  pData.cmd_head_low = 0xA5;
  pData.frame_length = 0x0d;
  pData.cmd_type_high = 0x10;
  pData.cmd_type_low = 0x52;
  pData.data = 1.0;
  pData.crc = Calculate_CRC((uint8_t *)&pData, sizeof(pData) - 4);
  pData.end_high = 0xFF;
  pData.end_low = 0xFF;
  UART_TransmitWithRetry(&huart1, (uint8_t *)&pData, sizeof(pData));
}
