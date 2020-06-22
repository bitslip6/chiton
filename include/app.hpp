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
//  Macros
//-----------------------------------------------------------------------------

#define APP_BG_NONE   (-LV_VER_RES)
#define APP_BG_FULL   0
#define APP_BG_NORMAL (-5 * (LV_VER_RES / 6))

/*Sizes*/
#define APP_BTN_HEIGHT (45)
#define APP_BTN_WIDTH  (120)

#define APP_STATUS_BAR_WIDTH  (LV_HOR_RES)
#define APP_STATUS_BAR_HEIGHT (40)
#define APP_PAGE_WIDTH        (LV_HOR_RES)
#define APP_PAGE_HEIGHT       (LV_VER_RES - APP_STATUS_BAR_HEIGHT)

/*Animations*/
#define APP_ANIM_Y          (LV_VER_RES / 20)
#define APP_NO_DELAY        (0)
#define APP_ANIM_DELAY      (50)
#define APP_ANIM_PAGE_DELAY (150)
#define APP_ANIM_TIME       (100)
#define APP_ANIM_TIME_BG    (300)
#define APP_ANIM_FADE_IN_TIME   (APP_ANIM_TIME + 20)

//-----------------------------------------------------------------------------
//  Public Types
//-----------------------------------------------------------------------------

namespace app
{
/* Enumeration of application screen pages. */
typedef enum {
    PAGE_INDEX_HOME,
    PAGE_INDEX_ERROR,
    PAGE_INDEX_SETTINGS,
    PAGE_INDEX_WIFI_SETTINGS,
} page_index_t;

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
    static struct Background bg_top;          /**< Top bar background. */
    static struct Background bg_bot;          /**< Bottom (main) background. */
    static page_index_t      last_page_index; /**< Last page shown by app. */
    static page_index_t      current_page_index; /**< Last page shown by app. */
    static lv_obj_t * page_display; /**< Main display area for the page (not
                                       including the status bar. */
    static lv_obj_t * status_bar;   /**< Status bar in the window display. */

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

    /* Update the window status bar. */
    void update_status_bar(uint32_t delay = APP_ANIM_PAGE_DELAY);

    /* Show the Home page on the screen. */
    void show_home_page(uint32_t delay);

    /* Show the Error page on the screen. */
    void show_error_page(std::string message, uint32_t delay);

    /* Show the settings menu. */
    void show_settings_page(uint32_t delay);

    /* Show the wifi settings. */
    void show_wifi_settings_page(uint32_t delay);
};

/* Initialize the application and HAL drivers. */
void init();

/* Refresh the application. This is necessary for the app to stay alive and
 * responsive.
 */
void refresh();

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

/* Go to a specific page index. */
void go_to_page_index(page_index_t index, uint32_t delay = APP_ANIM_PAGE_DELAY);

/* Initialize a page to default values. */
void init_page(lv_obj_t ** page, lv_scrollbar_mode_t mode);

} // namespace app

#endif // APP_H__
