#include "screen.h"
#include "stm32h7xx_hal.h"
#include "ui-myself.h"

#include <stddef.h>
#include <time.h>

UART_HandleTypeDef huart1;
TIM_HandleTypeDef htim14;
TIM_HandleTypeDef htim15;
TIM_HandleTypeDef htim13;

int minutes = 0;
int seconds = 0;
uint8_t usart1_tx = 0;
uint8_t EYE_status = 1;

uint16_t prepare_temprature = 0;
uint16_t prepare_crimp = 200;
uint16_t prepare_time = 5;
uint16_t bee = 1;
uint16_t prepare_set = 0;

lv_obj_t *global_panel = NULL;

void screen_refresh_prepare_ui(void) {}
void screen_refresh_runtime_ui(void) {}
void screen_reset_hotting_runtime_ui(void) {}
void screen_reset_pulsationning_runtime_ui(void) {}
void screen_reset_autoing_runtime_ui(void) {}

void HAL_GPIO_WritePin(int port, int pin, int state)
{
    (void)port;
    (void)pin;
    (void)state;
}

void HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, int channel)
{
    (void)htim;
    (void)channel;
}

void HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *htim)
{
    (void)htim;
}

void HAL_TIM_Base_Stop_IT(TIM_HandleTypeDef *htim)
{
    (void)htim;
}

uint32_t HAL_GetTick(void)
{
    return (uint32_t)((clock() * 1000) / CLOCKS_PER_SEC);
}

uint16_t Calculate_CRC(uint8_t *data, uint32_t len)
{
    uint16_t crc = 0;
    uint32_t i;

    for(i = 0; i < len; ++i) {
        crc = (uint16_t)(crc + data[i]);
    }

    return crc;
}

void UART_TransmitWithRetry(UART_HandleTypeDef *huart, uint8_t *data, uint32_t len)
{
    (void)huart;
    (void)data;
    (void)len;
}
