#include <png.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>


#include "draw.h"
#include "demos.h"
#include "system.h"

#define SDLDRIVER 1

#ifdef SDLDRIVER
#include <SDL2/SDL.h>
#endif

u_int8_t height_map[WIDTH] = {36, 36, 36, 32, 29, 22, 22, 29, 32, 36, 36, 36, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17 };
u_int8_t off_map[WIDTH] = {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14};



/**
 * hsv to rgb with floats...
 */ 
RGB HsvToRgb_f(HSV in) {
    float      hh, p, q, t, ff;
    int        i;
    RGB        out;

    hh = in.h;
    if (hh >= 360.0) {
        hh = 0.0;
    }
    hh /= 60.0;
    i = (int)hh;
    ff = hh - i;
    p = in.v * (1.0 - in.s);
    q = in.v * (1.0 - (in.s * ff));
    t = in.v * (1.0 - (in.s * (1.0 - ff)));

    switch(i) {
    case 0:
        out.r = in.v;
        out.g = t;
        out.b = p;
        break;
    case 1:
        out.r = q;
        out.g = in.v;
        out.b = p;
        break;
    case 2:
        out.r = p;
        out.g = in.v;
        out.b = t;
        break;

    case 3:
        out.r = p;
        out.g = q;
        out.b = in.v;
        break;
    case 4:
        out.r = t;
        out.g = p;
        out.b = in.v;
        break;
    case 5:
    default:
        out.r = in.v;
        out.g = p;
        out.b = q;
        break;
    }
    return out;     
}


// TODO move to pixel driver
u_int32_t HsvToRgb(u_int8_t h, u_int8_t s, u_int8_t v) {
    u_int8_t region, remainder, p, q, t;

    if (s <= 0) {
        return 0;
    }

    region = h / 43;
    remainder = (h - (region * 43)) * 6; 

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
			return v << 16 | t << 8 | p;
            break;
        case 1:
			return q << 16 | v << 8 | p;
            break;
        case 2:
			return p << 16 | v << 8 | t;
            break;
        case 3:
			return p << 16 | q << 8 | v;
            break;
        case 4:
			return t << 16 | p << 8 | v;
            break;
        default:
			return v << 16 | p << 8 | q;
            break;
    }

    return 0;
}

/**
 * set the pixel color in the buffer
 */
void inline PPixelSet(DisplayBuffers *disp, int x, int y, u_int32_t color) {
    disp->buffer1[(y * disp->width) + x] = color;
}

void HPixelSet(DisplayBuffers *disp, u_int8_t x, u_int8_t y, u_int8_t h, u_int8_t v, u_int8_t s) {
    disp->buffer1[(y * disp->width) + x] = HsvToRgb(h, s, v);
}

void inline HPixelOr(DisplayBuffers *disp, int x, int y, u_int8_t h, u_int8_t v, u_int8_t s) {
    disp->buffer1[(y * disp->width) + x] |= HsvToRgb(h, s, v);
}

void inline HPixelAnd(DisplayBuffers *disp, int x, int y, u_int8_t h, u_int8_t v, u_int8_t s) {
    disp->buffer1[(y * disp->width) + x] &= HsvToRgb(h, s, v);
}

void inline PixelAdd(DisplayBuffers *disp, u8 x, u8 y, u32 c1) {
    PPixelSet(disp, x, y, SatAdd32(c1, GetPixel(disp, x, y)));
}


/**
 * TODO: add gama correction...
 * decay the buffer
 */
void Decay(DisplayBuffers *disp, u_int32_t amount) {
    u_int32_t r = amount << 16;
    u_int32_t g = amount << 8;
    u_int32_t b = amount;

	// decay the whole screen
    for (int x = 0; x <= WIDTH; x++) {
        for (int y = 0; y <= HEIGHT; y++) {
            int off = (y*WIDTH)+x;

            int val = disp->buffer1[off];
            if ((val & 0x00FF0000) >= 1) { //&& (val >= 0x0001FFFF)) {
                disp->buffer1[off] -= r;
            }
            if ((val & 0x0000FF00) >= 1) { //&& (val >= 0x000001FF)) {
                disp->buffer1[off] -= g;
            }
            if ((val & 0x000000FF) >= 1) { //&& (val >= 0x00000001)) {
                disp->buffer1[off] -= b;
            }
        }
	}
}

