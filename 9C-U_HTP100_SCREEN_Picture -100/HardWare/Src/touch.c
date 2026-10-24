/*
 * @Author: zhangqi zq9278@gmail.com
 * @Date: 2024-04-05 15:06:57
 * @LastEditors: zhangqi zq9278@gmail.com
 * @LastEditTime: 2024-05-22 15:52:55
 * @FilePath: \keil_lvgl\HardWare\Src\touch.c
 * @Description: 
 * 
 * Copyright (c) 2024 by ${git_name_email}, All Rights Reserved. 
 */


#include "touch.h"
#include "tim.h"
#include <string.h>

#define TOUCH_IO_TIMEOUT_MS 20U
static volatile uint8_t touch_scan_pending;
static volatile uint8_t touch_recovery_pending;
volatile uint32_t touch_io_error_count;

extern I2C_HandleTypeDef hi2c4;

uint8_t Touch_state; // 触摸状态标志

const uint8_t GT9147_CFG_TBL[] =
	{
		0x44, 0xE0, 0x01, 0x80, 0x02, 0x05, 0x35, 0x00, 0x02, 0x08,
		0x1E, 0x08, 0x50, 0x3C, 0x0F, 0x05, 0x00, 0x00, 0x00, 0x00,
		0x50, 0x00, 0x00, 0x18, 0x1A, 0x1E, 0x14, 0x87, 0x27, 0x0A,
		0x4B, 0x4D, 0xD3, 0x07, 0x00, 0x00, 0x00, 0x02, 0x32, 0x1C,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00,
		0x2A, 0x32, 0x64, 0x94, 0xD5, 0x02, 0x07, 0x00, 0x00, 0x04,
		0xA5, 0x35, 0x00, 0x91, 0x3D, 0x00, 0x80, 0x46, 0x00, 0x70,
		0x51, 0x00, 0x63, 0x5D, 0x00, 0x63, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x14, 0x12, 0x10, 0x0E, 0x0C, 0x0A, 0x08, 0x06,
		0x04, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x24,
		0x22, 0x21, 0x20, 0x1F, 0x1E, 0x1D, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0x1E, 0x01};



Touch_Struct User_Touch;
/**
 * @description: 初始化gt911配置
 * @return {*}
 * @Author: zhangqi zq9278@gmail.com
 */
#include <stdint.h>
#include <stdio.h>

#define GT911_CONFIG_REG  0x8047  // GT911 配置寄存器
#define GT911_CONTROL_REG 0x8040  // GT911 控制寄存器

/**
 * @brief  设置 GT911 触摸中断模式
 * @param  mode: 触摸屏的中断模式
 *         - 0: 查询模式（Polling Mode）
 *         - 1: 下降沿触发（Falling Edge Triggered Mode）
 *         - 2: 上升沿触发（Rising Edge Triggered Mode）
 *         - 3: 电平触发（Level Triggered Mode）
 * @return 0 成功，-1 失败
 *
 * -----------------------------------------
 * GT911 触摸模式总结:
 *
 * 模式 0: 查询模式（Polling Mode）
 *    - INT 引脚始终低电平，MCU 需要轮询查询触摸状态。
 *    - 适用于 MCU **不支持外部中断** 或 **需要定期轮询的应用**。
 *
 * 模式 1: 下降沿触发模式（Falling Edge Mode）
 *    - 触摸发生时，INT 产生下降沿（高→低），触发 EXTI 中断。
 *    - 适用于 **GPIO_MODE_IT_FALLING** 配置的中断。
 *    - 适用于 **触摸时 INT 变低，抬起时变高** 的情况。
 *
 * 模式 2: 上升沿触发模式（Rising Edge Mode）
 *    - 触摸发生时，INT 产生上升沿（低→高），触发 EXTI 中断。
 *    - 适用于 **GPIO_MODE_IT_RISING** 配置的中断。
 *    - 适用于 **触摸时 INT 变高，抬起时变低** 的情况。
 *
 * 模式 3: 电平触发模式（Level Triggered Mode）
 *    - 触摸时，INT **一直保持低电平**，直到 MCU 读取数据并清除 `0x814E`。
 *    - 适用于低功耗模式，MCU 可以 **轮询** 或 **使用 EXTI 低电平触发**。
 *    - 使用 **GPIO_MODE_IT_LOW** 配置 EXTI。
 *
 * -----------------------------------------
 */
