#include <sys/types.h>
#include <stdint.h>

#include <map>
#include <string>

#include "utils.hpp"
#include "shapes.h"
#include "demos.h"
#include "data.h"
#include "system.h"
#include "calibration.h"
#include "camera.h"
#include <chrono>

//extern std::vector<calibration_status> vixel_autocal_status;
//extern double **autocalibrated_values;

namespace data
{
    static std::map<std::string, int> int_mapping;
    static std::atomic<bool> frame_ready{false};
    static uint16_t *calibrated = NULL;
    static std::vector<offset_samp_t> planar_points_calibrated;
    static std::vector<offset_samp_t> planar_points_raw;
    static std::vector<offset_samp_t> polar_points_calibrated;
    static std::vector<offset_samp_t> polar_points_raw;
    static std::vector<offset_samp_t> slam_points_calibrated;
    static std::unordered_map<int, std::vector<offset_samp_t>> polar_points_map;
    static std::unordered_map<int, std::vector<offset_samp_t>> planar_points_map;
    static measurement::History points;
    static uint64_t point_sequence = 0;
    static measurement::Histories histories;
    static measurement::FrameMetadata metadata;
    static uint64_t configuration_revision = 1;

    static uint64_t steady_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    measurement::FrameMetadata frame_metadata() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        return metadata;
    }
    std::condition_variable &frame_changed() {
        static std::condition_variable changed;
        return changed;
    }
    uint64_t frame_age_ms() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        return metadata.id ? steady_ms() - metadata.received_steady_ms : 0;
    }
    void publish_frame(const measurement::FrameMetadata &frame) {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        metadata = frame;
        frame_ready = true;
        frame_changed().notify_all();
    }
    uint64_t measurement_revision() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        return configuration_revision;
    }
    void invalidate_measurements() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        ++configuration_revision;
    }
    measurement::HistoryResult sample_point(const std::string &client, const std::string &selection,
                                            const measurement::FrameMetadata &frame, uint16_t value) {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        return histories.sample(client, selection, frame, value, steady_ms());
    }
    void select_measurement(const std::string &client, const std::string &selection) {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        histories.select_only(client, selection, steady_ms());
    }
    static u32 point_cloud_sz = 0;
    static u32 beta = 0X0C0C0C0C;

    static u32 bucket_counts[(ANGLE_POINTS +20) * BUCKET_SZ_RAW * BUCKET_FRAMES];
    static u32 bucket_totals[(ANGLE_POINTS +20) * BUCKET_SZ_RAW * BUCKET_FRAMES];
    static u32 tal_saturation[20000];//TAL_LANES+1 * TAL_BUCKETS+1];

    static u16 *smoothed = NULL;
    static u16 *slam = NULL;
    static u16 *planar_idx = NULL;

    static u8 *point_cloud_compressed = NULL;
    static s16 *point_cloud = NULL;
    static float *point_cloud_f = NULL;
    
    /**
     * clear the stdd list
     */
    void clear_points() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        points.clear();
        point_sequence = 0;
    }
    
    u32 get_beta() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        return beta;
    }
    
    void set_beta(u32 beta_value) {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        beta = beta_value;
    }

    u16 *get_planar_idx() {
        return planar_idx;
    }
    
    /**
     * add a new element (auto wraps at POINT_SZ elements)
     */
    void add_point(u16 value) {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        points.add(value, ++point_sequence);
    }
    
    /**
     * return the current stdd of all points
     */
    float point_stdd() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        return points.result().statistics.stdd;
    }

    /**
     * return average of all points
     */
    u16 point_mean() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        return static_cast<u16>(points.result().statistics.mean);
    }

    /**
     * return the difference between min and max points
     */
    u16 point_range() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        const auto statistics = points.result().statistics;
        return statistics.maximum - statistics.minimum;
    }
    
    /**
     * return pointer to the bucket counter for modulo of current frame
     */
    u32* get_tal_buckets() {
        return tal_saturation;
    }

    
    /**
     * return pointer to the bucket counter for modulo of current frame
     */
    u32* get_bucket_counts(int frame) {
        return &bucket_counts[((ANGLE_POINTS +20) * BUCKET_SZ_RAW) * (frame % BUCKET_FRAMES)];
    }

    /**
     * return pointer to the bucket counter for modulo of current frame
     */
    u32* get_bucket_totals(int frame) {
        return &bucket_totals[((ANGLE_POINTS +20) * BUCKET_SZ_RAW) * (frame % BUCKET_FRAMES)];
    }


    void init_defaults() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        TRACE("begin");
        int_mapping["frame_type"] = FRAME_TYPE_DEPTH;
        int_mapping["frame_format"] = FRAME_FORMAT_POLAR;
        int_mapping["frame_width"] = FRAME_POLAR_WIDTH_DEFAULT;
        int_mapping["frame_height"] = FRAME_POLAR_HEIGHT_DEFAULT;
        int_mapping["gain_correct"] = 1;
        frame_ready = false;

        calibrated = (uint16_t *)NEWMEM(uint16_t, 1500 * 1500); //FRAME_MAX_WIDTH * FRAME_MAX_HEIGHT);
        smoothed = (uint16_t *)NEWMEM(uint16_t, CAM_WIDTH * CAM_HEIGHT * 4); //FRAME_MAX_WIDTH * FRAME_MAX_HEIGHT (x2 for good measure)); 
        planar_idx = (uint16_t *)NEWMEM(uint16_t, CAM_WIDTH * CAM_HEIGHT * sizeof(uint16_t));
        slam = calibration::make_slam_buffer();

        set_frame_format(FRAME_FORMAT_POLAR);
        
        clear_points();
        histories.clear();
        metadata = {};
        configuration_revision = 1;
    
        point_cloud_compressed = (u8 *)NEWMEM(u8, 640*480*3 * 2);

        point_cloud = (s16 *)NEWMEM(s16, 640*480*3);
        point_cloud_f = (float *)NEWMEM(float, 640*480*3);

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

    u8 *get_point_cloud_compressed() {
        return point_cloud_compressed;
    }
    
    s16 *get_point_cloud() {
        return point_cloud;
    }

    u16 *get_slam() {
        return slam;
    }
    
    void set_point_cloud_size(const int size) {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        point_cloud_sz = size;
    }

    int get_point_cloud_size() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        return point_cloud_sz;
    }

    float *get_point_cloud_f() {
        return point_cloud_f;
    }
    


    /**
     * get the current mapping vector based on the current frame_format
     */
    //std::vector<offset_samp_t>* get_point_mapping() {
    std::unordered_map<int, std::vector<offset_samp_t>>* get_point_mapping() {
        
        // if (int_mapping["frame_format"] != FRAME_FORMAT_POLAR) { printf("PLANAR!\n"); }
        return (int_mapping["frame_format"] == FRAME_FORMAT_POLAR) ? &polar_points_map : &planar_points_map;
        /*

        if (int_mapping["frame_format"] == FRAME_FORMAT_PLANAR) {
            printf("planar\n");
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
        */
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

    uint16_t *get_smooth_ptr() {
        return smoothed;
    }


    /**
     * set the type of capture frame DEPTH or IR
     */
    bool set_frame_type(uint type) {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        Return_If((type != FRAME_TYPE_DEPTH) && (type != FRAME_TYPE_IR), 
        "frame type must be a defined type", false);

        int_mapping["frame_type"] = type;
        return true;
    }

    /**
     * enable / disable chiton gain correction for RAW -> mm values
     */
    void set_chiton_calibration(bool onoff) {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        int_mapping["gain_correct"] = (onoff) ? 1 : 0;
    }

    /**
     * return true if chiton gain correction is enabled
     */
    bool get_chiton_calibration() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        return int_mapping["gain_correct"] == 1;
    }

    /**
     * get the currently active frame type
     */
    uint get_frame_type() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
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
    
    void set_frame_dim(int width, int height) {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        int_mapping["frame_width"] = width;
        int_mapping["frame_height"] = height;
    }
    
   /**
     * set the frame format and update the mappings
     */
    bool set_frame_format(uint format) {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        WARN("[%d]", format);
        Return_If((format != FRAME_FORMAT_PLANAR) && (format != FRAME_FORMAT_POLAR), 
        "frame format must be a defined type", false);

        /*
        std::vector<offset_samp_t> tmp_points = gfx::make_all_polar_points_int2();
        polar_points_map = gfx::order_samples(tmp_points);
        planar_points_map = gfx::make_all_planar_points_int();
        
        return true;
        */
        
        std::lock_guard<std::mutex> guard(GOPTS->point_lock);
        int_mapping["frame_format"] = format;
        if (format == FRAME_FORMAT_PLANAR) {
            INFO("FORMAT PLANAR");
            int_mapping["frame_width"] = FRAME_PLANAR_WIDTH_DEFAULT;
            int_mapping["frame_height"] = (GOPTS->inner_dist > 0) ? (GOPTS->outer_dist - GOPTS->inner_dist) : FRAME_PLANAR_HEIGHT_DEFAULT;

            //std::vector<offset_samp_t> make_all_slam_points_int();
            std::vector<offset_samp_t> tmp_points = gfx::make_all_polar_points_int2();
            planar_points_map = gfx::order_samples(tmp_points);
            //gfx::make_all_slam_points_int();
            //data::
            //planar_points_calibrated = gfx::make_all_planar_points_int();
            //planar_points_raw = make_0_gain(&planar_points_calibrated);
            //int_mapping["frame_width"] = planar_points_map.size();// / FRAME_PLANAR_HEIGHT_DEFAULT;
        } else if (format == FRAME_FORMAT_POLAR) {
            int_mapping["frame_width"] = FRAME_POLAR_WIDTH_DEFAULT;
            int_mapping["frame_height"] = FRAME_POLAR_HEIGHT_DEFAULT;
            std::vector<offset_samp_t> tmp_points = gfx::make_all_polar_points_int2();
            polar_points_map = gfx::order_samples(tmp_points);
            //polar_points_calibrated = gfx::make_all_raw_points_int();
            //polar_points_raw = make_0_gain(&polar_points_calibrated);
        }
        /* else if (format == FRAME_FORMAT_SLAM) {
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
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        return int_mapping["frame_format"];
    }

    /**
     * get the width of the output frame
     */
    uint get_frame_width() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        return int_mapping["frame_width"];
    }

    /**
     * get the width of the output frame
     */
    uint get_frame_height() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        return int_mapping["frame_height"];
    }
    
    void set_frame_width(int height) {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        int_mapping["frame_width"] = height;
    }

    /**
     * get the number of bytes in the frame
     */
    uint get_frame_size_bytes() {
        std::lock_guard<std::recursive_mutex> owner(cam::state_mutex());
        return int_mapping["frame_height"] * int_mapping["frame_width"] * sizeof(uint16_t);
    }


    /**
     * return true if camera data is ready (or camera thread has exited...)
     */
    bool is_ready() {
        return frame_ready.load();
    }

    /**
     * set if data is ready (or camera thread has exited...)
     */
    void set_ready(bool ready) {
        frame_ready = ready;
    }
    
        
}