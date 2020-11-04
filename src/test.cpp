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


int main(int arc, char **argv) {

    sys::SystemInit(640, 480);
    printf("test begin 3\n");

    test_map_to_json();

    //test_find_laser(300, 250);

    //test_options();
    //test_i2c_read();
}




