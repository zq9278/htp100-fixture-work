/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "dma2d.h"
#include "i2c.h"
#include "ltdc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "fmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Lcd_int.h"
#include "ui.h"
#include "fixture_ui.h"
#include "touch.h"
// #include "touch.h"
#include "lv_port_indev_template.h"
#include "lv_port_disp_template.h"
#include "screen.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern uint8_t Tim7_flag_60ms;
 extern uint32_t save_picture_time;
extern uint16_t Tim7_flag_1s;
extern TIM_HandleTypeDef htim1;

#if 0
extern RingQueue uartRingQueue; // 初始化队�???
extern UART_RX_TypeDef uart_rx_data_t[UART_BUFFER_QUANTITY]; // 双缓冲数据存�???
extern uint8_t uart_buff_ctrl; // 缓冲区索�???
#endif
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_NVIC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* Kept in internal RAM for debugger inspection after a restart. */
volatile uint32_t screen_reset_flags;
volatile uint32_t screen_boot_stage;

static void screen_watchdog_feed(void)
{
  IWDG1->KR = 0xAAAAU;
}

static void screen_watchdog_start(void)
{
  screen_reset_flags = RCC->RSR;
  __HAL_RCC_CLEAR_RESET_FLAGS();
  /* Register sequence follows STM32H7 IWDG: start forces LSI on. No
   * dependency on HSE/PLL, SysTick, HAL initialization or external SDRAM.
   * 256 * (999 + 1) / nominal 32000 Hz = 8 s (LSI tolerance applies). */
  IWDG1->KR = 0xCCCCU;
  IWDG1->KR = 0x5555U;
  IWDG1->PR = 6U;
  IWDG1->RLR = 999U;
  /* Do not feed while waiting: IWDG itself resets a failed initialization. */
  while (IWDG1->SR != 0U) {}
  IWDG1->WINR = 0xFFFU;
  while (IWDG1->SR != 0U) {}
  screen_watchdog_feed();
}

static void screen_boot_progress(uint32_t stage)
{
  screen_boot_stage = stage;
  screen_watchdog_feed();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  screen_watchdog_start();
  screen_boot_progress(1U);

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
  screen_boot_progress(2U);

  /* USER CODE BEGIN SysInit */
  HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PA0, SYSCFG_SWITCH_PA0_CLOSE); // ��������Ϊģ�⿪�أ���ʵ�ֲ�ͬ�Ĺ��ܡ����磬���Խ���������Ϊģ�����롢ģ���������??????���ӵ��ڲ���ģ��ģ�飨?? ADC��DAC �ȣ��ر� PA0 ����
  HAL_SYSCFG_AnalogSwitchConfig(SYSCFG_SWITCH_PC2, SYSCFG_SWITCH_PC2_CLOSE); // �ر� PC2 ���ӡ�
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();

  /* Fixture protocol starts after the UI is ready; old boot handshake disabled. */
  screen_boot_progress(3U);
  MX_TIM7_Init();
  MX_TIM15_Init();
  MX_DMA2D_Init();
  MX_FMC_Init();
  screen_boot_progress(4U);
  lv_port_disp_clear_buffers();
  MX_LTDC_Init();
  MX_TIM14_Init();
  MX_TIM6_Init();
  MX_SPI1_Init();
  MX_I2C4_Init();
  MX_TIM13_Init();
  MX_TIM1_Init();

  /* Initialize interrupts */
  MX_NVIC_Init();
  /* USER CODE BEGIN 2 */
  /* LCD reset/init is performed once by lv_port_disp_init(). */
  GT911_Init();
  screen_boot_progress(5U);
  lv_init();
  lv_port_disp_init();
  screen_boot_progress(6U);
  lv_port_indev_init();
  // screen_finish_startup_language_preload();
  // ui_init(); /* Original treatment UI disabled. */
  FixtureUi_Init();
  screen_boot_progress(7U);
  //HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1);//start PWM for Beep
  /* Keep PA11/TIM1_CH4 dark until the first UI frame is drawn. No wait
   * for a hardware reload flag: frame submission remains immediate. */
  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 60);
  uint8_t backlight_started = 0U;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
		HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_14);
     //HAL_Delay(90);
#if 0
        UART_RX_TypeDef *frameData;
        while ((frameData = RingQueue_Pop(&uartRingQueue)) != NULL) {
            Serial_data_stream_parsing(frameData); // 解析数据�???
        }
#endif
        uint8_t serial_data[128];
        size_t received;
        /* Bound UART work so a noisy/continuous stream cannot starve UI
         * progress and watchdog servicing. Remaining bytes stay queued. */
        for (uint8_t batch = 0U; batch < 4U; ++batch) {
            received = ScreenUart_Read(serial_data, sizeof(serial_data));
            if (received == 0U) break;
            FixtureUi_Feed(serial_data, received);
        }
        GT911_Process();
        /* Legacy treatment UI, popups and protocol ticks are disabled. */
        FixtureUi_Tick();
        if (!backlight_started) {
            lv_obj_invalidate(lv_scr_act());
            lv_obj_invalidate(lv_layer_top());
            lv_refr_now(NULL);
            /* disp_flush has published the rendered pixels. This does not
             * wait on VSYNC, an interrupt, or an acknowledgement flag. */
            if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4) != HAL_OK) {
                Error_Handler();
            }
            __HAL_TIM_MOE_ENABLE(&htim1);
            backlight_started = 1U;
        }
        lv_timer_handler();
		
    HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_15);
/* Legacy treatment finish and screen-start retry disabled. */
        /* Only a completed communication/touch/UI loop proves liveness.
         * Never feed from SysTick, interrupts, or peripheral wait loops. */
        screen_boot_progress(8U);
	}
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief NVIC Configuration.
  * @retval None
  */
static void MX_NVIC_Init(void)
{
  /* TIM8_UP_TIM13_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(TIM8_UP_TIM13_IRQn, 3, 0);
  HAL_NVIC_EnableIRQ(TIM8_UP_TIM13_IRQn);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0xC0000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_32MB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
