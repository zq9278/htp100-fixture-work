/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    ltdc.c
 * @brief   This file provides code for the configuration
 *          of the LTDC instances.
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
#include "ltdc.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

LTDC_HandleTypeDef hltdc;

/* LTDC init function */
void MX_LTDC_Init(void)
{

  /* USER CODE BEGIN LTDC_Init 0 */

  /* USER CODE END LTDC_Init 0 */

  LTDC_LayerCfgTypeDef pLayerCfg = {0};

  /* USER CODE BEGIN LTDC_Init 1 */

  /* USER CODE END LTDC_Init 1 */
  hltdc.Instance = LTDC;
  hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
  hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
  hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
  hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IIPC;
  hltdc.Init.HorizontalSync = 0;
  hltdc.Init.VerticalSync = 5;
  hltdc.Init.AccumulatedHBP = 30;
  hltdc.Init.AccumulatedVBP = 35;
  hltdc.Init.AccumulatedActiveW = 510;
  hltdc.Init.AccumulatedActiveH = 675;
  hltdc.Init.TotalWidth = 522;
  hltdc.Init.TotalHeigh = 687;
  hltdc.Init.Backcolor.Blue = 0;
  hltdc.Init.Backcolor.Green = 0;
  hltdc.Init.Backcolor.Red = 0;
  if (HAL_LTDC_Init(&hltdc) != HAL_OK)
  {
    Error_Handler();
  }
  pLayerCfg.WindowX0 = 0;
  pLayerCfg.WindowX1 = 480;
  pLayerCfg.WindowY0 = 0;
  pLayerCfg.WindowY1 = 640;
  pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
  pLayerCfg.Alpha = 255;
  pLayerCfg.Alpha0 = 0;
  pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_CA;
  pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_CA;
  pLayerCfg.FBStartAdress = 0xC0000000;
  pLayerCfg.ImageWidth = 480;
  pLayerCfg.ImageHeight = 640;
  pLayerCfg.Backcolor.Blue = 0;
  pLayerCfg.Backcolor.Green = 0;
  pLayerCfg.Backcolor.Red = 0;
  if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LTDC_Init 2 */
  // LTDC->LIPCR =lcdltdc.vsw+lcdltdc.vbp+lcdltdc.pheight-1;//?????????????
  // LTDC->IER |=LTDC_IER_LIE;//??LTDC???
  // MY_NVIC_Init(0,2,LTDC_IRQn,2);
  // LTDC->SRCR |= (1<<1);

  // // ??LTDC??
  // LTDC->IER |= LTDC_IER_LIE;  // Line Interrupt Enable
  // // ???????????
  // LTDC->LIPCR = hltdc.Init.VerticalSync+  hltdc.Init.AccumulatedVBP +pLayerCfg.WindowY1- 1;  // ??DISPLAY_HEIGHT??????
  // // ?NVIC????????
  // NVIC_SetPriority(LTDC_IRQn, 0);  // ??????????
  // // ??????????????
  // LTDC->SRCR = LTDC_SRCR_VBR;  // Vertical Blanking Reload
  /* USER CODE END LTDC_Init 2 */

}

