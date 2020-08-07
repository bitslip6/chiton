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
 * @copyright       © 2020 Jabil, Inc. All Rights Reserved.
 *
 *****************************************************************************/

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include "app.hpp"

#include <assert.h>
#include <chrono>
#include <memory>
#include <thread>

#ifdef USE_MONITOR
    #include <SDL2/SDL.h>
#endif /* ! USE_MONITOR */

#include <unistd.h>

#include "app_hal.h"
#include "app_theme.hpp"
#include "lvgl/lvgl.h"
#include "utils.hpp"

//-----------------------------------------------------------------------------
//  Forward Declarations
//-----------------------------------------------------------------------------

namespace app
{
extern "C" {

/* Image (C-array) of the Wifi icon */
LV_IMG_DECLARE(app_icon_wifi);

/* Callback for the Wifi button. */
void back_btn_event_cb(lv_obj_t * btn, lv_event_t e);

/* Callback for the Wifi button. */
void wifi_btn_event_cb(lv_obj_t * btn, lv_event_t e);

/* Callback for the AdHoc button. */
void adhoc_btn_event_cb(lv_obj_t * btn, lv_event_t e);

/* Callback for selecting different tabs. */
void tab_select_event_cb(lv_obj_t * tabview, lv_event_t e);

/* Callback for selecting a WiFi network with which to connect. */
void wifi_network_select_btn_event_cb(lv_obj_t * btn, lv_event_t e);

/* Callback for joining a Wifi network. */
void wifi_join_network_btn_event_cb(lv_obj_t * btn, lv_event_t e);

/* Callback for text area events.
 * This is generalized for most text areas, since they'll all do basically the
 * same thing.
 */
void ta_event_cb(lv_obj_t * ta, lv_event_t e);

/* Callback for on-screen keyboard events. */
void kb_event_cb(lv_obj_t * kb, lv_event_t e);

/* Callback for clicking on a notification splash button. */
void notification_btn_event_cb(lv_obj_t * msgbox, lv_event_t e);

/* Callback for the LVGL task that checks the status of an ongoing network
 * connection.
 */
void check_network_connection_task_cb(lv_task_t * task);

void adhoc_network_start_btn_event_cb(lv_obj_t * btn, lv_event_t event);

}    // extern "C"
}    // namespace app

//-----------------------------------------------------------------------------
//  AppWindow
//-----------------------------------------------------------------------------

