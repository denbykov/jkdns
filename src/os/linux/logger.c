#include <logger.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <errno.h>

static const char *levels[] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "NOTICE",
    "WARN",
    "ERROR",
    "CRIT"
};

struct logger_s {
    int fd;
    int level;
    bool file_logging;
};

logger_t* current_logger = NULL;

static void init_file_logging(logger_t *logger, const char* log_file) {
    if (log_file == NULL) {
        fprintf(stderr, "init_file_logging: log_file is NULL\n");
        exit(1);
    }

    int fd = open(log_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("open log file");
        exit(1);
    }

    logger->fd = fd;
    logger->file_logging = true;
}

static void init_stdout_logging(logger_t *logger) {
    logger->fd = STDOUT_FILENO;
}

logger_t* init_logger(settings_t *settings) {
    logger_t *logger = calloc(1, sizeof(logger_t));

    logger->fd = -1;
    logger->level = -1;
    logger->file_logging = false;

    if (logger == NULL) {
        perror("init_logger.calloc");
        exit(1);
    }

    if (settings == NULL) {
        fprintf(stderr, "init_logger: settings is NULL\n");
        exit(1);
    }

    if (settings->log_file != NULL) {
        init_file_logging(logger, settings->log_file);
    } else {
        init_stdout_logging(logger);
    }

    if (settings->log_level == NULL) {
        logger->level = DEFAULT_LOG_LEVEL;
    } else {
        size_t num_levels = sizeof(levels) / sizeof(levels[0]);
        for (size_t level = 0; level < num_levels; level++) {
            if (strcmp(settings->log_level, levels[level]) == 0) {
                logger->level = (int)level;
            }
        }

        if (logger->level == -1) {
            fprintf(stderr, "init_logger: unrecognized log level %s\n", settings->log_level);
            exit(1);
        }
    }

    return logger;
}

void base_log(int64_t level, logger_t* logger, const char *fmt, ...) {
    if (level < logger->level) {
        return;
    }

    int saved_errno = errno;

    char buf[4096];
    size_t len = 0;

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    len += strftime(buf + len, sizeof(buf) - len, "%Y-%m-%d %H:%M:%S ", &tm);

    len += snprintf(buf + len, sizeof(buf) - len, "[%s] ", levels[level]);

    va_list ap;
    va_start(ap, fmt);
    len += vsnprintf(buf + len, sizeof(buf) - len, fmt, ap);
    va_end(ap);

    if (len < sizeof(buf) - 1) {
        buf[len++] = '\n';
    }

    write(logger->fd, buf, len);

    errno = saved_errno;
}

void base_log_perror(int64_t level, logger_t* logger, const char *fmt, ...) {
    if (level < logger->level) {
        return;
    }

    char user_msg[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(user_msg, sizeof(user_msg), fmt, ap);
    va_end(ap);

    char final_msg[4096];
    snprintf(final_msg, sizeof(final_msg), "%s: %s", user_msg, strerror(errno));

    base_log(level, logger, "%s", final_msg);
}

void close_logger(logger_t* logger) {
    if (logger == NULL) {
        fprintf(stderr, "close_logger: logger is NULL\n");
        exit(1);
    }

    if (logger->file_logging) {
        close(logger->fd);
    }

    free(logger);
}
