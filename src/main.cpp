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
#include <unistd.h>

#include "app.hpp"
#include "utils.hpp"

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

int run_test()
{
    std::cout << "Test complete." << std::endl;
    return 0;
}

int main()
{
    // return run_app();
    return run_test();
}
