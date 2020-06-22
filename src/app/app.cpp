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
#include <iostream>
#include <memory>

#include "app_hal.h"
#include "app_theme.h"
#include "lvgl/lvgl.h"

//-----------------------------------------------------------------------------
//  Forward Declarations
//-----------------------------------------------------------------------------

namespace app
{
extern "C" {

/* Image (C-array) of the Wifi icon */
LV_IMG_DECLARE(app_icon_wifi);
}

LV_EVENT_CB_DECLARE(
    video_btn_event_cb); /*< Callback for pressing the Video button. */
LV_EVENT_CB_DECLARE(
    settings_btn_event_cb); /*< Callback for pressing the Settings button. */
LV_EVENT_CB_DECLARE(
    back_btn_event_cb); /*< Callback for pressing the Back button. */
LV_EVENT_CB_DECLARE(
    home_btn_event_cb); /*< Callback for pressing the Back button. */
LV_EVENT_CB_DECLARE(wifi_btn_event_cb); /*< Callback for the Wifi button. */

/* Callback for animating background color changes. */
void animate_background_color_cb(lv_anim_t * a, lv_anim_value_t v);

} // namespace app

//-----------------------------------------------------------------------------
//  AppWindow
//-----------------------------------------------------------------------------

namespace app
{
page_index_t      AppWindow::last_page_index    = PAGE_INDEX_HOME;
page_index_t      AppWindow::current_page_index = PAGE_INDEX_HOME;
bool              AppWindow::built              = false;
lv_obj_t *        AppWindow::page_display       = NULL;
lv_obj_t *        AppWindow::status_bar         = NULL;
struct Background AppWindow::bg_top;
struct Background AppWindow::bg_bot;

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

    this->bg_top.previous_color = APP_THEME_WHITE;
    this->bg_top.active_color   = APP_THEME_WHITE;

    this->bg_bot.previous_color = APP_THEME_WHITE;
    this->bg_bot.active_color   = APP_THEME_WHITE;

    lv_theme_t * th = app_theme_init(LV_COLOR_BLACK,
                                     LV_COLOR_BLACK,
                                     0,
                                     &lv_font_montserrat_14,
                                     &lv_font_montserrat_22,
                                     &lv_font_montserrat_28,
                                     &lv_font_montserrat_32);
    lv_theme_set_act(th);

    lv_obj_t * scr = lv_obj_create(NULL, NULL);
    lv_scr_load(scr);

    lv_obj_t * parent = lv_scr_act();

