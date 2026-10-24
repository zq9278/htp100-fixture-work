#include "lv_port_disp.h"
#include "../../lvgl.h"
#include <rtthread.h>
#include <lcd_port.h>
#include <stm32h7xx_hal.h>
/*********************
 *      DEFINES
 *********************/
/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);
/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

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
    #if 0
    /* Example for 1) */
    static lv_disp_draw_buf_t draw_buf_dsc;
    static lv_color_t buf[MY_DISP_HOR_RES * 10];                          /*A buffer for 10 rows*/
    lv_disp_draw_buf_init(&draw_buf_dsc, buf, NULL, MY_DISP_HOR_RES * 10);   /*Initialize the display buffer*/

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/

    static lv_disp_drv_t disp_drv;                         /*Descriptor of a display driver*/
    lv_disp_drv_init(&disp_drv);                    /*Basic initialization*/

    /*Set up the functions to access to your display*/

    /*Set the resolution of the display*/
    disp_drv.hor_res = 240;
    disp_drv.ver_res = 240;

    /*Used to copy the buffer's content to the display*/
    disp_drv.flush_cb = disp_flush;

    /*Set a display buffer*/
    disp_drv.draw_buf = &draw_buf_dsc;

    /*Required for Example 3)*/
    //disp_drv.full_refresh = 1

    /* Fill a memory array with a color if you have GPU.
     * Note that, in lv_conf.h you can enable GPUs that has built-in support in LVGL.
     * But if you have a different GPU you can use with this callback.*/
    //disp_drv.gpu_fill_cb = gpu_fill;

    /*Finally register the driver*/
    lv_disp_drv_register(&disp_drv);
    #endif
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static rt_device_t lcd;
static lv_disp_draw_buf_t draw_buf_dsc;
static lv_disp_drv_t disp_drv;
static lv_color_t *fbuf1 = RT_NULL, *fbuf2 = RT_NULL, *fbuf3 = RT_NULL;
/*Initialize your display and the required peripherals.*/

static rt_err_t disp_flush_cb(rt_device_t dev, void *arg)
{
    lv_disp_flush_ready(&disp_drv);
    return RT_EOK;
}

static void disp_init(void)
{
    /*You code here*/
    struct rt_device_graphic_info info;
    lcd = rt_device_find("lcd");
    RT_ASSERT(lcd);
    if(!lcd)
    {
        return;
    }
    rt_device_open(lcd, RT_DEVICE_OFLAG_RDWR);
    rt_device_control(lcd, RTGRAPHIC_CTRL_GET_INFO, &info);
    
    fbuf1 = rt_malloc(info.width * info.height * sizeof(lv_color_t));
    fbuf2 = rt_malloc(info.width * info.height * sizeof(lv_color_t));
    fbuf3 = (lv_color_t *)((struct drv_lcd_device *)lcd)->front_buf;
    
    lv_disp_draw_buf_init(&draw_buf_dsc, fbuf1, fbuf2, info.width * info.height);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = info.width;
    disp_drv.ver_res = info.height;
    disp_drv.flush_cb = disp_flush;
    disp_drv.wait_cb = RT_NULL;
    disp_drv.draw_buf = &draw_buf_dsc;
    disp_drv.rotated = LV_DISP_ROT_NONE;
    disp_drv.sw_rotate = 0;
    disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);
    
    rt_device_set_tx_complete(lcd, disp_flush_cb);
}
extern LTDC_HandleTypeDef LtdcHandle;
/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
bool lv_disp_updated = false;
void *last_pending_buffer = RT_NULL;
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    /*The most simple case (but also the slowest) to put all pixels to the screen one-by-one*/
    /* get buf act*/
    void *buf_act = disp_drv->draw_buf->buf_act;
    if(lv_disp_updated)
    {
        if((fbuf1 != buf_act) && (fbuf1 != last_pending_buffer))
        {
            disp_drv->draw_buf->buf1 = buf_act;
            disp_drv->draw_buf->buf2 = fbuf1;
            disp_drv->draw_buf->buf_act = buf_act;
        }
        else if((fbuf2 != buf_act) && (fbuf2 != last_pending_buffer))
        {
            disp_drv->draw_buf->buf1 = buf_act;
            disp_drv->draw_buf->buf2 = fbuf2;
            disp_drv->draw_buf->buf_act = buf_act;
        }
        else if((fbuf3 != buf_act) && (fbuf3 != last_pending_buffer))
        {
            disp_drv->draw_buf->buf1 = buf_act;
            disp_drv->draw_buf->buf2 = fbuf3;
            disp_drv->draw_buf->buf_act = buf_act;
        }
        lv_disp_updated = false;
    }
    LTDC_Layer1->CFBAR = (uint32_t)color_p;
    last_pending_buffer = buf_act;
    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);
}

/*OPTIONAL: GPU INTERFACE*/

/*If your MCU has hardware accelerator (GPU) then you can use it to fill a memory with a color*/
//static void gpu_fill(lv_disp_drv_t * disp_drv, lv_color_t * dest_buf, lv_coord_t dest_width,
//                    const lv_area_t * fill_area, lv_color_t color)
//{
//    /*It's an example code which should be done by your GPU*/
//    int32_t x, y;
//    dest_buf += dest_width * fill_area->y1; /*Go to the first line*/
//
//    for(y = fill_area->y1; y <= fill_area->y2; y++) {
//        for(x = fill_area->x1; x <= fill_area->x2; x++) {
//            dest_buf[x] = color;
//        }
//        dest_buf+=dest_width;    /*Go to the next line*/
//    }
//}