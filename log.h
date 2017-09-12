#ifndef LOG_H
#define LOG_H
#include <cstdio>

#define dlog(fmt, ...)  \
    fprintf(stdout, "file: %s, line: %d, " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#endif // LOG_H
