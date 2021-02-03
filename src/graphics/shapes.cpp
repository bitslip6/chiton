#include <vector>
#include <algorithm>
#include <map>
#include <functional>

#include "demos.h"
#include "core.h"
#include "system.h"
#include "shapes.h"
#include "utils.hpp"
#include "data.h"
#include "color/hsv.h"
// #include "lib.h"

#include "fplus/fplus.hpp"
#include <arm_neon.h>



namespace gfx
{
    

struct GainPair {
    int idx;
    float radian;
    float radian_diff;
    GainPair(int i,float r,float d) : idx(i), radian(r), radian_diff(d) { }
    
    bool operator < (const GainPair& test) const { 
        auto r = std::abs(radian_diff) < std::abs(test.radian_diff);
        return r;
    }
    bool operator > (const GainPair& test) const { 
        return std::abs(radian_diff) > std::abs(test.radian_diff);
    }

    operator std::string() const { return std::string("Index: " + std::to_string(idx) + " rad: " + std::to_string(radian) + " diff: " + std::to_string(radian_diff)); }
};

//float get_diff(const GainPair& gp) { return gp.radian_diff; };

template <typename T>
void print_result(const T& x) {
    std::cout << fplus::show(x) << std::endl;
}

/**
 * calculate a single point on a circle at radius and radians
 *
 */
xy_t calculate_circle_point(uint16_t radius, float radians) {
    xy_t p = {(s16)round(radius * cos(radians)), (s16)round(radius * sin(radians))};
    return p;
}

/**
 * calculate a single point on a circle at radius and radians
 *
 */
xyf_t calculate_circle_point_float(uint16_t radius, float radians) {
    xyf_t p = {radius * cosf(radians), radius * sinf(radians)};
    return p;
}

/**
 * return degree in radians from -PI to +PI of pixel from center of image
 * 
 * PURE
 */
float pixel_to_radians(xy_t center, xy_t pixel) {
    return atan2f((float)pixel.x-center.x, (float)pixel.y-center.y);
}

/**
 * return a function that will accept a single point and will add offsetx and offsety
 */
std::function<xy_t(xy_t)> fwd_move_point(s16 offset_x, s16 offset_y) {
    return [offset_x, offset_y](xy_t point) {
        xy_t p = {(s16)(point.x + offset_x), (s16)(point.y + offset_y)};
        return p;
    };
}

/**
 * return a function that will accept a single point and will add offsetx and offsety
 */
std::function<xyf_t(xyf_t)> fwd_move_point_float(s16 offset_x, s16 offset_y) {
    return [offset_x, offset_y](xyf_t point) {
        xyf_t p = {(float)(point.x + offset_x), (float)(point.y + offset_y)};
        return p;
    };
}



/**
 * returns a new vector of points along a circle of radius
 */
std::vector<xy_t> circle_points(s16 center_x, s16 center_y, u16 numpoints, u16 radius) {
   
    std::vector<xy_t> points;

    // offset to center the view for the hidden area
    // TODO: move this to config.json
    float offset = -0.9f;
    // offset1 and offset2 move the sample point in an x/y direction
    // this is used to correct for the zig zag distortion
        auto offset1 = fwd_move_point(0, 0);
        auto offset2 = fwd_move_point(0, 0);
        int flip = 4; // flip offset direcion every point
        for (float angle = 0.0f+offset; angle < PI2+offset; angle += PI2 / (numpoints-1)) {
            if (angle > 3.8f && angle < 5.4f) { continue; }
            if (flip++ > 5) {
                flip = 0;
            }
            int off = 0;//(flip * 0.25f);
            auto circle_point = calculate_circle_point(radius + off, angle);
            auto offset_point = (flip) ? offset1(circle_point) : offset2(circle_point);
            points.emplace_back(offset_point);
            flip = !flip;
        }
       
        // move each point to the center position
        return fplus::transform(fwd_move_point(center_x, center_y), points);
    }

    /**
     * returns a new vector of points along a circle of radius
     */
    std::vector<xyf_t> circle_points_float(s16 center_x, s16 center_y, u16 numpoints, u16 radius) {
       
        std::vector<xyf_t> points;

        float offset = -0.9f;
        for (float angle = 0.0f+offset; angle < PI2+offset-0.5f; angle += PI2 / numpoints) {
            if (angle > 3.8f && angle < 5.2f) { continue; }
            points.emplace_back(calculate_circle_point_float(radius, angle));
        }

        return fplus::transform(fwd_move_point_float(center_x, center_y), points);
    }

