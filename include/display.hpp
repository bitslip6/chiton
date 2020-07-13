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

//---------------------------------------------------------------------
// Macros
//---------------------------------------------------------------------

#define DEFAULT_DISPLAY_WIDTH  (640)
#define DEFAULT_DISPLAY_HEIGHT (480)

//-----------------------------------------------------------------------------
//  Types
//-----------------------------------------------------------------------------

namespace graphics
{
struct ScreenInfo {
    size_t width;
    size_t height;
    size_t bpp;
    int    fd;
};

template<typename T> struct DisplayBuffer {
    size_t             size;
    std::shared_ptr<T> data {};
};

template<typename T> struct DisplayFloor {
    DisplayBuffer<T> data {};
    size_t           offset;
    size_t           last;
};

template<typename T> struct DisplayBufferGroup {
    std::shared_ptr<T> raw {};
    std::shared_ptr<T> buffer_1 {};
    std::shared_ptr<T> buffer_2 {};
    std::shared_ptr<T> buffer_d {};
    std::shared_ptr<T> buffer_t {};
    DisplayFloor<T>    floor {};

    size_t size;
};

}    // namespace graphics

#endif /* DISPLAY_HPP__ */
