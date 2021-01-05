/******************************************************************************
 *
 * @file            utils.cpp
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

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include "json.hpp"

#include "utils.hpp"
#include "unistd.h"
#include "demos.h"
#include "system.h"
#include <fplus/fplus.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <ctime>
#include <string>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <libgen.h>
#include <stdio.h>
#include <time.h>
#include <iterator>
#include <vector>
#include <map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <execinfo.h>
#include <cxxabi.h>
#include "unistd.h"
#include "image.h"
#include "data.h"

#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "fplus/fplus.hpp"


// TODO: remove this "StdLogger" code...
// Static reference to the logger's print mutex; 
std::mutex log_mtx;


/**
 * TODO: replace with sys::panic
 * panic with exit code
 */
void Panic(int code, const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("SYSTEM PANIC errno:[%d] errstr:[%s]!\n", errno, strerror(errno));
    printf(format, args);
    printf("\n");
    exit(code);
}



namespace utils {
    
    void Wait_Until(std::function<bool()> fn) { int ctr = 500; while (!fn() && ctr-- > 0) { usleep(5000); } if (ctr <= 0) { printf("wait until max reached!\n"); }}
    void Wait_Forever(std::function<bool()> fn) { while (!fn()) { usleep(5000); } }

    /**
     * utility method to display a point sample
     */
    void show_sample(xy_samp_t s) {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdouble-promotion"
        printf("[%d,%d]%f  [%d,%d]%f\n", s.p1.x, s.p2.x, s.xf, s.p1.y, s.p2.y, s.yf);
        #pragma GCC diagnostic pop
    }

    /**
     * buffer must be at least 40 characters
     */
    void format_time(char *buffer) {
        time_t rawtime;
        struct tm * timeinfo;

        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime (buffer, 40, "%c", timeinfo);
    }

    /**
     * display a log message 
     */ 
    void log_fn(int level, const char *func, const char *file, int lineno, const char *format, ...) {
        ReturnVoidIf(GOPTS == NULL || GOPTS->log_level == NULL, "can't call log_fn before parsing config (log_level map)");
        
        static std::map<int, std::string>level_map = { {0, "PANIC"}, {1, "PANIC"}, {2, "WARN "}, {3, "INFO "}, {4, "DEBUG"}, {5, "TRACE"} };

        // function log level 
        auto func_level = GOPTS->log_level->find(func);
        int log_level = (func_level == GOPTS->log_level->end()) ? GOPTS->default_log_level : func_level->second;
        // return if log level is not high enough
        if (log_level < level) { return; }
        
        // thread locking
        std::lock_guard<std::mutex> guard(log_mtx);
        // time format
        char time_buf[80];
        utils::format_time(time_buf);

        
        va_list args;
        va_start(args, format);
        auto func_str= level_map.find(level);
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdouble-promotion"
        fprintf(stderr, "%s, %s:%d(%s) [%s] ", time_buf, file, lineno, func, func_str->second.c_str());
        vfprintf(stderr, format, args);
        
        va_end(args);
        fprintf(stderr, "\n");
        #pragma GCC diagnostic pop
    }


   
    /**
     * run command, store return code in retcode and return vector of command output string lines
     */
    std::vector<std::string> get_command_output_lines(std::string cmd, int *retcode) {
        std::string stderr_route = " 2>&1";
        INFO("popen(%s)", cmd.c_str());

        if (cmd.rfind(stderr_route) == std::string::npos) {
            cmd.append(" 2>&1");
        }

        FILE *pipe = popen(cmd.c_str(), "r");
        Return_If(pipe == NULL, "unable to run command: ", {});

        std::array<char, 128>    a_buffer;
        std::vector<std::string> v_output;

        while (fgets(a_buffer.data(), 128, pipe) != NULL) {
            // For now this assumes each data is a full line. Could be improved
            // to section by newlines present in string, track the post-newline
            // remainder, and append next data to it as the next actual line.
            std::string data = a_buffer.data();
            INFO("popen [%s]", data.c_str());
            v_output.push_back(fplus::trim_right('\n', data));
        }
        INFO("buffer done");
        //INFO("%s", fplus::show_cont(v_output).c_str());

        int ret = pclose(pipe);
        WARN_IF(ret != 0, "return code was not 0! [%d]", ret);
        INFO("ret code");
        INFO("ret code: %d", ret);
        *retcode = ret;
        //Warn_If((*retcode != 0), "command returned non-zero [%d]:", *retcode);
        INFO("returning!");
        return v_output;
    }


