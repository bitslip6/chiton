/******************************************************************************
 *
 * @file            app.cpp
 *
 * @brief           Core application functionality
 *
 * @details
 *
 * @author(s)       Stephen Stauts
 *
 * @created         06/15/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2015-2020 SWYM, LLC. All Rights Reserved.
 *
 *****************************************************************************/

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include "app.hpp"

#include <SDL2/SDL.h>

#include "lvgl/lvgl.h"
#include "app_hal.h"

namespace app
{

void init(void)
{
    lv_init();

    app_hal_init_monitor_display();
    app_hal_init_mouse_input_device();

    /* Tick init.
     * You have to call 'lv_tick_inc()' in periodically to inform LittelvGL
     * about how much time were elapsed Create an SDL thread to do this
     */
    SDL_CreateThread(app_hal_thread_update_timer, "update_hal_timer", NULL);
}

void refresh(void)
{
    /* Periodically call the lv_task handler.
     * It could be done in a timer interrupt or an OS task too.
     */
    lv_task_handler();
}

} // namespace app
