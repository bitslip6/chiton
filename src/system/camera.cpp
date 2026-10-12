#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>

#include "camera.h"
#include "utils.hpp"
#include "webserver.h"
#include "data.h"
#include "illumination_diagnostics.h"

#include <atomic>

extern unsigned char *jpeg_data1;
extern std::shared_ptr<aditof::Camera> camera;



namespace cam
{
    namespace { bool capture_started = false; }

    // One owner for SDK access, settings, geometry and the complete frame pipeline.
    // Recursive because existing SDK helpers also enter this ownership boundary.
    std::recursive_mutex &state_mutex() {
        static std::recursive_mutex mutex;
        return mutex;
    }

    web::HttpResponse initialize_capture(bool enabled, std::function<aditof::Status()> verify_stream) {
        std::lock_guard<std::recursive_mutex> owner(state_mutex());
        if (!camera) return web::error_response(503, "camera unavailable");
        // SDK setup may already stream. Do not cycle STREAMOFF/STREAMON merely
        // to synchronize bookkeeping: legacy capture drivers may not resume.
        // A BUSY start is accepted only after a real frame proves acquisition.
        if (enabled) {
            const auto status = camera->start();
            if (status == aditof::Status::BUSY) {
                const auto verified = verify_stream ? verify_stream() : aditof::Status::UNAVAILABLE;
                if (verified != aditof::Status::OK)
                    return web::error_response(500, "SDK reports camera busy; first-frame verification failed (SDK status " +
                        std::to_string(static_cast<int>(verified)) + ")");
            } else if (status != aditof::Status::OK) {
                return web::error_response(500, "camera startup start failed (SDK status " +
                    std::to_string(static_cast<int>(status)) + ")");
            }
        } else {
            // The bundled local SDK returns BUSY for an already stopped device.
            const auto status = camera->stop();
            if (status != aditof::Status::OK && status != aditof::Status::BUSY)
                return web::error_response(500, "camera startup stop failed (SDK status " +
                    std::to_string(static_cast<int>(status)) + ")");
        }
        capture_started = enabled;
        if (GOPTS->experimental_illumination) {
            ++GOPTS->illumination_generation;
            ++GOPTS->illumination_capture_generation;
            illumination::invalidate_published_knowledge(GOPTS->illumination_generation.load());
        }
        GOPTS->enable = enabled;
        data::invalidate_measurements();
        data::frame_changed().notify_all();
        return web::json_response(200, {{"value", enabled}});
    }

    web::HttpResponse set_capture(bool enabled) {
        std::lock_guard<std::recursive_mutex> owner(state_mutex());
        // Track acknowledged SDK acquisition separately from the startup option.
        if (!camera) return web::error_response(503, "camera unavailable");
        if (enabled != capture_started) {
            const auto status = enabled ? camera->start() : camera->stop();
            if (status != aditof::Status::OK)
                return web::error_response(500, enabled ? "camera start failed" : "camera stop failed");
            capture_started = enabled;
            if (GOPTS->experimental_illumination) {
                ++GOPTS->illumination_generation;
                ++GOPTS->illumination_capture_generation;
                illumination::invalidate_published_knowledge(GOPTS->illumination_generation.load());
            }
            data::invalidate_measurements();
        }
        GOPTS->enable = enabled;
        data::frame_changed().notify_all();
        return web::json_response(200, {{"value", enabled}});
    }

    /**
     * update a single setting in chiton, executes lambda if url matches
     * [params]  a vector of / separated url parameters
     * [method name]  the method name in params to invoke the lambda if there is a match
     * [min_params]  is the minimum required parameters to execute the lambda
     */
    int update_setting(std::vector<std::string> params, std::string method_name, uint min_params, std::function<int()> fn) {

        if (params.empty()) throw std::invalid_argument("missing camera action");
        std::string url_method = params[0];
        if (url_method.compare(method_name) == 0 && params.size() >= min_params) {
            return fn();
        }
        return 404;
    }

    /**
     * execute function and provide exclusive locking around the camera
     */
    int run_with_lock(std::function<int()> fn) {
        std::lock_guard<std::recursive_mutex> guard(state_mutex());
        return fn();
    }

    int wait_for_lock(std::function<int()> fn) {
        return run_with_lock(fn);
    }


    /**
     * camera functions
     * params vector of url parts 
     * socket a connected socket
     */
     /*
    int camera_fn(std::vector<std::string> params, int socket) {
        
        std::string data = "{404:404}";
        int resp = 404;

        // turn the camera on
        resp = (resp != 404) ? resp : update_setting(params, "enable", 1, [&]() {
            utils::adi_call([&camera]() { return camera->start(); });
            GOPTS->enable = true;
            return 200;
        });

        
        // turn the camera off
        resp = (resp != 404) ? resp : update_setting(params, "disable", 1, [&]() {
            GOPTS->enable = false;
            utils::adi_call([&camera]() { return camera->stop(); });
            return 200;
        });


        // send the jpeg data
        resp = (resp != 404) ? resp : update_setting(params, "image", 1, [&]() {
            jlock = 1;
            WARN_IFNOT(
                send_data(socket, std::string("image/jpeg"), jpeg_data1, jlenptr),
                "unable to send jpeg data");
            jlock = 0;
            return 200;
        });
        
        if (params[0].compare(std::string("raw")) == 0) {
            WARN_IFNOT(
                send_data(socket, std::string("application/octet-stream"), (unsigned char*) copy, CAM_WIDTH*CAM_HEIGHT*2);
                "unable to send raw data stream");
            return 200;
        }

        return 404;
    }
    */

}
