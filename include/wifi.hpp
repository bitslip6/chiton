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
/* Possible connection states of the wifi device. */
enum class ConnectionState {
    CONNECTED,
    CONNECTING,
    DISCONNECTED,
};

/* Indices for nmcli device list output, i.e. `nmcli -t d`. */
enum class NmcliDeviceIndex {
    NAME = 0,
    TYPE,
    STATE,
    CONNECTION_ID,

    /* This must be last */
    NUM_INDICES
};

/* Indices for nmcli wifi device list output, i.e. `nmcli -t d wifi list`. */
enum class NmcliWifiDeviceIndex {
    IN_USE = 0,
    SSID,
    MODE,
    CHAN,
    RATE,
    SIGNAL,
    BARS,
    SECURITY,

    /* This must be last */
    NUM_INDICES
};

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

/* Get a sequence of all available wifi networks. */
std::vector<WifiNetwork> get_available_networks();

/* Connect to a specified network using its password. */
bool connect_to_network(std::string ssid, std::string password);

/* Return the current system wifi connection status. This will grab the first
 * wifi device that it finds, assuming the system only has one possible wifi
 * device.
 */
ConnectionState get_connection_status();

}    // namespace wifi

#endif    // WIFI_HPP__
