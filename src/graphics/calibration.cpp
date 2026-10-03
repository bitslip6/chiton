#include <stdint.h>
#include <algorithm>
#include <array>
#include <limits>
#include "neon_compat.h"
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <jpeglib.h>

#include "demos.h"
#include "system.h"
#include "data.h"
#include "data.h"
#include "shapes.h"
#include "core.h"
#include "color/hsv.h"
#include "utils.hpp"
#include "calibration.h"




namespace calibration
{ 
    static u16 *pixel_dist = NULL;
    static float *pixel_gain = NULL;
    static float *pixel_offset = NULL;
    static float *angle_sin_vert_cos = NULL;
    static float *vert_sin = NULL;
    static float *vert_cos = NULL;
    static float *angle_cos_vert_sin = NULL;
    static float *pixel_angle = NULL;
    static float *pixel_vert_radians = NULL;

    static u16 *pixel_angle_bucket = NULL;
    static u16 *angle_bucket = NULL;
    static u16 bucket_idx = 0;

    static u16 *pixel_line_bucket = NULL;


    static uint *src_offsets = NULL;
    static runline* runlines = NULL;
    static u16 num_runlines = 5555;
    //static u16 pixel_angle_bucket[1024];

    static point *calibration = NULL;

    float* get_pixel_gain() { return pixel_gain; }
    float* get_pixel_offset() { return pixel_offset; }
    float* get_x_factor() { return angle_sin_vert_cos; }
    float* get_y_factor() { return vert_sin; }
    float* get_z_factor() { return angle_cos_vert_sin; }
    uint* get_offsets() { return src_offsets; }
    u16 get_num_runlines() { return num_runlines; }
    runline* get_run_lines() { return runlines; }
    point* get_calibration() { return calibration; }

    u16 * get_pixel_dist() { return pixel_dist; }
    u16 * get_pixel_bucket() { return pixel_angle_bucket; }
    float *get_horiz_angle() { return pixel_angle; }
    float *get_vert_angle() { return pixel_vert_radians; }




    int padding(int len, int alignment) {
        return (alignment - (len % alignment)) % alignment;
    }

    u16 *make_slam_buffer() {
        return NEWMEM(u16, SLAM_POINTS * 101);
    }

/*
    void average_frame(const u16 *__restrict__ src, u16 *dst) {
        for (int i=0; i<(CAM_WIDTH*CAM_HEIGHT)-4;) {
            dst[i] = (dst[i] * 0.9f) + (src[i] * 0.1f);
            i++;
            //dst[i] = (src[i] * vert_cos[i]);
            //dst[i] = (dst[i] * 0.9f) + ((src[i] * vert_cos[i]) * 0.1f);
            dst[i] = (dst[i] * 0.9f) + (src[i] * 0.1f);
            i++;
            //dst[i] = (src[i] * vert_cos[i]);
            //dst[i] = (dst[i] * 0.9f) + ((src[i] * vert_cos[i]) * 0.1f);
            dst[i] = (dst[i] * 0.9f) + (src[i] * 0.1f);
            i++;
            //dst[i] = (src[i] * vert_cos[i]);
            //dst[i] = (dst[i] * 0.9f) + ((src[i] * vert_cos[i]) * 0.1f);
            dst[i] = (dst[i] * 0.9f) + (src[i] * 0.1f);
            i++;
        }
    }
    */

    void apply_vert_cos(const u16 *__restrict__ src, u16 *dst) {
        for (int j=0; j<num_runlines; j++) {
            for (uint i=runlines[j].srcstart; i<runlines[j].runlen;i++) {
                dst[i] = (src[i] * vert_cos[i]);
            }
        }
    }

    void average_frame(const u16 *__restrict__ src, u16 *dst) {
        if (GOPTS->smoothing) {
            for (int j=0; j<num_runlines; j++) {
                for (uint i=runlines[j].srcstart; i<runlines[j].runlen;i++) {
                    dst[i] = (dst[i] * 0.8f) + (src[i] * 0.2f);
                }
            }
        } else {
            memcpy(dst, src, CAM_WIDTH*CAM_HEIGHT*2);
        }
    }


    void avg_frame_fast(const u16 *__restrict__ src, u16 *__restrict__ dst) {
        for (int i=0; i<(CAM_WIDTH*CAM_HEIGHT)-4; i+= 4) {
            // transition buffer for float multiply result
            float32_t fbuf[4];
            // load 4 uint16's
            uint16x4_t U0 = vld1_u16(src+i);
            // convert uint16 to uint32s
            uint32x4_t I0 = vmovl_u16(U0);
            // convert uint32s to floats
            float32x4_t S0 = vcvtq_f32_u32(I0);
            //float32x4_t S0 = vreinterpretq_f32_u16(U0);

            // 128 (4 32bit floats) bits of gain data
            float32x4_t G0 = vld1q_f32((const float *)vert_cos+i);
            // load the offset directly into the destination
            float32x4_t A0 = vmulq_f32(S0, G0);
            // load the destination into out 4 output floats
            vst1q_f32(fbuf, A0);
            // copy the  output buffer to the destination frame
            //dst[i] = (dst[i]/2) +(fbuf[0]/2);
            //dst[i+1] = (dst[i+1]/2) +(fbuf[1]/2);
            //dst[i+2] = (dst[i+2]/2) +(fbuf[2]/2);
            //dst[i+3] = (dst[i+3]/2) +(fbuf[3]/2);

            dst[i] = fbuf[0];
            dst[i+1] = fbuf[1];
            dst[i+2] = fbuf[2];
            dst[i+3] = fbuf[3];
        }
    }




    void calibrate_frame(const u16 *__restrict__ src, u16 *__restrict__ dst) {

        const float *__restrict__ gain = calibration::get_pixel_gain();
        const float *__restrict__ pixel_offsets = calibration::get_pixel_offset();
        u16 *__restrict__ slam = data::get_slam();

        for (int j=0; j<num_runlines; j++) {
            const uint srcstart = runlines[j].srcstart;
            const uint runlen = runlines[j].runlen;


            if (GOPTS->smoothing) {
                for (uint i=srcstart; i<(runlen);i++) {
                    const float corrected = ((src[i] * gain[i]) + pixel_offsets[i]);// * vert_cos[i];
                    //const int16_t diff = corrected - dst[i];
                    //dst[i] = (std::abs(diff) > 256) ? corrected : (dst[i] * 0.7f ) + (corrected * 0.3f);
                    dst[i] = (dst[i] * 0.7f ) + (corrected * 0.3f);

                    const u16 offset = pixel_angle_bucket[i];
                    slam[offset] = (src[i] > 250) ? (std::min(src[i], slam[offset])): slam[offset];
                }
            } else {
                for (uint i=srcstart; i<(runlen);i++) {
                    dst[i] = ((src[i] * gain[i]) + pixel_offsets[i]);// * vert_cos[i];
                    const u16 offset = pixel_angle_bucket[i];
                    slam[offset] = (src[i] > 250) ? (std::min(src[i], slam[offset])): slam[offset];
                }
            }
        }
    }