namespace app
{
page_index_t AppWindow::last_page_index                     = PAGE_INDEX_HOME;
page_index_t AppWindow::current_page_index                  = PAGE_INDEX_HOME;
bool         AppWindow::built                               = false;
lv_obj_t *   AppWindow::keyboard                            = NULL;
lv_obj_t *   AppWindow::page_display                        = NULL;
lv_obj_t *   AppWindow::tabview                             = NULL;
std::array<lv_obj_t *, NUM_TAB_INDICES> AppWindow::tab_list = {};
wifi::WifiNetwork                       AppWindow::wifi_network;

AppWindow::AppWindow()
{
}

AppWindow::~AppWindow()
{
}

void AppWindow::build()
{
    if (this->is_built()) {
        return;
    }

    this->wifi_network = wifi::WifiNetwork();

    lv_obj_t * scr = lv_obj_create(NULL, NULL);
    lv_scr_load(scr);

    lv_obj_t * parent = lv_scr_act();

    this->tabview = lv_tabview_create(parent, NULL);
    lv_obj_set_event_cb(this->tabview, tab_select_event_cb);

    this->tab_list[TAB_INDEX_HOME] =
        lv_tabview_add_tab(this->tabview, LV_SYMBOL_HOME);
    this->tab_list[TAB_INDEX_VIDEO] =
        lv_tabview_add_tab(this->tabview, LV_SYMBOL_VIDEO);
    this->tab_list[TAB_INDEX_SETTINGS] =
        lv_tabview_add_tab(this->tabview, LV_SYMBOL_SETTINGS);

    lv_obj_set_style_local_pad_left(this->tabview,
                                    LV_TABVIEW_PART_TAB_BG,
                                    LV_STATE_DEFAULT,
                                    LV_HOR_RES / 2);

    lv_obj_t * btn = lv_btn_create(parent, NULL);
    lv_obj_set_size(btn, 35, 35);
    lv_obj_set_style_local_value_str(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_SYMBOL_LEFT);
    lv_obj_set_event_cb(btn, back_btn_event_cb);
    lv_obj_set_pos(btn, LV_DPX(10), LV_DPX(10));
    lv_obj_set_style_local_value_align(
        btn, LV_SWITCH_PART_BG, LV_STATE_DEFAULT, LV_ALIGN_CENTER);

    this->show_home_page();
    this->show_settings_page();

    /* Start from the home page. */
    this->current_page_index = PAGE_INDEX_HOME;
    this->page_display       = parent;
    this->built              = true;
}

bool AppWindow::has_wifi_connection()
{
    return (this->wifi_network.get_ssid().length() > 0);
}

void AppWindow::show_home_page()
{
    this->current_page_index = PAGE_INDEX_HOME;

    lv_obj_t * tab = this->tab_list[TAB_INDEX_HOME];
    lv_page_clean(tab);
    lv_page_set_scrl_layout(tab, LV_LAYOUT_CENTER);

    lv_coord_t grid_h = lv_page_get_height_grid(tab, 1, 1);
    lv_coord_t grid_w = lv_page_get_width_grid(tab, 1, 1);

    lv_obj_t * cont = lv_cont_create(tab, NULL);
    lv_cont_set_layout(cont, LV_LAYOUT_CENTER);
    lv_obj_set_size(cont, grid_w, grid_h);
    lv_obj_set_pos(cont, 0, 0);

    lv_obj_t * header = lv_label_create(cont, NULL);
    lv_obj_set_style_local_value_font(
        header, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_theme_get_font_normal());
    lv_label_set_long_mode(header,
                           LV_LABEL_LONG_BREAK);       /*Break the long lines*/
    lv_label_set_align(header, LV_LABEL_ALIGN_CENTER); /*Center aligned lines*/
    lv_label_set_text(header, "Greetings from the Chiton Test App!");
    lv_obj_set_width(header, 250);
}

void AppWindow::show_settings_page()
{
    this->current_page_index = PAGE_INDEX_SETTINGS;

    lv_obj_t * tab = this->tab_list[TAB_INDEX_SETTINGS];
    lv_page_clean(tab);
    lv_page_set_scrl_layout(tab, LV_LAYOUT_PRETTY_MID);

    lv_coord_t grid_h = lv_page_get_height_grid(tab, 1, 1);
    lv_coord_t grid_w = lv_page_get_width_grid(tab, 1, 1);

    lv_obj_t * btn;
    lv_obj_t * list = lv_list_create(tab, NULL);
    lv_list_set_scroll_propagation(list, true);
    lv_obj_set_size(list, grid_w, grid_h);
    lv_obj_set_pos(list, 0, 0);

    btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, "Wifi");
    lv_obj_set_event_cb(btn, wifi_btn_event_cb);

    btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, "Wifi Hotspot");
    lv_obj_set_event_cb(btn, adhoc_btn_event_cb);
}

