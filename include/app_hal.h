/******************************************************************************
 *
 * @file            app_hal.h
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

#ifndef APP_HAL_H__
#define APP_HAL_H__


#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
//  Functions
//-----------------------------------------------------------------------------

void app_hal_init_monitor_display(void);

void app_hal_init_mouse_input_device(void);

/**
 * A task to measure the elapsed time for LVGL
 * @param data unused
 * @return never return
 */
int app_hal_thread_update_timer(void * data);


#ifdef __cplusplus
}
#endif


#endif  // APP_HAL_H__
