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
#include <thread>

#include "app.hpp"
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

int main()
{
    sys::init_signal_handlers();

    std::string program_name("Chiton Test App");

    // return run_lvgl_app();
    return run_test();
}
