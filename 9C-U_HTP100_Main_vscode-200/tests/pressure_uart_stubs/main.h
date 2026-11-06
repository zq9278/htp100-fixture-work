#ifndef PRESSURE_UART_TEST_HAL_H
#define PRESSURE_UART_TEST_HAL_H
#include <stdint.h>
typedef enum { HAL_OK=0, HAL_ERROR=1 } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET=0, GPIO_PIN_SET=1 } GPIO_PinState;
typedef struct { volatile uint32_t gState; void *hdmarx; } UART_HandleTypeDef;
#define HAL_UART_STATE_READY 0U
#define HAL_UART_STATE_BUSY_TX 1U
#define LCD_PW_GPIO_Port ((void *)0)
#define LCD_PW_Pin 1U
#define DMA_IT_HT 0U
#define __HAL_DMA_DISABLE_IT(a,b) ((void)0)
uint32_t HAL_GetTick(void);
void HAL_GPIO_WritePin(void *,uint16_t,GPIO_PinState);
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *,uint8_t *,uint16_t);
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
#endif
