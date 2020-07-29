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
#include <cstdbool>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <sys/mman.h>
#include <sys/stat.h>

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
    utils::safe_cout("Unmapping display source");
    munmap(this->buffers.raw.data, this->buffers.raw.size);

    utils::safe_cout("Freeing allocated buffer memory");
    std::free(this->buffers.buffer2.data);
    std::free(this->buffers.display_frame.data);
    std::free(this->buffers.distance.data);
    std::free(this->buffers.time.data);
}

void SdlDisplay::build_window(const std::string & title)
{
    SDL_Init(SDL_INIT_VIDEO);    // Initialize SDL2

    /* Create the window and assign its smart pointer the specific SDL deleter
     * function.
     */
    this->window = std::shared_ptr<SDL_Window>(
        SDL_CreateWindow(title.c_str(),              // window title
                         SDL_WINDOWPOS_UNDEFINED,    // initial x position
                         SDL_WINDOWPOS_UNDEFINED,    // initial y position
                         this->screen.width,         // width, in pixels
                         this->screen.height,        // height, in pixels
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
}

void SdlDisplay::init_buffers(const std::string & source)
{
    utils::safe_cout("Opening ", source);
    this->screen.fd = open(source.c_str(), O_RDWR);
    if (this->screen.fd == -1) {
        sys::panic(1, "Error: Cannot open display source: %s", source.c_str());
    }

    // smem_len for fd_buffer. raw file format equiv for SDL?
    this->buffers.raw.size = this->screen.width * this->screen.height;
    this->buffers.raw.data = (bpp_t *)mmap(NULL,
                                           this->buffers.raw.size,
                                           PROT_READ,
                                           MAP_SHARED,
                                           this->screen.fd,
                                           0);
    if (this->buffers.raw.data == MAP_FAILED) {
        sys::panic(5, "Error: Failed to map display source to memory.");
    }

    this->buffers.floor.buffer.size = this->screen.width * this->screen.height;
    this->buffers.floor.last =
        this->buffers.floor.buffer.size * (MAX_FLOOR_BUFFERS - 1);

    /* Load fonts */

    /* Setup buffer memory */
    std::memset(this->buffers.raw.data, 0, this->buffers.raw.size);

    this->buffers.display_frame.size = this->buffers.raw.size;
    this->buffers.display_frame.data = static_cast<bpp_t *>(
        std::calloc(this->buffers.display_frame.size, sizeof(bpp_t)));

    this->buffers.buffer2.size =
        this->buffers.raw.size * (MAX_FLOOR_BUFFERS + 1) * sizeof(bpp_t);
    this->buffers.buffer2.data = static_cast<bpp_t *>(
        std::calloc(this->buffers.buffer2.size, sizeof(bpp_t)));

    this->buffers.distance.size = this->buffers.raw.size;
    this->buffers.distance.data = static_cast<bpp_t *>(
        std::calloc(this->buffers.distance.size, sizeof(bpp_t)));

    this->buffers.time.size = this->buffers.raw.size;
    this->buffers.time.data = static_cast<bpp_t *>(
        std::calloc(this->buffers.time.size, sizeof(bpp_t)));

    auto floor_end = this->buffers.floor.buffer.size * MAX_FLOOR_BUFFERS;
    auto frame_end = this->buffers.buffer2.size;

    if (floor_end >= frame_end) {
        sys::panic(
            1, "Error: Would have attempted to write more than buffer2 size");
    }

    for (size_t i = 0; i < frame_end; i++) {
        this->buffers.buffer2.data[i] = 4095;
    }
}

void SdlDisplay::load_image(const std::string & image)
{
    SDL_Surface * currentSurface = SDL_GetWindowSurface(this->window.get());

    SDL_Surface * imgSurface = IMG_Load(image.c_str());

    /* Create the texture */
    this->texture = std::shared_ptr<SDL_Texture>(
        SDL_CreateTextureFromSurface(this->renderer.get(), imgSurface),
        SDL_DestroyTexture);

    if (this->texture == nullptr) {
        sys::panic(1, "Failed to create texture: %s\n", SDL_GetError());
    }

    SDL_BlitSurface(imgSurface, NULL, currentSurface, NULL);
    SDL_UpdateWindowSurface(this->window.get());
    SDL_FreeSurface(imgSurface);
}

bool SdlDisplay::refresh()
{
    SDL_Event event;
    bool      exit_now = false;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            // handling of close button
            exit_now = true;
            break;

        default:
            break;
        }
    }

    return exit_now;
}

}    // namespace graphics
