/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    stm32h7xx_it.c
 * @brief   Interrupt Service Routines.
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
#include "stm32h7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tim.h"
#include "screen.h"
#include "touch.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
#define LVGL_MemoryAdd1 ((uint32_t)0xC0000000)
#define LVGL_MemoryAdd2 (LVGL_MemoryAdd1 + 480 * 640 * 2)
#define LVGL_MemoryAdd3 (LVGL_MemoryAdd2 + 480 * 640 * 2) // 显示缓冲区地�?????
extern bool lv_disp_updated;
static uint16_t counter = 0; // ��???���������ڼ����жϴ���
static uint16_t INT = 0;     // ��???���������ڼ����жϴ���
uint16_t Tim7Cnt;
uint16_t Tim7_flag_20ms;
uint16_t Tim7_flag_40ms;
uint16_t Tim7_flag_60ms;
uint16_t Tim7_flag_100ms;
uint16_t Tim7_flag_1s;
extern uint8_t lv_gpu_state;
extern lv_disp_drv_t *lvgl;
uint8_t usart1_tx = 1;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;

volatile int minutes = 10;     // ����ʱ��ʼ�ķ���?????
volatile int seconds = 0;      // ����ʱ��ʼ������
extern lv_obj_t *ui_cut_d_l11; // ����ʱ��ʾ�ı�ǩ
extern lv_obj_t *ui_cut_d_l12; // ����ʱ��ʾ�ı�ǩ
extern lv_obj_t *ui_cut_d_l21; // ����ʱ��ʾ�ı�ǩ
extern lv_obj_t *ui_cut_d_l22; // ����ʱ��ʾ�ı�ǩ
extern lv_obj_t *ui_cut_d_l31; // ����ʱ��ʾ�ı�ǩ
extern lv_obj_t *ui_cut_d_l32; // ����ʱ��ʾ�ı�ǩ
extern lv_obj_t *ui_Image53;
extern lv_obj_t *ui_Image56;
extern lv_obj_t *ui_Image57;
extern lv_obj_t *ui_Container3;
extern lv_obj_t *ui_Container5;
extern lv_obj_t *ui_Container8;

extern uint8_t work_mode; // 0:û����???1���ȷ�???2������???3����?????
//AXI_SRAM uint8_t USART1_RX_STA;    // ����״???��?????

//#define RX_BUFFER_SIZE 100
//uint8_t rx_buffer[RX_BUFFER_SIZE];

uint8_t rx_index = 0;
uint8_t last_byte = 0;
int frame_started = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern DMA2D_HandleTypeDef hdma2d;
extern I2C_HandleTypeDef hi2c4;
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim7;
extern TIM_HandleTypeDef htim13;
extern TIM_HandleTypeDef htim14;
extern TIM_HandleTypeDef htim15;
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern UART_HandleTypeDef huart1;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  //uint32_t CFSR = SCB->CFSR; // 读取 Configurable Fault Status Register
    //uint32_t HFSR = SCB->HFSR; // 读取 HardFault Status Register
//uint32_t fault_address = SCB->BFAR;
//printf("Bus Fault Address: 0x%08lX\n", SCB->BFAR);

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */
  lv_tick_inc(1);
  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32H7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32h7xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles DMA1 stream0 global interrupt.
  */
void DMA1_Stream0_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream0_IRQn 0 */

  /* USER CODE END DMA1_Stream0_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart1_rx);
  /* USER CODE BEGIN DMA1_Stream0_IRQn 1 */

  /* USER CODE END DMA1_Stream0_IRQn 1 */
}

/**
  * @brief This function handles DMA1 stream1 global interrupt.
  */
void DMA1_Stream1_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream1_IRQn 0 */

  /* USER CODE END DMA1_Stream1_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart1_tx);
  /* USER CODE BEGIN DMA1_Stream1_IRQn 1 */

  /* USER CODE END DMA1_Stream1_IRQn 1 */
}

/**
  * @brief This function handles EXTI line[9:5] interrupts.
  */
