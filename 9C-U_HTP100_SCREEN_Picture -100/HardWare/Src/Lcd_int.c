//
// Created by zhangqi on 2024/3/5.
//

#include "../Inc/Lcd_int.h"
// extern SPI_HandleTypeDef hspi1;
//  #define LCD_CS_LOW()     HAL_GPIO_WritePin(GPIOH, GPIO_PIN_13, GPIO_PIN_RESET) // CS置低
//  #define LCD_CS_HIGH()    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_13, GPIO_PIN_SET)   // CS置高
//  #define LCD_RESET_LOW()  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_8, GPIO_PIN_RESET) // RESET置低
//  #define LCD_RESET_HIGH() HAL_GPIO_WritePin(GPIOH, GPIO_PIN_8, GPIO_PIN_SET)   // RESET置高
// #define LCD_DC_COMMAND() HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET) // DC置低，发送命令
// #define LCD_DC_DATA()    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET)   // DC置高，发送数据

// 假设以下宏定义了对应的GPIO操作
#define CSB_LOW() HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_RESET)
#define CSB_HIGH() HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, GPIO_PIN_SET)
#define SCLB_LOW() HAL_GPIO_WritePin(GPIOH, GPIO_PIN_4, GPIO_PIN_RESET)
#define SCLB_HIGH() HAL_GPIO_WritePin(GPIOH, GPIO_PIN_4, GPIO_PIN_SET)
#define SDI_LOW() HAL_GPIO_WritePin(GPIOH, GPIO_PIN_5, GPIO_PIN_RESET)
#define SDI_HIGH() HAL_GPIO_WritePin(GPIOH, GPIO_PIN_5, GPIO_PIN_SET)
#define LCD_RESET_LOW() HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET) // RESET置低
#define LCD_RESET_HIGH() HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_SET)  // RESET置高

// // SPI发送单字节数据
// void LCD_SPI_Send(uint8_t data) {
//     data &= 0xFF;
//     HAL_SPI_Transmit(&hspi1, (const uint8_t *) &data, 1, HAL_MAX_HAL_Delay);
// }

// // 向LCD写入命令
// void LCD_WriteCommand(uint8_t cmd) {
//     // LCD_DC_COMMAND(); // 设置为命令模式
//     uint16_t frame = 0x0000 | cmd;  // 保证命令的最高位为0
//     LCD_CS_LOW();     // 激活LCD
//     LCD_SPI_Send(frame); // 通过SPI发送命令
//     LCD_CS_HIGH();    // 取消激活LCD
// }

// // 向LCD写入数据
// void LCD_WriteData(uint8_t data) {
//     //LCD_DC_DATA();    // 设置为数据模式
//     uint16_t frame = 0x0100 | data; // 保证数据的最高位为1
//     LCD_CS_LOW();     // 激活LCD
//     LCD_SPI_Send(frame); // 通过SPI发送数据
//     LCD_CS_HIGH();    // 取消激活LCD
// }

void SPI_SendData(uint8_t cmd)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        if (cmd & 0x80)
            SDI_HIGH();
        else
            SDI_LOW();
        cmd <<= 1;
        SCLB_LOW(); //_nop_(); _nop_();_nop_();_nop_();
        __NOP();
        SCLB_HIGH(); //_nop_();_nop_();_nop_();_nop_();
    }
}

void LCD_WriteCommand(uint8_t cmd)
{
    CSB_LOW(); // 激活LCD
    SDI_LOW(); // 如果是0，则设置数据线低电平

    SCLB_LOW(); // 拉低时钟
    __NOP();
    SCLB_HIGH(); // 拉高时钟以采样数据
    SPI_SendData(cmd);
    CSB_HIGH(); // 取消LCD激活
}

