/******************************************************************************
 *
 * @file            main.cpp
 *
 * @brief           Main Chiton application front-end.
 *
 * @details
 *
 * @author(s)       Stephen Stauts, Cory Marsh
 *
 * @created         06/15/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2020 SWYM, LLC and BitSlip6. All Rights Reserved.
 *
 *****************************************************************************/

//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include <sys/cdefs.h>
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
#include <zlib.h>
#include <sys/ioctl.h>

#include "fplus/fplus.hpp"
#include <jpeglib.h>
// #include "app.hpp"
#include "image.h"
#include "util.h"
#include "utils.hpp"
#include "demos.h"
#include "system.h"
#include "system.hpp"
#include "webserver.h"
#include "shapes.h"
#include "data.h"
#include "camera.h"
#include "request_validation.h"
#include "frame_read.h"
#include "calibration.h"
#include "zlib.h"
#include "CL/cl.h"
#include "rover.h"
#include "color/hsv.h"
#include "display.h"
#include "preview.h"
#include "resources.h"
#include <chrono>
#include <ifaddrs.h>
#include <net/if.h>

#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <execinfo.h>

#include <iostream>
#include <fstream>
#include <png.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>

#include <aditof/camera.h>
#include <aditof/device_interface.h>
#include <aditof/frame.h>
#include <aditof/system.h>

#include "system.h"
#define PORT 80
#define CHITON_VER 94


// TODO: find a better place for these...
RGB8 *hsv_cache;
//uint16_t *calibrated = NULL;
uint16_t *raw_data1 = NULL;
unsigned char *jpeg_ptr = NULL;
unsigned char *jpeg_rgb = NULL;
unsigned long jpeg_len = 0;

int *src_ptr = NULL;
int mapping_count = 0;
int *dst_ptr = NULL;

// is calibration enabled
/*
int vixel_index = 0;
int autocal_samples = 0;
std::vector<calibration_status> vixel_autocal_status;
double **autocalibrated_values;
calibration_status autocal_status = calibration_status::NONE;
int n_samples = 100;
std::vector<xy_t> autocal_points;
*/

// TODO: move to common file for testing.  remove dead code
GlobalOpts *GOPTS = NULL;
std::shared_ptr<aditof::Camera> camera = NULL;
std::mutex preview_lock;
display::Samples preview_samples{};
bool preview_ready = false;


/**
 * get a function pointer from a dl loaded library
 */
void *getFunctionPointer(void *lib, const char *funcName)
{
    // Get the function pointer to the function
    void *fptr = dlsym(lib, funcName);
    if (!fptr)
    {
        fprintf(stderr, "Could not get function pointer for %s\n  error is: %s\n\n", funcName, dlerror());
        return NULL;
    }
    return fptr;
}

/**
 * @brief  UNUSED.  Zlib default compression.
 * 
 * @param in_data pointer to allocated memory to compress
 * @param in_data_size  size of in_data
 * @param out_data  pointer to allocated memory to store compressed data
 * @return int size of compressed data
 */
int compress_memory(u8 *in_data, size_t in_data_size, u8 *out_data)
{
    z_stream strm = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    strm.zalloc = 0;
    strm.zfree = 0;
    strm.next_in = in_data;
    strm.avail_in = strm.total_in = in_data_size;
    strm.next_out = out_data;
    strm.avail_out = strm.total_out = in_data_size;
    int deflate_res = Z_OK;

    deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    deflate_res = deflate(&strm, Z_FINISH);
    WARN_IFNOT(deflate_res == Z_OK, "error compressing data");
    int sz = strm.total_out;
    deflateEnd(&strm);
    return sz;
}


/**
 * @brief draw a line from inner_dist-1 to outer_dist+2 along angle radian
 * @todo move to shapes
 * 
 * @param radians angle in radian to draw line. 0 is top, counter clockwise to 2pi
 * @return std::vector<Point2D_t> of points along line
 */
std::vector<Point2D_t> draw_angle_line(float radians) {

    xyf_t p1 = gfx::calculate_circle_point_float(GOPTS->inner_dist-1, radians);
    xyf_t p2 = gfx::calculate_circle_point_float(GOPTS->outer_dist+2, radians);

    float x1 = p1.x + GOPTS->center_x;
    float x2 = p2.x + GOPTS->center_x;
    float y1 = p1.y + GOPTS->center_y;
    float y2 = p2.y + GOPTS->center_y;

    linef_t line2 = {x1, y1, x2, y2};

    std::vector<Point2D_t> points = gfx::line_to_point_vector(line2, 200);

    return points;
} 


/**
 * @brief plot a line in image data of color along each point of points
 * @todo move to shapes
 * 
 * @param points 
 * @param image_data 
 * @param color 
 */

void plot_points(std::vector<Point2D_t> points, uint16_t *image_data, uint16_t color) {
    for (Point2D_t p : points) {
        int index = (p.y * CAM_WIDTH) + p.x;
        image_data[index] = color;
    }
}


bool sample_line(std::vector<Point2D_t> points, int physical_dist, float radian) {

    const int samples = 20;
    static int last_sum = 1;
    static int this_sum = 0;
    int max_diff = 0;
    uint16_t *calibrated = data::get_calibrated_ptr();
    calibration::point *calibration = calibration::get_calibration();
    float *vert_angle = calibration::get_vert_angle();

    u16 *data = NULL;//NEWMEM(u16, points.size() * samples*2);
    int totals[samples] = {0};



    // direct return...
    for (Point2D_t p : points) {
        int index = (p.y * CAM_WIDTH) + p.x;
        //int avg_sample = data[idx++] / count;
        if (calibrated[index] > 250) {
            uint16_t calc_dist = physical_dist / sinf(1.571f - vert_angle[index]);
            if (physical_dist < 1024) {
                // if we have a sample for this same distance, then average them
                if (calibration[index].near_dist != 0 && calibration[index].near_dist == calc_dist) {
                    calibration[index].near_point += calibrated[index];
                    calibration[index].near_point /= 2;
                } else {
                    calibration[index].near_dist = calc_dist;
                    calibration[index].near_point = calibrated[index];
                }
            } else {
                if (calibration[index].far_dist != 0 && calibration[index].far_dist == calc_dist) {
                    calibration[index].far_point += calibrated[index];
                    calibration[index].far_point /= 2;
                } else {
                    calibration[index].far_dist = calc_dist;
                    calibration[index].far_point = calibrated[index];
                }
            }
            calibration[index].center_angle = radian;
        }
    }

    return true;





    int slice = 0;
    // take samples along the line, wait 33 ms between samples for frame capture to update
    for (int i=0; i<samples; i++) {
        int idx = i*points.size();
        totals[i] = 0;
        for (Point2D_t p : points) {
            int index = (p.y * CAM_WIDTH) + p.x;
            data[idx++] = calibrated[index];
            totals[i] += calibrated[index];
        }

        totals[i] /= points.size();
        // wait 1 frame...
        usleep(33333);
    }


    // find min/max samples
    int max = 0;
    int min = 9000;
    for (int i=0; i<samples; i++) {
        if (totals[i] > max) { max = totals[i]; }
        if (totals[i] < min) { min = totals[i]; }
    }


    // count samples closer to min/max
    int min_total, max_total = 0;
    for (int i=0; i<samples; i++) {
        int diff1 = abs(totals[i] - max);
        int diff2 = abs(totals[i] - min);
        if (diff1 < diff2) {
            max_total++;
        } else {
            min_total++;
        }
    }


    // include samples closer to min/max
    int count = 0;
    for (int i=0; i<samples; i++) {
        int diff1 = std::abs(totals[i] - max);
        int diff2 = std::abs(totals[i] - min);
        // INFO("diff1: %d, diff2: %d, total: %d, min: %d, max: %d, points: %d", diff1, diff2, totals[i], min, max, points.size());
        if (max_total > min_total) {
            if (diff1 < diff2) {
                int idx = samples*points.size();
                for (Point2D_t p : points) {
                    int index = (p.y * CAM_WIDTH) + p.x;
                    data[idx++] = calibrated[index];
                }
                count++;
            }
            else if (diff1 > diff2) {
                int idx = samples*points.size();
                for (Point2D_t p : points) {
                    int index = (p.y * CAM_WIDTH) + p.x;
                    data[idx++] = calibrated[index];
                }
                count++;
            }
        }
    }
    int sum_diff = std::abs(last_sum - min);
    INFO("samples [%d], max [%d], min [%d], last [%d/%d]count [%d]", samples, max, min, last_sum, sum_diff, count);

    // take the average of the samples
    int idx = samples*points.size();
    for (Point2D_t p : points) {
        int index = (p.y * CAM_WIDTH) + p.x;
        int avg_sample = data[idx++] / count;
        if (avg_sample > 500) {
            if (physical_dist < 1024) {
                calibration[index].near_dist = physical_dist / sinf(1.571f - vert_angle[index]);
                calibration[index].near_point = avg_sample;
            } else {
                calibration[index].far_dist = physical_dist / sinf(1.571f - vert_angle[index]);
                calibration[index].far_point = avg_sample;
            }
            calibration[index].center_angle = radian;
        }
    }

    //free(data);
    // sample is not within tolerance
    /*
    if (sum_diff > 4)  {
        WARN ("FAILED SAMPLE - last num");
        if (last_sum == 1 ) { last_sum = min; }
        // adjust last sum by 20% of the difference
        last_sum -= ((last_sum - min)*0.2f); 
        return false;
    }
    */
    last_sum = min;
    bool result = (max - min) < 5;
    if (!result) { WARN("FAILED SAMPLE, min/max"); }
    return result;
}


