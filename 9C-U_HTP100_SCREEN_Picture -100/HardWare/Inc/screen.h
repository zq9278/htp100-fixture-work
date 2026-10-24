#include "main.h" 
#include <stddef.h>
#ifndef __screen_H
#define __screen_H
#define AXI_SRAM __attribute__((section(".AXI_SRAM")))

#define UART_RX_BUFFER_SIZE  256 // 串口接收缓冲区大小
#define FRAME_HEADER_BYTE1   0x5A // 帧头第一个字节
#define FRAME_HEADER_BYTE2   0xA5 // 帧头第二个字节
#define FRAME_TAIL_BYTE1     0xFF // 帧尾第一个字节
#define FRAME_TAIL_BYTE2     0xFF // 帧尾第二个字节
extern uint8_t EYE_status;

typedef struct __attribute__((packed)) {
    uint8_t cmd_head_high; // 
    uint8_t cmd_head_low;  //
    uint8_t frame_length; // 帧长度0x0d
    uint8_t cmd_type_high; // 
    uint8_t cmd_type_low;  // 
    float data;
    uint16_t crc; // CRC 校验
    uint8_t end_high; // 
    uint8_t end_low;  // 
} CTRL_MSG, *PCTRL_MSG;

typedef struct __attribute__((packed)) {
    uint8_t cmd_head_high;
    uint8_t cmd_head_low;
    uint8_t frame_length;
    uint8_t cmd_type_high;
    uint8_t cmd_type_low;
    uint32_t value;
    uint16_t crc;
    uint8_t end_high;
    uint8_t end_low;
} CTRL_MSG_U32, *PCTRL_MSG_U32;
//recept_data结构体类型，表示一个完整的结构体对象。用 . 访问成员
//recept_data_p结构体指针类型，表示一个指向recept_data结构体对象的指针。用 -> 访问成员
//调试结构体
typedef struct __attribute__((packed)){
  uint8_t cmd_head_high; //
  uint8_t cmd_head_low;  //
    uint8_t frame_length; // 帧长度0x0b
  uint8_t cmd_type_high; //
  uint8_t cmd_type_low;  //
  uint16_t value;
    uint16_t crc; // CRC 校验
  uint8_t end_high; //
  uint8_t end_low;  //
} prepare_data, *prepare_data_p;




uint16_t Calculate_CRC(uint8_t *data, uint16_t length); // 计算CRC校验值
size_t ScreenUart_Read(uint8_t *destination, size_t capacity);
void Serial_data_stream_parsing(const uint8_t *data, size_t length);
void screen_preload_language_from_uart(const uint8_t *data, uint16_t size);
void screen_finish_startup_language_preload(void);
void command_parsing(const uint8_t *received_data);
void processData(PCTRL_MSG msg);
void processData_u32(PCTRL_MSG_U32 msg);
void processData_prepare(prepare_data_p msg);
void work_flinsh(void);
void  screenstart(void);
void  screenack(void);
uint8_t screen_start_ack_received(void);
uint8_t screen_start_language_received(void);
void screen_refresh_prepare_ui(void);
void screen_refresh_runtime_ui(void);
void screen_fault_popup_tick(void);
void screen_treatment_ack_tick(void);
void screen_reset_hotting_runtime_ui(void);
void screen_reset_pulsationning_runtime_ui(void);
void screen_reset_autoing_runtime_ui(void);
HAL_StatusTypeDef UART_TransmitWithRetry(UART_HandleTypeDef *huart, uint8_t *data, uint16_t size);

#endif /*__GT911_H_*/


