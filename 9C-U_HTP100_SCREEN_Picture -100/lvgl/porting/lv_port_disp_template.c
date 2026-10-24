/**
 * @file lv_port_disp_templ.c
 *
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp_template.h"

#include <stdbool.h>
#include <string.h>
#include "main.h"
#define LVGL_MemoryAdd ((uint32_t)0xC0000000)//显存地址
  //static lv_color_t *buf_2_3 = (lv_color_t *)(LVGL_MemoryAdd3);
uint32_t addr;
bool lv_disp_updated;
#define AXI_SRAM __attribute__((section(".AXI_SRAM")))
volatile uint8_t lv_gpu_state = 0;
volatile lv_disp_drv_t *lvgl;
#define LTDC_FRAMEBUF_ADDR 0xC0000000 // 显存基地址
#define TFT_WEIGHT 480                // 屏幕宽
#define TFT_WIDTH 640                 // 屏幕高
/*********************
 *      DEFINES
 *********************/
#ifndef MY_DISP_HOR_RES
// #warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen width, default value 320 is used for now.
#define MY_DISP_HOR_RES 480
#endif

#ifndef MY_DISP_VER_RES
// #warning Please define or replace the macro MY_DISP_HOR_RES with the actual screen height, default value 240 is used for now.
#define MY_DISP_VER_RES 640
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
// static void gpu_fill(lv_disp_drv_t * disp_drv, lv_color_t * dest_buf, lv_coord_t dest_width,
//         const lv_area_t * fill_area, lv_color_t color);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/* Boot only: SDRAM must be initialized and LTDC must still be stopped.
 * Both buffers are outside LVGL's heap (which starts at SDRAM + 4 MiB). */
void lv_port_disp_clear_buffers(void)
{
    const uint32_t bytes = 2U * MY_DISP_HOR_RES * MY_DISP_VER_RES * sizeof(lv_color_t);
    memset((void *)LVGL_MemoryAdd, 0, bytes);
    SCB_CleanDCache_by_Addr((uint32_t *)LVGL_MemoryAdd, (int32_t)bytes);
    __DSB();
}

