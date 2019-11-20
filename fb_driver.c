#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/fb.h>

#include <math.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <png.h>
#include <stdbool.h>
#include "fb_driver.h"
#include "system.h"

#define FB_WIDTH 320
#define FB_HEIGHT 200



// TODO: time fb_line vs fb_line4
// frame bufffer info
u_int16_t *FBM;
struct ScreenInfo FB_INFO;
//struct FBPngImage FB_FONT_IMAGE;



/**
 * initialize the video device (SDL or FrameBuffer)
 */
DisplayBuffers VideoInit(char *device, u_int16_t ) {
    DisplayBuffers *display = malloc(sizeof(DisplayBuffers));

    display.width = WIDTH;
    display.height = HEIGHT;
    display.sz = WIDTH * HEIGHT;
    Setup_Handlers2();

#ifdef FBDRIVER
    display.raw = fb_init("/dev/fb0");
    if (display.raw == NULL) {
        Panic("unable to allocate famebuffer or virtual buffer", 30);
    }
    display.buffer1 = malloc(sizeof(u_int32_t) * WIDTH * HEIGHT);
    display.buffer2 = malloc(sizeof(u_int32_t) * WIDTH * HEIGHT);
    display.buffert = malloc(sizeof(u_int32_t) * WIDTH * HEIGHT);
    printf("The framebuffer and virtual buffer mapped successfully.\n");
#endif
#ifdef SDLDRIVER

    //SDL_Renderer *renderer;
    SDL_Event event;


    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow (
        "SDL2",
        100, 100,
        WIDTH * FBPIXEL_SZ, HEIGHT * FBPIXEL_SZ,
        SDL_WINDOW_SHOWN);


    SDL_Renderer* renderer = SDL_CreateRenderer (
        window,
        -1,
        SDL_RENDERER_ACCELERATED);


    SDL_RendererInfo info;
    SDL_GetRendererInfo( renderer, &info );
    printf("Renderer name: %s\nTexture formats:\n", info.name);
    for( Uint32 i = 0; i < info.num_texture_formats; i++ ) {
        printf(" -- %s\n", SDL_GetPixelFormatName( info.texture_formats[i] ));
    }

    SDL_Texture *texture = SDL_CreateTexture (
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        WIDTH * FBPIXEL_SZ, HEIGHT * FBPIXEL_SZ);

    printf("texture created...\n");
    // SDL_CreateWindowAndRenderer(WIDTH * FBPIXEL_SZ, HEIGHT * FBPIXEL_SZ, 0, &window, &renderer);
    //SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    //SDL_RenderClear(renderer);

/*
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    for (int i = 0; i < WIDTH * FBPIXEL_SZ; ++i) {
        SDL_RenderDrawPoint(renderer, i, i);
    }
    */
    SDL_RenderPresent(renderer);
    SDL_PollEvent(&event);

    display.buffer1 = malloc(sizeof(u_int32_t) * WIDTH * HEIGHT);
    display.buffer2 = malloc(sizeof(u_int32_t) * WIDTH * HEIGHT);
    display.buffert = malloc(sizeof(u_int32_t) * WIDTH * FBPIXEL_SZ * HEIGHT * FBPIXEL_SZ);
    display.renderer = renderer;
    display.window = window;
    display.texture = texture;

    // TOODO: open the window!
#endif

    return display;
}



// swaps two numbers 
void swap(int* a , int*b) { 
    int temp = *a; 
    *a = *b; 
    *b = temp; 
} 

// returns absolute value of number 
static inline float absolute(float x ) { 
    if (x < 0) return -x; 
    else return x; 
} 
  
//returns integer part of a floating point number 
static inline int iPartOfNumber(float x) { 
    return (int)x; 
} 
  
//rounds off a number 
static inline int roundNumber(float x) { 
    return iPartOfNumber(x + 0.5) ; 
} 
  
//returns fractional part of a number 
static inline float fPartOfNumber(float x) { 
    if (x>0) return x - iPartOfNumber(x); 
    else return x - (iPartOfNumber(x)+1); 
  
} 
  
