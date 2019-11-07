#include <aditof/camera.h>
#include <aditof/frame.h>
#include <aditof/system.h>
#include <glog/logging.h>
#include <iostream>
#include <fstream>
#include "cvui.h"

#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <wiringPi.h>
#include <softPwm.h>


#include <fcntl.h>
#include <linux/i2c-dev.h>



//#include <cv.h>
//#include <highgui.h>
//#include <opencv2/imgproc/imgproc.hpp>

using namespace aditof;
using namespace cv;


int bus;
int drivePid = 0;
uint64_t s1, s2, d1;
struct timeval t1, t2;


int main(int argc, char *argv[]) {

    google::InitGoogleLogging(argv[0]);
    FLAGS_alsologtostderr = 1;
    printf("starting...\n");

    wiringPiSetupGpio() ;
    softPwmCreate (18, 1024, 1024) ;

    char *filename = "/dev/i2c-1";
    if ((bus = open(filename, O_RDWR)) < 0) {
        /* ERROR HANDLING: you can check errno to see what went wrong */
        printf("Failed to open the i2c bus\n");
        exit(1);
    }
    printf("opened i2c bus\n");


    int addr = 0x15;          // The I2C address of the piborg
    if (ioctl(bus, I2C_SLAVE, addr) < 0) {
        printf("Failed to acquire bus access and/or talk to slave errno[%d].\n", errno);
        exit(2);
    }

    char *data;
    char rbuf[10] = {0};
    RawRead(0x99, 6, rbuf);

    if (rbuf[1] != 0x15) {
	    printf("unable to find Thunderborg, found [%0x] instead\n", rbuf[1]);
	    exit(3);
    }

    printf("FOUND THUNDER BORG! [%0x %0x]\n", rbuf[0], rbuf[1]);
    ReadBattery();
    FailSafe();
    


    Status status = Status::OK;

    System system;
    status = system.initialize();
    if (status != Status::OK) {
        printf("Could not initialize system!\n");
        return 0;
    }

    std::vector<Camera *> cameras;
    system.getCameraList(cameras);
    if (cameras.empty()) {
        printf("No cameras found\n");
        return 0;
    }

    auto camera = cameras.front();
    status = camera->initialize();
    if (status != Status::OK) {
        printf("Could not initialize camera!\n");
        return 0;
    }

    std::vector<std::string> frameTypes;
    camera->getAvailableFrameTypes(frameTypes);
    if (frameTypes.empty()) {
        printf("no frame type avaialble!\n");
        return 0;
    }
    status = camera->setFrameType(frameTypes.front());
    if (status != Status::OK) {
        printf("Could not set camera frame type!\n");
        return 0;
    }
    printf("set frame type to [%s]\n", frameTypes.front());

    std::vector<std::string> modes;
    camera->getAvailableModes(modes);
    if (modes.empty()) {
        printf("no camera modes available!\n");
        return 0;
    }
    status = camera->setMode("medium");//modes.front());
    if (status != Status::OK) {
        printf("Could not set camera mode!\n");
        return 0;
    }
    printf("set mode to [medium]\n");

    aditof::Frame frame;
    uint16_t *data1;

    int ctr = 0;
    int pid = 0;
    int wstatus = 0;




    // printf("init pid [%d]\n", pid);

    DriveForward(90);
    while(1) {
        gettimeofday(&t2, NULL);
        s2 = t2.tv_sec * 1000000 + t2.tv_usec;
        d1 = s2 - s1;

        status = camera->requestFrame(&frame);
        if (status != Status::OK) {
            printf("Could not request frame!\n");
            return 0;
        }


        status = frame.getData(FrameDataType::RAW, &data1);
        if (status != Status::OK) {
            printf("Could not get frame data!\n");
            return 0;
        }

        if (!data1) {
            printf("no memory allocated in frame\n");
            return 0;
        }



        int dist1 = data1[(640*200)+300];
        int dist2 = data1[(640*200)+340];
        int dist3 = data1[(640*240)+300];
        int dist4 = data1[(640*240)+340];
        int dist = (dist1 + dist2 + dist3 + dist4) / 4;
        //printf("%d, - [%d]\n", ctr, dist);

        if (dist < 650) {
            DriveLeft();
        }

        if (++ctr > 4) {

            ctr = 0;
            if (pid > 0) {
                int tmp = waitpid(pid, NULL, WNOHANG);
                if (tmp == -1) {
                    //printf("error [%s] pid[%d]\n", strerror(errno), pid);
                    continue;
                }
                else if (tmp != pid) {
                    //printf("still waiting: [%d/%d] err?[%d]\n", tmp, pid, errno);
                    continue;
                } else {
                    //printf("waited [%d]\n", tmp);
                    pid = 0;
                }
            }
            
            pid = fork();
            if (pid == 0) {
                //printf("child process!: [%d]\n", pid);
                cv::Mat m_depthImage;
                m_depthImage = cv::Mat(480, 640, CV_16UC1, data1);
                //int m_distanceVal = m_depthImage.at<ushort>(pointxy);

                m_depthImage.convertTo(m_depthImage, CV_8U, 255.0 / 5999);
                applyColorMap(m_depthImage, m_depthImage, cv::COLORMAP_RAINBOW);
                flip(m_depthImage, m_depthImage, 1);
                int color = 0;

                char text[20];
                sprintf(text, "[%d]", dist);
                cv::Point2d pointxy1(300, 200);
                cv::circle(m_depthImage, pointxy1, 4, cv::Scalar(color, color, color), -1, 8, 0);

                cv::Point2d pointxy2(340, 240);
                cv::circle(m_depthImage, pointxy2, 4, cv::Scalar(color, color, color), -1, 8, 0);
                
                cv::Point2d pointxy3(300, 240);
                cv::circle(m_depthImage, pointxy3, 4, cv::Scalar(color, color, color), -1, 8, 0);

                cv::Point2d pointxy4(340, 200);
                cv::circle(m_depthImage, pointxy4, 4, cv::Scalar(color, color, color), -1, 8, 0);


                if (dist < 650) {
                    cv::Point2d textxy(1, 1);
                    cv::putText(m_depthImage, "Collision Avoidance", textxy, cv::FONT_HERSHEY_DUPLEX, 2,
                        cv::Scalar(color, color, color));
                }
                cv::Point2d textxy2(250, 400);
                cv::putText(m_depthImage, text, textxy2, cv::FONT_HERSHEY_DUPLEX, 2,
                    cv::Scalar(color, color, color));

                cv::resize(m_depthImage, m_depthImage, cv::Size(320,240));
                imwrite("/tmp/out2.jpg", m_depthImage); 
                //printf("wrote image\n");


                char *cmdargs[] = {"/home/pi/fbv/fbv", "/tmp/out2.jpg", "-i", "-c", NULL};
                execvp("/home/pi/fbv/fbv", cmdargs);
                //printf("disp image\n");

                exit(0);
            }
            ctr = 0;
        }

        //printf("%d, [%d] - (%d)\n", ctr, m_distanceVal, status);

    }


	/*
    for (unsigned int i = 0; i < fDetails.width * fDetails.height; ++i) {
        std::cout << data1[i] << " ";
    }
    */

    return 0;
}
