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

#include "fplus/fplus.hpp"

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


int main(int arc, char **argv) {

    sys::SystemInit(640, 480);
    utils::init_options();
    printf("test begin 4\n");

    //test_map_to_json();


    test_slice();
    test_logging();
    //test_find_laser(300, 250);

    //test_options();
    //test_i2c_read();
}




