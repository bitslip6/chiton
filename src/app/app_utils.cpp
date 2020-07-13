/******************************************************************************
 *
 * @file            app_utils.cpp
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

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include "app_utils.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>

#include "fplus/fplus.hpp"

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace utils
{

std::vector<std::string> get_command_output_lines(std::string cmd)
{
    std::string stderr_route = " 2>&1";
    if (cmd.rfind(stderr_route) == std::string::npos) {
        cmd.append(" 2>&1");
    }

    FILE * pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        printf("Could not open command.\n");
        return {};
    }

    std::array<char, 128>    a_buffer;
    std::vector<std::string> v_output;

    while (fgets(a_buffer.data(), 128, pipe) != NULL) {
        /* For now this assumes each data is a full line. Could be improved
         * to section by newlines present in string, track the post-newline
         * remainder, and append next data to it as the next actual line.
         */
        std::string data = a_buffer.data();
        v_output.push_back(fplus::trim_right('\n', data));
    }

    auto retcode = pclose(pipe);

    if (retcode) {
        printf("Process exited with non-zero code: %d\n", retcode);
    }

    return v_output;
}

std::vector<std::string> clean_split(std::string str, std::string delimiter)
{
    auto split_str = fplus::split_by_token(delimiter, false, str);
    return fplus::transform(
        [](const std::string & x) -> std::string {
            return fplus::trim_whitespace(x);
        },
        split_str);
}

} // namespace utils
