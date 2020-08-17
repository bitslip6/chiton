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
//  Types
//-----------------------------------------------------------------------------

namespace wifi
{
/* Possible connection states of the wifi device. */
enum class ConnectionState {
    CONNECTED,
    CONNECTING,
    DISCONNECTED,
    UNAVAILABLE,
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

struct WifiDevice {
    std::string ifname;
    std::string state;
    std::string id;
};

class WifiNetwork
{
    std::string _ssid;

  public:
    WifiNetwork(std::string ssid = "") : _ssid(ssid) {};
    ~WifiNetwork() {};

    std::string get_ssid()
    {
        return this->_ssid;
    }
};

class AdhocNetwork
{
    const std::string _ssid;      /* Network SSID. */
    const std::string _psk;       /* Network WPA-PSK password. */
    const std::string _ipv4_addr; /* IPV4 Address */

  public:
    /* Construct an adhoc network with an SSID that is hardcoded for all
     * instances.
     */
    AdhocNetwork()
        : _ssid("chiton-hotspot"),
          _psk("chiton-hotspot"),
          _ipv4_addr("192.168.1.1") {};
    ~AdhocNetwork() {};

    /* Return whether or not the adhoc network is actively running. */
    bool is_active();

    /* Return whether or not the adhoc connection has been created. */
    bool exists();

    /* Return the adhoc network's SSID. */
    std::string get_ssid() const
    {
        return this->_ssid;
    }

    std::string get_psk() const
    {
        return this->_psk;
    }

    std::string get_ipv4() const
    {
        return this->_ipv4_addr;
    }

    /* Configure the network.
     * @return true on failure.
     */
    bool configure();

    /* Start the network. */
    void start();

    /* Stop the network. */
    void stop(bool detach = true);

    /* Delete the network configuration from the platform.
     * @return true on failure.
     */
    bool delete_configuration();
};

}    // namespace wifi

//---------------------------------------------------------------------
// Public Functions
//---------------------------------------------------------------------

namespace wifi
{
/* Get a sequence of all available wifi networks. */
std::vector<WifiNetwork> get_available_networks();

/* Connect to a specified network using its password. */
bool connect_to_network(std::string ssid, std::string password);

/* nmcli creates a network for each attempted connection to an ssid. this flushes them out */
void delete_old_networks ( void ) ;

/* Return the current system wifi connection status. This will grab the first
 * wifi device that it finds, assuming the system only has one possible wifi
 * device.
 */
ConnectionState get_connection_status();

/* Return the wifi interface name, such as "wlan0". */
std::string get_interface_name();

/* Return the active wifi connection id */
std::string get_connection_id();

/* Return information about the current wifi device. */
WifiDevice get_device_info();

/* Return the current IPv4 address of wifi, or empty if not connected. */
std::string get_ipv4_address();

/* Turn on the wifi radio.
 * @return True on success.
 */
bool turn_radio_on();

/* Turn off the wifi radio.
 * @return True on success;
 */
bool turn_radio_off();

}    // namespace wifi

#endif    // WIFI_HPP__
