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


#include "lv_drivers/display/monitor.h"
#include "lv_drivers/indev/mouse.h"

//-----------------------------------------------------------------------------
//  Functions
//-----------------------------------------------------------------------------

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

void app_hal_init_mouse_input_device(void)
{
    /* Add the mouse as input device
     * Use the 'mouse' driver which reads the PC's mouse.
     */
    mouse_init();
    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;

    /* This function will be called periodically (by the library) to get the
     * mouse position and state.
     */
    indev_drv.read_cb        = mouse_read;
    lv_indev_t * mouse_indev = lv_indev_drv_register(&indev_drv);

    /* Set a cursor for the mouse. */
    LV_IMG_DECLARE(mouse_cursor_icon); /* Declare the image file. */
    lv_obj_t * cursor_obj = lv_img_create(
        lv_scr_act(), NULL); /* Create an image object for the cursor. */
    lv_img_set_src(cursor_obj, &mouse_cursor_icon); /* Set the image source. */
    lv_indev_set_cursor(
        mouse_indev, cursor_obj); /* Connect the image  object to the driver. */
}

int app_hal_thread_update_timer(void * data)
{
    (void)data;

    while (1) {
        SDL_Delay(5);   /*Sleep for 5 millisecond*/
        lv_tick_inc(5); /*Tell LittelvGL that 5 milliseconds were elapsed*/
    }

    return 0;
}
