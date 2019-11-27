#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef __cplusplus
    extern "C" {
        #include "input.h"
        #include "system.h"
    }
#else
    #include "input.h"
    #include "system.h"
#endif

double time_it(void (*fp)()) {
    struct timeval start, end;
    gettimeofday(&start, NULL);
    fp();
    gettimeofday(&end, NULL);
    double time = (end.tv_sec - start.tv_sec) * 1e6;
    time += (end.tv_usec - start.tv_usec) * 1e-6;
    return time;
}

/**
 * tests the return value of func every usec micro seconds and blocks
 * when func returns true, wait_for returns
 */
void wait_for(bool(*func)(), uint usec) {
    while(true) {
        if ((func() == false)) {
            usleep(usec);
            continue;
        }
        break;
    }
}



/**
 * panic with error code
 */
void Panic(int code, const char *format, ...) {
    va_list args;
    va_start(args, format);

    printf("SYSTEM PANIC\n");
    vprintf(format, args);
    va_end(args);
    printf("\n");

    exit(code);
}


/**
 * end on control c
 */
 /*
void ctrl_c_handler(int signum) {
	(void)(signum);
    printf("CTRL-C!\n");
    GOPTS->running = 0;

    #ifdef SDLDRIVER
    SDL_DestroyRenderer(GDISP->renderer);
    SDL_DestroyWindow(GDISP->window);
    SDL_Quit();
    #endif
}


void SigHandlers(void) {
    struct sigaction sa = (struct sigaction) { .sa_handler = ctrl_c_handler, };

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}
*/
