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

struct logger_s {
    int fd;
    int level;
    bool file_logging;
};

extern const char* log_levels[];
extern logger_t* current_logger;

logger_t* init_logger(settings_t *settings);
void close_logger(logger_t *logger);

void base_log(int64_t level, logger_t *logger, const char *fmt, ...);
void base_log_perror(int64_t level, logger_t *logger, const char *fmt, ...);

void init_file_logging(logger_t *logger, const char* log_file);
void init_stdout_logging(logger_t *logger);

#define log_trace(...)  base_log(LOG_TRACE, logger, ##__VA_ARGS__)
#define log_debug(...)  base_log(LOG_DEBUG, logger, ##__VA_ARGS__)
#define log_info(...)   base_log(LOG_INFO, logger, ##__VA_ARGS__)
#define log_notice(...) base_log(LOG_NOTICE, logger, ##__VA_ARGS__)
#define log_warn(...)   base_log(LOG_WARN, logger, ##__VA_ARGS__)
#define log_error(...)  base_log(LOG_ERROR, logger, ##__VA_ARGS__)
#define log_crit(...)   base_log(LOG_CRIT, logger, ##__VA_ARGS__)

#define log_ptrace(...)  base_log_perror(LOG_TRACE, logger, ##__VA_ARGS__)
#define log_pdebug(...)  base_log_perror(LOG_DEBUG, logger, ##__VA_ARGS__)
#define log_pinfo(...)   base_log_perror(LOG_INFO, logger, ##__VA_ARGS__)
#define log_pnotice(...) base_log_perror(LOG_NOTICE, logger, ##__VA_ARGS__)
#define log_pwarn(...)   base_log_perror(LOG_WARN, logger, ##__VA_ARGS__)
#define log_perror(...)  base_log_perror(LOG_ERROR, logger, ##__VA_ARGS__)
#define log_pcrit(...)   base_log_perror(LOG_CRIT, logger, ##__VA_ARGS__)

#define CHECK_INVARIANT(cond, fmt, ...)                                 \
    do {                                                                \
        if (!(cond)) {                                                  \
            base_log(LOG_CRIT, logger,                                  \
                     "Invariant failed in %s (%s:%d): " fmt,            \
                     __func__, __FILE__, __LINE__,                      \
                     ##__VA_ARGS__);                                    \
            abort();                                                    \
        }                                                               \
    } while(0)

#define PANIC(fmt, ...)                                            \
    do {                                                      \
        base_log(LOG_CRIT, logger,                            \
                 "Invariant failed in %s (%s:%d): " fmt,      \
                 __func__, __FILE__, __LINE__,                \
                 ##__VA_ARGS__);                              \
        abort();                                              \
    } while(0)
