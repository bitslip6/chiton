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

#ifndef APP_HPP__
#define APP_HPP__

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include <array>
#include <stdbool.h>
#include <stdint.h>
#include <string>

#include "lvgl/lvgl.h"
#include "wifi.hpp"

//-----------------------------------------------------------------------------
//  Macros
//-----------------------------------------------------------------------------

#define APP_BG_NONE   (-LV_VER_RES)
#define APP_BG_FULL   0
#define APP_BG_NORMAL (-5 * (LV_VER_RES / 6))

/*Sizes*/
#define APP_BTN_HEIGHT (45)
#define APP_BTN_WIDTH  (120)

#define APP_STATUS_BAR_WIDTH   (LV_HOR_RES)
#define APP_STATUS_BAR_HEIGHT  (45)
#define APP_PAGE_WIDTH         (LV_HOR_RES)
#define APP_PAGE_HEIGHT        (LV_VER_RES - APP_STATUS_BAR_HEIGHT)
#define APP_PAGE_HEADER_X      (15)
#define APP_PAGE_HEADER_Y      (5)
#define APP_PAGE_BOTTOM_Y      (145)
#define APP_PAGE_HEADER_HEIGHT (40)
#define APP_PAGE_CONTAINER_Y   (70)

/*Animations*/
#define APP_ANIM_Y            (LV_VER_RES / 20)
#define APP_NO_DELAY          (0)
#define APP_ANIM_DELAY        (50)
#define APP_ANIM_PAGE_DELAY   (150)
#define APP_ANIM_TIME         (100)
#define APP_ANIM_TIME_BG      (300)
#define APP_ANIM_FADE_IN_TIME (APP_ANIM_TIME + 20)

//-----------------------------------------------------------------------------
//  Public Types
//-----------------------------------------------------------------------------

namespace app
{
/* Enumeration of page indices. These pages include subpages of tabs. */
typedef enum {
    PAGE_INDEX_HOME = 0, /**< Home tab home page. */
    PAGE_INDEX_VIDEO,    /**< Video tab home page. */
    PAGE_INDEX_SETTINGS, /**< Settings tab home page. */

    /* Tab subpages */
    PAGE_INDEX_WIFI_SETTINGS,   /**< Page showing general wifi settings. */
    PAGE_INDEX_WIFI_CONNECTION, /**< Page for joining a wifi network. */
} page_index_t;

/* Enumeration of tab indices. */
typedef enum {
    TAB_INDEX_HOME = 0, /**< General status tab. */
    TAB_INDEX_VIDEO,    /**< Tab governing video functions. */
    TAB_INDEX_SETTINGS, /**< Tab governing all settings options. */

    /* This must go last. */
    NUM_TAB_INDICES
} tab_index_t;

class AppWindow;

}    // namespace app

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
    static page_index_t last_page_index;    /**< Last page shown by app. */
    static page_index_t current_page_index; /**< Last page shown by app. */
    static lv_obj_t *   keyboard;           /**< App on-screen keyboard. */
    static lv_obj_t *   page_display; /**< Main display area for the page (not
                                         including the status bar. */
    static lv_obj_t * tabview;        /**< Main tab holder object. */
    static std::array<lv_obj_t *, NUM_TAB_INDICES>
                             tab_list; /**< Array of tabs used by the viewer. */
    static wifi::WifiNetwork wifi_network; /**< App wifi network connection. */

    AppWindow();
    ~AppWindow();

    /* Build the app. This only needs to be called once, at startup. */
    void build();

    /* Application has been built at startup. */
    bool is_built()
    {
        return this->built;
    }

    /* True if the app has an active wifi connection. */
    bool has_wifi_connection();

    /* Show the home page under the Home tab. */
    void show_home_page();

    /* Show the settings menu under the Settings tab. */
    void show_settings_page();

    /* Show the wifi settings in the Settings tab. */
    void show_wifi_settings_page();

    /* Show the wifi page for entering connection info in the Settings tab. */
    void show_wifi_connection_page();
};

/* Initialize the application and HAL drivers. */
void init();

/* Refresh the application. This is necessary for the app to stay alive and
 * responsive.
 */
void refresh();

/* Go to a specific page index. This can include default tab pages, if that
 * was the last page visited, and will switch tabs accordingly.
 */
void go_to_page_index(page_index_t index, bool tab_click = false);

/* Go back to the previously viewed page. */
void go_to_previous_page();

/* Show the notification splash on the screen. */
void show_notification(std::string message);

}    // namespace app

#endif    // APP_HPP__