    /* Configure the bottom (main) background. */
    this->bg_bot.obj = lv_obj_create(parent, NULL);
    lv_obj_clean_style_list(this->bg_bot.obj, LV_OBJ_PART_MAIN);
    lv_obj_set_style_local_bg_opa(
        this->bg_bot.obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_obj_set_style_local_bg_color(this->bg_bot.obj,
                                    LV_OBJ_PART_MAIN,
                                    LV_STATE_DEFAULT,
                                    this->bg_bot.active_color);
    lv_obj_set_size(this->bg_bot.obj, LV_HOR_RES, LV_VER_RES); // correct?
    lv_obj_set_y(this->bg_bot.obj, APP_BG_FULL);

    /* Configure the top (bar) background. */
    this->bg_top.obj = lv_obj_create(parent, NULL);
    lv_obj_clean_style_list(this->bg_top.obj, LV_OBJ_PART_MAIN);
    lv_obj_set_style_local_bg_opa(
        this->bg_top.obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_obj_set_style_local_bg_color(this->bg_top.obj,
                                    LV_OBJ_PART_MAIN,
                                    LV_STATE_DEFAULT,
                                    this->bg_top.active_color);
    lv_obj_set_size(this->bg_top.obj, LV_HOR_RES, LV_VER_RES); // Correct?
    lv_obj_set_y(this->bg_top.obj, APP_BG_NORMAL);

    /* FIXME: https://github.com/bitslip6/chiton/issues/5 */
    this->update_status_bar(0);

    /* Start from the home page. */
    this->page_display = parent;
    this->built        = true;
}

void AppWindow::update_status_bar(uint32_t delay)
{
    lv_obj_t * parent = lv_scr_act();

    this->status_bar = lv_cont_create(parent, NULL);
    lv_obj_set_size(
        this->status_bar, APP_STATUS_BAR_WIDTH, APP_STATUS_BAR_HEIGHT);
    lv_obj_clean_style_list(this->status_bar, LV_CONT_PART_MAIN);
    lv_obj_align(this->status_bar, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);

    lv_obj_t * btn = lv_btn_create(this->status_bar, NULL);
    lv_theme_apply(btn, (lv_theme_style_t)APP_THEME_BTN_BORDER);
    lv_obj_set_size(btn, 35, 35);
    lv_obj_set_style_local_value_str(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, "H");
    lv_obj_set_event_cb(btn, home_btn_event_cb);
    lv_obj_align_origo(btn, NULL, LV_ALIGN_IN_LEFT_MID, 25, 0);
    animate_in(btn, delay);

    btn = lv_btn_create(this->status_bar, NULL);
    lv_theme_apply(btn, (lv_theme_style_t)APP_THEME_BTN_BORDER);
    lv_obj_set_size(btn, 35, 35);
    lv_obj_set_style_local_value_str(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, "<--");
    lv_obj_set_event_cb(btn, back_btn_event_cb);
    lv_obj_align_origo(btn, NULL, LV_ALIGN_IN_LEFT_MID, 70, 0);
    animate_in(btn, delay);

    lv_obj_t * icon = lv_img_create(this->status_bar, NULL);
    lv_img_set_src(icon, &app_icon_wifi);
    lv_obj_align_origo(icon, NULL, LV_ALIGN_IN_RIGHT_MID, -80, 0);
    animate_in(icon, delay);

    btn = lv_btn_create(this->status_bar, NULL);
    lv_theme_apply(btn, (lv_theme_style_t)APP_THEME_BTN_BORDER);
    lv_obj_set_size(btn, 35, 35);
    lv_obj_set_style_local_value_str(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, "S");
    lv_obj_set_event_cb(btn, settings_btn_event_cb);
    lv_obj_align_origo(btn, NULL, LV_ALIGN_IN_RIGHT_MID, -25, 0);
    animate_in(btn, delay);
}

void AppWindow::show_home_page(uint32_t delay)
{
    this->current_page_index = PAGE_INDEX_HOME;

    lv_obj_t * parent = lv_scr_act();

    animate_background(
        APP_NO_DELAY, APP_THEME_DARK_BLUE, APP_THEME_WHITE, false);

    init_page(&this->page_display, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t * header = lv_label_create(this->page_display, NULL);
    lv_obj_set_style_local_value_font(
        header, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_theme_get_font_normal());
    lv_label_set_long_mode(header,
                           LV_LABEL_LONG_BREAK);     /*Break the long lines*/
    lv_label_set_align(header, LV_LABEL_ALIGN_LEFT); /*Center aligned lines*/
    lv_label_set_text(header, "Home");
    lv_obj_set_width(header, 150);
    lv_obj_set_pos(header, APP_PAGE_HEADER_X, APP_PAGE_HEADER_Y);
    lv_obj_fade_in(header, APP_ANIM_TIME * 2, delay + APP_ANIM_FADE_IN_TIME);

    lv_obj_t * btn = lv_btn_create(this->page_display, NULL);
    lv_theme_apply(btn, (lv_theme_style_t)APP_THEME_BTN_CIRCLE);
    lv_obj_set_size(btn, APP_BTN_WIDTH, APP_BTN_HEIGHT);
    lv_obj_set_style_local_value_str(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, "Video");
    lv_obj_set_style_local_value_font(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_theme_get_font_normal());
    lv_obj_set_style_local_bg_color(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, APP_THEME_GREEN);
    lv_obj_set_style_local_bg_color(
        btn,
        LV_OBJ_PART_MAIN,
        LV_STATE_PRESSED,
        lv_color_darken(APP_THEME_GREEN, LV_OPA_20));
    lv_obj_align_origo(btn, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_obj_fade_in(btn, APP_ANIM_TIME * 2, delay + APP_ANIM_FADE_IN_TIME);
    lv_obj_set_event_cb(btn, video_btn_event_cb);
}

void AppWindow::show_error_page(std::string message, uint32_t delay)
{
    this->last_page_index    = this->current_page_index;
    this->current_page_index = PAGE_INDEX_ERROR;

    lv_obj_t * parent = lv_scr_act();

    animate_background(50, APP_THEME_RED, APP_THEME_RED, true);
    delay += APP_ANIM_DELAY;

    lv_obj_t * err_txt = lv_label_create(parent, NULL);
    lv_label_set_text(err_txt, "ERROR:");
    lv_theme_apply(err_txt, (lv_theme_style_t)APP_THEME_LABEL_WHITE);
    lv_obj_align(err_txt, parent, LV_ALIGN_IN_TOP_MID, 0, 10);

    lv_obj_t * txt = lv_label_create(parent, NULL);
    lv_label_set_text(txt, message.c_str());
    lv_label_set_long_mode(txt, LV_LABEL_LONG_BREAK);
    lv_label_set_align(txt, LV_LABEL_ALIGN_CENTER);
    lv_theme_apply(txt, (lv_theme_style_t)APP_THEME_LABEL_WHITE);
    lv_obj_set_pos(txt, 55, 85);
    lv_obj_set_width(txt, 220);

    lv_obj_t * btn = lv_btn_create(lv_scr_act(), NULL);
    lv_theme_apply(btn, (lv_theme_style_t)APP_THEME_BTN_BORDER);
    lv_obj_set_size(btn, APP_BTN_WIDTH, APP_BTN_HEIGHT);
    lv_obj_align(btn, parent, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_local_value_str(
        btn, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, "Back");
    lv_obj_set_event_cb(btn, back_btn_event_cb);

    animate_in(txt, delay);
    delay += APP_ANIM_DELAY;

    animate_in(btn, delay);
    delay += APP_ANIM_DELAY;

    animate_in(btn, delay);
}

void AppWindow::show_settings_page(uint32_t delay)
{
    this->current_page_index = PAGE_INDEX_SETTINGS;

    animate_background(delay, APP_THEME_DARK_BLUE, APP_THEME_WHITE, false);

    init_page(&this->page_display, LV_SCROLLBAR_MODE_ON);

    lv_obj_t * header = lv_label_create(this->page_display, NULL);
    lv_obj_set_style_local_value_font(
        header, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_theme_get_font_normal());
    lv_label_set_long_mode(header,
                           LV_LABEL_LONG_BREAK);     /*Break the long lines*/
    lv_label_set_align(header, LV_LABEL_ALIGN_LEFT); /*Center aligned lines*/
    lv_label_set_text(header, "Settings");
    lv_obj_set_width(header, 150);
    lv_obj_set_pos(header, APP_PAGE_HEADER_X, APP_PAGE_HEADER_Y);
    lv_obj_fade_in(header, APP_ANIM_TIME * 2, delay + APP_ANIM_FADE_IN_TIME);

    lv_obj_t * btn;
    lv_obj_t * list = lv_list_create(this->page_display, NULL);
    lv_list_set_scroll_propagation(list, true);
    lv_obj_set_size(list, APP_PAGE_WIDTH, APP_PAGE_HEIGHT - 80);
    lv_obj_set_pos(list, 0, APP_PAGE_HEADER_HEIGHT + 5);
    animate_in(list, delay);

    btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, "Wifi");
    lv_obj_set_event_cb(btn, wifi_btn_event_cb);
}

void AppWindow::show_wifi_settings_page(uint32_t delay)
{
    this->current_page_index = PAGE_INDEX_WIFI_SETTINGS;

    animate_background(delay, APP_THEME_DARK_BLUE, APP_THEME_WHITE, false);

    init_page(&this->page_display, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t * btn = lv_btn_create(this->status_bar, NULL);
    lv_theme_apply(btn, (lv_theme_style_t)APP_THEME_BTN_CIRCLE);
    lv_obj_set_size(btn, APP_BTN_WIDTH, APP_BTN_HEIGHT);
    lv_obj_set_style_local_value_str(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, "Back");
    lv_obj_set_style_local_value_font(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_theme_get_font_normal());
    lv_obj_set_style_local_bg_color(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, APP_THEME_GREEN);
    lv_obj_set_style_local_bg_color(
        btn,
        LV_OBJ_PART_MAIN,
        LV_STATE_PRESSED,
        lv_color_darken(APP_THEME_GREEN, LV_OPA_20));
    lv_obj_fade_in(btn, APP_ANIM_TIME * 2, delay + APP_ANIM_FADE_IN_TIME);
    lv_obj_set_event_cb(btn, back_btn_event_cb);
    lv_obj_set_pos(btn, APP_PAGE_WIDTH / 2, APP_PAGE_HEIGHT / 2);
}

} // namespace app

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
    window->show_home_page(0);

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

lv_obj_t * add_icon(lv_obj_t *   parent,
                    const void * src_bg,
                    const void * src_icon,
                    std::string  txt)
{
    lv_obj_t * bg = lv_img_create(parent, NULL);
    lv_obj_set_click(bg, true);
    // lv_theme_apply(bg, (lv_theme_style_t)APP_THEME_ICON);
    lv_img_set_src(bg, src_bg);
    lv_img_set_antialias(bg, false);

    lv_obj_t * icon = lv_img_create(bg, NULL);
    lv_img_set_src(icon, src_icon);
    lv_obj_set_style_local_image_recolor_opa(
        icon, LV_IMG_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);
    lv_obj_align(icon, NULL, LV_ALIGN_IN_TOP_RIGHT, -30, 30);

    lv_obj_t * label = lv_label_create(bg, NULL);
    lv_label_set_text(label, txt.c_str());
    lv_obj_align(label, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 30, -30);

    return bg;
}

void animate_in(lv_obj_t * obj, uint32_t delay)
{
    std::unique_ptr<AppWindow> window(new AppWindow());

    if (obj != window->bg_top.obj && obj != window->bg_bot.obj
        && obj != lv_scr_act()) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, obj);
        lv_anim_set_time(&a, APP_ANIM_TIME);
        lv_anim_set_delay(&a, delay);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_values(
            &a, lv_obj_get_y(obj) - APP_ANIM_Y, lv_obj_get_y(obj));
        lv_anim_start(&a);

        lv_obj_fade_in(obj, APP_ANIM_TIME - 50, delay);
    }
}

void animate_out_all(lv_obj_t * obj, uint32_t delay)
{
    lv_obj_t * child = lv_obj_get_child_back(obj, NULL);

    std::unique_ptr<AppWindow> window(new AppWindow());

    while (child) {
        if (child != window->bg_top.obj && child != window->bg_bot.obj
            && child != lv_scr_act()) {
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, child);
            lv_anim_set_time(&a, APP_ANIM_TIME);
            lv_anim_set_delay(&a, delay);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
            if (lv_obj_get_y(child) < 80) {
                lv_anim_set_values(
                    &a, lv_obj_get_y(child), lv_obj_get_y(child) - APP_ANIM_Y);
            } else {
                lv_anim_set_values(
                    &a, lv_obj_get_y(child), lv_obj_get_y(child) + APP_ANIM_Y);

                delay += APP_ANIM_DELAY;
            }
            lv_anim_set_ready_cb(&a, lv_obj_del_anim_ready_cb);
            lv_anim_start(&a);
        }
        child = lv_obj_get_child_back(obj, child);
    }
}

void animate_background(uint32_t   delay,
                        lv_color_t top_color,
                        lv_color_t bot_color,
                        bool       fullscreen)
{
    std::unique_ptr<AppWindow> window(new AppWindow());

    lv_coord_t top_y_act = lv_obj_get_y(window->bg_top.obj);
    lv_coord_t top_y_new = fullscreen ? APP_BG_FULL : APP_BG_NORMAL;

    if (top_y_new != APP_BG_NORMAL && top_y_new == top_y_act
        && window->bg_top.active_color.full == top_color.full) {
        return;
    }

    lv_anim_t a;
    if ((top_y_new == APP_BG_NORMAL && top_y_new == top_y_act)
        || (top_y_new == APP_BG_NORMAL && top_y_act == APP_BG_FULL)) {
        lv_anim_path_t path;
        lv_anim_path_init(&path);
        lv_anim_path_set_cb(&path, anim_path_triangle);

        lv_anim_init(&a);
        lv_anim_set_var(&a, window->bg_top.obj);
        lv_anim_set_time(&a, APP_ANIM_TIME_BG + APP_ANIM_PAGE_DELAY);
        lv_anim_set_delay(&a, delay);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_values(&a, top_y_act, top_y_new);
        lv_anim_set_path(&a, &path);
        lv_anim_start(&a);
    } else {
        lv_anim_init(&a);
        lv_anim_set_var(&a, window->bg_top.obj);
        lv_anim_set_time(&a, APP_ANIM_TIME_BG);
        lv_anim_set_delay(&a, delay);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_values(&a, lv_obj_get_y(window->bg_top.obj), top_y_new);
        lv_anim_start(&a);
    }

    /* Update the top background color. */
    window->bg_top.previous_color = window->bg_top.active_color;
    window->bg_top.active_color   = top_color;

    /* Update the bottom background color. */
    window->bg_bot.previous_color = window->bg_bot.active_color;
    window->bg_bot.active_color   = bot_color;

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)animate_background_color_cb);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, APP_ANIM_TIME_BG);
    lv_anim_set_path(&a, &lv_anim_path_def);
    lv_anim_start(&a);
}

