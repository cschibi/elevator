#ifndef CONFIG_H
#define CONFIG_H

/* =========================================================
 * config.h  –  Compile-time constants & runtime config
 * ========================================================= */

#include <stddef.h>

/* ── Hard limits ─────────────────────────────────────────── */
#define MAX_FLOORS      90
#define MAX_ELEVATORS   6
#define MAX_REQUESTS    128  /* circular request queue depth  */
#define MAX_LOG_MSG     256  /* max chars per log line        */
#define DOOR_DWELL_TICKS      2   /* ticks doors stay open        */
#define IDLE_REPOSITION_TICKS 10  /* idle ticks before repositioning */

/* ── Scheduler strategy IDs ──────────────────────────────── */
typedef enum {
    SCHED_FCFS  = 0,   /* First Come First Served           */
    SCHED_LOOK  = 1,   /* LOOK (bi-directional scan)        */
    SCHED_SSTF  = 2,   /* Shortest Seek Time First          */
    SCHED_COUNT        /* sentinel – keep last              */
} SchedulerType;

/* ── Runtime config struct ───────────────────────────────── */
typedef struct {
    int           num_floors;       /* 2 .. MAX_FLOORS       */
    int           num_elevators;    /* 1 .. MAX_ELEVATORS    */
    SchedulerType scheduler;        /* strategy in use       */
    int           log_to_file;      /* 0 = stdout only       */
    char          log_path[128];    /* path when log_to_file */
    int           tick_ms;          /* ms per sim tick (0=instant) */
} SimConfig;

/* ── Default config ──────────────────────────────────────── */
#define DEFAULT_CONFIG { \
    .num_floors    = MAX_FLOORS,    \
    .num_elevators = MAX_ELEVATORS, \
    .scheduler     = SCHED_LOOK,    \
    .log_to_file   = 0,             \
    .log_path      = "elevator.log",\
    .tick_ms       = 0              \
}

/* ── Public API ──────────────────────────────────────────── */
void config_print(const SimConfig *cfg);
int  config_validate(const SimConfig *cfg);

#endif /* CONFIG_H */
