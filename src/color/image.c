#include <png.h>
#include <assert.h>

#include "util.h"
#include "demos.h"
#include "system.h"
#include "color/hsv.h"
#include "color/blends.h"
#include "color/image.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * advance the frame flip and sprite flip animation counters
 */
void AdvanceSpriteAnimation(SpriteAnimation *a) {
    // advance the frame flip counter
    if(++a->frame_ctr > a->flip_frames) {
        a->frame_ctr = 0;
        // after frame flip advance the sprite counter
        if(++a->sprite_ctr >= a->num_sprites) {
            a->sprite_ctr = 0;
        }
    }
}


/**
 * render the complete animation pipeline
 * TODO: add bounds checking
 * TODO: add sprite movement
 */
void SpriteAnimationPipeline(SpriteAnimation *a) {
    assert(a->alpha != 0);
    assert(a->data != NULL);

    AdvanceSpriteAnimation(a);
    
    SpritePtr *s = &a->sprites[a->sprite_ctr];

    // destination buffer initial offset = ((animation y offset + sprite y offset) * dst width) + dst x offset
    s16 dst_offset = ((a->draw_y+s->yoff) * a->draw_width)+a->draw_x + s->xoff;
    // src offset = sprite index y off * source image width + index x off
    s16 src_offset = (s->src_y_off * a->data_width) + s->src_x_off;
    RGB8 dst_pixel;
    for (u8 y=0; y<s->height; y++) {
        for (u8 x=0; x<s->width; x++) {
            // get the destination pixel 
            RGBA8 src_pixel = a->data[src_offset+x];
            if (src_pixel.a > 0.0f) {
                U32RGB8(a->draw_ptr[dst_offset+x], &dst_pixel);
                BlendPixelAlpha(&dst_pixel, &src_pixel, a->blend_mode);
                dst_pixel.r *= a->alpha;
                dst_pixel.g *= a->alpha;
                dst_pixel.b *= a->alpha;
                a->draw_ptr[dst_offset+x] = RGB8U32(&dst_pixel);
            }
        }
        dst_offset += a->draw_width;
        src_offset += a->data_width;
    }
}


/**
 * 
 */
void AnimationFromImage(SpriteAnimation *a, SpriteOffset *offset) {

    assert(a->data != NULL);
    a->sprites = NEWMEM(SpritePtr, a->num_sprites + 1);

    for (int i=0; i<a->num_sprites; i++) {
        SpritePtr *s = &a->sprites[i];
        s->width = offset[i].width;
        s->height = offset[i].height;
        s->src_x_off = offset[i].xoff;
        s->src_y_off = offset[i].yoff;
        s->xoff = -ceil(s->width / 2);
        s->yoff = -floor(s->height / 2);
    }

}



/**
 * read a PNG and return ARGB mapped malloced data
 **/
RGBA8* ReadPNG_RGBA(char* file_name, int *width, int *height) {

    int x, y;
    //int width, height;
    png_byte color_type;
    png_byte bit_depth;

    png_structp png_ptr;
    png_infop info_ptr;
    //int number_of_passes;
    png_bytep *row_pointers;

    unsigned char header[8];    // 8 is the maximum size that can be checked

    /* open file and test for it being a png */
    FILE *fp = fopen(file_name, "rb");
    if (!fp) {
        Panic(80, "[read_png_file] File:'%s' could not be opened for reading", file_name);
    }

    int num = fread(header, 1, 8, fp);
    if (num != 1*8) {
        Panic(81, "Read png header file '%s' %d, expected 8 bytes", file_name, num);
    }

    if (png_sig_cmp(header, 0, 8)) {
        Panic(81, "[read_png_file] File:'%s' is not recognized as a PNG file", file_name);
    }


    /* initialize stuff */
    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png_ptr) {
        Panic(82, "[read_png_file] File:'%s' png_create_read_struct failed", file_name);
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        Panic(83, "[read_png_file] File:'%s' png_create_info_struct failed", file_name);
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        Panic(84, "[read_png_file] File:'%s' Error during init_io", file_name);
    }

    png_init_io(png_ptr, fp);
    png_set_sig_bytes(png_ptr, 8);

    png_read_info(png_ptr, info_ptr);

    (*width) = png_get_image_width(png_ptr, info_ptr);
    (*height) = png_get_image_height(png_ptr, info_ptr);
    color_type = png_get_color_type(png_ptr, info_ptr);
    bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    //number_of_passes = png_set_interlace_handling(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    printf("PNG W: %d, H: %d, color type: %d, Bit Depth: %d\n", *width, *height, color_type, bit_depth);


    /* read file */
    if (setjmp(png_jmpbuf(png_ptr))) {
        Panic(85, "[read_png_file] File:'%s' Error during read_image", file_name);
    }

    row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * (*height));
    for (y=0; y<*height; y++) {
        row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr,info_ptr));
    }

    png_read_image(png_ptr, row_pointers);

    int h = *height;
    int w = *width;

    //u_int32_t *image = malloc((*width + 1)* (*height +1)* sizeof(u_int32_t));
    //bzero(image, (*width +1) * (*height +1) * sizeof(u_int32_t));
    RGBA8 *image = NEWMEM(RGBA8, (w+1) * (h+1));
    int dptr = 0;

      
    for (y=0; y<h; y++) {
        png_byte* row = row_pointers[y];
        for (x=0; x<w; x++) {
            png_byte* ptr = &(row[x*4]);
            dptr = y*w + x;
            image[dptr].r = ptr[0];
            image[dptr].g = ptr[1];
            image[dptr].b = ptr[2];
            image[dptr].a = (float)(ptr[3]/(float)255);
            //dptr++;

            /*
            if (y>2 && y<32 && x >100 && x < 132) {
            printf("x/y: %d/%d = [%d.%d.%d.%d]\n", x, y, ptr[0], ptr[1], ptr[2], ptr[3]);
            }

            u_int32_t val = ((ptr[1] > 254) ? 254 : ptr[1]) << 8 | ((ptr[0] > 254) ? 254 : ptr[0]) << 16 | ((ptr[3] > 254) ? 254 : ptr[3]) << 24| ((ptr[2] > 254) ? 254 : ptr[2]);
            //png_byte* ptr = &(row[x*3]);
            //u_int32_t val = ((ptr[1] > 254) ? 254 : ptr[1]) << 8 | ((ptr[0] > 254) ? 254 : ptr[0]) << 16 | ((ptr[2] > 254) ? 254 : ptr[2]);
             
            if (val != trans) {
                image[((h-y) * w)+x] = val;// 0x000000FF;
            }
            */
        }
    }
    fclose(fp);

    /* cleanup heap allocation */
    for (y=0; y<h; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);


    return image;
}

#ifdef __cplusplus
}
#endif