void LCD_WriteData(uint8_t data)
{
    CSB_LOW();  // 激活LCD
    SDI_HIGH(); // 如果是0，则设置数据线低电平

    SCLB_LOW(); // 拉低时钟
    __NOP();
    SCLB_HIGH(); // 拉高时钟以采样数据
    SPI_SendData(data);
    CSB_HIGH(); // 取消LCD激活
}
#ifndef SCREEN_PANEL_OLD
#define SCREEN_PANEL_OLD 0
#endif
#if SCREEN_PANEL_OLD != 0 && SCREEN_PANEL_OLD != 1
#error "SCREEN_PANEL_OLD must be 0 or 1"
#endif
// LCD初始化函数`
void LCD_Init(void)
{
	#if SCREEN_PANEL_OLD
    // 复位LCD
    LCD_RESET_HIGH(); // 复位引脚置高
    HAL_Delay(1);     // 延时5ms
    // HAL_Delay(5);     // 延时5ms
    LCD_RESET_LOW();  // 复位引脚置低，开始复位
    HAL_Delay(10);    // 延时10ms
                      //     HAL_Delay(1);    // 延时10ms
    LCD_RESET_HIGH(); // 复位引脚置高，结束复位
    HAL_Delay(120);   // 延时120ms等待LCD准备好
    //     HAL_Delay(1);   // 延时120ms等待LCD准备好

    // 发送初始化命令序列
    // 进入命令设置模式
    //***************************************************************//LCD SETING

    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x13);

    LCD_WriteCommand(0xEF);
    LCD_WriteData(0x08);

    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x10);

    LCD_WriteCommand(0xC0);
    LCD_WriteData(0x4F);
    LCD_WriteData(0x00);

    LCD_WriteCommand(0xC1);
    LCD_WriteData(0x10);
    LCD_WriteData(0x02);

    LCD_WriteCommand(0xC2);
    LCD_WriteData(0x07);
    LCD_WriteData(0x02);

    LCD_WriteCommand(0xCC);
    LCD_WriteData(0x10);

    LCD_WriteCommand(0xB0);
    LCD_WriteData(0x00);
    LCD_WriteData(0x10);
    LCD_WriteData(0x17);
    LCD_WriteData(0x0D);
    LCD_WriteData(0x11);
    LCD_WriteData(0x06);
    LCD_WriteData(0x05);
    LCD_WriteData(0x08);
    LCD_WriteData(0x07);
    LCD_WriteData(0x1F);
    LCD_WriteData(0x04);
    LCD_WriteData(0x11);
    LCD_WriteData(0x0E);
    LCD_WriteData(0x29);
    LCD_WriteData(0x30);
    LCD_WriteData(0x1F);

    LCD_WriteCommand(0xB1);
    LCD_WriteData(0x00);
    LCD_WriteData(0x0D);
    LCD_WriteData(0x14);
    LCD_WriteData(0x0E);
    LCD_WriteData(0x11);
    LCD_WriteData(0x06);
    LCD_WriteData(0x04);
    LCD_WriteData(0x08);
    LCD_WriteData(0x08);
    LCD_WriteData(0x20);
    LCD_WriteData(0x05);
    LCD_WriteData(0x13);
    LCD_WriteData(0x13);
    LCD_WriteData(0x26);
    LCD_WriteData(0x30);
    LCD_WriteData(0x1F);

    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x11);
    LCD_WriteCommand(0xB0);
    LCD_WriteData(0x65);

    LCD_WriteCommand(0xB1);
    LCD_WriteData(0x71);

    LCD_WriteCommand(0xB2);
    LCD_WriteData(0x87);

    LCD_WriteCommand(0xB3);
    LCD_WriteData(0x80);

    LCD_WriteCommand(0xB5);
    LCD_WriteData(0x4D);

    LCD_WriteCommand(0xB7);
    LCD_WriteData(0x85);

    LCD_WriteCommand(0xB8);
    LCD_WriteData(0x20);

    LCD_WriteCommand(0xC1);
    LCD_WriteData(0x78);

    LCD_WriteCommand(0xC2);
    LCD_WriteData(0x78);

    LCD_WriteCommand(0xD0);
    LCD_WriteData(0x88);

    LCD_WriteCommand(0xEE);
    LCD_WriteData(0x42);

    LCD_WriteCommand(0xE0);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x02);

    LCD_WriteCommand(0xE1);
    LCD_WriteData(0x04);
    LCD_WriteData(0xA0);
    LCD_WriteData(0x06);
    LCD_WriteData(0xA0);
    LCD_WriteData(0x05);
    LCD_WriteData(0xA0);
    LCD_WriteData(0x07);
    LCD_WriteData(0xA0);
    LCD_WriteData(0x00);
    LCD_WriteData(0x44);
    LCD_WriteData(0x44);

    LCD_WriteCommand(0xE2);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);

    LCD_WriteCommand(0xE3);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x22);
    LCD_WriteData(0x22);

    LCD_WriteCommand(0xE4);
    LCD_WriteData(0x44);
    LCD_WriteData(0x44);

    LCD_WriteCommand(0xE5);
    LCD_WriteData(0x0C);
    LCD_WriteData(0x90);
    LCD_WriteData(0xA0);
    LCD_WriteData(0xA0);
    LCD_WriteData(0x0E);
    LCD_WriteData(0x92);
    LCD_WriteData(0xA0);
    LCD_WriteData(0xA0);
    LCD_WriteData(0x08);
    LCD_WriteData(0x8C);
    LCD_WriteData(0xA0);
    LCD_WriteData(0xA0);
    LCD_WriteData(0x0A);
    LCD_WriteData(0x8E);
    LCD_WriteData(0xA0);
    LCD_WriteData(0xA0);

    LCD_WriteCommand(0xE6);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x22);
    LCD_WriteData(0x22);

    LCD_WriteCommand(0xE7);
    LCD_WriteData(0x44);
    LCD_WriteData(0x44);

    LCD_WriteCommand(0xE8);
    LCD_WriteData(0x0D);
    LCD_WriteData(0x91);
    LCD_WriteData(0xA0);
    LCD_WriteData(0xA0);
    LCD_WriteData(0x0F);
    LCD_WriteData(0x93);
    LCD_WriteData(0xA0);
    LCD_WriteData(0xA0);
    LCD_WriteData(0x09);
    LCD_WriteData(0x8D);
    LCD_WriteData(0xA0);
    LCD_WriteData(0xA0);
    LCD_WriteData(0x0B);
    LCD_WriteData(0x8F);
    LCD_WriteData(0xA0);
    LCD_WriteData(0xA0);

    LCD_WriteCommand(0xEB);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0xE4);
    LCD_WriteData(0xE4);
    LCD_WriteData(0x44);
    LCD_WriteData(0x00);
    LCD_WriteData(0x40);

    LCD_WriteCommand(0xED);
    LCD_WriteData(0xFF);
    LCD_WriteData(0xF5);
    LCD_WriteData(0x47);
    LCD_WriteData(0x6F);
    LCD_WriteData(0x0B);
    LCD_WriteData(0xA1);
    LCD_WriteData(0xAB);
    LCD_WriteData(0xFF);
    LCD_WriteData(0xFF);
    LCD_WriteData(0xBA);
    LCD_WriteData(0x1A);
    LCD_WriteData(0xB0);
    LCD_WriteData(0xF6);
    LCD_WriteData(0x74);
    LCD_WriteData(0x5F);
    LCD_WriteData(0xFF);

    LCD_WriteCommand(0xEF);
    LCD_WriteData(0x08);
    LCD_WriteData(0x08);
    LCD_WriteData(0x08);
    LCD_WriteData(0x45);
    LCD_WriteData(0x3F);
    LCD_WriteData(0x54);

    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);

    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x13);

    LCD_WriteCommand(0xE6);
    LCD_WriteData(0x16);

    LCD_WriteCommand(0xE8);
    LCD_WriteData(0x00);
    LCD_WriteData(0x0E);

    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);

    LCD_WriteCommand(0x11);
    HAL_Delay(120);
    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x13);

    LCD_WriteCommand(0xE8);
    LCD_WriteData(0x00);
    LCD_WriteData(0x0C);
    HAL_Delay(10);
    LCD_WriteCommand(0xE8);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);

    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);

    LCD_WriteCommand(0x29);

    LCD_WriteCommand(0x3A);
    LCD_WriteData(0x77);
    LCD_WriteCommand(0x29);

    LCD_WriteCommand(0x36);
    LCD_WriteData(0x08);
