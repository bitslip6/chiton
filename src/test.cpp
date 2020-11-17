#include "util.h"
#include "utils.hpp"
#include "demos.h"
#include "image.h"
#include "system.h"
#include "utils.hpp"
#include "system.hpp"
#include <math.h>
#include <sstream>
#include <stdio.h>
#include <map>
#include <vector>
#include <string>

#include <arm_acle.h>
#include "system.h"
#include "demos.h"
#include "image.h"
#include "shapes.h"

#include "fplus/fplus.hpp"



#ifdef __ARM_FEATURE_QBIT
#error "arm fp"
#endif

RGB8 *hsv_cache;
GlobalOpts *GOPTS;
DisplayBuffers *GDISP;
std::shared_ptr<aditof::Camera> camera = NULL;

void test_find_laser(u16 x1, u16 y1) {

    printf("test find laser begin %d/%d!\n", x1, y1);
    GOPTS->center_x = 320;
    GOPTS->center_y = 240;
    //u16 x1 = 10;
    //u16 y1 = 10;
    u8 quad = find_laser(x1, y1);
    printf("quadrant: [%d]\n", quad);
}

// should be able to call init options multiple times
void test_options() {
    printf("init options 1\n");
    utils::init_options();
    printf("init options 2\n");
    utils::init_options();
}

void test_i2c_read() {
    
    std::stringstream ss;
    utils::i2c_write_afe_register(0xc002, 04);
    ss << std::hex << utils::i2c_read_afe_register(0xc002);

    std::cout << " read i2c string: " << std::endl;
    std::cout << std::string(ss.str()) << std::endl;
}

void test_map_to_json() {
    std::map<std::string, std::string> test_map; 
    test_map["key1"] = "value1";

    std::cout << utils::map_to_json_string(test_map);
}

void test_slice() {
    std::vector<std::string> tvector;
    tvector.emplace_back(std::string("100"));
    tvector.emplace_back(std::string("200"));
    tvector.emplace_back(std::string("300"));
    tvector.emplace_back(std::string("400"));
    std::cout << fplus::show(tvector) << std::endl;

    std::vector<std::string> ans1 = utils::slice(tvector, 0, 1);
    std::cout << fplus::show(ans1) << std::endl;
    std::vector<std::string> ans2 = utils::slice(tvector, 2, 3);
    std::cout << fplus::show(ans2) << std::endl;
}

void test_logging() {
    utils::log_fn(1, __FUNCTION__, __FILE__, __LINE__, "direct %d", 9997);
    INFO("test message [%d]", 1234);

    
}

void test_circles() {

    std::vector<xyf_t> points = gfx::circle_points_float(320, 240, 200, 50);

    for (auto p : points) { 
        std::cout << " " << p.x  << "." << p.y;
    }

    std::cout << std::endl;
}


std::vector<xyf_samp_t> make_all_points(int planar_width, int line_len) {

    std::vector<xyf_t> inner = gfx::circle_points_float(314, 174, planar_width, 130);
    std::vector<xyf_t> outer = gfx::circle_points_float(314, 174, planar_width, 295);

    std::vector<xyf_samp_t> all_points;
    int len = inner.size();

    all_points.reserve(line_len * len);

    for (int i=0; i<len; i++) {
        xyf_t p1 = inner.at(i);
        xyf_t p2 = outer.at(i);
        linef_t l1 = {p1, p2};
        
        std::vector<xyf_samp_t> points = gfx::line_to_vector_float(l1, line_len);
        all_points.insert(all_points.end(), points.begin(), points.end());
    }

    printf("strips [%d] reserved: [%d]\n", len, (line_len * len));
    printf("num points [%lu]\n", all_points.size());

    return all_points;
}

u32 calc_off(u16 x, u16 y, u16 width)  {
    int off1 = (y * width);
    //printf("calc %dx%d = [%d]\n",x, y, (off1 + x) );
    return off1 + x;
}

