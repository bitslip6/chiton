/******************************************************************************
 *
 * @file            system.cpp
 *
 * @brief           System-level functions
 *
 * @details
 *
 * @author(s)       Stephen Stauts
 *
 * @created         7/9/2020   (MM/DD/YYYY)
 *
 * @copyright       © 2020 Jabil, Inc. All Rights Reserved.
 *
 *****************************************************************************/


//-----------------------------------------------------------------------------
//  Includes
//-----------------------------------------------------------------------------

#include "demos.h"
#include "system.hpp"

#include <csignal>
#include <cstdarg>
#include <cstdlib>
#include <vector>
#include <string.h>
#include <linux/fb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

// #include "fb_driver.h"
#include "utils.hpp"

namespace
{
std::vector<shutdown_fn_t> v_fns = {};
}    // namespace

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace sys
{
void panic(int code, const char * format, ...)
{
    va_list args;
    va_start(args, format);

    utils::safe_cerr("<-- !! SYSTEM PANIC !! -->");
    utils::safe_print(format, args);

    va_end(args);

    exit(code);
}

void init_signal_handlers()
{
    std::signal(SIGINT, ctrl_c_handler);
    std::signal(SIGTERM, ctrl_c_handler);
}

void ctrl_c_handler(int signum)
{
    utils::safe_cerr("---> Caught CTRL+C!");

    shutdown_now(signum);
}

void register_shutdown_fn(shutdown_fn_t func)
{
    v_fns.push_back(func);
}

void shutdown_now(int exit_code)
{
    if (v_fns.empty() == false) {
        utils::safe_cerr("Executing registered shutdown functions");
        for (auto fn : v_fns) {
            fn();
        }
    }
    exit(exit_code);
}

/**
 * initialize the system display drivers
 */
DisplayBuffers* SystemInit(u16 width, u16 height) {
    DisplayBuffers *display;
    GlobalOpts *opts;
    display = (DisplayBuffers *) malloc(sizeof(DisplayBuffers));
    opts = (GlobalOpts *) malloc(sizeof(GlobalOpts));

    GOPTS = opts;
    opts->running = true;

    GDISP = display;
    display->width = width;
    display->height = height;
    display->invx = (float)1/width;
    display->invy = (float)1/height;
    display->sz = width * height;

    init_signal_handlers();

#ifdef FBDRIVER
    display->fd = open("/dev/fb1", O_RDWR);
    if (display->fd == -1) {
        utils::Panic(1, "Error: cannot open framebuffer device: /dev/fb1");
    } 
    
    display->raw = (db_t *)mmap(0, display->sz, PROT_READ | PROT_WRITE, MAP_SHARED, display->fd, 0);
    if (*display->raw == 65535) {
        utils::Panic(5, "Error: failed to map framebuffer device to memory");
    }
    
    display->buffer1 = (db_t*) malloc(sizeof(db_t) * width * height);
    display->buffer2 = (db_t*) malloc(sizeof(db_t) * width * height);
    // display->buffert = (u_int32_t*) malloc(sizeof(u_int32_t) * width * height);
    printf("The framebuffer and virtual buffer mapped successfully.\n");
#endif
#ifdef SDLDRIVER

    //SDL_Renderer *renderer;
    SDL_Event event;


    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow (
        "Demos 2.0",
        10, 10,
        width * FB_PIXEL_SZ, height * FB_PIXEL_SZ,
        SDL_WINDOW_SHOWN);


    SDL_Renderer* renderer = SDL_CreateRenderer (
        window,
        -1,
        SDL_RENDERER_ACCELERATED);


    /*
    SDL_RendererInfo info;
    SDL_GetRendererInfo( renderer, &info );
    printf("Renderer name: %s\nTexture formats:\n", info.name);
    for( Uint32 i = 0; i < info.num_texture_formats; i++ ) {
        printf(" -- %s\n", SDL_GetPixelFormatName( info.texture_formats[i] ));
    }
    */

    SDL_Texture *texture = SDL_CreateTexture (
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        width * FB_PIXEL_SZ, height * FB_PIXEL_SZ);

    //printf("texture created...\n");
    //SDL_CreateWindowAndRenderer(width * FB_PIXEL_SZ, HEIGHT * FB_PIXEL_SZ, 0, &window, &renderer);
    //SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    //SDL_RenderClear(renderer);

    /*
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    for (int i = 0; i < width * FB_PIXEL_SZ; ++i) {
        SDL_RenderDrawPoint(renderer, i, i);
    }
    */
    SDL_RenderPresent(renderer);
    SDL_PollEvent(&event);

    //display->buffer1 = malloc(sizeof(u_int32_t) * width * height);
    display->renderer = renderer;
    display->window = window;
    display->texture = texture;
#endif
    display->buffer1 = (db_t *) NEWMEM(db_t, display->sz+16); 
    display->buffer2 = (db_t *) NEWMEM(db_t, display->sz+16);
    //display->buffert = (u_int32_t*) NEWMEM(u_int32_t, (width * FB_PIXEL_SZ) * (height* FB_PIXEL_SZ) * 4);
    //display->zbuffer = (int16_t*) NEWMEM(int16_t, width * height);

    display->draw_ptr = display->buffer1;
    return display;
}


/**
 * initialize the frame buffer, display some info to the console
 * Panic if we can't
 */ 
/*
DisplayBuffers* fb_init(char *file_name) {
    //struct fb_var_screeninfo vinfo;
    //struct fb_fix_screeninfo finfo;


    DisplayBuffers *disp = (DisplayBuffers *) malloc(sizeof(DisplayBuffers));
    // init_fonts();

    printf("open %s\n", file_name);
    disp->fd = open(file_name, O_RDWR);
    // Open the file for reading and writing
    if (disp->fd == -1) {
        Panic(1, "Error: cannot open framebuffer device: %s", file_name);
    } 

    // Get fixed screen information
    if (ioctl(disp->fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        Panic(2, "Error reading fixed information");
    }

    // Get variable screen information
    if (ioctl(disp->fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        Panic(3, "Error reading variable information");
    }

    // display some stats
    printf("Frame Buffer Rez: %dx%d, Virtual Rez: %dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.xres_virtual, vinfo.yres_virtual, vinfo.bits_per_pixel);
    printf("%d Xoff, %d Yoff, %d line len\n", vinfo.xoffset, vinfo.yoffset, finfo.line_length);
    printf("accel: [%d] smem_len: [%d]\n", finfo.accel, finfo.smem_len);

    disp->sz = finfo.smem_len;
    disp->width = 320;//vinfo.xres;
    disp->height = 240;//vinfo.yres;
    disp->floor_sz = disp->width * (disp->height);
    disp->floor_last = disp->floor_sz * (MAX_FLOOR_BUFFERS - 1);

    // error on unexpected video format
    if (vinfo->bits_per_pixel != 16) {
        Panic("currently only support for 16BPP", 4);
    }

    // Map the device to memory
    disp->raw = (db_t *)mmap(0, disp->sz, PROT_READ | PROT_WRITE, MAP_SHARED, disp->fd, 0);
    if ((int)disp->raw == -1) {
        Panic(5, "Error: failed to map framebuffer device to memory");
    }
    printf("just mapped: SZ:%d, W%d.IW%d.IBZ%d\n", disp->sz, disp->width, disp->width, disp->sz);

    //FB_LARGE_FONT = read_png_file((char*) "large_font.png");
    //FB_TINY_FONT  = read_png_file((char*) "small_font.png");

    //init_fonts();
    //ioctl(FB_INFO.fd, FBIOBLANK, VESA_POWERDOWN: VESA_NO_BLANKING);
    ioctl(disp->fd, FBIOBLANK, VESA_NO_BLANKING);
    bzero(disp->raw, disp->sz);

    //disp->buffert = (db_t*) malloc(disp->sz);
    //disp->bufferd = (db_t*) malloc(disp->sz);
    disp->buffer1 = (db_t*) malloc(disp->sz);
    disp->buffer2 = (db_t*) malloc(disp->sz);
    //disp->floor_buff = (db_t*) malloc(disp->sz);
    /*
    disp->buffer2 = (db_t*) malloc(disp->floor_sz * (MAX_FLOOR_BUFFERS+1) * sizeof(db_t));
    //printf("buffer2 size: %d, LAST: %d\n", disp->floor_sz * (MAX_FLOOR_BUFFERS+1) * sizeof(db_t), disp->floor_last);
    for (int i=0; i < disp->floor_sz * (MAX_FLOOR_BUFFERS); i++) {
        disp->buffer2[i] = 4095;
    }

    //memset(disp->buffert, 0, disp->sz);
    memset(disp->buffer2, 0, disp->sz);
    memset(disp->buffer1, 0, disp->sz);
    //memset(disp->floor_buff, 0, disp->sz);

    printf("buffer2 zeroed\n");
    //disp->buffert = (db_t*) malloc(disp->sz);

    return disp;
}
*/




}    // namespace sys