void AppWindow::show_wifi_settings_page()
{
    this->current_page_index = PAGE_INDEX_WIFI_SETTINGS;

    lv_obj_t * tab = this->tab_list[TAB_INDEX_SETTINGS];
    lv_page_clean(tab);
    lv_page_set_scrl_layout(tab, LV_LAYOUT_PRETTY_MID);

    lv_coord_t grid_h = lv_page_get_height_grid(tab, 1, 1);
    lv_coord_t grid_w = lv_page_get_width_grid(tab, 1, 1);

    static lv_style_t style_box = theme::get_style_box(LV_ALIGN_OUT_TOP_LEFT);

    /* Create the Wifi status/information section. */
    lv_obj_t * cont = lv_cont_create(tab, NULL);
    lv_cont_set_layout(cont, LV_LAYOUT_PRETTY_MID);
    lv_obj_set_size(cont, grid_w, 110);
    lv_obj_add_style(cont, LV_CONT_PART_MAIN, &style_box);
    lv_obj_set_drag_parent(cont, true);
    lv_obj_set_style_local_value_str(
        cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, "Wifi");

    lv_obj_t * page = lv_page_create(cont, NULL);
    lv_obj_clean_style_list(page, LV_TABLE_PART_BG);
    lv_obj_set_size(page, grid_w, grid_h);
    lv_page_set_scroll_propagation(page, true);

    static lv_style_t style_cell = theme::get_style_table_cell();

    lv_obj_t * tbl_info = lv_table_create(page, NULL);
    lv_obj_clean_style_list(tbl_info, LV_TABLE_PART_BG);
    lv_obj_add_style(tbl_info, LV_TABLE_PART_CELL4, &style_cell);
    lv_obj_set_drag_parent(tbl_info, true);

    lv_coord_t table_w_max = lv_page_get_width_fit(page);
    lv_table_set_col_cnt(tbl_info, 2);
    lv_table_set_col_width(
        tbl_info, 0, LV_MATH_MAX(30, 3 * table_w_max / 4 - 1));
    lv_table_set_col_width(
        tbl_info, 1, LV_MATH_MAX(60, 3 * table_w_max / 4 - 1));

    /* Set up table rows/columns */
    lv_table_set_cell_value(tbl_info, 0, 0, "Connected");
    lv_table_set_cell_value(tbl_info, 1, 0, "SSID");

    /* Populate the table with network info if connected */
    if (this->has_wifi_connection()) {
        lv_table_set_cell_value(tbl_info, 0, 1, LV_SYMBOL_OK);
        lv_table_set_cell_value(
            tbl_info, 1, 1, this->wifi_network.get_ssid().c_str());
    } else {
        lv_table_set_cell_value(tbl_info, 0, 1, LV_SYMBOL_CLOSE);
        lv_table_set_cell_value(tbl_info, 1, 1, "");
    }

    /* Create the Wifi networks section. */
    cont = lv_cont_create(tab, NULL);
    lv_obj_set_size(cont, grid_w, 110);
    lv_obj_add_style(cont, LV_CONT_PART_MAIN, &style_box);
    lv_obj_set_drag_parent(cont, true);
    lv_obj_set_style_local_value_str(
        cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, "Available Networks");

    page = lv_page_create(cont, NULL);
    lv_obj_clean_style_list(page, LV_LIST_PART_BG);
    lv_obj_set_size(page, grid_w, grid_h);
    lv_page_set_scroll_propagation(page, true);

    lv_obj_t * l_networks = lv_list_create(page, NULL);
    lv_list_set_scroll_propagation(l_networks, true);
    lv_obj_set_size(l_networks, grid_w, grid_h);

    /* Populate the list with available networks */
    std::vector<wifi::WifiNetwork> v_networks = wifi::get_available_networks();

    lv_obj_t * btn;
    for (wifi::WifiNetwork network : v_networks) {
        btn = lv_list_add_btn(
            l_networks, LV_SYMBOL_WIFI, network.get_ssid().c_str());
        lv_btn_set_checkable(btn, true);
        lv_obj_set_event_cb(btn, wifi_network_select_btn_event_cb);
    }
}

