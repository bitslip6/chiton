/******************************************************************************
 *
 * @file            utils.hpp
 *
 * @brief           C/C++ utilities for application needs.
 *
 * @details
 *
 * @author(s)       Stephen Stauts, Cory Marsh
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

#include <chrono>
#include <cstdarg>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

//-----------------------------------------------------------------------------
//  Macros
//-----------------------------------------------------------------------------

namespace utils
{
#ifdef DEBUG
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif /* DEBUG */

/* Shortcut allocator with logging output. */
#define NEWMEM(type, units)                                                    \
    calloc(units, sizeof(type));                                               \
    LOG("NEWMEM allocated %zu bytes\n", (sizeof(type) * units));

}    // namespace utils

//---------------------------------------------------------------------
// Types
//---------------------------------------------------------------------

/* Thread-safe logger for standard output streams. */
class StdLogger
{
    std::ostream &    stdstream;
    static std::mutex mtx_print;

    /* Recursive base case */
    void recursive_print()
    {
        stdstream << std::endl;
    }

    /* Recursively print to output stream for each arg. */
    template<typename T, typename... Args>
    void recursive_print(T var, Args... args)
    {
        stdstream << var;
        this->recursive_print(args...);
    }


  public:
    /* Construct the logger with its specified output stream. */
    StdLogger(std::ostream & os = std::cout) : stdstream(os) {};
    ~StdLogger() {};

    /* Print to the output stream in a thread-safe manner. */
    template<typename T, typename... Args> void log(T var, Args... args)
    {
        std::lock_guard<std::mutex> guard(this->mtx_print);
        this->recursive_print(var, args...);
    }
};

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace utils
{
/* Execute a shell command and return a vector of each line output by the
 * process. This routes stderr into the default stdout output.
 */
std::vector<std::string> get_command_output_lines(std::string cmd);

/* Split a string based on a delimiter and trim it for extraneous whitespace. */
std::vector<std::string> clean_split(std::string str, std::string delimiter);

/* Seed the random number generator.
 * Using `nullptr` will default the seed to the current system time.
 */
void seed_random(std::time_t * p_time = nullptr);

}    // namespace utils

//-----------------------------------------------------------------------------
//  Templates
//-----------------------------------------------------------------------------

namespace utils
{
/* Return a random number of type T. */
template<typename T> T get_random(T seed)
{
    return (T)std::rand() / (T)(RAND_MAX / seed + 1);
}

/* Safely free allocated memory of type T. */
template<typename T> void safe_free(T * p_mem)
{
    if (p_mem != nullptr) {
        free(p_mem);
    }
}

/* Safely print a series of arguments to the STDOUT stream. */
template<typename T, typename... Args> void safe_cout(T var, Args... args)
{
    StdLogger log = StdLogger(std::cout);
    log.log(var, args...);
}

/* Safely print a series of arguments to the STDERR stream. */
template<typename T, typename... Args> void safe_cerr(T var, Args... args)
{
    StdLogger log = StdLogger(std::cerr);
    log.log(var, args...);
}

}    // namespace utils

#endif    // UTILS_H__