lv_anim_value_t anim_path_triangle([[gnu::unused]] const lv_anim_path_t * path,
                                   const lv_anim_t *                      a)
{
    /*Calculate the current step*/
    uint32_t        step;
    lv_anim_value_t ret = 0;
    if (a->time == a->act_time) {
        ret = (lv_anim_value_t)a->end;
    } else {
        if (a->act_time < a->time / 2) {
            step = ((int32_t)a->act_time * 1024) / (a->time / 2);
            int32_t new_value;
            new_value = (int32_t)step * (APP_BG_NORMAL - a->start);
            new_value = new_value >> 10;
            new_value += a->start;

            ret = (lv_anim_value_t)new_value;
        } else {
            uint32_t t = a->act_time - a->time / 2;
            step       = ((int32_t)t * 1024) / (a->time / 2);
            int32_t new_value;
            new_value = (int32_t)step * (a->end - APP_BG_NORMAL);
            new_value = new_value >> 10;
            new_value += APP_BG_NORMAL;

            ret = (lv_anim_value_t)new_value;
        }
    }

    return ret;
}

void init_page(lv_obj_t ** page, lv_scrollbar_mode_t mode)
{
    *page = lv_page_create(lv_scr_act(), NULL);
    lv_obj_set_size(*page, APP_PAGE_WIDTH, APP_PAGE_HEIGHT);
    lv_page_set_scrollbar_mode(*page, mode);
    lv_obj_clean_style_list(*page, LV_OBJ_PART_MAIN);
    lv_obj_align(*page, NULL, LV_ALIGN_IN_TOP_MID, 0, APP_STATUS_BAR_HEIGHT);
}

