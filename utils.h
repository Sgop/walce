#ifndef UTILS_H__
#define UTILS_H__

char* arg_start(char* line);
char* arg_next_sep(char sep);
char* arg_next(void);
char* arg_until(const char* stop);
char* arg_rest(void);

#endif
