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

#include "utils.hpp"
#include "unistd.h"
#include "demos.h"
#include "system.h"
#include "json.hpp"
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
#include <iterator>
#include <vector>
#include <map>
#include <iostream>


#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <fcntl.h>
#include <sys/ioctl.h>


#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>
#include <glog/logging.h>


#include "fplus/fplus.hpp"

//---------------------------------------------------------------------
// Class: StdLogger
//---------------------------------------------------------------------

/* Static reference to the logger's print mutex; */
std::mutex StdLogger::mtx_print;

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------


    /**
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

    void Warn_If(bool condition, const char *format, int line, const char *file, ...) {
        va_list args;
        va_start(args, file);
        if (condition) {
            printf("WARN: %s:%d, errno:[%d] errstr:[%s]!\n", file, line, errno, strerror(errno));
            printf(format, args);
            printf("\n");
        }
    }

    void Warn_IfNot(bool condition, const char *format, int line, const char *file, ...) {
        va_list args;
        va_start(args, file);
        if (!condition) {
            printf("WARN: %s:%d, errno:[%d] errstr:[%s]!\n", file, line, errno, strerror(errno));
            printf(format, args);
            printf("\n");
        }
    }



namespace utils {
    
    void Wait_Until(std::function<bool()> fn) { while (!fn()) { usleep(10000); } }
    
    /**
     * run command, store return code in retcode and return vector of command output string lines
     */
    std::vector<std::string> get_command_output_lines(std::string cmd, int *retcode) {
        std::string stderr_route = " 2>&1";

        if (cmd.rfind(stderr_route) == std::string::npos) {
            cmd.append(" 2>&1");
        }

        FILE *pipe = popen(cmd.c_str(), "r");
        Return_IfNot(pipe, "unable to run command: ", {});

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

        Warn_If((*retcode = pclose(pipe)) != 0, "command returned non-zero [%d]:", __LINE__, __FILE__, *retcode);
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
        Warn_If(i2c_write(fd, i2c_format_addr(register_address), 2) != 2, "unable to write register address for read\n", __LINE__, __FILE__);
        Warn_If(i2c_read_close(fd, register_data, 2) != 2, "unable to write register address for read\n", __LINE__, __FILE__);
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
        Warn_If(i2c_write(fd, data, 4) != 4, "unable to write register and address for write\n", __LINE__, __FILE__);
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
        return ((u16)(raw * gain)) + offset;
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
        float diff = 0;
        u16 closest = 9999;
        vixel->offset = 9999;

        for (int test_offset = -100; test_offset < 100; test_offset++) {
            for (float test_gain = 1.3f; test_gain < 1.8f; test_gain += 0.005f) {
                u16 result1 = calibrate_pixel(vixel->point1, test_gain, test_offset);
                u16 result2 = calibrate_pixel(vixel->point2, test_gain, test_offset);

                u16 total_diff = abs_diff(vixel->target1, result1) + abs_diff(vixel->target2, result2);
                //printf("TD / CLOSE [%d, %d], TEST OFF / VIX OFFSET: [%d, %d] [%d . %d]\n", total_diff, closest, test_offset, vixel->offset, std::abs(test_offset), std::abs(vixel->offset));
                if (total_diff <= closest) {
                    //if (std::abs(test_offset) < std::abs(vixel->offset)) {
                        closest = total_diff;
                        vixel->gain = test_gain;
                        vixel->offset = test_offset;
                    //}
                }
            }
        }

        u16 result1 = calibrate_pixel(vixel->point1, vixel->gain, vixel->offset);
        u16 result2 = calibrate_pixel(vixel->point2, vixel->gain, vixel->offset);
        printf("[%d] %f,%d calibrated point 1/2 [%d, %d]\n", vixel->vixel, vixel->gain, vixel->offset, result1, result2);
    }

    /**
     * process 1 line of json into a vixel calibration
     * calculates optimal gain and offset for 2 measurements (point1 & point2)
     * returns "new" allocated memory
     */
    VixelCalibration* vixel_to_calibration(json item) {
        VixelCalibration *v = new VixelCalibration();
        v->vixel = (int)item["vixel"];
        v->point1 = (int)item["point1"] * (float)item["pre_gain"];
        v->point2 = (int)item["point2"] * (float)item["pre_gain"];
        v->target1 = (int)item["target1"];
        v->target2 = (int)item["target2"];
        return v;
    }



    /**
     * 0 out the global options
     */
    void init_options() {

        GOPTS->max_cam_value = 7000;
        GOPTS->min_cam_value = 0;
        GOPTS->depth_frame = true;
        GOPTS->quality = 90;
        GOPTS->noise_reduction = 10;
        GOPTS->color_mode = 2;
        GOPTS->enable = true;
        GOPTS->value_offset = 0;
        GOPTS->geometry = false;
        GOPTS->chitoncal = true;

        std::ifstream t("/usr/local/chiton/current/config.json");
        json config_data;
        t >> config_data;

        // json code provides it's own iterator and does not support STL so no fplus support...
        // fplus would be:
        for (json::iterator it = config_data["measurements"].begin(); it != config_data["measurements"].end(); ++it) {
            json foo = it.value();
            VixelCalibration *vixel = vixel_to_calibration(foo);
            find_gain(vixel);
            GOPTS->cal->insert(std::pair<int, VixelCalibration*>((int)foo["vixel"], vixel));
        }

        GOPTS->center_x = config_data["center_x"];
        GOPTS->center_y = config_data["center_y"];

        strcpy(GOPTS->mode, "near");
    }
    
    /**
     * get executable path for current binary
     */
    std::string get_self_path() {
       char rawPathName[PATH_MAX];
       realpath("/proc/self/exe", rawPathName);
       char *dir = dirname(rawPathName);
       return  std::string(dir);
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
            int temp = std::stoi(line);
            thermalfile.close();
            return temp/1000.0f;
        }

        return 0.0f;
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

        if (status != aditof::Status::OK) {
            std::cerr << "ADI CALL FAILED! stopping system" << std::endl;
            GOPTS->running = false;
            return false;
        }

        return true;
    }


    /**
     * convert a map of string, string to a list of json key value pairs
     */
    std::string map_to_json_string(std::map<std::string, std::string> input) {
        std::string result = "{ ";

        for (std::map<std::string,std::string>::iterator it=input.begin(); it!=input.end(); ++it) {
            result = result + "'" + it->first + "' : '" + it->second + "', ";
        }

        return result.substr(0, result.length()-2) + " }\n";
    }


    /**
     * convert a map of string, string to a list of json key value pairs
     */
    std::string map_to_json_int(std::map<std::string, int> input) {
        std::string result = "{ ";
        for (std::map<std::string,int>::iterator it=input.begin(); it!=input.end(); ++it) {
            result = result + "'" + it->first + "' : " + std::to_string(it->second) + ", ";
        }
        
        return result.substr(0, result.length()-2) + " }\n";
    }
    
    /**
     * convert a map of string, string to a list of json key value pairs
     */
    std::string map_to_json_float(std::map<std::string, float> input) {
        std::string result = "{ ";

        for (std::map<std::string,float>::iterator it=input.begin(); it!=input.end(); ++it) {
            result = result + "'" + it->first + "' : " + std::to_string(it->second) + ", ";
        }
        
        return result.substr(0, result.length()-2) + " }\n";
    }
    
}
