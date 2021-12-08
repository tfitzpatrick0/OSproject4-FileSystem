/* logging.h: Logging Macros */

#ifndef LOGGING_H
#define LOGGING_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Logging Macros */

#ifndef NDEBUG
#define debug(M, ...) \
    fprintf(stderr, "DEBUG %s:%d:%s: " M "\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define debug(M, ...)
#endif

#define info(M, ...) \
    fprintf(stderr, "INFO  " M "\n", ##__VA_ARGS__)

#define error(M, ...) \
    fprintf(stderr, "ERROR " M "\n", ##__VA_ARGS__)

#endif

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
