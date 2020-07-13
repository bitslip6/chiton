/******************************************************************************
 *
 * @file            app_utils.hpp
 *
 * @brief           C/C++ utilities for app needs.
 *
 * @details
 *
 * @author(s)       Stephen Stauts
 *
 * @created         6/18/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2020 Jabil, Inc. All Rights Reserved.
 *
 *****************************************************************************/

#ifndef APP_UTILS_H__
#define APP_UTILS_H__


//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include <string>
#include <vector>

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace utils
{

/* Execute a shell command and return a vector of each line output by the
 * process. This routes stderr into the default stdout output.
 */
std::vector<std::string> get_command_output_lines(std::string cmd);

/* Split a string based on a delimiter and trim it for extraneous whitespace. */
std::vector<std::string> clean_split(std::string str, std::string delimiter);

} //namespace utils

#endif  // APP_UTILS_H__