void AppWindow::show_wifi_join_network_page(std::string & ssid)
{
    this->current_page_index = PAGE_INDEX_WIFI_JOIN_NETWORK;

    lv_obj_t * tab = this->tab_list[TAB_INDEX_SETTINGS];
    lv_page_clean(tab);
    lv_page_set_scrl_layout(tab, LV_LAYOUT_PRETTY_TOP);

    lv_coord_t grid_w = lv_page_get_width_grid(tab, 1, 1);

    static lv_style_t style_box = theme::get_style_box(LV_ALIGN_OUT_TOP_MID);

    lv_obj_t * cont = lv_cont_create(tab, NULL);
    lv_cont_set_layout(cont, LV_LAYOUT_PRETTY_MID);
    lv_obj_set_size(cont, grid_w, 115 + APP_BTN_HEIGHT);
    lv_obj_add_style(cont, LV_CONT_PART_MAIN, &style_box);
    lv_obj_set_drag_parent(cont, true);
    lv_obj_set_style_local_value_str(
        cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, "Join Network");

    lv_obj_t * ta_ssid;
    ta_ssid = lv_textarea_create(cont, NULL);
    lv_cont_set_fit2(ta_ssid, LV_FIT_PARENT, LV_FIT_NONE);
    lv_textarea_set_text(ta_ssid, ssid.c_str());
    lv_textarea_set_one_line(ta_ssid, true);
    lv_textarea_set_cursor_hidden(ta_ssid, true);

    lv_obj_t * ta_pwd;
    ta_pwd = lv_textarea_create(cont, ta_ssid);
    lv_textarea_set_text(ta_pwd, "");
    lv_textarea_set_placeholder_text(ta_pwd, "Password");
    lv_textarea_set_pwd_mode(ta_pwd, true);
    lv_textarea_set_pwd_show_time(ta_pwd, 0);
    lv_obj_set_event_cb(ta_pwd, ta_event_cb);

    /* Go back to the Wifi settings page when cancel is pressed on keyboard. */
    static bool go_back_on_kb_cancel = true;
    ta_pwd->user_data = (lv_obj_user_data_t)&go_back_on_kb_cancel;

    lv_obj_t * btn = lv_btn_create(cont, NULL);
    lv_btn_set_fit2(btn, LV_FIT_NONE, LV_FIT_TIGHT);
    lv_obj_set_width(btn, APP_BTN_WIDTH);
    lv_obj_t * label = lv_label_create(btn, NULL);
    lv_label_set_text(label, "Join");
    lv_obj_set_event_cb(btn, wifi_join_network_btn_event_cb);

    static CredentialTexts creds(ta_ssid, ta_pwd);
    btn->user_data = (lv_obj_user_data_t)&creds;
}

void AppWindow::show_wifi_joining_page(const std::string & ssid, bool adhoc)
{
    this->current_page_index =
        adhoc ? PAGE_INDEX_WIFI_JOINING : PAGE_INDEX_ADHOC_STARTING;

    lv_obj_t * tab = this->tab_list[TAB_INDEX_SETTINGS];

    lv_page_clean(tab);
    lv_page_set_scrl_layout(tab, LV_LAYOUT_CENTER);

    lv_coord_t grid_h = lv_page_get_height_grid(tab, 1, 1);
    lv_coord_t grid_w = lv_page_get_width_grid(tab, 1, 1);

    lv_obj_t * cont = lv_cont_create(tab, NULL);
    lv_cont_set_layout(cont, LV_LAYOUT_CENTER);
    lv_obj_set_size(cont, grid_w, grid_h);
    lv_obj_set_pos(cont, 0, 0);

    std::string prompt(adhoc ? "Setting up hotspot " : "Joining ");
    prompt.reserve(100);
    prompt.append(ssid);
    prompt.append("...");

    /* Update the app's network with the SSID. */
    this->wifi_network = wifi::WifiNetwork(ssid);

    lv_obj_t * header = lv_label_create(cont, NULL);
    lv_obj_set_style_local_value_font(
        header, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_theme_get_font_normal());
    lv_label_set_long_mode(header,
                           LV_LABEL_LONG_BREAK);       /*Break the long lines*/
    lv_label_set_align(header, LV_LABEL_ALIGN_CENTER); /*Center aligned lines*/
    lv_label_set_text(header, prompt.c_str());
    lv_obj_set_width(header, 250);

    lv_obj_t * preload = lv_spinner_create(cont, NULL);
    lv_obj_set_size(preload, 75, 75);
    lv_obj_align(preload, NULL, LV_ALIGN_CENTER, 0, 0);

    static bool adhoc_conn                      = false;
    adhoc_conn                                  = adhoc;
    const uint32_t connection_attempt_period_ms = 1000;
    lv_task_t *    check_connection_task =
        lv_task_create(check_network_connection_task_cb,
                       connection_attempt_period_ms,
                       LV_TASK_PRIO_HIGH,
                       &adhoc_conn);

    /* Task tracks its own timeout, but ensure it will not continue forever
     * by giving it some arbitrary timeout via N repeats.
     * timeout = (attempt_period * N);
     */
    lv_task_set_repeat_count(check_connection_task, 20);
}

