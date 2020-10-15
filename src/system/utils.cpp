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
#include "json.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <ctime>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <libgen.h>

#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "fplus/fplus.hpp"

//---------------------------------------------------------------------
// Class: StdLogger
//---------------------------------------------------------------------

/* Static reference to the logger's print mutex; */
std::mutex StdLogger::mtx_print;

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace utils {
	std::vector<std::string> get_command_output_lines(std::string cmd, int *retcode) {

		std::string stderr_route = " 2>&1";

		if (cmd.rfind(stderr_route) == std::string::npos) {
			cmd.append(" 2>&1");
		}

		FILE * pipe = popen(cmd.c_str(), "r");
		if (!pipe) {
			LOG("Could not open command.\n");
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

		auto ret = pclose(pipe);

		if (ret) {
			LOG("Process exited with non-zero code: %d\n", ret);
		}

		if (retcode != nullptr) {
			*retcode = ret;
		}

		return v_output;
	}

	int execute_command(std::string cmd) {
		std::string null_route = " > /dev/null 2>&1";
		if (cmd.rfind(null_route) == std::string::npos) {
			cmd.append(null_route);
		}

		FILE * pipe = popen(cmd.c_str(), "r");
		if (!pipe) {
			LOG("Could not open command.\n");
			return 1;
		}

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

	/**
	 * panic with exit code
	 */
	void Panic(int code, const char *format, ...) {
		va_list args;
		va_start(args, format);
		printf("SYSTEM PANIC!\n");
		printf(format, args);
		exit(code);
	}

	void Warn_If(bool condition, const char *format, int line, const char *file, ...) {
		va_list args;
		va_start(args, format);
		if (condition) {
			printf("WARN: %s:%d", file, line);
			printf(format, args);
		}
	}

	void Warn_IfNot(bool condition, const char *format, int line, const char *file, ...) {
		va_list args;
		va_start(args, format);
		if (!condition) {
			printf("WARN: %s:%d", file, line);
			printf(format, args);
		}
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
	 * open i2c port and address return -1 or valid open file handle
	 */
	int i2c_open(int bus, int port) {
		char filename[64];
		int file;

		snprintf(filename, 64, "/dev/i2c-%d", bus);

		file = open(filename, O_RDWR);

		if (file > 0) {
			int i=0;
			if ((i = ioctl(file, I2C_SLAVE, port)) >= 0) {
				return file;
			}
		}

		return -1;
	}

	/**
	 * returns number of bytes read into allocated_mem
	 * on error return -1
	 */
	int i2c_read(int fd, char *allocated_mem, int max_read) {
		if (fd) {
			return read(fd, (void*) allocated_mem, max_read);
		}
		return -1;
	}

	/**
	 * read i2c and close the file handle
	 */
	int i2c_read_close(int fd, char *allocated_mem, int max_read) {
		int r = i2c_read(fd, allocated_mem, max_read);
		if (fd) { close(fd); }
		return r;
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

	void calculate_gains() {
		
	}


	/**
	 * 0 out the global options
	 */
	void init_options() {
		GOPTS->max_cam_value = 6000;
		GOPTS->min_cam_value = 0;
		GOPTS->depth_frame = true;
		GOPTS->quality = 90;
		GOPTS->noise_reduction = 10;
		GOPTS->color_mode = 2;
		GOPTS->enable = true;
		GOPTS->value_offset = 0;
		GOPTS->gain = 1.5f;
		GOPTS->offset = 0.0f;
		GOPTS->center_x = 312;
		GOPTS->center_y = 174;


		std::ifstream t("config.json");
		t >> GOPTS->config;

		calculate_gains();

		//std::cout << " center X: " << GOPTS->config.at("center_x") << std::endl;
		//std::string config_str((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
		//printf("config.json: [%s]\n", config_str.c_str());
		//GOPTS->config = json::parse(config_str);

		//std::cout << GOPTS->config.dump(4);

		
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

}
