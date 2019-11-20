#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "system.h"
#include "fb_driver.h"

DemoConf GCONF;


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
    printf("CTRL-C!\n");
    GCONF.running = 0;

    #ifdef SDLDRIVER
    SDL_DestroyRenderer(GDISP->renderer);
    SDL_DestroyWindow(GDISP->window);
    SDL_Quit();
    #endif
}


void SigHandlers(void) {
    //struct sigaction sa = (struct sigaction) { .sa_handler = ctrl_c_handler, };

    //sigaction(SIGINT, &sa, NULL);
    //sigaction(SIGTERM, &sa, NULL);
    signal(SIGINT, ctrl_c_handler);
    signal(SIGTERM, ctrl_c_handler);
    printf("call dig handlers\n");
}

void Setup_Handlers2(void) {
    SigHandlers();
}
