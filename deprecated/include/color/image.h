#ifndef __IMAGE_H
#define __IMAGE_H 1

#include <stdint.h>

typedef struct Sprite {
    RGBA8 *data;
    u8 width;
    u8 height;
    s16 xoff;
    s16 yoff;
} Sprite;

typedef struct SpriteOffset {
    s16 xoff;
    s16 yoff;
    s16 width;
    s16 height;
} SpriteOffset;

typedef struct SpritePtr {

    s16 src_x_off;
    s16 src_y_off;

    u8 width;
    u8 height;

    s16 xoff;
    s16 yoff;
} SpritePtr;



typedef struct SpriteAnimation {
    // animation counters
    u8 num_sprites;
    s16 flip_frames;
    u8 sprite_ctr;
    s16 frame_ctr;

    // sprite data
    SpritePtr *sprites;
    RGBA8 *data;
    s16 data_width;

    // render location data
    u32 *draw_ptr;
    s16 draw_x;
    s16 draw_y;
    u8 draw_width;

    // draw method
    u8 blend_mode;
    float alpha;
} SpriteAnimation;




//u_int32_t* ReadPNG(char* file_name, u_int32_t trans, int *width, int *height);
RGBA8* ReadPNG_RGBA(char* file_name, int *width, int *height);
void SpriteAnimationPipeline(SpriteAnimation *a);
//void AnimationFromImage(SpriteAnimation *a, SpriteOffset *offset);
void AnimationFromImage(SpriteAnimation *a, SpriteOffset *offset);

#endif
