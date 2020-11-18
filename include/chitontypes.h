#include <stdint.h>

#ifndef CHITON_TYPES_H
#define CHITON_TYPES_H

typedef unsigned short db_t;

typedef signed char s8;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned short db_t;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;


// averaged 4 point sample
typedef struct xy_samp_t {
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
    float xf;
    float yf;
} xy_samp_t;


#endif