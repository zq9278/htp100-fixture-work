/* Desktop preview: acknowledge navigation only; never invent sensor readings. */
#include "main.h"
#include "combined_ui.h"
#include "fixture_wire.h"
#include <SDL.h>
UART_HandleTypeDef huart1;
static uint8_t reply[FX_FRAME_SIZE];
static int pending;
uint32_t HAL_GetTick(void) { return SDL_GetTicks(); }
int HAL_UART_Transmit(UART_HandleTypeDef *uart,uint8_t *data,uint16_t size,uint32_t timeout) {
    (void)uart;(void)timeout;
    if(size==FX_FRAME_SIZE && data[0]==0xE7 && data[3]==FX_SELECT) {
        Fx_Frame(reply,FX_STATUS,Fx_Get16(data+4),data[6]);
        reply[7]=FX_IDLE;reply[8]=FX_OK;Fx_Seal(reply);pending=1;
    }
    return HAL_OK;
}
void FixtureSim_Tick(void) {
    if(pending) { pending=0;CombinedUi_Feed(reply,sizeof(reply)); }
}
void FixtureBuzzer_Stop(void) {}
void FixtureBuzzer_Success(void) {}
void FixtureBuzzer_Tick(void) {}
