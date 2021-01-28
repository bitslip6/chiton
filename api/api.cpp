#include <stdio.h>
#include <stdint.h>
#include <map>
#include "utils.hpp"

extern "C" {
    
    int robot_fd = 0;
    
    void format_u16(u16 value, char *data) {
        data[0] = (value & 0xFF00) >> 8;
        data[1] = (value & 0x00FF);
    }

// API Functions:
//
// see config.json for more info 
//
// set the frame rate to a value between 1-30
// set_frame_rate(unsigned char);
//
// set the minimum size an object must be in mm to detect it
// set_min_object_size(unsigned short); 
//
// set the distance between the top of the sensor and the floor
// this setting is used for ground oclusion
// set_floor_distance(unsinged short);


/**
 * process single frame data, raw data is arranged like a standard png, or 
 * jpeg image. this method is called once for every frame.
 *
 * frame number is 16bit unsigned short located at frame[(width*height)-1]
 *
 * frame is a pointer to a list of width*hieght points of 16bit unsinged
 * integer data. each data point represents a distance value in millimeters.
 * data is arranged like a normal image, memory increases along
 * each row (width) and then after each row, the column increases (height)
 * ---->
 * ---->
 * ---->
 *
 * dist is a mapping of angles (in radians) to minimum distance values. the 
 * distance value is the distance to the closest non-floor object at that angle.
 *
 *
 */
void process_standard_frame(uint16_t *frame, int width, int height, std::map<float, unsigned short> &dist) {
	printf("4 process frame %dx%d number [%d] dist size:(%zu)\n", width, height, frame[(width*height)-1], dist.size());
    
    static uint16_t value = 1000;
    
    auto v = dist.find(0.0f);
    //printf("V: %d\n", v->second);
    if (v != dist.end()) {
        if (v->first < 0.04f && v->first > -0.04f) {
            if (robot_fd > 1) {
                //printf("%f x %d\n", it.first, it.second);
                char data[2];
                value += (0.3 * v->second) + (0.7 * value);
                format_u16(value, data);
                utils::i2c_write(robot_fd, data, 2);
            }
        }
    }
}

/**
 * called when library is loaded (dlopen)
 */
void __attribute__ ((constructor)) initLibrary(void) {
    printf("^^  Custom Chiton Library is initialized\n");
    robot_fd = utils::i2c_open(2, 0x21);
    printf("robot fd [%d]\n", robot_fd);
}

/**
 * called when library exits
 */
void __attribute__ ((destructor)) cleanUpLibrary(void) {
    printf("$$  Custom Chiton Library exited\n");
}

}