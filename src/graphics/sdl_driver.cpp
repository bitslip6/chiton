/******************************************************************************
 *
 * @file            sdl_driver.cpp
 *
 * @brief
 *
 * @details
 *
 * @author(s)       Stephen Stauts
 *
 * @created         7/9/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2020 Jabil, Inc. All Rights Reserved.
 *
 *****************************************************************************/


//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include "sdl_driver.hpp"

#include <SDL2/SDL_image.h>

#include "system.hpp"
#include "utils.hpp"

//---------------------------------------------------------------------
// Class: SdlDisplay
//---------------------------------------------------------------------

namespace graphics
{
SdlDisplay::SdlDisplay()
{
}

SdlDisplay::~SdlDisplay()
{
}

void SdlDisplay::build_window(size_t              width,
                              size_t              height,
                              const std::string & title)
{
    this->screen.width  = width;
    this->screen.height = height;

    SDL_Init(SDL_INIT_VIDEO);    // Initialize SDL2

    /* Create the window and assign its smart pointer the specific SDL deleter
     * function.
     */
    this->window = std::shared_ptr<SDL_Window>(
        SDL_CreateWindow(title.c_str(),              // window title
                         SDL_WINDOWPOS_UNDEFINED,    // initial x position
                         SDL_WINDOWPOS_UNDEFINED,    // initial y position
                         width,                      // width, in pixels
                         height,                     // height, in pixels
                         SDL_WINDOW_OPENGL           // flags - see below
                         ),
        SDL_DestroyWindow);

    if (this->window == nullptr) {
        sys::panic(1, "Failed to create window: %s\n", SDL_GetError());
    }

    /* Create the renderer */
    this->renderer = std::shared_ptr<SDL_Renderer>(
        SDL_CreateRenderer(this->window.get(), -1, SDL_RENDERER_ACCELERATED),
        SDL_DestroyRenderer);

    if (this->renderer == nullptr) {
        sys::panic(1, "Failed to create renderer: %s\n", SDL_GetError());
    }

    // creates a surface to load an image into the main memory
    SDL_Surface * currentSurface = SDL_GetWindowSurface(this->window.get());

    // please provide a path for your image
    SDL_Surface * imgSurface = IMG_Load("data/images/640x480.png");

    /* Create the texture */
    this->texture = std::shared_ptr<SDL_Texture>(
        SDL_CreateTextureFromSurface(this->renderer.get(), imgSurface),
        // SDL_CreateTexture(this->renderer,
        // SDL_PIXELFORMAT_ARGB8888,
        // SDL_TEXTUREACCESS_STREAMING,
        // width * SDL_PIXEL_SIZE,
        // height * SDL_PIXEL_SIZE),
        SDL_DestroyTexture);

    if (this->texture == nullptr) {
        sys::panic(1, "Failed to create texture: %s\n", SDL_GetError());
    }

    SDL_BlitSurface(imgSurface, NULL, currentSurface, NULL);
    SDL_UpdateWindowSurface(this->window.get());
    SDL_FreeSurface(imgSurface);
}
}    // namespace graphics
