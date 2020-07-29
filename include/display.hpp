/******************************************************************************
 *
 * @file            display.hpp
 *
 * @brief           Display functionality.
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

#ifndef DISPLAY_HPP__
#define DISPLAY_HPP__

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

//---------------------------------------------------------------------
// Macros
//---------------------------------------------------------------------

#define DEFAULT_DISPLAY_WIDTH  (640)
#define DEFAULT_DISPLAY_HEIGHT (480)

#define MAX_FLOOR_BUFFERS (16)

#if (HDMI_DISPLAY)
typedef uint32_t bpp_t;
#elif (TFT_DISPLAY)
typedef uint16_t bpp_t;
#else
    #error "display.hpp: Display type not specified"
#endif /* HDMI_DISPLAY */

//-----------------------------------------------------------------------------
//  Types
//-----------------------------------------------------------------------------

namespace graphics
{
enum class ActiveFrame {
    DISPLAY,
    DATA,
};

struct ScreenInfo {
    size_t width;
    size_t height;
    size_t bpp;
    int    fd;

    ScreenInfo(size_t width_, size_t height_) : width(width_), height(height_)
    {
        this->bpp = sizeof(bpp_t) * 8;
        this->fd  = -1;
    };
};

struct DisplayBuffer {
    size_t     size;
    std::mutex mtx;
    bpp_t *    data;
};

struct DisplayFloor {
    DisplayBuffer buffer;
    size_t        offset;
    size_t        last;
};

struct DisplayBufferGroup {
    ActiveFrame   last_frame;
    DisplayBuffer raw;
    DisplayBuffer display_frame; /* buffer1 */
    DisplayBuffer buffer2;
    DisplayBuffer distance; /* bufferd */
    DisplayBuffer time;     /* buffert */
    DisplayFloor  floor;
};

class Display
{
    /* Update window rendering. */
    virtual bool refresh() = 0;

    /* Initialize the buffers from the display source */
    virtual void init_buffers(const std::string & source) = 0;

  protected:
    ScreenInfo screen;

  public:
    DisplayBufferGroup buffers;

    Display(size_t width  = DEFAULT_DISPLAY_WIDTH,
            size_t height = DEFAULT_DISPLAY_HEIGHT)
        : screen(ScreenInfo(width, height)) {};
};

}    // namespace graphics

#endif /* DISPLAY_HPP__ */
