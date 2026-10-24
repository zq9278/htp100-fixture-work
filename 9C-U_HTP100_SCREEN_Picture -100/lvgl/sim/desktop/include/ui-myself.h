#ifndef UI_MYSELF_H
#define UI_MYSELF_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

#define AXI_SRAM

typedef struct {
    uint8_t cmd_head_high;
    uint8_t cmd_head_low;
    uint8_t frame_length;
    uint8_t cmd_type_high;
    uint8_t cmd_type_low;
    float data;
    uint16_t crc;
    uint8_t end_high;
    uint8_t end_low;
} CTRL_MSG;

typedef struct {
    uint8_t cmd_head_high;
    uint8_t cmd_head_low;
    uint8_t frame_length;
    uint8_t cmd_type_high;
    uint8_t cmd_type_low;
    uint16_t value;
    uint16_t crc;
    uint8_t end_high;
    uint8_t end_low;
} prepare_data;

uint16_t Calculate_CRC(uint8_t *data, uint32_t len);
void UART_TransmitWithRetry(UART_HandleTypeDef *huart, uint8_t *data, uint32_t len);

#endif
