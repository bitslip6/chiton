#include <sys/types.h>
#include <stdint.h>

#include <map>
#include <string>

#include "utils.hpp"
#include "shapes.h"
#include "demos.h"
#include "data.h"
#include "system.h"

//extern std::vector<calibration_status> vixel_autocal_status;
//extern double **autocalibrated_values;

namespace data
{
    static std::map<std::string, int> int_mapping;
    static uint16_t *calibrated = NULL;
    static std::vector<offset_samp_t> planar_points_calibrated;
    static std::vector<offset_samp_t> planar_points_raw;
    static std::vector<offset_samp_t> polar_points_calibrated;
    static std::vector<offset_samp_t> polar_points_raw;
    static std::vector<offset_samp_t> slam_points_calibrated;

    void init_defaults() {
        TRACE("begin");
        int_mapping["frame_type"] = FRAME_TYPE_DEPTH;
        int_mapping["frame_format"] = FRAME_FORMAT_PLANAR;
        int_mapping["frame_width"] = FRAME_PLANAR_WIDTH_DEFAULT;
        int_mapping["frame_height"] = FRAME_PLANAR_HEIGHT_DEFAULT;
        int_mapping["gain_correct"] = 1;
        int_mapping["is_ready"] = 0;

        calibrated = (uint16_t *)NEWMEM(uint16_t, FRAME_MAX_WIDTH * FRAME_MAX_HEIGHT);
        set_frame_format(FRAME_FORMAT_PLANAR);

/*
        for (int i = 0; i < 8; i++) {
            vixel_autocal_status.emplace_back(calibration_status::NONE);
        }

        autocalibrated_values = new double *[NUMBER_OF_CAL_DISTANCES];
        for(int i = 0; i < NUMBER_OF_CAL_DISTANCES; i++) {
            autocalibrated_values[i] = new double[NUMBER_OF_VIXELS]{};
        }
        */

        TRACE("fin");
    }

    /**
     * get the current mapping vector based on the current frame_format
     */
    std::vector<offset_samp_t>* get_point_mapping() {
        if (int_mapping["frame_format"] == FRAME_FORMAT_PLANAR) {
            if (int_mapping["gain_correct"] == 1) {
                return &planar_points_calibrated;
            } else {
                return &planar_points_raw;
            }
        } else if (int_mapping["frame_format"] == FRAME_FORMAT_POLAR) {
            if (int_mapping["gain_correct"] == 1) {
                return &polar_points_calibrated;
            } else {
                return &polar_points_raw;
            }
        } else if (int_mapping["frame_format"] == FRAME_FORMAT_SLAM) {
            return &slam_points_calibrated;
        }

        WARN("UNKNOWN POINT MAPPING, USING RAW");
        return &planar_points_raw;
    }

    /**
     * get a pointer to calibrated data memory
     */
    uint16_t *get_calibrated_ptr() {
        if (calibrated == NULL) {
            set_frame_format(FRAME_FORMAT_PLANAR);
        }
        return calibrated;
    }

    /**
     * set the type of capture frame DEPTH or IR
     */
    bool set_frame_type(uint type) {
        Return_If((type != FRAME_TYPE_DEPTH) && (type != FRAME_TYPE_IR), 
        "frame type must be a defined type", false);

        int_mapping["frame_type"] = type;
        return true;
    }

    /**
     * enable / disable chiton gain correction for RAW -> mm values
     */
    void set_chiton_calibration(bool onoff) {
        int_mapping["gain_correct"] = (onoff) ? 1 : 0;
    }

    /**
     * return true if chiton gain correction is enabled
     */
    bool get_chiton_calibration() {
        return int_mapping["gain_correct"] == 1;
    }

    /**
     * get the currently active frame type
     */
    uint get_frame_type() {
        return int_mapping["frame_type"];
    }

    /**
     * return a 0 gain version if input
     */
    std::vector<offset_samp_t> make_0_gain(std::vector<offset_samp_t> *input) {
        std::vector<offset_samp_t> gain0;
        for (offset_samp_t point : *input) {
            point.gain = 1.0f;
            point.gain_offset = 0;
            gain0.emplace_back(point);
        }

        return gain0;
    }

    /**
     * set the frame format and update the mappings
     */
    bool set_frame_format(uint format) {
        TRACE("[%d]", format);
        Return_If((format != FRAME_FORMAT_PLANAR) && (format != FRAME_FORMAT_POLAR), 
        "frame format must be a defined type", false);

        std::lock_guard<std::mutex> guard(GOPTS->point_lock);
        int_mapping["frame_format"] = format;
        if (format == FRAME_FORMAT_PLANAR) {
            int_mapping["frame_width"] = FRAME_PLANAR_WIDTH_DEFAULT;
            int_mapping["frame_height"] = FRAME_PLANAR_HEIGHT_DEFAULT;
            planar_points_calibrated = gfx::make_all_planar_points_int();
            planar_points_raw = make_0_gain(&planar_points_calibrated);
            int_mapping["frame_width"] = planar_points_calibrated.size() / FRAME_PLANAR_HEIGHT_DEFAULT;
        } else if (format == FRAME_FORMAT_POLAR) {
            int_mapping["frame_width"] = FRAME_POLAR_WIDTH_DEFAULT;
            int_mapping["frame_height"] = FRAME_POLAR_HEIGHT_DEFAULT;
            polar_points_calibrated = gfx::make_all_raw_points_int();
            polar_points_raw = make_0_gain(&polar_points_calibrated);
        }/* else if (format == FRAME_FORMAT_SLAM) {
            int_mapping["frame_height"] = 1;
            int_mapping["frame_width"] = FRAME_PLANAR_WIDTH_DEFAULT;
            slam_points_calibrated = gfx::make_all_slam_points_int();
        }*/

        INFO("frame size set to [%dx%d]", int_mapping["frame_width"], int_mapping["frame_height"]);
        return true;
    }

    /**
     * get the currently active frame type
     */
    uint get_frame_format() {
        return int_mapping["frame_format"];
    }

    /**
     * get the width of the output frame
     */
    uint get_frame_width() {
        return int_mapping["frame_width"];
    }

    /**
     * get the width of the output frame
     */
    uint get_frame_height() {
        return int_mapping["frame_height"];
    }

    /**
     * get the number of bytes in the frame
     */
    uint get_frame_size_bytes() {
        return int_mapping["frame_height"] * int_mapping["frame_width"] * sizeof(uint16_t);
    }


    /**
     * return true if camera data is ready (or camera thread has exited...)
     */
    bool is_ready() {
        return int_mapping["is_ready"] == 1;
    }

    /**
     * set if data is ready (or camera thread has exited...)
     */
    void set_ready(bool ready) {
        int_mapping["is_ready"] = (ready) ? 1 : 0;
    }


}