#ifndef LOGGER_H
#define LOGGER_H

/* =========================================================
 * logger.h  –  Levelled audit-trail logger
 *
 *  Levels (ascending severity):
 *    DEBUG < INFO < WARN < ERROR
 *
 *  Usage:
 *    log_info("Elevator %d arrived at floor %d", id, floor);
 * ========================================================= */

#include <stdio.h>

/* ── Log levels ──────────────────────────────────────────── */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_LEVEL_COUNT
} LogLevel;

/* ── Init / teardown ─────────────────────────────────────── */
/**
 * @brief Initialise logger.
 * @param min_level  Minimum level to emit.
 * @param filepath   If non-NULL, tee output to this file.
 * @return 0 on success, -1 on error.
 */
int  logger_init(LogLevel min_level, const char *filepath);
void logger_close(void);

/* ── Core emit ───────────────────────────────────────────── */
void logger_log(LogLevel level, const char *file, int line,
                const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

/* ── Convenience macros (capture __FILE__ / __LINE__) ────── */
#define log_debug(...) logger_log(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define log_info(...)  logger_log(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define log_warn(...)  logger_log(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define log_error(...) logger_log(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

/* ── Tick annotation (sim-time prefix) ───────────────────── */
void logger_set_tick(long tick);

#endif /* LOGGER_H */
