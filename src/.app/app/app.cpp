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
#include <assert.h>
#include <iostream>
#include <memory>

#include "app_hal.h"
#include "app_theme.h"
#include "lvgl/lvgl.h"

//-----------------------------------------------------------------------------
//  Macros
//-----------------------------------------------------------------------------

/*Bg positions*/
#define APP_BG_NONE   (-LV_VER_RES)
#define APP_BG_FULL   0
#define APP_BG_NORMAL (-5 * (LV_VER_RES / 6))

/*Sizes*/
#define APP_BTN_H (40)
#define APP_BTN_W (120)

/*Animations*/
#define APP_ANIM_Y          (LV_VER_RES / 20)
#define APP_NO_DELAY        (0)
#define APP_ANIM_DELAY      (65)
#define APP_ANIM_PAGE_DELAY (200)
#define APP_ANIM_TIME       (150)
#define APP_ANIM_TIME_BG    (300)

/*Padding*/
#define APP_TITLE_PAD 35

//-----------------------------------------------------------------------------
//  Forward Declarations
//-----------------------------------------------------------------------------

namespace app
{
extern "C" {

/* Image (C-array) of the Wifi icon */
LV_IMG_DECLARE(app_icon_wifi);

}

/* Callback for pressing the Video button. */
LV_EVENT_CB_DECLARE(home_video_btn_event_cb);

/* Callback for pressing the Settings button. */
LV_EVENT_CB_DECLARE(home_settings_btn_event_cb);

/* Callback for pressing the Back button. */
LV_EVENT_CB_DECLARE(back_btn_event_cb);

/* Callback for animating background color changes. */
void animate_background_color_cb(lv_anim_t * a, lv_anim_value_t v);

} // namespace app

//-----------------------------------------------------------------------------
//  AppWindow
//-----------------------------------------------------------------------------

namespace app
{
app_page_t        AppWindow::last_page = APP_PAGE_HOME;
bool              AppWindow::built     = false;
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
    lv_obj_set_style_local_bg_color(
        this->bg_bot.obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, this->bg_bot.active_color);
    lv_obj_set_size(this->bg_bot.obj, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_y(this->bg_bot.obj, APP_BG_FULL);

    /* Configure the top (bar) background. */
    this->bg_top.obj            = lv_obj_create(parent, NULL);
    lv_obj_clean_style_list(this->bg_top.obj, LV_OBJ_PART_MAIN);
    lv_obj_set_style_local_bg_opa(
        this->bg_top.obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_obj_set_style_local_bg_color(
        this->bg_top.obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, this->bg_top.active_color);
    lv_obj_set_size(this->bg_top.obj, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_y(this->bg_top.obj, APP_BG_NORMAL);

    /* Start from the home page. */
    this->built = true;
}

void AppWindow::show_home_page(uint32_t delay)
{
    lv_obj_t * parent = lv_scr_act();
    this->last_page   = APP_PAGE_HOME;

    animate_background(1000, APP_THEME_BLUE, APP_THEME_WHITE, false);

    lv_obj_t * icon;
    lv_obj_t * cont_bar = lv_cont_create(parent, NULL);
    lv_obj_set_size(cont_bar, 320, 40);
    lv_obj_clean_style_list(cont_bar, LV_CONT_PART_MAIN);
    lv_obj_align(cont_bar, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);

    icon = lv_img_create(cont_bar, NULL);
    lv_img_set_src(icon, &app_icon_wifi);
    lv_obj_align_origo(icon, NULL, LV_ALIGN_CENTER, 85, 0);
    animate_in(icon, delay);

    lv_obj_t * cont = lv_cont_create(parent, NULL);
    lv_obj_set_size(cont, 320, 160);
    lv_obj_clean_style_list(cont, LV_CONT_PART_MAIN);
    lv_obj_align(cont, NULL, LV_ALIGN_IN_TOP_MID, 0, 40);

    lv_obj_t * btn = lv_btn_create(cont, NULL);
    lv_theme_apply(btn, (lv_theme_style_t)APP_THEME_BTN_CIRCLE);
    lv_obj_set_size(btn, APP_BTN_W, APP_BTN_H);
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
    lv_obj_align_origo(btn, NULL, LV_ALIGN_CENTER, 0, -30);
    lv_obj_fade_in(btn, APP_ANIM_TIME * 2, delay + APP_ANIM_TIME + 50);
    lv_obj_set_event_cb(btn, home_video_btn_event_cb);

    btn = lv_btn_create(cont, NULL);
    lv_theme_apply(btn, (lv_theme_style_t)APP_THEME_BTN_CIRCLE);
    lv_obj_set_size(btn, APP_BTN_W, APP_BTN_H);
    lv_obj_set_style_local_value_str(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, "Settings");
    lv_obj_set_style_local_value_font(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, lv_theme_get_font_normal());
    lv_obj_set_style_local_bg_color(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, APP_THEME_RED);
    lv_obj_set_style_local_bg_color(btn,
                                    LV_OBJ_PART_MAIN,
                                    LV_STATE_PRESSED,
                                    lv_color_darken(APP_THEME_RED, LV_OPA_20));
    lv_obj_align_origo(btn, NULL, LV_ALIGN_CENTER, 0, 50);
    lv_obj_fade_in(btn, APP_ANIM_TIME * 2, delay + APP_ANIM_TIME + 50);
    lv_obj_set_event_cb(btn, home_settings_btn_event_cb);
}

void AppWindow::show_error_page(std::string   message,
                                lv_event_cb_t btn_event_cb,
                                uint32_t      delay)
{
    lv_obj_t * parent = lv_scr_act();

    animate_background(APP_NO_DELAY, APP_THEME_RED, APP_THEME_RED, true);
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
    lv_obj_set_size(btn, APP_BTN_W, APP_BTN_H);
    lv_obj_align(btn, parent, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
    lv_obj_set_event_cb(btn, back_btn_event_cb);

    if (btn_event_cb == NULL) {
        lv_obj_set_style_local_value_str(
            btn, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, "Home");
        this->last_page = APP_PAGE_HOME;
    } else {
        lv_obj_set_style_local_value_str(
            btn, LV_BTN_PART_MAIN, LV_STATE_DEFAULT, "Back");
    }

    animate_in(txt, delay);
    delay += APP_ANIM_DELAY;

    animate_in(btn, delay);
    delay += APP_ANIM_DELAY;

    animate_in(btn, delay);
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
    window->show_home_page(1000);

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
    lv_theme_apply(bg, (lv_theme_style_t)APP_THEME_ICON);
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

void animate_background(uint32_t delay, lv_color_t top_color, lv_color_t bot_color, bool fullscreen)
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
    window->bg_bot.active_color = bot_color;

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)animate_background_color_cb);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, APP_ANIM_TIME_BG);
    lv_anim_set_path(&a, &lv_anim_path_def);
    lv_anim_start(&a);
}

lv_anim_value_t anim_path_triangle(const lv_anim_path_t * path,
                                   const lv_anim_t *      a)
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
} // namespace app