void go_to_page_index(page_index_t index, uint32_t delay)
{
    std::unique_ptr<AppWindow> window(new AppWindow());

    bool from_error = (window->current_page_index == PAGE_INDEX_ERROR);

    animate_out_all(from_error ? lv_scr_act() : window->page_display, 0);
    if (from_error) {
        window->update_status_bar();
    }

    /* Update the last seen page index. */
    window->last_page_index = window->current_page_index;

    switch (index) {
    case PAGE_INDEX_HOME:
        window->show_home_page(delay);
        break;

    case PAGE_INDEX_SETTINGS:
        window->show_settings_page(delay);
        break;

    case PAGE_INDEX_WIFI_SETTINGS:
        window->show_wifi_settings_page(delay);
        break;

    case PAGE_INDEX_ERROR:
    default:
        if (from_error) {
            /* Avoid looping and weirdness by just going home. */
            window->show_home_page(delay);
        } else {
            window->show_error_page("Unknown page.", delay);
        }
        break;
    }
}

} // namespace app

//---------------------------------------------------------------------
// Callbacks
//---------------------------------------------------------------------

namespace app
{
void animate_background_color_cb([[gnu::unused]] lv_anim_t * a,
                                 lv_anim_value_t             v)
{
    std::unique_ptr<AppWindow> window(new AppWindow());

    lv_color_t color = lv_color_mix(
        window->bg_top.active_color, window->bg_top.previous_color, v);
    lv_obj_set_style_local_bg_color(
        window->bg_top.obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, color);
}

LV_EVENT_CB_DECLARE(video_btn_event_cb)
{
    /* Do nothing if not clicked. */
    if (e != LV_EVENT_CLICKED) {
        return;
    }

    std::unique_ptr<AppWindow> window(new AppWindow());
    animate_out_all(lv_scr_act(), 0);
    window->show_error_page("Not yet implemented.", APP_ANIM_PAGE_DELAY);
}

LV_EVENT_CB_DECLARE(settings_btn_event_cb)
{
    /* Do nothing if not clicked. */
    if (e != LV_EVENT_CLICKED) {
        return;
    }

    go_to_page_index(PAGE_INDEX_SETTINGS);
}

LV_EVENT_CB_DECLARE(back_btn_event_cb)
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

LV_EVENT_CB_DECLARE(home_btn_event_cb)
{
    /* Do nothing if not clicked. */
    if (e != LV_EVENT_CLICKED) {
        return;
    }

    std::unique_ptr<AppWindow> window(new AppWindow());

    if (window->current_page_index != PAGE_INDEX_HOME) {
        go_to_page_index(PAGE_INDEX_HOME);
    }
}

LV_EVENT_CB_DECLARE(wifi_btn_event_cb)
{
    /* Do nothing if not clicked. */
    if (e != LV_EVENT_CLICKED) {
        return;
    }

    go_to_page_index(PAGE_INDEX_WIFI_SETTINGS);
}
} // namespace app
