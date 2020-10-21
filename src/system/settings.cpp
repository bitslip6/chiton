#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <climits>
#include <cstring>
#include <algorithm>
#include <functional>
#include <numeric>
#include <mutex>
#include <unistd.h>
#include <sys/ioctl.h>


#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "system.h"
#include "webserver.h"
#include "image.h"
#include "utils.hpp"



/**
 *  todo: refactor to use utils::
 */
auto i2c_magic = [](const uint16_t adi9083_reg_address, uint16_t& adi9083_reg_value, bool reg_read) -> int {
  int ret = 0;

  int fd = open("/dev/i2c-3", O_RDWR);
  if(fd < 0){
    fprintf(stderr, "%s:%d function:%s perror:%s\n",__FILE__,__LINE__, __FUNCTION__, strerror(errno));
    ret = -1;
    goto i2c_cleanup;
  }
  
  if (ioctl(fd, I2C_SLAVE_FORCE, 0x64) < 0) {
    fprintf(stderr, "%s:%d function:%s perror:%s\n",__FILE__,__LINE__, __FUNCTION__, strerror(errno));
    ret = -1;
    goto i2c_cleanup;
  }

    if(reg_read){
        uint8_t reg_address_read[2];
        reg_address_read[0] = ( adi9083_reg_address & 0x00FF);
        reg_address_read[1] = ( adi9083_reg_address & 0xFF00) >> 8;
        ret = write(fd, reg_address_read, 2);
        if (ret < 0) {
            fprintf(stderr, "%s:%d function:%s perror:%s\n",__FILE__,__LINE__, __FUNCTION__, strerror(errno));
            ret = -1;
            goto i2c_cleanup;
        }
        uint8_t reg_value_read[2];
        ret = read(fd, reg_value_read, 2);
        if (ret < 0) {
            fprintf(stderr, "%s:%d function:%s perror:%s\n",__FILE__,__LINE__, __FUNCTION__, strerror(errno));
            ret = -1;
            goto i2c_cleanup;
        }
        adi9083_reg_value = (reg_value_read[0] << 8) + reg_value_read[1];
        printf("i2c read success reg:%04x val:%04x\n",adi9083_reg_address,adi9083_reg_value);

    } else {
        uint8_t reg_address_value_write[4];
        reg_address_value_write[0] = ( adi9083_reg_address & 0x00FF);
        reg_address_value_write[1] = ( adi9083_reg_address & 0xFF00) >> 8;
        reg_address_value_write[2] = ( adi9083_reg_value & 0xFF00) >> 8;
        reg_address_value_write[3] = ( adi9083_reg_value & 0x00FF);
        ret = write(fd, reg_address_value_write, 4);
        if (ret < 0) {
            fprintf(stderr, "%s:%d function:%s perror:%s\n",__FILE__,__LINE__, __FUNCTION__, strerror(errno));
            ret = -1;
            goto i2c_cleanup;
        }
        printf("i2c write success reg:%04x val:%04x\n",adi9083_reg_address,adi9083_reg_value);
    } 

i2c_cleanup:
  close(fd);

  return ret;
};


int get_general_reg_value(const uint16_t adi9083_reg_address, uint16_t& adi9083_reg_value)
{
   return i2c_magic(adi9083_reg_address,adi9083_reg_value,true);
}

int set_general_reg_value(const uint16_t adi9083_reg_address, uint16_t& adi9083_reg_value)
 {
   return i2c_magic(adi9083_reg_address,adi9083_reg_value,false);
}




/**
 * update a single setting in chiton 
 */
int update_setting(std::vector<std::string> params, std::string method_name, uint min_params, std::function<int()> fn) {

    std::string url_method = params[0];
    if (url_method.compare(method_name) == 0 && params.size() >= min_params) {
        return fn();
    }
    return 404;
}


/**
 * TODO: find a better way to only set the return value than the turnary here
 */