    void calibrate_frame_f(const u16 *__restrict__ src, float *dst) {
        const float *__restrict__ gain = calibration::get_pixel_gain();
        const float *__restrict__ offset = calibration::get_pixel_offset();

        int pixels = 0;
        int runidx = 0;
        bool done = false;
        for (int j=0; j<num_runlines; j++) {
            uint srcstart = runlines[j].srcstart;
            uint runlen = runlines[j].runlen;

            for (uint i=srcstart; i<(runlen); i++) {
                float d = ((src[i] * gain[i]));
                dst[i] = (dst[i] * 0.8f) + (d * 0.2f);
                i++;

                /*
                // transition buffer for float multiply result
                float32_t fbuf[4];
                // load 4 uint16's
                uint16x4_t U0 = vld1_u16(src+i);
                // convert uint16 to uint32s
                uint32x4_t I0 = vmovl_u16(U0);
                // convert uint32s to floats
                float32x4_t S0 = vcvtq_f32_u32(I0);

                // 128 (4 32bit floats) bits of gain data
                float32x4_t G0 = vld1q_f32((const float *)gain+i);
                // load the offset directly into the destination
                float32x4_t D0 = vld1q_f32((const float *)offset+i);
                // multiply src * gain and add the result to the destination
                float32x4_t A0 = vmlaq_f32(D0, S0, G0);
                // load the destination into out 4 output floats
                vst1q_f32((float *)dst+i , A0);
                */
                //pixels += 4;
            }
            //runidx++;
        }
    }


    u16 find_min(const u16 *__restrict__ src, uint start, uint end) {
        u16 min = 0xFFFF;
        for (uint i=start; i<end; i++) {
            if (src[i] < min) { 
                min = src[i];
            }
        }
        return min;
    }

    u8 min_dist_to_power_level(u16 current_dist, u8 current_power)  {
        s8 mapped_value = (s8)core::value_map(current_dist, 400, 2000, -0x01, 0x0D);

        if (current_power-2 > mapped_value) { 
            //INFO("dist: [%d](%x) -> %x  -1", current_dist, current_power, mapped_value);
            // target mapped value is less than current, subtract by 1/2 the distance
            return current_power-MAX(1, (current_power - mapped_value)/2);
        } else if (current_power+2 < mapped_value) {
            //INFO("dist: [%d](%x) -> %x  +1", current_dist, current_power, mapped_value);
            // target mapped value is more than current, add by 1/2 the distance
            return current_power+MAX(1, (mapped_value - current_power)/2);
        }
        //INFO("FLAT dist: [%d](%x) -> %x  0", current_dist, current_power, mapped_value);
        // target value is same (close enough) to current value
        return MAX(current_power, 0x0);
        //return CLAMP(current_power, 0x00, 0x0C);
    }

    uint32_t dynamic_illumination(u16 *__restrict__ slam, u16 slam_idx, u32 cur_ill) {
        u16 *new_slam = slam + (SLAM_POINTS *slam_idx);

        u32 illuminate = 0;
        u8 t0 = (min_dist_to_power_level(find_min(new_slam, 128, 320), cur_ill & 0x000000FF));
        u8 t1 = (min_dist_to_power_level(find_min(new_slam, 320, 512), (cur_ill & 0x0000FF00)>>8));
        u8 t2 = (min_dist_to_power_level(find_min(new_slam, 512, 704), (cur_ill & 0x00FF0000)>>16));
        u8 t3 = (min_dist_to_power_level(find_min(new_slam, 704, 896), (cur_ill & 0xFF000000)>>24));

        //INFO("%x %x %x %x", t0, t1, t2, t3);

        illuminate |= t0;
        illuminate |= t1 << 8;
        illuminate |= t2 << 16;
        illuminate |= t3 << 24;
        INFO("current illumination 0x%08x -> 0x%08x", cur_ill, illuminate);

        return illuminate;
    }

    void vector_slam(u16 *__restrict__ src, u16 *__restrict__ slam, u16 slam_idx) {
        u16 *new_slam = slam + (SLAM_POINTS *slam_idx);
        u16 *slam_info = slam + (SLAM_POINTS * 99);
        for (int i=0; i<SLAM_POINTS; i++) { new_slam[i] = 9999; CLEAR_SLAM_SPIKE(slam_info[i]); }

        // compute slam points
        for (int j=0;j<num_runlines;j++) { 
            for (uint i=runlines[j].srcstart; i<runlines[j].runlen; i++) {
                u16 offset = pixel_angle_bucket[i];
                if (offset > SLAM_POINTS) { PANIC("i %d offset (%d)", i, offset); }
                if ((src[i] < new_slam[offset]) && src[i] > 250) { 
                    new_slam[offset] = src[i];
                }
            }
        }

        // throw out spikes
        for (int k=0; k<4; k++) {
            for (int i=0; i < SLAM_POINTS-3; i++) {
                if (new_slam[i] == 9999) { i+=3; continue; }
                if ((new_slam[i] - new_slam[i-3]) < -15 && (new_slam[i] - new_slam[i+3]) < -15) {
                    new_slam[i] = (new_slam[i-3] + new_slam[i+3]) / 2;
                    SET_SLAM_SPIKE(slam_info[i]);
                }
            }
        }


        // average over time if values are close
        for (int i=0; i < SLAM_POINTS; i++) {
            uint total = 0;
            uint num = 0;
            for (int j=9; j>=0; j--) {
                u16 check_idx = (j * SLAM_POINTS) + i;
                // if the difference in current slam point and previous slam 
                // point is less than 60mm, add it to the average
                if (std::abs(slam[check_idx] - new_slam[i]) < 60) { 
                    total += slam[check_idx];
                    num++;
                }
            }
            // if we have at least 3 points, average them and add to rolling average
            if (num > 2) {
                new_slam[i] = (new_slam[i] * 0.3f) + ((total/num) * 0.7f);
            }
            // set static slam point
            if (num > 7) {
                SET_SLAM_STATIC(slam_info[i]);
                CLEAR_SLAM_DYNAMIC(slam_info[i]);
            }
            // set dynamic slam point
            else {
                CLEAR_SLAM_STATIC(slam_info[i]);
                SET_SLAM_DYNAMIC(slam_info[i]);
            }
            CLEAR_SLAM_AOI(slam_info[i]);
        }

        // SET AREA OF INTREST
        // slam_info 1024 is min dist in AOI angles, 0 is overall min dist
        const u16 min_dist = 800;
        slam_info[1024] = 6000;
        slam_info[0] = 6000;

        for (int i=384; i<640; i++) {
            float angle_off = std::abs(512 - i) * SLAM_ANGLE;
            float check_dist = cosf(angle_off) * min_dist;
            if (new_slam[i] < check_dist) { slam_info[0] = MIN(slam_info[0], new_slam[i]); SET_SLAM_AOI(slam_info[i]); }

            else if (i > 480 && i < 522) {
                SET_SLAM_AOI(slam_info[i]);
                if (new_slam[i] < slam_info[1024]) { slam_info[1024] = new_slam[i]; }
            }
        }
    }


