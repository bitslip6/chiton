/**
 * demos.c
 *
 * Copyright (c) 2019 Cory Marsh  <cory@bitslip6.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 *     1.  Redistributions of source code must retain the above copyright notice, this list of
 *         conditions and the following disclaimer.
 *     2.  Redistributions in binary form must reproduce the above copyright notice, this list
 *         of conditions and the following disclaimer in the documentation and/or other materials
 *         provided with the distribution.
 *     3.  Neither the name of the owner nor the names of its contributors may be used to endorse
 *         or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

// #define FBDRIVER 
// #define LEDDRIVER 

#define VERSION "0.03.01"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <getopt.h>
#include <math.h>
#include <time.h>

#include "demos.h"
#include "draw.h"
#include "system.h"
#include "retro/blob.h"




#ifdef DBDRIVER
#include <linux/fb.h>
#endif
#ifdef SDLDRIVER
#include <SDL2/SDL.h>
#endif

#ifdef LEDDRIVER
#include "clk.h"
#include "gpio.h"
#include "dma.h"
#include "pwm.h"
#include "version.h"
#include "ws2811.h"
#endif





DisplayBuffers SystemInit() {
    DisplayBuffers display;
    GlobalOpts opts;

    opts.running = 1;
    opts.clear_exit = 1;

    GOPTS = &opts;
    GDISP = &display;

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
#ifdef LEDDRIVER
    ws2811_return_t ret;
    int led_ctr = 0;
    for (int i=0; i<WIDTH; i++) {
        led_ctr += height_map[i];
    }
    display.buffer1 = malloc(sizeof(ws2811_led_t) * WIDTH * HEIGHT);
    display.buffer2 = malloc(sizeof(ws2811_led_t) * WIDTH * HEIGHT);
    display.buffert = malloc(sizeof(ws2811_led_t) * WIDTH * HEIGHT);
 
    ledstring.channel[0].count = led_ctr;
    if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS)
    {
        fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
    }
    printf("The WS8211 driver init successfully.\n");
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



/*
void PixelAA(DisplayBuffers *disp, float x, float y, unsigned char h, unsigned char s, unsigned char v) {
    unsigned char sx = (unsigned char)roundf(x);
    unsigned char sy = (unsigned char)roundf(y);
    printf("AA sx:%d sy:%d = X:%f Y:%f\n", sx, sy, x, y);

    for (char i=-1; i<2; i++) {
        unsigned char oy = (sy+i)*disp.width;
        float dx = x-1-sx;
        buffer[oy -1] = color;
    }
}
*/




/*
#ifdef FBDRIVER
void Render(u_int32_t *src) {
    int off = 0;
    for (int y=0; y<HEIGHT; y++) {
        off = y * WIDTH;
        for (int x=0; x<WIDTH; x++) {
            u_int32_t color = src[off++];
            for (int fb_y=0; fb_y<FBPIXEL_SZ; fb_y++) {
                int offy = ((y * FBPIXEL_SZ) + fb_y) * FBWIDTH;
                for (int fb_x=0; fb_x<FBPIXEL_SZ; fb_x++) {
                    fbm[offy + (x*FBPIXEL_SZ) + fb_x] = color;
                }
            }
        }
    }
}
}
*/










int main(int argc, char *argv[])
{
    srand(time(NULL) + getpid());
    //fbm = fb_init("/dev/fb0");
    AnimationOptions opts = {8, 32, 22, 128};
    DisplayBuffers disp = SystemInit();
    Animate_Blobs(&disp, &opts);
    printf("animate finished\n");
    exit(0);

/*
    while(running) {
        Animate_Pacman();
        animate_plasma();
        Animate_Matrix(30);
        Animate_Fractal();
        Animate_Roto_Textures("ftest1/", 0.01, 0.00, 32, 32, 399, 1);
    }
    exit(0);


    int pinkx, pinky;
    */
    /*
    char imgbuff[64];


    double xoff, yoff, rads;

    u_int32_t *tframe = ReadPNG("test.png", 0x00, &pinkx, &pinky);

    for (int x=0; x<WIDTH; x++) {
        for (int y=0; y<HEIGHT; y++) {
            printf("X: %d Y: %d = %x\n", x, y, tframe[(y*pinkx)+x]);
            Pixel(x, y, tframe[(y*pinkx)+x]);
        }
    }
    Render(buffer);

    exit(0);
    xoff = 42;
    yoff = 42;
    */
    /*
    for (int iptr=0; iptr<200; iptr++) {
        sprintf(imgbuff, "ftest1/D00%03d.png", iptr);
        rads += 0.08;

        u_int32_t *tframe = ReadPNG(imgbuff, 0x00, &pinkx, &pinky);
        printf("image %s W%d H%d\n", imgbuff, pinkx, pinky);

        Roto(tframe, pinkx, pinky, buffer, WIDTH, HEIGHT, rads, xoff, yoff, 1.0);
        Render(buffer);
        free(tframe);
        usleep(1000000 / 24);
    }
    exit(0);
    */


/*

    u_int32_t *frame[6];
    frame[0] = 0;
    frame[1] = 0;
    frame[2] = 0;
    frame[3] = 0;
    frame[4] = 0;
    printf("loading animate...\n");
    int xoff = 0;
    while(running) {
        if (xoff > pinkx) { xoff = 0; }
    for (int i=1; i<=4; i++) {
        char fname[32];
        sprintf(fname, "pacman%da.png", i);
        //sprintf(buffer, "pin1.png", i);
        if (frame[i] == 0) {
            frame[i] = ReadPNG(fname, 0x00, &pinkx, &pinky);
        }
            printf("read: %s %dX%d\n", fname, pinkx, pinky);
            for (int x=0; x<WIDTH; x++) {
                for (int y=0; y<HEIGHT; y++) {
                    //printf("X: %d Y: %d = %d\n", x, y, frame[i][(y*pinkx)+x]);
                    Pixel(x, y, frame[i][(y*pinkx)+x-xoff]);
                }
            }
            //DecayPer(2);
            Render(buffer);
            usleep(1000000/10);
        xoff++;
    }

    }
    sleep(1);
    
	//animate_matrix(20);
    exit(0);
    */
}
