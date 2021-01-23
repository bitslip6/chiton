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
#include <cmath>
#include <algorithm>
#include <numeric>

#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>
/*
#include <glog/logging.h>
*/

#include "demos.h"
#include "util.h"

//-----------------------------------------------------------------------------
//  Macros
//-----------------------------------------------------------------------------

#define STDLOGGER_MAX_PRINT_LENGTH (256)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdouble-promotion"
/**
 * memory management
 */
#define NEWMEM(type, units) (type *)calloc(units, sizeof(type)); DEBUG("NEWMEM allocated %zu bytes\n", (sizeof(type) * units));


// loging helpers
#define TRACE(...) utils::log_fn(5, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define DEBUG(...) utils::log_fn(4, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define INFO(...) utils::log_fn(3, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define WARN(...) utils::log_fn(2, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)
#define PANIC(...) utils::log_fn(1, __FUNCTION__, __FILE__, __LINE__, ##__VA_ARGS__)

#define WARN_IF(conditional, ...) if ((conditional)) { WARN(__VA_ARGS__); }
#define WARN_IFNOT(conditional, ...) if (!(conditional)) { WARN(__VA_ARGS__); }

#define PANIC_IF(condition, error_str, exit_return) if ((condition)) { if (error_str) { PANIC(error_str); } exit(exit_return); }
#define PANIC_IFNOT(condition, error_str, exit_return) if ((!condition)) { if (error_str) { PANIC(error_str); } exit(exit_return); }

#define Return_If(condition, error_str, error_return) if ((condition)) { if (error_str) { WARN(error_str); } return error_return; }
#define Return_IfNot(condition, error_str, error_return) if (!(condition)) { if (error_str) { WARN(error_str); } return error_return; }
#define ReturnVoidIf(condition, error_str) if ((condition)) { if (error_str != NULL) { WARN(error_str); } return; }
#define ReturnVoid_IfNot(condition, error_str) if (!(condition)) { if (error_str != NULL) { WARN(error_str); } return; }

#define TOKEN_PASTE(x, y) x##y
#define CAT(x,y) TOKEN_PASTE(x,y)

// time the function "func" with a DEBUG message named "name"
#define TIMER(name, func) ChronoTimer CAT(cro, __LINE__); func; DEBUG("%s timed (%d us)", name, CAT(cro, __LINE__).elapsed_time_us());
#define TIMER2(name, interval, func) ChronoTimer CAT(cro, __LINE__); func; static u32 CAT(timer, __LINE__); if((CAT(timer, __LINE__)++ % interval) == 0) { DEBUG("%s timed (%d us)", name, CAT(cro, __LINE__).elapsed_time_us()); }

#define MIN(a, b) (((a) > (b)) ? b : a)
#define MAX(a, b) (((a) > (b)) ? a : b)
#define CLAMP(x, b, a) MIN(a, MAX(b, x))
#define CLAMP640(x) MAX(1, MIN(639, x))
#define CLAMP480(x) MAX(1, MIN(479, x))

#pragma GCC diagnostic pop

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


class ChronoTimer: std::chrono::high_resolution_clock {
    const std::chrono::time_point<std::chrono::high_resolution_clock> start;
public:
    ChronoTimer(): start(std::chrono::high_resolution_clock::now()) {}
    rep elapsed_time_ms() const { 
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count(); 
    }
    rep elapsed_time_us() const { 
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count(); 
    }
};

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace utils
{
    
void log_fn(int level, const char *func, const char *file, int lineno, const char *format, ...);

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
void Wait_Forever(std::function<bool()> fn);

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

    void log_fn(int level, const char *func, const char *file, int lineno, const char *format, ...);
    void format_time(char *buffer);

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
    
    std::vector<std::string> slice(std::vector<std::string> list, int start, int end);
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

    void show_sample(xy_samp_t s);
    uint16_t calc_mean(std::vector<uint16_t> data);
    //uint16_t calc_std_deviation(std::vector<uint16_t> data, uint16_t mean);
    float calc_std_deviation(std::vector<uint16_t> data, uint16_t mean);

    void print_stacktrace(void);
    int redirect_stdout(void);

    std::string compress_debuginfo(void);
    void get_rclocal_output(void);
    void get_dmesg_output(void);
    void get_console_log_output(void);
    void get_i2cdetect_output(void);
    void get_network_status_output(void);

    double calibrate_vixel(int index);
    bool is_within_range(double measured_value, double ref_value, double* estimated_value, int range);
    double mean_cal_square(int index, uint16_t *raw_data1);
    double mean_cal_square_nsamples(double mean, int samples);
    double mean_cal_square(int index, uint16_t *raw_data1, std::vector<xy_t> points);
    xy_t tranpose_to_topleft_coords(xy_t);

    void update_calibration_on_json(double **calibrated_values);

    std::vector<double> read_config_json_radians(void);

    int clamp(std::string input, int min, int max);
    
    template <typename T>
    T clamp2(const std::string &input, T min, T max)
    {
        T value = min;

        try
        {
            value = stoi(input, FIRST_POS, STOI_DECIMAL_BASE);
        }
        catch (std::invalid_argument &e)
        {
            PANIC("Invalid Argument... Exception was thrown.");
            return min;
        }
        catch (std::out_of_range &e)
        {
            PANIC("std::stoi out_of_range exception. value = %d", value);
            return min;
        }
        catch (...)
        {
            PANIC("General std::stoi exception was thrown.");
            return min;
        }

        return std::clamp(value, min, max); //c++17
    }

}    // namespace utils
double calc_mean(std::vector<float> data);

double calc_std_deviation(std::vector<float> data,double mean);



#endif    // UTILS_H__