    s16 avg_distance(u16 frame1, u16 min_angle, u16 max_angle) {
        uint total = 0;
        uint offset = frame1 * SLAM_POINTS;
        for (int i=min_angle; i<max_angle; i++) {
            total += angle_bucket[offset + i];
        }

        return total  / (max_angle - min_angle);
    }


    s16 range_distnace(u16 frame1, u16 frame2, u16 min_angle, u16 max_angle) {
        return avg_distance(frame1, min_angle, max_angle) - avg_distance(frame2, min_angle, max_angle);
    }


    xy_t find_translation(u16 frame1, u16 frame2) {
        bool done = false;
        static xy_t offset = {0, 0};
        while (!done) {
            offset.y = range_distnace(frame1, frame2, 512-32, 512+32);
            s16 x1 = range_distnace(frame1, frame2, 256-32, 256+32);
            s16 x2 = range_distnace(frame1, frame2, 768-32, 768+32);
            // average 2 distances, one is negative, so we subract not add.
            // offset.x = average x delta to the sensor's left (top down view)
            offset.x = (x1 - x2) / 2; 
        }

        return offset;
    }


    /**
     * @brief  estimate the camera rotation
     * 
     * @param target_frame 
     * @param test_frame 
     * @param estimate 
     * @return s16 
     */
    s16 estimate_rotation(const u16 *__restrict__ target_frame, const u16 *__restrict__ test_frame) {
        s16 rotation_idx = 0;
        int min_offset = 99999999;

        printf("EST ROT\n");

/*
        int offset = 0;
        int ctr = 0;
        for (s16 i=0; i < SLAM_POINTS; i++) {
            //printf("i = [%d]\n", i);
            offset += target_frame[i] - test_frame[i + window];
            ctr++;
        }
        */
 


        for (s16 window = -128; window < 128; window++) {
            int offset = 0;
            int ctr = 0;
            // slide over the window
            for (s16 i = MAX(0, 0+window); i < MIN(SLAM_POINTS, SLAM_POINTS+window); i++) {
                //printf("i = [%d]\n", i);
                offset += target_frame[i] - test_frame[i + window];
                ctr++;
            }
            offset /= ctr;
            //printf("window [%d] offset [%d] \n", window, offset);

            if (min_offset > offset) {
                min_offset = offset;
                rotation_idx = window;
            }
        }

        printf("estimate rotation: [%d] (%d)", rotation_idx, min_offset);

        return rotation_idx;
    }


    // Unimplemented legacy stub; no inputs are currently consumed.
    void find_rotation(u16, u16, xy_t) {
    }









    /**
     * @brief generate calibrated data and render a 3d point cloud using vectorized math
     * requires calibraion::make_calibration_data() to be called first...
     * 
     * @param src the pointer to raw data
     * @param dst the pointer to the calibrated data 
     */
     void vector_point_cloud(u16 *__restrict__ src) {
     
        //const uint max = calibration::get_num_points();

        const float *__restrict__ gain = calibration::get_pixel_gain();
        const float *__restrict__ offset = calibration::get_pixel_offset();
        const float *__restrict__ x_factor = calibration::get_x_factor();
        const float *__restrict__ y_factor = calibration::get_y_factor();
        const float *__restrict__ z_factor =  calibration::get_z_factor();
        s16 * pc = data::get_point_cloud();
		float *t;

        u16 *__restrict__ slam = data::get_slam();
        u16 *slam_info = slam + (SLAM_POINTS * 99);

        int max = 0;
        uint pcidx = 0;

        for (int j=0;j<num_runlines;j++) {
            uint srcstart __attribute__ ((aligned (4))) = runlines[j].srcstart;
            uint runlen __attribute__ ((aligned (4))) = runlines[j].runlen;
            //done = (srcstart == 0);

            for (uint i=srcstart; i<(runlen & ~3); i+=4) {
                //for (uint j=0; j<4; j++) {
                    // skip spikes!
                    //u16 slam_offset = pixel_angle_bucket[i];
                    //if ((slam_info[slam_offset] & (1 << SLAM_SPIKE_BIT))) { continue; }

                    float d = src[i];
                    pc[pcidx++] = (d * x_factor[i]);
                    pc[pcidx++] = (d * y_factor[i]);
                    pc[pcidx++] = (d * z_factor[i]);
                    pc[pcidx++] = (d);

                //}
               /*
                float32_t fbuf[4];
                // load 4 uint16's
                uint16x4_t U0 = vld1_u16(src+i);
                // convert uint16 to uint32s
                uint32x4_t I0 = vmovl_u16(U0);
                // convert uint32s to floats
                float32x4_t S0 = vcvtq_f32_u32(I0);

                // 128 (4 32bit floats) bits of x data
                float32x4_t T0 = vld1q_f32((const float *)gain+i);
                // load the offset directly into the destination
                float32x4_t T1 = vld1q_f32((const float *)offset+i);
                // multiply src * gain and add the result to the destination
                float32x4_t A0 = vmlaq_f32(T1, S0, T0);

                // load xfactor
                T0 = vld1q_f32((const float *)x_factor+i);
                // multiply distance by x factor
                T1 = vmulq_f32(A0, T0);
                // load request into float bufrfer
                vst1q_f32(fbuf, T1);
                // copy the output buffer to the destination frame
                pc[pcidx] = fbuf[0];
                pc[pcidx+4] = fbuf[1];
                pc[pcidx+8] = fbuf[2];
                pc[pcidx+12] = fbuf[3];
                pcidx++;

                // load yfactor
                T0 = vld1q_f32((const float *)y_factor+i);
                // multiply distance by x factor
                T1 = vmulq_f32(A0, T0);
                // load request into float bufrfer
                vst1q_f32(fbuf, T1);
                // copy the output buffer to the destination frame
                pc[pcidx] = fbuf[0];
                pc[pcidx+4] = fbuf[1];
                pc[pcidx+8] = fbuf[2];
                pc[pcidx+12] = fbuf[3];
                pcidx++;

                // load zfactor
                T0 = vld1q_f32((const float *)z_factor+i);
                // multiply distance by x factor
                T1 = vmulq_f32(A0, T0);
                // load request into float bufrfer
                vst1q_f32(fbuf, T1);
                // copy the output buffer to the destination frame
                pc[pcidx] = fbuf[0];
                pc[pcidx+4] = fbuf[1];
                pc[pcidx+8] = fbuf[2];
                pc[pcidx+12] = fbuf[3];
                pcidx++;

                // load raw depth value
                vst1q_f32(fbuf, A0);
                // copy the output buffer to the destination frame
                pc[pcidx] = fbuf[0];
                pc[pcidx+4] = fbuf[1];
                pc[pcidx+8] = fbuf[2];
                pc[pcidx+12] = fbuf[3];
                pcidx++;
                */

            }
        }// while (!done && pcidx < 640*480*4);

        data::set_point_cloud_size(pcidx);
        GOPTS->frame_request = false;
    }

