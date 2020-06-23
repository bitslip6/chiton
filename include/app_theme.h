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
#define APP_THEME_WHITE        lv_color_hex(0xffffff)
#define APP_THEME_LIGHT        lv_color_hex(0xf3f8fe)
#define APP_THEME_GRAY         lv_color_hex(0x8a8a8a)
#define APP_THEME_LIGHT_GRAY   lv_color_hex(0xc4c4c4)
#define APP_THEME_DARK_BLUE    lv_color_hex(0x2f3243) // 006fb6
#define APP_THEME_GREEN        lv_color_hex(0x4cb242)
#define APP_THEME_RED          lv_color_hex(0xd51732)
#define APP_THEME_LIGHT_PURPLE lv_color_hex(0x8e56b6)

#define APP_THEME_COLOR_SCR        (IS_LIGHT_THEME ? lv_color_hex(0xeaeff3) : lv_color_hex(0x444b5a))
#define APP_THEME_COLOR_SCR_TEXT   (IS_LIGHT_THEME ? lv_color_hex(0x3b3e42) : lv_color_hex(0xe7e9ec))

/*BUTTON*/
#define APP_THEME_COLOR_BTN           (IS_LIGHT_THEME ? lv_color_hex(0xffffff) : lv_color_hex(0x586273))
#define APP_THEME_COLOR_BTN_PR        (IS_LIGHT_THEME ? lv_color_mix(lv_theme_get_color_primary(), APP_THEME_COLOR_BTN, LV_OPA_10) : lv_color_mix(lv_theme_get_color_primary(), APP_THEME_COLOR_BTN, LV_OPA_30))

#define APP_THEME_COLOR_BTN_CHK       (lv_theme_get_color_primary())
#define APP_THEME_COLOR_BTN_CHK_PR    (lv_color_darken(lv_theme_get_color_primary(), LV_OPA_30))
#define APP_THEME_COLOR_BTN_DIS       (IS_LIGHT_THEME ? lv_color_hex3(0xccc) : lv_color_hex3(0x888))

#define APP_THEME_COLOR_BTN_BORDER        (lv_theme_get_color_primary())
#define APP_THEME_COLOR_BTN_BORDER_PR     (lv_theme_get_color_primary())
#define APP_THEME_COLOR_BTN_BORDER_CHK    (lv_theme_get_color_primary())
#define APP_THEME_COLOR_BTN_BORDER_CHK_PR (lv_theme_get_color_primary())
#define APP_THEME_COLOR_BTN_BORDER_INA    (IS_LIGHT_THEME ? lv_color_hex3(0x888) : lv_color_hex(0x404040))

/*BACKGROUND*/
#define APP_THEME_COLOR_BG            (IS_LIGHT_THEME ? lv_color_hex(0xffffff) : lv_color_hex(0x586273))
#define APP_THEME_COLOR_BG_PR         (IS_LIGHT_THEME ? lv_color_hex(0xeeeeee) : lv_color_hex(0x494f57))
#define APP_THEME_COLOR_BG_CHK        (lv_theme_get_color_primary())
#define APP_THEME_COLOR_BG_PR_CHK     (lv_color_darken(lv_theme_get_color_primary(), LV_OPA_20))
#define APP_THEME_COLOR_BG_DIS        APP_THEME_COLOR_BG

#define APP_THEME_COLOR_BG_BORDER         (IS_LIGHT_THEME ? lv_color_hex(0xd6dde3) : lv_color_hex(0x808a97))   /*dfe7ed*/
#define APP_THEME_COLOR_BG_BORDER_PR      (IS_LIGHT_THEME ? lv_color_hex3(0xccc) : lv_color_hex(0x5f656e))
#define APP_THEME_COLOR_BG_BORDER_CHK     (IS_LIGHT_THEME ? lv_color_hex(0x3b3e42) : lv_color_hex(0x5f656e))
#define APP_THEME_COLOR_BG_BORDER_CHK_PR  (IS_LIGHT_THEME ? lv_color_hex(0x3b3e42) : lv_color_hex(0x5f656e))
#define APP_THEME_COLOR_BG_BORDER_DIS     (IS_LIGHT_THEME ? lv_color_hex(0xd6dde3) : lv_color_hex(0x5f656e))

#define APP_THEME_COLOR_BG_TEXT           (IS_LIGHT_THEME ? lv_color_hex(0x3b3e42) : lv_color_hex(0xffffff))
#define APP_THEME_COLOR_BG_TEXT_PR        (IS_LIGHT_THEME ? lv_color_hex(0x3b3e42) : lv_color_hex(0xffffff))
#define APP_THEME_COLOR_BG_TEXT_CHK       (IS_LIGHT_THEME ? lv_color_hex(0xffffff) : lv_color_hex(0xffffff))
#define APP_THEME_COLOR_BG_TEXT_CHK_PR    (IS_LIGHT_THEME ? lv_color_hex(0xffffff) : lv_color_hex(0xffffff))
#define APP_THEME_COLOR_BG_TEXT_DIS       (IS_LIGHT_THEME ? lv_color_hex3(0xaaa) : lv_color_hex3(0x999))

/*SECONDARY BACKGROUND*/
#define APP_THEME_COLOR_BG_SEC            (IS_LIGHT_THEME ? lv_color_hex(0xd4d7d9) : lv_color_hex(0x45494d))
#define APP_THEME_COLOR_BG_SEC_BORDER     (IS_LIGHT_THEME ? lv_color_hex(0xdfe7ed) : lv_color_hex(0x404040))
#define APP_THEME_COLOR_BG_SEC_TEXT       (IS_LIGHT_THEME ? lv_color_hex(0x31404f) : lv_color_hex(0xa5a8ad))
#define APP_THEME_COLOR_BG_SEC_TEXT_DIS   (IS_LIGHT_THEME ? lv_color_hex(0xaaaaaa) : lv_color_hex(0xa5a8ad))

#define IS_LIGHT_THEME (lv_theme_get_flags() & LV_THEME_MATERIAL_FLAG_LIGHT)


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
