/******************************************************************************
 *
 * @file            system.cpp
 *
 * @brief           System-level functions
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

#include "system.hpp"

#include <csignal>
#include <cstdarg>
#include <cstdlib>

#include "utils.hpp"

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace sys
{
void panic(int code, const char * format, ...)
{
    va_list args;
    va_start(args, format);

    printf("<-- !! SYSTEM PANIC !! -->\n");
    printf(format, args);

    va_end(args);

    exit(code);
}

void init_signal_handlers()
{
    std::signal(SIGINT, ctrl_c_handler);
    std::signal(SIGTERM, ctrl_c_handler);
}

void ctrl_c_handler(int signum)
{
    printf("---> Caught CTRL+C!\n");
    exit(signum);
}

}    // namespace sys
