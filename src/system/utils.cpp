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
#include "measurement.h"
#include "resources.h"
#include <ifaddrs.h>
#include <arpa/inet.h>
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
#include "calibration.h"

 #include <linux/i2c-dev.h>
// #include <i2c/smbus.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

extern "C" {
    #include <linux/i2c.h>
    #include <linux/i2c-dev.h>
    // #include <i2c/smbus.h>
}

#include "fplus/fplus.hpp"

#define CHITON_I2C_SMBUS	0x0720	/* SMBus transfer */

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
    
    void Wait_Until(std::function<bool()> fn, int id) { int ctr = 1500; while (!fn() && (!GOPTS || GOPTS->running > 0) && ctr-- > 0) { usleep(5000); } if (ctr <= 0) { WARN("wait until max reached id [%d]!\n", id); }}
    void Wait_Forever(std::function<bool()> fn) { while (!fn() && (!GOPTS || GOPTS->running > 0)) { usleep(5000); } }

    /**
     * utility method to display a point sample
     */
    void show_sample(xy_samp_t s) {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdouble-promotion"
        printf("[%d,%d]%f  [%d,%d]%f\n", s.p1.x, s.p2.x, s.xf, s.p1.y, s.p2.y, s.yf);
        #pragma GCC diagnostic pop
    }

    bool file_exists (const std::string& name) {
        struct stat buffer;   
        return (stat (name.c_str(), &buffer) == 0); 
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
        //
        
        va_end(args);
        fprintf(stderr, "\n");
        #pragma GCC diagnostic pop
    }


   
    /**
     * run command, store return code in retcode and return vector of command output string lines
     */
    std::vector<std::string> get_command_output_lines(std::string cmd, int *retcode) {
        std::string stderr_route = " 2>&1";

        if (cmd.rfind(stderr_route) == std::string::npos) {
            cmd.append(" 2>&1");
        }

        if (retcode) *retcode = -1;
        FILE *pipe = popen(cmd.c_str(), "r");
        if (!pipe) throw std::runtime_error("unable to run command");

        std::array<char, 128>    a_buffer;
        std::vector<std::string> v_output;

        while (fgets(a_buffer.data(), 128, pipe) != NULL) {
            // For now this assumes each data is a full line. Could be improved
            // to section by newlines present in string, track the post-newline
            // remainder, and append next data to it as the next actual line.
            std::string data = a_buffer.data();
            TRACE("popen [%s]", data.c_str());
            v_output.push_back(fplus::trim_right('\n', data));
        }

        int ret = pclose(pipe);
        WARN_IF(ret != 0, "(%s) return code was not 0! [%d]", cmd.c_str(), ret);
        TRACE("ret code: %d", ret);
        if (retcode != nullptr) { *retcode = ret; }

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

		INFO("format address [%x -> %x]", register_address, reg_address_read);
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

		INFO("format data [%x -> %x]", register_data, reg_data[2]);
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
    int i2c_open_raw(int bus, UNUSED int port) {
        char filename[64];
        int file;

        // open the i2c device
        snprintf(filename, 64, "/dev/i2c-%d", bus);
        file = open(filename, O_RDWR);
        Return_If(file <= 0, "unable to open i2c bus", 0);
        return file;
    }


    /**
     * open i2c port and address return -1 or valid open file handle
     * TODO: feels a little ugly.  clean up.
     * TODO: create #define to return if condition true/false (1 line test, warn, return error)
     */
    int i2c_open(int bus, int port) {
        int file = i2c_open_raw(bus, port);
        Return_If(file <= 0, "i2c_open fail", 0);

        // ioctl to slave (_FORCE allows us to attach to kernel run devices)
        WARN_IF(ioctl(file, I2C_SLAVE_FORCE, port) != 0, "unable to force i2c slave", EXIT_I2C_ERROR);
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
    u16 i2c_read_register16(int fd, u16 register_address) {
        if (fd < 1) {
            printf("i2c register read 16 failed to open\n");
            return 0;
        }

		INFO("read 16 bit register @%x", register_address);	
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
		INFO("write 16 bit register %x@%x", register_value, register_address);	
        char *data = i2c_format_addr_data(register_address, register_value);
        WARN_IF(i2c_write(fd, data, 4) != 4, "unable to write register and address for write");
        return 1;
    }


   

    /**
     * open an I2C interface for smbus read operations
     */
    i2c_read_t i2c_smbus_open_read(int bus, int port) {
        i2c_read_t fd = (i2c_read_t)utils::i2c_open(bus, port);
        PANIC_IF(ioctl(fd, I2C_PEC, 0) < 0, "unable to set smbus packet error checking", EXIT_I2C_ERROR);
        return fd;
    }

    /**
     * open an I2C interface for smbus write operations
     */
    i2c_write_t i2c_smbus_open_write(int bus, int port) {
        i2c_write_t fd = (i2c_write_t)utils::i2c_open(bus, port);
        PANIC_IF(ioctl(fd, I2C_PEC, 1) < 0, "unable to set smbus packet error checking", EXIT_I2C_ERROR);
        return fd;
    }

    /**
     * format an address and value into smbus format data
     */
    smbus_data_t i2c_smbus_format(u16 address, u16 value) {
        auto data = std::make_unique<smbus_data_t>();
        data->address  = (address >> 8) & 0xFF;
        data->block[0] = address & 0xFF;
        data->block[1] = (value >> 8) & 0xFF;
        data->block[2] = value & 0xFF;
        data->block_sz = 3;
        return *(data);
    }
    

    s32 i2c_smbus_access(int file, char read_write, __u8 command, int size, union i2c_smbus_data *data) {
        struct i2c_smbus_ioctl_data args;
        s32 err;
    
        args.read_write = read_write;
        args.command = command;
        args.size = size;
        args.data = data;
    
        err = ioctl(file, CHITON_I2C_SMBUS, &args);
        if (err == -1) {
            err = -errno;
        }
        return err;
    }
    
    
    //int i2c_smbus_write_i2c_block_data(int fd, u16 address, u16 size, u8 * data) { PANIC("i2c fail"); return -1; }
    //int i2c_smbus_write_byte_data(int fd, u16 address, u16 data) { PANIC("i2c fail"); return -1; }
    //u16 i2c_smbus_read_word_data(int fd, u16 address) { PANIC("i2c fail"); return -1; }
    //
    s32 i2c_smbus_write_byte_data(int file, u8 command, u8 value) {
        union i2c_smbus_data data;
        data.byte = value;
        return i2c_smbus_access(file, 0, command, 2, &data);
    }
    
    s32 i2c_smbus_read_word_data(int file, u8 command) {
        union i2c_smbus_data data;
        int err;
    
        // read is 0, word data is 3
        err = i2c_smbus_access(file, 1, command, 3, &data);
        if (err < 0) {
           return err;
        }
   
       return 0x0FFFF & data.word;
    }
    
    s32 i2c_smbus_read_byte_data(int file, u8 command) {
        union i2c_smbus_data data;
        int err;
    
        // read is 0 byte data is 2
        err = i2c_smbus_access(file, 1, command, 2, &data);
        if (err < 0) {
            return err;
        }
    
        return 0x0FF & data.byte;
    }

    //
    s32 i2c_smbus_write_i2c_block_data(int file, u8 command, u8 length, const u8 *values) {
        union i2c_smbus_data data;
        int i;
        if (length > 32) { length = 32; }
        for (i = 1; i <= length; i++) {
            data.block[i] = values[i-1];
        }
        data.block[0] = length;
        // write is 0, read is 1, I2C_SMBUS_BLOCK_DATA is 5
        return i2c_smbus_access(file, 0, command, 5, &data);
    }
    
    /**
     * write register_value to register_address
     * @see i2c_smbus_open_write
     */
    void i2c_smbus_write(i2c_write_t fd, u16 register_address, u16 register_value) {
        smbus_data_t data = i2c_smbus_format(register_address, register_value);
        PANIC_IF(i2c_smbus_write_i2c_block_data((int)fd, data.address, data.block_sz, data.block) < 0, "unable to write to AFE register", EXIT_I2C_ERROR);
    }

    /**
     * read register_value from register_address
     * @see i2c_smbus_open_read
     */
    u16 i2c_smbus_read(i2c_read_t fd, u16 register_address) {
        smbus_data_t data = i2c_smbus_format(register_address, 0);
        PANIC_IF(i2c_smbus_write_byte_data((int)fd, data.address, data.block[0]) < 0, "unable to write to AFE register", EXIT_I2C_ERROR);
        u16 res = i2c_smbus_read_word_data((int)fd, data.address);
        PANIC_IF(res < 0, "unable to read from AFE register", EXIT_I2C_ERROR);
        return res;
    }



    /**
     * read a register value from the afe
     */
    u16 i2c_read_afe_register(u16 register_address) {
        i2c_read_t fd = i2c_smbus_open_read(3, 0x64);
        u16 result = i2c_smbus_read(fd, register_address);
        PANIC_IF(close(fd), "unable to close AFE register", EXIT_I2C_ERROR);
        return result;
    }


    /**
     * read a register value from the afe
     */
    void i2c_write_afe_register(u16 register_address, u16 register_value) {
        i2c_write_t fd = i2c_smbus_open_write(3, 0x64);
        i2c_smbus_write(fd, register_address, register_value);
        PANIC_IF(close(fd), "unable to close AFE register", EXIT_I2C_ERROR);
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
	u16 two_bytes_to_int(char *mem) {
		u8 m1 = (mem[0] << 8);
		u8 m2 = (mem[1]);
		u16 m3 = (m1+m2);
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
        // LINEAR REGRESSION FIT:
        u16 closest = 9999;
        vixel->offset = 9999;

        for (int test_offset = 0; test_offset < 256; test_offset++) {
            for (float test_gain = 1.28f; test_gain < 1.58f; test_gain += 0.001f) {
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


/*
        vixel->gain = 1.46484f;//(((float)(vixel->target2 - vixel->target1) / (float)(vixel->point2 - vixel->point1)) + 1.46f) / 2;
        //vixel->gain = 1.46484f;//(((float)(vixel->target2 - vixel->target1) / (float)(vixel->point2 - vixel->point1)) + 1.46f) / 2;
        vixel->gain = (float)(vixel->target2 - vixel->target1) / (float)(vixel->point2 - vixel->point1);
        int offset1 = vixel->target2 - (vixel->point2 * vixel->gain);
        int offset2 = vixel->target1 - (vixel->point1 * vixel->gain);
        int final_off = (offset1 + offset2) / 2;
        vixel->offset = final_off;//MAX(MIN(final_off, 150), -150);

        // Legacy override (part of the disabled calibration code above).
        if (vixel->vixel == 4) {
            printf("FORCE GAIN!\n");
            vixel->gain = 1.46f;
            vixel->offset = -31;
        }
        */

        //vixel->gain = 1.46f;
        //vixel->offset = 32;

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdouble-promotion"
        INFO("[%d] %f,%03d linear-fit calibrated point 1/2 [%d, %d]", vixel->vixel, vixel->gain, vixel->offset, vixel->point1, vixel->point2);
        #pragma GCC diagnostic pop
    }



    /**
     * process 1 line of json into a vixel calibration
     * calculates optimal gain and offset for 2 measurements (point1 & point2)
     * returns "new" allocated memory
     */
    VixelCalibration* vixel_to_calibration(json item) {
        VixelCalibration *v = new VixelCalibration();
        //PANIC_IF(item["vixel"].is_number(), "vixel number must be a string", EXIT_CONFIG_ERROR);
        //PANIC_IFNOT(item["radian_min"].is_number(), "radian_min required for each vixel", EXIT_CONFIG_ERROR);
        //PANIC_IFNOT(item["radian_max"].is_number(), "radian_max required for each vixel", EXIT_CONFIG_ERROR);
        
        v->vixel = (int)item["vixel"];
        v->point1 = (int)item["point1"] * (float)item["pre_gain"];
        v->point2 = (int)item["point2"] * (float)item["pre_gain"];
        v->target1 = (int)item["target1"];
        v->target2 = (int)item["target2"];
        //v->radian_min = (float)item["radian_min"];
        //v->radian_max = (float)item["radian_max"];
        v->radian = (float)item["radian"];
        return v;
    }


    /**
     * helper function to map an integer json field
     */
    int map_json_int(json data, std::string name, int default_value) {
        if (data[name].is_number_integer()) {
            return data[name];
        }

        WARN("chiton.json is missing integer field [%s]", name.c_str());
        return default_value;
    }

    void save_calibration() {

    }

    void validate_configuration(const json &config) {
        auto number = [](const json &object, const std::string &field, double low, double high, bool integer) {
            if (!object.contains(field) || !object.at(field).is_number() ||
                (integer && !object.at(field).is_number_integer()))
                throw std::invalid_argument("invalid configuration field: " + field);
            const double value = object.at(field).get<double>();
            uint64_t bits;
            static_assert(sizeof(bits) == sizeof(value), "64-bit double required");
            memcpy(&bits, &value, sizeof(bits));
            if ((bits & 0x7ff0000000000000ull) == 0x7ff0000000000000ull || value < low || value > high)
                throw std::invalid_argument("configuration field outside range: " + field);
            return value;
        };
        if (!config.is_object() || !config.contains("measurements") ||
            !config.at("measurements").is_array() || config.at("measurements").size() != 8)
            throw std::invalid_argument("configuration requires measurements 1-8");
        std::array<bool, 8> ids{};
        for (const auto &item : config.at("measurements")) {
            const int id = number(item, "vixel", 1, 8, true);
            if (ids[id - 1]) throw std::invalid_argument("duplicate calibration measurement");
            ids[id - 1] = true;
            number(item, "point1", 0, 65535, true);
            number(item, "point2", 0, 65535, true);
            number(item, "target1", 0, 9000, true);
            number(item, "target2", 0, 9000, true);
            number(item, "pre_gain", 0.001, 100, false);
            number(item, "radian", -PI2, PI2, false);
        }
        auto optional = [&](const std::string &field, double low, double high, int fallback) {
            return config.contains(field) ? number(config, field, low, high, true) : fallback;
        };
        optional("center_x", 0, CAM_WIDTH - 1, 320);
        optional("center_y", 0, CAM_HEIGHT - 1, 240);
        if (optional("inner_dist", 0, 1024, 140) >= optional("outer_dist", 1, 1024, 290))
            throw std::invalid_argument("inner_dist must be less than outer_dist");
        optional("default_log_level", 0, 5, 3);
        if (config.contains("log_level")) {
            if (!config.at("log_level").is_array()) throw std::invalid_argument("log_level must be an array");
            for (const auto &item : config.at("log_level")) {
                number(item, "level", 0, 5, true);
                if (!item.contains("function_name") || !item.at("function_name").is_string())
                    throw std::invalid_argument("function_name must be a string");
            }
        }
    }

    /**
     * 0 out the global options
     */
    void init_options() {

        GOPTS->max_cam_value = 6500;
        GOPTS->min_cam_value = 0;
        GOPTS->depth_frame = true;
        GOPTS->dynamic_illumination = false;
        GOPTS->quality = 90;
        GOPTS->noise_reduction = 80;
        GOPTS->color_mode = 1;
        GOPTS->enable = true;
        GOPTS->smoothing = false;
        GOPTS->inner_dist = 140;
        GOPTS->outer_dist = 290;
        GOPTS->draw_slam = false;
        GOPTS->default_log_level = 3;
        GOPTS->calibration_enabled = 0;
        GOPTS->eth0 = "eth: ";
        GOPTS->wlan0 = "wifi: ";
        GOPTS->message = "status: ";
        GOPTS->frame_request = false;

        std::string resolved_config = resources::find("chiton.json");
        const char *config_path = getenv("CHITON_CONFIG_FILE");
        if (!config_path || !*config_path)
            config_path = resolved_config.empty() ? "chiton.json" : resolved_config.c_str();

        json config_data;
        std::ifstream config_file(config_path);
        if (!config_file.is_open()) {
            PANIC("Cannot open configuration file: %s", config_path);
            exit(EXIT_CONFIG_ERROR);
        }
        try {
            config_file >> config_data;
            validate_configuration(config_data);
        } catch (const std::exception &error) {
            PANIC("Invalid configuration file %s: %s", config_path, error.what());
            exit(EXIT_CONFIG_ERROR);
        }


        // Construct the complete measurement set before publishing any entries.
        std::map<int, std::unique_ptr<VixelCalibration>> measurements;
        for (const auto &item : config_data["measurements"]) {
            auto vixel = std::unique_ptr<VixelCalibration>(vixel_to_calibration(item));
            find_gain(vixel.get());
            measurements.emplace(vixel->vixel, std::move(vixel));
        }
        for (auto &item : *GOPTS->cal) delete item.second;
        GOPTS->cal->clear();
        for (auto &item : measurements) GOPTS->cal->emplace(item.first, item.second.release());

        // add function log level
        if (config_data["log_level"].is_array()) {
            for (auto item : config_data["log_level"]) {
                PANIC_IF(! item["level"].is_number_integer(), "log level must be 1-5", EXIT_CONFIG_ERROR);
                PANIC_IF(! item["function_name"].is_string(), "function_name level must be string", EXIT_CONFIG_ERROR);
                GOPTS->log_level->insert(std::pair<std::string, int>(item["function_name"], item["level"]));
            }
            fplus::show(GOPTS->log_level);
        } else {
            WARN("log_level in chiton.json must be an array of {function_name:'name', level:0-5}");
        }

 
        GOPTS->center_x = map_json_int(config_data, "center_x", 320);
        GOPTS->center_y = map_json_int(config_data, "center_y", 240);
        GOPTS->inner_dist = map_json_int(config_data, "inner_dist", 140);
        GOPTS->outer_dist = map_json_int(config_data, "outer_dist", 290);
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
#ifdef CHITON_ADITOF_STUB
        return 0.0f; // No TMP108/I2C access with the file-backed camera.
#endif
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
        if (start < 0 || end < start || static_cast<size_t>(end) >= list.size())
            throw std::out_of_range("invalid slice bounds");
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
  
    uint16_t calc_mean(const std::vector<uint16_t> &data) {
        if (data.empty()) return 0;
        const uint64_t sum = std::accumulate(data.begin(), data.end(), uint64_t{0});
        return static_cast<uint16_t>(sum / data.size());
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
            WARN("std::stoi out_of_range exception. value = %d", value);
            return min;
        }
        catch (...) {
            WARN("unable to parse as int [%s]", input.c_str());
            return min;
        }

        return std::clamp(value, min, max);
    }

    void bt() {
        void *array[10];
        size_t size;

        // get void*'s for all entries on the stack
        size = backtrace(array, 10);
        // print out all the frames to stderr
        backtrace_symbols_fd(array, size, 1);
    }


    float calc_std_deviation_broke(std::vector<uint16_t> data, uint16_t mean) {
        // new list of numbers (window size)
        std::vector<int16_t> diff(data.size());
        std::vector<int16_t> diffsq(data.size());
        // calculate distance from mean for each data element
        std::transform(data.begin(), data.end(), diff.begin(), [mean](int x) { return (x - mean); });
        // calculate inner product
        std::transform(diff.begin(), diff.end(), diffsq.begin(), [](int x) { return x*x; });

        uint32_t sq_sum = std::inner_product(diffsq.begin(), diffsq.end(), diffsq.begin(), 0);

        float std = (float)sqrtf((float)(sq_sum / (int)data.size()));
        return std;
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
        std::string tar_cmd = "tar -zcvf /usr/local/chiton/" + file_name + " /usr/local/chiton/debuginfo/";
        std::string copy_targz_cmd = "mv /usr/local/chiton/" + file_name + " /usr/local/chiton/current/files/";
        if (system(tar_cmd.c_str()) != 0 || system(copy_targz_cmd.c_str()) != 0)
            throw std::runtime_error("unable to create diagnostic archive");

        return file_name;
    }

    void execute_system_cmd(std::string command) {
        std::stringstream ss(command);
        std::string cmd_name;
        std::getline(ss, cmd_name, ' ');
        std::string file_name = cmd_name + current_time_to_string();
        std::string full_cmd_name = command;// + " >> /usr/local/chiton/debuginfo/" + file_name + ".txt 2>&1";
        std::cout << "Full command line: " << full_cmd_name << std::endl;
        UNUSED int ret = system(full_cmd_name.c_str());
    }

    void execute_system_cat(std::string file_path) {
        std::stringstream ss(file_path);
        std::string file_name;
        while(getline(ss, file_name, '/')) {}
        std::string cat_cmd = "cat " + file_path + " >> /usr/local/chiton/debuginfo/" + file_name + current_time_to_string() + ".txt";
        std::cout << "Full cat line: " << cat_cmd << std::endl;
        UNUSED int ret = system(cat_cmd.c_str());
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
        std::string console_log_cmd = "cp console_log.txt /usr/local/chiton/debuginfo/" + console_log_name;
        std::string applog = "journalctl -u chiton -n 10000  >> /usr/local/chiton/debuginfo/journal.log";
        UNUSED int ret = system(console_log_cmd.c_str());
        ret = system(applog.c_str());
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
        command = "route -n";
        execute_system_cmd(command);
        command = "iwconfig";
        execute_system_cmd(command);
    }

    int redirect_stdout(void) {
        std::string full_name = "console_log.txt";
        if (access( full_name.c_str(), F_OK ) != -1) { // console_log.txt exists
            UNUSED int ret = system("rm console_log.txt");
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

    xy_t tranpose_to_topleft_coords(xy_t point) {
        xy_t result;

        result.x = point.x + GOPTS->center_x;
        result.y = GOPTS->center_y - point.y;

        return result;
    }

    /**
     * get the man distance for a calibration window
     */
    double mean_calibration_window(int index, uint16_t *raw_data1, std::vector<xy_t> points) {
        int x1 = points.at(index).x;
        int y1 = points.at(index).y;
        int x2 = x1 + 10;
        int y2 = y1 + 10;
        UNUSED int x_interval = FRAME_POLAR_WIDTH_DEFAULT;

        return calc_mean(get_window(raw_data1, FRAME_POLAR_WIDTH_DEFAULT, CAM_HEIGHT, CAM_WIDTH * CAM_HEIGHT, x1, y1, x2, y2));
    }

    double mean_cal_square_nsamples(double mean, int samples) {
        return mean/samples;
    }

    bool is_within_range(double measured_value, double ref_value, double* estimated_value, int range) {
        const double expected_gain = 1.43f;
        double compensated_value = measured_value * expected_gain;
        double upper = ref_value + (ref_value * range/100);
        double lower = ref_value - (ref_value * range/100);
        std::cout << "compensated_value: " << compensated_value << std::endl;

        if (compensated_value < upper && compensated_value > lower ) {
            (*estimated_value) = compensated_value;
            return true;
        } else {
            return false;
        }
    }

    
    void update_json_calibration(int index, bool is_near, u16 sensed_value) {
        INFO("open chiton.new");
        std::ofstream file_output("/usr/local/chiton/current/chiton.new");

        INFO("open chiton.updated");
        std::ifstream config_file("/usr/local/chiton/current/chiton.updated", std::ifstream::in);
        INFO("parsing...");
        nlohmann::ordered_json config_json = json::parse(config_file);
        INFO("parsed!");
        
        //std::string idx = (std::abs(target - GOPTS->near_calibration) < std::abs(target - GOPTS->far_calibration)) ? std::string("1") : std::string("2");
        std::string idx = (is_near) ? std::string("1") : std::string("2");
        int target = (is_near) ? GOPTS->near_calibration : GOPTS->far_calibration;
        printf("index:%d idx:%s   target:%d,   sensed:%d\n", index, idx.c_str(), target, sensed_value);
        config_json["measurements"][index-1]["pre_gain"] = 1.0;
        config_json["measurements"][index-1]["target"+idx] = target;
        config_json["measurements"][index-1]["point"+idx] = sensed_value;

        //auto j2 = R"({ "foobar": 123, "pi": "abc" })"_json;
        //printf(" WTF: %s\n", std::to_string(config_json["measurements"].at(index)["target1"]));
        printf("[\n");
        std::cout << config_json["measurements"] << std::endl;
        printf("]\n");
        
        get_command_output_lines("mv /usr/local/chiton/current/chiton.new /usr/local/chiton/current/chiton.updated");
            
        file_output << std::setw(4) << config_json << std::endl;
        //file_output << std::setw(4) << j2 << std::endl;
    }

    void set_file_contents(std::string filename, std::string contents) {
        TRACE("");
        std::ofstream outfile (filename);
        INFO("overwrite [%s]", filename.c_str());
        if (!outfile.is_open()) throw std::runtime_error("unable to open " + filename);
        outfile << contents;
        outfile.close();
        if (!outfile) throw std::runtime_error("unable to write " + filename);
    }


    std::string get_gateway_for_interface(std::string interface) {
        std::string command = "route -n | grep " + interface + " | awk '{ print $2 }'";
        std::cout << command << std::endl;
        int retcode = 0;
        std::vector<std::string> gateway_name = get_command_output_lines(command, &retcode);
        return (gateway_name.size() < 1) ? "" : gateway_name.front();
    }

    int set_gateway(std::string interface) {
        if (interface != "wlan0" && interface != "eth0") return -1;
        const auto gateway = get_gateway_for_interface("wlan0");
        if (gateway.empty()) return -1;
        set_gateway_metric(interface == "wlan0" ? 25 : 600, gateway);
        return 0;
    }

    void set_gateway_metric(int metric_value, std::string gateway_ip) {
        WARN("set_gateway_metric: metric[%d], gateway[%s]", metric_value, gateway_ip.c_str());
        std::string del_command = "sudo route del default gw " + gateway_ip + " dev wlan0";
        std::string add_command = "sudo route add -net default gw " + gateway_ip + " netmask 0.0.0.0 dev wlan0 metric " + std::to_string(metric_value);
        int result = -1;
        get_command_output_lines(del_command, &result);
        if (result != 0) throw std::runtime_error("unable to delete old gateway route");
        get_command_output_lines(add_command, &result);
        if (result != 0) throw std::runtime_error("unable to add gateway route");
    }

    std::string get_interface_address(std::string interface) {
        struct ifaddrs *interfaces = nullptr;
        std::string result;
        if (getifaddrs(&interfaces) != 0) return result;
        for (auto *item = interfaces; item; item = item->ifa_next) {
            if (!item->ifa_addr || item->ifa_addr->sa_family != AF_INET || interface != item->ifa_name) continue;
            char address[INET_ADDRSTRLEN];
            auto *ipv4 = reinterpret_cast<struct sockaddr_in *>(item->ifa_addr);
            if (inet_ntop(AF_INET, &ipv4->sin_addr, address, sizeof(address))) result = address;
            break;
        }
        freeifaddrs(interfaces);
        return result;
    }

} // namespace utils


// calculate the mean of a vector of numbers
double calc_mean(const std::vector<float> &data) {
    if (data.empty()) return 0;
    double sum = 0;
    for (float value : data) sum += static_cast<double>(value);
    return sum / data.size();
}

float calc_std_deviation(const std::vector<u16> &data, double mean) {
    return measurement::deviation(data, mean);
}


/*
int value_map(int x, int in_min, int in_max, int out_min, int out_max) {
   return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float value_mapf(float x, float in_min, float in_max, float out_min, float out_max) {
   return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
*/

