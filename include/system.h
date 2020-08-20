#ifndef __SYSTEM_H
#define __SYSTEM_H 1

#include "demos.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DemoConf {
    u8 iron;
    u8 depthon;
    u8 bbon;
    u8 fpson;
    u8 senseon;
    u8 flooron;
    u16 range;
    u16 rangeoffset;
    float rangefactor;


    uint sense;
    uint max_depth;
    uint min_depth;
    uint mid_depth;

    uint bb_min_x;
    uint bb_max_x;

    uint bb_min_y;
    uint bb_max_y;

    uint vixel_w;
    uint vixel_h;

    u8 running;
    u8 fps;
} DemoConf;



#define FB_PIXEL_SZ 5

// exit with
void Panic(int code, const char *format, ...);
// install signal handlers
void Setup_Handlers(void);

DisplayBuffers* SystemInit(u16 width, u16 height);

   
#ifdef __cplusplus
}
#endif

#endif
