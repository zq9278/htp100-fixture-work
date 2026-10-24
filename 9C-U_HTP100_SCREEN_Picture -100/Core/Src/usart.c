/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
#include "usart.h"

/* USER CODE BEGIN 0 */
#include <stdio.h>
#include "screen.h"

#if 0

RingQueue uartRingQueue = { .head = 0, .tail = 0, .count = 0 }; // 初始化队�????
AXI_SRAM UART_RX_TypeDef uart_rx_data_t[UART_BUFFER_QUANTITY]; // 双缓冲数据存�????
volatile uint8_t uart_buff_ctrl = 0; // 缓冲区索�????
#endif

#define UART_RX_STREAM_SIZE 512U

/* DMA owns this buffer while reception is active. Keep it cache-line aligned
 * so cache maintenance cannot touch unrelated application data. */
static AXI_SRAM __attribute__((aligned(32))) uint8_t uart_rx_dma[UART_RX_BUFFER_SIZE];
static uint8_t uart_rx_stream[UART_RX_STREAM_SIZE];
static volatile uint16_t uart_rx_write;
static volatile uint16_t uart_rx_read;
static volatile uint32_t uart_rx_overflow_count;
static volatile uint32_t uart_rx_restart_error_count;

static void uart_rx_dma_invalidate(void)
{
  SCB_InvalidateDCache_by_Addr((uint32_t *)uart_rx_dma,
                              (int32_t)sizeof(uart_rx_dma));
}

static HAL_StatusTypeDef screen_uart_start_receive(void)
{
  HAL_StatusTypeDef status;

  uart_rx_dma_invalidate();
  status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_dma,
                                       sizeof(uart_rx_dma));
  if (status == HAL_OK) {
    /* RxEvent must represent IDLE or a full buffer. Treating the 128-byte
     * half-transfer callback as a completed block corrupts buffer ownership. */
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
  }
  return status;
}

static void screen_uart_store_from_isr(const uint8_t *data, uint16_t length)
{
  for (uint16_t index = 0U; index < length; ++index) {
    uint16_t next = (uint16_t)((uart_rx_write + 1U) % UART_RX_STREAM_SIZE);
    if (next == uart_rx_read) {
      uart_rx_overflow_count++;
      break;
    }
    uart_rx_stream[uart_rx_write] = data[index];
    uart_rx_write = next;
  }
}

size_t ScreenUart_Read(uint8_t *destination, size_t capacity)
{
  size_t count = 0U;

  while (destination != NULL && count < capacity &&
         uart_rx_read != uart_rx_write) {
    destination[count++] = uart_rx_stream[uart_rx_read];
    uart_rx_read = (uint16_t)((uart_rx_read + 1U) % UART_RX_STREAM_SIZE);
  }
  return count;
}
/* USER CODE END 0 */

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */
#if 0
  SCB_CleanInvalidateDCache();
  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_data_t[uart_buff_ctrl].buffer, UART_RX_BUFFER_SIZE); // �????启DMA接收
    // 启动空闲中断接收 (可�?�，增强接收能力)
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
#endif
  uart_rx_write = 0U;
  uart_rx_read = 0U;
  uart_rx_overflow_count = 0U;
  uart_rx_restart_error_count = 0U;
  if (screen_uart_start_receive() != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE END USART1_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA10     ------> USART1_RX
    PA9     ------> USART1_TX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1 DMA Init */
    /* USART1_RX Init */
    hdma_usart1_rx.Instance = DMA1_Stream0;
    hdma_usart1_rx.Init.Request = DMA_REQUEST_USART1_RX;
    hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_rx.Init.Mode = DMA_NORMAL;
    hdma_usart1_rx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmarx,hdma_usart1_rx);

    /* USART1_TX Init */
    hdma_usart1_tx.Instance = DMA1_Stream1;
    hdma_usart1_tx.Init.Request = DMA_REQUEST_USART1_TX;
    hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmatx,hdma_usart1_tx);

    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspInit 1 */
  /* USER CODE END USART1_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA10     ------> USART1_RX
    PA9     ------> USART1_TX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_10|GPIO_PIN_9);

    /* USART1 DMA DeInit */
    HAL_DMA_DeInit(uartHandle->hdmarx);
    HAL_DMA_DeInit(uartHandle->hdmatx);

    /* USART1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
// int fputc(int ch, FILE *f)
// {
// 	HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 100);	// ��?????��???????
// 	return (ch);
// }
#if 0
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
   if (huart->Instance == USART1) { // 确保�???? USART1
        UART_RX_TypeDef *completed_data = NULL;
        if (Size > 0 && Size <= UART_RX_BUFFER_SIZE) {
            uart_rx_data_t[uart_buff_ctrl].size = Size; // 存储接收数据长度
            UART_RX_TypeDef *pUartData = &uart_rx_data_t[uart_buff_ctrl]; // 取当前缓冲区数据
            completed_data = pUartData;
            RingQueue_Push(&uartRingQueue, pUartData); // 数据存入环形队列
            uart_buff_ctrl = (uart_buff_ctrl + 1) % UART_BUFFER_QUANTITY; // 切换到下�????个缓冲区
        }
				SCB_CleanInvalidateDCache();
        if (completed_data != NULL) {
            screen_preload_language_from_uart(completed_data->buffer,
                                              completed_data->size);
        }
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_data_t[uart_buff_ctrl].buffer, UART_RX_BUFFER_SIZE); // 重新启动DMA接收
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
      HAL_UART_Abort(huart);
			SCB_CleanInvalidateDCache();
       HAL_UARTEx_ReceiveToIdle_DMA(&huart1, uart_rx_data_t[uart_buff_ctrl].buffer, UART_RX_BUFFER_SIZE); // �????启DMA接收
       HAL_GPIO_WritePin(GPIOH, GPIO_PIN_14, GPIO_PIN_RESET);
    }
}
#endif

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  if (huart->Instance != USART1) {
    return;
  }

  /* A half-transfer event does not release the DMA buffer. It is disabled
   * when reception starts, but retain this guard for defensive recovery. */
  if (HAL_UARTEx_GetRxEventType(huart) == HAL_UART_RXEVENT_HT) {
    return;
  }

  if (Size > 0U && Size <= sizeof(uart_rx_dma)) {
    uart_rx_dma_invalidate();
    /* Fixture parser runs exclusively in the foreground loop. */
    screen_uart_store_from_isr(uart_rx_dma, Size);
  }

  if (screen_uart_start_receive() != HAL_OK) {
    uart_rx_restart_error_count++;
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {
    (void)HAL_UART_AbortReceive(huart);
    if (screen_uart_start_receive() != HAL_OK) {
      uart_rx_restart_error_count++;
    }
  }
}

int fputc(int ch,FILE *f)//重定向printf
{
   HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
  return ch;
}

/* USER CODE END 1 */

