#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>


double time_it(void (*fp)());
void wait_for(bool(*func)(), uint usec);

void Panic(int code, const char *format, ...);