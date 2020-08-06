/******************************************************************************
 *
 * @file            wifi.cpp
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

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include "wifi.hpp"

#include <thread>

#include "utils.hpp"

namespace wifi
{
//---------------------------------------------------------------------
// Thread Functions
//---------------------------------------------------------------------

/* Connect to a wifi network via the appropriate nmcli command.
 * Since this can take a few seconds, it is placed into its own thread to
 * prevent the application thread from being blocked and unresponsive until
 * the connection is complete.
 */
void nmcli_connect(std::string ssid, std::string password)
{
    LOG("Connect Thread: Executing\n");

    std::string cmd("nmcli -t d wifi connect ");
    cmd.reserve(1000);
    cmd.append(ssid);
    cmd.append(" password ");
    cmd.append(password);

    [[gnu::unused]] std::vector<std::string> v_lines =
        utils::get_command_output_lines(cmd);

    LOG("Connect Thread: Exiting\n");
}

//---------------------------------------------------------------------
// Public Functions
//---------------------------------------------------------------------

void configure_adhoc( void )
{
    // Making sure that there wasn't a prior connection with the same name
    std::vector<std::string> v_lines =
        utils::get_command_output_lines("nmcli connection delete id chiton-hotspot");

    // Looking for wifi device name
    v_lines = utils::get_command_output_lines("nmcli -t d");
    std::string              delimiter  = ":";
    std::string              wifi_device = "";
    for (std::string line : v_lines) {
        auto clean_splits = utils::clean_split(line, delimiter);
        if (clean_splits.at(1).compare("wifi") == 0) {
            wifi_device = clean_splits.at(0);
        }
    }

    // Creating profile named chiton-hotspot
    v_lines =
        utils::get_command_output_lines("nmcli con add type wifi ifname "+wifi_device+" con-name chiton-hotspot autoconnect yes ssid chiton-hotspot");

    // Assigning the profile to be a wifi
    v_lines =
        utils::get_command_output_lines("nmcli con modify chiton-hotspot 802-11-wireless.mode ap 802-11-wireless.band bg ipv4.method shared");

    // Setting the password type to wpa-psk -> TODO: haven't managed to get an open network working yet
    v_lines =
        utils::get_command_output_lines("nmcli con modify chiton-hotspot wifi-sec.key-mgmt wpa-psk");

    // Setting the password to something easy to type in/remember
    v_lines =
        utils::get_command_output_lines("nmcli con modify chiton-hotspot wifi-sec.psk \"chiton12345\"");

    v_lines =
        utils::get_command_output_lines("nmcli con modify chiton-hotspot ipv4.address 192.168.1.1/25");

}

void start_adhoc ( void )
{
    std::vector<std::string> v_lines =
        utils::get_command_output_lines("nmcli con up chiton-hotspot");
}

void stop_adhoc ( void )
{
    std::vector<std::string> v_lines =
        utils::get_command_output_lines("nmcli con down chiton-hotspot");
}


std::vector<WifiNetwork> get_available_networks()
{
    /* Use "terse" output ("-t") to generate easy-to-parse output. */
    std::vector<std::string> v_lines =
        utils::get_command_output_lines("nmcli -t d wifi list");

    if (v_lines.size() == 0) {
        return {};
    }

    std::vector<WifiNetwork> v_networks = {};
    std::string              delimiter  = ":";
    for (std::string line : v_lines) {
        auto clean_splits = utils::clean_split(line, delimiter);
        v_networks.push_back(WifiNetwork(
            clean_splits.at(static_cast<int>(NmcliWifiDeviceIndex::SSID))));
    }

    return v_networks;
}

bool connect_to_network(std::string ssid, std::string password)
{
    if ((ssid.length() == 0) || (password.length() == 0)
        || (get_connection_status() == ConnectionState::CONNECTED)) {
        return false;
    }

    /* Use an actual pthread instead of LVGL task so that the nmcli command
     * doesn't cause the app to pause while executing the task. This will
     * actually allow the connection to run in the background, keeping the app
     * running in real-time.
     */
    std::thread thd_connection(nmcli_connect, ssid, password);

    /* Detach to not block-and-wait with join. Detach cleans up thread resources
     * automatically.
     */
    thd_connection.detach();

    return true;
}

ConnectionState get_connection_status()
{
    std::vector<std::string> v_lines =
        utils::get_command_output_lines("nmcli -t d");

    LOG("Checking connection status...\n");

    std::string state;
    std::string delimiter = ":";

    for (std::string line : v_lines) {
        auto device_info = utils::clean_split(line, delimiter);

        if (device_info.at(static_cast<int>(NmcliDeviceIndex::TYPE))
                .compare("wifi")
            == 0) {
            state = device_info.at(static_cast<int>(NmcliDeviceIndex::STATE));
            break;
        }
    }

    if (state.compare("connected") == 0) {
        return ConnectionState::CONNECTED;
    } else if (state.compare("disconnected") == 0) {
        return ConnectionState::DISCONNECTED;
    } else {
        return ConnectionState::CONNECTING;
    }
}

}    // namespace wifi
