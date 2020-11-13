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
#include "system.h"
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
}

//-----------------------------------------------------------------------------
//  Public Functions
//-----------------------------------------------------------------------------

namespace sys
{
    void init_signal_handlers() {
        TRACE("");
        std::signal(SIGINT, ctrl_c_handler);
        std::signal(SIGTERM, ctrl_c_handler);
    }

    void ctrl_c_handler(int signum)
    {
        WARN("---> Caught CTRL+C!");
        shutdown_now(signum);
    }

    void register_shutdown_fn(shutdown_fn_t func)
    {
        v_fns.push_back(func);
    }

    void shutdown_now(int exit_code) {
        TRACE("shutdown");
        
        if (v_fns.empty() == false) {
            INFO("Executing registered shutdown functions");
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

        opts->running = 1;
        opts->cal = new std::map<int, VixelCalibration*>;
        opts->log_level = new std::map<std::string, int>;
        GOPTS = opts;

        display->width = width;
        display->height = height;
        display->invx = (float)1/width;
        display->invy = (float)1/height;
        display->sz = width * height;

        init_signal_handlers();

#ifdef FBDRIVER
        display->fd = open("/dev/fb1", O_RDWR);
        PANIC_IF(display->fd < 0, "Error: cannot open framebuffer device: /dev/fb1", EXIT_SYSCALL_ERROR);
        
        display->raw = (db_t *)mmap(0, display->sz, PROT_READ | PROT_WRITE, MAP_SHARED, display->fd, 0);
        PANIC_IF(display->raw == MAP_FAILED, "Error: cannot open framebuffer device: /dev/fb1", EXIT_SYSCALL_ERROR);
        
        display->buffer1 = (db_t*) malloc(sizeof(db_t) * width * height);
        display->buffer2 = (db_t*) malloc(sizeof(db_t) * width * height);
        // display->buffert = (u_int32_t*) malloc(sizeof(u_int32_t) * width * height);
        INFO("The framebuffer and virtual buffer mapped successfully");
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
}
