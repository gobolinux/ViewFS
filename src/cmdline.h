
#ifndef CMDLINE_H
#define CMDLINE_H

#include <stdbool.h>

void parse_cmdline(int argc, char** argv, char** out_watch_dir, char** out_mountpoint, int* out_foreground);

#endif
