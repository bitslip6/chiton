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
 * @copyright       © 2015-2020 SWYM, LLC. All Rights Reserved.
 *
 *****************************************************************************/

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include "wifi.hpp"

#include <array>
#include <stdio.h>


//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace wifi
{
std::vector<std::string> get_command_output_lines(std::string cmd)
{
    std::string stderr_route = " 2>&1";
    if (cmd.rfind(stderr_route) == std::string::npos)
    {
        cmd.append(" 2>&1");
    }

    FILE * pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        printf("Could not open command.\n");
        return {};
    }

    std::array<char, 128> buffer;
    std::vector<std::string> output;

    while(fgets(buffer.data(), 128, pipe) != NULL)
    {
        /* For now this assumes each data is a full line. Could be improved
         * to section by newlines present in string, track the post-newline
         * remainder, and append next data to it as the next actual line.
         */
        std::string data = buffer.data();
        if (data.back() == '\n')
        {
            data.erase(data.size() - 1, 1);     /* Remove trailing newline */
        }
        output.push_back(data);
    }

    auto retcode = pclose(pipe);

    if (retcode)
    {
        printf("Process exited with non-zero code: %d\n", retcode);
    }

    return output;
}

} // namespace wifi
