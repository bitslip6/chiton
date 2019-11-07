#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>
#include <iostream>
#include <fstream>

#include "main.h"

using namespace aditof;

aditof::Camera *get_tof_camera() {
    aditof::Status status = aditof::Status::OK;

    aditof::System system;
    status = system.initialize();
    if (status != aditof::Status::OK) {
        Panic("Could not initialize TOF system. ensure power and connection");
    }

    std::vector<aditof::Camera *> cameras;
    system.getCameraList(cameras);
    if (cameras.empty()) {
        Panic("Could not find camera. ensure power and connection");
    }

    aditof::Camera *camera = cameras.front();
    status = camera->initialize();
    if (status != aditof::Status::OK) {
        Panic("Could not iniitalize camera. ensure power and connection");
    }

    std::vector<std::string> frameTypes;
    camera->getAvailableFrameTypes(frameTypes);
    if (frameTypes.empty()) {
        Panic("no camera frame types. ensure power and connection");
    }
    status = camera->setFrameType(frameTypes.front());
    if (status != aditof::Status::OK) {
        Panic("could not set camera frame type. ensure power and connection");
    }

    std::vector<std::string> modes;
    camera->getAvailableModes(modes);
    if (modes.empty()) {
        Panic("no camera modes available");
    }

    status = camera->setMode("medium");
    if (status != Status::OK) {
        Panic("could not set camera mode");
    }

    return camera;
}

void set_camera_mode(aditof::Camera *camera, char *mode) {
    aditof::Status status = aditof::Status::OK;
    status = camera->setMode("medium");
    if (status != Status::OK) {
        Panic("could not set camera mode");
    }
}

/**
 *  return depth data. will block and allocate memory 
 */
void get_depth_frame(aditof::Camera *camera, uint16_t *data1) {
    aditof::Frame frame;
    Status status = Status::OK;
    printf("req frame\n");
    status = camera->requestFrame(&frame);
    if (status != Status::OK) {
        Panic("Could not request frame!\n");
    }


    printf("get data from frame\n");
    status = frame.getData(FrameDataType::DEPTH, &data1);
    if (status != Status::OK) {
        Panic("Could not get frame data!\n");
    }
}