//returns 1 - fractional part of number 
static inline float rfPartOfNumber(float x) { 
    return 1 - fPartOfNumber(x); 
} 
  


/**
 * multiply a color by amount (2 = 2x brighter, .5 = 1/2 bright)
 */
u_int16_t color_mul(u_int16_t c, float amount) {
    if (amount > 1) {
        return 
            min((((0xFF << 16) & c) * amount), 0xFF)+
            min((((0xFF << 8) & c) * amount),  0xFF) +
            min((((0xFF) & c) * amount),       0xFF);
    } else {
        return 
            max((((0xFF << 16) & c) * amount), 0x00)+
            max((((0xFF << 8) & c) * amount), 0x00) +
            max((((0xFF) & c) * amount), 0x00);
    }
}

/**
 * add a color to the existing color, dont go over or under
 * on RG or B 
 */
u_int16_t color_add(u_int16_t c, u_int16_t amount) {
    return 
        min(max(((0xFF << 16) & c) + (amount << 16), 0xFF), 0) +
        min(max(((0xFF << 8) & c) + (amount << 8),   0xFF), 0) +
        min(max(((0xFF) & c) + amount, 0xFF), 0);
}




/**
 * return info about the current frame buffer
 */
struct ScreenInfo fb_get_buffer_info() {
    return FB_INFO;
}

/**
 * todo: attach signal handler and call this function.
 * todo: remove global variables
 */
void fb_cleanup() {
    munmap(FBM, FB_INFO.buffer_sz);
    close(FB_INFO.fd);
}

/**
 * set every pixel in a buffer to black (0)
 */
void clear_buffer(u_int16_t *buffer) {
    bzero(buffer, FB_INFO.buffer_sz);
}

/**
 * set every pixel in a buffer to black (0)
 */
void decay_buffer(u_int16_t *buffer, float amount) {
    for(int y=0;y++;y<FB_INFO.height) {
        int ypos = y*FB_WIDTH;
        for(int x=0;x++;x<FB_INFO.width) {
            u_int16_t c = buffer[ypos];
            if (c > 0) {
                buffer[ypos] = color_mul(c, amount);
            }
            ypos++;
        }
    }
}

void put_pixel(int x, int y, u_int32_t color, u_int32_t *buf) {

	y *= 4;
	x *= 4;
	buf[(y*1920)+x] = color;
	buf[(y*1920)+x+1] = color;
	buf[(y*1920)+x+2] = color;
	buf[(y*1920)+x+3] = color;

	buf[((y+1)*1920)+x] = color;
	buf[((y+1)*1920)+x+1] = color;
	buf[((y+1)*1920)+x+2] = color;
	buf[((y+1)*1920)+x+3] = color;

	buf[((y+2)*1920)+x] = color;
	buf[((y+2)*1920)+x+1] = color;
	buf[((y+2)*1920)+x+2] = color;
	buf[((y+2)*1920)+x+3] = color;

	buf[((y+3)*1920)+x] = color;
	buf[((y+3)*1920)+x+1] = color;
	buf[((y+3)*1920)+x+2] = color;
	buf[((y+3)*1920)+x+3] = color;
}



/**
 * initialize the frame buffer, display some info to the console
 * Panic if we can't
 */
