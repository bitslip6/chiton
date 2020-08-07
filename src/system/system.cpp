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
#include <vector>

#include "utils.hpp"

namespace
{
std::vector<shutdown_fn_t> v_fns = {};
}    // namespace

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace sys
{
void panic(int code, const char * format, ...)
{
    va_list args;
    va_start(args, format);

    utils::safe_cerr("<-- !! SYSTEM PANIC !! -->");
    utils::safe_print(format, args);

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
    utils::safe_cerr("---> Caught CTRL+C!");

    shutdown_now(signum);
}

void register_shutdown_fn(shutdown_fn_t func)
{
    v_fns.push_back(func);
}

void shutdown_now(int exit_code)
{
    if (v_fns.empty() == false) {
        utils::safe_cerr("Executing registered shutdown functions");
        for (auto fn : v_fns) {
            fn();
        }
    }
    exit(exit_code);
}

}    // namespace sys