//---------------------------------------------------------------------
// Callbacks
//---------------------------------------------------------------------

namespace app
{
void animate_background_color_cb(lv_anim_t * a, lv_anim_value_t v)
{
    std::unique_ptr<AppWindow> window(new AppWindow());

    lv_color_t color = lv_color_mix(
        window->bg_top.active_color, window->bg_top.previous_color, v);
    lv_obj_set_style_local_bg_color(
        window->bg_top.obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, color);
}

LV_EVENT_CB_DECLARE(home_video_btn_event_cb)
{
    /* Do nothing if not clicked. */
    if (e != LV_EVENT_CLICKED) {
        return;
    }
    animate_out_all(lv_scr_act(), 0);

    std::unique_ptr<AppWindow> window(new AppWindow());
    window->show_error_page(
        "Not yet implemented.", back_btn_event_cb, APP_ANIM_PAGE_DELAY);
}

LV_EVENT_CB_DECLARE(home_settings_btn_event_cb)
{
    /* Do nothing if not clicked. */
    if (e != LV_EVENT_CLICKED) {
        return;
    }
    animate_out_all(lv_scr_act(), 0);

    std::unique_ptr<AppWindow> window(new AppWindow());
    window->show_error_page(
        "Not yet implemented.", back_btn_event_cb, APP_ANIM_PAGE_DELAY);
}

LV_EVENT_CB_DECLARE(back_btn_event_cb)
{
    /* Do nothing if not clicked. */
    if (e != LV_EVENT_CLICKED) {
        return;
    }

    animate_out_all(lv_scr_act(), 0);
    std::unique_ptr<AppWindow> window(new AppWindow());

    switch (window->last_page) {
    case APP_PAGE_HOME:
        window->show_home_page(APP_ANIM_PAGE_DELAY);
        break;

    case APP_PAGE_ERROR:
        /* Avoid looping and weirdness by just going home. */
        window->show_home_page(APP_ANIM_PAGE_DELAY);
        break;

    case APP_PAGE_SETTINGS:
    default:
        window->show_error_page("Unknown page.", NULL, APP_ANIM_PAGE_DELAY);
        break;
    }
}

} // namespace app