    int execute_command(std::string cmd) {
        std::string null_route = " > /dev/null 2>&1";
        if (cmd.rfind(null_route) == std::string::npos) {
            cmd.append(null_route);
        }

        FILE * pipe = popen(cmd.c_str(), "r");
        Return_IfNot(pipe, "unable to run cmd", 1);

        std::array<char, 128> a_buffer;
        while (fgets(a_buffer.data(), 128, pipe) != NULL) {
            // do nothing
        }

        return pclose(pipe);
    }


    std::vector<std::string> clean_split(std::string str, std::string delimiter) {
        auto split_str = fplus::split_by_token(delimiter, false, str);
        return fplus::transform(
            [](const std::string & x) -> std::string {
                return fplus::trim_whitespace(x);
            },
            split_str);
    }


    void seed_random(std::time_t * p_time) {
        std::srand(std::time(p_time));
    }

    /**
     * signed unsigned int compare,
     * returns -1,0,1 
     */
    int su_cmp(int c1, unsigned int c2) {
        if (c1 >= 0 && (unsigned) c1 >= c2) {
            return ((unsigned int)c1 > c2) ? 1 : 0;
        }
        return -1;
    }

    /**
     * format 16bit i2c address to byte array[2] address 
     * NOT THREAD SAFE, RETURNS STATIC MEMORY
     */
    char* i2c_format_addr(u16 register_address) {
        // static memory, don't want this dealocated after function returns
        static char reg_address_read[2];
        reg_address_read[0] = (register_address & 0x00FF);
        reg_address_read[1] = (register_address & 0xFF00) >> 8;

        return reg_address_read;
    }

    /**
     * i2c format address and data
     * NOT THREAD SAFE, RETURNS STATIC MEMORY
     */ 
    char* i2c_format_addr_data(u16 register_address, u16 register_data) {
        static char reg_data[4];
        memcpy(&reg_data[0], i2c_format_addr(register_address), 2);
        memcpy(&reg_data[2], i2c_format_value(register_data), 2);
        return reg_data;
    }

    /**
     * format 16bit i2c address to byte array[2] address 
     * NOT THREAD SAFE, RETURNS STATIC MEMORY
     */
    char* i2c_format_value(u16 register_value) {
        // static memory, don't want this dealocated after function returns
        static char reg_address_write[2];
        reg_address_write[0] = (register_value & 0xFF00) >> 8;
        reg_address_write[1] = (register_value & 0x00FF);
        return reg_address_write;
    }


    /**
     * open i2c port and address return -1 or valid open file handle
     * TODO: feels a little ugly.  clean up.
     * TODO: create #define to return if condition true/false (1 line test, warn, return error)
     */
    int i2c_open(int bus, int port) {
        char filename[64];
        int file;

        // open the i2c device
        snprintf(filename, 64, "/dev/i2c-%d", bus);
        file = open(filename, O_RDWR);
        Return_If(file <= 0, "unable to open i2c bus", -1);

        // ioctl to slave (_FORCE allows us to attach to kernel run devices)
        Return_If(ioctl(file, I2C_SLAVE_FORCE, port) != 0, "unable to force i2c slave", -1);
        return file;
    }

    /**
     * returns number of bytes read into allocated_mem
     * on error return -1
     */
    int i2c_read(int fd, char *allocated_mem, int max_read) {
        Return_If(fd <= 0, "unable to i2c read from null fd", -1);
        return read(fd, (void*) allocated_mem, max_read);
    }