    VixelCalibration* merge_vixel_calibration(VixelCalibration *v0, VixelCalibration *v1, float angle) {

        float v0dist = std::abs(angle - v0->radian);
        float v1dist = std::abs(angle - v1->radian);
        //float total = v0dist + v1dist;
        //float v0per = v0dist / total;
        //float v1per = v1dist / total;
        
        float value = ((v0->gain*(v1->radian - angle))+(v1->gain*(angle - v0->radian))) / (v1->radian - v0->radian);
        float value2 = ((v0->offset*(v1->radian - angle))+(v1->offset*(angle - v0->radian))) / (v1->radian - v0->radian);
        
        

        
        VixelCalibration *v = new VixelCalibration();
        v->vixel = v0->vixel;
        v->radian = angle;//v0->radian;
        v->gain = value;//v0->gain;//value; //(v0->gain * v1per) + (v1->gain * v0per) ;
        v->offset = (s16)std::roundf(value2);//v->offset;//(s16)((v0->offset * v1per) + (v1->offset * v0per));
        //v->radian_max = v0->radian_max;
        //v->radian_min = v0->radian_min;

        //printf("[%f][%f] = {%f} [%f] (%f) = [%f]\n", v0per, v1per, total, (v0per + v1per), angle, v->gain);
        return v;
    }

    /**
     * find the vixel calibration for a given angle in radians
     */
    VixelCalibration* find_calibration_for_radian(float angle) {
        
        // iterate over all calibration entries
        //for (auto& [vixel_num, calibration] : *GOPTS->cal) {
        //for (const auto &cal : *GOPTS->cal) {

        if (angle < -2.005f) {
            return GOPTS->cal->find(8)->second;
            /*
            VixelCalibration *v = new VixelCalibration();
            v->gain = vm.gain;
            v->offset = vm.offset;
            // printf("begin %f = calibration idx [%d]", angle, vm.vixel);
            return v;
            */
        }
        if (angle > 2.005f) {
            return GOPTS->cal->find(1)->second;
            /*
            VixelCalibration *v = new VixelCalibration();
            v->gain = vm.gain;
            v->offset = vm.offset;
            // printf("end %f = calibration idx [%d]", angle, vm.vixel);
            return v;
            */
        }
        std::vector<GainPair*> gains;
        for (auto const&[key, val] : *GOPTS->cal) {
            //printf("new gain key: %d rad:(%f), angle:(%f) diff:(%f)\n", key, val->radian, angle, (angle - val->radian));
            gains.emplace_back(new GainPair(key, val->radian, angle - val->radian));
        }
        //fplus::show(gains);
        std::sort( gains.begin( ), gains.end( ), [angle]( const auto& lhs, const auto& rhs ) { 
            //printf("%f (%f) < %f\n", angle, lhs->radian_diff, rhs->radian_diff);
            return std::abs(lhs->radian_diff) < std::abs(rhs->radian_diff);
         });
        //printf("gains (%f) (%f)\n", gains.at(0)->radian_diff, gains.at(1)->radian_diff);
        
        VixelCalibration closest, next_close;
        /*
        float min_dist = 9999;
        int closest_idx, next_idx;
        VixelCalibration closest, next_close;
        for (uint i=1; i<=GOPTS->cal->size(); i++) {
            VixelCalibration v0 = *GOPTS->cal->find(i)->second;
            float test_dist = std::abs(v0.radian - angle);
            //printf("%d, %d TEST: %f - %f = %f\n", i, v0.vixel, v0.radian, angle, test_dist);
            if (test_dist < min_dist) {
                min_dist = test_dist;
                closest_idx = i;
            }
        }

        next_idx = std::clamp(closest_idx + ((angle < closest.radian) ? -1 : 1), 1, 8);

        if (std::abs(next_idx - closest_idx) > 1) {
            printf("%f: 1:%d, 2:%d\n", angle, closest_idx, next_idx);
        }
        */
        closest = *GOPTS->cal->find(gains.at(0)->idx)->second;
        next_close = *GOPTS->cal->find(gains.at(1)->idx)->second;
        
        //PANIC_IF((std::abs(gains.at(0)->idx - gains.at(1)->idx) > 1), "not using 2 measurements next to each other!", -9);

        auto f = merge_vixel_calibration(&closest, &next_close, angle);
        // printf("(%f) 1: %d 2: %d, gain [%f]\n", angle, gains.at(0)->idx, gains.at(1)->idx, f->gain);
        return f;
                   
        //PANIC("unable to find vixel calibration for angle: [%f]", angle);
        //VixelCalibration *v = new VixelCalibration();
        //return v;
    }

    



    // returns absolute value of number
    static inline float absolute(float x ) {
        if (x < 0) return -x;
        else return x;
    }

    //returns integer part of a floating point number
    static inline int iPartOfNumber(float x) {
        return (int)floorf(x);
    }
     
    //rounds off a number
    static inline int roundNumber(float x) {
        return iPartOfNumber(x + 0.5f) ;
    }
     