/**
 * camera functions
 * params vector of url parts 
 * socket a connected socket
 */
web::HttpResponse calibrate_fn(std::vector<std::string> params)
{
    if (params.size() < 2) throw std::invalid_argument("missing calibration action");
    const std::vector<std::string> actions = {"sample2", "save", "load", "process", "infill", "smooth"};
    if (std::find(actions.begin(), actions.end(), params[0]) == actions.end())
        return web::error_response(404, "unknown calibration action");
    std::unique_lock<std::recursive_mutex> guard(cam::state_mutex());
    request::count(params, params[0] == "sample2" ? 5 : 2);
    if (params[0] == "sample2") {
        request::integer(params[2], 400, 3000);
        request::integer(params[3], 0, 1023);
        request::number(params[4], 0, 62832);
    }
    static int last_sum = 1;
    static int this_sum = 0;
    static u16 all_points[1024];
    nlohmann::json response = {{"status", "ok"}, {"action", params[0]}};

    //INFO("calibrate_fn");

    if (params[0].compare(std::string("sample2")) == 0) {

        uint16_t *calibrated = data::get_calibrated_ptr();
        calibration::point *calibration = calibration::get_calibration();
        int physical_dist = utils::clamp(params[2], 400, 3000);
        int angle_idx = std::stoi(params[3]);
        float radian = std::stof(params[4]) / 10000.0f;
        GOPTS->calibrate_angle = radian;

        WARN("calibrating at angle %d @%d, radian(%f)", angle_idx, physical_dist, static_cast<double>(radian));

        std::vector<Point2D_t> points = draw_angle_line(radian);


        float *vert_angle = calibration::get_vert_angle();
        u16* dist = calibration::get_pixel_dist();

        sample_line(points, physical_dist, radian);


/*
        this_sum = 0;
        int max_diff = 0;
        int ctr = 0;
        for (Point2D_t p : points) {
            int index = (p.y * CAM_WIDTH) + p.x;
            // center angle
            if (dist[index] == 232) {
                if (calibrated[index] > physical_dist) {
                    PANIC("calibrating with chiton calibration enagled.  enable smoothing and disable chiton cal");
                }
            }
          
            // near calibration point
            if (physical_dist < 1024) {
                // calculate the hypotenuse (physical distance from the camera at vertical angle)
                // hypotenuse = b / sin(ang b);  vert_angle is angle a, so angle b is 90(1.571) - angle a.  physical_dist is b
                calibration[index].near_dist = physical_dist / sinf(1.571f - vert_angle[index]);
                calibration[index].near_point = calibrated[index];
                INFO("<1024  index %dx%d  -  (%d) / (%d)", p.x, p.y, calibration[index].near_dist, calibrated[index]);
            } else {
                calibration[index].far_dist = physical_dist / sinf(1.571f - vert_angle[index]);
                calibration[index].far_point = calibrated[index];
                INFO(">1024  index %dx%d  -  (%d) / (%d)", p.x, p.y, calibration[index].far_dist, calibrated[index]);
            }
            calibration[index].center_angle = radian;
            this_sum += calibrated[index];

            max_diff = std::max(max_diff, abs(calibrated[index] - all_points[ctr++]));
            all_points[ctr-1] = calibrated[index];
        }

        if (last_sum != 1) {
            int five_percent = this_sum * 0.03;
            int diff = std::abs(this_sum - last_sum);
            if (diff < five_percent) {
                INFO("calibration is good");
                response = "{\"status\":\"ok\", \"value\": " + std::to_string(this_sum) + ", \"max_diff\": " + std::to_string(max_diff) + "}";
            } else {
                PANIC("calibration is bad");
                response = "{\"status\":\"fail\", \"value\": " + std::to_string(this_sum) + "}";
            }
        }
        last_sum = this_sum;
        */

        /*
        for (Point2D_t p : points) {
            int index = (p.y * CAM_WIDTH) + p.x;
            calibrated[index] = 100;
        }
        */
    }
    else if (params[0].compare(std::string("save")) == 0) {
        WARN("saving calibration");
        utils::write_calibration(calibration::get_calibration(), utils::calibration_path(), CAM_WIDTH * CAM_HEIGHT);
    }
    else if (params[0].compare(std::string("load")) == 0) {
        WARN("loading calibration");
        utils::read_calibration(utils::calibration_path());
        calibration::process_calibration();
    }
    else if (params[0].compare(std::string("process")) == 0) {
        WARN("process calibration");
        calibration::process_calibration();
    }
    else if (params[0].compare(std::string("infill")) == 0) {
        WARN("infill calibration begin");
        calibration::fill_missing_calibration_data();
        WARN("infill calibration complete");
    } else if (params[0].compare(std::string("smooth")) == 0) {
        WARN("calibration smooth begin");
        calibration::point *p = calibration::get_calibration();
        int ksz = 1;
        std::unique_ptr<float, decltype(&free)> kernel(gfx::make_gausian_kernel(ksz), &free);
        gfx::apply_kernel(ksz, kernel.get(), p);
        calibration::process_calibration();
        WARN("calibration smooth complete");
    }


    if (params[0] != "save") data::invalidate_measurements();
    return web::json_response(200, response);
}



/**
 * camera functions
 * params vector of url parts 
 * socket a connected socket
 */