int GT911_SetInterruptMode(uint8_t mode)
{
    uint8_t config[186];  // GT911 配置数据
    uint8_t checksum = 0;

    // 1. 读取 GT911 配置寄存器（0x8047）
    GT911_ReadReg(GT911_CONFIG_REG, config, sizeof(config));

    // 2. 设置 INT 触发模式
    if (mode == 0) {
        config[2] &= ~0x08;  // 查询模式（Polling Mode），清除 Bit3
    } else if (mode == 1 || mode == 2 || mode == 3) {
        config[2] |= 0x08;  // 中断模式（Edge 或 Level Mode），设置 Bit3 = 1
    } else {
       // printf("无效的 GT911 触摸模式\n");
        return -1;
    }

    // 3. 重新计算校验和
    for (int i = 0; i < sizeof(config) - 1; i++) {
        checksum += config[i];
    }
    checksum = (~checksum) + 1;  // 计算新的校验和
    config[sizeof(config) - 1] = checksum;  // 写入校验和

    // 4. 写回修改后的配置
    GT911_WriteReg(GT911_CONFIG_REG, config, sizeof(config));

    // 5. 让 GT911 重新加载配置
    uint8_t reload = 0x01;
    GT911_WriteReg(GT911_CONTROL_REG, &reload, 1);

    //printf("GT911 触摸模式设置为 %d\n", mode);
    return 0;
}
void GT911_Software_Reset(void)
{
    uint8_t temp = 2;
    GT911_WriteReg(GT_CTRL_REG, &temp, 1);  // 复位
    HAL_Delay(10);
    
    temp = 0;
    GT911_WriteReg(GT_CTRL_REG, &temp, 1);
    HAL_Delay(10);

    //printf("✔️ GT911 软件复位完成\n");
}
 // 初始化 GT911 时，控制 INT 引脚 PG7
void GT911_INT_LOW_and_OUTPUT(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_7, GPIO_PIN_RESET); // 拉低 INT
}

void GT911_INT_INPUT_MODE(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct); // 设置为输入模式
}
void GT911_Init(void)
{
	
	 uint8_t mode = 1;
    uint8_t config[186];  // GT911 配置数据

	GT911_INT_LOW_and_OUTPUT();
    // 1. 复位 GT911
    GT_RESET_LOW();  
    HAL_Delay(20);
    GT_RESET_HIGH();
    HAL_Delay(60);
	GT911_INT_INPUT_MODE();
	GT911_Software_Reset();
	HAL_Delay(50);
	uint8_t clear = 0;
	GT911_WriteReg(0x814E, &clear, 1);
	GT911_WriteReg(0x814E, &clear, 1);
	GT911_WriteReg(0x814E, &clear, 1);
	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 7, 0);
HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

 //GT911_SetInterruptMode(3);
}


static HAL_StatusTypeDef touch_io_result(HAL_StatusTypeDef status)
{
    if (status != HAL_OK) {
        touch_io_error_count++;
        touch_recovery_pending = 1U;
    }
    return status;
}

HAL_StatusTypeDef GT911_WriteReg(uint16_t reg, uint8_t *buffer, uint8_t size)
{
    return touch_io_result(HAL_I2C_Mem_Write(&hi2c4, GT911_DIV_W, reg,
                             I2C_MEMADD_SIZE_16BIT, buffer, size, TOUCH_IO_TIMEOUT_MS));
}

HAL_StatusTypeDef GT911_ReadReg(uint16_t reg, uint8_t *buffer, uint8_t size)
{
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c4, GT911_DIV_R, reg,
                             I2C_MEMADD_SIZE_16BIT, buffer, size, TOUCH_IO_TIMEOUT_MS);
    if (status != HAL_OK) memset(buffer, 0, size);
    return touch_io_result(status);
}

