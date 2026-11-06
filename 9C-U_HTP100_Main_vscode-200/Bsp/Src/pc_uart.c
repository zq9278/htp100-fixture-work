/* G0 USART1 PB6/PB7: independent PC protocol, interrupt-driven RX and TX. */
#include "pc_uart.h"
#include "main.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#define RX_SIZE 256U
#define TX_SIZE 1024U
static uint8_t rx_byte,rx[RX_SIZE],tx[TX_SIZE];
static volatile uint16_t rx_write,rx_read;
static uint16_t tx_read,tx_write,inflight;
static uint32_t started;
static volatile bool rx_armed;
bool PcUart_Init(void) {
    rx_armed=HAL_UART_Receive_IT(&huart1,&rx_byte,1)==HAL_OK;
    return rx_armed;
}
void PcUart_RxFromIsr(void) {
    uint16_t next=(uint16_t)((rx_write+1U)%RX_SIZE);
    if(next!=rx_read) { rx[rx_write]=rx_byte;rx_write=next; }
    rx_armed=HAL_UART_Receive_IT(&huart1,&rx_byte,1)==HAL_OK;
}
void PcUart_ErrorFromIsr(void) {
    (void)HAL_UART_AbortReceive(&huart1);
    rx_armed=HAL_UART_Receive_IT(&huart1,&rx_byte,1)==HAL_OK;
}
size_t PcUart_Read(uint8_t *p,size_t n) {
    size_t count=0;taskENTER_CRITICAL();
    while(p && count<n && rx_read!=rx_write) {
        p[count++]=rx[rx_read];rx_read=(uint16_t)((rx_read+1U)%RX_SIZE);
    }
    taskEXIT_CRITICAL();return count;
}
void PcUart_Tick(void) {
    if(!rx_armed) { taskENTER_CRITICAL();(void)PcUart_Init();taskEXIT_CRITICAL(); }
    if(inflight) {
        if(huart1.gState==HAL_UART_STATE_READY) {
            tx_read=(uint16_t)((tx_read+inflight)%TX_SIZE);inflight=0;
        } else if(HAL_GetTick()-started>=100U) {
            (void)HAL_UART_AbortTransmit(&huart1);tx_read=tx_write=inflight=0;
        } else return;
    }
    if(tx_read!=tx_write) {
        uint16_t n=tx_write>tx_read?(uint16_t)(tx_write-tx_read):(uint16_t)(TX_SIZE-tx_read);
        if(HAL_UART_Transmit_IT(&huart1,tx+tx_read,n)==HAL_OK) { inflight=n;started=HAL_GetTick(); }
    }
}
bool PcUart_Write(const uint8_t *p,uint16_t n) {
    uint16_t used=(uint16_t)((tx_write+TX_SIZE-tx_read)%TX_SIZE);
    if(!p || !n || n>TX_SIZE-1U-used) return false;
    while(n--) { tx[tx_write]=*p++;tx_write=(uint16_t)((tx_write+1U)%TX_SIZE); }
    PcUart_Tick();return true;
}
