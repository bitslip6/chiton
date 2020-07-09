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

#include "fplus/fplus.hpp"
#include "app.hpp"


//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

int main()
{
    app::init();

    std::list<std::string> things = { "same old", "same old" };
    if (fplus::all_the_same(things)) {
        std::cout << "All things being equal." << std::endl;
    } else {
        std::cout << "Nothing will ever be the same." << std::endl;
    }

    while (1) {
    	app::refresh();
    	usleep(5000); 		/* May not be necessary; part of example code. */
    }
}