static nlohmann::json frame_json(const measurement::FrameMetadata &frame) {
    return {{"frame_id", frame.id}, {"captured_at_ms", frame.captured_at_ms},
            {"frame_age_ms", data::frame_age_ms()}, {"color_mode", frame.color_mode},
            {"current_mode", GOPTS->color_mode.load()},
            {"configuration_revision", frame.configuration_revision},
            {"frame_type", frame.depth ? "depth" : "ir"}, {"units", frame.units()},
            {"calibration_applied", frame.calibrated}, {"smoothed", frame.smoothed},
            {"plugin_applied", frame.plugin}, {"units_scope", frame.calibrated ? "calibration_aoi" : "full_sensor"},
            {"invalid_sample_policy", "all_uint16_included"}};
}
static web::HttpResponse frame_response(web::HttpResponse response, const measurement::FrameMetadata &frame,
                                        int width, int height, const std::string &layout,
                                        const std::string &units) {
    const uint16_t endian = 1;
    response.headers = {{"Cache-Control", "no-store"},
        {"X-Chiton-Frame-Id", std::to_string(frame.id)},
        {"X-Chiton-Frame-Age-Ms", std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() - frame.received_steady_ms)},
        {"X-Chiton-Color-Mode", std::to_string(frame.color_mode)},
        {"X-Chiton-Captured-At-Ms", std::to_string(frame.captured_at_ms)},
        {"X-Chiton-Frame-Type", frame.depth ? "depth" : "ir"},
        {"X-Chiton-Width", std::to_string(width)}, {"X-Chiton-Height", std::to_string(height)},
        {"X-Chiton-Layout", layout}, {"X-Chiton-Units", units},
        {"X-Chiton-Source-Units", frame.units()},
        {"X-Chiton-Units-Scope", frame.calibrated ? "calibration_aoi" : "full_sensor"},
        {"X-Chiton-Configuration-Revision", std::to_string(frame.configuration_revision)},
        {"X-Chiton-Invalid-Sample-Policy", "all_uint16_included"},
        {"X-Chiton-Byte-Order", layout == "jpeg" || layout == "obj_xyz_divided_by_6000" ? "not_applicable" :
            *reinterpret_cast<const uint8_t *>(&endian) ? "little" : "big"}};
    return response;
}