    //returns fractional part of a number
    static inline float fPartOfNumber(float x) {
        if (x>0) return x - iPartOfNumber(x);
        else return x - (iPartOfNumber(x)+1);
    }
   
    //returns fractional part of a number
    static inline float rfPartOfNumber(float x) {
        return 1 - fPartOfNumber(x);
    }

    /**
     * returns true if line is > vertical and < horizontal
     */
    bool is_steep(line_t line) {
        return std::abs(line.p2.y - line.p1.y) > std::abs(line.p2.x - line.p1.x) ;
    }



    /**
     * draw a line, no multiply
     **/
    /*
    void sample_line(uint16_t *src, uint16_t *dst, line_t line) { //int x0, int y0, int x1, int y1, u8 hue, u8 sat) {

        bool steep = false;
        if (absolute(line.p1.x-line.p2.x) < absolute(line.p1.y-line.p2.y)) {
            std::swap(line.p1.x, line.p1.y);
            std::swap(line.p2.x, line.p2.y);
            steep = true;
        }
        if (line.p1.x > line.p2.x) {
            std::swap(line.p1.x, line.p2.x);
            std::swap(line.p1.y, line.p2.y);
        }
        int dx = line.p2.x-line.p1.x;
        int dy = line.p2.y-line.p1.y;
        int dx2 = dx * 2;
        int derror2 = std::abs(dy)*2;
        int error2 = 0;
        int y = line.p1.y;

        for (int x=line.p1.x; x <= line.p2.x; x++) {
            if (steep) {
                //db_t color = HsvToRgb(hue, sat, 250);//color;
                // printf("fb line X.%d, Y.%d?\n", x, y);
                //disp->buffer1[disp->width*x + y] = color;
            } else {
                db_t color =  HsvToRgb(hue, sat, 250);//color;
                // printf("fb line X.%d, Y.%d?\n", x, y);
                disp->buffer1[disp->width*y + x] = color;
            }
            error2 += derror2;
            if (error2 > dx) {
                y += (y1>y0?1:-1);
                error2 -= dx2;
            }
        }
    }
    */

    // take an x/y floating point and convert to 4 sample points with fractional components
    xy_samp_t point_to_sample(float x1, float y1, s16 dx, s16 dy) {
        s16 x2, x3, y2, y3;
        float xf = (fPartOfNumber(x1));
        float yf = (fPartOfNumber(y1));

        //printf("DX %d DY %d: %f=%f, %f=%f\n", dx, dy, x1, xf, y1, yf);

        // find previous y pixel depending on direction
        y2 = CLAMP480(iPartOfNumber(y1));
        y3 = CLAMP480((dy > 0) ? y2 +1 : y2 -1);

        // find previous x pixel depending on direction
        x2 = CLAMP640(iPartOfNumber(x1));
        x3 = CLAMP640((dx > 0) ? x2 +1 : x2 -1);
       
        xy_samp_t p = {
            xy_t{x2, y2}, xy_t{x3, y3},
            xf, yf, 
            pixel_to_radians(xy_t{(s16)GOPTS->center_x, (s16)GOPTS->center_y}, xy_t{x2, y2}),
            0
        };

        //printf("sample, dx/dy %d/%d, x2/3 %d,%d\n", dx, dy, x2, x3);
        return p;
    }

    offset_samp_t point_to_sample2(float x1, float y1, uint src_width, uint dst_offset, uint slice) {
        s16 x, y;
        //float xf = (fPartOfNumber(x1)) - 0.5f;
        //float yf = (fPartOfNumber(y1)) - 0.5f;
        float xf = fPartOfNumber(x1);
        float yf = fPartOfNumber(y1);

        // find previous y pixel depending on direction
        //y = CLAMP480(iPartOfNumber(y1));
        y = iPartOfNumber(y1);
        if (y < 0) { printf("%f.%f\n", x1, y1); PANIC("less than 0"); }
        //y = (yf < 0.5f) ? y-1 : y;

        // find previous x pixel depending on direction
        x = CLAMP640(iPartOfNumber(x1));
        //x = (xf < 0.5f) ? x-1 : x;

        xf = (xf >= 0.5f) ? xf - 0.5f : 0.5f + xf;
        yf = (yf >= 0.5f) ? yf - 0.5f : 0.5f + yf;

        uint src_offset = (y*src_width)+x;
        float angle = pixel_to_radians(xy_t{(s16)GOPTS->center_x, (s16)GOPTS->center_y}, xy_t{x, y});
        u8 dist = std::clamp(core::calc_distance(GOPTS->center_x, GOPTS->center_y, x, y), 0, 250);
        //printf("x: %d, y: %d  - (%f)  ", x, y, angle);
        VixelCalibration *calibration = find_calibration_for_radian(angle);

       
        offset_samp_t p = {
            src_offset,
            dst_offset,
            xf, yf,
            angle,
            calibration->gain,
            calibration->offset,
            dist,
            (u16) slice
        };

        return p;
    }