    void add_sample(calibration::point *sample, int idx) {
        calibration::point *src = get_calibration();
        sample->near_dist += src[idx].near_dist;
        sample->far_dist += src[idx].far_dist;
        sample->near_point += src[idx].near_point;
        sample->far_point += src[idx].far_point;
        sample->center_angle += src[idx].center_angle;
    }

    //calibration::point sample_index(int x, int y) {
    calibration::point sample_index(int idx) {
        calibration::point *p = get_calibration();
        calibration::point ret;
        // int idx = (y * CAM_WIDTH) + x;
        

        int num_points = 0;
        ret.near_point = 0;
        ret.near_dist = 0;
        ret.far_point = 0;
        ret.far_dist = 0;
        ret.center_angle = 0;

        // left pixel
        if (p[idx-1].near_point > 400) {
            add_sample(&ret, idx-1);
            num_points++;
        }
        // right pixel
        if (p[idx+1].near_point > 400) {
            add_sample(&ret, idx+1);
            num_points++;
        }
                // down pixel
        if (p[idx+CAM_WIDTH].near_point > 400) {
            add_sample(&ret, idx+CAM_WIDTH);
            num_points++;
        }

        // don't look at 
        if (idx > CAM_WIDTH) {
            // up pixel
            if (p[idx-CAM_WIDTH].near_point > 400) {
                add_sample(&ret, idx-CAM_WIDTH);
                num_points++;
            }

            // up left pixel
            if (p[(idx-CAM_WIDTH)-1].near_point > 400) {
                add_sample(&ret, idx-CAM_WIDTH-1);
                num_points++;
            }
        
            // up right pixel
            if (p[(idx-CAM_WIDTH)+1].near_point > 400) {
                add_sample(&ret, idx-CAM_WIDTH+1);
                num_points++;
            }
        }
        // down left pixel
        if (p[(idx+CAM_WIDTH)-1].near_point > 400) {
            add_sample(&ret, idx+CAM_WIDTH-1);
            num_points++;
        }
        // down right pixel
        if (p[(idx+CAM_WIDTH)+1].near_point > 400) {
            add_sample(&ret, idx+CAM_WIDTH+1);
            num_points++;
        }

        ret.near_point /= num_points;
        ret.near_dist /= num_points;
        ret.far_point /= num_points;
        ret.far_dist /= num_points;
        ret.center_angle /= num_points;

        if (num_points > 0) {
            INFO ("sample points %d", num_points);
        }
        return ret;
    }


    /**
     * @brief fill in missing calibration data with average of nearby points
     * 
     */
    void fill_missing_calibration_data() {
        calibration::point *p = get_calibration();

        //for (int y=0; y<CAM_HEIGHT; y++) {
        //    for (int x=0; x<CAM_WIDTH; x++) {

        for (int j=0; j<num_runlines; j++) {
            uint srcstart = runlines[j].srcstart;
            uint runlen = runlines[j].runlen;

            for (uint i=srcstart; i<(runlen); i++) {
                //float d = ((src[i] * gain[i]));
                //dst[i] = (dst[i] * 0.8f) + (d * 0.2f);
                //i++;

                //int idx = (y * CAM_WIDTH) + x;
                int idx = i;
                //if (pixel_angle[idx] < 3.141f && pixel_angle[idx] > 2.89f) {
                    //INFO("ANGLE %f", pixel_angle[idx]);
                    //if (p[idx]. == 0 || p[idx].far_point == 0) {
                    //if (
                    //    (pixel_gain[idx] > 1.6f || pixel_gain[idx] < 1.01f) || 
                    //    (pixel_offset[idx] > 150.0f || pixel_offset[idx] < -150.0f))
                    if (p[idx].near_point == 0 || p[idx].far_point == 0) {
                        //INFO("FILLING %d", idx);
                        //p[idx] = sample_index(idx);
                    //{
                        calibration::point tmp = sample_index(idx);
                        int y = floorf(idx / CAM_WIDTH);
                        int x = idx - (y * CAM_WIDTH);
                        INFO("Filling gain @[%dx%d] rad:%f, dist:%d. gain: %f off: %f", x, y,
                             static_cast<double>(tmp.center_angle), pixel_dist[idx],
                             static_cast<double>(pixel_gain[idx]), static_cast<double>(pixel_offset[idx]));

                        INFO("Filling in missing calibration data for pixel (%d) = %d / %d @%f", idx,
                             tmp.near_point, tmp.far_point, static_cast<double>(tmp.center_angle));
                        if (tmp.near_point != 0 || tmp.far_point != 0) {
                            p[idx].near_dist = tmp.near_dist;
                            p[idx].far_dist = tmp.far_dist;
                            p[idx].near_point = tmp.near_point;
                            p[idx].far_point = tmp.far_point;
                            p[idx].center_angle = tmp.center_angle;
                            p[idx].index = idx;
                        }
                    }
               // }
            }
        }
    }


