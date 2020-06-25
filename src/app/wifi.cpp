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

#include "app_utils.hpp"
#include "fplus/fplus.hpp"

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace wifi
{

//---------------------------------------------------------------------
// Public Functions
//---------------------------------------------------------------------

std::vector<WifiNetwork> get_available_networks()
{
    std::vector<std::string> v_lines =
        utils::get_command_output_lines("nmcli device wifi list");

    /* The first line out is the header line, so finding a single network would
     * make the minimum length 2.
     */
    if (v_lines.size() < 2) {
        return {};
    }

    /* Remove the header line from iterators */
    v_lines.erase(v_lines.begin());

    const auto ssid_idx = 1; /* nmcli always outputs SSID in as element [1] */

    std::vector<WifiNetwork> v_networks = {};
    std::string              delimiter  = "  ";
    for (std::string line : v_lines) {
        auto clean_splits = utils::clean_split(line, delimiter);
        v_networks.push_back(WifiNetwork(clean_splits.at(ssid_idx)));
    }

    return v_networks;
}

} // namespace wifi
