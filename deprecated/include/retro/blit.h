#include "../demos.h"

#define BLIT_WRAP 0x1
#define BLIT_CLAMP 0x2
#define BLIT_CENTER 0x4

typedef struct BlitElm {
    u8 xpos;
    u8 ypos;
    u8 w;
    u8 h;
    u8 *mask;
    u8 hue;
    u8 sat;
} BlitElm;

typedef struct blob_t {
    float xpos;
    float ypos;
    float xspeed;
    float yspeed;
    u_int8_t oradius;
    u_int8_t cradius;
	u_int8_t h;
    BlitElm blitElm;
} blob_t;


void Animate_Blobs(DisplayBuffers *disp, AnimationOptions *opts);
