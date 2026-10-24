#ifndef STM32H7XX_HAL_H
#define STM32H7XX_HAL_H

#include <stdint.h>

typedef struct {
    int unused;
} UART_HandleTypeDef;

typedef struct {
    int unused;
} TIM_HandleTypeDef;

#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET 1

#define GPIOE 0
#define GPIOH 1

#define GPIO_PIN_4 4
#define GPIO_PIN_15 15

#define TIM_CHANNEL_1 1
#define TIM_FLAG_UPDATE 1

#define Bee_vcc_GPIO_Port GPIOE
#define Bee_vcc_Pin GPIO_PIN_4

#define __HAL_TIM_CLEAR_FLAG(htim, flag) ((void)(htim), (void)(flag))

void HAL_GPIO_WritePin(int port, int pin, int state);
void HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, int channel);
void HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *htim);
void HAL_TIM_Base_Stop_IT(TIM_HandleTypeDef *htim);
uint32_t HAL_GetTick(void);

#endif
