#include <chrono>
#include <cstdarg>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <memory>

typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned short db_t;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;

typedef struct smbus_data_t {
    u16 address;
    u16 block_sz;
    u8 block[3];
} smbus_data_t;

smbus_data_t i2c_smbus_format(u16 address, u16 value) {
    //std::unique_ptr<smbus_data_t> data(new smbus_data_t);
    auto data = std::make_unique<smbus_data_t>();
    data->address = (address >> 8) & 0xFF;
    data->block[0] = address & 0xFF;
    data->block[1] = (value >> 8) & 0xFF;
    data->block[2] = value & 0xFF;
    data->block_sz = 3;
    return *(data);
}

int main() {
    std::cout << "This is an endless loop to test memory leaking on the i2c_smbus_format call" << std::endl;
    std::cout << "To stop, press CRTL+C" << std::endl;

    while(1) {
        i2c_smbus_format(0x2222, 0x3333);
    }

    return 0;
}