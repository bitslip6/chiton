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
    PAGE_INDEX_WIFI_SETTINGS,     /**< Page showing general wifi settings. */
    PAGE_INDEX_WIFI_JOIN_NETWORK, /**< Page for joining a wifi network. */
    PAGE_INDEX_WIFI_JOINING,

    PAGE_INDEX_ADHOC_SETTINGS, /** Page that automatically makes the adhoc
                                  connection */
    PAGE_INDEX_ADHOC_STARTING,

    /* This must go last */
    NUM_PAGE_INDICES
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
/* Helper class for maintaining pointers to text areas containg credential
 * information. This allows the app to have the information passed as an object
 * into callback functions.
 */
class CredentialTexts
{
    lv_obj_t * ta_id;
    lv_obj_t * ta_pwd;

  public:
    CredentialTexts(lv_obj_t * ta_id_, lv_obj_t * ta_pwd_)
        : ta_id(ta_id_), ta_pwd(ta_pwd_) {};

    ~CredentialTexts() {};

    std::string get_id()
    {
        return std::string(lv_textarea_get_text(this->ta_id));
    }

    std::string get_pwd()
    {
        return std::string(lv_textarea_get_text(this->ta_pwd));
    }

    /* Clear the credential string texts. */
    void erase()
    {
        lv_textarea_set_text(this->ta_id, "");
        lv_textarea_set_text(this->ta_pwd, "");
    }
};


class AppWindow
{
    /* Flag for tracking built at startup and prevent multiple calls to
     * build().
     */
    static bool built;

  public:
    /* Last page shown by app. */
    static page_index_t last_page_index;

    /* Last page shown by app. */
    static page_index_t current_page_index;

    /* App on-screen keyboard. */
    static lv_obj_t * keyboard;

    /* Main display area for the page (not including the status bar). */
    static lv_obj_t * page_display;

    /* Main tab holder object. */
    static lv_obj_t * tabview;

    /* Array of tabs used by the viewer. */
    static std::array<lv_obj_t *, NUM_TAB_INDICES> tab_list;

    /* App wifi network connection. */
    static wifi::WifiNetwork wifi_network;

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

    /* Show the wifi settings under the Settings tab. */
    void show_wifi_settings_page();

    /* Show the adhoc information page */
    void show_adhoc_settings_page();

    /* Show the wifi page for joining a network, under the Settings tab. */
    void show_wifi_join_network_page(std::string & ssid);

    /* Show the wifi page for an ongoing connection attempt, under the
     * Settings tab.
     */
    void show_wifi_joining_page(const std::string & ssid, bool adhoc = false);
};

/* Initialize the application and HAL drivers. */
void init();

/* Run the application. */
void run();

/* Close the application cleanly. */
void close();

/* Go to a specific page index. This can include default tab pages, if that
 * was the last page visited, and will switch tabs accordingly.
 */
void go_to_page_index(page_index_t index,
                      void *       param             = NULL,
                      bool         tab_click         = false,
                      bool         cli_notifications = false);

/* Go back to the previously viewed page. */
void go_to_previous_page();

/* Show the notification splash on the screen. */
void show_notification(std::string  message,
                       bool         print_to_cli   = false,
                       page_index_t route_on_close = NUM_PAGE_INDICES);

}    // namespace app

#endif    // APP_HPP__