void EXTI9_5_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI9_5_IRQn 0 */

  /* USER CODE END EXTI9_5_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);
  /* USER CODE BEGIN EXTI9_5_IRQn 1 */

  /* USER CODE END EXTI9_5_IRQn 1 */
}

/**
  * @brief This function handles USART1 global interrupt.
  */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */

  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */

  /* USER CODE END USART1_IRQn 1 */
}

/**
  * @brief This function handles TIM8 update interrupt and TIM13 global interrupt.
  */
void TIM8_UP_TIM13_IRQHandler(void)
{
  /* USER CODE BEGIN TIM8_UP_TIM13_IRQn 0 */

  /* USER CODE END TIM8_UP_TIM13_IRQn 0 */
  HAL_TIM_IRQHandler(&htim13);
  /* USER CODE BEGIN TIM8_UP_TIM13_IRQn 1 */

  /* USER CODE END TIM8_UP_TIM13_IRQn 1 */
}

/**
  * @brief This function handles TIM8 trigger and commutation interrupts and TIM14 global interrupt.
  */
void TIM8_TRG_COM_TIM14_IRQHandler(void)
{
  /* USER CODE BEGIN TIM8_TRG_COM_TIM14_IRQn 0 */

  /* USER CODE END TIM8_TRG_COM_TIM14_IRQn 0 */
  HAL_TIM_IRQHandler(&htim14);
  /* USER CODE BEGIN TIM8_TRG_COM_TIM14_IRQn 1 */

  /* USER CODE END TIM8_TRG_COM_TIM14_IRQn 1 */
}

/**
  * @brief This function handles TIM6 global interrupt, DAC1_CH1 and DAC1_CH2 underrun error interrupts.
  */
void TIM6_DAC_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_DAC_IRQn 0 */

  /* USER CODE END TIM6_DAC_IRQn 0 */
  HAL_TIM_IRQHandler(&htim6);
  /* USER CODE BEGIN TIM6_DAC_IRQn 1 */

  /* USER CODE END TIM6_DAC_IRQn 1 */
}

/**
  * @brief This function handles TIM7 global interrupt.
  */
void TIM7_IRQHandler(void)
{
  /* USER CODE BEGIN TIM7_IRQn 0 */

  /* USER CODE END TIM7_IRQn 0 */
  HAL_TIM_IRQHandler(&htim7);
  /* USER CODE BEGIN TIM7_IRQn 1 */

  /* USER CODE END TIM7_IRQn 1 */
}

/**
  * @brief This function handles DMA2D global interrupt.
  */
void DMA2D_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2D_IRQn 0 */
//   if ((DMA2D->ISR & DMA2D_FLAG_TC) != 0U)
//   {
//     if ((DMA2D->CR & DMA2D_IT_TC) != 0U)
//     {
//       /* ǥ³ý±ꖾ�? */
//       DMA2D->CR &= ~DMA2D_IT_TC;
//       DMA2D->IFCR = DMA2D_FLAG_TC;
//       /* ˢЂϔʾɨ±????? */
//       // if (lv_gpu_state == 1)
//           lv_gpu_state ^= 1;
//        lv_disp_flush_ready(lvgl);
      
//     }
//   }
  /* USER CODE END DMA2D_IRQn 0 */
  HAL_DMA2D_IRQHandler(&hdma2d);
  /* USER CODE BEGIN DMA2D_IRQn 1 */

  /* USER CODE END DMA2D_IRQn 1 */
}

/**
  * @brief This function handles I2C4 error interrupt.
  */
void I2C4_ER_IRQHandler(void)
{
  /* USER CODE BEGIN I2C4_ER_IRQn 0 */

  /* USER CODE END I2C4_ER_IRQn 0 */
  HAL_I2C_ER_IRQHandler(&hi2c4);
  /* USER CODE BEGIN I2C4_ER_IRQn 1 */

  /* USER CODE END I2C4_ER_IRQn 1 */
}

/**
  * @brief This function handles TIM15 global interrupt.
  */
