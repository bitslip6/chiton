#ifndef SYSTEM_H
#define SYSTEM_H 1

// exit with
void Panic(int code, const char *format, ...);
// install signal handlers
void Setup_Handlers2(void);

#endif