void GT911_RequestScan(void) { touch_scan_pending = 1U; }
void GT911_RequestRecovery(void) { touch_recovery_pending = 1U; }

/* Main-loop only: keep blocking I2C and recovery out of EXTI callbacks. */
void GT911_Process(void)
{
    static uint32_t last_poll;
    static uint32_t last_recovery;
    uint32_t now = HAL_GetTick();
    if (touch_recovery_pending) {
        User_Touch.Touch_State = 0;
        User_Touch.Touch_Number = 0;
        if (now - last_recovery < 1000U) return;
        last_recovery = now;
        touch_recovery_pending = 0U;
        /* Reinitialize the actual touch bus (I2C4, not I2C1). */
        if (HAL_I2C_DeInit(&hi2c4) != HAL_OK || HAL_I2C_Init(&hi2c4) != HAL_OK) {
            touch_recovery_pending = 1U;
            return;
        }
        GT911_Init();
        if (touch_recovery_pending) return;
    }
    /* Poll as a fallback for edges missed during startup or noise. */
    if (!touch_scan_pending && now - last_poll < 50U) return;
    touch_scan_pending = 0U;
    last_poll = now;
    gt911_Scanf();
}

/**
 * @description: 跟新gt911配置文件
 * @return {*}
 * @Author: zhangqi zq9278@gmail.com
 */

void GT911_Send_Config(uint8_t mode)
{
	uint8_t buf[2];

	buf[0] = 0;
	buf[1] = mode; // �Ƿ�д�뵽GT9147 FLASH?  ���Ƿ���籣��
	for (uint8_t i = 0; i < sizeof(GT9147_CFG_TBL); i++)
	{
		buf[0] += GT9147_CFG_TBL[i]; // ����У���
	}
	buf[0] = (~buf[0]) + 1;

	GT911_WriteReg(GT_CFGS_REG, (uint8_t *)GT9147_CFG_TBL, sizeof(GT9147_CFG_TBL)); // ���ͼĴ�������

	GT911_WriteReg(GT_CHECK_REG, buf, 2); // д��У���,�����ø��±��
}

/**
 * @description: gt911软复位
 * @return {*}
 * @Author: zhangqi zq9278@gmail.com
 */

void Software_Reset(uint8_t gt_SR_type)
{
	uint8_t _temp = 0; // �м����
	if (gt_SR_type)
	{
		_temp = 2;
		GT911_WriteReg(GT_CTRL_REG, &_temp, 1);
	}
	else
	{
		_temp = 0;
		GT911_WriteReg(GT_CTRL_REG, &_temp, 1);
	}
}

/**
 * @description: 扫描触摸屏
 * @return {*}
 * @Author: zhangqi zq9278@gmail.com
 */


void gt911_Scanf(void)
{
    uint8_t status;
    uint8_t clear = 0;
    uint8_t points[40];
    if (GT911_ReadReg(GT_GSTID_REG, &status, 1) != HAL_OK) goto failed;
    /* Keep a held touch if the controller has no new sample yet. */
    if ((status & 0x80U) == 0U) return;
    uint8_t count = status & 0x0FU;
    if (count > 5U) goto failed;
    if (count != 0U &&
        GT911_ReadReg(GT_TPD_Sta, points, count * 8U) != HAL_OK) goto failed;
    if (GT911_WriteReg(GT_GSTID_REG, &clear, 1) != HAL_OK) goto failed;
    for (uint8_t i = 0U; i < count; ++i) {
        const uint8_t *p = &points[i * 8U];
        User_Touch.Touch_XY[i].X_Point = p[X_L] | ((uint16_t)p[X_H] << 8);
        User_Touch.Touch_XY[i].Y_Point = p[Y_L] | ((uint16_t)p[Y_H] << 8);
        User_Touch.Touch_XY[i].S_Point = p[S_L] | ((uint16_t)p[S_H] << 8);
    }
    Touch_state = status;
    User_Touch.Touch_Number = count;
    User_Touch.Touch_State = count != 0U;
    return;
failed:
    User_Touch.Touch_Number = 0;
    User_Touch.Touch_State = 0;
    touch_recovery_pending = 1U;
}
