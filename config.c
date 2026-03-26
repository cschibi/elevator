/* =========================================================
 * config.c  –  Config validation & display
 * ========================================================= */

#include "config.h"
#include "logger.h"
#include <stdio.h>

static const char *sched_names[SCHED_COUNT] = {
    [SCHED_FCFS] = "FCFS",
    [SCHED_LOOK] = "LOOK",
    [SCHED_SSTF] = "SSTF",
};

/* ── config_validate ─────────────────────────────────────── */
int config_validate(const SimConfig *cfg)
{
    if (!cfg) return -1;

    if (cfg->num_floors < 2 || cfg->num_floors > MAX_FLOORS) {
        log_error("config: num_floors %d out of range [2,%d]",
                  cfg->num_floors, MAX_FLOORS);
        return -1;
    }
    if (cfg->num_elevators < 1 || cfg->num_elevators > MAX_ELEVATORS) {
        log_error("config: num_elevators %d out of range [1,%d]",
                  cfg->num_elevators, MAX_ELEVATORS);
        return -1;
    }
    if ((int)cfg->scheduler < 0 || cfg->scheduler >= SCHED_COUNT) {
        log_error("config: unknown scheduler id %d", (int)cfg->scheduler);
        return -1;
    }
    if (cfg->tick_ms < 0) {
        log_error("config: tick_ms must be >= 0");
        return -1;
    }
    return 0;
}

/* ── config_print ────────────────────────────────────────── */
void config_print(const SimConfig *cfg)
{
    if (!cfg) return;
    printf("=== Simulator Configuration ===\n");
    printf("  Floors      : %d\n",  cfg->num_floors);
    printf("  Elevators   : %d\n",  cfg->num_elevators);
    printf("  Scheduler   : %s\n",  sched_names[cfg->scheduler]);
    printf("  Tick (ms)   : %d\n",  cfg->tick_ms);
    printf("  Log to file : %s\n",  cfg->log_to_file ? cfg->log_path : "no");
    printf("===============================\n");
}
