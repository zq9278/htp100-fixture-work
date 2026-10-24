
#ifndef __GT911_H
#define __GT911_H

#include "main.h" // �?保这里引入的�?您项�?的HAL库主头文�?

// #define TOUCH_MAX 5 // 最大触摸点�?

// typedef struct
// {
// 	uint8_t flag;		   //	触摸标志位，�?1时表示有触摸操作
// 	uint8_t num;		   //	触摸点数
// 	uint16_t x[TOUCH_MAX]; //	x坐标
// 	uint16_t y[TOUCH_MAX]; //	y坐标
// } TouchStructure;

// /*------------------------------------ 寄存定义 -----------------------------------*/

// // #define GT9XX_IIC_RADDR 0xBB			// IIC初�?�化地址
// // #define GT9XX_IIC_WADDR 0xBA
// #define GT9XX_IIC_RADDR 0x14``````````````````````````` // IIC初�?�化地址
// #define GT9XX_IIC_WADDR 0x28

// #define GT9XX_CFG_ADDR 0x8047  // 固件配置信息寄存器和配置起�?�地址
// #define GT9XX_READ_ADDR 0x814E // 触摸信息寄存�?
// #define GT9XX_ID_ADDR 0x8140   // 触摸面板ID寄存�?

// void Touch_Scan(void); // 触摸�?�?













/*I2C句柄*/
#define		GT911_I2C			hi2c4

#define		GT911_DIV_ID	0XBA	//设�?�地址 //0X28 //0XBA

#define 	GT911_DIV_W		(GT911_DIV_ID | 0)	//写地址
#define 	GT911_DIV_R		(GT911_DIV_ID | 0)	//读地址

#define GT_RESET_LOW()  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET) // RESET�?�?
#define GT_RESET_HIGH() HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET)   // RESET�?�?



//GT911 部分寄存器定�? 
#define GT_CTRL_REG 	0X8040   	//GT911控制寄存�?
#define GT_CFGS_REG 	0X8047   	//GT911配置起�?�地址寄存�?
#define GT_CHECK_REG 	0X80FF   	//GT911校验和寄存器
#define GT_PID_REG 		0X8140   	//GT911产品ID寄存�?
 
#define GT_GSTID_REG 	0X814E   	//GT911当前检测到的触摸情�?,�?7位是触摸标志位，�?4位是触摸点数�?�?

#define GT_TPD_Sta		0X8150		//触摸点起始数�?地址
#define GT_TP1_REG 		0X8150  	//�?一�?触摸点数�?地址
#define GT_TP2_REG 		0X8158		//�?二个触摸点数�?地址
#define GT_TP3_REG 		0X8160		//�?三个触摸点数�?地址
#define GT_TP4_REG 		0X8168		//�?四个触摸点数�?地址
#define GT_TP5_REG 		0X8170		//�?五个触摸点数�?地址
#define DEBOUNCE_TIME_MS 50 // ��������ʱ��

#define GT_TOUCH_MAX			5			//对于gt911，最多同时获�?5�?触摸点的数据

typedef enum
{
	X_L = 0,
	X_H = 1,
	Y_L = 2,
	Y_H = 3,
	S_L	= 4,
	S_H = 5
}Data_XYS_P;	//数据X、Y、触摸大小数�?偏移�?

typedef enum
{
	TOUCH__NO		= 0x00,	//没有触摸
	TOUCH_ING		= 0x80	//�?触摸	
}TOUCH_STATE_enum;	//触摸状�?

typedef struct
{
	uint16_t	X_Point;	//X坐标
	uint16_t	Y_Point;	//Y坐标
	uint16_t	S_Point;	//触摸点大�?
}XY_Coordinate;	//触摸点坐�?


/*触摸结构�?*/
typedef struct 
{
	uint8_t Touch_State				;	//触摸状�?
	uint8_t Touch_Number			;	//触摸数量
	XY_Coordinate Touch_XY[GT_TOUCH_MAX]	;	//触摸的x坐标，�?�于gt911最�?5�?点的坐标
}Touch_Struct;	//触摸信息结构�?


/*外部变量�?*/
extern Touch_Struct	User_Touch;



/*外部函数�?*/
HAL_StatusTypeDef GT911_WriteReg(uint16_t _usRegAddr, uint8_t *_pRegBuf, uint8_t _ucLen);
HAL_StatusTypeDef GT911_ReadReg(uint16_t _usRegAddr, uint8_t *_pRegBuf, uint8_t _ucLen);

void GT911_Send_Config(uint8_t mode);		//配置初�?�化ft911
void Software_Reset(uint8_t gt_SR_type);//复位或者不复位gt911
void gt911_Scanf(void);									//�?描触摸模�?
void GT911_Init(void);
void GT911_RequestScan(void);
void GT911_RequestRecovery(void);
void GT911_Process(void);
int GT911_SetInterruptMode(uint8_t mode);
//static bool debounce_touch_state(void);






#endif /*__GT911_H_*/


