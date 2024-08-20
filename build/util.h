#ifndef INCLUDE_UTIL_H
#define INCLUDE_UTIL_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

void *
xmalloc(size_t n);
void *
xcalloc(size_t n, size_t sz);
void *
xrealloc(void *p, size_t n);

/* Logging */

/* ANSI escapes */
#define T_DIM "\x1b[2m"
#define T_BOLD "\x1b[1m"
#define T_ITAL "\x1b[3m"
#define T_LINE "\x1b[4m"
#define T_RED "\x1b[38;5;1m"
#define T_GREEN "\x1b[38;5;2m"
#define T_YELLOW "\x1b[38;5;3m"
#define T_BLUE "\x1b[38;5;4m"
#define T_NORM "\x1b[0m"

enum MsgT {
	msgt_info = 0,
	msgt_warn,
	msgt_err,
	msgt_raw,
	msgt_end,
};
static const char *_msgtstr[msgt_end] =
	{T_BLUE, T_YELLOW, T_RED, T_NORM
}; /* not a macro so we don't forget to update it */

static const char *_dbg_file_global = NULL;
static int _dbg_line_global = 0;

#define log(type, msg, ...)                                 \
	do {                                                    \
		if (type == msgt_raw) {                             \
			printf(msg "\n", __VA_ARGS__);                  \
			break;                                          \
		}                                                   \
		printf(                                             \
			T_DIM "%s:%d: " T_NORM "%s" msg T_NORM "\n",    \
			_dbg_file_global ? _dbg_file_global : __FILE__, \
			_dbg_line_global ? _dbg_line_global : __LINE__, \
			_msgtstr[type],                                 \
			__VA_ARGS__                                     \
		);                                                  \
		fflush(stdout);                                     \
	} while (0)

#define die(msg, ...)                    \
	do {                                 \
		log(msgt_err, msg, __VA_ARGS__); \
		exit(1);                         \
	} while (0)

#endif