    // take an x/y floating point and convert to 4 sample points with fractional components
    xyf_samp_t point_to_sample_float(float x1, float y1, float dx, float dy) {
        float x2, x3, y2, y3;
        float xf = (fPartOfNumber(x1));
        float yf = (fPartOfNumber(y1));

        //printf("%f=%f, %f=%f\n", x1, xf, y1, yf);

        // find previous y pixel depending on direction
        y2 = CLAMP480(iPartOfNumber(y1));
        y3 = CLAMP480((dy > 0) ? y2+1 : y2-1);

        // find previous x pixel depending on direction
        x2 = CLAMP640(iPartOfNumber(x1));
        x3 = CLAMP640((dx > 0) ? x2+1 : x2-1);
       
        //xyf_samp_t p = {x2, y2, x3, y3, xf, yf};
        xyf_samp_t p = {x1, y1, x3, y3, xf, yf};
        return p;
    }


    /**
     * draw a floating point line using naive implementation
     */
    std::vector<xyf_samp_t> line_to_vector_float(linef_t line, int segments) {
        line.p1.x = CLAMP640(line.p1.x);
        line.p2.x = CLAMP640(line.p2.x);
        line.p1.y = CLAMP480(line.p1.y);
        line.p2.y = CLAMP480(line.p2.y);
       
        //compute the slope
        float dx = line.p2.x - line.p1.x;
        float dy = line.p2.y - line.p1.y;

        float gradient_x = (dx / segments);
        float gradient_y = (dy / segments);

        // printf("gradient x/y [%f/%f] X:%f, total:%f\n", gradient_x, gradient_y, dx+dy, gradient_x+gradient_y);

        std::vector<xyf_samp_t> points;
        points.reserve(segments);

        float x1 = line.p1.x;
        float y1 = line.p1.y;
        
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdouble-promotion"
        printf("gradient %f - %f = %f . %f\n", line.p2.x, line.p1.x, gradient_x, gradient_y);

        for (int i=1; i<segments; i++) {
            points.emplace_back(point_to_sample_float(x1, y1, dx, dy));
            x1 += gradient_x;
            y1 += gradient_y;
        }

        points.emplace_back(point_to_sample_float(x1, y1, dx, dy));

        //xyf_samp_t s1 = points.at(1);
        //xyf_samp_t s2 = points.at(50);
        //printf("line length: [%f] [%f]\n", core::calc_distance_float(s1.x1, s1.y1, s2.x1, s2.y2), core::calc_distance_float(line.p1.x, line.p1.y, line.p2.x, line.p2.y));
        #pragma GCC diagnostic pop

        return points;
    }




    /**
     * draw a naive floating point line and return vector with segment points in it
     */
    std::vector<offset_samp_t> line_to_vector(line_t line, int segments, uint src_width, uint dst_width, uint segment_number) {
        line.p1.x = CLAMP640(line.p1.x);
        line.p2.x = CLAMP640(line.p2.x);
        line.p1.y = CLAMP480(line.p1.y);
        line.p2.y = CLAMP480(line.p2.y);

        // PANIC("line: [%d.%d] [%d.%d]", line.p1.x, line.p1.y, line.p2.x, line.p2.y);

        // bind first parameter (segments to the step function)
        //auto segment_step = std::bind(core::step<int16_t>, segments, std::placeholders::_1, std::placeholders::_2);

        // compute the slope for each line
        float gradient_x = core::step(segments, line.p1.x, line.p2.x);
        float gradient_y = core::step(segments, line.p1.y, line.p2.y);

        std::vector<offset_samp_t> points;
        points.reserve(segments);

        float x1 = line.p1.x;
        float y1 = line.p1.y;
        uint doffset = dst_width - segment_number;
        
        // TODO: WHY IS point to sample not adding fractional points ??????
        for (int i=1; i<=segments-1; i++) {
            points.emplace_back(point_to_sample2(x1, y1, src_width, doffset, i));
            x1 += gradient_x;
            y1 += gradient_y;

            // advance 1 pix down
            doffset += dst_width;
        }
        points.emplace_back(point_to_sample2(line.p2.x, line.p2.y, src_width, doffset, segments));//, line.p2.x, line.p2.y));

        return points;
    }