//u_int16_t* fb_init(const char* file_name, const char *font_file) {
u_int32_t* fb_init(const char *file_name) {
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

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

    // display some stats
    printf("Frame Buffer Rez: %dx%d, Virtual Rez: %dx%d, %dbpp\n", vinfo.xres, vinfo.yres, 
        vinfo.xres_virtual, vinfo.yres_virtual, vinfo.bits_per_pixel);
    printf("%d Xoff, %d Yoff, %d line len\n", vinfo.xoffset, vinfo.yoffset, finfo.line_length);
    printf("accel: [%d] smem_len: [%d]\n", finfo.accel, finfo.smem_len);
    FB_INFO.bpp = vinfo.bits_per_pixel;
    FB_INFO.buffer_sz = finfo.smem_len;
    FB_INFO.width = vinfo.xres;
    FB_INFO.height = vinfo.yres;

    // error on unexpected video format
    /*
    if (vinfo.bits_per_pixel != 16) {
        Panic("currently only support for 16BPP", 4);
    }
    */

    // Map the device to memory
    FBM = (u_int16_t *)mmap(0, FB_INFO.buffer_sz, PROT_READ | PROT_WRITE, MAP_SHARED, FB_INFO.fd, 0);
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

/**
 * draw a line, no multiply
 **/
void fb_line(int x0, int y0, int x1, int y1, u_int16_t color, u_int16_t *buffer) {
    bool steep = false; 
    if (absolute(x0-x1)<absolute(y0-y1)) { 
        swap(&x0, &y0); 
        swap(&x1, &y1); 
        steep = true; 
    } 
    if (x0>x1) { 
        swap(&x0, &x1); 
        swap(&y0, &y1); 
    } 
    int dx = x1-x0; 
    int dy = y1-y0; 
    int dx2 = dx * 2;
    int derror2 = absolute(dy)*2; 
    int error2 = 0; 
    int y = y0; 
    for (int x=x0; x<=x1; x++) { 
        if (steep) { 
            PIXEL(buffer, y, x, color);
        } else { 
            PIXEL(buffer, x, y, color);
        } 
        error2 += derror2; 
        if (error2 > dx) { 
            y += (y1>y0?1:-1); 
            error2 -= dx2; 
        } 
    } 
} 






/**
 * draw image
 */
void draw_image(int src_width, int src_height, int dx, int dy, u_int16_t *src, u_int16_t *dst) {

    int dst_off, src_off = 0;
    dst_off = PIXEL_OFF(dx, dy);

    for (uint y=src_height; y != 0; y--) {

        // skip 1 vertical line in src
        src_off = (src_width * y);

        //for (uint x=0; x<src_width; x+=2) {
        for (uint x=src_width - 2; x != 0; x--) {
            dst[dst_off++] = src[src_off++];
        }

        // skip 1 vertical line in dst, 
        // need to check src_width in case it is not FB_WIDTH*2
        dst_off += FB_WIDTH - (src_width);
    }
}

/**
 * reads the png image pointed to by filename and returns
 * memory alocated to display the image.  This does multiple
 * mallocs, seems like the implementation could be optomized
 * Ripped directly from libpng site...
 */
/*
FBPngImage read_png_file(const char *filename) {
  FILE *fp = fopen((char *)filename, "rb");

  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png) { Panic("unable to read PNG", 6); }

  png_infop info = png_create_info_struct(png);
  if(!info) { Panic("unable to create PNG info struct", 7); }

  if(setjmp(png_jmpbuf(png))) { Panic("png jmp abort?", 8); }

  png_init_io(png, fp);

  png_read_info(png, info);

  FBPngImage image;

  image.width      = png_get_image_width(png, info);
  image.height     = png_get_image_height(png, info);
  image.color_type = png_get_color_type(png, info);
  image.bit_depth  = png_get_bit_depth(png, info);

  // Read any color_type into 8bit depth, RGBA format.
  // See http://www.libpng.org/pub/png/libpng-manual.txt

  if(image.bit_depth == 16)
    png_set_strip_16(png);

  if(image.color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_palette_to_rgb(png);

  // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
  if(image.color_type == PNG_COLOR_TYPE_GRAY && image.bit_depth < 8)
    png_set_expand_gray_1_2_4_to_8(png);

  if(png_get_valid(png, info, PNG_INFO_tRNS))
    png_set_tRNS_to_alpha(png);

  // These color_type don't have an alpha channel then fill it with 0xff.
  if(image.color_type == PNG_COLOR_TYPE_RGB ||
     image.color_type == PNG_COLOR_TYPE_GRAY ||
     image.color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

  if(image.color_type == PNG_COLOR_TYPE_GRAY ||
     image.color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    png_set_gray_to_rgb(png);

  png_read_update_info(png, info);

  if (image.row_pointers) { Panic("row pointers init already!", 9); }

  image.row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * image.height);
  for(int y = 0; y < image.height; y++) {
    image.row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png,info));
  }

  png_read_image(png, image.row_pointers);

  printf("loaded image file [%s]\n", filename);
  fclose(fp);
  png_destroy_read_struct(&png, &info, NULL);

  return image;
}
*/


/**
 * writes the string to the display using the font previously
 * loaded at FB_FONT_IMAGE
 */
/*
void fb_write_string(char *msg, int buffx, int buffy, u_int16_t fgcolor, u_int16_t *buffer) {
    int l = strlen(msg);
    // loop over each char
    for (int i = 0; i < l; i++) {
        // map the char to the font offset
        FontChar fontc = fontData[(int)msg[i]];

        // loop over each row
        for(int y = 0; y < fontc.height; y++) {
            png_bytep row = FB_FONT_IMAGE.row_pointers[y+fontc.y];
            // loop over each column
            for(int x = 0; x <fontc.width; x++) {
                // map out the alpha level
                png_bytep px = &(row[(fontc.x + x) * 4]);
                // calculate the color as white
                u_int16_t color = ((px[3] >> 3)<<11) + ((px[3] >> 2)<<5) + (px[3] >> 3);
                // plot the pixel
                int offset = ((y+buffy+fontc.yoffset)*FB_WIDTH)+x+buffx+fontc.xoffset;
                if (color > 0) {
                    color = (fgcolor > 0) ? fgcolor : color;
                    buffer[offset] = color;
                }
            }
        }

        // skip to the next character
        buffx += 18;
    }
}
*/

  
/**
 * draw an antialiased line using wu algorithm
 */
void fb_aa_line(int x0 , int y0 , int x1 , int y1, u_int32_t bcolor, u_int16_t *buffer) { 
    int steep = absolute(y1 - y0) > absolute(x1 - x0) ; 

    // swap the co-ordinates if slope > 1 or we 
    // draw backwards 
    if (steep) 
    { 
        swap(&x0 , &y0); 
        swap(&x1 , &y1); 
    } 
    if (x0 > x1) 
    { 
        swap(&x0 ,&x1); 
        swap(&y0 ,&y1); 
    } 
  
    //compute the slope 
    float dx = x1-x0; 
    float dy = y1-y0; 
    float gradient;
    if (dx == 0.0) 
        gradient = 1; 
    else 
        gradient = dy/dx; 
    //printf("gradient: [%f] [%d]\n", gradient, steep);
  
    int xpxl1 = (x0 <= 0) ? 1 : x0; 
    int xpxl2 = x1; 
    float intersectY = (y0 <= 0) ? 1 : y0; 

    // main loop 
    if (steep) 
    { 
        int x; 
        // TODO optomize this loop!
        for (x = xpxl1 ; x <=xpxl2 && x < FB_HEIGHT; x++) 
        { 
            // pixel coverage is determined by fractional 
            // part of y co-ordinate 
            int px = 255 * fPartOfNumber(intersectY);
            u_int16_t color = ((px >> 3)<<11) + ((px >> 2)<<5) + (px >> 3);
            PIXEL_OR(buffer, iPartOfNumber(intersectY), x, color); 

            px = 255 * rfPartOfNumber(intersectY);
            color = ((px >> 3)<<11) + ((px >> 2)<<5) + (px >> 3);
            PIXEL_OR(buffer, iPartOfNumber(intersectY)-1, x, color); 

            intersectY += gradient; 
        } 
    } 
    else
    { 
        int x; 
        for (x = xpxl1 ; x <=xpxl2 && x < FB_WIDTH && intersectY < FB_HEIGHT; x++) 
        { 
            int px = 255 * fPartOfNumber(intersectY);
            u_int16_t color = ((px >> 3)<<11) + ((px >> 2)<<5) + (px >> 3);
            buffer[(iPartOfNumber(intersectY)*FB_WIDTH)+x] |= color;

            px = 255 * rfPartOfNumber(intersectY);
            color = ((px >> 3)<<11) + ((px >> 2)<<5) + (px >> 3);
            buffer[((iPartOfNumber(intersectY)-1)*FB_WIDTH)+x] |= color;

            intersectY += gradient; 
        } 
    } 
} 
 

