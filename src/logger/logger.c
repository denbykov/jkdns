#include "logger/logger.h"

#include <stdlib.h>
#include <string.h>

const char *log_levels[] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "NOTICE",
    "WARN",
    "ERROR",
    "CRIT"
};

logger_t* current_logger = NULL;

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
        size_t num_levels = sizeof(log_levels) / sizeof(log_levels[0]);
        for (size_t level = 0; level < num_levels; level++) {
            if (strcmp(settings->log_level, log_levels[level]) == 0) {
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