    /**
     * return the value of a sample at a sample location
     */
    static inline uint16_t sample_point(uint16_t *src, u16 width, xy_samp_t *p) {

        //return src[(p->p1.y*width)+p->p1.x] * 1.46f;

        u32 off1 = (u32)(p->p1.y * width);
        u32 off2 = (u32)(p->p2.y * width);
        
        float v1 = src[off1+(u16)(p->p1.x)];
        float v2 = src[off1+(u16)(p->p2.x)];
        float v3 = src[off2+(u16)(p->p1.x)];
        float v4 = src[off2+(u16)(p->p2.x)];

    
        // upper left + upper right
        // lower left + lower right
        float xa = (v1 * (1-p->xf) * (1-p->yf)) + (v2 * (p->xf * (1-p->yf)));
        float xb = (v3 * (1-p->xf) * p->yf) + (v4 * (p->xf * p->yf));

        #ifdef MAX_RANGE
            return CLAMP((xa + xb), 0, MAX_RANGE);
        #elif MAX_VALUE
            return MAX((xa + xb), MAX_VALUE);
        #else
            return (xa + xb);
        #endif
    }

    bool map_points_greyscale_old(const std::vector<offset_samp_t> &points, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data, std::map<float, uint16_t> &dist_map) {

        std::lock_guard<std::mutex> guard(GOPTS->point_lock);
        //std::map<u16, u16> dist_map;
        
        // u16 segment = 0;
        u16 vert_count = 0;
        u16 dist = 9999;
        const int height = data::get_frame_height();


        // optimization:
        // we only need to calculate the gain/offset for each "slice"
        // so we should iterate over each slice and then each column in the slice
        // this way we can still vectorize the gain and offset calculation...
        for(auto p : points) {
            
            /*
            int xoff = (p.xf > 0.5f) ? -1 : 1;
            int yoff = (p.yf > 0.5f) ? -640 : 640;

            // DEBUG 
            int value1= src[p.src_off] * (p.xf);
            int value2= src[p.src_off] * (p.yf);

            int value3= src[p.src_off+xoff] * (1-p.xf);
            int value4= src[p.src_off+(yoff)] * (1-p.yf);

            int tvalue = (value1 + value2 + value3 + value4) / 2;
            dst[p.dst_off] = (tvalue * p.gain ) + p.gain_offset;
            */


            s16 t = static_cast<s16>(src[p.src_off] * p.gain);
            dst[p.dst_off] = vqaddh_s16(t, p.gain_offset);
            if (p.gain_offset < 0 && t < 100) {
                printf("off: %d, t1: %d, t2: %d\n", p.gain_offset, t, dst[p.dst_off]);
            }
            //vadd_s16(dst[p.dst_off], p.gain_offset);

            // DEBUG
            // dst[p.dst_off] = src[p.src_off];

            // map greyscale jpeg image data
            uint rgb_offset = p.dst_off * 3; 
            uint8_t v = 255-(dst[p.dst_off] / 26); // trim 6000 down to <255
            
            //uint8_t v = int8_t((p.gain-1.4f) * 1200.0f);

            jpeg_data[rgb_offset] = v;
            jpeg_data[rgb_offset+1] = v;
            jpeg_data[rgb_offset+2] = v;
            
            dist = std::min(dist, dst[p.dst_off]);
            
            // find closest pixel in each "strip"
            if (vert_count++ > height) {
                //printf("angle: %f\n", p.angle);
                dist_map[(float)p.angle] = (uint16_t)dist;
                vert_count = 0;
                dist = 9999;
            }
        }
        

        /*
        int height_2 = 240;//data::get_frame_height() / 2;
        for (int i=0; i<data::get_frame_width(); i++) {
            u16 dist = dist_map[i];
            float angle = angle_map[i];
            
            xy_t pos = calculate_circle_point(dist/20, angle);
            //printf("segment [%d] dist: [%d] [%d.%d]\n", i, dist, pos.x, pos.y);
        }
        
        printf("\n\n");
        */
        
        return true;
    }