    /**
     * returns number of bytes written from allocated_mem
     * on error return -1
     */
    int i2c_write(int fd, char *allocated_mem, int max_write) {
		if (fd > 0) { return write(fd, (void*) allocated_mem, max_write); }
    	return -1;
	}

    /**
     * read i2c and close the file handle
     */
    int i2c_read_close(int fd, char *allocated_mem, int max_read) {
        int r = -1;
        if (fd > 0) {
            r = i2c_read(fd, allocated_mem, max_read);
            close(fd);
        }
        return r;
    }

    /**
     * write i2c and close the file handle
     */
    int i2c_write_close(int fd, char *allocated_mem, int max_write) {
        int r = -1;
        if (fd > 0) {
            r = i2c_write(fd, allocated_mem, max_write);
            close(fd);
        }
		return r;
	}

    /**
     * read a 16 bit register
     */
    int i2c_read_register16(int fd, u16 register_address) {
        if (fd < 1) {
            printf("i2c register read 16 failed to open\n");
            return 0;
        }

        // printf("register address: %d\n", register_address);
        char register_data[2];
        register_data[0] = 0;
        register_data[1] = 0;
        WARN_IF(i2c_write(fd, i2c_format_addr(register_address), 2) != 2, "unable to write register address for read");
        WARN_IF(i2c_read_close(fd, register_data, 2) != 2, "unable to write register address for read");
        return two_bytes_to_int(register_data);
    }

    /**
     * read a 16 bit register
     */
    int i2c_write_register16(int fd, u16 register_address, u16 register_value) {
        if (fd < 1) {
            printf("i2c register write 16 failed to open\n");
            return 0;
        }
        char *data = i2c_format_addr_data(register_address, register_value);
        WARN_IF(i2c_write(fd, data, 4) != 4, "unable to write register and address for write");
        return 1;
    }



    /**
     * read a register value from the afe
     */
    int i2c_read_afe_register(u16 register_address) {
        return i2c_read_register16(i2c_open(3, 0x64), register_address);
    }

    /**
     * read a register value from the afe
     */
    int i2c_write_afe_register(u16 register_address, u16 register_value) {
        return i2c_write_register16(i2c_open(3, 0x64), register_address, register_value);
    }


    /**
     * convert raw i2c 108 temp sensor data to an int
     */
    int tmp108_convert(char *mem) {
        int m1 = (mem[0] << 8);
        int m2 = (mem[1]);
        int m3 = (m1+m2)>>4;
        return m3;
    }

    /**
     * convert two bytes of i2c read data to an int
     */
	int two_bytes_to_int(char *mem) {
		int m1 = (mem[0] << 8);
		int m2 = (mem[1]);
		int m3 = (m1+m2);
		return m3;
	}

    /**
     * convert read 108 temp to ferenheight
     */
    float tmp108_to_f(int converted) {
        float f = (tmp108_to_c(converted) * 1.8f) + 32;
        return f;
    }


    /**
     * convert read 108 temp to celcius
     */
    float tmp108_to_c(int converted) {
        float c = converted*0.0625f;
        return c;
    }


    /**
     * take the raw sensor input, gain and offset and produce a calibrated depth in mm
     */
    u16 calibrate_pixel(u16 raw, float gain, s16 offset) {
        return std::min((((u16)(raw * gain)) + offset), 6000);
    }

    /**
     * get the absolute distance between 2 ints
     */
    uint abs_diff(int t1, int t2) {
        return std::abs(t1 - t2);
    }