void AppWindow::show_adhoc_settings_page()
{
    this->current_page_index = PAGE_INDEX_ADHOC_SETTINGS;

    lv_obj_t * tab = this->tab_list[TAB_INDEX_SETTINGS];
    lv_page_clean(tab);
    lv_page_set_scrl_layout(tab, LV_LAYOUT_PRETTY_MID);

    lv_coord_t grid_h = lv_page_get_height_grid(tab, 1, 1);
    lv_coord_t grid_w = lv_page_get_width_grid(tab, 1, 1);

    static lv_style_t style_box = theme::get_style_box(LV_ALIGN_OUT_TOP_LEFT);

    /* Create the Wifi status/information section. */
    lv_obj_t * cont = lv_cont_create(tab, NULL);
    lv_cont_set_layout(cont, LV_LAYOUT_PRETTY_MID);
    lv_obj_set_size(cont, grid_w, 110);
    lv_obj_add_style(cont, LV_CONT_PART_MAIN, &style_box);
    lv_obj_set_drag_parent(cont, true);
    lv_obj_set_style_local_value_str(
        cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, "Hotspot Details");

    lv_obj_t * page = lv_page_create(cont, NULL);
    lv_obj_clean_style_list(page, LV_TABLE_PART_BG);
    lv_obj_set_size(page, grid_w, grid_h);
    lv_page_set_scroll_propagation(page, true);

    static lv_style_t style_cell = theme::get_style_table_cell();

    lv_obj_t * tbl_info = lv_table_create(page, NULL);
    lv_obj_clean_style_list(tbl_info, LV_TABLE_PART_BG);
    lv_obj_add_style(tbl_info, LV_TABLE_PART_CELL4, &style_cell);
    lv_obj_set_drag_parent(tbl_info, true);

    lv_table_set_col_cnt(tbl_info, 2);
    lv_table_set_row_cnt(tbl_info, 5);


    // Set up table rows/columns //
    lv_table_set_cell_value(tbl_info, 0, 0, "SSID:");
    lv_table_set_cell_value(tbl_info, 1, 0, "PSK:");
    lv_table_set_cell_value(tbl_info, 2, 0, "IP:");

    wifi::AdhocNetwork adhoc = wifi::AdhocNetwork();

    /* Use a local variable to avoid repeatedly polling nmcli. */
    bool adhoc_is_active = adhoc.is_active();

    std::string ssid = adhoc_is_active ? adhoc.get_ssid() : LV_SYMBOL_CLOSE;
    std::string psk  = adhoc_is_active ? adhoc.get_psk() : LV_SYMBOL_CLOSE;
    std::string ip   = adhoc_is_active ? adhoc.get_ipv4() : LV_SYMBOL_CLOSE;

    lv_table_set_cell_value(tbl_info, 0, 1, ssid.c_str());
    lv_table_set_cell_value(tbl_info, 1, 1, psk.c_str());
    lv_table_set_cell_value(tbl_info, 2, 1, ip.c_str());

    lv_coord_t control_box_h = APP_BTN_HEIGHT + 20;

    cont = lv_cont_create(tab, NULL);
    lv_obj_set_size(cont, grid_w, control_box_h);
    lv_obj_add_style(cont, LV_CONT_PART_MAIN, &style_box);
    lv_obj_set_drag_parent(cont, true);
    lv_obj_set_style_local_value_str(
        cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, "Control");

    page = lv_page_create(cont, NULL);
    lv_obj_clean_style_list(page, LV_LIST_PART_BG);
    lv_obj_set_size(page, grid_w, control_box_h);
    lv_page_set_scroll_propagation(page, true);

    lv_obj_t * btn = lv_btn_create(page, NULL);
    lv_btn_set_fit2(btn, LV_FIT_NONE, LV_FIT_TIGHT);
    lv_obj_set_width(btn, APP_BTN_WIDTH);
    lv_obj_t * label = lv_label_create(btn, NULL);
    lv_label_set_text(label, "Start");
    lv_obj_set_pos(btn, 20, 12);
    lv_obj_set_state(btn,
                     adhoc_is_active ? LV_STATE_DISABLED : LV_STATE_DEFAULT);
    lv_obj_set_event_cb(btn, adhoc_network_start_btn_event_cb);

    btn = lv_btn_create(page, NULL);
    lv_btn_set_fit2(btn, LV_FIT_NONE, LV_FIT_TIGHT);
    lv_obj_set_width(btn, APP_BTN_WIDTH);
    label = lv_label_create(btn, NULL);
    lv_label_set_text(label, "Stop");
    lv_obj_set_pos(btn, APP_BTN_WIDTH + 40, 12);
    lv_obj_set_state(btn,
                     adhoc_is_active ? LV_STATE_DEFAULT : LV_STATE_DISABLED);
}

}    // namespace app

