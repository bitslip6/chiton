#ifndef __SYSTEM_H
#define __SYSTEM_H 1


#ifdef __cplusplus
extern "C" {
#endif

// exit with
void Panic(int code, const char *format, ...);
// install signal handlers
void Setup_Handlers2(void);


#ifdef __cplusplus
}
#endif

#endif