    void make_calibration_data() {
        xy_t center = {GOPTS->center_x, GOPTS->center_y};
        printf("mcd 1\n");
        
        if (pixel_gain != NULL) { free(pixel_gain); }
        pixel_gain = NEWMEM(float, 640*480);

        printf("mcd 2\n");

        if (pixel_offset != NULL) { free(pixel_offset); }
        pixel_offset = NEWMEM(float, 640*480);
        if (angle_sin_vert_cos != NULL) { free(angle_sin_vert_cos); }
        angle_sin_vert_cos = NEWMEM(float, 640*480);
        if (vert_sin != NULL) { free(vert_sin); }
        vert_sin = NEWMEM(float, 640*480);
        if (vert_cos != NULL) { free(vert_cos); }
        vert_cos = NEWMEM(float, 640*480);
        if (angle_cos_vert_sin != NULL) { free(angle_cos_vert_sin); }
        angle_cos_vert_sin = NEWMEM(float, 640*480);
        if (src_offsets != NULL) { free(src_offsets); }
        src_offsets = NEWMEM(uint, 640*480);

        if (runlines != NULL) { free(runlines); }
        runlines = NEWMEM(runline, 2048);

        // the point calibration data
        if (calibration != NULL) { free(calibration); }
        calibration = (point *)NEWMEM(point, 640*480);

        if (pixel_angle != NULL) { free(pixel_angle); }
        pixel_angle = NEWMEM(float, 640*480);
        if (pixel_vert_radians != NULL) { free(pixel_vert_radians); }
        pixel_vert_radians = NEWMEM(float, 640*480);

        if (pixel_dist != NULL) { free(pixel_dist); }
        pixel_dist = NEWMEM(u16, 640*480);

        if (pixel_angle_bucket != NULL) { free(pixel_angle_bucket); }
        pixel_angle_bucket = NEWMEM(u16, 640*480);
        if (angle_bucket != NULL) { free(angle_bucket); }
        angle_bucket = NEWMEM(u16, 1024 * SLAM_POINTS);
        if (pixel_line_bucket != NULL) { free(pixel_line_bucket); }
        pixel_line_bucket = NEWMEM(u16, 640*480);


        printf("calibration\n");

        int runbegin = 0;
        int runidx = 0;
        for (int16_t y=0; y<FRAME_POLAR_HEIGHT_DEFAULT; y++) {
            for (int16_t x=0; x<FRAME_POLAR_WIDTH_DEFAULT; x++) {
                
                xy_t pixel = {x, y};
                uint32_t offset = (y * FRAME_POLAR_WIDTH_DEFAULT) + x;

                float angle = -gfx::pixel_to_radians(center, pixel);


                u16 dist = core::calc_distance(GOPTS->center_x, GOPTS->center_y, x, y);
                float dist_f = core::calc_distance_float(GOPTS->center_x, GOPTS->center_y, x, y);
                
                // skip past the 90deg at the top
                // skip inner and outer pixels 
                //if (dist < 137 || dist > 285) { continue; }
                if ((angle > -0.795f && angle < 0.795f) || (dist < GOPTS->inner_dist || dist > GOPTS->outer_dist)) {
                    // we have a black pixel, end of run, calculate run length and add the run;
                    if (runbegin > 0) {
                        runline *r = &runlines[runidx];
                        // left side starts on run begin, right side starts on PADDED run being (lose 0-3 pixels) 

                        // runpadding
                        r->srcstart = runbegin;//(runbegin > 320) ? runbegin + padding(runbegin, 4): runbegin;

                        // len is the full length byte aligned (lose 0-3 pixels)
                        r->runlen = offset;
                        // runpadding

                        runbegin = 0;
                        runidx++; // increase number of runs...
                    }
                    continue;
                }

                // have a new pixel, begin a new run...
                if (runbegin == 0) { runbegin = offset; }
                pixel_dist[offset] = dist;
                

                float vertical_angle = 0;
                if (dist >= 261 && dist <= 285) {
                    vertical_angle = core::value_map(dist_f, 261, 285, -7.633f, -15.0f);
                } else if (dist >= 240 && dist <= 261) {
                    vertical_angle = core::value_map(dist_f, 240, 261, 0.0f, -7.633f);
                } else if (dist >= 223 && dist <= 240) {
                    vertical_angle = core::value_map(dist_f, 223, 240, 7.633f, 0.0f);
                } else if (dist >= 204 && dist <= 223) {
                    vertical_angle = core::value_map(dist_f, 204, 223, 14.175f, 7.633f);
                } else if (dist >= 188 && dist <= 204) {
                    vertical_angle = core::value_map(dist_f, 188, 204, 20.621f, 14.175f);
                } else if (dist >= 173 && dist <= 188) {
                    vertical_angle = core::value_map(dist_f, 173, 188, 26.565f, 20.621f);
                } else if (dist >= 161 && dist <= 173) {
                    vertical_angle = core::value_map(dist_f, 161, 173, 31.995f, 26.565f);
                } else if (dist >= 151 && dist <= 161) {
                    vertical_angle = core::value_map(dist_f, 151, 161, 38.964f, 31.995f);
                } else if (dist >= 137 && dist <= 151) {
                    vertical_angle = core::value_map(dist_f, 137, 151, 44.8f, 38.964f);
                } 

                float vert_radians = vertical_angle * 0.0174533f;

                pixel_gain[offset] = ADI_GAIN;// * vertical_gain_multiplier(vertical_angle);
                pixel_offset[offset] = 0.0f;//49.0f;
                pixel_angle[offset] = (angle >= 0) ? angle : (PI * 2) + angle;

                // every pixel has a bucket destination
                u16 angle_idx = pixel_angle[offset] * 163;
                pixel_angle_bucket[offset] = 1024 - angle_idx;
                pixel_line_bucket[offset] = pixel_angle[offset] / ANGLE_POINTS_INC;

                // X
                angle_sin_vert_cos[offset] = -(sinf(angle) * cosf(vert_radians));
                // Y
                vert_sin[offset] = sinf(vert_radians);
                // Z
                angle_cos_vert_sin[offset] = cosf(angle) * cosf(vert_radians);

                if (vert_radians > 0) {
                    vert_cos[offset] = cosf(vert_radians/PI);
                } else {
                    vert_cos[offset] = cosf(vert_radians);
                }
                src_offsets[offset] = (y * FRAME_POLAR_WIDTH_DEFAULT) + x;
                pixel_vert_radians[offset] = vert_radians;
            }
        }
        WARN("RUN LINES [%d]", runidx);
        num_runlines = runidx;
    }

    /**
     * @calibrate per pixel offsets, expe
     * 
     * @param src  (averaged input data)
     * @param distance  (distance to wall at the center_angle), camera must be perpendicular to this angle and wall!
     * @param center_angle  (the angle (from 0-2PI) to the wall)
     */
    void save_calibration_frame(const u16 *__restrict__ src, const u16 distance, const float center_angle) {
        const float upper = center_angle + 0.42f;
        const float lower = center_angle - 0.42f;

        calibration::point *p = calibration;
        for (int i=0; i<CAM_WIDTH*CAM_HEIGHT; i++) {
            int y = floor(i/640);
            int x = i - y*640;

            // skip pixels that have no data
            if (pixel_angle[i] == 0.0f) { continue; }
            // skip pixels that are not being calibrated
            if (pixel_angle[i] < lower || pixel_angle[i] > upper) { continue; }
            //s16 dist = core::calc_distance(GOPTS->center_x, GOPTS->center_y, x, y);
            //if (dist > GOPTS->outer_dist || dist < GOPTS->inner_dist) { continue; }

            p[i].center_angle = center_angle;
            p[i].index = i;
            if (distance < 1500) {
                p[i].near_dist = distance;
                p[i].near_point = src[i];
            } else {
                p[i].far_dist = distance;
                p[i].far_point = src[i];
            }
            if (pixel_angle[i] > center_angle - 0.05f && pixel_angle[i] < center_angle + 0.05f) {
                printf("SAVE FRAME! %dx%d [%d] [%d]\n", x, y, p[i].near_dist, p[i].near_point);
            }
        }
    }

