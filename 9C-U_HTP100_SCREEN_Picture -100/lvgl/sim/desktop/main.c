#define SDL_MAIN_HANDLED

#include "lvgl.h"
#include "ui.h"

#include <SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SIM_HOR_RES 480
#define SIM_VER_RES 640

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint16_t framebuffer[SIM_HOR_RES * SIM_VER_RES];

static struct {
    int16_t x;
    int16_t y;
    bool pressed;
    bool quit;
} mouse_state;

static void sdl_display_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    int32_t x;
    int32_t y;

    for(y = area->y1; y <= area->y2; ++y) {
        for(x = area->x1; x <= area->x2; ++x) {
            framebuffer[y * SIM_HOR_RES + x] = color_p->full;
            color_p++;
        }
    }

    if(lv_disp_flush_is_last(disp_drv)) {
        SDL_UpdateTexture(texture, NULL, framebuffer, SIM_HOR_RES * (int)sizeof(uint16_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    lv_disp_flush_ready(disp_drv);
}

static void sdl_mouse_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    data->point.x = mouse_state.x;
    data->point.y = mouse_state.y;
    data->state = mouse_state.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static int init_sdl(void)
{
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        return -1;
    }

    window = SDL_CreateWindow(
        "SLK01 LVGL SDL Simulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SIM_HOR_RES,
        SIM_VER_RES,
        SDL_WINDOW_SHOWN
    );
    if(window == NULL) {
        return -1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if(renderer == NULL) {
        return -1;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, SIM_HOR_RES, SIM_VER_RES);
    if(texture == NULL) {
        return -1;
    }

    memset(framebuffer, 0, sizeof(framebuffer));
    return 0;
}

static void init_lvgl_port(void)
{
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[SIM_HOR_RES * 40];
    static lv_color_t buf2[SIM_HOR_RES * 40];
    static lv_disp_drv_t disp_drv;
    static lv_indev_drv_t indev_drv;

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SIM_HOR_RES * 40);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SIM_HOR_RES;
    disp_drv.ver_res = SIM_VER_RES;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);
}

static void process_events(void)
{
    SDL_Event event;

    while(SDL_PollEvent(&event)) {
        switch(event.type) {
        case SDL_QUIT:
            mouse_state.quit = true;
            break;
        case SDL_MOUSEMOTION:
            mouse_state.x = (int16_t)event.motion.x;
            mouse_state.y = (int16_t)event.motion.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if(event.button.button == SDL_BUTTON_LEFT) {
                mouse_state.pressed = true;
                mouse_state.x = (int16_t)event.button.x;
                mouse_state.y = (int16_t)event.button.y;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if(event.button.button == SDL_BUTTON_LEFT) {
                mouse_state.pressed = false;
                mouse_state.x = (int16_t)event.button.x;
                mouse_state.y = (int16_t)event.button.y;
            }
            break;
        default:
            break;
        }
    }
}

static void save_framebuffer_bmp(const char *path)
{
    SDL_Surface *surface;

    if(path == NULL || path[0] == '\0') {
        return;
    }

    surface = SDL_CreateRGBSurfaceWithFormatFrom(
        framebuffer,
        SIM_HOR_RES,
        SIM_VER_RES,
        16,
        SIM_HOR_RES * (int)sizeof(uint16_t),
        SDL_PIXELFORMAT_RGB565
    );
    if(surface != NULL) {
        SDL_SaveBMP(surface, path);
        SDL_FreeSurface(surface);
    }
}

int main(int argc, char **argv)
{
    uint32_t last_tick;
    const char *screen_name;
    const char *lang_name;
    const char *capture_path;
    uint32_t capture_after_ms = 0;
    uint32_t runtime_ms = 0;
    (void)argc;
    (void)argv;

    if(init_sdl() != 0) {
        return 1;
    }

    lv_init();
    init_lvgl_port();
    screen_name = getenv("SLK_SCREEN");
    if(screen_name != NULL && screen_name[0] != '\0') {
        ui_debug_init_screen(screen_name);
    } else {
        ui_init();
    }
    lang_name = getenv("SLK_LANG");
    if(lang_name != NULL) {
        if(strcmp(lang_name, "en") == 0 || strcmp(lang_name, "EN") == 0) {
            ui_lang_set(UI_LANG_EN);
        } else if(strcmp(lang_name, "zh") == 0 || strcmp(lang_name, "ZH") == 0) {
            ui_lang_set(UI_LANG_ZH);
        }
    }
    capture_path = getenv("SLK_CAPTURE_BMP");
    if(capture_path != NULL && capture_path[0] != '\0') {
        const char *capture_ms = getenv("SLK_CAPTURE_MS");
        capture_after_ms = capture_ms != NULL ? (uint32_t)atoi(capture_ms) : 500;
    }

    last_tick = SDL_GetTicks();
    while(!mouse_state.quit) {
        uint32_t now = SDL_GetTicks();
        uint32_t elapsed = now - last_tick;

        last_tick = now;
        process_events();
        lv_tick_inc(elapsed);
        lv_timer_handler();
        runtime_ms += elapsed;
        if(capture_after_ms > 0 && runtime_ms >= capture_after_ms) {
            save_framebuffer_bmp(capture_path);
            break;
        }
        SDL_Delay(5);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
