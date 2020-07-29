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
class SdlDisplay : Display
{
    std::shared_ptr<SDL_Renderer> renderer;
    std::shared_ptr<SDL_Texture>  texture;
    std::shared_ptr<SDL_Window>   window;
    SDL_Rect                      framebox;

  public:
    SdlDisplay();
    ~SdlDisplay();

    /* Build the main window */
    void build_window(const std::string & title);

    /* Initialize the buffers from the display source */
    void init_buffers(const std::string & source);

    /* Load an image into the SDL display */
    void load_image(const std::string & image);

    /* Update window rendering. */
    bool refresh();

    void render_frame();
};

}    // namespace graphics
#endif /* SDL_DRIVER_HPP__ */