    /**
     * @brief update the average gain for a particular distance...
     * TODO: newly saved calibration data is RAW data  
     * TODO: verify that angle rotation mapping is correct...
     * 
     * 
     * @param dist the distance the measurement is for 
     * @param expected  the expected distance in mm
     * @param actual the RAW measured distance in mm
     * @param update true to update the average, -32000, just return it (overloaded input)...
     * @return float  the average gain at this distance...
     */
    gain_t* average_gain(uint i, u16 expected_far, u16 actual_far, s16 expected_near, s16 actual_near) {
        /*
        static float gain_map[640] = {ADI_GAIN};
        static float gain_total[640] = {0.0f};
        static uint gain_number[640] = {0};
        */
        static gain_t g;

        //float s1 = expected_far / (actual_far * vert_cos[i] * ADI_GAIN);
        //float s2 = expected_near / (actual_near * vert_cos[i] * ADI_GAIN);

        //float s1 = expected_far / (actual_far) * vert_cos[i];
        //float s2 = expected_near / (actual_near) * vert_cos[i];

        //float s1 = (expected_far)/ (actual_far);
        //float s2 = (expected_near)/ (actual_near);

        // PASS 1 (no offset for gain)
        ///////////////////////
        float s1 = (expected_far * vert_cos[i]) / (actual_far);
        float s2 = (expected_near * vert_cos[i]) / (actual_near);
        //_Float16 f1 = 0.0f;
        g.gain = ((s1 + s2) / 2);

// SHOULD WE * vert_cos here ??
        float o1 = (expected_near * vert_cos[i]) - (actual_near * g.gain);
        float o2 = (expected_far * vert_cos[i]) - (actual_far * g.gain);
        g.offset = (o1 + o2) / 2;

        // PASS 2 (use offset for gain)
        ///////////////////////
        /*
        s1 = ((expected_far + g.offset) * vert_cos[i])/ (actual_far);
        s2 = ((expected_near + g.offset) * vert_cos[i])/ (actual_near);
        g.gain = ((s1 + s2) / 2);

        o1 = expected_near - (actual_near * g.gain);
        o2 = expected_far - (actual_far * g.gain);
        g.offset = (o1 + o2) / 2;
        */


        return &g;




/*
        if (near_offset > -32000 && expected > 0 && actual > 0) {
            if (std::abs(actual - expected) > 800) { 
                printf("averaging ERROR: [%d] = (%f) / (%d) = (%f) [%d . %d]\n", dist, gain_total[dist], gain_number[dist], gain_map[dist], expected, actual);
                return gain_map[dist];
            }
            float amount = (float)expected / ((float)actual + near_offset);
            gain_total[dist] += amount;
            gain_number[dist] = gain_number[dist] + 1;
            gain_map[dist] =  gain_total[dist] / (float)gain_number[dist];
            if (dist == 239) {
                printf("averaging gain: [%d] = (%f) / (%d) = (%f) [%d . %d]\n", dist, gain_total[dist], gain_number[dist], gain_map[dist], expected, actual);
            }
            //printf("dist: %d, gain: %f\n", dist, gain_map[dist]);
        }
        //if () { printf("dist %d = %f\n", dist, gain_map[dist]); }
        return (gain_map[dist] + 1.006f) / 2;
        */
    }


    s16 calculate_wall_distance(const float angle_x_off, const float angle_y_off, const u16 expected_distance) {
        float d2 = expected_distance * expected_distance;
        // calculate X position
        float dist_x = (float)expected_distance / (cosf(angle_x_off));
        // calculate Y position
        float dist_y = (float)expected_distance / (cosf(angle_y_off));


        // calculate expected distance
        s16 dx = dist_x - expected_distance;
        s16 dy = dist_y - expected_distance;
        s16 ez = expected_distance + sqrtf(dx*dx + dy*dy);
        INFO("ang_x_off: %f, angle_y_off: %f, dist_x: %f, dist_y: %f   (%d)\n",
             static_cast<double>(angle_x_off), static_cast<double>(angle_y_off),
             static_cast<double>(dist_x), static_cast<double>(dist_y), ez);

        return ez;
    }

    /**
     * @brief performs a gain / offset calibration for a single pixel
     * NOT THREAD SAFE!
     * 
     * @param sample  the read sensor data
     * @param distance  the wall range (distance to flat object center point)
     * @param x_angle_off  x offset in radians from sample center (+ or -)
     * @param y_angle_off  y offset in radians from sample cneter (+ or -)
     */
    gain_t calibrate_pixel(const u16 near_sample, const u16 near_distance, const u16 far_sample, const u16 far_distance, const float x_angle_off, const float y_angle_off, UNUSED u16 dist) {

        PANIC_IF(true, "DEPRICATED!", 0);
        s16 expected_near = calculate_wall_distance(x_angle_off, y_angle_off, near_distance);
        s16 expected_far = calculate_wall_distance(x_angle_off, y_angle_off, far_distance);


        s16 actual_diff = expected_far - expected_near;
        s16 measured_diff = far_sample - near_sample;


        //float vgain = vertical_gain_multiplier(y_angle_off * 57.2958f);
        //return average_gain(dist, 0, 0, false);


//(1083 / 1020) (985), (2068 / 1805) (1743) (-0.040990, -0.263387)
//[0.796954 / 219.898453]    // 


// TODO: continue here and set gain to average_gain(i, 0,0,0 false)
        static gain_t g = {0.0f, 0.0f};

/*
        //float avg_offset = ((far_sample * vgain)- expected_far) + ((near_sample * vgain) - expected_near) / 2;
        float offset = (near_sample * vgain) - expected_near;


        //g.gain = (float)actual_diff / (float)measured_diff;
        //g.offset = near_sample - (float)(g.gain * near_sample);
        //g.offset = (near_sample * vgain) - expected_near;
        //g.gain = expected_far / (far_sample - g.offset);
        g.offset = expected_near - near_sample;
        g.gain = 1.0f;//vgain;//1.0f;//ADI_GAIN;
        //g.offset = -offset;
        


        // Legacy diagnostics (part of the disabled code above).
        if (std::abs(y_angle_off) > 0.5f) {
            printf("(%d / %d) (%d), (%d / %d) (%d) (%f, %f)\n", near_sample, expected_near, near_distance, far_sample, expected_far, far_distance, x_angle_off, y_angle_off);
            printf("[%f / %f]\n", g.gain, g.offset);
        }
        */

        //float slope2 = ADI_GAIN * g.ain;

        return g;
    }

/*
        printf("%dx%d [%d]  -  [%d, %d] (%d, %d) da:%d, dm:%d,   slope: %f,  offset: %f\n", x, y, i, 
            calibration[i].near_dist, calibration[i].near_point, 
            calibration[i].far_dist, calibration[i].far_point, 
            actual_diff, measured_diff, slope1,  slope2, offset);
            */



