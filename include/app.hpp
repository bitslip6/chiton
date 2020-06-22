/******************************************************************************
 *
 * @file            app.hpp
 *
 * @brief           Core application functionality.
 *
 * @details
 *
 * @author(s)       Stephen Stauts
 *
 * @created         06/15/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2020 Jabil, Inc. All Rights Reserved.
 *
 *****************************************************************************/

#ifndef APP_H__
#define APP_H__

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>
#include <string>

#include "lvgl/lvgl.h"

//-----------------------------------------------------------------------------
//  Public Types
//-----------------------------------------------------------------------------

namespace app
{
/* Enumeration of application screen pages. */
typedef enum {
    APP_PAGE_HOME,
    APP_PAGE_ERROR,
    APP_PAGE_SETTINGS,
    APP_PAGE_WIFI_SETTINGS,
} app_page_t;

/* Container for background-related properties. */
struct Background {
    lv_obj_t * obj;
    lv_color_t previous_color;
    lv_color_t active_color;
};

class AppWindow;

} // namespace app

//-----------------------------------------------------------------------------
//  Public Function Prototypes
//-----------------------------------------------------------------------------

namespace app
{
class AppWindow
{
    static bool built; /*< Flag for tracking built at startup and prevent
                          multiple calls to build(). */

  public:
    static struct Background bg_top;    /*< Top bar background. */
    static struct Background bg_bot;    /*< Bottom (main) background. */
    static app_page_t        last_page; /*< Last page shown by app. */
    static lv_obj_t *        active_page;
    static lv_obj_t *        status_bar;

    /* Constructor */
    AppWindow();

    /* Deconstructor */
    ~AppWindow();

    /* Build the app. This only needs to be called once, at startup. */
    void build();

    /* Application has been built at startup. */
    bool is_built()
    {
        return this->built;
    }

    void update_status_bar(uint32_t delay);

    /* Show the Home page on the screen. */
    void show_home_page(uint32_t delay);

    /* Show the Error page on the screen. */
    void show_error_page(std::string   message,
                         lv_event_cb_t btn_event_cb,
                         uint32_t      delay);

    void show_settings_page(uint32_t delay);

    void show_wifi_settings_page(uint32_t delay);
};

/* Initialize the application and HAL drivers. */
void init();

/* Refresh the application. This is necessary for the app to stay alive and
 * responsive.
 */
void refresh();

void init_page(lv_obj_t ** page, lv_scrollbar_mode_t mode);

/* Add an icon to the interface.
 *
 * @note This is derived from
 * https://github.com/lvgl/lv_examples/blob/master/src/lv_demo_printer/lv_demo_printer.c
 */
lv_obj_t * add_icon(lv_obj_t *   parent,
                    const void * src_bg,
                    const void * src_icon,
                    std::string  txt);

/* Animate objects into the window screen.
 *
 * @note This is derived from
 * https://github.com/lvgl/lv_examples/blob/master/src/lv_demo_printer/lv_demo_printer.c
 */
void animate_in(lv_obj_t * obj, uint32_t delay);

/* Animate all objects out of the window screen.
 *
 * @note This is derived from
 * https://github.com/lvgl/lv_examples/blob/master/src/lv_demo_printer/lv_demo_printer.c
 */
void animate_out_all(lv_obj_t * obj, uint32_t delay);

/* Animate changes with the background.
 *
 * @note This is derived from
 * https://github.com/lvgl/lv_examples/blob/master/src/lv_demo_printer/lv_demo_printer.c
 */
void animate_background(uint32_t   delay,
                        lv_color_t top_color,
                        lv_color_t bot_color,
                        bool       fullscreen);

/**
 * Calculate the current value of an animation applying linear characteristic.
 *
 * @note This is derived from
 * https://github.com/lvgl/lv_examples/blob/master/src/lv_demo_printer/lv_demo_printer.c
 */
lv_anim_value_t anim_path_triangle(const lv_anim_path_t * path,
                                   const lv_anim_t *      a);


} // namespace app

#endif // APP_H__
