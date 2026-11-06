#include "app_main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "tim.h"
#include "board_port.h"
#include "screen_uart.h"
#include "ads1220_driver.h"
#include "storage_driver.h"
#include "charger_driver.h"
#include "pressure_monitor.h"
#include "pc_uart.h"
#include "app_log.h"
#include "ws2812_driver.h"
#include "fixture_controller.h"
#include "eye_driver.h"
#include "treatment_hw.h"
#include "combined_runtime.h"

static const PressureMonitorPort port={HAL_GetTick,Board_ScreenWrite,
    StorageDriver_ReadBytes,StorageDriver_WriteBytes,PcUart_Write,Fixture_PressureSelected};

static void fixture_status_led(uint8_t state) {
    /* WS2812 uses GRB: only PASS/FAIL override the default solid blue. */
    uint32_t grb=state==FX_PASS?0x300000U:
                 state==FX_FAIL?0x003000U:0x000020U;
    Ws2812Driver_SetFixtureColor(grb);
}
static const FixturePort fixture_port={
    HAL_GetTick,TreatmentHw_SafeOutputsOff,EyeDriver_FixtureProbe,
    TreatmentHw_HeaterControl,EyeDriver_FixturePrepare,Board_ScreenWrite,
    EyeDriver_FixtureCheckUsable,fixture_status_led,EyeDriver_FixtureDiagnostics,
    EyeDriver_FixtureClearMarker
};

static CombinedParser screen_parser;

static struct {
    bool valid, charging, full;
    uint8_t soc;
    uint16_t mv;
    uint32_t updated;
} power_status;

static void outputs_off(void)
{
    __HAL_TIM_SET_COMPARE(&htim14,TIM_CHANNEL_1,0U);
    (void)HAL_TIM_PWM_Stop(&htim14,TIM_CHANNEL_1);
    HAL_GPIO_WritePin(TMC_ENN_GPIO_Port,TMC_ENN_Pin,GPIO_PIN_SET);
}

static void power_poll(void)
{
    bool charging=false, full=false;
    uint16_t soc=0,mv=0;
    bool ok=ChargerDriver_Read(&charging,&full,&soc,&mv);
    bool gauge=ChargerDriver_GaugeValid();
    taskENTER_CRITICAL();
    power_status.valid=ok; power_status.charging=charging; power_status.full=full;
    power_status.soc=gauge?(uint8_t)soc:255; power_status.mv=gauge?mv:0;
    power_status.updated=HAL_GetTick();
    taskEXIT_CRITICAL();
}

static void power_task(void *argument)
{
    (void)argument;
    (void)ChargerDriver_Init(); /* Keep original charge and gauge configuration. */
    for(;;) { power_poll(); vTaskDelay(pdMS_TO_TICKS(1000U)); }
}

static void update_power(void)
{
    taskENTER_CRITICAL();
    PressureMonitor_Power(power_status.valid && HAL_GetTick()-power_status.updated<2500U,
        power_status.charging,power_status.full,power_status.soc,power_status.mv);
    taskEXIT_CRITICAL();
}

void AppMain_Task(void *argument)
{
    (void)argument;
    AppLog_Init(); outputs_off();
    TreatmentHw_Init();EyeDriver_Init();Fixture_Init(&fixture_port);
    /* H7 display stays powered, including during charging. PC uses USART1. */
    ScreenUart_SetPower(true); (void)ScreenUart_Init(); (void)PcUart_Init();
    StorageDriver_PreparePins(); /* Never call legacy Init: it may erase EEPROM. */
    PressureMonitor_Init(&port);
    /* I2C retries must never hold up the higher-priority pressure sampler. */
    if(xTaskCreate(power_task,"charge",384,NULL,tskIDLE_PRIORITY+1U,NULL)!=pdPASS) {
        LOGE("[Power] Failed to allocate charging task");
    }
    Ws2812Driver_Init();
    Ads1220Driver_PreparePins();
    bool ready=Ads1220Driver_Init();
    PressureMonitor_AdcReady(ready);
    uint32_t retry=HAL_GetTick(), last_raw=retry;
    for(;;) {
        uint8_t data[128];
        size_t count=Board_ScreenRead(data,sizeof(data));
        if(count) Combined_Feed(&screen_parser,data,count,Combined_Dispatch);
        count=PcUart_Read(data,sizeof(data));
        if(count) PressureMonitor_FeedPc(data,count);
        uint32_t now=HAL_GetTick();
        if(!ready && now-retry>=2000U) {
            retry=now; ready=Ads1220Driver_RetryInit();
            PressureMonitor_AdcReady(ready);
        }
        if(ready && HAL_GPIO_ReadPin(ADS1220_DRDY_GPIO_Port,ADS1220_DRDY_Pin)==GPIO_PIN_RESET) {
            int32_t raw=0; bool valid=Ads1220Driver_ReadRaw(&raw);
            PressureMonitor_Sample(valid,raw); last_raw=HAL_GetTick();
            if(!valid) { ready=false; PressureMonitor_AdcReady(false); retry=last_raw; }
        } else if(ready && now-last_raw>=100U) {
            PressureMonitor_Sample(false,0); last_raw=now;
            /* Recover from an unplugged/stalled ADC without stopping charging. */
            ready=false; PressureMonitor_AdcReady(false); retry=now;
        }
        update_power();
        PressureMonitor_Tick();
        ScreenUart_TxTick();
        PcUart_Tick();
        if(Board_TakePowerLossEvent()) { outputs_off();Fixture_PowerLost(); }
        Fixture_Tick();
        if(Fixture_PressureSelected()) Ws2812Driver_SetFixtureColor(ready?0x000020U:0x003000U);
        Ws2812Driver_Tick();
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
}
