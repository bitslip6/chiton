/******************************************************************************
 *
 * @file            system.hpp
 *
 * @brief           System-level functions.
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

#ifndef SYSTEM_HPP__
#define SYSTEM_HPP__

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace sys
{
extern "C" {
/* Exit the program with a specific code and message. */
void panic(int code, const char * format, ...);
}    // extern "C"

/* Initialize signal handlers. */
void init_signal_handlers();

extern "C" {

/* Handler for CTRL-C (SIGINT). */
void ctrl_c_handler(int signum);

}    // extern "C"
}    // namespace sys

#endif /* SYSTEM_HPP__ */
