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

#include <iostream>
#include <memory>
#include <unistd.h>

#include "app.hpp"
#include "sdl_driver.hpp"
#include "utils.hpp"
#include "utils/command_line/CommandLineOptions.h"
#include "utils/command_line/CommandLineParser.h"

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

struct DemoOptions {
    bool use_sdl;
    bool use_framebuffer;
};

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

int run_app()
{
    std::cout << "Starting GUI app..." << std::endl;

    app::init();

    while (1) {
        app::refresh();
        usleep(5000); /* May not be necessary; part of example code. */
    }

    return 0;
}

int run_demo(const std::string & title, const DemoOptions & options)
{
    std::unique_ptr<graphics::SdlDisplay> display(new graphics::SdlDisplay());

    display->build_window(DEFAULT_TFT_WIDTH, DEFAULT_TFT_HEIGHT, title);

    usleep(3000000);

    return 0;
}

int run_test()
{
    std::cout << "Test complete." << std::endl;
    return 0;
}

int main(int argc, char ** argv)
{
    using namespace arm_compute::utils;

    std::string program_name("Chiton Test App");

    CommandLineParser             cli_parser = CommandLineParser();
    std::shared_ptr<ToggleOption> sdl_opt    = std::shared_ptr<ToggleOption>(
        cli_parser.add_option<ToggleOption>("sdl", true));
    std::shared_ptr<ToggleOption> fb_opt = std::shared_ptr<ToggleOption>(
        cli_parser.add_option<ToggleOption>("framebuffer", false));
    // std::shared_ptr<ToggleOption> help_opt    =
    // std::shared_ptr<ToggleOption>(
    //     cli_parser.add_option<ToggleOption>("help", false));

    sdl_opt->set_help("Use SDL2 for displaying the demo");
    fb_opt->set_help("Use the framebuffer for displaying the demo");
    // help_opt->set_help("Show this printout.");

    cli_parser.parse(argc, argv);
    bool valid_options = cli_parser.validate();
    if ((valid_options == false)) {
        cli_parser.print_help(program_name);
        return !valid_options;
    }

    DemoOptions options;
    options.use_sdl         = sdl_opt->value();
    options.use_framebuffer = sdl_opt->value();

    // return run_app();
    // return run_test();
    return run_demo(program_name, options);
}
