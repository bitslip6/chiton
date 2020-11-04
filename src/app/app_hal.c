/******************************************************************************
 *
 * @file            app_hal.c
 *
 * @brief           HAL driver layer for application devices.
 *
 * @details
 *
 * @author(s)       Stephen Stauts
 *
 * @created         06/16/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2020 Jabil, Inc. All Rights Reserved.
 *
 *****************************************************************************/

/**
 * MIT licence
 * Copyright (c) 2020 LVGL LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "app_hal.h"

#include <stdio.h>
#include <unistd.h>

#if USE_MONITOR
    #include "lv_drivers/display/monitor.h"
#elif USE_FBDEV
    #include "lv_drivers/display/fbdev.h"
#else
    #error "app_hal.c: No display driver set"
#endif /* USE_MONITOR */

#include "lv_drivers/indev/evdev.h"
#include "lv_drivers/indev/mouse.h"

//---------------------------------------------------------------------
// Macros
//---------------------------------------------------------------------

/* Use the standard buffer size from stdio. */
#define FRAMEBUFFER_SIZE 320*240*2 * 2

#define UPDATE_DELAY_MS (5)
#define UPDATE_DELAY_US (UPDATE_DELAY_MS * 1000)

//-----------------------------------------------------------------------------
//  Functions
//-----------------------------------------------------------------------------

#if USE_MONITOR
void app_hal_init_monitor_display(void)
{
    /* Use the 'monitor' driver which creates window on PC's monitor to simulate
     * a display.
     */
    monitor_init();

    /* Create a display buffer. */
    static lv_disp_buf_t disp_buf1;
    static lv_color_t    buf1_1[LV_HOR_RES_MAX * 120];
    lv_disp_buf_init(&disp_buf1, buf1_1, NULL, LV_HOR_RES_MAX * 120);

    /* Create a display. */
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.buffer   = &disp_buf1;
    disp_drv.flush_cb = monitor_flush;
    lv_disp_drv_register(&disp_drv);
}
#endif /* USE_MONITOR */

#if USE_FBDEV
void app_hal_init_framebuffer_display(void)
{
    /*Linux frame buffer device init*/
    fbdev_init();

    /*A small buffer for LittlevGL to draw the screen's content*/
    static lv_color_t buf1[FRAMEBUFFER_SIZE];
    static lv_color_t buf2[FRAMEBUFFER_SIZE];

    /*Initialize a descriptor for the buffer*/
    static lv_disp_buf_t disp_buf;
    lv_disp_buf_init(&disp_buf, buf1, buf2, FRAMEBUFFER_SIZE);

    /*Initialize and register a display driver*/
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.buffer   = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    // disp_drv.screen_transp = 0;
    lv_disp_drv_register(&disp_drv);


    /*Create a "Hello world!" label*/
    /*
    lv_disp_set_bg_color(NULL, LV_COLOR_RED);
    //lv_style_set_bg_color(NULL, LV_COLOR_RED);
    lv_obj_t * label = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(label, "Hello world!");
    lv_obj_align(label, NULL, LV_ALIGN_CENTER, 20, -20);
    printf("hello world created\n");

    // Handle LitlevGL tasks (tickless mode)
    while(1) {
        lv_tick_inc(500);
        lv_task_handler();
        usleep(5000);
    }
    */
}
#endif /* USE_FBDEV */


void app_hal_init_input_device(void)
{
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;

#if USE_MOUSE
    mouse_init();
    indev_drv.read_cb = mouse_read;
#elif USE_EVDEV
    evdev_init();
    indev_drv.read_cb = evdev_read;
#else
    #error "app_hal.c: No input device set"
#endif /* USE_MOUSE */

    lv_indev_t *input_dev = lv_indev_drv_register(&indev_drv);

#if !USE_LIBINPUT
    /* Set a cursor for the input device if not a touchscreen. */

    /* Declare the image file. */
    LV_IMG_DECLARE(mouse_cursor_icon);

    /* Create an image object for the cursor. */
    lv_obj_t * cursor_obj = lv_img_create(lv_scr_act(), NULL);

    /* Set the image source. */
    lv_img_set_src(cursor_obj, &mouse_cursor_icon);

    /* Connect the image  object to the driver. */
    lv_indev_set_cursor(input_dev, cursor_obj);
#endif /* TFT_DISPLAY */
}

#if USE_MONITOR
int app_hal_thread_update_timer(__attribute__((unused)) void * data)
{
    while (1) {
        SDL_Delay(UPDATE_DELAY_MS); /*Sleep in millisecond*/

        /*Tell LVGL that N milliseconds have elapsed*/
        lv_tick_inc(UPDATE_DELAY_MS);
    }

    return 0;
}
#else
void app_hal_thread_update_timer(__attribute__((unused)) void * data)
{

    printf("update timer\n");
    while (1) {
        usleep(UPDATE_DELAY_US);

        /*Tell LVGL that N milliseconds have elapsed*/
        lv_tick_inc(UPDATE_DELAY_MS);
    }
}
#endif /* USE_MONITOR */
