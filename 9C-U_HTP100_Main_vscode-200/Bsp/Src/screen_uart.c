#include "screen_uart.h"

#include "FreeRTOS.h"
#include "main.h"
#include "task.h"
#include "usart.h"

#define SCREEN_RX_DMA_SIZE 128U
#define SCREEN_RING_SIZE   512U

static uint8_t g_dma[SCREEN_RX_DMA_SIZE];
static uint8_t g_ring[SCREEN_RING_SIZE];
static volatile uint16_t g_write;
static volatile uint16_t g_read;
static bool g_initialized;

#ifdef PRESSURE_MONITOR_BUILD
#define SCREEN_TX_SIZE 1024U
static uint8_t g_tx[SCREEN_TX_SIZE];
static uint16_t g_tx_read, g_tx_write, g_tx_inflight;
static uint32_t g_tx_started;
#endif

void ScreenUart_TxTick(void)
{
#ifdef PRESSURE_MONITOR_BUILD
    if(g_tx_inflight) {
        if(huart2.gState==HAL_UART_STATE_READY) {
            g_tx_read=(uint16_t)((g_tx_read+g_tx_inflight)%SCREEN_TX_SIZE);
            g_tx_inflight=0;
        } else if(HAL_GetTick()-g_tx_started>=100U) {
            (void)HAL_UART_AbortTransmit(&huart2);
            g_tx_read=g_tx_write=g_tx_inflight=0;
        } else return;
    }
    if(g_tx_read!=g_tx_write) {
        uint16_t n=g_tx_write>g_tx_read?(uint16_t)(g_tx_write-g_tx_read):(uint16_t)(SCREEN_TX_SIZE-g_tx_read);
        if(HAL_UART_Transmit_DMA(&huart2,g_tx+g_tx_read,n)==HAL_OK) {
            g_tx_inflight=n; g_tx_started=HAL_GetTick();
        }
    }
#endif
}

static void screen_uart_start_receive(void)
{
    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart2, g_dma, sizeof(g_dma));
    __HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT);
}

bool ScreenUart_Init(void)
{
    if (g_initialized) {
        return true;
    }
    g_write = 0U;
    g_read = 0U;
    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, g_dma, sizeof(g_dma)) != HAL_OK) {
        return false;
    }
    __HAL_DMA_DISABLE_IT(huart2.hdmarx, DMA_IT_HT);
    g_initialized = true;
    return true;
}

void ScreenUart_SetPower(bool enabled)
{
    /* LCD_PW is an open-drain, active-low screen power enable. */
    HAL_GPIO_WritePin(LCD_PW_GPIO_Port, LCD_PW_Pin,
                      enabled ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

bool ScreenUart_Write(const uint8_t *data, uint16_t length)
{
#ifdef PRESSURE_MONITOR_BUILD
    if(data==NULL || length==0U) return false;
    uint16_t occupied=(uint16_t)((g_tx_write+SCREEN_TX_SIZE-g_tx_read)%SCREEN_TX_SIZE);
    if(length>SCREEN_TX_SIZE-1U-occupied) return false;
    /* Copy whole frames before returning: caller buffers often live on stack.
     * The read index advances only after HAL reports DMA transmission complete. */
    for(uint16_t i=0;i<length;i++) {
        g_tx[g_tx_write]=data[i]; g_tx_write=(uint16_t)((g_tx_write+1U)%SCREEN_TX_SIZE);
    }
    ScreenUart_TxTick();
    return true;
#else
    return data != NULL && length != 0U &&
           HAL_UART_Transmit(&huart2, (uint8_t *)data, length, 30U) == HAL_OK;
#endif
}

size_t ScreenUart_Read(uint8_t *destination, size_t capacity)
{
    size_t count = 0U;

    taskENTER_CRITICAL();
    while (destination != NULL && count < capacity && g_read != g_write) {
        destination[count++] = g_ring[g_read];
        g_read = (uint16_t)((g_read + 1U) % SCREEN_RING_SIZE);
    }
    taskEXIT_CRITICAL();
    return count;
}

void ScreenUart_RxEventFromIsr(uint16_t length)
{
    for (uint16_t index = 0U; index < length && index < SCREEN_RX_DMA_SIZE; ++index) {
        uint16_t next = (uint16_t)((g_write + 1U) % SCREEN_RING_SIZE);
        if (next == g_read) {
            break;
        }
        g_ring[g_write] = g_dma[index];
        g_write = next;
    }
    screen_uart_start_receive();
}

void ScreenUart_ErrorFromIsr(void)
{
    (void)HAL_UART_AbortReceive(&huart2);
    screen_uart_start_receive();
}
