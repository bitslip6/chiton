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

#include <stdbool.h>
#include <string>
#include <vector>

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace wifi
{
class WifiNetwork
{
    std::string ssid;

  public:
    WifiNetwork(std::string ssid_ = "") : ssid(ssid_) {};
    ~WifiNetwork() {};

    std::string get_ssid()
    {
        return this->ssid;
    }
};

std::vector<WifiNetwork> get_available_networks();

} // namespace wifi

#endif // WIFI_HPP__