    void process_old_calibration() {
        INFO("processing old calibration data");
        for (int y=0; y<CAM_HEIGHT; y++) {
            for (int x=0; x<CAM_WIDTH; x++) {
                float angle = gfx::pixel_to_radians(xy_t{(s16)GOPTS->center_x, (s16)GOPTS->center_y}, xy_t{x, y});
                VixelCalibration *vixel = gfx::find_calibration_for_radian(angle);
                u16 dist = core::calc_distance(GOPTS->center_x, GOPTS->center_y, x, y);
                int offset = (y * CAM_WIDTH) + x;
                pixel_gain[offset] = vixel->gain;// * vert_cos[offset];
                pixel_offset[offset] = vixel->offset;
                /*
                if (y == 420) {
                    printf("XXX! %d, %d, %f,  #%d  G:%f,  O: %d\n", x, y, angle, vixel->vixel, vixel->gain, vixel->offset);
                }
                */
                //delete vixel; // free dynamically allocated memory...
            }
        }

/*
        for (int i=0; i<CAM_WIDTH * CAM_HEIGHT; i++) {
            float angle = pixel_angle[i];
            VixelCalibration *vixel = gfx::find_calibration_for_radian(angle);
            pixel_gain[i] = vixel->gain;// * vert_cos[i];
            pixel_offset[i] = vixel->offset;
            if (i % 1024 == 0) {
                printf("%f / %f\n", pixel_gain[i], pixel_offset[i]);
            }
        }
        */

        INFO("calibration data loaded");
    }


    void process_calibration() {
        calibration::point *p = get_calibration();

        for (int i=0; i<CAM_WIDTH*CAM_HEIGHT; i++) {

            pixel_gain[i] = 1.0f;
            pixel_offset[i] = 0.0f;

            //if (p[i].center_angle == 0) {
                //INFO("center angle 0");
            //    continue; }
            //if (pixel_dist[i] == 0) {
                //INFO("pixel dist 0");
            //    continue; }
            if (calibration[i].far_point == 0 || calibration[i].near_point == 0) {
                //INFO("FP 0 / NP 0"); 
                continue; }

            //if (p[i].far_point > 2200 || p[i].far_point < 1400) { printf("CALIBRATION SAVE ERROR: [%d / %d] [%d / %d]\n", p[i].near_point, p[i].near_dist, p[i].far_point, p[i].far_dist); continue; }
            //printf("angle: [%f]\n", calibration[i].center_angle);
            //printf("(%f / %f) [%d / %d]\n", pixel_angle[i], pixel_vert_radians[i], calibration[i].far_point, calibration[i].near_dist);
            //s16 expected_far = calculate_wall_distance(calibration[i].center_angle - pixel_angle[i], pixel_vert_radians[i], calibration[i].far_dist);
            //s16 expected_near = calculate_wall_distance(calibration[i].center_angle - pixel_angle[i], pixel_vert_radians[i], calibration[i].near_dist);
            // CONTINUE HERE: print some debug info
            //gain_t *g = average_gain(i, expected_far, calibration[i].far_point, expected_near, calibration[i].near_point);


            const int inrange = int(calibration[i].far_point) - calibration[i].near_point;
            const int outrange = int(calibration[i].far_dist) - calibration[i].near_dist;
            // Incomplete/degenerate samples must not generate NaN/Inf coefficients.
            if (inrange <= 0 || outrange <= 0) continue;

            //float slope1 = (float)actual_diff / (float)measured_diff;
            //float offset = (calibration[i].near_point * slope1) //calibration[i].near_point - (float)(slope1 * measured_diff);


            float slope3 = (float)outrange / (float)inrange;
            float offset = calibration[i].near_dist - (float)(slope3 * calibration[i].near_point);
            pixel_gain[i] = slope3;
            pixel_offset[i] = offset;


/*
            float slope1 = ((float)calibration[i].near_dist / (float)calibration[i].near_point);
            float slope2 = ((float)calibration[i].far_dist / (float)calibration[i].far_point);
            //float offset = 0.0f;


            pixel_gain[i] = (slope1 + slope2) / 2; //g->gain;
            if (pixel_gain[i] < 1.0f) {
                pixel_gain[i] = 1.0f;
            }
            pixel_offset[i] = 0;//calibration[i].near_dist - (calibration[i].near_point * pixel_gain[i]);


            int y = floor(i/640);
            int x = i - y*640;

            float adj = (float)calibration[i].near_point * pixel_gain[i];
            */


            if (pixel_dist[i] == 232) {
                //INFO("(s1:%f, s2:%f, s3:%f, %d) %f [%f / %f]", slope1, slope2, slope3, offset, pixel_vert_radians[i], 
                //((calibration[i].near_point * pixel_gain[i]) - calibration[i].near_dist),
                //((calibration[i].far_point * pixel_gain[i]) - calibration[i].far_dist));
                INFO("L(%d, %d) H(%d, %d) = %f, %f", 
                    calibration[i].near_dist, calibration[i].near_point,
                    calibration[i].far_dist, calibration[i].far_point,
                    static_cast<double>(pixel_gain[i]), static_cast<double>(pixel_offset[i]));
            }

/*
            INFO("[%d/%d]@%d calc (%f/%d) = (%f) (%f)", calibration[i].near_dist, calibration[i].near_point, i, 
                adj,
                calibration[i].near_dist, pixel_gain[i], pixel_offset[i]);
                */
        }

        return;

        // PANIC_IF(true, "check", 0);

        for (int i=0; i<CAM_WIDTH*CAM_HEIGHT; i++) {
            if (p[i].far_point == 0) { continue; }// || calibration[i].near_point == 0) { continue; }// || calibration[i].center_angle == 0) { continue; }
            if (p[i].center_angle == 0) { continue; }
            if (pixel_dist[i] == 0) { continue; }

            int y = floor(i/640);
            int x = i - y*640;

    //calibration::point *p = calibration::get_calibration();
    //p[2].near_dist = 123;
    /*
            s16 actual_diff = calibration[i].far_dist - calibration[i].near_dist;
            s16 measured_diff = calibration[i].far_point - calibration[i].near_point;

            float slope1 = (float)actual_diff / (float)measured_diff;
            float slope2 = ADI_GAIN * slope1;
            float offset = calibration[i].near_point - (float)(slope1 * measured_diff);
            */


/*
            if (std::abs(pixel_vert_radians[i]) > 0.5f) {
                printf("%dx%d\n", x, y);
            }
*/

            gain_t g = calibrate_pixel(calibration[i].near_point, calibration[i].near_dist, calibration[i].far_point, calibration[i].far_dist, pixel_angle[i] - calibration[i].center_angle, pixel_vert_radians[i], pixel_dist[i]);
            pixel_offset[i] = g.offset;
            pixel_gain[i] = g.gain;

            //printf("%dx%d [%d]  -  %f / %f (%f , %f)\n", x, y, i, pixel_angle[i], pixel_vert_radians[i], g.gain, g.offset);
        }
    }