/**
 * read a PNG and return ARGB mapped malloced data
 **/
u_int32_t* ReadPNG(char* file_name, u_int32_t trans, int *width, int *height) {

    int x, y;
    //int width, height;
    png_byte color_type;
    png_byte bit_depth;

    png_structp png_ptr;
    png_infop info_ptr;
    int number_of_passes;
    png_bytep * row_pointers;

    unsigned char header[8];    // 8 is the maximum size that can be checked

    /* open file and test for it being a png */
    FILE *fp = fopen(file_name, "rb");
    if (!fp) {
        Panic(80, "[read_png_file] File:'%s' could not be opened for reading", file_name);
    }

    fread(header, 1, 8, fp);
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

    number_of_passes = png_set_interlace_handling(png_ptr);
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

    u_int32_t *image = malloc((*width + 1)* (*height +1)* sizeof(u_int32_t));
    bzero(image, (*width +1) * (*height +1) * sizeof(u_int32_t));

    int h = *height;
    int w = *width;
      
    for (y=0; y<h; y++) {
        png_byte* row = row_pointers[y];
        for (x=0; x<w; x++) {
            png_byte* ptr = &(row[x*4]);
            u_int32_t val = ((ptr[1] > 254) ? 254 : ptr[1]) << 8 | ((ptr[0] > 254) ? 254 : ptr[0]) << 16 | ((ptr[3] > 254) ? 254 : ptr[3]) << 24| ((ptr[2] > 254) ? 254 : ptr[2]);
             
            if (val != trans) {
                image[((h-y) * w)+x] = val;// 0x000000FF;
            }
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


void ClearDisp(DisplayBuffers *disp) {
    memset(disp->buffer1, 0, (sizeof(u_int32_t) * disp->sz));
}

/**
 * main render frame
 */
void RenderDisp(DisplayBuffers *disp) {
    //printf("Render display: %dx%d\n", disp->width, disp->height);
    //printf("RAW: %x Buffer1: %x\n", disp->raw, disp->buffer1);

#ifdef LEDDRIVER
    int ctr = 0;
    for (int x = 0; x < disp->width; x++) {
        for (int y = 0; y < height_map[x]; y++) {
            disp->ledstring.channel[0].leds[ctr++] = disp->buffer1[(y * disp->width) + x];
        }
        x++;
        if (x < WIDTH) {
            for (int y = 0; y < height_map[x]; y++) {
                int by = height_map[x] - 1 - y ;
                disp->ledstring.channel[0].leds[ctr++] = disp->buffer1[(by * disp->width) + x];
            }
        }
    }

    if ((ws2811_render(&ledstring)) != WS2811_SUCCESS) {
        Panic(1, "ws2811_render() failed";
    }
#endif

#ifdef  SDLDRIVER

    int soff = 0;
    for (int y=0; y < disp->height; y++) {
        soff = y * disp->width;
        for (int x=0; x < disp->width; x++) {
            //u_int32_t color = 0x000000FF;
            u_int32_t color = disp->buffer1[soff++];
            // SDL_SetRenderDrawColor(disp->renderer, color & 0x00FF0000, color & 0x0000FF00, color & 0x000000FF, 255);


            // draw 12x12 pixels
            int off = ((y*FBPIXEL_SZ) * disp->width * FBPIXEL_SZ) + x*FBPIXEL_SZ;
            for (int fb_y=0; fb_y<FBPIXEL_SZ; fb_y++) {
                //int offy = ((y + fb_y) * FBPIXEL_SZ) * disp->width;
                //int offx = (x*FBPIXEL_SZ);
                //int off = (((y) * FBPIXEL_SZ)+fb_y)*disp->width;
                for (int fb_x=0; fb_x<FBPIXEL_SZ; fb_x++) {
                    //SDL_RenderDrawPoint(disp->renderer, offx+fb_x, offy);
                    disp->buffert[off] = color;
                    off++;
                }
                off += (disp->width-1)*FBPIXEL_SZ;
            }
        }
    }
    //printf("update tex %x\n", disp->texture);
    SDL_UpdateTexture(disp->texture, NULL, disp->buffert, disp->width * 4 * FBPIXEL_SZ);
    SDL_RenderCopy(disp->renderer, disp->texture, NULL, NULL );
    SDL_RenderPresent(disp->renderer);
#endif

}




#ifdef FBDRIVER
u_int32_t* fb_init(const char *file_name) {
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    struct ScreenInfo FB_INFO;
    u_int32_t *FBM;

    FB_INFO.fd = open(file_name, O_RDWR);
    // Open the file for reading and writing
    if (FB_INFO.fd == -1) {
        Panic("Error: cannot open framebuffer device", 1);
    }

    // Get fixed screen information
    if (ioctl(FB_INFO.fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        Panic("Error reading fixed information", 2);
    }

    // Get variable screen information
    if (ioctl(FB_INFO.fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        Panic("Error reading variable information", 3);
    }

    printf("\nVscreen Info:-\n");
    printf(" Xres   = %d | Yres   = %d\n",vinfo.xres,vinfo.yres);
    printf(" BPP    = %d | Height = %d | Width = %d\n",vinfo.bits_per_pixel,
                                                             vinfo.height,
                                                             vinfo.width);
    printf(" Xres_V = %d | Yres_V = %d\n",vinfo.height,vinfo.width);
    printf(" Pixel format : RGBX_%d%d%d%d\n",vinfo.red.length,
                                                 vinfo.green.length,
                                                 vinfo.blue.length,
                                                 vinfo.transp.length);
    printf(" Begin of bitfields(Byte ordering):-\n");     //In my case :
    printf("  Red    : %d\n",vinfo.red.offset);    //Red    : 16
    printf("  Blue   : %d\n",vinfo.blue.offset);   //Blue   : 0
    printf("  Green  : %d\n",vinfo.green.offset);  //Green  : 8
    printf("  Transp : %d\n",vinfo.transp.offset); //Transp : 24



    // display some stats
    printf("Frame Buffer Rez: %dx%d, Virtual Rez: %dx%d, %dbpp\n", vinfo.xres, vinfo.yres, 
        vinfo.xres_virtual, vinfo.yres_virtual, vinfo.bits_per_pixel);
    printf("%d Xoff, %d Yoff, %d line len\n", vinfo.xoffset, vinfo.yoffset, finfo.line_length);
    printf("accel: [%d] smem_len: [%d]\n", finfo.accel, finfo.smem_len);
    FB_INFO.bpp = vinfo.bits_per_pixel;
    FB_INFO.buffer_sz = finfo.smem_len;
    FB_INFO.width = vinfo.xres;
    FB_INFO.height = vinfo.yres;

    // Map the device to memory
    FBM = (u_int32_t *)mmap(0, FB_INFO.buffer_sz, PROT_READ | PROT_WRITE, MAP_SHARED, FB_INFO.fd, 0);
    if ((int)FBM == -1) {
        Panic("Error: failed to map framebuffer device to memory", 5);
    }

    //FB_FONT_IMAGE = read_png_file(font_file);
    //init_fonts();
    //ioctl(FB_INFO.fd, FBIOBLANK, VESA_POWERDOWN: VESA_NO_BLANKING);
    ioctl(FB_INFO.fd, FBIOBLANK, VESA_NO_BLANKING);
    bzero(FBM, FB_INFO.buffer_sz);
    printf("cleared screen\n");
    return FBM;
}
#endif