    /**
     * todo: refactor into fplus
     */
    void find_gain(VixelCalibration *vixel) {
        //float diff = 0;
        /* LINEAR REGRESSION FIT:
        u16 closest = 9999;
        vixel->offset = 9999;

        for (int test_offset = -100; test_offset < 100; test_offset++) {
            for (float test_gain = 1.3f; test_gain < 1.8f; test_gain += 0.005f) {
                u16 result1 = calibrate_pixel(vixel->point1, test_gain, test_offset);
                u16 result2 = calibrate_pixel(vixel->point2, test_gain, test_offset);

                u16 total_diff = abs_diff(vixel->target1, result1) + abs_diff(vixel->target2, result2);
                if (total_diff <= closest) {
                        closest = total_diff;
                        vixel->gain = test_gain;
                        vixel->offset = test_offset;
                }
            }
        }
        */


        vixel->gain = (float)(vixel->target2 - vixel->target1) / (float)(vixel->point2 - vixel->point1);
        int offset1 = vixel->target2 - (vixel->point2 * vixel->gain);
        int offset2 = vixel->target1 - (vixel->point1 * vixel->gain);
        vixel->offset = (offset1 + offset2) / 2;

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdouble-promotion"
        printf("[%d] %f,%03d calibrated point 1/2 [%d, %d]\n", vixel->vixel, vixel->gain, vixel->offset, vixel->point1, vixel->point2);
        #pragma GCC diagnostic pop
    }



    /**
     * process 1 line of json into a vixel calibration
     * calculates optimal gain and offset for 2 measurements (point1 & point2)
     * returns "new" allocated memory
     */
    VixelCalibration* vixel_to_calibration(json item) {
        VixelCalibration *v = new VixelCalibration();
        PANIC_IF(item["vixel"].is_number(), "vixel number must be a string", EXIT_CONFIG_ERROR);
        PANIC_IFNOT(item["radian_min"].is_number(), "radian_min required for each vixel", EXIT_CONFIG_ERROR);
        PANIC_IFNOT(item["radian_max"].is_number(), "radian_max required for each vixel", EXIT_CONFIG_ERROR);
        
        v->vixel = utils::clamp((std::string)item["vixel"], 0, 8);
        v->point1 = (int)item["point1"] * (float)item["pre_gain"];
        v->point2 = (int)item["point2"] * (float)item["pre_gain"];
        v->target1 = (int)item["target1"];
        v->target2 = (int)item["target2"];
        v->radian_min = (float)item["radian_min"];
        v->radian_max = (float)item["radian_max"];
        return v;
    }


    /**
     * helper function to map an integer json field
     */
    int map_json_int(json data, std::string name, int default_value) {
        if (data[name].is_number_integer()) {
            return data[name];
        }

        WARN("config.json is missing integer field [%s]", name.c_str());
        return default_value;
    }

    /**
     * 0 out the global options
     */
    void init_options() {

        GOPTS->max_cam_value = 7000;
        GOPTS->min_cam_value = 0;
        GOPTS->depth_frame = true;
        GOPTS->waiting = 0;
        GOPTS->quality = 90;
        GOPTS->noise_reduction = 10;
        GOPTS->color_mode = 2;
        GOPTS->enable = true;
        GOPTS->value_offset = 0;
        GOPTS->geometry = false;
        GOPTS->chitoncal = true;
        GOPTS->planar = true;
        GOPTS->inner_dist = 140;
        GOPTS->outer_dist = 290;
        GOPTS->default_log_level = 3;
        GOPTS->is_calibration_enabled = false;

        json config_data;
        try {
            std::ifstream t("/usr/local/chiton/current/config.json");
            t >> config_data;
        } catch(...) {
            PANIC_IF(true, "/usr/local/chiton/current/config.json is invalid", EXIT_CONFIG_ERROR);
        }

        PANIC_IF(config_data["measurements"].is_null(), "config.json requires measurements", EXIT_CONFIG_ERROR);

        // json code provides it's own iterator and does not support STL so no fplus support...
        for (json::iterator it = config_data["measurements"].begin(); it != config_data["measurements"].end(); ++it) {
            json foo = it.value();
            VixelCalibration *vixel = vixel_to_calibration(foo);
            find_gain(vixel);
            GOPTS->cal->insert(std::pair<int, VixelCalibration*>(utils::clamp((std::string)foo["vixel"], 0, 8), vixel));
        }

        // add function log level
        if (config_data["log_level"].is_array()) {
            for (auto item : config_data["log_level"]) {
                PANIC_IF(! item["level"].is_number_integer(), "log level must be 1-5", EXIT_CONFIG_ERROR);
                PANIC_IF(! item["function_name"].is_string(), "function_name level must be string", EXIT_CONFIG_ERROR);
                GOPTS->log_level->insert(std::pair<std::string, int>(item["function_name"], item["level"]));
            }
            fplus::show(GOPTS->log_level);
        } else {
            WARN("log_level in config.json must be an array of {function_name:'name', level:0-5}");
        }

 
        GOPTS->center_x = map_json_int(config_data, "center_x", 320);
        GOPTS->center_y = map_json_int(config_data, "center_y", 240);
        GOPTS->inner_dist = map_json_int(config_data, "inner_dist", 130);
        GOPTS->outer_dist = map_json_int(config_data, "outer_dist", 295);
        GOPTS->default_log_level = map_json_int(config_data, "default_log_level", 3);

        strcpy(GOPTS->mode, "near");
    }
    