    float vertical_gain_multiplier(float vertical_angle) {
        float gain_vert = 1.0f;

        if (vertical_angle < 0.0f) {
            gain_vert = core::value_map(vertical_angle, 0, -15.0f, 1.0f, 0.96620f);
        } else {
            gain_vert = core::value_map(vertical_angle, 0, 45.0f, 1.0f, 0.7250f);
        }




/*
        if (vertical_angle < -5.7f) {
            //gain_vert = core::value_map(vertical_angle, -5.7f, -15.0f, 0.99055f, 0.96620f);
            gain_vert = core::value_map(vertical_angle, -15.0f, -5.7f, 0.96620f, 0.98055f);
        } else if (vertical_angle < 0.0f) {
            //gain_vert = core::value_map(vertical_angle, -5.7f, 0, 0.99055f, 1.0f);
            gain_vert = core::value_map(vertical_angle, 0, -5.7f, 1.0f, 0.99055f);
        } else if (vertical_angle < 6.4f) {
            gain_vert = core::value_map(vertical_angle, 0, 6.4f, 1.0f, 1.0f);
        } else if (vertical_angle < 16.14f) {
            gain_vert = core::value_map(vertical_angle, 6.4f, 16.14f, 1.0f, 0.97126f);
        } else if (vertical_angle < 22.32f) {
            gain_vert = core::value_map(vertical_angle, 16.14f, 22.32f, 0.97126f, 0.96480f);
        } else if (vertical_angle < 27.52f) {
            gain_vert = core::value_map(vertical_angle, 22.32f, 27.52f, 0.96480f, 0.95573f);
            //gain_vert = core::value_map(vertical_angle, 22.32f, 27.52f, 0.96480f, 0.94573f);
        } else if (vertical_angle < 30.97f) {
            gain_vert = core::value_map(vertical_angle, 27.52f, 30.97f, 0.95573f, 0.94680f);
            //gain_vert = core::value_map(vertical_angle, 27.52f, 30.97f, 0.94573f, 0.92680f);
        } else if (vertical_angle < 35.25f) {
            gain_vert = core::value_map(vertical_angle, 30.97f, 35.25f, 0.94680f, 0.917350f);
            //gain_vert = core::value_map(vertical_angle, 30.97f, 35.25f, 0.92680f, 0.89f);
        } else if (vertical_angle < 39.16f) {
            gain_vert = core::value_map(vertical_angle, 35.25f, 39.16, 0.917350f, 0.868947f);
            //gain_vert = core::value_map(vertical_angle, 35.25f, 39.16, 0.89f, 0.81f);
        } else if (vertical_angle < 42.36f) {
            //gain_vert = core::value_map(vertical_angle, 39.16, 42.36f, 0.868947f, 0.76680f);
            gain_vert = core::value_map(vertical_angle, 39.16, 42.36f, 0.868947f, 0.77680f);
            //gain_vert = core::value_map(vertical_angle, 39.16, 42.36f, 0.868947f, 0.65);
        } else {
            //gain_vert = core::value_map(vertical_angle, 42.36f, 45.0f, 0.76680f, 0.7250f);
            gain_vert = core::value_map(vertical_angle, 42.36f, 45.0f, 0.77680f, 0.7250f);
            //gain_vert = core::value_map(vertical_angle, 42.36f, 44.16f, 0.77680f, 0.765f);
            //gain_vert = core::value_map(vertical_angle, 42.36f, 44.5f, 0.72f, 0.65f);
        }
        */

        return gain_vert;
    }

    unsigned long gain_mapping(JSAMPLE *__restrict__ jpeg_data) {

        const float *__restrict__ gain = calibration::get_pixel_gain();
        const float *__restrict__ offset = calibration::get_pixel_offset();

        uint rgb_off = 0;
        for (int j=0; j<num_runlines; j++) {
            rgb_off = runlines[j].srcstart * 3;
            for (uint i=runlines[j].srcstart; i<runlines[j].runlen;i++) {
                jpeg_data[rgb_off++] = (128 + ((1.1f - gain[i])*255));
                jpeg_data[rgb_off++] = 0;
                jpeg_data[rgb_off++] = offset[i];
            }
        }
        return 0;
    }

    /**
     * @brief  find the closest wall angle in radians
     * 
     * @param data 
     * @return float 
     */
    float find_closest_wall_radian(const u16 *data, u16 target_depth_mm) {
        constexpr size_t bucket_count = FRAME_PLANAR_WIDTH_DEFAULT;
        constexpr size_t frame_size = CAM_WIDTH * CAM_HEIGHT;
        if (!data || !runlines || !pixel_angle_bucket || !angle_cos_vert_sin || num_runlines <= 6)
            return -1.0f;

        std::array<uint64_t, bucket_count> totals{};
        std::array<uint64_t, bucket_count> counts{};
        const float *z_factor = calibration::get_z_factor();

        // Keep the existing exclusion of the first/last three runs. Clamp both
        // the inclusive start and exclusive end to [0, frame capacity].
        for (size_t j = 3; j + 3 < num_runlines; ++j) {
            const size_t begin = std::min(static_cast<size_t>(runlines[j].srcstart), frame_size);
            const size_t end = std::min(static_cast<size_t>(runlines[j].runlen), frame_size);
            for (size_t i = begin; i < end; ++i) {
                if (data[i] == 0) continue;
                // Buckets are unsigned, so their floor is already zero. Clamp
                // the ceiling before any access (including malformed metadata).
                const size_t bucket = std::min(static_cast<size_t>(pixel_angle_bucket[i]), bucket_count - 1);
                const double projected = static_cast<double>(data[i]) * static_cast<double>(z_factor[i]);
                const double bounded = std::max(0.0, std::min(projected, 65535.0));
                if (bounded < 1.0) continue;
                totals[bucket] += static_cast<uint64_t>(bounded);
                ++counts[bucket];
            }
        }

        size_t closest_bucket = bucket_count;
        double closest_distance = std::numeric_limits<double>::max();
        for (size_t i = 0; i < bucket_count; ++i) {
            // Excluded angles/empty frames naturally leave empty buckets.
            if (counts[i] == 0) continue;
            const double distance = static_cast<double>(totals[i] / counts[i]) * 1.47;
            const double difference = std::abs(distance - static_cast<double>(target_depth_mm));
            if (difference < closest_distance) {
                closest_distance = difference;
                closest_bucket = i;
            }
        }
        if (closest_bucket == bucket_count) return -1.0f;
        // Preserve the existing angular conversion; this change bounds sampling.
        return static_cast<float>(closest_bucket) * ANGLE_POINTS_INC;
    }

    /*
    void each_pixel(void *fn, const u16 *__restrict__ src) {

        const float *__restrict__ gain = calibration::get_pixel_gain();
        const float *__restrict__ offset = calibration::get_pixel_offset();

        for (int j=0; j<num_runlines; j++) {
            uint srcstart = runlines[j].srcstart;
            uint runlen = runlines[j].runlen;

            for (uint i=srcstart; i<(runlen);i++) {
                fn(src[i])
            }
        }
    }
    */
}
