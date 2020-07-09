/******************************************************************************
 *
 * @file            utils.hpp
 *
 * @brief           C/C++ utilities for application needs.
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

#ifndef UTILS_H__
#define UTILS_H__


//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include <string>
#include <vector>

#include "lvgl/lvgl.h"

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace utils
{
#ifdef DEBUG
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif /* DEBUG */

/* Helper class for maintaining pointers to text areas containg credential
 * information. This allows the app to have the information passed as an object
 * into callback functions.
 */
class CredentialTexts
{
    lv_obj_t * ta_id;
    lv_obj_t * ta_pwd;

  public:
    CredentialTexts(lv_obj_t * ta_id_, lv_obj_t * ta_pwd_)
        : ta_id(ta_id_), ta_pwd(ta_pwd_) {};

    ~CredentialTexts() {};

    std::string get_id()
    {
        return std::string(lv_textarea_get_text(this->ta_id));
    }

    std::string get_pwd()
    {
        return std::string(lv_textarea_get_text(this->ta_pwd));
    }

    /* Clear the credential string texts. */
    void erase()
    {
        lv_textarea_set_text(this->ta_id, "");
        lv_textarea_set_text(this->ta_pwd, "");
    }
};

/* Execute a shell command and return a vector of each line output by the
 * process. This routes stderr into the default stdout output.
 */
std::vector<std::string> get_command_output_lines(std::string cmd);

/* Split a string based on a delimiter and trim it for extraneous whitespace. */
std::vector<std::string> clean_split(std::string str, std::string delimiter);

}    // namespace utils

#endif    // UTILS_H__
