/******************************************************************************
 *
 * @file            app_theme.cpp
 *
 * @brief           Custom app theme functions.
 *
 * @details
 *
 * @created         06/25/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2020 Jabil, Inc. All Rights Reserved.
 *
 *****************************************************************************/

#include "app_theme.hpp"

//---------------------------------------------------------------------
// Functions
//---------------------------------------------------------------------

namespace theme
{
lv_style_t get_style_box(lv_align_t alignment)
{
    lv_style_t style_box;

    lv_style_init(&style_box);
    lv_style_set_value_align(&style_box, LV_STATE_DEFAULT, alignment);
    lv_style_set_value_ofs_y(&style_box, LV_STATE_DEFAULT, -LV_DPX(10));
    lv_style_set_margin_top(&style_box, LV_STATE_DEFAULT, LV_DPX(30));

    return style_box;
}

lv_style_t get_style_table_cell()
{
    lv_style_t style_cell;
    lv_style_init(&style_cell);
    lv_style_set_bg_opa(&style_cell, LV_STATE_DEFAULT, LV_OPA_50);
    lv_style_set_bg_color(&style_cell, LV_STATE_DEFAULT, LV_COLOR_GRAY);

    return style_cell;
}

} // namespace theme
