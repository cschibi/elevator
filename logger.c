/* =========================================================
 * logger.c  –  Thread-safe(ish) levelled logger
 * ========================================================= */

#include "logger.h"
#include <stdarg.h>
#include <string.h>
#include <time.h>

/* ── Internal state ──────────────────────────────────────── */
static struct {
    LogLevel  min_level;
    FILE     *fp;          /* file handle, NULL = stdout only */
    long      tick;        /* current sim tick                */
    int       initialised;
} g_log = { LOG_INFO, NULL, 0, 0 };

static const char *level_tag[LOG_LEVEL_COUNT] = {
    [LOG_DEBUG] = "DBG",
    [LOG_INFO]  = "INF",
    [LOG_WARN]  = "WRN",
    [LOG_ERROR] = "ERR",
};

/* ANSI colours for terminal (skipped when writing to file) */
static const char *level_colour[LOG_LEVEL_COUNT] = {
    [LOG_DEBUG] = "\033[0;37m",   /* grey  */
    [LOG_INFO]  = "\033[0;32m",   /* green */
    [LOG_WARN]  = "\033[0;33m",   /* yellow*/
    [LOG_ERROR] = "\033[0;31m",   /* red   */
};
#define ANSI_RESET "\033[0m"

/* ── logger_init ─────────────────────────────────────────── */
int logger_init(LogLevel min_level, const char *filepath)
{
    g_log.min_level   = min_level;
    g_log.tick        = 0;
    g_log.initialised = 1;

    if (filepath && filepath[0] != '\0') {
        g_log.fp = fopen(filepath, "a");
        if (!g_log.fp) {
            fprintf(stderr, "[logger] WARNING: cannot open log file '%s'\n",
                    filepath);
            /* non-fatal – fall back to stdout only */
        }
    }
    return 0;
}

/* ── logger_close ────────────────────────────────────────── */
void logger_close(void)
{
    if (g_log.fp) {
        fclose(g_log.fp);
        g_log.fp = NULL;
    }
    g_log.initialised = 0;
}

/* ── logger_set_tick ─────────────────────────────────────── */
void logger_set_tick(long tick)
{
    g_log.tick = tick;
}

/* ── logger_log ──────────────────────────────────────────── */
void logger_log(LogLevel level, const char *file, int line,
                const char *fmt, ...)
{
    if (!g_log.initialised)    return;
    if (level < g_log.min_level) return;

    /* Strip path prefix from __FILE__ */
    const char *base = strrchr(file, '/');
    base = base ? base + 1 : file;

    /* Format the caller's message */
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    /* ── stdout (coloured) ── */
    fprintf(stdout, "%s[T%04ld][%s][%s:%d] %s%s\n",
            level_colour[level],
            g_log.tick,
            level_tag[level],
            base, line,
            msg,
            ANSI_RESET);
    fflush(stdout);

    /* ── file (plain) ── */
    if (g_log.fp) {
        fprintf(g_log.fp, "[T%04ld][%s][%s:%d] %s\n",
                g_log.tick, level_tag[level], base, line, msg);
        fflush(g_log.fp);
    }
}
