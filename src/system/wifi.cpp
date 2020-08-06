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
// Class: AdhocNetwork
//---------------------------------------------------------------------
bool AdhocNetwork::_active = false;

bool AdhocNetwork::exists()
{
    return utils::execute_command("nmcli -p con show " + this->get_ssid()) == 0;
}

bool AdhocNetwork::configure()
{
    if (this->is_active()) {
        LOG("Cannot configure adhoc network - already active.\n");
        return 1;
    }

    // Delete configuration; ignore error if it has not yet been created
    this->delete_configuration();

    std::string ifname(get_interface_name());
    std::string ssid(this->get_ssid());
    std::string pwd(ssid);

    std::string modify_root("nmcli con modify ");
    modify_root.append(ssid);

    std::vector<std::string> v_commands = {};

    // Add the network
    v_commands.push_back("nmcli con add type wifi ifname " + ifname
                         + " con-name " + ssid + " autoconnect yes ssid "
                         + ssid);

    // Assign as wifi profile
    v_commands.push_back(modify_root
                         + " 802-11-wireless.mode ap "
                           "802-11-wireless.band bg ipv4.method shared");

    // Assign WPA-PSK security
    v_commands.push_back(modify_root + " wifi-sec.key-mgmt wpa-psk");

    // Assign password
    v_commands.push_back(modify_root + " wifi-sec.psk \"" + pwd + "\"");

    // Assign ipv4 address
    v_commands.push_back(modify_root + " ipv4.address 192.168.1.1/25");

    for (auto cmd : v_commands) {
        auto retcode = utils::execute_command(cmd);

        if (retcode) {
            LOG("Adhoc configuration failed:\n");
            LOG("\t\"%s\" --> exit code %d\n", cmd.c_str(), retcode);
            return true;
        }
    }

    return false;
}

bool AdhocNetwork::start()
{
    if (this->is_active()) {
        LOG("Adhoc network already started.\n");
        return true;
    }

    auto retcode = utils::execute_command("nmcli con up " + this->get_ssid());

    this->_active = (retcode == 0);

    return !this->_active;
}

bool AdhocNetwork::stop()
{
    if (this->is_active() == false) {
        LOG("Cannot stop adhoc network; not active.\n");
        return true;
    }

    auto retcode = utils::execute_command("nmcli con down " + this->get_ssid());

    this->_active = (retcode != 0);

    return this->_active;
}

bool AdhocNetwork::delete_configuration()
{
    return static_cast<bool>(utils::execute_command(
        "nmcli connection delete id " + this->get_ssid()));
}

//---------------------------------------------------------------------
// Public Functions
//---------------------------------------------------------------------

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

std::string get_interface_name()
{
    std::vector<std::string> v_lines =
        utils::get_command_output_lines("nmcli -t d");

    std::string ifname("Unknown");
    std::string delimiter = ":";

    for (std::string line : v_lines) {
        auto device_info = utils::clean_split(line, delimiter);

        if (device_info.at(static_cast<int>(NmcliDeviceIndex::TYPE))
                .compare("wifi")
            == 0) {
            ifname = device_info.at(static_cast<int>(NmcliDeviceIndex::NAME));
            break;
        }
    }

    return ifname;
}


}    // namespace wifi