uint16_t map_point(uint16_t *src, u16 width, xy_samp_t *p) {

    
    u32 off1 = (u32)(p->y1 * width);
    u32 off2 = (u32)(p->y2 * width);

    //printf("Y: %d, Y: %d, X: %d, X: %d (%f.%f)\n", off1, off2, p->x1, p->x2, p->xf, p->yf);

/*
    uint16_t v1 = src[calc_off(p->x1, p->y1, width)];
    uint16_t v2 = src[calc_off(p->x2, p->y1, width)];
    uint16_t v3 = src[calc_off(p->x1, p->y2, width)];
    uint16_t v4 = src[calc_off(p->x2, p->y2, width)];
    */
    float v1 = src[off1+(u16)(p->x1)];
    float v2 = src[off1+(u16)(p->x2)];
    float v3 = src[off2+(u16)(p->x1)];
    float v4 = src[off2+(u16)(p->x2)];

// 50+100 (75) +150+ 200 (175)
/*
    printf("%d %d %d %d\n", v1, v2, v3, v4);
    v1 *= (1-p->xf);
    printf("%d %d %d %d\n", v1, v2, v3, v4);
    v2 *= (p->xf);
    printf("%d %d %d %d\n", v1, v2, v3, v4);
    v1 *= (1-p->yf);
    printf("%d %d %d %d\n", v1, v2, v3, v4);
    v2 *= (1-p->yf);
    printf("%d %d %d %d\n", v1, v2, v3, v4);
    uint16_t va = v1 + v2; 
    printf("va=%d\n", va);

    v3 *= (1-p->xf);
    printf("%d %d %d %d\n", v1, v2, v3, v4);
    v4 *= (p->xf);
    printf("%d %d %d %d\n", v1, v2, v3, v4);
    v3 *= (p->yf);
    printf("%d %d %d %d\n", v1, v2, v3, v4);
    v4 *= (p->yf);
    printf("%d %d %d %d\n", v1, v2, v3, v4);
    uint16_t vb = v3 + v4; 
    printf("vb=%d\n", vb);
    printf("vx=%d\n", va+vb);
    */

    //return va+vb;


    float xa = (v1 * (1-p->xf) * (1-p->yf)) + (v2 * (p->xf * (1-p->yf)));
    float xb = (v3 * (1-p->xf) * p->yf) + (v4 * (p->xf * p->yf));
    //uint16_t xt = (v3 * (1-p->xf) * p->yf);
    //uint16_t xz = (v4 * (p->xf) * p->yf);

    //printf("xa %f xb %f xt %d xz %d\n", xa, xb, xt, xz);

    return CLAMP((xa + xb), 0, 6000);
    //return v1;
}

uint16_t map_point2(uint16_t *src, u16 width, xyf_samp_t *p) {
    
    u32 off1 = (u32)(p->y1 * width);
    u32 off2 = (u32)(p->y2 * width);
    
    //u32 off2 = off1 - width;
    uint16_t v1 = src[off1+(u16)(p->x1)];
    uint16_t v2 = v1+1;//src[off1+(u16)(p->x2)];
    uint16_t v3 = src[off2+(u16)(p->x1)];
    uint16_t v4 = v3+1;//src[off2+(u16)(p->x2)];

    if () {
    }


    //float xa = (v1 * (1-p->xf) * (1-p->yf)) + (v2 * (p->xf * (1-p->yf)));
    //float xb = (v3 * (1-p->xf) * p->yf) + (v4 * (p->xf * p->yf));

    float xa = (v1 * (1-p->xf) * (p->yf)) + (v2 * (p->xf * (p->yf)));
    float xb = (v3 * (1-p->xf) * (1-p->yf)) + (v4 * (p->xf * (1-p->yf)));

    //float xa = (v1 * (p->xf) * (1-p->yf)) + (v2 * ((1-p->xf) * (1-p->yf)));
    //float xb = (v3 * (p->xf) * (p->yf)) + (v4 * ((1-p->xf) * (p->yf)));

    return CLAMP((xa + xb), 0, 6000);
}




void map_points(std::vector<xy_samp_t> points, uint16_t *src, uint16_t *dst, uint16_t width, uint16_t height) {

    u32 offset = 0;
    u32 idx1 = 0;
    u32 idx2 = 0;

/*
    uint16_t previous = 0;
    bool inobj = false;
    int inhur = 0;
    const long sz = points.size();
    */

    for(auto p : points) {
        dst[offset] = (u16)map_point(src, 640, &p);

        /*
        // todo: merge with neighboor object detect if <3 pixels && <150 dist
        if (!inobj && inhur <= 0 && previous - dst[offset] > 200) {
            //printf("in [%dx%d]\n", idx2, idx1);
            //dst[offset] = 0;
            inhur = 3;
            inobj = true;
        } else if (inobj && inhur <= 0 && previous - dst[offset] < -200) {
            //printf("out [%dx%d]\n", idx2, idx1);
            inhur = 3; 
            inobj = false;
            //dst[offset] = 0;
        }
        
        previous = previous * 0.7f + dst[offset] * 0.3f;
        */
        
        // advance 1 pix down
        offset = (offset + width);// % sz;
        // end of vert line

        if (++idx1 >= height) {
            idx1 = 0;
            offset = ++idx2;
            //inobj = false;
            //inhur = 0;
            //previous = 0;
        }
    }
    //printf ("w/h [%d/%d] idx1 %d, idx2 %d\n", width, height, idx1, idx2);
}

