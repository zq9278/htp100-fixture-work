#include "main.h"
#include "screen_uart.h"
#include "pc_uart.h"
#include <string.h>
#define CHECK(x) do { if(!(x)) return __LINE__; } while(0)
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart1;
static uint8_t *it_pointer,*rx_pointer,pc_wire[4096];
static uint16_t it_length;
static uint32_t pc_wire_length;
static uint8_t *dma_pointer, wire[8192], expected[8192];
static uint16_t dma_length;
static uint32_t clock_ms, wire_length, expected_length, aborts;
uint32_t HAL_GetTick(void) { return clock_ms; }
void HAL_GPIO_WritePin(void *p,uint16_t n,GPIO_PinState s) { (void)p;(void)n;(void)s; }
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *u,uint8_t *p,uint16_t n) { (void)u;(void)p;(void)n;return HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *u) { (void)u;return HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *u) { aborts++;u->gState=HAL_UART_STATE_READY;dma_length=0;return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *u,uint8_t *p,uint16_t n) {
    if(u->gState!=HAL_UART_STATE_READY) return HAL_ERROR;
    dma_pointer=p;dma_length=n;u->gState=HAL_UART_STATE_BUSY_TX;return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *u,uint8_t *p,uint16_t n) { (void)u;(void)n;rx_pointer=p;return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *u,uint8_t *p,uint16_t n) {
    if(u->gState!=HAL_UART_STATE_READY) return HAL_ERROR;
    it_pointer=p;it_length=n;u->gState=HAL_UART_STATE_BUSY_TX;return HAL_OK;
}
static void drain_pc(void) {
    while(it_length) {
        memcpy(pc_wire+pc_wire_length,it_pointer,it_length);pc_wire_length+=it_length;it_length=0;
        huart1.gState=HAL_UART_STATE_READY;PcUart_Tick();
    }
}
static void drain(void) {
    while(dma_length) {
        memcpy(wire+wire_length,dma_pointer,dma_length);wire_length+=dma_length;dma_length=0;
        huart2.gState=HAL_UART_STATE_READY;ScreenUart_TxTick();
    }
}
int main(void) {
    CHECK(ScreenUart_Init());
    uint8_t data[74];
    for(unsigned i=0;i<sizeof(data);i++) data[i]=(uint8_t)i;
    CHECK(ScreenUart_Write(data,sizeof(data)));
    memcpy(expected,data,sizeof(data));expected_length=sizeof(data);
    memset(data,255,sizeof(data)); /* DMA must not use this caller's buffer. */
    drain();CHECK(wire_length==expected_length && !memcmp(wire,expected,wire_length));
    for(unsigned round=0;round<40;round++) {
        memset(data,(int)round,sizeof(data));
        CHECK(ScreenUart_Write(data,sizeof(data)));
        memcpy(expected+expected_length,data,sizeof(data));expected_length+=sizeof(data);
        if(round%3==2) drain(); /* Includes circular queue wrap and split DMAs. */
    }
    drain();CHECK(wire_length==expected_length && !memcmp(wire,expected,wire_length));
    unsigned accepted=0;
    while(ScreenUart_Write(data,sizeof(data))) accepted++;
    CHECK(accepted==13); /* 1023 usable bytes; no partial enqueue on overflow. */
    uint32_t before=wire_length;drain();CHECK(wire_length==before+accepted*sizeof(data));
    CHECK(!ScreenUart_Write(0,1) && !ScreenUart_Write(data,0));
    CHECK(ScreenUart_Write(data,sizeof(data)));
    clock_ms+=101;ScreenUart_TxTick();CHECK(aborts==1 && dma_length==0);
    before=wire_length;CHECK(ScreenUart_Write(data,sizeof(data)));drain();
    CHECK(wire_length==before+sizeof(data));
    CHECK(PcUart_Init());
    for(unsigned i=0;i<300;i++) { *rx_pointer=(uint8_t)i;PcUart_RxFromIsr(); }
    uint8_t received[300];CHECK(PcUart_Read(received,sizeof(received))==255);
    for(unsigned i=0;i<255;i++) CHECK(received[i]==(uint8_t)i);
    CHECK(PcUart_Read(received,sizeof(received))==0);
    PcUart_ErrorFromIsr();*rx_pointer=42;PcUart_RxFromIsr();CHECK(PcUart_Read(received,1)==1 && received[0]==42);
    memset(data,77,sizeof(data));CHECK(PcUart_Write(data,sizeof(data)));
    memset(data,88,sizeof(data));CHECK(ScreenUart_Write(data,sizeof(data)));drain();
    memset(data,99,sizeof(data));drain_pc();CHECK(pc_wire_length==74 && pc_wire[0]==77 && pc_wire[73]==77);
    unsigned pc_accepted=0;while(PcUart_Write(data,sizeof(data))) pc_accepted++;
    CHECK(pc_accepted==13);drain_pc();CHECK(pc_wire_length==74+13*74);
    return 0;
}
