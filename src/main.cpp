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
int run_app();

/* Generic test function that does not need an app, e.g. testing FunctionalPlus
 * calls.
 */
int run_test();

/* Run the SDL/Framebuffer display demo. */
int run_demo(const std::string & title, uint32_t fps = 60);

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

int run_app()
{
    std::cout << "Starting GUI app..." << std::endl;

    app::init();

    while (1) {
        app::refresh();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    std::cout << "Closing GUI app..." << std::endl;

    return 0;
}

int run_test()
{
    std::cout << "Test complete." << std::endl;
    return 0;
}

int run_demo(const std::string & title, uint32_t fps)
{
    std::cout << "Running display demo..." << std::endl;

    std::unique_ptr<graphics::SdlDisplay> display(new graphics::SdlDisplay());

    display->build_window(DEFAULT_TFT_WIDTH, DEFAULT_TFT_HEIGHT, title);

    std::string image("data/images/640x480.png");
    display->load_image(image);

    bool close = false;
    while (!close) {
        SDL_Delay(1000 / fps);
        close = display->refresh();
    }

    std::cout << "Closing demo..." << std::endl;

    return 0;
}

int main()
{
    sys::init_signal_handlers();

    std::string program_name("Chiton Test App");

    // return run_app();
    // return run_test();
    return run_demo(program_name);
}