//-----------------------------------------------------------------------------
//  Namespace Functions
//-----------------------------------------------------------------------------

namespace app
{
void init()
{
    lv_init();

    /* Initialize HAL drivers as defined during compilation. */
#if USE_MONITOR
    app_hal_init_monitor_display();
#elif USE_FBDEV
    app_hal_init_framebuffer_display();
#endif /* USE_MONITOR */

    app_hal_init_input_device();

    std::unique_ptr<AppWindow> window(new AppWindow());
    window->build();
}

void run()
{
    /* Init LVGL's backend timing thread. */
#if USE_MONITOR
    /* Create thread via SDL's API. */
    SDL_Thread * thd_update_hal_timer = SDL_CreateThread(
        app_hal_thread_update_timer, "thd_update_hal_timer", NULL);

    /* Let the SDL thread clean itself up. */
    SDL_DetachThread(thd_update_hal_timer);
#else
    std::thread thd_update_hal_timer(app_hal_thread_update_timer, nullptr);

    /* Detach to run in background without blocking join.
     * This cleans up thread resources automatically on exit.
     */
    thd_update_hal_timer.detach();
#endif /* USE_MONITOR */

    /* Approx. 60 FPS */
    const auto refresh_rate_ms = 16;

    /* Run the app, refreshing at a specific rate. */
    while (1) {
        /* Periodically call the lv_task handler.
         * It could be done in a timer interrupt or an OS task too.
         */
        lv_task_handler();

        std::this_thread::sleep_for(std::chrono::milliseconds(refresh_rate_ms));
    }
}

void go_to_page_index(page_index_t index,
                      void *       param,
                      bool         tab_click,
                      bool         cli_notifications)
{
    std::unique_ptr<AppWindow> window(new AppWindow());

    /* Update the last seen page index. */
    window->last_page_index = window->current_page_index;

    switch (index) {
    case PAGE_INDEX_HOME:
        if (!tab_click) {
            lv_tabview_set_tab_act(window->tabview, TAB_INDEX_HOME, LV_ANIM_ON);
        }
        window->show_home_page();
        break;

    case PAGE_INDEX_VIDEO:
        if (!tab_click) {
            lv_tabview_set_tab_act(
                window->tabview, TAB_INDEX_VIDEO, LV_ANIM_ON);
        }
        break;

    case PAGE_INDEX_SETTINGS:
        if (!tab_click) {
            lv_tabview_set_tab_act(
                window->tabview, TAB_INDEX_SETTINGS, LV_ANIM_ON);
        }
        window->show_settings_page();
        break;

    case PAGE_INDEX_ADHOC_SETTINGS:
        window->show_adhoc_settings_page();
        break;

    case PAGE_INDEX_WIFI_SETTINGS:
        window->show_wifi_settings_page();
        break;

    case PAGE_INDEX_WIFI_JOIN_NETWORK: {
        if (param == NULL) {
            show_notification("No network has been selected.",
                              cli_notifications,
                              PAGE_INDEX_WIFI_SETTINGS);
        } else {
            std::string ssid = *(std::string *)param;
            window->show_wifi_join_network_page(ssid);
        }
    } break;

    case PAGE_INDEX_WIFI_JOINING: {
        if (param == NULL) {
            show_notification("Invalid SSID; cannot join network.",
                              cli_notifications,
                              PAGE_INDEX_WIFI_SETTINGS);
        } else {
            window->show_wifi_joining_page(*(std::string *)param);
        }
    } break;

    case PAGE_INDEX_ADHOC_STARTING: {
        if (param == NULL) {
            show_notification("Invalid SSID; cannot create hotspot.",
                              cli_notifications,
                              PAGE_INDEX_ADHOC_SETTINGS);
        } else {
            window->show_wifi_joining_page(*(std::string *)param, true);
        }
    } break;

    case NUM_PAGE_INDICES:
    default:
        show_notification("Could not find page.", cli_notifications);
        break;
    }
}

void go_to_previous_page()
{
    std::unique_ptr<AppWindow> window(new AppWindow());
    if (window->last_page_index != window->current_page_index) {
        if ((window->last_page_index == PAGE_INDEX_WIFI_JOINING)
            || (window->last_page_index == PAGE_INDEX_ADHOC_STARTING)) {
            window->last_page_index = PAGE_INDEX_SETTINGS;
        }

        go_to_page_index(window->last_page_index);
    }
}

void show_notification(std::string  message,
                       bool         print_to_cli,
                       page_index_t route_on_close)
{
    if (print_to_cli) {
        printf("%s\n", message.c_str());
        return;
    }

    lv_obj_t * parent = lv_scr_act();

    static const char * btns[] = { "Ok", "" };

    lv_obj_t * msgbox = lv_msgbox_create(parent, NULL);
    lv_msgbox_add_btns(msgbox, btns);
    lv_msgbox_set_text(msgbox, message.c_str());
    lv_obj_set_event_cb(msgbox, notification_btn_event_cb);

    lv_obj_t * btnm = lv_msgbox_get_btnmatrix(msgbox);
    lv_btnmatrix_set_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_CHECK_STATE);

