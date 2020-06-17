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
#define APP_BTN_H (25)
#define APP_BTN_W (100)

/*Animations*/
#define APP_ANIM_Y       (LV_VER_RES / 20)
#define APP_ANIM_DELAY   (40)
#define APP_ANIM_TIME    (150)
#define APP_ANIM_TIME_BG (300)

/*Padding*/
#define APP_TITLE_PAD 35

//-----------------------------------------------------------------------------
//  Forward Declarations
//-----------------------------------------------------------------------------


extern "C" {
    LV_IMG_DECLARE(app_icon_wifi);
    LV_IMG_DECLARE(app_img_btn_bg_4);
    LV_IMG_DECLARE(app_img_setup);
}

//-----------------------------------------------------------------------------
//  AppWindow
//-----------------------------------------------------------------------------

namespace app
{
bool              AppWindow::built = false;
struct Background AppWindow::bg_bar;
struct Background AppWindow::bg_main;

AppWindow::AppWindow()
{
}

AppWindow::~AppWindow()
{
}

void AppWindow::set_background_color(lv_color_t main_color, lv_color_t bar_color)
{
    lv_obj_t * parent = lv_scr_act();

    this->bg_main.previous_color = this->bg_main.active_color;

    this->bg_main.obj = lv_obj_create(parent, NULL);
    lv_obj_clean_style_list(this->bg_main.obj, LV_OBJ_PART_MAIN);
    lv_obj_set_style_local_bg_opa(
        this->bg_main.obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_obj_set_style_local_bg_color(
        this->bg_main.obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, main_color);
    lv_obj_set_size(this->bg_main.obj, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_y(this->bg_main.obj, APP_BG_FULL);
    this->bg_main.active_color = main_color;

    this->bg_bar.previous_color = this->bg_bar.active_color;
    this->bg_bar.obj = lv_obj_create(parent, NULL);
    lv_obj_clean_style_list(this->bg_bar.obj, LV_OBJ_PART_MAIN);
    lv_obj_set_style_local_bg_opa(
        this->bg_bar.obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_COVER);
    lv_obj_set_style_local_bg_color(
        this->bg_bar.obj, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, bar_color);
    lv_obj_set_size(this->bg_bar.obj, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_y(this->bg_bar.obj, APP_BG_NORMAL);
    this->bg_bar.active_color = bar_color;
}


void AppWindow::build()
{
    if (this->is_built()) {
        return;
    }

    this->bg_bar.previous_color = APP_THEME_WHITE;
    this->bg_bar.active_color   = APP_THEME_WHITE;

    this->bg_main.previous_color = APP_THEME_WHITE;
    this->bg_main.active_color   = APP_THEME_WHITE;

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

    this->set_background_color(APP_THEME_WHITE, APP_THEME_BLUE);

    /* Start from the home page. */
    this->built = true;
    this->show_home_page(0);
}

void AppWindow::show_home_page(uint32_t delay)
{
    lv_obj_t * parent = lv_scr_act();

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
    lv_obj_set_size(btn, 150, 60);
    lv_obj_set_style_local_value_str(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, "Video");
    lv_obj_set_style_local_value_font(btn,
                                      LV_OBJ_PART_MAIN,
                                      LV_STATE_DEFAULT,
                                      lv_theme_get_font_normal());
    lv_obj_set_style_local_bg_color(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, APP_THEME_GREEN);
    lv_obj_set_style_local_bg_color(btn,
                                    LV_OBJ_PART_MAIN,
                                    LV_STATE_PRESSED,
                                    lv_color_darken(APP_THEME_GREEN, LV_OPA_20));
    lv_obj_align_origo(btn, NULL, LV_ALIGN_CENTER, 0, -40);
    lv_obj_fade_in(btn, APP_ANIM_TIME * 2, delay + APP_ANIM_TIME + 50);
    // lv_obj_set_event_cb(icon, setup_icon_event_cb);

    btn = lv_btn_create(cont, NULL);
    lv_theme_apply(btn, (lv_theme_style_t)APP_THEME_BTN_CIRCLE);
    lv_obj_set_size(btn, 150, 60);
    lv_obj_set_style_local_value_str(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, "Settings");
    lv_obj_set_style_local_value_font(btn,
                                      LV_OBJ_PART_MAIN,
                                      LV_STATE_DEFAULT,
                                      lv_theme_get_font_normal());
    lv_obj_set_style_local_bg_color(
        btn, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, APP_THEME_RED);
    lv_obj_set_style_local_bg_color(btn,
                                    LV_OBJ_PART_MAIN,
                                    LV_STATE_PRESSED,
                                    lv_color_darken(APP_THEME_RED, LV_OPA_20));
    lv_obj_align_origo(btn, NULL, LV_ALIGN_CENTER, 0, 40);
    lv_obj_fade_in(btn, APP_ANIM_TIME * 2, delay + APP_ANIM_TIME + 50);
    // lv_obj_set_event_cb(icon, setup_icon_event_cb);

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

    AppWindow window = AppWindow();
    window.build();

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

void animate_in(lv_obj_t * obj, uint32_t delay)
{
    AppWindow window = AppWindow();
    if (obj != window.bg_bar.obj && obj != window.bg_main.obj
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

    AppWindow window = AppWindow();

    while (child) {
        if (child != window.bg_bar.obj && child != window.bg_main.obj
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

lv_obj_t * add_icon(lv_obj_t * parent, const void * src_bg, const void * src_icon, std::string txt)
{

    lv_obj_t * bg = lv_img_create(parent, NULL);
    lv_obj_set_click(bg, true);
    lv_theme_apply(bg, (lv_theme_style_t)APP_THEME_ICON);
    lv_img_set_src(bg, src_bg);
    lv_img_set_antialias(bg, false);

    lv_obj_t * icon = lv_img_create(bg, NULL);
    lv_img_set_src(icon, src_icon);
    lv_obj_set_style_local_image_recolor_opa(icon, LV_IMG_PART_MAIN, LV_STATE_DEFAULT, LV_OPA_0);
    lv_obj_align(icon, NULL, LV_ALIGN_IN_TOP_RIGHT, -30, 30);

    lv_obj_t * label = lv_label_create(bg, NULL);
    lv_label_set_text(label, txt.c_str());
    lv_obj_align(label, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 30, -30);


    return bg;
}

lv_obj_t * create_title(std::string txt)
{
    lv_obj_t * title = lv_label_create(lv_scr_act(), NULL);
    lv_theme_apply(title, (lv_theme_style_t)APP_THEME_TITLE);
    lv_label_set_text(title, txt.c_str());
    lv_obj_align(title, NULL, LV_ALIGN_IN_TOP_MID, 0, APP_TITLE_PAD);

    return title;
}

} // namespace app
