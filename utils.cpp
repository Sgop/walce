#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "utils.h"
#include "log.h"

static char* next = NULL;

char* arg_next_sep(char sep)
{
    char* line = next;
    char* temp;
    
    if (!next)
        return NULL;
        
    while (isspace(*line)) 
        line++;

    if (*line == 0)
        return NULL;
        
    temp = strchr(line, sep);
        
    if (temp)
    {
        *temp++ = 0;
        next = temp;
    }
    else
        next = NULL;

    return line;
}

char* arg_next()
{
    return arg_next_sep(' ');
}

char* arg_start(char* line)
{
    next = line;
    return arg_next();
}

char* arg_until(const char* stop)
{
    char* temp;
    char* line = next;
    int len = strlen(stop);
    
    if (!next)
        return NULL;
    while (isspace(*line)) 
        line++;
    if (*line == 0)
        return NULL;

    temp = line;
        
    while (temp)
    {
        if (strncmp(temp, stop, len) == 0)
            break;
        
        temp = strchr(temp+1, ' ');
        if (temp)
            temp++;
    }
        
    if (temp)
    {
        *(temp-1) = 0;
        next = temp;
    }
    else
        next = NULL;

    return line;
}

char* arg_rest(void)
{
    return next;
}


