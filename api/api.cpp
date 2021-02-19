#include <stdio.h>
#include <stdint.h>
#include <map>
#include <vector>
#include "utils.hpp"
#include "api.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"

extern "C" {
    
    int robot_fd = 0;
    float direction_vector = 0.2f;
    float direction_width = 0.1f;
    
    void format_u16(unsigned short value, char *data) {
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
void process_standard_frame(uint16_t *frame, int width, int height, std::vector<AngleDist> &distances) {


    /*
    printf("create [%d]\n", distances.size());
    if (distances.size() < 1) { 
        printf("return!\n");
        return;
    }
    printf("woah\n");
    */
    //printf("woah %d\n", distances.size());
    return;

    std::vector<int> min_dist = std::vector<int>();
    //min_dist.emplace_back(1);

    for (std::size_t i=1; i<distances.size()-1; ++i) {
        AngleDist dist = distances[i];

        // add all points between the zone of interest
        //if (dist.angle > (direction_vector - direction_width) && dist.angle < (direction_vector + direction_width)) {
        if (dist.angle > 0.45f && dist.angle < 1.4f) {
            //min_dist.emplace_back(dist.dist);
        }
    }

    return;

    // sort min at the top
    std::sort( min_dist.begin( ), min_dist.end( ), []( const auto& lhs, const auto& rhs ) { 
        return lhs<rhs;//std::abs(lhs->radian_diff) < std::abs(rhs->radian_diff);
    });

    // average the top 3 and return top distances

    char data[2];
    int value = min_dist.at(0);
    //printf("min dist: %d\n", value);

    format_u16(value, data);
    utils::i2c_write(robot_fd, data, 2);

    
}

void process_standard_frame2(uint16_t *frame, int width, int height, std::vector<AngleDist> &dist) {
	// printf("4 process frame %dx%d number [%d] dist size:(%zu)\n", width, height, frame[(width*height)-1], dist.size());
    
    static uint16_t value = 1000;
    
    std::vector<AngleDist>::iterator it;
    
    float begin0, end0 = 0.0f;
    float begin1, end1 = 0.0f;
    float begin2, end2 = 0.0f;
    
    uint32_t total0 = 0;
    uint32_t total1 = 0;
    uint32_t total2 = 0;

    //uint16_t dist;
    UNUSED SliceTranche t;
    t.tranche[0] = 0;
 
        /*
    for (it = dist.begin(); it != dist.end(); it++) {
        if (it->second.tranche[0] > 7 && t.tranche[0] > 7) {
            total0 += t.tranche[0];
            if (begin0 == 0.0f) {
                begin0 = it->first;
            }
        }
        else if (total0 > 0 && it->second.tranche[0] > 7) {
            end0 = it->first;
            printf("000 close [%f / %f] [%d]\n", begin0, end0, total0);
            begin0 = 0.0f;
            end0 = 0.0f;
            total0 = 0;
        }
        else if (it->second.tranche[1] > 5 && t.tranche[1] > 5) {
            begin1 = it->first;
        }
        else if (it->second.tranche[2] > 3 && t.tranche[2] > 3) {
            begin2 = it->first;
        }
        
        t.tranche[0] = it->second.tranche[0];
        t.tranche[1] = it->second.tranche[1];
        t.tranche[2] = it->second.tranche[2];
        t.tranche[3] = it->second.tranche[3];
        t.tranche[4] = it->second.tranche[4];
        t.tranche[5] = it->second.tranche[5];
        t.tranche[6] = it->second.tranche[6];
        t.tranche[7] = it->second.tranche[7];
        
        

        printf(" %f [%d] [%d] [%d] [%d] [%d] [%d] [%d] [%d]\n", it->first,
                it->second.tranche[0],
                it->second.tranche[1],
                it->second.tranche[2],
                it->second.tranche[3],
                it->second.tranche[4],
                it->second.tranche[5],
                it->second.tranche[6],
                it->second.tranche[7]);
    }
                */


    /*
    auto v = dist.find(0.0f);
    //printf("V: %d\n", v->second);
    if (v != dist.end()) {
        printf("angle:  %f\n", v->first);
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
    */
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

#pragma GCC diagnostic pop