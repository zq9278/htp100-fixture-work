#include "board_port.h"
#include "screen_uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "tim.h"
#include "ws2812_driver.h"

static volatile bool power_lost;

bool Board_ScreenWrite(const uint8_t *data, uint16_t length)
{
    return ScreenUart_Write(data, length);
}

size_t Board_ScreenRead(uint8_t *data, size_t capacity)
{
    return ScreenUart_Read(data, capacity);
}

void Board_UartRxEventFromIsr(uint16_t length)
{
    ScreenUart_RxEventFromIsr(length);
}

void Board_UartErrorFromIsr(void)
{
    ScreenUart_ErrorFromIsr();
}

void Board_PowerLossFromIsr(void)
{
    /* Remove PWM immediately; the task reports the reason and resets PID. */
    __HAL_TIM_SET_COMPARE(&htim14, TIM_CHANNEL_1, 0U);
    power_lost = true;
}

bool Board_TakePowerLossEvent(void)
{
    taskENTER_CRITICAL();
    bool value = power_lost;
    power_lost = false;
    taskEXIT_CRITICAL();
    return value;
}

/* Treatment button remains disabled; WS2812 DMA must release its buffer. */
void Board_ButtonFromIsr(void) { }
void Board_Ws2812FinishedFromIsr(void) { Ws2812Driver_FinishedFromIsr(); }