    /**
     * get executable path for current binary
     */
    std::string get_self_path() {
        char rawPathName[PATH_MAX];
        WARN_IFNOT(realpath("/proc/self/exe", rawPathName), "unable to resolve /proc/self/exe");
        char *dir = dirname(rawPathName);
        return std::string(dir);
    }

    // get vixel temp via tmp108 sensor
    float get_vixel_temp_c() {
        char buf[BUFSIZ];
        memset(buf, 0, 10);
        int ret = i2c_read_close(i2c_open(2, 0x49), buf, 3);
        if (ret == 3) {
            return tmp108_to_c(tmp108_convert(buf));
        }

        return 0.0f;
    }

    // get cpu temp via kernel fs
    float get_cpu_temp_c() {
        std::ifstream thermalfile ("/sys/class/thermal/thermal_zone0/temp");
        if (thermalfile.is_open()) {
            std::string line;
            std::getline(thermalfile, line);
            //int temp = std::stoi(line);
            int temp = clamp(line, 0, 120000);
            thermalfile.close();
            return temp/1000.0f;
        }

        return 0.0f;
    }


    /**
     * allocate and return a new vector composed of sliced vector at start to end
     */
    std::vector<std::string> slice(std::vector<std::string> list, int start, int end) {
        auto first = list.begin() + start;
        auto last = list.begin() + end + 1;

        std::vector<std::string> result(first, last);
        return result;
    }



    template <typename R, typename... Args> std::function<R (Args...)> memoize(R (*fn)(Args...)) {
        std::map<std::tuple<Args...>, R> table;
        return [fn, table](Args... args) mutable -> R {
            auto argt = std::make_tuple(args...);
            auto memoized = table.find(argt);
            if(memoized == table.end()) {
                            auto result = fn(args...);
                            table[argt] = result;
                            return result;
            } else {
                            return memoized->second;
            }
        };
    }
  


    /**
     * helper function for calling adi methods
     * call adi function and return status
     */
    bool adi_call(std::function<aditof::Status()> fn) {
        
        aditof::Status status = aditof::Status::OK;
        status = fn();

        PANIC_IF(status != aditof::Status::OK, "adi call failed", EXIT_ADI_ERROR);
        return true;
    }


    /**
     * convert a map of string, string to a list of json key value pairs
     */
    std::string map_to_json_string(std::map<std::string, std::string> input) {
        std::string result = "{ ";

        for (std::map<std::string,std::string>::iterator it=input.begin(); it!=input.end(); ++it) {
            result = result + "\"" + it->first + "\" : '" + it->second + "', ";
        }

        return result.substr(0, result.length()-2) + " }\n";
    }


