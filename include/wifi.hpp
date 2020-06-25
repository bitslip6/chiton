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
 * @copyright       © 2020 Jabil, Inc. All Rights Reserved.
 *
 *****************************************************************************/

#ifndef WIFI_HPP__
#define WIFI_HPP__

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
/* Return a vector of SSID strings for available wifi networks. */
std::vector<std::string> get_available_networks();

} // namespace wifi

#endif // WIFI_HPP__
