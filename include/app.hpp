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
 * @copyright       © 2015-2020 SWYM, LLC. All Rights Reserved.
 *
 *****************************************************************************/

#ifndef APP_H__
#define APP_H__

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>
#include <string>

#include "lvgl/lvgl.h"

//-----------------------------------------------------------------------------
//  Public Types
//-----------------------------------------------------------------------------

namespace app
{
class AppWindow;

} // namespace app

//-----------------------------------------------------------------------------
//  Public Function Prototypes
//-----------------------------------------------------------------------------

namespace app
{
struct Background {
    lv_obj_t * obj;
    lv_color_t previous_color;
    lv_color_t active_color;
};

class AppWindow
{
    static bool built;

    void set_background_color(lv_color_t bar_color, lv_color_t main_color);

  public:
    static struct Background bg_main;
    static struct Background bg_bar;

    AppWindow();
    ~AppWindow();

    void build();

    bool is_built()
    {
        return this->built;
    }

    void show_home_page(uint32_t delay);
};

void init();

void refresh();

void animate_in(lv_obj_t * obj, uint32_t delay);

void animate_out_all(lv_obj_t * obj, uint32_t delay);

lv_obj_t * add_icon(lv_obj_t * parent, const void * src_bg, const void * src_icon, std::string txt);

lv_obj_t * create_title(std::string txt);


} // namespace app

#endif // APP_H__
