#ifndef __SYSTEM_H
#define __SYSTEM_H 1

#include "demos.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FB_PIXEL_SZ 5

// exit with
void Panic(int code, const char *format, ...);
// install signal handlers
void Setup_Handlers2(void);

DisplayBuffers* SystemInit(u8 width, u8 height);
void Setup_Costume(Costume_t *c);

#ifdef __cplusplus
}
#endif

#endif
