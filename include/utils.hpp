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
#include <map>
#include <functional>

#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>
#include <glog/logging.h>

#include "util.h"

//-----------------------------------------------------------------------------
//  Macros
//-----------------------------------------------------------------------------

#define STDLOGGER_MAX_PRINT_LENGTH (256)

void Warn_If(bool condition, const char *format, int line, const char *file, ...);
void Warn_IfNot(bool condition, const char *format, int line, const char *file, ...);
void Panic(int code, const char *format, ...);

namespace utils
{
/* Shortcut allocator with logging output. */
	/*
#define NEWMEM(type, units)                                                    \
    calloc(units, sizeof(type));                                               \
    LOG("NEWMEM allocated %zu bytes\n", (sizeof(type) * units));
	*/

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

    /* Print to the output stream in a thread-safe manner.
     * This automatically appends a newline at the end of the string.
     */
    template<typename T, typename... Args>
    void log(T var, Args... args)
    {
        std::lock_guard<std::mutex> guard(this->mtx_print);
        this->recursive_print(var, args...);
    }

    /* Format and print to the output stream in a thread-safe manner. */
    template<typename... Args>
    void print(const char * format, Args... args)
    {
        char buffer[STDLOGGER_MAX_PRINT_LENGTH];
        vsnprintf(buffer, STDLOGGER_MAX_PRINT_LENGTH, format, args...);

        std::lock_guard<std::mutex> guard(this->mtx_print);
        this->recursive_print(buffer);
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
std::vector<std::string> get_command_output_lines(std::string cmd,
                                                  int * retcode = nullptr);

/* Execute a command, routing its output to /dev/null.
 * To capture or print output, use get_command_output_lines.
 */
int execute_command(std::string cmd);

/* Split a string based on a delimiter and trim it for extraneous whitespace. */
std::vector<std::string> clean_split(std::string str, std::string delimiter);

/* Seed the random number generator.
 * Using `nullptr` will default the seed to the current system time.
 */
void seed_random(std::time_t * p_time = nullptr);

int su_cmp(int c1, unsigned int c2);

void Wait_Until(std::function<bool()> fn);

}    // namespace utils

//-----------------------------------------------------------------------------
//  Templates
//-----------------------------------------------------------------------------

namespace utils
{
/* Return a random number of type T. */
template<typename T>
T get_random(T seed)
{
    return (T)std::rand() / (T)(RAND_MAX / seed + 1);
}

/* Safely free allocated memory of type T. */
template<typename T>
void safe_free(T * p_mem)
{
    if (p_mem != nullptr) {
        free(p_mem);
    }
}

/* Safely print a series of arguments to the STDOUT stream. */
template<typename T, typename... Args>
void safe_cout(T var, Args... args)
{
    StdLogger log = StdLogger(std::cout);
    log.log(var, args...);
}

/* Safely print a series of arguments to the STDERR stream. */
template<typename T, typename... Args>
void safe_cerr(T var, Args... args)
{
    StdLogger log = StdLogger(std::cerr);
    log.log(var, args...);
}

/* Safely print a formatted string to the STDOUT stream. */
template<typename... Args>
void safe_print(const char * format, Args... args)
{
    StdLogger log = StdLogger(std::cout);
    log.print(format, args...);
}

	int i2c_read_close(int fd, char *allocated_mem, int max_read);
    int i2c_write_close(int fd, char *allocated_mem, int max_read);
	int i2c_read(int fd, char *allocated_mem, int max_read);
    int i2c_write(int fd, char *allocated_mem, int max_read);
	int i2c_open(int bus, int port);
    char* i2c_format_addr(u16 register_address);
    char* i2c_format_value(u16 register_value);
    char* i2c_format_addr_data(u16 register_address, u16 register_data);
    int i2c_read_register16(int fd, u16 register_address);
    int i2c_read_afe_register(u16 register_address);
    int i2c_write_afe_register(u16 register_address, u16 register_value);
    int i2c_write_register16(int fd, u16 register_address, u16 register_value);

    float get_vixel_temp_c();
    float get_cpu_temp_c();
	int tmp108_convert(char *mem);
	float tmp108_to_f(int converted);
	float tmp108_to_c(int converted);
    int two_bytes_to_int(char *mem);
	void init_options();
	uint8_t *get_data_u8(std::string filename, int *out_sz);
	std::string get_self_path();
    int su_cmp(int c1, unsigned int c2);
    uint16_t calibrate_pixel(u16 raw, float gain, s16 offset);
    bool adi_call(std::function<aditof::Status()> fn);
    std::string map_to_json_string(std::map<std::string, std::string> input);
    std::string map_to_json_int(std::map<std::string, int> input);
    std::string map_to_json_float(std::map<std::string, float> input);
    
    std::vector<std::string> get_command_output_lines(std::string cmd, int *retcode);
	template <typename R, typename... Args> std::function<R (Args...)> memoize(R (*fn)(Args...));

}    // namespace utils

#endif    // UTILS_H__
