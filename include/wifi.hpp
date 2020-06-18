/******************************************************************************
 *
 * @file            wifi.hpp
 *
 * @brief           Wifi usage and interaction functionality.
 *
 * @details
 *
 * @author(s)       Stephen Stauts
 *
 * @created         06/18/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2015-2020 SWYM, LLC. All Rights Reserved.
 *
 *****************************************************************************/

#ifndef WIFI_H__
#define WIFI_H__

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include <string>
#include <vector>

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace wifi
{
std::vector<std::string> get_command_output_lines(std::string cmd);
} // namespace wifi

#endif // WIFI_H__
