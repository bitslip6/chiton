#include <math.h>
#include <sstream>
#include <stdio.h>
#include <map>
#include <vector>
#include <string>
#include <stdint.h>
#include <stdlib.h>
#include <stdlib.h>
#include <unistd.h>

#include "demos.h"
#include "system.h"
#include "utils.hpp"
#include "rover.h"
//#include <arm_fp16.h>
//#include <arm_neon.h>


GlobalOpts *GOPTS = NULL;
unsigned char *jpeg_rgb = NULL;
RGB8 *hsv_cache;

/*
void test_floats() {

    for (int frame=0; frame<30; frame++) {
        for (int i=0; i<640*480; i++) {

        }
    }

}
*/


/*
float point_cloud(const float *n_angle_sin_vert_cos, const float *vert_sin, const float *angle_cos_vert_cos, const float *angle_cos, const float *gain, const float *offset, const uint16_t *src, uint16_t *dst, int16_t *pc) {
    //const uint max = calibration::ge;
    int idx = 0;
    uint *offsets;
    uint16_t *offlen;
    for (uint j=0; j<max; j++) {
        uint runstart __attribute__ ((aligned (4))) = offsets[j];
        uint runlen __attribute__ ((aligned (4))) = offlen[j];
        for (uint i=runstart; i<(runlen & ~3); i++) {
            dst[i] = (src[i] * gain[i]) + offset[i];
            pc[idx] = n_angle_sin_vert_cos[i] * dst[i];
            pc[idx+1] = vert_sin[i] * dst[i];
            pc[idx+2] = angle_cos_vert_cos[i] * dst[i];
            pc[idx+3] = dst[i];
            idx+=4;
        }
        //printf("%d.%d\n", runstart, runlen);
        //std::cout << j;
    }
    return (float)pc[128];
}
*/

int main(int arc, char **argv)
{

/*
    rover::tx_data data;
    int uart_fd = rover::uart_connect();
    printf("uart connect: [%d]\n", uart_fd);

    data.left_motor = 160;
    data.right_motor = 160;
    data.param_1 = 0;
    data.param_2 = 0;
    rover::tx_uart(uart_fd, data);

    sleep(1);

    data.left_motor = 125;
    data.right_motor = 125;
    rover::tx_uart(uart_fd, data);
    */


    GOPTS = (GlobalOpts *) malloc(sizeof(GlobalOpts));
    GOPTS->log_level = new std::map<std::string, int>;
    GOPTS->center_x = 320;
    GOPTS->center_y = 240;
    printf("scratch!\n");

    calibration::make_calibration_data();


    std::ifstream file1("frame1.raw", std::ios::binary);
    printf("frame1!\n");
    std::ifstream file2("frame2.raw", std::ios::binary);
    printf("frame2!\n");

    std::streamsize size1 = file1.tellg();
    std::streamsize size2 = file2.tellg();
    printf("frame sz [%d]!\n", size1);
    file1.seekg(0, std::ios::beg);
    file2.seekg(0, std::ios::beg);
    printf("frame seek!\n");

//#define NEWMEM(type, units) (type *)calloc(units, sizeof(type)); DEBUG("NEWMEM allocated %zu bytes\n", (sizeof(type) * units));
    u16 *frame1 = (u16 *)calloc(640*480, sizeof(u16));//NEWMEM(u16, 640*480);
    printf("frame new mem 1!\n");

    //u16 *frame2 = NEWMEM(u16, 640*480);
    u16 *frame2 = (u16 *)calloc(640*480, sizeof(u16));//NEWMEM(u16, 640*480);
    printf("frame new mem 2!\n");

    file1.read((char *)frame1, size1);
    file2.read((char *)frame2, size2);
    printf("frame read!\n");


    u16 *slam1 = calibration::make_slam_buffer();
    printf("slam 1!\n");
    u16 *slam2 = calibration::make_slam_buffer();
    printf("slam 2!\n");


    calibration::vector_slam(frame1, slam1, 0);
    printf("vect 1!\n");

    calibration::vector_slam(frame2, slam2, 0);
    printf("vect 2!\n");

    s16 rotation = calibration::estimate_rotation(slam1, slam2);

    printf("rotation estimation: [%d]\n", rotation);


}