    /**
     * convert a map of string, string to a list of json key value pairs
     */
    std::string map_to_json_int(std::map<std::string, int> input) {
        std::string result = "{ ";
        for (std::map<std::string,int>::iterator it=input.begin(); it!=input.end(); ++it) {
            result = result + "\"" + it->first + "\" : " + std::to_string(it->second) + ", ";
        }
        
        return result.substr(0, result.length()-2) + " }\n";
    }
    
    /**
     * convert a map of string, string to a list of json key value pairs
     */
    std::string map_to_json_float(std::map<std::string, float> input) {
        std::string result = "{ ";

        for (std::map<std::string,float>::iterator it=input.begin(); it!=input.end(); ++it) {
            result = result + "\"" + it->first + "\" : " + std::to_string(it->second) + ", ";
        }
        
        return result.substr(0, result.length()-2) + " }\n";
    }
  
    uint16_t calc_mean(std::vector<uint16_t> data) {
        Return_IfNot(data.size() > 0, "calculate mean on empty window", 0);
        return static_cast<uint16_t>((std::accumulate(data.begin(), data.end(), 0))/data.size());
    }

    /**
     * convert a string to int and clamp to a range of values
     */
    int clamp(std::string input, int min, int max) {
        int value = min;
        try {
            value = stoi(input, FIRST_POS, STOI_DECIMAL_BASE);
        }
        catch (std::out_of_range &e) {
            PANIC("std::stoi out_of_range exception. value = %d", value);
            return min;
        }
        catch (...) {
            PANIC("unable to parse as int [%s]", input);
            return min;
        }

        return std::clamp(value, min, max);
    }


    float calc_std_deviation(std::vector<uint16_t> data, uint16_t mean) {
        // new list of numbers (window size)
        std::vector<int16_t> diff(data.size());
        std::vector<int16_t> diffsq(data.size());
        // calculate distance from mean for each data element
        std::transform(data.begin(), data.end(), diff.begin(), [mean](uint16_t x) { return (x - mean); });
        //std::cout << fplus::show(diff) << std::endl;
        // calculate inner product
        std::transform(diff.begin(), diff.end(), diffsq.begin(), [](uint16_t x) { return x*x; });

        uint32_t sq_sum = std::inner_product(diffsq.begin(), diffsq.end(), diffsq.begin(), 0);
        // square root of sum of differences
        double std = (double)std::sqrt((double)sq_sum / ((double)data.size()));
        printf("size: [%lu] sum [%u] std: [%f]\n", data.size(), sq_sum, std);

        return (float)std;
    }

    /** Print a demangled stack backtrace of the caller function to FILE* out. */
    void print_stacktrace(void) {
        const unsigned int max_frames = 64;
        FILE *out = stderr;

        fprintf(out, "stack trace:\n");

        // storage array for stack trace address data
        void *addrlist[max_frames];

        // retrieve current stack addresses
        int addrlen = backtrace(addrlist, sizeof(addrlist) / sizeof(void *));

        if (addrlen == 0) {
            fprintf(out, "  <empty, possibly corrupt>\n");
            return;
        }

        // resolve addresses into strings containing "filename(function+address)",
        // this array must be free()-ed
        char **symbollist = backtrace_symbols(addrlist, addrlen);

        // allocate string which will be filled with the demangled function name
        size_t funcnamesize = 256;
        char *funcname = (char *)malloc(funcnamesize);

        // iterate over the returned symbol lines. skip the first, it is the
        // address of this function.
        for (int i = 1; i < addrlen; i++) {
            char *begin_name = 0, *begin_offset = 0, *end_offset = 0;

            // find parentheses and +address offset surrounding the mangled name:
            // ./module(function+0x15c) [0x8048a6d]
            for (char *p = symbollist[i]; *p; ++p) {
                if (*p == '(') {
                    begin_name = p;
                } else if (*p == '+') {
                    begin_offset = p;
                }
                else if (*p == ')' && begin_offset) {
                    end_offset = p;
                    break;
                }
            }

            if (begin_name && begin_offset && end_offset && begin_name < begin_offset) {
                *begin_name++ = '\0';
                *begin_offset++ = '\0';
                *end_offset = '\0';

                // mangled name is now in [begin_name, begin_offset) and caller
                // offset in [begin_offset, end_offset). now apply
                // __cxa_demangle():

                int status;
                char *ret = abi::__cxa_demangle(begin_name,
                                                funcname, &funcnamesize, &status);
                if (status == 0) {
                    funcname = ret; // use possibly realloc()-ed string
                    fprintf(out, "  %s : %s+%s\n", symbollist[i], funcname, begin_offset);
                }
                else {
                    // demangling failed. Output function name as a C function with
                    // no arguments.
                    fprintf(out, "  %s : %s()+%s\n", symbollist[i], begin_name, begin_offset);
                }
            }
            else {
                // couldn't parse the line? print the whole line.
                fprintf(out, "  %s\n", symbollist[i]);
            }
        }

        free(funcname);
        free(symbollist);
    }