#endif
   // Vendor-provided init sequence (new panel)

   #if !SCREEN_PANEL_OLD
    LCD_RESET_HIGH();
    HAL_Delay(10);
    LCD_RESET_LOW();
    HAL_Delay(80);
    LCD_RESET_HIGH();
    HAL_Delay(80);

    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x13);

    LCD_WriteCommand(0xEF);
    LCD_WriteData(0x08);

    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x10);

    LCD_WriteCommand(0xC0);
    LCD_WriteData(0x4F);
    LCD_WriteData(0x00);

    LCD_WriteCommand(0xC1);
    LCD_WriteData(0x0C);
    LCD_WriteData(0x08);

    LCD_WriteCommand(0xC2);
    LCD_WriteData(0x17);
    LCD_WriteData(0x06);

    LCD_WriteCommand(0xC6);
    LCD_WriteData(0x80);

    LCD_WriteCommand(0xCC);
    LCD_WriteData(0x18);

    LCD_WriteCommand(0xB0);
    LCD_WriteData(0x40);
    LCD_WriteData(0x0D);
    LCD_WriteData(0x53);
    LCD_WriteData(0x07);
    LCD_WriteData(0x0D);
    LCD_WriteData(0x06);
    LCD_WriteData(0x08);
    LCD_WriteData(0x06);
    LCD_WriteData(0x08);
    LCD_WriteData(0x24);
    LCD_WriteData(0x03);
    LCD_WriteData(0x13);
    LCD_WriteData(0x11);
    LCD_WriteData(0x68);
    LCD_WriteData(0x2D);
    LCD_WriteData(0xD7);

    LCD_WriteCommand(0xB1);
    LCD_WriteData(0x40);
    LCD_WriteData(0x1D);
    LCD_WriteData(0x66);
    LCD_WriteData(0x14);
    LCD_WriteData(0x16);
    LCD_WriteData(0x07);
    LCD_WriteData(0x0C);
    LCD_WriteData(0x09);
    LCD_WriteData(0x07);
    LCD_WriteData(0x26);
    LCD_WriteData(0x06);
    LCD_WriteData(0x12);
    LCD_WriteData(0x10);
    LCD_WriteData(0x6B);
    LCD_WriteData(0x30);
    LCD_WriteData(0xD7);

    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x11);

    LCD_WriteCommand(0xB0);
    LCD_WriteData(0x4D);

    LCD_WriteCommand(0xB1);
    LCD_WriteData(0x80);

    LCD_WriteCommand(0xB2);
    LCD_WriteData(0x89);

    LCD_WriteCommand(0xB3);
    LCD_WriteData(0x80);

    LCD_WriteCommand(0xB5);
    LCD_WriteData(0x47);

    LCD_WriteCommand(0xB7);
    LCD_WriteData(0x85);

    LCD_WriteCommand(0xB8);
    LCD_WriteData(0x33);

    LCD_WriteCommand(0xC0);
    LCD_WriteData(0x09);

    LCD_WriteCommand(0xC1);
    LCD_WriteData(0x08);

    LCD_WriteCommand(0xC2);
    LCD_WriteData(0x08);

    LCD_WriteCommand(0xD0);
    LCD_WriteData(0x88);

    HAL_Delay(100);

    LCD_WriteCommand(0xE0);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x02);

    LCD_WriteCommand(0xE1);
    LCD_WriteData(0x07);
    LCD_WriteData(0xC0);
    LCD_WriteData(0x07);
    LCD_WriteData(0xC0);
    LCD_WriteData(0x06);
    LCD_WriteData(0xC0);
    LCD_WriteData(0x06);
    LCD_WriteData(0xC0);
    LCD_WriteData(0x00);
    LCD_WriteData(0x24);
    LCD_WriteData(0x24);

    LCD_WriteCommand(0xE2);
    LCD_WriteData(0x20);
    LCD_WriteData(0x20);
    LCD_WriteData(0x22);
    LCD_WriteData(0x22);
    LCD_WriteData(0x8D);
    LCD_WriteData(0xC0);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x8C);
    LCD_WriteData(0xC0);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);

    LCD_WriteCommand(0xE3);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x22);
    LCD_WriteData(0x22);

    LCD_WriteCommand(0xE4);
    LCD_WriteData(0x44);
    LCD_WriteData(0x44);

    LCD_WriteCommand(0xE5);
    LCD_WriteData(0x09);
    LCD_WriteData(0x8D);
    LCD_WriteData(0x26);
    LCD_WriteData(0xFA);
    LCD_WriteData(0x0B);
    LCD_WriteData(0x8F);
    LCD_WriteData(0x26);
    LCD_WriteData(0xFA);
    LCD_WriteData(0x0D);
    LCD_WriteData(0x89);
    LCD_WriteData(0x26);
    LCD_WriteData(0xFA);
    LCD_WriteData(0x0F);
    LCD_WriteData(0x8B);
    LCD_WriteData(0x26);
    LCD_WriteData(0xFA);

    LCD_WriteCommand(0xE6);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x22);
    LCD_WriteData(0x22);

    LCD_WriteCommand(0xE7);
    LCD_WriteData(0x44);
    LCD_WriteData(0x44);

    LCD_WriteCommand(0xE8);
    LCD_WriteData(0x08);
    LCD_WriteData(0x8C);
    LCD_WriteData(0x26);
    LCD_WriteData(0xFA);
    LCD_WriteData(0x0A);
    LCD_WriteData(0x8E);
    LCD_WriteData(0x26);
    LCD_WriteData(0xFA);
    LCD_WriteData(0x0C);
    LCD_WriteData(0x88);
    LCD_WriteData(0x26);
    LCD_WriteData(0xFA);
    LCD_WriteData(0x0E);
    LCD_WriteData(0x8A);
    LCD_WriteData(0x26);
    LCD_WriteData(0xFA);

    LCD_WriteCommand(0xEB);
    LCD_WriteData(0x02);
    LCD_WriteData(0x01);
    LCD_WriteData(0xE4);
    LCD_WriteData(0xE4);
    LCD_WriteData(0x88);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);

    LCD_WriteCommand(0xED);
    LCD_WriteData(0xFF);
    LCD_WriteData(0x04);
    LCD_WriteData(0x56);
    LCD_WriteData(0x7F);
    LCD_WriteData(0xBA);
    LCD_WriteData(0x2F);
    LCD_WriteData(0xFF);
    LCD_WriteData(0xFF);
    LCD_WriteData(0xFF);
    LCD_WriteData(0xFF);
    LCD_WriteData(0xF2);
    LCD_WriteData(0xAB);
    LCD_WriteData(0xF7);
    LCD_WriteData(0x65);
    LCD_WriteData(0x40);
    LCD_WriteData(0xFF);

    LCD_WriteCommand(0xEF);
    LCD_WriteData(0x01);
    LCD_WriteData(0x01);
    LCD_WriteData(0x08);
    LCD_WriteData(0x40);
    LCD_WriteData(0x3F);
    LCD_WriteData(0x64);

    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x13);

    LCD_WriteCommand(0xE8);
    LCD_WriteData(0x00);
    LCD_WriteData(0x0E);

    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);

    LCD_WriteCommand(0x11);
    HAL_Delay(120);

    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x13);

    LCD_WriteCommand(0xE8);
    LCD_WriteData(0x00);
    LCD_WriteData(0x0C);
    HAL_Delay(20);

    LCD_WriteCommand(0xE8);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);

    LCD_WriteCommand(0xE6);
    LCD_WriteData(0x16);
    LCD_WriteData(0x7C);

    LCD_WriteCommand(0xFF);
    LCD_WriteData(0x77);
    LCD_WriteData(0x01);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);
    LCD_WriteData(0x00);

    LCD_WriteCommand(0x36);
    LCD_WriteData(0x08);

    LCD_WriteCommand(0x29);
    HAL_Delay(20);
    #endif


    // 2.4-16BIT旧屏幕16bit
    // LCD_WriteCommand(0xFF); //PAGE3
    // LCD_WriteData(0x77);
    // LCD_WriteData(0x01);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x13);

    // LCD_WriteCommand(0xEF);
    // LCD_WriteData(0x08);

    // LCD_WriteCommand(0xFF);//PAGE01
    // LCD_WriteData(0x77);
    // LCD_WriteData(0x01);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x10);

    // LCD_WriteCommand(0xC0); //Display Line Setting
    // LCD_WriteData(0x4F);
    // LCD_WriteData(0x00);

    // LCD_WriteCommand(0xC1);//Porch Control
    // LCD_WriteData(0x10);
    // LCD_WriteData(0x0C);

    // LCD_WriteCommand(0xC2);// Inversion set
    // LCD_WriteData(0x01);
    // LCD_WriteData(0x14);

    // LCD_WriteCommand(0xCC);
    // LCD_WriteData(0x10);

    // LCD_WriteCommand(0xB0);//Positive Voltage Gamma Control
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x0B);
    // LCD_WriteData(0x13);
    // LCD_WriteData(0x0D);
    // LCD_WriteData(0x10);
    // LCD_WriteData(0x07);
    // LCD_WriteData(0x02);
    // LCD_WriteData(0x08);
    // LCD_WriteData(0x07);
    // LCD_WriteData(0x1F);
    // LCD_WriteData(0x04);
    // LCD_WriteData(0x11);
    // LCD_WriteData(0x0F);
    // LCD_WriteData(0x28);
    // LCD_WriteData(0x2F);
    // LCD_WriteData(0x1F);

    // LCD_WriteCommand(0xB1); //Negative Voltage Gamma Control
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x0C);
    // LCD_WriteData(0x13);
    // LCD_WriteData(0x0C);
    // LCD_WriteData(0x10);
    // LCD_WriteData(0x05);
    // LCD_WriteData(0x02);
    // LCD_WriteData(0x08);
    // LCD_WriteData(0x08);
    // LCD_WriteData(0x1E);
    // LCD_WriteData(0x05);
    // LCD_WriteData(0x13);
    // LCD_WriteData(0x11);
    // LCD_WriteData(0x27);
    // LCD_WriteData(0x30);
    // LCD_WriteData(0x1F);

    // LCD_WriteCommand(0xFF);//PAGE1
    // LCD_WriteData(0x77);
    // LCD_WriteData(0x01);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x11);

    // LCD_WriteCommand(0xB0);	//Vop Amplitude setting
    // LCD_WriteData(0x4D);//

    // LCD_WriteCommand(0xB1);//VCOM amplitude setting
    // LCD_WriteData(0x3D);//

    // LCD_WriteCommand(0xB2);//VGH Voltage setting
    // LCD_WriteData(0x87);

    // LCD_WriteCommand(0xB3);//TEST Command Setting
    // LCD_WriteData(0x80);

    // LCD_WriteCommand(0xB5);//VGL Voltage setting
    // LCD_WriteData(0x45);

    // LCD_WriteCommand(0xB7);//Power Control 1
    // LCD_WriteData(0x85);

    // LCD_WriteCommand(0xB8);//Power Control 2
    // LCD_WriteData(0x20);

    // LCD_WriteCommand(0xC0);
    // LCD_WriteData(0x09);

    // LCD_WriteCommand(0xC1);//Source pre_drive timing set1
    // LCD_WriteData(0x78);

    // LCD_WriteCommand(0xC2);//Source EQ2 Setting
    // LCD_WriteData(0x78);

    // LCD_WriteCommand(0xD0);//Source EQ2 Setting
    // LCD_WriteData(0x88);

    // HAL_Delay(1);
    // //*********GIP SET*************//
    // LCD_WriteCommand(0xE0);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x02);

    // LCD_WriteCommand(0xE1);
    // LCD_WriteData(0x04);
    // LCD_WriteData(0xB0);
    // LCD_WriteData(0x06);
    // LCD_WriteData(0xB0);
    // LCD_WriteData(0x05);
    // LCD_WriteData(0xB0);
    // LCD_WriteData(0x07);
    // LCD_WriteData(0xB0);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x44);
    // LCD_WriteData(0x44);

    // LCD_WriteCommand(0xE2);
    // LCD_WriteData(0x20);
    // LCD_WriteData(0x20);
    // LCD_WriteData(0x44);
    // LCD_WriteData(0x44);
    // LCD_WriteData(0x96);
    // LCD_WriteData(0xA0);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x96);
    // LCD_WriteData(0xA0);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);

    // LCD_WriteCommand(0xE3);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x22);
    // LCD_WriteData(0x22);

    // LCD_WriteCommand(0xE4);
    // LCD_WriteData(0x44);
    // LCD_WriteData(0x44);

    // LCD_WriteCommand(0xE5);
    // LCD_WriteData(0x0C);
    // LCD_WriteData(0x90);
    // LCD_WriteData(0xB0);
    // LCD_WriteData(0xA0);
    // LCD_WriteData(0x0E);
    // LCD_WriteData(0x92);
    // LCD_WriteData(0xB0);
    // LCD_WriteData(0xA0);
    // LCD_WriteData(0x08);
    // LCD_WriteData(0x8C);
    // LCD_WriteData(0xB0);
    // LCD_WriteData(0xA0);
    // LCD_WriteData(0x0A);
    // LCD_WriteData(0x8E);
    // LCD_WriteData(0xB0);
    // LCD_WriteData(0xA0);

    // LCD_WriteCommand(0xE6);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x22);
    // LCD_WriteData(0x22);

    // LCD_WriteCommand(0xE7);
    // LCD_WriteData(0x44);
    // LCD_WriteData(0x44);

    // LCD_WriteCommand(0xE8);
    // LCD_WriteData(0x0D);
    // LCD_WriteData(0x91);
    // LCD_WriteData(0xB0);
    // LCD_WriteData(0xA0);
    // LCD_WriteData(0x0F);
    // LCD_WriteData(0x93);
    // LCD_WriteData(0xB0);
    // LCD_WriteData(0xA0);
    // LCD_WriteData(0x09);
    // LCD_WriteData(0x8D);
    // LCD_WriteData(0xB0);
    // LCD_WriteData(0xA0);
    // LCD_WriteData(0x0B);
    // LCD_WriteData(0x8F);
    // LCD_WriteData(0xB0);
    // LCD_WriteData(0xA0);

    // LCD_WriteCommand(0xE9);
    // LCD_WriteData(0x36);
    // LCD_WriteData(0x00);

    // LCD_WriteCommand(0xEB);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0xE4);
    // LCD_WriteData(0xE4);
    // LCD_WriteData(0x44);
    // LCD_WriteData(0x88);
    // LCD_WriteData(0x40);

    // LCD_WriteCommand(0xED);
    // LCD_WriteData(0xC1);
    // LCD_WriteData(0xA2);
    // LCD_WriteData(0xBF);
    // LCD_WriteData(0x0F);
    // LCD_WriteData(0x67);
    // LCD_WriteData(0x45);
    // LCD_WriteData(0xFF);
    // LCD_WriteData(0xFF);
    // LCD_WriteData(0xFF);
    // LCD_WriteData(0xFF);
    // LCD_WriteData(0x54);
    // LCD_WriteData(0x76);
    // LCD_WriteData(0xF0);
    // LCD_WriteData(0xFB);
    // LCD_WriteData(0x2A);
    // LCD_WriteData(0x1C);

    // LCD_WriteCommand(0xEF);
    // LCD_WriteData(0x10);
    // LCD_WriteData(0x0D);
    // LCD_WriteData(0x04);
    // LCD_WriteData(0x08);
    // LCD_WriteData(0x3F);
    // LCD_WriteData(0x1F);

    // LCD_WriteCommand(0xFF);
    // LCD_WriteData(0x77);
    // LCD_WriteData(0x01);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x13);

    // LCD_WriteCommand(0xE8);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x0E);

    // LCD_WriteCommand(0xFF);
    // LCD_WriteData(0x77);
    // LCD_WriteData(0x01);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);

    // LCD_WriteCommand(0x11);
    // HAL_Delay(1);

    // LCD_WriteCommand(0xFF);
    // LCD_WriteData(0x77);
    // LCD_WriteData(0x01);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x13);

    // LCD_WriteCommand(0xE8);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x0C);
    // HAL_Delay(1);

    // LCD_WriteCommand(0xE8);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);

    // LCD_WriteCommand(0xFF);
    // LCD_WriteData(0x77);
    // LCD_WriteData(0x01);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);
    // LCD_WriteData(0x00);

    // LCD_WriteCommand(0x3a);
    // LCD_WriteData(0x50);//16BIT

    // LCD_WriteCommand(0x29);//DISPLAY ON
    // HAL_Delay(1);
}
