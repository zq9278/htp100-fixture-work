#include "fixture_buzzer.h"
#include "main.h"
#include "tim.h"
#include <stdbool.h>
#include <stdint.h>

static uint32_t started;
static bool active,on;

void FixtureBuzzer_Stop(void)
{
    (void)HAL_TIM_PWM_Stop(&htim15,TIM_CHANNEL_1);
    active=on=false;
}

void FixtureBuzzer_Success(void)
{
    FixtureBuzzer_Stop();
    started=HAL_GetTick();
    active=on=HAL_TIM_PWM_Start(&htim15,TIM_CHANNEL_1)==HAL_OK;
}

void FixtureBuzzer_Tick(void)
{
    if(!active) return;
    uint32_t elapsed=HAL_GetTick()-started;
    /* Two 150ms beeps separated by 120ms; no delays in the UI loop. */
    if(elapsed>=420U) { FixtureBuzzer_Stop(); return; }
    bool wanted=elapsed<150U || elapsed>=270U;
    if(wanted==on) return;
    if(wanted) {
        if(HAL_TIM_PWM_Start(&htim15,TIM_CHANNEL_1)!=HAL_OK) {
            FixtureBuzzer_Stop(); return;
        }
    } else (void)HAL_TIM_PWM_Stop(&htim15,TIM_CHANNEL_1);
    on=wanted;
}