    float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
        return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }


    /**
     * todo: map points from vertical strips to horizontal strips
     */
    u16 map_points_greyscale(const std::vector<offset_quad_samp_t> &points, uint16_t *src, uint16_t *dst, UNUSED unsigned char *jpeg_data, UNUSED std::map<float, SliceTranche> &dist_map) {
        
        // optimization:
        // we only need to calculate the gain/offset for each "slice"
        // so we should iterate over each slice and then each column in the slice
        // this way we can still vectorize the gain and offset calculation...
        //for(auto p : points) {
        
        u16 *result = new u16[4];
        //float32x8_t gain = {1.46f, 1.46f, 1.46f, 1.46f, 1.46f, 1.46f, 1.46f, 1.46f};
        //float32x4_t gain = vmovq_n_f32(1.46f);
        //float32x4_t offset = vmovq_n_f32(-70.0f);

        u16 min_dist = 9999;
        float angle = 99.9f;
        unsigned int off = 0;

        for (auto sample : points) {
            
            /*
            const auto slice = dist_map[(float)sample.angle];
            if (tranche == dist_map.end()) {
                tranche = new SliceTranche(); 
            }
            dist_map[(float)sample.angle] = (uint16_t)dist;
            */

            u16 t = src[sample.src_off];
            if (t < 1000) {
                t /= 200;
                // slice.tranche[t]++;
                dist_map[(float)sample.angle].tranche[t]++;
            }
            
            
            /*

            uint16x4_t src_u16 = vld1_u16(src+sample.src_off);
            uint32x4_t src_u32 = vmovl_u16(src_u16);

            float32x4_t fl = vcvtq_f32_u32(src_u32);
            float32x4_t gained = vfmaq_f32(fl, sample.gain, sample.offset);
            uint32x4_t ul32 = vcvtq_u32_f32(gained);

            //vst1_u16(dst+i, vmovn_u32(ul32));
            vst1_u16(result, vmovn_u32(ul32));
            dst[sample.src_off] = result[0];
            dst[sample.src_off+1] = result[1];
            dst[sample.src_off+2] = result[2];
            dst[sample.src_off+3] = result[3];

            
            //if (sample.angle > -0.70f && sample.angle < 0.70f && result[0] > 100 && sample.center_dist < 277) {
            if (result < 1000 &&  sample.angle > 0.50f && sample.angle < 1.75f && result[0] > 100 && sample.center_dist < 277) {
                min_dist = std::min(min_dist, result[0]);
                if (min_dist != result[0]) {
                    off = sample.src_off;
                    angle = sample.angle;
                }
            } 
            */

            uint rgb_offset = sample.dst_off * 3; 
            
            /*
            if (sample.y > 470) {
                v = dist_map[(float)sample.angle].tranche[t] / ;
            }
            */

            uint8_t v = 255-(src[sample.src_off] / 16);
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;

            v = 255-(src[sample.src_off+1] / 16);
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;

            v = 255-(src[sample.src_off+2] / 16);
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;

            v = 255-(src[sample.src_off+3] / 16);
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;
            
            continue;

            /*
            uint8_t v = 255-(result[0] / 26);
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;

            v = 255-(result[1] / 26);
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;

            v = 255-(result[2] / 26);
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;

            v = 255-(result[3] / 26);
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;
            jpeg_data[rgb_offset++] = v;
            */
        }
        
        
        
        int y = off/640;
        int x = off-(y*640);
        
        // printf("min offset <%f> [%d]: (%d) %d.%d\n", angle, min_dist, off, x, y);

        return min_dist;
    }

    /**
     * todo: map points from vertical strips to horizontal strips
     */
    bool map_points_gain(const std::vector<offset_samp_t> &points, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data) {

        std::lock_guard<std::mutex> guard(GOPTS->point_lock);
        std::map<u16, u16> dist_map;
        
        // u16 segment = 0;
        // u16 vert_count = 0;
        u16 dist = 9999;
        // const int height = data::get_frame_height();

        // optimization:
        // we only need to calculate the gain/offset for each "slice"
        // so we should iterate over each slice and then each column in the slice
        // this way we can still vectorize the gain and offset calculation...
        for(auto p : points) {
            
            // int xoff = (p.xf > 0.5f) ? -1 : 1;
            // int yoff = (p.yf > 0.5f) ? -640 : 640;

            dst[p.dst_off] = src[p.src_off];

            // map greyscale jpeg image data
            uint rgb_offset = p.dst_off * 3; 
            
            uint8_t v = int8_t((p.gain-1.4f) * 1200.0f);
            jpeg_data[rgb_offset] = v;
            jpeg_data[rgb_offset+1] = v;
            jpeg_data[rgb_offset+2] = v;
            
            dist = std::min(dist, dst[p.dst_off]);
        }
        
        //printf("\n\n");
        
        return true;
    }
 
    

    /**
     * todo: map points from vertical strips to horizontal strips
     */
    bool map_points_slam(const std::vector<offset_samp_t> &points, uint16_t *src, UNUSED uint16_t *dst, unsigned char *jpeg_data) {

        std::lock_guard<std::mutex> guard(GOPTS->point_lock);
        std::map<u16, u16> dist_map;
        
        // u16 segment = 0;
        // u16 vert_count = 0;
        // u16 dist = 9999;
        const int height = data::get_frame_height();
        
        std::memset(jpeg_data, 0, 640*480*3);


        const int rows = points.size() / height;
        //offset_samp_t *p;
        uint idx=0;

        auto p = points[0];
        for (int y=0; y<rows; y++) {
            u16 closest = 16000;
            for (int x=0; x<height-20; x++) {
                p = points[idx++];
                closest = std::min(closest, src[p.src_off]);
            }
            float angle = p.angle * (float)(M_PI / 180); 

            int dx = closest * cos(angle);
            int dy = closest * sin(angle);

            idx += 20;
            printf("y: %d, idx: %d, %f, %d, %d, %d\n", y, idx, p.angle, closest, dx, dy);
            //dst[]
        }

        return true;
    }



    /**
     * todo: map points from vertical strips to horizontal strips
     */
    bool map_points_hsv(const std::vector<offset_samp_t> &points, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data) {

        std::lock_guard<std::mutex> guard(GOPTS->point_lock);
        //printf("map to mem at [%x]\n", dst);

        for(auto p : points) {
            
            dst[p.dst_off] = (src[p.src_off] * p.gain) + p.gain_offset;

            // map hsv jpeg image data
            uint rgb_offset = p.dst_off * 3; 
            depth_to_hsv_raw(dst + p.dst_off, jpeg_data + rgb_offset);
        }

        return true;
    }




    /**
     * create a series of points connecting all elements in points1 
     * with all elements of points2, each line_len elements long
     * 
     * returns a vector of xy_samp_t points1.size() * line_len long
     * 
     * TODO: deleteme 
     */
    std::vector<xy_samp_t> connect_vector_points(std::vector<xy_t> points1, std::vector<xy_t> points2, int line_len) {
        std::vector<xy_samp_t> all_points;
        Return_IfNot(points1.size() == points2.size(), "vector lists must be the same size", all_points);
        all_points.reserve(line_len * points1.size());
        

        /*
        int len=all_points.size();
        for (int i=0; i<len; i++) {
            line_t new_line = {points1.at(i), points2.at(i)};
            // OFFSET FIX REMOVE THIS: fplus::append(all_points, gfx::line_to_vector(new_line, line_len));
        }
        */

        return all_points;
    }


    

    std::vector<xy_samp_t> calibrate_map(std::vector<xy_samp_t> points) {
        std::vector<xy_samp_t> calibrated;
        calibrated.reserve(points.size());

        for (xy_samp_t point : points) {
            VixelCalibration *v = find_calibration_for_radian(point.angle);
            
            point.xf *= v->gain;
            point.yf *= v->gain;

            point.offset = v->offset;
            calibrated.emplace_back(point);
        }

        return calibrated;
    }

    std::vector<offset_samp_t> make_all_slam_points_int() {
        /*
        std::vector<offset_samp_t> all_points;
        all_points.reserve(FRAME_PLANAR_WIDTH_DEFAULT);
        
        std::vector<xy_t> circle = circle_points(GOPTS->center_x, GOPTS->center_y, FRAME_PLANAR_WIDTH_DEFAULT, 200);
        
        for (auto point : circle) {
            float angle = pixel_to_radians(xy_t{(s16)GOPTS->center_x, (s16)GOPTS->center_y}, point);
            

        }
        */
        TRACE("");
        const int width = data::get_frame_width();
        const int height = data::get_frame_height();

        const std::vector<xy_t> inner = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * 1.334, GOPTS->inner_dist);
        const std::vector<xy_t> outer = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * 1.334, GOPTS->outer_dist);

        DEBUG("inner/outer circle sz: [%d] [%d] vs [%dx%d]", inner.size(), outer.size(), width, height);

        std::vector<offset_samp_t> all_points;
        const int len = inner.size();
        all_points.reserve(height * len);

        for (int i=0; i<len; i++) {
            xy_t p1 = inner.at(i);
            xy_t p2 = outer.at(i);
            line_t l1 = {p1, p2};

            std::vector<offset_samp_t> p = gfx::line_to_vector(l1, height, CAM_WIDTH, len, i);
            all_points.insert(all_points.end(), p.begin(), p.end());
        }

        DEBUG("width [%d] height: [%d] reserved elements: [%d], computed points [%lu]", len, height, (height * len), (unsigned long)all_points.size());
        return all_points;
    }

    std::vector<offset_samp_t> make_all_raw_points_int() {
        std::vector<offset_samp_t> all_points;
        all_points.reserve(CAM_HEIGHT * CAM_WIDTH);

        float xf = 0.0f;
        float yf = 0.0f;

        for (int16_t y=0; y<CAM_HEIGHT; y++) {
            for (int16_t x=0; x<CAM_WIDTH; x++) {
                
                float angle = pixel_to_radians(xy_t{(s16)GOPTS->center_x, (s16)GOPTS->center_y}, xy_t{x, y});
                VixelCalibration *calibration = find_calibration_for_radian(angle);
                float gain = calibration->gain;
                s16 offset = (s16)calibration->offset;

                //u8 dist = std::clamp(core::calc_distance(GOPTS->center_x, GOPTS->center_y, x, y), 0, 250);
                u16 dist = core::calc_distance(GOPTS->center_x, GOPTS->center_y, x, y);
                if (dist > GOPTS->outer_dist || dist < GOPTS->inner_dist) {
                    gain = 0;
                    offset = 0;
                }
                if (angle < -2.5f || angle > 2.5f) {
                    gain = 0;
                    offset = 0;
                }
                offset_samp_t p = {
                    (uint)(y*CAM_WIDTH)+x,
                    (uint)(y*CAM_WIDTH)+x,
                    xf, yf,
                    angle,
                    gain,
                    offset,
                    dist,
                    0
                };

                all_points.emplace_back(p);
            }
        }

        return all_points;
    }
    

    /**
     * create a vector of offset sample points at:
     *  position: GOPTS->center_x, center_x, 
     *  between the radius
     *  radius: inner, outer diameter (from GOPTS)
     * 
     * PURE
     */
    std::vector<offset_samp_t> make_all_planar_points_int() {
        TRACE("");
        const int width = data::get_frame_width();
        const int height = data::get_frame_height();

        const std::vector<xy_t> inner = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * 1.334, GOPTS->inner_dist);
        const std::vector<xy_t> outer = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * 1.334, GOPTS->outer_dist);

        DEBUG("inner/outer circle sz: [%d] [%d] vs [%dx%d]", inner.size(), outer.size(), width, height);

        std::vector<offset_samp_t> all_points;
        const int len = inner.size();
        all_points.reserve(height * len);

        for (int i=0; i<len; i++) {
            xy_t p1 = inner.at(i);
            xy_t p2 = outer.at(i);
            line_t l1 = {p1, p2};

            std::vector<offset_samp_t> p = gfx::line_to_vector(l1, height, CAM_WIDTH, len, i);
            all_points.insert(all_points.end(), p.begin(), p.end());
        }

        DEBUG("width [%d] height: [%d] reserved elements: [%d], computed points [%lu]", len, height, (height * len), (unsigned long)all_points.size());
        return all_points;
    }


    std::vector<offset_quad_samp_t> make_all_quad_planar_points_int() {
        TRACE("quad planar points");
        const int width = data::get_frame_width();
        const int height = data::get_frame_height();

        const std::vector<xy_t> inner = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * 1.334, GOPTS->inner_dist);
        const std::vector<xy_t> outer = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, width * 1.334, GOPTS->outer_dist);

        DEBUG("inner/outer circle sz: [%d] [%d] vs [%dx%d]", inner.size(), outer.size(), width, height);

        std::vector<offset_quad_samp_t> all_points;
        const int len = inner.size();
        all_points.reserve(height * len);

        xy_t center = {(s16)GOPTS->center_x, (s16)GOPTS->center_y};

        float *gain_src   = new float[4];
        float *offset_src = new float[4];

        offset_quad_samp_t *sample = new offset_quad_samp_t[650*500];

        int i=0;
        for (u16 y=0; y<CAM_HEIGHT; y++) {
            for (u16 x=0; x<CAM_WIDTH; x+=4) {
            
                sample[i].src_off = (y*CAM_WIDTH)+x;
                sample[i].dst_off = sample[i].src_off;
                
                xy_t pixel = {x, y};
                float angle1 = pixel_to_radians(center, pixel);
                pixel.x++;
                float angle2 = pixel_to_radians(center, pixel);
                pixel.x++;
                float angle3 = pixel_to_radians(center, pixel);
                pixel.x++;
                float angle4 = pixel_to_radians(center, pixel);
                
                VixelCalibration *calibration1 = find_calibration_for_radian(angle1);
                VixelCalibration *calibration2 = find_calibration_for_radian(angle2);
                VixelCalibration *calibration3 = find_calibration_for_radian(angle3);
                VixelCalibration *calibration4 = find_calibration_for_radian(angle4);
                
                gain_src[0] = calibration1->gain;
                gain_src[1] = calibration2->gain;
                gain_src[2] = calibration3->gain;
                gain_src[3] = calibration4->gain;

                offset_src[0] = calibration1->offset;
                offset_src[1] = calibration2->offset;
                offset_src[2] = calibration3->offset;
                offset_src[3] = calibration4->offset;

                sample[i].gain = vld1q_f32(gain_src);
                sample[i].offset = vld1q_f32(offset_src);
                sample[i].angle = angle1;
                sample[i].x = x;
                sample[i].y = y;
                
                //core::calc_distance_accurate(GOPTS->center_x, GOPTS->center_y, x, y);
                sample[i].center_dist = core::calc_distance(GOPTS->center_x, GOPTS->center_y, x, y);

                if (sample[i].center_dist > GOPTS->inner_dist && sample[i].center_dist < GOPTS->outer_dist) {
                    all_points.emplace_back(sample[i]);// insert(all_points.end(), p.begin(), p.end());
                    i++;
                }

            }
        }

        DEBUG("width [%d] height: [%d] reserved elements: [%d], computed points [%lu]", len, height, (height * len), (unsigned long)all_points.size());
        return all_points;
    }


}