    std::string current_time_to_string(void) {
        std::chrono::system_clock::time_point time = std::chrono::system_clock::now();
        std::string format = "_%Y-%m-%d-%H-%M-%S";
        std::time_t tt = std::chrono::system_clock::to_time_t(time);
        std::tm tm = *std::gmtime(&tt);
        std::stringstream ss;

        ss << std::put_time(&tm, format.c_str());
        
        return ss.str();
    }

    std::string compress_debuginfo(void) {
        std::string file_name = "chitondebug_" + current_time_to_string() + ".tar.gz";
        std::string tar_cmd = "tar -zcvf " + file_name + " debuginfo/";
        std::string copy_targz_cmd = "mv " + file_name + " bin/files/";
        system(tar_cmd.c_str());
        system(copy_targz_cmd.c_str());

        return file_name;
    }

    void execute_system_cmd(std::string command) {
        std::stringstream ss(command);
        std::string cmd_name;
        std::getline(ss, cmd_name, ' ');
        std::string file_name = cmd_name + current_time_to_string();
        std::string full_cmd_name = command + " > debuginfo/" + file_name + ".txt 2>&1";
        std::cout << "Full command line: " << full_cmd_name << std::endl;
        system(full_cmd_name.c_str());
    }

    void execute_system_cat(std::string file_path) {
        std::stringstream ss(file_path);
        std::string file_name;
        while(getline(ss, file_name, '/')) {}
        std::string cat_cmd = "cat " + file_path + " > debuginfo/" + file_name + current_time_to_string() + ".txt";
        std::cout << "Full cat line: " << cat_cmd << std::endl;
        system(cat_cmd.c_str());
    }

    void get_rclocal_output(void) {
        WARN("rc.local requested");
        execute_system_cat("/var/log/syslog");
    }

    void get_dmesg_output(void) {
        WARN("dmesg requested");
        std::string command = "dmesg";
        execute_system_cmd(command);
    }

    void get_console_log_output(void) {
        WARN("console logs requested");
        std::string console_log_name = "console_log" + current_time_to_string() + ".txt";
        std::string console_log_cmd = "cp console_log.txt debuginfo/" + console_log_name;
        system(console_log_cmd.c_str());
    }

    void get_i2cdetect_output(void) {
        WARN("i2cdetect requested");
        execute_system_cmd("i2cdetect -y 0");
        execute_system_cmd("i2cdetect -y 1");
        execute_system_cmd("i2cdetect -y 2");
        execute_system_cmd("i2cdetect -y 3");
    }

    void get_network_status_output(void) {
        WARN("network_status requested");

        std::string command = "ifconfig -a";
        execute_system_cmd(command);
        command = "netstat -nr";
        execute_system_cmd(command);
        command = "iwconfig";
        execute_system_cmd(command);
    }

