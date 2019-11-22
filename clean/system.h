#ifndef __CHITON_SYSTEM_H
#define __CHITON_SYSTEM_H 1

// global system configuration
typedef struct GlobalOpts {
    u_int8_t clear_exit;
    u_int8_t running;
    char *device;


    u_int8_t bb_min_x;
    u_int8_t bb_min_y;
    u_int8_t bb_max_x;
    u_int8_t bb_max_y;
} GlobalOpts;

typedef bool (*BoolCallback)();


// defined in (main) m3.cpp
extern GlobalOpts GOPTS;

// exit with
void Panic(int code, const char *format, ...);
// install signal handlers
void Setup_Handlers2(void);
// wait for func to return true, spin wait and test every usec seconds
void wait_for(BoolCallback *func, uint usec);

#endif