void TIM15_IRQHandler(void)
{
  /* USER CODE BEGIN TIM15_IRQn 0 */

  /* USER CODE END TIM15_IRQn 0 */
  HAL_TIM_IRQHandler(&htim15);
  /* USER CODE BEGIN TIM15_IRQn 1 */

  /* USER CODE END TIM15_IRQn 1 */
}

/* USER CODE BEGIN 1 */
extern TIM_HandleTypeDef htim13;
extern TIM_HandleTypeDef htim15;
extern uint8_t work_finish; // 0 没结束，1 工作完成
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{if(htim==&htim13){
	HAL_GPIO_WritePin(GPIOH, GPIO_PIN_15, GPIO_PIN_SET);
    HAL_TIM_PWM_Stop(&htim15, TIM_CHANNEL_1);
	
	HAL_TIM_Base_Stop_IT(&htim13);
}
  if (htim == &htim7) // 20ms��ʱ?????
  {
    Tim7Cnt++;
    Tim7_flag_20ms = 1;           // ��һ�ξ�?????20����
    if ((Tim7Cnt & 0x02) == 0x02) // �����ξ�?????40����
    {
      Tim7_flag_40ms = 1;
      // Tim7Cnt = 0; // �����????????
    }
    if ((Tim7Cnt & 0x03) == 0x03) // �����ξ�?????40����
    {
      Tim7_flag_60ms = 1;
      // Tim7Cnt = 0; // �����????????
    }
    if ((Tim7Cnt & 0x05) == 0x05) // �����ξ�?????40����
    {
      Tim7_flag_100ms = 1;
      //Tim7Cnt = 0; // �����????????
    }
		if (Tim7Cnt >= 35) // ÿ50��20ms���ڵ���1��
        {
            Tim7_flag_1s = 1;
            Tim7Cnt = 0; // ���ü�����
        }
  }
  /* Legacy treatment countdown and LVGL calls from ISR disabled for fixture. */
#if 0
  if (htim == &htim14)
  {
    if (seconds == 0)
    {
      if (minutes > 0)
      {

        minutes--;
        seconds = 59;
        switch (work_mode)
        {
        case 1:
          lv_label_set_text_fmt(ui_cut_d_l11, "%02d", minutes); // ��ʾ����
          lv_label_set_text_fmt(ui_cut_d_l12, "%02d", seconds); // ��ʾ����
          break;
        case 2:
          lv_label_set_text_fmt(ui_cut_d_l21, "%02d", minutes); // ��ʾ����
          lv_label_set_text_fmt(ui_cut_d_l22, "%02d", seconds); // ��ʾ����
          break;
        case 3:
          lv_label_set_text_fmt(ui_cut_d_l31, "%02d", minutes); // ��ʾ����
          lv_label_set_text_fmt(ui_cut_d_l32, "%02d", seconds); // ��ʾ����
          break;
        default:
          break;
        }
      }
      else
      {
			 work_finish = 1; // 0 没结束，1 工作完成
      }
    }
    else
    {
      seconds--;
      switch (work_mode)
      {
      case 1:
        lv_label_set_text_fmt(ui_cut_d_l12, "%02d", seconds); // ��ʾ����
        break;
      case 2:
        lv_label_set_text_fmt(ui_cut_d_l22, "%02d", seconds); // ��ʾ����
        break;
      case 3:
        lv_label_set_text_fmt(ui_cut_d_l32, "%02d", seconds); // ��ʾ����
        break;
      default:
        break;
      }
      // lv_label_set_text_fmt(ui_cut_d_l12, "%02d", seconds);
      // HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_14);
    }
  }
#endif
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_7) GT911_RequestScan();
}
 extern int8_t wTransferState;
void LTDC_IRQHandler(void)
{
LTDC->ICR = (uint32_t)LTDC_IER_LIE;
        wTransferState = 1;
}
#if 0
extern volatile uint8_t uart_tx_done;
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {  // 确保是你用的串口
        uart_tx_done = 1;
    }
}
#endif
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == hi2c4.Instance) GT911_RequestRecovery();
}
/* USER CODE END 1 */