void HAL_LTDC_MspInit(LTDC_HandleTypeDef* ltdcHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(ltdcHandle->Instance==LTDC)
  {
  /* USER CODE BEGIN LTDC_MspInit 0 */

  /* USER CODE END LTDC_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    PeriphClkInitStruct.PLL3.PLL3M = 25;
    PeriphClkInitStruct.PLL3.PLL3N = 400;
    PeriphClkInitStruct.PLL3.PLL3P = 2;
    PeriphClkInitStruct.PLL3.PLL3Q = 2;
    PeriphClkInitStruct.PLL3.PLL3R = 20;
    PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_0;
    PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
    PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    /* LTDC clock enable */
    __HAL_RCC_LTDC_CLK_ENABLE();

    __HAL_RCC_GPIOK_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();
    __HAL_RCC_GPIOJ_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**LTDC GPIO Configuration
    PK5     ------> LTDC_B6
    PG10     ------> LTDC_G3
    PI7     ------> LTDC_B7
    PK4     ------> LTDC_B5
    PJ15     ------> LTDC_B3
    PK3     ------> LTDC_B4
    PK7     ------> LTDC_DE
    PJ14     ------> LTDC_B2
    PJ12     ------> LTDC_B0
    PJ13     ------> LTDC_B1
    PI12     ------> LTDC_HSYNC
    PI13     ------> LTDC_VSYNC
    PI14     ------> LTDC_CLK
    PK2     ------> LTDC_G7
    PK1     ------> LTDC_G6
    PJ9     ------> LTDC_G2
    PJ0     ------> LTDC_R1
    PJ8     ------> LTDC_G1
    PJ7     ------> LTDC_G0
    PJ6     ------> LTDC_R7
    PI15     ------> LTDC_R0
    PJ1     ------> LTDC_R2
    PB10     ------> LTDC_G4
    PB11     ------> LTDC_G5
    PJ5     ------> LTDC_R6
    PJ2     ------> LTDC_R3
    PJ3     ------> LTDC_R4
    PJ4     ------> LTDC_R5
    */
    GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_4|GPIO_PIN_3|GPIO_PIN_7
                          |GPIO_PIN_2|GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_LTDC;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_15|GPIO_PIN_14|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_9|GPIO_PIN_0|GPIO_PIN_8|GPIO_PIN_7
                          |GPIO_PIN_6|GPIO_PIN_1|GPIO_PIN_5|GPIO_PIN_2
                          |GPIO_PIN_3|GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN LTDC_MspInit 1 */
  HAL_LTDC_ProgramLineEvent(&hltdc,5+35+640);
    HAL_NVIC_SetPriority(LTDC_IRQn, 0xE, 0);
    HAL_NVIC_EnableIRQ(LTDC_IRQn);
  /* USER CODE END LTDC_MspInit 1 */
  }
}

