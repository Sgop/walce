#ifndef LOG_H__
#define LOG_H__

#include <stdio.h>

enum {
    MODE_GUI  = 0x01,
};

void log_init();
void log_exit();
void log_set_mode(int mode);
FILE* log_get_fd();

void log_line(const char* fmt, ...);
void log_depth(int depth, const char* fmt, ...);
void log_error(const char* fmt, ...);
void send_line(const char* fmt, ...);
char* get_line();

namespace walce
{

  class Communicate
  {
  public:
    Communicate();
  protected:

  };

  extern Communicate Comm;

}



#endif