    int redirect_stdout(void) {
        std::string full_name = "console_log.txt";
        if (access( full_name.c_str(), F_OK ) != -1) { // console_log.txt exists
            system("rm console_log.txt");
        }

        int out = open(full_name.c_str(), O_RDWR | O_CREAT | O_APPEND | O_SYNC, 0600);
        if (-1 == out) {
            perror("opening console_log.txt");
            return -1;
        }

        if (-1 == dup2(out, fileno(stdout))) {
            perror("cannot redirect stdout");
            return -1;
        }
        if (-1 == dup2(out, fileno(stderr))) {
            perror("cannot redirect stderr");
            return -1;
        }

        return 0;
    }

    void update_vixel_json(int vixel_number, double pregain) {
        int n_vixel = 8;
        std::ifstream config_file;
        std::ofstream file_output("config.json.new");
        config_file.open("/home/root/chiton5/config.json.test");

        int position = n_vixel - vixel_number;

        json config_json = json::parse(config_file);

        std::cout << config_json["measurements"].at(position)["pre_gain"] << std::endl;

        config_json["measurements"].at(position)["pre_gain"] = pregain;

        std::cout << config_json["measurements"].at(position)["pre_gain"] << std::endl;
    }

    void auto_calibration(uint16_t *raw_data1) {
        // std::vector<double> avg_results(8, 0);
        // std::vector<uint16_t> square_values;
        // std::vector<calibration_status> vixel_calibration(8, calibration_status::NONE);

        // for (int i = 0; i < 8; i++) {
        //     int x1 = 88 + (i * 187);
        //     int x2 = 98 + (i * 187);

        //     square_values = get_window(raw_data1, x_interval, x1, y1, x2, y2);
        //     avg_results[i] = calc_mean(square_values);
        //     std::cout << "avg_results " << i << ": " << avg_results[i] << std::endl;
        // }
    }

    double mean_cal_square(int index, uint16_t *raw_data1) {
        int x1 = 88 + (index * 187);
        int x2 = 98 + (index * 187);
        const int y1 = 77;
        const int y2 = 87;
        int x_interval = data::get_frame_width();
        double average;

        std::vector<uint16_t> pixels = get_window(raw_data1, x_interval, x1, y1, x2, y2);
        average = calc_mean(pixels);
        //std::cout << "x1: " << x1 << "y1: " << y1 << "x2: " << x2 << "y2: " << y2 << std::endl;

        return average;
    }

    double mean_cal_square_nsamples(double mean, int samples) {
        return mean/samples;
    }

    bool return_within_range(double mean, double ref_value, int range) {
        double upper = ref_value + (ref_value * range/100);
        double lower = ref_value - (ref_value * range/100);

        if (mean < upper && mean > lower ) {
            return true;
        } else {
            return false;
        }
    }

    void update_calibration_on_json(double **calibrated_values) {
        std::ifstream config_file;
        std::ofstream file_output("config.json.new");

        config_file.open("/home/root/chiton5/config.json.test");
        json config_json = json::parse(config_file);

        for (int vixel_index = 0; vixel_index < NUMBER_OF_VIXELS; vixel_index++) {
            config_json["measurements"].at(vixel_index)["target1"] = GOPTS->near_calibration;
            config_json["measurements"].at(vixel_index)["target2"] = GOPTS->far_calibration;

            config_json["measurements"].at(vixel_index)["point1"] = calibrated_values[NEAR_DISTANCE][vixel_index];
            config_json["measurements"].at(vixel_index)["point2"] = calibrated_values[FAR_DISTANCE][vixel_index];  //remove hardcoded values
        }

        file_output << std::setw(4) << config_json << std::endl;

    }

} // namespace utils


// calculate the mean of a vector of numbers
double calc_mean(std::vector<float> data) {
     double sum=0;
     for(double d: data){
       sum+= d;
     }
     return sum/data.size();
    
    //return static_cast<double>(std::accumulate(data.begin(), data.end(), 0))/data.size();
}

double calc_std_deviation(std::vector<float> data, double mean){
    double sumsq = 0;
    for(double d: data){
      double diff = d - mean;
      sumsq += diff*diff;
    }

    return sqrt(sumsq / data.size() ); 
}