int setting_fn(std::vector<std::string> params, int socket) {
    
    std::string data;
    int resp = 404;

    // change color rotation
    resp = (resp != 404) ? resp : update_setting(params, "degree", 2, [&]() {
        GOPTS->color_rotation = atof(params[2].c_str());
        data = "{\"value\":" + std::to_string(GOPTS->color_rotation) + "}";
        return 200;
    });

    // change image quality
    resp = (resp != 404) ? resp : update_setting(params, "quality", 2, [&]() {
        GOPTS->quality = atoi(params[2].c_str());
        data = "{\"value\":" + std::to_string(GOPTS->quality) + "}";
        return 200;
    });

    // upgrade system
    resp = (resp != 404) ? resp : update_setting(params, "version", 2, [&]() {
        std::string cmd = "/usr/local/chiton/upgrade.sh " + params[2];
        system(cmd.c_str());
        GOPTS->running = false;
        data = "{\"value\":" + params[2] + "}";
        return 200;
    });

    // enable/disable camera
    resp = (resp != 404) ? resp : update_setting(params, "camera", 2, [&]() {
        GOPTS->enable = (params[2].compare("enable") == 0);
        data = "{\"value\":" + std::to_string(GOPTS->enable) + "}";
        return 200;
    });

    // enable/disable camera
    resp = (resp != 404) ? resp : update_setting(params, "depth", 2, [&]() {
        GOPTS->depth_frame = (params[2].compare("ir") == 0);
        data = "{\"value\":" + std::to_string(GOPTS->enable) + "}";
        return 200;
    });


    // change the image color coding
    resp = (resp != 404) ? resp : update_setting(params, "colormode", 2, [&]() {
        GOPTS->color_mode = std::stoi(params[2]);
        data = "{\"value\":" + std::to_string(GOPTS->color_mode) + "}";
        return 200;
    });

    // change the max value for image coding
    resp = (resp != 404) ? resp : update_setting(params, "max", 2, [&]() {
        GOPTS->max_cam_value = std::stoi(params[2]);
        RGB8 c;
        uint16_t t = 1;
        depth_to_hsv(&t, &c, true);
        data = "{\"value\":" + std::to_string(GOPTS->max_cam_value) + "}";
        return 200;
    });

    // change the min value for image coding
    resp = (resp != 404) ? resp : update_setting(params, "min", 2, [&]() {
        GOPTS->min_cam_value = std::stoi(params[2]);
        RGB8 c;
        uint16_t t = 1;
        depth_to_hsv(&t, &c, true);
        data = "{\"value\":" + std::to_string(GOPTS->min_cam_value) + "}";
        return 200;
    });

    // toggle the adi calibration
    resp = (resp != 404) ? resp : update_setting(params, "adical", 2, [&]() {
        GOPTS->geometry = params[2] == "on";
        camera->setControl("geometry_correction", params[2]);
        data = "{\"value\":" + std::to_string(GOPTS->geometry) + "}";
        return 200;
    });

    // toggle the chiton calibration
    resp = (resp != 404) ? resp : update_setting(params, "chitoncal", 2, [&]() {
        GOPTS->chitoncal = (params[2] == "on");
        data = "{\"value\":" + std::to_string(GOPTS->chitoncal) + "}";
        return 200;
    });

    // set noise reduction
    resp = (resp != 404) ? resp : update_setting(params, "noisereduction", 2, [&]() {
        GOPTS->noise_reduction = std::stoi(params[2]);
        data = "{\"value\":" + std::to_string(GOPTS->noise_reduction) + "}";
        return 200;
    });

    // TODO: get noise values
    resp = (resp != 404) ? resp : update_setting(params, "noise", 2, [&]() {
        GOPTS->noise_reduction = std::stoi(params[2]);
        data = "{\"value\":" + std::to_string(GOPTS->noise_reduction) + "}";
        return 200;
    });

    // send temp values
    resp = (resp != 404) ? resp : update_setting(params, "temp", 2, [&]() {
        data =  "{\"cputemp\":" + std::to_string(utils::get_cpu_temp_c()) + ",\"vixeltemp\":" + std::to_string(utils::get_vixel_temp_c()) + "}";
        return 200;
    });

    // read i2c data
    resp = (resp != 404) ? resp : update_setting(params, "generalread", 2, [&]() {
        uint16_t reg_address = std::strtoul(params[2].c_str(), nullptr, 16);
        uint16_t reg_value = 0;
        int reg_status = get_general_reg_value(reg_address, reg_value);
        char hexa[5];
        snprintf(hexa,sizeof(hexa),"%04x",reg_value);
        data = "{\"status\":" + std::to_string(reg_status) + ", \"value\":\"" +hexa  + "\" }";
        return 200;
    });

    // write i2c data
    resp = (resp != 404) ? resp : update_setting(params, "generalwrite", 2, [&]() {
        uint16_t reg_address = std::strtoul(params[2].c_str(), nullptr, 16);
        uint16_t reg_value = std::strtoul(params[3].c_str(), nullptr, 16);
        int reg_status = set_general_reg_value(reg_address, reg_value);
        data = "{\"status\":" + std::to_string(reg_status) + "}";
        return 200;
    });

    std::string raw = "HTTP/1.1 " + std::to_string(resp) + " OK\nContent-Type: application/json\nAccess-Control-Allow-Origin: *\nContent-Length: " + std::to_string(data.length()) + "\n\n" + data;
    send_headers(raw, socket);

    return resp;
}