void HAL_LTDC_MspDeInit(LTDC_HandleTypeDef* ltdcHandle)
{

  if(ltdcHandle->Instance==LTDC)
  {
  /* USER CODE BEGIN LTDC_MspDeInit 0 */

  /* USER CODE END LTDC_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_LTDC_CLK_DISABLE();

    /**LTDC GPIO Configuration
    PK5     ------> LTDC_B6
    PG10     ------> LTDC_G3
    PI7     ------> LTDC_B7
    PK4     ------> LTDC_B5
    PJ15     ------> LTDC_B3
    PK3     ------> LTDC_B4
    PK7     ------> LTDC_DE
    PJ14     ------> LTDC_B2
    PJ12     ------> LTDC_B0
    PJ13     ------> LTDC_B1
    PI12     ------> LTDC_HSYNC
    PI13     ------> LTDC_VSYNC
    PI14     ------> LTDC_CLK
    PK2     ------> LTDC_G7
    PK1     ------> LTDC_G6
    PJ9     ------> LTDC_G2
    PJ0     ------> LTDC_R1
    PJ8     ------> LTDC_G1
    PJ7     ------> LTDC_G0
    PJ6     ------> LTDC_R7
    PI15     ------> LTDC_R0
    PJ1     ------> LTDC_R2
    PB10     ------> LTDC_G4
    PB11     ------> LTDC_G5
    PJ5     ------> LTDC_R6
    PJ2     ------> LTDC_R3
    PJ3     ------> LTDC_R4
    PJ4     ------> LTDC_R5
    */
    HAL_GPIO_DeInit(GPIOK, GPIO_PIN_5|GPIO_PIN_4|GPIO_PIN_3|GPIO_PIN_7
                          |GPIO_PIN_2|GPIO_PIN_1);

    HAL_GPIO_DeInit(GPIOG, GPIO_PIN_10);

    HAL_GPIO_DeInit(GPIOI, GPIO_PIN_7|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15);

    HAL_GPIO_DeInit(GPIOJ, GPIO_PIN_15|GPIO_PIN_14|GPIO_PIN_12|GPIO_PIN_13
                          |GPIO_PIN_9|GPIO_PIN_0|GPIO_PIN_8|GPIO_PIN_7
                          |GPIO_PIN_6|GPIO_PIN_1|GPIO_PIN_5|GPIO_PIN_2
                          |GPIO_PIN_3|GPIO_PIN_4);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10|GPIO_PIN_11);

  /* USER CODE BEGIN LTDC_MspDeInit 1 */

  /* USER CODE END LTDC_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
void LTDC_Draw_Point(uint16_t x, uint16_t y, uint32_t color)
{

  // ????????????????????????????��???????????????????????
  if ((x < LCD_WIDTH) && (y < LCD_HEIGHT))
  {
    // ????????????????????��??��?????????????????????????????
    uint32_t pixelOffset = (y * LCD_WIDTH) + x;

    // ��???????��???????��????��????��?????????????????????��????????????????
    *((uint32_t *)LCD_FRAME_BUF_ADDR + pixelOffset) = color;
  }
}
void dma2d_transfer_data_r2m(uint32_t *addr, uint32_t xSize, uint32_t ySize, uint32_t offsetLine, uint32_t color)
{
  DMA2D->CR = DMA2D_R2M; // dma2d mode: register to memory.
  DMA2D->OPFCCR = DMA2D_OUTPUT_ARGB8888;

  DMA2D->OCOLR = color;
  DMA2D->OMAR = (uint32_t)addr;
  DMA2D->OOR = offsetLine;
  DMA2D->NLR = (uint32_t)(xSize << 16) | (uint16_t)ySize;

  DMA2D->CR |= DMA2D_CR_START;
  while (DMA2D->CR & DMA2D_CR_START)
    ;
}
 void DMA2D_DrawAlphaBitmap(void  * pDst, 
                                       void  * pSrc, 
                                   uint32_t xSize, 
                                   uint32_t ySize, 
                                   uint32_t OffLineSrc, 
                                   uint32_t OffLineDst, 
                                    uint32_t PixelFormat) 
    {
    /* DMA2D��????��?????��??��??��?????��??��?��?????, ????��??��???????????????????????????????DMA2D??????????????????��????????????????????��??????����???? */  
        DMA2D->CR      = 0x00020000UL | (1 << 9);
        DMA2D->FGMAR   = (uint32_t)pSrc;
        DMA2D->BGMAR   = (uint32_t)pDst;
        DMA2D->OMAR    = (uint32_t)pDst;
        DMA2D->FGOR    = OffLineSrc;
        DMA2D->BGOR    = OffLineDst;
        DMA2D->OOR     = OffLineDst;
        
        /* ?????????��??????????????LTDC_PIXEL_FORMAT_ARGB8888???????????????��???????????????????????????????????��??????????????��????? */
        DMA2D->FGPFCCR = LTDC_PIXEL_FORMAT_RGB565;
        DMA2D->BGPFCCR = PixelFormat;
        DMA2D->OPFCCR  = PixelFormat;
        DMA2D->NLR     = (uint32_t)(xSize << 16) | (uint16_t)ySize;
    
        /* ?????��?????? */
        DMA2D->CR   |= DMA2D_CR_START;   
    
        /* ??????DMA2D???????????? */
        while (DMA2D->CR & DMA2D_CR_START) {} 
    }

  void _DMA2D_Copy(void * pSrc, 
                            void * pDst, 
                                                uint32_t xSize, 
                                                uint32_t ySize, 
                                                uint32_t OffLineSrc, 
                                                uint32_t OffLineDst, 
                                                uint32_t PixelFormat) 
{
 
        /* DMA2D��????��?????��??��??��?????��??��?��????, ????��??��??????????????????????DMA2D?????? */ 
        DMA2D->CR      = 0x00000000UL | (1 << 9);
        DMA2D->FGMAR   = (uint32_t)pSrc;
        DMA2D->OMAR    = (uint32_t)pDst;
        DMA2D->FGOR    = OffLineSrc;
        DMA2D->OOR     = OffLineDst;
         
        /* ????????????????????????��??��????��???RGB565��??????????? */
        DMA2D->FGPFCCR = LTDC_PIXEL_FORMAT_RGB565;
        DMA2D->OPFCCR  = LTDC_PIXEL_FORMAT_RGB565;
         
        DMA2D->NLR     = (uint32_t)(xSize << 16) | (uint16_t)ySize;
 
        /* ?????��?????? */
        DMA2D->CR   |= DMA2D_CR_START;   
 
        /* ??????DMA2D???????????? */
        while (DMA2D->CR & DMA2D_CR_START) {} 
}
/* USER CODE END 1 */