void lv_port_disp_init(void)
{
  /*-------------------------
   * Initialize your display
   * -----------------------*/
  disp_init();

  /*-----------------------------
   * Create a buffer for drawing
   *----------------------------*/

  /**
   * LVGL requires a buffer where it internally draws the widgets.
   * Later this buffer will passed to your display driver's `flush_cb` to copy its content to your display.
   * The buffer has to be greater than 1 display row
   *
   * There are 3 buffering configurations:
   * 1. Create ONE buffer:
   *      LVGL will draw the display's content here and writes it to your display
   *
   * 2. Create TWO buffer:
   *      LVGL will draw the display's content to a buffer and writes it your display.
   *      You should use DMA to write the buffer's content to the display.
   *      It will enable LVGL to draw the next part of the screen to the other buffer while
   *      the data is being sent form the first buffer. It makes rendering and flushing parallel.
   *
   * 3. Double buffering
   *      Set 2 screens sized buffers and set disp_drv.full_refresh = 1.
   *      This way LVGL will always provide the whole rendered screen in `flush_cb`
   *      and you only need to change the frame buffer's address.
   */

  /* Example for 1) */
  // static lv_disp_draw_buf_t draw_buf_dsc_1;
  // static AXI_SRAM lv_color_t buf_1[MY_DISP_HOR_RES * 500];                          /*A buffer for 10 rows*/
  // lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, NULL, MY_DISP_HOR_RES * 500);   /*Initialize the display buffer*/

  //    static lv_disp_draw_buf_t draw_buf_dsc_2;
  // static AXI_SRAM lv_color_t buf_2_1[MY_DISP_HOR_RES * 200];
  // static AXI_SRAM lv_color_t buf_2_2[MY_DISP_HOR_RES * 200];
  // lv_disp_draw_buf_init(&draw_buf_dsc_2, buf_2_1, buf_2_2, MY_DISP_HOR_RES * 200); /*Initialize the display buffer*/

   static lv_disp_draw_buf_t draw_buf_dsc_3;
   static lv_color_t *buf_3_1 = (lv_color_t *)(LVGL_MemoryAdd); /*A screen sized buffer*/
   static lv_color_t *buf_3_2 = (lv_color_t *)(LVGL_MemoryAdd + MY_DISP_HOR_RES * MY_DISP_VER_RES * sizeof(lv_color_t));
   lv_disp_draw_buf_init(&draw_buf_dsc_3, buf_3_1, buf_3_2, MY_DISP_HOR_RES * MY_DISP_VER_RES); /*Initialize the display buffer*/
 

  /*-----------------------------------
   * Register the display in LVGL
   *----------------------------------*/

  static lv_disp_drv_t disp_drv; /*Descriptor of a display driver*/
  lv_disp_drv_init(&disp_drv);   /*Basic initialization*/

  /*Set up the functions to access to your display*/

  /*Set the resolution of the display*/
  disp_drv.hor_res = MY_DISP_HOR_RES;
  disp_drv.ver_res = MY_DISP_VER_RES;

  /*Used to copy the buffer's content to the display*/
  disp_drv.flush_cb = disp_flush;

  /*Set a display buffer*/
disp_drv.draw_buf = &draw_buf_dsc_3;
  /*Required for Example 3)*/
  disp_drv.full_refresh = 1;

  /* Fill a memory array with a color if you have GPU.
   * Note that, in lv_conf.h you can enable GPUs that has built-in support in LVGL.
   * But if you have a different GPU you can use with this callback.*/
  // disp_drv.gpu_fill_cb = gpu_fill;

  /*Finally register the driver*/
  lv_disp_drv_register(&disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
  /*You code here*/
  LCD_Init();
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
  disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
  disp_flush_enabled = false;
}
/**
 * 使用 DMA2D 复制缓冲区到屏幕
 * 
 * @param x      目标区域左上角的 X 坐标
 * @param y      目标区域左上角的 Y 坐标
 * @param width  目标区域的宽度（像素）
 * @param height 目标区域的高度（像素）
 * @param color  源数据缓冲区（RGB565 格式）
 */
void LCD_CopyBuffer(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t *color)
{
    /* 1️⃣ 停止 DMA2D 传输，确保上一次传输已完成 */
    DMA2D->CR &= ~(DMA2D_CR_START);  

    /* 2️⃣ 设置 DMA2D 工作模式：存储器到存储器模式 (Memory-to-Memory) */
    DMA2D->CR = DMA2D_M2M;

    /* 3️⃣ 设置前景（源）颜色格式 */
    DMA2D->FGPFCCR = LTDC_PIXEL_FORMAT_RGB565;  // RGB565 格式（16-bit）

    /* 4️⃣ 设置前景层（源数据）偏移量 */
    DMA2D->FGOR = 0;  // 源数据没有行偏移（数据是连续的）

    /* 5️⃣ 设置目标缓冲区（显存）偏移量 */
    DMA2D->OOR = MY_DISP_HOR_RES - width;  // 计算显存每行剩余的偏移量

    /* 6️⃣ 设置前景层（源数据）存储器地址 */
    DMA2D->FGMAR = (uint32_t)color;  // 源数据缓冲区的地址

    /* 7️⃣ 设置目标缓冲区（显存）存储器地址 */
    DMA2D->OMAR = LVGL_MemoryAdd + 2 * (MY_DISP_HOR_RES * y + x);  
    // 计算目标存储地址：
    // - `MY_DISP_HOR_RES * y` 计算目标区域在屏幕中的行偏移
    // - `x` 计算目标区域的列偏移
    // - 由于 RGB565 是 16 位（2 字节），所以需要乘 2

    /* 8️⃣ 设置传输的像素宽度（X）和行数（Y） */
    DMA2D->NLR = (width << 16) | height;  
    // - `width` (高16位) 指定一行要传输的像素数
    // - `height` (低16位) 指定要传输的行数

    /* 9️⃣ 启动 DMA2D 传输 */
    DMA2D->CR |= DMA2D_CR_START;  

    /* 🔟 等待 DMA2D 传输完成（同步模式，CPU 阻塞等待） */
    //while (DMA2D->CR & DMA2D_CR_START);  
    // ⚠️ 这会导致 CPU 等待 DMA2D 传输完成，可能影响性能

    /* 启用 DMA2D 传输完成中断 */
    DMA2D->CR |= DMA2D_CR_TCIE;  
    
}

/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
 int8_t wTransferState;
 extern LTDC_HandleTypeDef hltdc;
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    (void)area;
    /* full_refresh supplies one complete, 32-byte-aligned RGB565 frame.
     * Publish CPU writes before LTDC reads SDRAM. DMA2D output has already
     * been invalidated when its transfer finished; do not clean stale data. */
    SCB_CleanDCache_by_Addr((uint32_t *)color_p,
                          MY_DISP_HOR_RES * MY_DISP_VER_RES * sizeof(lv_color_t));
    __DSB();
    __HAL_LTDC_LAYER(&hltdc, 0)->CFBAR = (uint32_t)color_p;
    /* Do not wait for a display timing event in the UI thread. During an
     * unstable startup it may not arrive. Keep cache synchronization, but
     * restore immediate reload until asynchronous buffer ownership is used. */
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&hltdc);
    __DSB();
    lv_disp_flush_ready(disp_drv);

}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
