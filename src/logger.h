#pragma once

#include <stdint.h>

#include <settings/settings.h>

#define LOG_TRACE   0
#define LOG_DEBUG   1
#define LOG_INFO    2
#define LOG_NOTICE  3
#define LOG_WARN    4
#define LOG_ERROR   5
#define LOG_CRIT    6

#define DEFAULT_LOG_LEVEL LOG_INFO

typedef struct logger_s logger_t;
extern logger_t* current_logger;

logger_t* init_logger(settings_t *settings);
void close_logger(logger_t *logger);

void base_log(int64_t level, logger_t *logger, const char *fmt, ...);
void base_log_perror(int64_t level, logger_t *logger, const char *fmt, ...);

#define log_trace(...)  base_log(LOG_TRACE, __VA_ARGS__)
#define log_debug(...)  base_log(LOG_DEBUG, __VA_ARGS__)
#define log_info(...)   base_log(LOG_INFO, __VA_ARGS__)
#define log_notice(...) base_log(LOG_NOTICE, __VA_ARGS__)
#define log_warn(...)   base_log(LOG_WARN, __VA_ARGS__)
#define log_error(...)  base_log(LOG_ERROR, __VA_ARGS__)
#define log_crit(...)   base_log(LOG_CRIT, __VA_ARGS__)

#define log_trace_perror(...)  base_log_perror(LOG_TRACE, __VA_ARGS__)
#define log_debug_perror(...)  base_log_perror(LOG_DEBUG, __VA_ARGS__)
#define log_info_perror(...)   base_log_perror(LOG_INFO, __VA_ARGS__)
#define log_notice_perror(...) base_log_perror(LOG_NOTICE, __VA_ARGS__)
#define log_warn_perror(...)   base_log_perror(LOG_WARN, __VA_ARGS__)
#define log_error_perror(...)  base_log_perror(LOG_ERROR, __VA_ARGS__)
#define log_crit_perror(...)   base_log_perror(LOG_CRIT, __VA_ARGS__)

#define CHECK_INVARIANT(cond, fmt, ...)                         \
    do {                                                        \
        if (!(cond)) {                                          \
            base_log(LOG_CRIT, current_logger,                  \
                     "Invariant failed in %s (%s:%d): " fmt,    \
                     __func__, __FILE__, __LINE__,              \
                     ##__VA_ARGS__);                            \
            abort(); /* or exit(EXIT_FAILURE) */                \
        }                                                       \
    } while(0)
