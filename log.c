#include <stdio.h>
#include <stdarg.h>
#include "log.h"
#include "thread.h"

static FILE* debug = NULL;
static int LogMode = 0;

void log_init()
{
    debug = fopen("debug.log", "w");
}

void log_exit()
{
    if (debug)
        fclose(debug);
}

void log_set_mode(int mode)
{
    LogMode = mode;
}

FILE* log_get_fd(void)
{
    if (LogMode & MODE_GUI)
        return debug;
    else
        return stdout;
    return NULL;
}


static void log_line_full(const char* prefix, const char* fmt, va_list ap)
{
    FILE* fd = log_get_fd();

    IO_enter();
    fprintf(fd, "%s", prefix);
    vfprintf(fd, fmt,  ap);
    fprintf(fd, "\n");
    fflush(fd);
    IO_leave();
}

void log_line(const char* fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    log_line_full(" | ", fmt, ap);
    va_end(ap);
}

void log_depth(int depth, const char* fmt, ...)
{
    va_list ap;
    char prefix[1024];
    
    va_start(ap, fmt);
    sprintf(prefix, " | (%02d)%*s", depth, 2*depth, "");
    log_line_full(prefix, fmt, ap);
    va_end(ap);
}

void log_error(const char* fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    log_line_full("*** ", fmt, ap);
    va_end(ap);
}

static void log_input(const char* fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    log_line_full(">  ", fmt, ap);
    va_end(ap);
}

void send_line(const char* fmt, ...)
{
    va_list ap;

    if (LogMode & MODE_GUI)
    {
        IO_enter();
        va_start(ap, fmt);
        vfprintf(stdout, fmt, ap);
        fprintf(stdout, "\n");
        va_end(ap);
        fflush(stdout);
        IO_leave();
    }
    va_start(ap, fmt);
    log_line_full(" < ", fmt, ap);
    va_end(ap);
}

char* get_line()
{
    static char line[1000];
    char* next = line;
       
    while ((*next = fgetc(stdin)) != EOF)
    {
        if (*next == 10)
        {
            *next = 0;
            break;
        }
        next++;
    }
    log_input("%s", line);
    
    return line;
}

