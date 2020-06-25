/******************************************************************************
 *
 * @file            app_theme.hpp
 *
 * @brief           Custom app theme functions
 *
 * @details         Many of the macros contained within are from the
 *                  lv_theme_materials.c file (license included below), but have
 *                  been exposed here so that the app can customize them as
 *                  needed.
 *
 * @author(s)       Stephen Stauts
 *
 * @created         06/25/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2020 Jabil, Inc. All Rights Reserved.
 *
 *****************************************************************************/

/**
 * MIT licence
 * Copyright (c) 2020 LVGL LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef APP_THEME_HPP__
#define APP_THEME_HPP__

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------
// Macros
//---------------------------------------------------------------------

/* Colors */
#define APP_THEME_WHITE        lv_color_hex(0xffffff)
#define APP_THEME_LIGHT        lv_color_hex(0xf3f8fe)
#define APP_THEME_GRAY         lv_color_hex(0x8a8a8a)
#define APP_THEME_LIGHT_GRAY   lv_color_hex(0xc4c4c4)
#define APP_THEME_DARK_BLUE    lv_color_hex(0x2f3243) // 006fb6
#define APP_THEME_GREEN        lv_color_hex(0x4cb242)
#define APP_THEME_RED          lv_color_hex(0xd51732)
#define APP_THEME_LIGHT_PURPLE lv_color_hex(0x8e56b6)

#define APP_THEME_COLOR_SCR                                                    \
    (IS_LIGHT_THEME ? lv_color_hex(0xeaeff3) : lv_color_hex(0x444b5a))
#define APP_THEME_COLOR_SCR_TEXT                                               \
    (IS_LIGHT_THEME ? lv_color_hex(0x3b3e42) : lv_color_hex(0xe7e9ec))

/* Buttons */
#define APP_THEME_COLOR_BTN                                                    \
    (IS_LIGHT_THEME ? lv_color_hex(0xffffff) : lv_color_hex(0x586273))
#define APP_THEME_COLOR_BTN_PR                                                 \
    (IS_LIGHT_THEME ? lv_color_mix(                                            \
         lv_theme_get_color_primary(), APP_THEME_COLOR_BTN, LV_OPA_10)         \
                    : lv_color_mix(lv_theme_get_color_primary(),               \
                                   APP_THEME_COLOR_BTN,                        \
                                   LV_OPA_30))

#define APP_THEME_COLOR_BTN_CHK (lv_theme_get_color_primary())
#define APP_THEME_COLOR_BTN_CHK_PR                                             \
    (lv_color_darken(lv_theme_get_color_primary(), LV_OPA_30))
#define APP_THEME_COLOR_BTN_DIS                                                \
    (IS_LIGHT_THEME ? lv_color_hex3(0xccc) : lv_color_hex3(0x888))

#define APP_THEME_COLOR_BTN_BORDER        (lv_theme_get_color_primary())
#define APP_THEME_COLOR_BTN_BORDER_PR     (lv_theme_get_color_primary())
#define APP_THEME_COLOR_BTN_BORDER_CHK    (lv_theme_get_color_primary())
#define APP_THEME_COLOR_BTN_BORDER_CHK_PR (lv_theme_get_color_primary())
#define APP_THEME_COLOR_BTN_BORDER_INA                                         \
    (IS_LIGHT_THEME ? lv_color_hex3(0x888) : lv_color_hex(0x404040))

/* Primary Background */
#define APP_THEME_COLOR_BG                                                     \
    (IS_LIGHT_THEME ? lv_color_hex(0xffffff) : lv_color_hex(0x586273))
#define APP_THEME_COLOR_BG_PR                                                  \
    (IS_LIGHT_THEME ? lv_color_hex(0xeeeeee) : lv_color_hex(0x494f57))
#define APP_THEME_COLOR_BG_CHK (lv_theme_get_color_primary())
#define APP_THEME_COLOR_BG_PR_CHK                                              \
    (lv_color_darken(lv_theme_get_color_primary(), LV_OPA_20))
#define APP_THEME_COLOR_BG_DIS APP_THEME_COLOR_BG

#define APP_THEME_COLOR_BG_BORDER                                              \
    (IS_LIGHT_THEME ? lv_color_hex(0xd6dde3)                                   \
                    : lv_color_hex(0x808a97)) /*dfe7ed*/
#define APP_THEME_COLOR_BG_BORDER_PR                                           \
    (IS_LIGHT_THEME ? lv_color_hex3(0xccc) : lv_color_hex(0x5f656e))
#define APP_THEME_COLOR_BG_BORDER_CHK                                          \
    (IS_LIGHT_THEME ? lv_color_hex(0x3b3e42) : lv_color_hex(0x5f656e))
#define APP_THEME_COLOR_BG_BORDER_CHK_PR                                       \
    (IS_LIGHT_THEME ? lv_color_hex(0x3b3e42) : lv_color_hex(0x5f656e))
#define APP_THEME_COLOR_BG_BORDER_DIS                                          \
    (IS_LIGHT_THEME ? lv_color_hex(0xd6dde3) : lv_color_hex(0x5f656e))

#define APP_THEME_COLOR_BG_TEXT                                                \
    (IS_LIGHT_THEME ? lv_color_hex(0x3b3e42) : lv_color_hex(0xffffff))
#define APP_THEME_COLOR_BG_TEXT_PR                                             \
    (IS_LIGHT_THEME ? lv_color_hex(0x3b3e42) : lv_color_hex(0xffffff))
#define APP_THEME_COLOR_BG_TEXT_CHK                                            \
    (IS_LIGHT_THEME ? lv_color_hex(0xffffff) : lv_color_hex(0xffffff))
#define APP_THEME_COLOR_BG_TEXT_CHK_PR                                         \
    (IS_LIGHT_THEME ? lv_color_hex(0xffffff) : lv_color_hex(0xffffff))
#define APP_THEME_COLOR_BG_TEXT_DIS                                            \
    (IS_LIGHT_THEME ? lv_color_hex3(0xaaa) : lv_color_hex3(0x999))

/* Secondary Background */
#define APP_THEME_COLOR_BG_SEC                                                 \
    (IS_LIGHT_THEME ? lv_color_hex(0xd4d7d9) : lv_color_hex(0x45494d))
#define APP_THEME_COLOR_BG_SEC_BORDER                                          \
    (IS_LIGHT_THEME ? lv_color_hex(0xdfe7ed) : lv_color_hex(0x404040))
#define APP_THEME_COLOR_BG_SEC_TEXT                                            \
    (IS_LIGHT_THEME ? lv_color_hex(0x31404f) : lv_color_hex(0xa5a8ad))
#define APP_THEME_COLOR_BG_SEC_TEXT_DIS                                        \
    (IS_LIGHT_THEME ? lv_color_hex(0xaaaaaa) : lv_color_hex(0xa5a8ad))

#define IS_LIGHT_THEME (lv_theme_get_flags() & LV_THEME_MATERIAL_FLAG_LIGHT)

#ifdef __cplusplus
} /* extern "C" */
#endif

namespace theme
{
lv_style_t get_style_box(lv_align_t alignment);

lv_style_t get_style_table_cell();
} // namespace theme

#endif /* APP_THEME_HPP__ */
