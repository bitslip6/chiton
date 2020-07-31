#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <SDL2/SDL.h>

// #include "demos.h"
#include "util.h"
#include "system.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * panic with error code
 */
void Panic(int code, const char *format, ...) {
    va_list args;
    va_start(args, format);

    printf("SYSTEM PANIC\n");
    printf(format, args);
    exit(code);
}


/**
 * end on control c
 */
void ctrl_c_handler(int signum) {
	(void)(signum);
    printf(" CTRL-C: [%d]\n", GOPTS->running);
    GOPTS->running = 0;
    printf(" running: [%d]\n", GOPTS->running);
}

void Setup_Handlers(void) {

    signal(SIGINT, ctrl_c_handler);
    signal(SIGTERM, ctrl_c_handler);
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
    opts->clear_exit = 1;

    GDISP = display;
    display->width = width;
    display->height = height;
    display->invx = (float)1/width;
    display->invy = (float)1/height;
    display->sz = width * height;

    Setup_Handlers();

#ifdef FBDRIVER
    display.raw = fb_init("/dev/fb0");
    if (display.raw == NULL) {
        Panic("unable to allocate famebuffer or virtual buffer", 30);
    }
    display.buffer1 = malloc(sizeof(u_int32_t) * width * height);
    display.buffer2 = malloc(sizeof(u_int32_t) * width * height);
    display.buffert = malloc(sizeof(u_int32_t) * width * height);
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
    display->buffer1 = NEWMEM(u_int32_t, display->sz+16); 
    display->buffer2 = NEWMEM(u_int32_t, display->sz+16);
    display->buffert = NEWMEM(u_int32_t, (width * FB_PIXEL_SZ) * (height* FB_PIXEL_SZ) * 4);
    display->zbuffer = NEWMEM(int16_t, width * height);

    display->draw_ptr = display->buffer1;
    return display;
}

#ifdef __cplusplus
}
#endif
