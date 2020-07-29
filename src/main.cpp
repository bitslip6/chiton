/******************************************************************************
 *
 * @file            main.cpp
 *
 * @brief           Main Chiton application front-end.
 *
 * @details
 *
 * @author(s)       Stephen Stauts, Cory Marsh
 *
 * @created         06/15/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2020 Jabil, Inc. All Rights Reserved.
 *
 *****************************************************************************/

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>

#include "app.hpp"
#include "sdl_driver.hpp"
#include "system.hpp"
#include "utils.hpp"

//-----------------------------------------------------------------------------
//  Prototypes
//-----------------------------------------------------------------------------

/* User LVGL app */
int run_lvgl_app();

/* Generic test function that does not need an app, e.g. testing FunctionalPlus
 * calls.
 */
int run_test();

/* Run the SDL/Framebuffer display demo. */
int run_display_demo(const std::string & title, uint32_t fps = 60);

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

int run_lvgl_app()
{
    utils::safe_cout("Starting GUI app...");

    app::init();

    while (1) {
        app::refresh();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    utils::safe_cout("Closing GUI app...");

    return 0;
}

int run_test()
{
    utils::safe_cout("Test complete.");
    return 0;
}

int run_display_demo(const std::string & title, uint32_t fps)
{
    utils::safe_cout("Running display demo...");

    std::string source("data/images/sw_test_target55-70mm_25.raw");

    graphics::SdlDisplay display = graphics::SdlDisplay();

    display.build_window(title);
    // display.load_image(source);
    display.init_buffers(source);

    bool close = false;

    /* Draw on the image to display in a separate thread. */
    std::thread thd_draw([&] {
        utils::safe_cout("Starting drawing thread");
        while (close == false) {
        }
        utils::safe_cout("Exiting drawing thread");
    });

    /* Refresh the display in the main context thread. */
    while (close == false) {
        SDL_Delay(1000 / fps);
        close = display.refresh();
    }

    /* Wait for spawned threads to quit. */
    thd_draw.join();

    utils::safe_cout("Closing demo...");

    return 0;
}

int main()
{
    sys::init_signal_handlers();

    std::string program_name("Chiton Test App");

    // return run_lvgl_app();
    // return run_test();
    return run_display_demo(program_name);
}