    if (route_on_close < NUM_PAGE_INDICES) {
        static page_index_t index = route_on_close;
        msgbox->user_data         = (lv_obj_user_data_t)&index;
    }
}

}    // namespace app

//---------------------------------------------------------------------
// Callbacks
//---------------------------------------------------------------------

namespace app
{
void back_btn_event_cb([[gnu::unused]] lv_obj_t * btn, lv_event_t e)
{
    /* Do nothing if not clicked. */
    if (e != LV_EVENT_CLICKED) {
        return;
    }

    go_to_previous_page();
}

void wifi_btn_event_cb([[gnu::unused]] lv_obj_t * btn, lv_event_t e)
{
    /* Do nothing if not clicked. */
    if (e != LV_EVENT_CLICKED) {
        return;
    }

    // usleep(3000000);    // BUGFIX: temporarily here to fix wifi list not
    // showin
    //                     // up after setting up the device in adhoc mode
    go_to_page_index(PAGE_INDEX_WIFI_SETTINGS);
}

void adhoc_btn_event_cb([[gnu::unused]] lv_obj_t * btn, lv_event_t e)
{
    /* Do nothing if not clicked. */
    if (e != LV_EVENT_CLICKED) {
        return;
    }

    go_to_page_index(PAGE_INDEX_ADHOC_SETTINGS);
}

void tab_select_event_cb(lv_obj_t * tabview, lv_event_t e)
{
    if (e != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    go_to_page_index((page_index_t)lv_tabview_get_tab_act(tabview), NULL, true);
}

void wifi_network_select_btn_event_cb(lv_obj_t * btn, lv_event_t e)
{
    if (e != LV_EVENT_CLICKED) {
        return;
    }
    std::string ssid(lv_list_get_btn_text(btn));
    go_to_page_index(PAGE_INDEX_WIFI_JOIN_NETWORK, &ssid);
}

void wifi_join_network_btn_event_cb(lv_obj_t * btn, lv_event_t e)
{
    if (e != LV_EVENT_CLICKED) {
        return;
    }

    CredentialTexts * credentials = (CredentialTexts *)btn->user_data;

    if (credentials == NULL) {
        show_notification("Invalid credentials; cannot join network.",
                          false,
                          PAGE_INDEX_WIFI_SETTINGS);
    } else if (credentials->get_pwd().length() == 0) {
        show_notification("Empty passwords are not allowed.");
    } else {
        std::string ssid     = credentials->get_id();
        std::string password = credentials->get_pwd();
        credentials->erase();

        /* Show the connecting page */
        go_to_page_index(PAGE_INDEX_WIFI_JOINING, &ssid);

        /* Kick off the connection process. */
        wifi::connect_to_network(ssid, password);
    }
}

void ta_event_cb(lv_obj_t * ta, lv_event_t e)
{
    std::unique_ptr<AppWindow> window(new AppWindow());

    if (e == LV_EVENT_RELEASED) {
        if (window->keyboard == NULL) {
            lv_obj_set_height(window->tabview, LV_VER_RES / 2);
            window->keyboard = lv_keyboard_create(lv_scr_act(), NULL);
            lv_obj_set_event_cb(window->keyboard, kb_event_cb);

            /* Pass the text area data to the keyboard */
            window->keyboard->user_data = ta->user_data;

            lv_indev_wait_release(lv_indev_get_act());
        }
        lv_textarea_set_cursor_hidden(ta, false);

        lv_obj_t * active_tab = lv_tabview_get_tab(
            window->tabview, lv_tabview_get_tab_act(window->tabview));
        lv_page_focus(active_tab, lv_textarea_get_label(ta), LV_ANIM_ON);
        lv_keyboard_set_textarea(window->keyboard, ta);
    } else if (e == LV_EVENT_DEFOCUSED) {
        lv_textarea_set_cursor_hidden(ta, true);
    }
}

void kb_event_cb(lv_obj_t * kb, lv_event_t e)
{
    std::unique_ptr<AppWindow> window(new AppWindow());
    lv_keyboard_def_event_cb(window->keyboard, e);

    if ((e == LV_EVENT_CANCEL) && (window->keyboard != NULL)) {
        lv_obj_user_data_t user_data = kb->user_data;
        bool go_back = (kb->user_data != NULL) ? *(bool *)user_data : false;

        lv_obj_set_height(window->tabview, LV_VER_RES);
        lv_obj_del(window->keyboard);
        window->keyboard = NULL;

        if (go_back) {
            go_to_previous_page();
        }
    } else if ((e == LV_EVENT_APPLY) && (window->keyboard != NULL)) {
        lv_obj_set_height(window->tabview, LV_VER_RES);
        lv_obj_del(window->keyboard);
        window->keyboard = NULL;
    }
}

void notification_btn_event_cb(lv_obj_t * msgbox, lv_event_t event)
{
    if (event != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    // Only one button; assume "OK" triggered this event.
    lv_msgbox_start_auto_close(msgbox, 0);

    lv_obj_user_data_t user_data = msgbox->user_data;
    if (user_data != NULL) {
        go_to_page_index(*(page_index_t *)user_data, NULL, false, false);
    }
}

void check_network_connection_task_cb(lv_task_t * task)
{
    static size_t check_count = 0;

    check_count += 1;

    const size_t max_check_attempts = 10;
    bool         timed_out          = (check_count >= max_check_attempts);
    static bool  in_progress        = false;
    bool         stop_task          = false;

    bool         adhoc = *(bool *)task->user_data;
    page_index_t goto_page =
        adhoc ? PAGE_INDEX_ADHOC_SETTINGS : PAGE_INDEX_WIFI_SETTINGS;

    wifi::ConnectionState status = wifi::get_connection_status();

    switch (status) {
    case wifi::ConnectionState::CONNECTING:
        in_progress = true;
        break;

    case wifi::ConnectionState::CONNECTED: {
        std::string result(adhoc ? "Setup" : "Connection");
        result.append(" successful!");
        in_progress = false;
        stop_task   = true;
        show_notification(result, false, goto_page);
    } break;

    case wifi::ConnectionState::DISCONNECTED: {
        if (in_progress || timed_out) {
            in_progress = false;
            stop_task   = true;

            /* Ensure the app doesn't track a network that wasn't connected . */
            std::unique_ptr<AppWindow> window(new AppWindow());
            window->wifi_network = wifi::WifiNetwork("");

            std::string result(adhoc ? "setup hotspot." : "connect to network");
            show_notification("Could not " + result, false, goto_page);
        }
    } break;

    default:
        printf("Invalid connection state; should not be here!\n");
        stop_task = true;
        break;
    }

    if (stop_task) {
        lv_task_del(task);
    }
}

void adhoc_network_start_btn_event_cb([[gnu::unused]] lv_obj_t * btn,
                                      lv_event_t                 event)
{
    if (event != LV_EVENT_CLICKED) {
        return;
    }

    wifi::AdhocNetwork adhoc = wifi::AdhocNetwork();

    if (adhoc.exists() == false) {
        bool setup_failed = adhoc.configure();
        if (setup_failed) {
            show_notification("Failed to configure adhoc network.");
            return;
        }
    }

    adhoc.start();

    std::string ssid(adhoc.get_ssid());
    go_to_page_index(PAGE_INDEX_ADHOC_STARTING, &ssid);
}

}    // namespace app
