/**
 * @file app_theme.h
 *
 */

#ifndef APP_THEME_H
#define APP_THEME_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl/lvgl.h"

/*********************
 *      DEFINES
 *********************/
/*Colors*/
#define APP_THEME_WHITE      lv_color_hex(0xffffff)
#define APP_THEME_LIGHT      lv_color_hex(0xf3f8fe)
#define APP_THEME_GRAY       lv_color_hex(0x8a8a8a)
#define APP_THEME_LIGHT_GRAY lv_color_hex(0xc4c4c4)
#define APP_THEME_BLUE       lv_color_hex(0x2f3243) // 006fb6
#define APP_THEME_GREEN      lv_color_hex(0x4cb242)
#define APP_THEME_RED        lv_color_hex(0xd51732)

/**********************
 *      TYPEDEFS
 **********************/
typedef enum {
    APP_THEME_TITLE = _LV_THEME_BUILTIN_LAST,
    APP_THEME_LABEL_WHITE,
    APP_THEME_ICON,
    APP_THEME_BTN_BORDER,
    APP_THEME_BTN_CIRCLE,
    APP_THEME_BOX_BORDER,
    APP_THEME_BTN_BACK
} app_theme_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Initialize the default
 * @param color_primary the primary color of the theme
 * @param color_secondary the secondary color for the theme
 * @param flags ORed flags starting with `LV_THEME_DEF_FLAG_...`
 * @param font_small pointer to a small font
 * @param font_normal pointer to a normal font
 * @param font_subtitle pointer to a large font
 * @param font_title pointer to a extra large font
 * @return a pointer to reference this theme later
 */
lv_theme_t * app_theme_init(lv_color_t        color_primary,
                            lv_color_t        color_secondary,
                            uint32_t          flags,
                            const lv_font_t * font_small,
                            const lv_font_t * font_normal,
                            const lv_font_t * font_subtitle,
                            const lv_font_t * font_title);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* APP_THEME_H */
