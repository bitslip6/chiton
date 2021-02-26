#include <vector>
#include <algorithm>
#include <map>
#include <functional>
#include <math.h>


#include "demos.h"
#include "core.h"
#include "system.h"
#include "shapes.h"
#include "utils.hpp"
#include "data.h"
#include "color/hsv.h"

#include "fplus/fplus.hpp"



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
    radians = -radians;
    radians -= PI/2;
    xy_t p = {(s16)round(radius * cos(radians)), (s16)round(radius * sin(radians))};
    return p;
}



/**
 * calculate a single point on a circle at radius and radians
 *
 */
xyf_t calculate_circle_point_float(uint16_t radius, float radians) {
    radians -= PI/2;
    xyf_t p = {radius * cosf(radians), radius * sinf(radians)};
    return p;
}

/**
 * return degree in radians from -PI to +PI of pixel from center of image
 * 
 * PURE
 */
float pixel_to_radians(xy_t center, xy_t pixel) {
    return atan2f((float)center.x-pixel.x, (float)center.y-pixel.y);
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
        for (float angle = 0.0f+offset; angle < PI2+offset; angle += PI2 / (numpoints-1)) {
            if (angle > 3.8f && angle < 5.4f) { continue; }
            points.emplace_back(calculate_circle_point(radius, angle));
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
        
        float r0 = v0->radian;
        float r1 = v1->radian;
        
        if (std::abs(r0) == std::abs(r1)) {
            if (angle > 0) { r1 =  3.45f; } // NOTE: 3.45 NOT 3.14 (3.45 = 2.82 + (vixel distance: .63)
            else { r1 =  -3.45f; }
        }

        float v0dist = std::abs(angle - r0);
        float v1dist = std::abs(angle - r1);

        
        float value = ((v0->gain*(r1 - angle))+(v1->gain*(angle - r0))) / (r1 - r0);
        float offset = ((v0->offset*(r1 - angle))+(v1->offset*(angle - r0))) / (r1 - r0);
        
        
        VixelCalibration *v = new VixelCalibration();
        v->vixel = (v0dist < v1dist) ? v0->vixel : v1->vixel;
        v->radian = angle;
        v->gain = value;
        v->offset = (s16)offset;

        return v;
    }

    /**
     * find the vixel calibration for a given angle in radians
     * TODO: blending vixels 4 and 5 (-3.14 +3.14 needs work...)
     * TODO: make sure 8 and 1 are on the correct sides
     * TODO: expert system.  it's ugly but fast and will work 100%
     */
    VixelCalibration* find_calibration_for_radian(float angle) {
        
        // iterate over all calibration entries
        //for (auto& [vixel_num, calibration] : *GOPTS->cal) {
        //for (const auto &cal : *GOPTS->cal) {
        // we could hard code angles for next_close here for > 2.71 = vixel @ -3.14 and < 2.71 @ +3.14

        if (angle > -1.05f && angle < 0) {
            return GOPTS->cal->find(8)->second;
        }
        if (angle < 1.05f && angle > 0) {
            return GOPTS->cal->find(1)->second;
        }
        VixelCalibration closest, next_close;

        if (angle <= -2.82f ) { 
            //return GOPTS->cal->find(8)->second;
            closest = *GOPTS->cal->find(5)->second;
            next_close = *GOPTS->cal->find(4)->second;
        } else if (angle >= 2.82f ) {
            //return GOPTS->cal->find(8)->second;
            //closest = *GOPTS->cal->find(4)->second;
            //next_close = *GOPTS->cal->find(4)->second;
            closest = *GOPTS->cal->find(4)->second;
            next_close = *GOPTS->cal->find(5)->second;
        } else {
            std::vector<GainPair*> gains;
            for (auto const&[key, val] : *GOPTS->cal) {
                gains.emplace_back(new GainPair(key, val->radian, angle - val->radian));
            }

            //  maybe we can reduce this to just 2 vixels ?
            // sort by vixel distance to all other vixels...
            std::sort( gains.begin( ), gains.end( ), [angle]( const auto& lhs, const auto& rhs ) { 
                return std::abs(lhs->radian_diff) < std::abs(rhs->radian_diff);
            } );
            
            closest = *GOPTS->cal->find(gains.at(0)->idx)->second;
            next_close = *GOPTS->cal->find(gains.at(1)->idx)->second;
        }
        
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
        y = CLAMP480(iPartOfNumber(y1));
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
            (u16) slice,
            false
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
        auto point = point_to_sample2(line.p2.x, line.p2.y, src_width, doffset, segments);
        point.bottom = true;
        points.emplace_back(point);

        return points;
    }

    /**
     * return the value of a sample at a sample location
     */
    static inline uint16_t sample_point(uint16_t *src, u16 width, xy_samp_t *p) {

        //return src[(p->p1.y*width)+p->p1.x] * 1.43f;

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
    
    void find_min_slice_dist(std::vector<u16> &slice, SliceData *data) {
        u16 len = slice.size();
        if (len < 7) { 
            data->min = 100;
            data->max = 100;
            data->mean = 100;
            data->mode = 100;
            return;
        }
        
        std::sort(slice.begin( ), slice.end( ), [](const auto& lhs, const auto& rhs) { 
            return lhs<rhs;
        });

        data->max = (slice.at(len - 3) + slice.at(len - 4))/2;
        data->min = (slice.at(4));// + slice.at(4))/2;
        data->mode = slice.at(len/2);
    }


    /**
     * todo: map points from vertical strips to horizontal strips
     */
    u16 map_points_fastest(const std::vector<offset_samp_t> &points, uint16_t *src, std::vector<AngleDist> &distances) {
        
        distances.clear();
        
        UNUSED u8 highlight = 0;
        u16 dist_zero = 0;

        std::vector<u16> slice = std::vector<u16>();
        slice.reserve(180);
        
        for (std::size_t i=0; i<points.size(); ++i) {
            offset_samp_t sample = points[i];
            
            if (sample.bottom) {
                
                SliceData data = {};
                find_min_slice_dist(slice, &data);
                
                AngleDist d;
                d.angle = sample.angle;
                d.dist = (data.min * sample.gain) + sample.gain_offset;
                d.dist_max = (data.max * sample.gain) + sample.gain_offset;
                d.saturated = dist_zero;
                d.gain = sample.gain;
                d.offset = sample.gain_offset;
                
                slice.clear();
                distances.emplace_back(d);

                highlight = 0;
                dist_zero = 0;
                continue;
            }

            u16 t = src[sample.src_off];

            if (t < 150) {
                highlight = 3;
                dist_zero++;
                continue;
            } else {
                slice.emplace_back(t);
            }
        }
        
        return 0;
    }



    /**
     * todo: map points from vertical strips to horizontal strips
     */
    bool map_points_greyscale(const std::vector<offset_samp_t> &points, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data) {

        //std::lock_guard<std::mutex> guard(GOPTS->point_lock);
        std::map<u16, u16> dist_map;
        
        UNUSED u16 segment = 0;
        UNUSED u16 vert_count = 0;
        u16 dist = 9999;
        UNUSED const int height = data::get_frame_height();

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
            */

            dst[p.dst_off] = (src[p.src_off] * p.gain ) + p.gain_offset;

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
            
            /*// find closest pixel in each "strip"
            if (vert_count++ > height) {
                //dist_map[segment++] = dist;
                vert_count = 0;
                printf("segment [%d] dist: [%d]\n", segment, dist);
                dist = 9999;
            }
            */
        }
        
        //printf("\n\n");
        
        return true;
    }

    /**
     * todo: map points from vertical strips to horizontal strips
     */
    bool map_points_gain(const std::vector<offset_samp_t> &points, uint16_t *src, uint16_t *dst, unsigned char *jpeg_data) {

        std::lock_guard<std::mutex> guard(GOPTS->point_lock);
        std::map<u16, u16> dist_map;
        
        UNUSED u16 segment = 0;
        UNUSED u16 vert_count = 0;
        u16 dist = 9999;
        UNUSED const int height = data::get_frame_height();

        // optimization:
        // we only need to calculate the gain/offset for each "slice"
        // so we should iterate over each slice and then each column in the slice
        // this way we can still vectorise the gain and offset calculation...
        for(auto p : points) {
            
            //UNUSED int xoff = (p.xf > 0.5f) ? -1 : 1;
            //UNUSED int yoff = (p.yf > 0.5f) ? -640 : 640;

            dst[p.dst_off] = src[p.src_off];

            // map greyscale jpeg image data
            uint rgb_offset = p.dst_off * 3; 

            if(isnan(p.gain)) {
                PANIC("gain is NAN: [%f.%f] %f (%f) - %d", p.xf, p.yf, p.angle, p.gain, p.gain_offset);
                exit(0);
            }
            
            uint8_t v = int8_t((p.gain-1.4f) * 1000.0f);
            /*
            if (v < 64) {
                printf("%f = %f = %d\n", p.angle, p.gain, v);
            }
            */
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
        
        UNUSED u16 segment = 0;
        UNUSED u16 vert_count = 0;
        UNUSED u16 dist = 9999;
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
            float angle = p.angle * (float)M_PI / 180; 

            int dx = closest * cos(angle);
            int dy = closest * sin(angle);

            idx += 20;
            INFO("y: %d, idx: %d, %f, %d, %d, %d\n", y, idx, (double)p.angle, closest, dx, dy);
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
                if (angle > -0.77f && angle < 0.77f) {
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
                    (u16)y,
                    false
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
            //DEBUG("line [%d] off1[%d]", i, p.at(0).dst_off);
            all_points.insert(all_points.end(), p.begin(), p.end());
        }

        DEBUG("width [%d] height: [%d] reserved elements: [%d], computed points [%lu]", len, height, (height * len), (unsigned long)all_points.size());
        return all_points;
    }


    
}