void map_points2(std::vector<xyf_samp_t> points, uint16_t *src, uint16_t *dst, uint16_t width, uint16_t height) {

    u32 offset = 0;
    u32 idx1 = 0;
    u32 idx2 = 0;

    for(auto p : points) {
        dst[offset] = (u16)map_point2(src, 640, &p);
        
        // advance 1 pixel down
        offset = (offset + width);// % sz;

        // end of vert line
        if (++idx1 >= height) {
            idx1 = 0;
            offset = ++idx2;
        }
    }
}


void test_sample() {
    uint16_t *tst1 = (uint16_t*)NEWMEM(uint16_t, 32);
    tst1[1] = 100;
    tst1[2] = 200;
    tst1[6] = 300;
    tst1[7] = 400;

    xyf_samp_t s = {1, 0, 2, 1, 0.5f, 0.5f};

    int r = map_point2(tst1, 5, &s);
    printf("result[%d]\n", r);
}

void test_line() {
    int planar_width = 2700;
    int height = 165;
    int len;

    uint16_t *dst = (uint16_t*)NEWMEM(uint16_t, planar_width*(height+2));
    uint16_t *src = get_data_u16("foo.raw", &len);

    std::vector<xyf_samp_t> points = make_all_points(planar_width, height);
    int strips = (int)points.size() / height; 
    TIMER("planar", map_points2(points, src, dst, strips, height));

    put_data_u16("planar10.raw", (char *)dst, points.size() * sizeof(dst));
    printf("t2 points [%lu] X rez:[%d] Y rez:[%d]\n", points.size(), strips, height);
    //printf("read %d bytes\n", len);
}

void test_crap() {
    int planar_width = 640;
    std::vector<xy_t> inner = gfx::circle_points(314, 174, planar_width, 130);
    std::vector<xy_t> outer = gfx::circle_points(314, 174, planar_width, 295);

    for (int i=0; i<planar_width; i++) {
        xy_t p1 = inner.at(i);
        xy_t p2 = outer.at(i);
        line_t l1 = {p1, p2};

        std::vector<xy_samp_t> points = gfx::line_to_vector(l1, 165);
        /*
        int idx = 2048;
        for (auto p : points) {
            dst[(p.y*planar_width)+p.x] = idx++;
        }

        dst[(p1.y * planar_width) +p1.x] = 4000;
        dst[(p2.y * planar_width) +p2.x] = 4000;
        */

        //int l = gfx::aa_line(src, &dst[i], l1, planar_width);
        //printf("finish i [%d] len[%d]\n", i, points.size());
    }
    
    //put_data_u16("planar4.raw", (char *)dst, planar_width*480*sizeof(dst));

    //printf("write %ld bytes\n", planar_width*480*sizeof(dst));
}


void test_math() {
    uint16_t offset = 900;
    uint16_t sample = 660;
    int result = 660 - offset;
    printf("%d - 900 = %d\n", sample, result);

    sample = 400;
    offset = 1;
    result = 100;
    int input_i = 400;
    int input_j = 200;

    uint16_t result16 = 0;
    uint16_t input_i16 = 300;
    uint16_t input_j16 = 500;

    printf("ADD %d + %d = %d\n", input_i, input_j, result);
    __asm ("ADD %[result], %[input_i], %[input_j]" : [result] "=r" (result) : [input_i] "r" (input_i), [input_j] "r" (input_j));
    printf("ADD %d + %d = %d\n", input_i, input_j, result);

    // __asm ("__QSUB16 %[result], %[input_i], %[input_j]" : [result] "=r" (result16) : [input_i] "r" (input_i16), [input_j] "r" (input_j16));
    //result16 = __ror(input_i16, 2);
    printf("SUB CC %d + %d = %d\n", input_i16, input_j16, result16);
    
    printf(" sub2 %d - 900 = %d\n", sample, result);
    //__asm ("ADD %0, %1, %2\r\t" : "=r" (result) : "r" (&sample), "r" (&offset));
    //__asm ("dec %0\r\t" : "=r" (result));

    printf(" sub2 %d - 900 = %d\n", sample, result);
}



int main(UNUSED int arc, UNUSED char **argv) {

printf("main 1\n");
    sys::SystemInit(640, 480);
printf("main 2\n");
    utils::init_options();
printf("main 3\n");

GOPTS->default_log_level = 1;
TRACE("foobar");
DEBUG("foobar");
INFO("foobar");
WARN("foobar");
PANIC("foobar");

    //test_map_to_json();


    test_line();
    //test_slice();
    //test_logging();
    //test_find_laser(300, 250);

    //test_options();
    //test_i2c_read();
}




