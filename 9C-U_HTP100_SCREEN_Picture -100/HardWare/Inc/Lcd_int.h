//
// Created by zhangqi on 2024/3/5.
//
#include "main.h"
#ifndef H750_CLION_LCD_INT_H
#define H750_CLION_LCD_INT_H

#endif //H750_CLION_LCD_INT_H
void SPI_SendData(uint8_t cmd);

// 向LCD写入命令
void LCD_WriteCommand(uint8_t cmd) ;

// 向LCD写入数据
void LCD_WriteData(uint8_t data);

// LCD初始化函数
void LCD_Init(void);