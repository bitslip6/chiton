/******************************************************************************
 *
 * @file            sdl_driver.hpp
 *
 * @brief
 *
 * @details
 *
 * @author(s)       Stephen Stauts, Cory Marsh
 *
 * @created         7/9/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2020 Jabil, Inc. All Rights Reserved.
 *
 *****************************************************************************/

#ifndef SDL_DRIVER_HPP__
#define SDL_DRIVER_HPP__

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include <SDL2/SDL.h>
#include <memory>
#include <string>

#include "display.hpp"

//---------------------------------------------------------------------
// Macros
//---------------------------------------------------------------------

#define SDL_PIXEL_SIZE (5)

//---------------------------------------------------------------------
// Types
//---------------------------------------------------------------------

namespace graphics
{
class SdlDisplay
{
#if (HDMI_DISPLAY)
    DisplayBufferGroup<uint32_t> buffers {};
#elif (TFT_DISPLAY)
    DisplayBufferGroup<uint16_t> buffers {};
#else
    #error "sdl_driver.hpp: Display type not specified"
#endif /* HDMI_DISPLAY */

    ScreenInfo                    screen {};
    std::shared_ptr<SDL_Renderer> renderer {};
    std::shared_ptr<SDL_Texture>  texture {};
    std::shared_ptr<SDL_Window>   window {};

  public:
    SdlDisplay();
    ~SdlDisplay();

    /* Build the main window */
    void build_window(size_t width, size_t height, const std::string & title);

    /* Load an image into the window */
    void load_image(const std::string & image);

    /* Update window rendering. */
    bool refresh();
};

}    // namespace graphics
#endif /* SDL_DRIVER_HPP__ */
