#ifndef PC_UART_H
#define PC_UART_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
bool PcUart_Init(void);
bool PcUart_Write(const uint8_t *,uint16_t);
size_t PcUart_Read(uint8_t *,size_t);
void PcUart_Tick(void);
void PcUart_RxFromIsr(void);
void PcUart_ErrorFromIsr(void);
#endif
