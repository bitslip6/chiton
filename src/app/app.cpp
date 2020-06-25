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

#include <SDL2/SDL.h>
#include <assert.h>
#include <memory>

#include "app_hal.h"
#include "app_theme.hpp"
#include "lvgl/lvgl.h"

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

/* Callback for selecting different tabs. */
void tab_select_event_cb(lv_obj_t * tabview, lv_event_t e);

/* Callback for selecting a WiFi network with which to connect. */
void network_select_btn_event_cb(lv_obj_t * btn, lv_event_t e);

/* Callback for text area events.
 * This is generalized for most text areas, since they'll all do basically the
 * same thing.
 */
void ta_event_cb(lv_obj_t * ta, lv_event_t e);

/* Callback for on-screen keyboard events. */
void kb_event_cb(lv_obj_t * kb, lv_event_t e);

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
        lv_obj_set_event_cb(btn, network_select_btn_event_cb);
    }
}

void AppWindow::show_wifi_connection_page()
{
    this->current_page_index = PAGE_INDEX_WIFI_CONNECTION;

    lv_obj_t * tab = this->tab_list[TAB_INDEX_SETTINGS];
    lv_page_clean(tab);
    lv_page_set_scrl_layout(tab, LV_LAYOUT_PRETTY_TOP);

    lv_coord_t grid_w = lv_page_get_width_grid(tab, 1, 1);

    static lv_style_t style_box = theme::get_style_box(LV_ALIGN_OUT_TOP_MID);

    lv_obj_t * cont = lv_cont_create(tab, NULL);
    lv_cont_set_layout(cont, LV_LAYOUT_PRETTY_MID);
    lv_obj_set_size(cont, grid_w, 90);
    lv_obj_add_style(cont, LV_CONT_PART_MAIN, &style_box);
    lv_obj_set_drag_parent(cont, true);
    lv_obj_set_style_local_value_str(
        cont, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, "Join Network");

    std::string header = std::string("The Network SSID");

    lv_obj_t * lbl_ssid = lv_label_create(cont, NULL);
    lv_label_set_long_mode(lbl_ssid, LV_LABEL_LONG_SROLL_CIRC);
    lv_label_set_align(lbl_ssid, LV_LABEL_ALIGN_CENTER);
    lv_label_set_text(lbl_ssid, header.c_str());
    lv_obj_set_width(lbl_ssid, grid_w);

    lv_obj_t * ta_pwd = lv_textarea_create(cont, NULL);
    lv_cont_set_fit2(ta_pwd, LV_FIT_PARENT, LV_FIT_NONE);
    lv_textarea_set_text(ta_pwd, "");
    lv_textarea_set_placeholder_text(ta_pwd, "Password");
    lv_textarea_set_one_line(ta_pwd, true);
    lv_textarea_set_pwd_mode(ta_pwd, true);
    lv_textarea_set_cursor_hidden(ta_pwd, true);
    lv_obj_set_event_cb(ta_pwd, ta_event_cb);
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

    app_hal_init_monitor_display();
    app_hal_init_mouse_input_device();
    std::unique_ptr<AppWindow> window(new AppWindow());
    window->build();

    /* Tick init.
     * You have to call 'lv_tick_inc()' in periodically to inform LittelvGL
     * about how much time were elapsed Create an SDL thread to do this
     */
    SDL_CreateThread(app_hal_thread_update_timer, "update_hal_timer", NULL);
}

void refresh()
{
    /* Periodically call the lv_task handler.
     * It could be done in a timer interrupt or an OS task too.
     */
    lv_task_handler();
}

void go_to_page_index(page_index_t index, bool tab_click)
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

    case PAGE_INDEX_WIFI_SETTINGS:
        window->show_wifi_settings_page();
        break;

    case PAGE_INDEX_WIFI_CONNECTION:
        window->show_wifi_connection_page();
        break;

    default:
        show_notification("Unknown page.");
        break;
    }
}

void show_notification(std::string message)
{
    lv_obj_t * parent = lv_scr_act();

    static const char * btns[] = { "Ok", "" };

    lv_obj_t * msgbox = lv_msgbox_create(parent, NULL);
    lv_msgbox_add_btns(msgbox, btns);
    lv_msgbox_set_text(msgbox, message.c_str());

    lv_obj_t * btnm = lv_msgbox_get_btnmatrix(msgbox);
    lv_btnmatrix_set_btn_ctrl(btnm, 0, LV_BTNMATRIX_CTRL_CHECK_STATE);
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

    std::unique_ptr<AppWindow> window(new AppWindow());
    if (window->last_page_index != window->current_page_index) {
        go_to_page_index(window->last_page_index);
    }
}

void wifi_btn_event_cb([[gnu::unused]] lv_obj_t * btn, lv_event_t e)
{
    /* Do nothing if not clicked. */
    if (e != LV_EVENT_CLICKED) {
        return;
    }

    go_to_page_index(PAGE_INDEX_WIFI_SETTINGS);
}

void tab_select_event_cb(lv_obj_t * tabview, lv_event_t e)
{
    if (e != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    go_to_page_index((page_index_t)lv_tabview_get_tab_act(tabview), true);
}

void network_select_btn_event_cb([[gnu::unused]] lv_obj_t * btn, lv_event_t e)
{
    if (e != LV_EVENT_CLICKED) {
        return;
    }

    go_to_page_index(PAGE_INDEX_WIFI_CONNECTION);
}

void ta_event_cb(lv_obj_t * ta, lv_event_t e)
{
    std::unique_ptr<AppWindow> window(new AppWindow());

    if (e == LV_EVENT_RELEASED) {
        if (window->keyboard == NULL) {
            lv_obj_set_height(window->tabview, LV_VER_RES / 2);
            window->keyboard = lv_keyboard_create(lv_scr_act(), NULL);
            lv_obj_set_event_cb(window->keyboard, kb_event_cb);
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
        lv_obj_set_height(window->tabview, LV_VER_RES);
        lv_obj_del(window->keyboard);
        window->keyboard = NULL;
    }
}

}    // namespace app
