#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void error_report(int exitCode, const char *message, ...)
{
    va_list args;
    va_start(args, message);

    vfprintf(stderr, message, args);
    fprintf(stderr, "\n");

    va_end(args);
    exit(exitCode);
}