web::HttpResponse camera_fn(std::vector<std::string> params, const std::string &query)
{
    request::count(params, 2);
    const bool frame_read = params[0] == "frame" || params[0] == "raw" || params[0] == "image" ||
        params[0] == "pointcloud" || params[0] == "pointcloud_obj";
    const auto options = frame_read ? web::frame_read(query) : web::FrameRead{};
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(options.timeout_ms);
    std::unique_lock<std::recursive_mutex> guard(cam::state_mutex(), std::defer_lock);
    // Ownership contention shares the frame-wait budget, including reacquisition
    // after a notification. Never block indefinitely behind an SDK call.
    const auto acquire_frame = [&]() {
        while (!guard.try_lock()) {
            if (!GOPTS->enable || GOPTS->running <= 0) return 503;
            if (std::chrono::steady_clock::now() >= deadline) return 504;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return 0;
    };
    if (options.newer) {
        const int error = acquire_frame();
        if (error) return web::error_response(error, "frame ownership unavailable before deadline");
    } else guard.lock();

    if (params[0] == "enable" || params[0] == "disable")
        return cam::set_capture(params[0] == "enable");

    const std::vector<std::string> actions = {"raw", "pointcloud", "pointcloud_obj", "image", "dimension", "wall_radian", "frame"};
    if (std::find(actions.begin(), actions.end(), params[0]) == actions.end())
        return web::error_response(404, "unknown camera action");
    while (params[0] != "dimension") {
        if (!GOPTS->enable || GOPTS->running <= 0)
            return web::error_response(503, "capture disabled or shutting down");
        const bool cloud = params[0] == "pointcloud" || params[0] == "pointcloud_obj";
        if ((cloud && GOPTS->color_mode != 4) || (params[0] == "image" && GOPTS->color_mode == 4))
            return web::json_response(409, {{"error", cloud ? "select cloud mode (4) before exporting" : "JPEG unavailable in cloud mode"},
                                          {"status", 409}, {"current_mode", GOPTS->color_mode.load()}});
        const auto frame = data::frame_metadata();
        const bool compatible = !(cloud || params[0] == "image") || frame.color_mode == GOPTS->color_mode;
        if (data::is_ready() && compatible && (!options.newer || frame.id > options.after)) break;
        if (!options.newer) return web::error_response(503, "camera frame not ready in current mode");
        if (std::chrono::steady_clock::now() >= deadline)
            return web::error_response(504, "no newer compatible frame before deadline");
        // Wait on a request-local lock, not the camera lock: condition-variable
        // reacquisition of the latter could exceed the budget behind a SDK call.
        guard.unlock();
        std::mutex notification_mutex;
        std::unique_lock<std::mutex> notification(notification_mutex);
        data::frame_changed().wait_until(notification, std::min(deadline,
            std::chrono::steady_clock::now() + std::chrono::milliseconds(25)));
        const int error = acquire_frame();
        if (error) return web::error_response(error, "frame ownership unavailable before deadline");
    }

    if (params[0] == "frame") {
        auto body = frame_json(data::frame_metadata());
        body["width"] = CAM_WIDTH; body["height"] = CAM_HEIGHT;
        body["raw_layout"] = "processed_uint16_row_major";
        auto response = web::json_response(200, body);
        response.headers["Cache-Control"] = "no-store";
        return response;
    }
    if (params[0].compare(std::string("raw")) == 0)
    {
        DEBUG("send raw image");
        const uint16_t *calibrated = data::get_calibrated_ptr();
        std::vector<uint16_t> snapshot(calibrated, calibrated + CAM_WIDTH * CAM_HEIGHT);
        const auto metadata = data::frame_metadata();
        auto response = frame_response(web::binary_response("application/octet-stream", snapshot.data(), snapshot.size() * sizeof(uint16_t)),
            metadata, CAM_WIDTH, CAM_HEIGHT, "processed_uint16_row_major", metadata.units());
        // Fixed ASCII filenames avoid client-controlled header values. Keep
        // snapshot metadata and bytes unchanged; only suggest saving the export.
        response.headers["Content-Disposition"] = "attachment; filename=\"chiton-raw.bin\"";
        guard.unlock();
        return response;
    }
    else if (params[0].compare(std::string("pointcloud")) == 0)
    {
        TRACE("send pointcloud data [%d]", data::get_point_cloud_size());
        //float *pc = data::get_point_cloud_f();
        //bool okay = web::send_data(socket, std::string("application/octet-stream"), (unsigned char *)pc, data::get_point_cloud_size() * 4);

        s16 *pc = data::get_point_cloud();
        //u8 *data = data::get_point_cloud_compressed();
        //int size = compress_memory((u8*)pc, data::get_point_cloud_size() * 4, data);

        std::vector<s16> snapshot(pc, pc + data::get_point_cloud_size());
        const auto metadata = data::frame_metadata();
        auto response = frame_response(web::binary_response("application/octet-stream", snapshot.data(), snapshot.size() * sizeof(s16)),
            metadata, snapshot.size() / 4, 1, "int16_xyzd", metadata.units());
        response.headers["Content-Disposition"] = "attachment; filename=\"chiton-pointcloud.bin\"";
        guard.unlock();
        return response;
    }
    else if (params[0].compare(std::string("pointcloud_obj")) == 0)
    {
        s16 *pc = data::get_point_cloud();
        std::vector<s16> snapshot(pc, pc + data::get_point_cloud_size());
        const auto metadata = data::frame_metadata();
        guard.unlock();
        std::string body;
        for (size_t i = 0; i + 3 < snapshot.size(); i += 4) {
            char line[128];
            snprintf(line, sizeof(line), "v %.4f %.4f %.4f\n",
                     snapshot[i] / 6000.0, snapshot[i+1] / 6000.0, snapshot[i+2] / 6000.0);
            body += line;
        }
        auto response = frame_response(web::binary_response("text/plain; charset=utf-8", body.data(), body.size()),
            metadata, snapshot.size() / 4, 1, "obj_xyz_divided_by_6000", "normalized_6000_" + metadata.units());
        response.headers["Content-Disposition"] = "attachment; filename=\"chiton-pointcloud.obj\"";
        return response;
    }

    else if (params[0].compare(std::string("image")) == 0)
    {
        std::vector<unsigned char> snapshot;
        if (jpeg_ptr && jpeg_len) snapshot.assign(jpeg_ptr, jpeg_ptr + jpeg_len);
        const auto metadata = data::frame_metadata();
        guard.unlock();
        if (snapshot.empty()) return web::error_response(503, "JPEG frame unavailable in current mode");
        return frame_response(web::binary_response("image/jpeg", snapshot.data(), snapshot.size()), metadata,
            metadata.color_mode > 4 ? 1500 : CAM_WIDTH, metadata.color_mode > 4 ? 165 : CAM_HEIGHT,
            "jpeg", "image_intensity");
    }

    else if (params[0].compare(std::string("dimension")) == 0)
    {

        INFO("dimension! [%d] [%d]", GOPTS->center_x, GOPTS->center_y);
        return web::json_response(200, {{"inner_dist", GOPTS->inner_dist}, {"outer_dist", GOPTS->outer_dist},
                                       {"center_x", GOPTS->center_x}, {"center_y", GOPTS->center_y}});
    }
    else if (params[0].compare(std::string("wall_radian")) == 0)
    {
        float g_angle = calibration::find_closest_wall_radian(raw_data1, 600);
        INFO("wall radian: %f", static_cast<double>(g_angle));
        if (g_angle < 0) return web::error_response(404, "no usable wall found");
        return web::json_response(200, {{"radians", g_angle}});
    }
    /*
    else if (params[0].compare(std::string("slam")) == 0)
    {
        u16 *new_slam = slam+(slam_idx * SLAM_POINTS);
    }
    */
    return web::error_response(404, "unknown camera action");
}

/**
 * get the value of a pixel in the current data pointer 
 * TODO: move std deviation out of GOPTS and move it here
 */
web::HttpResponse value_fn(std::vector<std::string> params)
{
    std::unique_lock<std::recursive_mutex> guard(cam::state_mutex());
    std::string client;
    if (params.size() >= 3 && params[2] == "client") {
        if (params.size() != 6 && params.size() != 8) throw std::invalid_argument("invalid client value arguments");
        client = params[3];
        measurement::validate_client(client);
        params.erase(params.begin() + 2, params.begin() + 4);
    }
    if (params.size() != 4 && params.size() != 6) throw std::invalid_argument("invalid value arguments");
    if (params[0] != "get") return web::error_response(404, "unknown value action");
    const bool rectangle = params.size() == 6;
    if (rectangle && GOPTS->color_mode >= 5) throw std::invalid_argument("planar rectangles unsupported");
    auto point = web::url_point_to_2d(utils::slice(params, 2, 3), rectangle);
    std::shared_ptr<Point2D_t> end;
    if (rectangle) {
        end = web::url_point_to_2d(utils::slice(params, 4, 5), true);
        if (point->x == end->x || point->y == end->y) throw std::invalid_argument("empty rectangle");
        if (point->x > end->x) std::swap(point->x, end->x);
        if (point->y > end->y) std::swap(point->y, end->y);
    }
    if (!GOPTS->enable || !data::is_ready()) return web::error_response(503, "camera frame not ready or capture disabled");
    const auto frame = data::frame_metadata();
    const uint16_t *pixels = data::get_calibrated_ptr();
    std::string selection = (rectangle ? "roi:" : "point:") + std::to_string(point->x) + ":" + std::to_string(point->y);
    if (rectangle) selection += ":" + std::to_string(end->x) + ":" + std::to_string(end->y);
    selection += ":" + std::to_string(frame.configuration_revision) + ":" + frame.units() +
                 ":" + std::to_string(GOPTS->color_mode) + ":" + std::to_string(data::get_frame_format());
    measurement::Statistics statistics;
    measurement::HistoryResult history;
    if (rectangle) {
        // Spatial statistics of exactly one immutable, locked frame; no temporal/global median.
        statistics = measurement::statistics(get_window(data::get_calibrated_ptr(), CAM_WIDTH, CAM_HEIGHT,
            CAM_WIDTH * CAM_HEIGHT, point->x, point->y, end->x, end->y));
        point->x += (end->x - point->x) / 2;
        point->y += (end->y - point->y) / 2;
    }
    uint16_t value;
    if (GOPTS->color_mode < 5) value = pixels[point->y * CAM_WIDTH + point->x];
    else {
        const int destination = point->y * 1500 + point->x;
        const int *found = std::find(dst_ptr, dst_ptr + mapping_count, destination);
        if (found == dst_ptr + mapping_count) throw std::invalid_argument("unmapped pixel");
        value = pixels[src_ptr[found - dst_ptr]];
    }
    if (!client.empty()) {
        if (rectangle) data::select_measurement(client, selection);
        else { history = data::sample_point(client, selection, frame, value); statistics = history.statistics; }
    } else if (!rectangle) statistics = measurement::statistics({value});

    auto result = frame_json(frame);
    result["value"] = value;
    result["mean"] = statistics.mean;
    result["stdd"] = statistics.stdd * 1000.0; // Legacy milli-unit field.
    result["stddev"] = statistics.stdd;
    result["diff"] = statistics.maximum - statistics.minimum;
    result["min"] = statistics.minimum; result["max"] = statistics.maximum;
    result["rms"] = statistics.stdd; // Legacy mean-centered RMS.
    result["rms_about_zero"] = statistics.rms;
    result["1std_mean"] = statistics.filtered_mean;
    result["filtered_count"] = statistics.filtered_count;
    result["sample_count"] = statistics.count;
    result["statistics_kind"] = !rectangle && !client.empty() ? "temporal_observed_frames" : "spatial";
    result["history_reset"] = history.reset;
    if (!rectangle && !client.empty()) {
        result["history_capacity"] = measurement::history_capacity;
        result["first_frame_id"] = history.first_frame; result["last_frame_id"] = history.last_frame;
        result["first_captured_at_ms"] = history.first_time_ms; result["last_captured_at_ms"] = history.last_time_ms;
        result["temporal"] = statistics.stdd;
    }
    result["width"] = GOPTS->color_mode >= 5 ? 1500 : CAM_WIDTH;
    result["height"] = GOPTS->color_mode >= 5 ? 165 : CAM_HEIGHT;
    result["x"] = point->x; result["y"] = point->y;
    // Preserve the existing device-global preview marker, not shared statistical history.
    GOPTS->point.x = point->x; GOPTS->point.y = point->y;
    float rads = gfx::pixel_to_radians(xy_t{(s16)GOPTS->center_x, (s16)GOPTS->center_y}, xy_t{point->x, point->y});
    result["radians"] = ((rads >= 0) ? rads : (PI * 2) + rads) * 1000;
    auto response = web::json_response(200, result);
    response.headers["Cache-Control"] = "no-store";
    return response;
}


/**
 * auto calibration process
 */
void check_auto_calibration(u16 *raw)
{

    return;
    static int frame_ctr[NUMBER_OF_VIXELS + 10];
    static int accumulator[NUMBER_OF_VIXELS + 10];
    static int ctr;

    //if (!GOPTS->is_calibration_enabled)
    if (GOPTS->calibration_enabled > 1)
    {
        for (int i = 0; i < NUMBER_OF_VIXELS; i++)
        {
            frame_ctr[i] = accumulator[i] = 0;
        }
        return;
    }

    // printf("check: %d / %d\n");
    std::string dist = "";
    for (const auto &[index, cal] : *GOPTS->cal)
    {
        if (GOPTS->calibrate_angle > 0 && GOPTS->calibrate_angle != index)
        {
            continue;
        }

        xy_t p = gfx::calculate_circle_point(238, cal->radian);
        xy_t p2 = {(s16)std::clamp(p.x + GOPTS->center_x, 1, 640), (s16)std::clamp(GOPTS->center_y + p.y, 1, 480)};

        //printf("%d.%d %d.%d\n", p.x, p.y, p2.x, p2.y);

        // INFO("%f = %d.%d x %d.%d", cal->radian, p.x, p.y, p2.x, p2.y);
        //u16 avg_point = raw[(p2.y*FRAME_POLAR_WIDTH_DEFAULT)+p2.x];
        u16 avg_point = utils::calc_mean(get_window(raw, FRAME_POLAR_WIDTH_DEFAULT, CAM_HEIGHT, CAM_WIDTH * CAM_HEIGHT, p2.x - 5, p2.y - 5, p2.x + 5, p2.y + 5));
        //printf("avg point: %d\n", avg_point);
        dist += " . " + std::to_string(avg_point);

        u16 calibrated = avg_point * 1.38f;
        //printf("calibrate vixel: %d  EST: (%d/%d)  (%f/%f)\n", GOPTS->calibrate_vixel, avg_point, calibrated, GOPTS->far_calibration * 0.9f, GOPTS->far_calibration * 1.4f);
        if ((calibrated < GOPTS->near_calibration * .9f || calibrated > GOPTS->near_calibration * 1.4f) &&
            (calibrated < GOPTS->far_calibration * .9f || calibrated > GOPTS->far_calibration * 1.4f))
        {
            frame_ctr[index] = 0;
            accumulator[index] = 0;
            continue;
        }
        frame_ctr[index]++;
        if (frame_ctr[index] < 200)
        {
            if (frame_ctr[index] % 10 == 1)
            {
                printf("waiting... calibrate vixel [#%d]\n", index);
            }
            accumulator[index] = 0;
            ctr = 0;
        }
        else if (frame_ctr[index] >= 200 && frame_ctr[index] <= 300)
        {
            accumulator[index] += avg_point;
            ctr++;
            INFO("! calibrate sampling vixel:%d, [%d] @%dx%d = %d\n", ctr, index, p.x, p.y, avg_point);
        }
        else if (frame_ctr[index] == 301)
        {
            double avg_value = accumulator[index] / ctr;
            bool is_near = (std::abs(calibrated - GOPTS->near_calibration) < std::abs(calibrated - GOPTS->far_calibration)) ? true : false;
            WARN("!!! update calibration vixel:[%d] = sampled: [%f] avg_point: [%d] (%d) \n", index, avg_value, avg_point, calibrated);
            utils::update_json_calibration(index, is_near, (u16)round(avg_value));
        }
    }
}

/**
 * run the camera and convert data into jpeg data
 */
int run_camera()
{

    INFO("run camera");
    std::unique_lock<std::recursive_mutex> setup_guard(cam::state_mutex());
    aditof::System system;
    std::vector<std::shared_ptr<aditof::Camera>> cameras;
    std::vector<std::string> frameTypes;

    INFO("loading optional api.so");
    //void (*custom_frame)(uint16_t * frame, int width, int height, std::vector<AngleDist> &dist) = NULL;
    void (*custom_frame)(uint16_t * frame, float *horiz_angle, float *vert_angle, u16 *angle_bucket) = NULL;
    void (*set_runlines)(calibration::runline *lines, unsigned short num_lines) = NULL;

    auto plugin_path = resources::find("api.so");
    GOPTS->api_lib = plugin_path.empty() ? nullptr : dlopen(plugin_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (GOPTS->api_lib == NULL) {
        const char *error = plugin_path.empty() ? "not found in installed/local locations" : dlerror();
        WARN("optional api.so unavailable [%s]; camera will continue", error);
    } else {
        INFO("found api.so. loaded");
        //custom_frame = (void (*)(uint16_t * frame, int width, int height, std::vector<AngleDist> &distances))
        custom_frame = (void (*)(uint16_t * frame, float *horiz_angle, float *vert_angle, u16 *angle_bucket))
            getFunctionPointer(GOPTS->api_lib, "process_standard_frame");

        if (custom_frame == NULL) {
            INFO("process_standard_frame not found");
        } else {
            INFO("process_standard_frame callback found");
        }

        set_runlines = (void (*)(calibration::runline *lines, unsigned short num_lines))
            getFunctionPointer(GOPTS->api_lib, "set_runlines");

        if (set_runlines == NULL) {
            INFO("set_runlines not found");
        } else {
            INFO("set_runlines callback found");
            set_runlines(calibration::get_run_lines(), calibration::get_num_runlines());
        }

    }

    // DELETE ME
    /*
    std::vector<xy_t> inner_points = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, 1024, GOPTS->inner_dist);
    int zr = 0;
    for (const auto &p : inner_points)
    {
        INFO("inner point: (%d) - %d.%d", ++zr, p.x, p.y);
    }
    */

    // system initialize
    TRACE("camera sys init");
    utils::adi_call([&]() { return system.initialize(); });
    TRACE("camera get list");
    utils::adi_call([&]() {
                        auto status = system.getCameraList(cameras);
                        if (status != aditof::Status::OK || cameras.empty()) {
                            GOPTS->running = EXIT_ADI_ERROR;
                            return aditof::Status::GENERIC_ERROR;
                        }
                        camera = cameras.front();
                        return aditof::Status::OK;
                    });
    TRACE("camera init");
    utils::adi_call([&]() { return camera->initialize(); });
    //camera->getDevice()->writeAFERegisters()
    TRACE("camera get frame type");
    utils::adi_call([&]() {
                        camera->getAvailableFrameTypes(frameTypes);
                        return frameTypes.empty() ? aditof::Status::GENERIC_ERROR : aditof::Status::OK;
                    });

    // set frame type and mode
    TRACE("camera set frame type");
    utils::adi_call([&]() { return camera->setFrameType(frameTypes.front()); });
    TRACE("camera set mode");
    utils::adi_call([&]() { return camera->setMode(GOPTS->mode); });

    // turn off adi camera calibration and geometry correction
    INFO("adi geometry correction off");
    camera->setControl("geometry_correction", "off");
    camera->setControl("noise_reduction_threshold", std::to_string(GOPTS->noise_reduction));

    uint64_t frame_number = 0;
    std::vector<uint16_t> plugin_frame;
    INFO("camera running");
    jpeg_rgb = (unsigned char *)NEWMEM(uint8_t, data::get_frame_size_bytes() * 3);
    src_ptr = NEWMEM(int, 1500*165);
    dst_ptr = NEWMEM(int, 1500*165);
    INFO("jpeg_rgb bytes [%d]", data::get_frame_size_bytes() * 3);

    uint16_t *calibrated = data::get_calibrated_ptr();

    std::vector<AngleDist> distances = gfx::create_distance_map();

    aditof::Frame frame;
    u16 regs[4] = {0x4011, 0x4012, 0x4013, 0x4014};
    u16 values[4] = {0x0C, 0x0C, 0x0C, 0x0C};
    u32 beta_ctl = 0;
    int last_color_mode = 0;

    u16 *slam = calibration::make_slam_buffer();
    u16 slam_idx = 0;

    // get references to pixel angles and pixel buckets
    float *horiz_angle = calibration::get_horiz_angle();
    float *vert_angle = calibration::get_vert_angle();
    u16 *angle_bucket = calibration::get_pixel_bucket();

    data::set_frame_format(FRAME_FORMAT_POLAR);

    std::unordered_map<int, std::vector<offset_samp_t>>* mapping = data::get_point_mapping();


    mapping_count = calc_src_dst_points(src_ptr, dst_ptr, 1500, 165, 1500 * 165);
    const auto acquisition = cam::initialize_capture(GOPTS->enable, [&]() {
        return camera->requestFrame(&frame);
    });
    if (acquisition.status != 200) {
        std::cerr << "Chiton: camera acquisition initialization failed: " << acquisition.content << std::endl;
        GOPTS->running = EXIT_ADI_ERROR;
        return 1;
    }
    setup_guard.unlock();







    while (GOPTS->running > 0)
    {
#ifdef CHITON_ADITOF_STUB
        // Simulator-only deterministic stalled-publication fixture; never a SDK hang.
        const char *pause_file = std::getenv("ADITOF_STUB_PAUSE_FILE");
        if (pause_file && access(pause_file, F_OK) == 0) { usleep(10000); continue; }
#endif

        // if the camera is disabled, wait 1/30 second and loop again..
        if (GOPTS->enable == false) {
            usleep(33333);
            continue;
        }

        std::unique_lock<std::recursive_mutex> frame_guard(cam::state_mutex());
        if (!GOPTS->enable || GOPTS->running <= 0) continue;

        cam::run_with_lock([&]() {
                               // get a reference to the internal API data pointer
                               aditof::FrameDataType type = (GOPTS->depth_frame) ? aditof::FrameDataType::DEPTH : aditof::FrameDataType::IR;
                               TIMER2("request_frame", 500, utils::adi_call([&]() { return camera->requestFrame(&frame); }));
                               utils::adi_call([&]() { return frame.getData(type, &raw_data1); });
                               return 0;
                           });




/*
        if (GOPTS->dynamic_illumination && GOPTS->depth_frame)
        {
            if (((beta_ctl & 0x000000FF) != values[0]) ||
                ((beta_ctl & 0x0000FF00) != values[1]) ||
                ((beta_ctl & 0x00FF0000) != values[2]) ||
                ((beta_ctl & 0xFF000000) != values[3]))
            {
                values[0] = beta_ctl & 0x000000FF;
                values[1] = (beta_ctl & 0x0000FF00) >> 8;
                values[2] = (beta_ctl & 0x00FF0000) >> 16;
                values[3] = (beta_ctl & 0xFF000000) >> 24;

                camera->getDevice()->writeAfeRegisters(regs, values, 4);
            }
        }
        else if (values[0] != 0x0C || values[1] != 0x0C || values[2] != 0x0C || values[3] != 0x0C)
        {
            values[0] = 0x0C;
            values[1] = 0x0C;
            values[2] = 0x0C;
            values[3] = 0x0C;
            camera->getDevice()->writeAfeRegisters(regs, values, 4);
        }
       */ 



        // clear image buffer when switching from rgb to greyscale
        // TODO: 
        if (last_color_mode != GOPTS->color_mode) {
            INFO("CLEAR IMAGE BUFFER, new mode: %d", (int)GOPTS->color_mode);
            memset(jpeg_rgb, 0, CAM_WIDTH*CAM_HEIGHT*3);
            memset(calibrated, 0, CAM_WIDTH*CAM_HEIGHT*2);
        }


        // chiton cal enabled, AND we are a depth frame
        bool chitoncal = data::get_chiton_calibration() && GOPTS->depth_frame;
        measurement::FrameMetadata metadata;
        metadata.id = frame_number + 1;
        metadata.captured_at_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        metadata.received_steady_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        metadata.configuration_revision = data::measurement_revision();
        metadata.depth = GOPTS->depth_frame;
        metadata.calibrated = chitoncal && GOPTS->calibration_enabled <= 1;
        metadata.smoothed = GOPTS->smoothing;
        metadata.plugin = custom_frame != nullptr;
        metadata.color_mode = GOPTS->color_mode;
        // first calibration averaging sample, 0 out the calibrated buffer (may contain IR data)
        if (GOPTS->calibration_enabled == CALIBRATION_SAMPLE_FRAMES) { memcpy(calibrated, raw_data1, CAM_WIDTH*CAM_HEIGHT*2);}

        // do calibration and extended averaging on data until time to take the sample
        if (GOPTS->calibration_enabled > 1) {
            WARN("SAMPLING number [%d]", GOPTS->calibration_enabled);
            TIMER2("average", 5, calibration::average_frame(raw_data1, calibrated));
            GOPTS->calibration_enabled--;
        } else if (chitoncal) {
            TIMER2("calibrate", 240, calibration::calibrate_frame(raw_data1, calibrated));
        } else {
            TIMER2("average", 240, calibration::average_frame(raw_data1, calibrated));
        }

        
        // do calibration
        if (GOPTS->calibration_enabled == 1) {
            WARN("SAMPLING CALIBRATION!");
            TIMER2("calibrate_sample", 1, calibration::save_calibration_frame(calibrated, GOPTS->near_calibration, GOPTS->calibrate_angle));
            WARN("SAMPLING COMPLETE! [%d], [%f]", GOPTS->near_calibration, static_cast<double>(GOPTS->calibrate_angle));
            GOPTS->calibration_enabled--;
            //if (GOPTS->near_calibration > 1500) {
                calibration::process_calibration();
                data::invalidate_measurements();
                printf("CALIBRATION PROCESSED\n");
                calibration::point *p = calibration::get_calibration();
                try {
                    utils::write_calibration(p, utils::calibration_path(), CAM_WIDTH * CAM_HEIGHT);
                } catch (const std::exception &error) {
                    WARN("unable to save automatic calibration: %s", error.what());
                    GOPTS->set_message("calibration save failed");
                }
            //}
        }

        // Legacy callbacks see their 16-bit counter only on a private frame copy.
        // Ordinary edits are applied back; the sentinel pixel remains a measurement.
        if (custom_frame != NULL) {
            measurement::legacy_plugin(plugin_frame, calibrated, CAM_WIDTH * CAM_HEIGHT,
                static_cast<uint16_t>(frame_number), [&](uint16_t *pixels) {
                    custom_frame(pixels, horiz_angle, vert_angle, angle_bucket);
                });
        }
        ++frame_number;

        if (GOPTS->calibrate_angle != 0.0f) {
            //int index = GOPTS->calibrate_angle;
            std::vector<Point2D_t> points = draw_angle_line(GOPTS->calibrate_angle);
            //INFO("draw line at angle index: %f  [%d]", GOPTS->calibrate_angle, points.size());
            plot_points(points, raw_data1, 5500);
            //plot_points(points, raw_data1, 0);
            points.clear();
            points.shrink_to_fit();
        }


        // Publish an owned preview snapshot, never an SDK frame pointer.
        {
            std::lock_guard<std::mutex> lock(preview_lock);
            for (int y = 0; y < 190; ++y)
                std::copy_n(raw_data1 + (y + 280) * CAM_WIDTH + 160, display::width,
                            preview_samples.data() + y * display::width);
            preview_ready = true;
        }

        // calculate dynamic illumination value every 5 frames if enabled
        if (GOPTS->dynamic_illumination && GOPTS->depth_frame) {
            if (frame_number % 3 == 1) {
                // calculate slam dataset if we are not in slam output mode
                if (GOPTS->color_mode != 0) {
                    TIMER2("vector_slam", 180, calibration::vector_slam(calibrated, slam, slam_idx));
                }
                beta_ctl = calibration::dynamic_illumination(slam, slam_idx, beta_ctl);
           }
        }
        // if disabled, set to full illumination
        else {
            beta_ctl = 0x0C0C0C0C;
            // HANDLE reducing illumination on vixels not being calibrated
            /*
            if (GOPTS->calibrate_angle != 0.0f) {
                if (GOPTS->calibrate_angle > -3.14f) {
                    beta_ctl = 0x000C0C00;
                    if (GOPTS->calibrate_angle > -1.7f) { beta_ctl = 0x0000000C; }
                    else if (GOPTS->calibrate_angle > -2.4f) { beta_ctl = 0x000C000C; }
                    else if (GOPTS->calibrate_angle > -2.7f) { beta_ctl = 0x0000000C; }
                }
                else {
                    beta_ctl = 0x000C0C00;
                    if (GOPTS->calibrate_angle < -3.58f) { beta_ctl = 0x00000C00; }
                    else if (GOPTS->calibrate_angle < -4.02f) { beta_ctl = 0x0C000C00; }
                    else if (GOPTS->calibrate_angle < -4.46f) { beta_ctl = 0x0C000000; }
                }
            }
            */
        }

        // every 5 frames, update the illumination value on the camera
        if (frame_number % 3 == 1) {
            values[3] = beta_ctl & 0x000000FF;
            values[2] = (beta_ctl & 0x0000FF00) >> 8;
            values[1] = (beta_ctl & 0x00FF0000) >> 16;
            values[0] = (beta_ctl & 0xFF000000) >> 24;

            camera->getDevice()->writeAfeRegisters(regs, values, 4);
        }


        resolution_t planar_rez = {CAM_WIDTH, CAM_HEIGHT};

        if (GOPTS->color_mode == 6) {
            INFO("create color planar ");
            planar_rez = {1500, 165};
            TIMER2("create_planar_color_jpeg", 420, create_planar_color_jpeg(raw_data1, jpeg_rgb, src_ptr, mapping_count));
        }
        else if (GOPTS->color_mode == 5) {
            planar_rez = {1500, 165};
            TIMER2("create_planar_jpeg", 420, create_planar_jpeg(raw_data1, jpeg_rgb, src_ptr, dst_ptr, mapping_count));
        }
        else if (GOPTS->color_mode == 4) {
            TIMER2("point_cloud", 420, calibration::vector_point_cloud(calibrated));
        }
        // GREYSCALE IMAGE
        else if (GOPTS->color_mode == 1) {
            TIMER2("greyscale_jpeg_create", 420, create_greyscale_jpeg(raw_data1, jpeg_rgb, chitoncal));
        }
        // COLOR IMAGE
        else if (GOPTS->color_mode == 3) {
            TIMER2("rgb_jpeg_create", 420, create_color_jpeg(calibrated, jpeg_rgb));
        }
        // GAIN IMAGE
        else if (GOPTS->color_mode == 2) {
            TIMER2("gain_map", 420, calibration::gain_mapping(jpeg_rgb));
        }
        // COLOR IMAGE
        else if (GOPTS->color_mode == 0) {
            /*
            if (frame_number % 5 != 1 || !GOPTS->dynamic_illumination) {
                slam_idx = (slam_idx +1) % 10;
                TIMER2("vector_slam", 180, calibration::vector_slam(calibrated, slam, slam_idx));
                //beta_ctl = calibration::dynamic_illumination(slam, slam_idx, beta_ctl);
            }
            */

            TIMER2("vector_slam", 180, calibration::vector_slam(calibrated, slam, slam_idx));
            create_slam_jpeg(slam, slam_idx, jpeg_rgb);
        } else {
            memset(jpeg_rgb, 0, 640*480*3);
        }



        // TODO: ADD ME!
        //calibration::dynamic_illumination(&raw_data1)


        // TODO: improve JPEG encoding here...

        if (jpeg_ptr != NULL)
        {
            free(jpeg_ptr);
            jpeg_ptr = NULL;
        }
        jpeg_len = 0;
        if (GOPTS->color_mode == 3 || GOPTS->color_mode == 2 || GOPTS->color_mode == 0 || GOPTS->color_mode == 6)
        {
            jpeg_len = write_rgb_jpeg(jpeg_rgb, (long)&jpeg_ptr, planar_rez);
        }
        else if (GOPTS->color_mode <= 1 || GOPTS->color_mode == 5)
        {
            jpeg_len = write_greyscale_jpeg(jpeg_rgb, (long)&jpeg_ptr, planar_rez);
        }
        // Frame ownership is released by frame_guard at the end of this iteration.

        data::publish_frame(metadata);
        // Cloud acquisition is continuous, not a side effect of a retrieval.
        const bool cloud_mode = GOPTS->color_mode == 4;
        last_color_mode = GOPTS->color_mode;
        slam_idx = (slam_idx +1) % 10;
        frame_guard.unlock();
        if (cloud_mode) usleep(33333);
        // The synchronous stub otherwise immediately reacquires ownership and can
        // starve queued settings/snapshot readers indefinitely.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    cam::run_with_lock([]() { camera->stop(); return 0; });
    free(slam);
    INFO("camera thread done");
    data::set_ready(true);
    return 1;
}

/**
 * move to a new file, something like background.cpp or tasks.cpp
 * return true if temperature is too high to start
 */
bool monitor_update()
{
    std::lock_guard<std::recursive_mutex> guard(cam::state_mutex());
    GOPTS->vixel_temperature = utils::get_vixel_temp_c();
    GOPTS->cpu_temperature = utils::get_cpu_temp_c();

    // temp warning
    if (GOPTS->cpu_temperature > 75 || GOPTS->vixel_temperature > 70) {
        GOPTS->set_message("TEMP WARNING");
        WARN("TEMP WARNING cpu %.2fc vixel %.2fc", GOPTS->cpu_temperature, GOPTS->vixel_temperature);
    }

    // temp shutdown
    if (GOPTS->cpu_temperature > 80 || GOPTS->vixel_temperature > 80)
    {
        GOPTS->set_message("TEMP SHUTDOWN");
        PANIC("TEMP SHUTDOWN CPU: %.2fc VIXEL: %.2fc", GOPTS->cpu_temperature, GOPTS->vixel_temperature);
        GOPTS->running = EXIT_TEMP_ERROR; // exit
        // Main loop will join camera and HTTP workers before exiting.
        return true;
    }

    return (GOPTS->cpu_temperature > 75 || GOPTS->vixel_temperature > 70) ? true : false;
}

/**
 * temperature monitor thread
 */
void run_monitor()
{
    while (GOPTS->running > 0)
    {
        // refresh temperature
        monitor_update();

        // poll temp 1 / second
        usleep(1000 * 1000);
    }
}

void robot()
{
    bool connected = false;
    int fd = 0;
    while (GOPTS->running > 0 && fd == 0)
    {
        fd = utils::i2c_open(2, 0x21);
        usleep(1000 * 1000);
    }
    INFO("FOUND ROBOT AT 0x21 File Descriptor @[%d]", fd);
    unsigned char drive[5] = {0xFF, 0x0D, GOPTS->min_left, GOPTS->min_front, GOPTS->min_right};
    while (GOPTS->running > 0)
    {
        //int wrote = utils::i2c_write(fd, drive, 3);
        int wrote = write(fd, drive, 3);
        // INFO("wrote bytes: [%d]", wrote);
        usleep(1000 * 1000);
    }
    if (fd > 0)
    {
        close(fd);
    }
}

/**
 * enable core dumps
 */
void auto_debug_setup()
{
    std::cout << "auto setup for debug" << std::endl;
    UNUSED int r = system("ulimit -c unlimited");
    r = system("sysctl -w kernel.core_pattern=\"/usr/local/chiton/chiton.core\"");
}

/*
 std::vector<xy_samp_t> create_planar_mapping(std::vector<VixelCalibration> vixel) {

    std::vector<xy_t> inner = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, 1200, GOPTS->inner_dist);
    std::vector<xy_t> outer = gfx::circle_points(GOPTS->center_x, GOPTS->center_y, 1200, GOPTS->outer_dist);


    std::vector<xy_samp_t> points = gfx::connect_vector_points(inner, outer, 165);
    INFO("num points: [%d]", points.size());
    return points;
 }
 */

/** Main-thread rendering and event loop. Capture and HTTP remain workers. */
int run_app(display::Backend &backend)
{
    std::unique_ptr<display::Preview> preview;
    if (backend.visible()) preview = std::make_unique<display::Preview>();
    const auto start = std::chrono::steady_clock::now();
    auto next_frame = start;
    auto next_animation = start;
    int animation = 0;
    bool animating = backend.visible();
    while (GOPTS->running > 0) {
        if (sys::termination_requested() || !backend.process_events()) {
            GOPTS->running = 0;
            break;
        }
        auto now = std::chrono::steady_clock::now();
        if (animating && now >= next_animation) {
            char filename[64];
            snprintf(filename, sizeof(filename), "jabil/xx_%04d.png", animation);
            auto path = resources::find(filename);
            auto image = path.empty() ? nullptr : read_png_file(path.c_str(), false);
            if (!image) {
                std::cerr << "Chiton: optional startup animation unavailable; skipping\n";
                animating = false;
            } else {
                backend.present(preview->startup_image(*image));
                animating = ++animation <= 46;
                next_animation = now + std::chrono::milliseconds(26);
            }
        }
        if (preview && !animating && now >= next_frame) {
            display::Samples samples;
            bool ready;
            {
                std::lock_guard<std::mutex> lock(preview_lock);
                ready = preview_ready;
                if (ready) samples = preview_samples;
            }
            std::string ethernet, wifi, message;
            {
                std::lock_guard<std::mutex> lock(GOPTS->status_lock);
                ethernet = GOPTS->eth0;
                wifi = GOPTS->wlan0;
                message = GOPTS->message;
            }
            auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
            backend.present(preview->render(ready ? &samples : nullptr, ethernet, wifi, message, seconds));
            next_frame = now + std::chrono::milliseconds(110);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return 0;
}

void update_ethernet()
{
    int i = 0;
    while (GOPTS->running > 0)
    {
        usleep(33333);
        if (i++ % 300 == 1)
        {
            std::string ethernet = "eth : " + utils::get_interface_address("eth0");
            std::string wifi = "wifi: " + utils::get_interface_address("wlan0");
#ifdef CHITON_ADITOF_STUB
            // Desktop distributions use predictable names such as enp*/wlp*.
            struct ifaddrs *interfaces = nullptr;
            if (getifaddrs(&interfaces) == 0) {
                for (auto *item = interfaces; item; item = item->ifa_next) {
                    if (!item->ifa_addr || item->ifa_addr->sa_family != AF_INET || (item->ifa_flags & IFF_LOOPBACK)) continue;
                    std::string name(item->ifa_name);
                    if (name[0] == 'w') wifi = "wifi: " + utils::get_interface_address(name);
                    else ethernet = "net : " + utils::get_interface_address(name);
                }
                freeifaddrs(interfaces);
            }
#endif
            std::lock_guard<std::mutex> lock(GOPTS->status_lock);
            GOPTS->eth0 = ethernet;
            GOPTS->wlan0 = wifi;
        }
    }
}



/**
 * program begin
 */
int main(int argc, char **argv)
{
    display::Mode display_mode = display::Mode::Auto;
    try {
        if (const char *mode = getenv("CHITON_DISPLAY")) display_mode = display::parse_mode(mode);
        for (int i = 1; i < argc; ++i) {
            std::string arg(argv[i]);
            if (arg == "--help" || arg == "-h") {
                std::cout << "Usage: chiton [--display auto|framebuffer|desktop|headless]\n";
                return 0;
            }
            if (arg == "--display" && i + 1 < argc) display_mode = display::parse_mode(argv[++i]);
            else if (arg.rfind("--display=", 0) == 0) display_mode = display::parse_mode(arg.substr(10));
            else throw std::invalid_argument("unknown/incomplete option: " + arg);
        }
    } catch (const std::exception &error) {
        std::cerr << "Chiton: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }

    // Fail explicit backend selection before allocating app state or workers.
    std::unique_ptr<display::Backend> backend;
    try { backend = display::create(display_mode); }
    catch (const std::exception &error) {
        std::cerr << "Chiton: display initialization failed: " << error.what() << std::endl;
        return EXIT_FAILURE;
    }

#if defined(DEBUG_MODE) && !defined(CHITON_ADITOF_STUB)
    auto_debug_setup();
//utils::redirect_stdout();
#endif

    // init system defaults and load chiton.json
    DisplayBuffers *system_display = sys::SystemInit(320, 240);
    utils::init_options();
    hsv_cache = init_rgb_cache();
    data::init_defaults();
    INFO("chiton ver: [%d]", CHITON_VER);
    calibration::make_calibration_data();
    // utils::print_stacktrace();

    const std::string file = utils::calibration_path();
    bool calibration_loaded = false;
    if (utils::file_exists(file)) {
        try {
            utils::read_calibration(file);
            calibration::process_calibration();
            calibration_loaded = true;
        } catch (const std::exception &error) {
            std::cerr << "Chiton: ignoring invalid calibration: " << error.what() << std::endl;
        }
    }
    if (!calibration_loaded) {
        float *pixel_gain = calibration::get_pixel_gain();
        float *pixel_offset = calibration::get_pixel_offset();
        for (int i=0; i<CAM_WIDTH*CAM_HEIGHT; i++) {
            pixel_gain[i] = 1.0f;
            pixel_offset[i] = 0.0f;
        }

        calibration::process_old_calibration();
    }










    
/*
    calibration::point *p = calibration::get_calibration();
    p[2].near_dist = 123;
    p[2].near_point = 456;
    p[2].index = 99;
    utils::write_calibration(p, "cal2.bin", CAMWIDTH * CAMHEIGHT);
    */



    
    std::map<std::string, web::RequestHandler> fnMap;
    fnMap["camera"] = camera_fn;
    // Legacy handlers intentionally ignore query strings; frame retrieval has
    // an explicit typed query contract instead of adding positional path fields.
    fnMap["value"] = [](std::vector<std::string> p, const std::string &) { return value_fn(std::move(p)); };
    fnMap["rover"] = [](std::vector<std::string> p, const std::string &) { return web::rover_fn(std::move(p)); };
    fnMap["setting"] = [](std::vector<std::string> p, const std::string &) { return web::setting_fn(std::move(p)); };
    fnMap["software"] = [](std::vector<std::string> p, const std::string &) { return web::software_fn(std::move(p)); };
    fnMap["calibrate"] = [](std::vector<std::string> p, const std::string &) { return calibrate_fn(std::move(p)); };
    std::signal(SIGPIPE, SIG_IGN); // A disconnected HTTP client is not fatal.
    int web_result = 0;
    std::thread monitor_thread;
#ifndef CHITON_ADITOF_STUB
    monitor_thread = std::thread(run_monitor);
#else
    INFO("file-backed camera: hardware temperature monitor disabled");
#endif
    std::thread camera_thread(run_camera);
    std::thread ethernet_thread(update_ethernet);
    std::thread http_thread([&]() {
        while (GOPTS->running > 0 && !data::is_ready()) usleep(5000);
        if (GOPTS->running > 0) web_result = web::web_server(PORT, fnMap);
        if (web_result < 0) GOPTS->running = EXIT_SYSCALL_ERROR;
    });
    try { run_app(*backend); }
    catch (const std::exception &error) {
        std::cerr << "Chiton: display presentation failed: " << error.what() << std::endl;
        GOPTS->running = EXIT_FB_ERROR;
    }
    const int exit_reason = GOPTS->running;
    // Preserve a failure code while asking every worker to stop.
    if (exit_reason > 0) GOPTS->running = 0;
    http_thread.join();
    camera_thread.join();
    if (monitor_thread.joinable()) monitor_thread.join();
    ethernet_thread.join();
    backend.reset(); // SDL cleanup stays on the main thread.
    camera.reset();
    if (GOPTS->api_lib) { dlclose(GOPTS->api_lib); GOPTS->api_lib = nullptr; }
    free(jpeg_ptr);
    free(jpeg_rgb);
    free(src_ptr);
    free(dst_ptr);
    free(hsv_cache);
    free(system_display->buffer1);
    free(system_display->buffer2);
    free(system_display);
    INFO("threads joined");
    for (auto &item : *GOPTS->cal) delete item.second;
    delete GOPTS->cal;
    delete GOPTS->log_level;
    delete GOPTS;
    GOPTS = nullptr;
    return exit_reason < 0 || web_result < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
