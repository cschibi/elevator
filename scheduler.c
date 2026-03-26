/* =========================================================
 * scheduler.c  –  FCFS, LOOK, and SSTF strategy impls
 * ========================================================= */

#include "scheduler.h"
#include "logger.h"
#include <stdlib.h>   /* abs() */
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════
 * Helpers
 * ═══════════════════════════════════════════════════════════ */

/** Euclidean distance between elevator's current floor and target floor. */
static int distance(const Elevator *e, int target)
{
    int d = e->current_floor - target;
    return d < 0 ? -d : d;
}

/**
 * Cost of dispatching elevator e to service a request at req_floor
 * heading in req_dir.  Lower is better.
 *
 *  Penalise:
 *   +0   if e is already heading toward req_floor (same direction)
 *   +MAX_FLOORS if e would have to reverse direction
 */
static int dispatch_cost(const Elevator *e, int req_floor, Direction req_dir)
{
    if (e->state == ELEV_FAULT) return 9999;

    int base = distance(e, req_floor);

    /* Same direction & on the way → minimal extra cost */
    if (e->direction == req_dir || e->direction == DIR_IDLE)
        return base;

    /* Opposite direction → penalise heavily */
    return base + MAX_FLOORS;
}

/* ═══════════════════════════════════════════════════════════
 * FCFS – First Come, First Served
 *   Assign every request to the elevator with the smallest
 *   current queue (fewest pending stops).
 * ═══════════════════════════════════════════════════════════ */

static int fcfs_pending(const Elevator *e)
{
    int cnt = 0;
    for (int f = 0; f < MAX_FLOORS; f++)
        cnt += e->stops[f];
    return cnt;
}

static void fcfs_assign(Elevator *elevators, int n, const Request *req)
{
    int best = 0, best_cnt = fcfs_pending(&elevators[0]);
    for (int i = 1; i < n; i++) {
        int cnt = fcfs_pending(&elevators[i]);
        if (cnt < best_cnt) { best_cnt = cnt; best = i; }
    }
    elevator_add_stop(&elevators[best], req->floor);
    log_info("FCFS: request floor %d -> Elevator %d", req->floor, best);
}

static const char *fcfs_name(void) { return "FCFS"; }

static const SchedulerOps fcfs_ops = {
    .name      = "FCFS",
    .assign    = fcfs_assign,
    .tick_hook = NULL,
    .get_name  = fcfs_name,
};

/* ═══════════════════════════════════════════════════════════
 * LOOK – Bi-directional scan
 *   Pick the elevator whose current trajectory naturally
 *   passes closest to the requested floor.
 * ═══════════════════════════════════════════════════════════ */

static void look_assign(Elevator *elevators, int n, const Request *req)
{
    int best = 0;
    int best_cost = dispatch_cost(&elevators[0], req->floor, req->direction);

    for (int i = 1; i < n; i++) {
        int cost = dispatch_cost(&elevators[i], req->floor, req->direction);
        if (cost < best_cost) { best_cost = cost; best = i; }
    }
    elevator_add_stop(&elevators[best], req->floor);
    log_info("LOOK: request floor %d dir=%s -> Elevator %d (cost %d)",
             req->floor, dir_name(req->direction), best, best_cost);
}

static const char *look_name(void) { return "LOOK"; }

static const SchedulerOps look_ops = {
    .name      = "LOOK",
    .assign    = look_assign,
    .tick_hook = NULL,
    .get_name  = look_name,
};

/* ═══════════════════════════════════════════════════════════
 * SSTF – Shortest Seek Time First
 *   Assign to whichever elevator is physically closest,
 *   ignoring direction (greedy, may cause starvation).
 * ═══════════════════════════════════════════════════════════ */

static void sstf_assign(Elevator *elevators, int n, const Request *req)
{
    int best = -1, best_d = MAX_FLOORS + 1;
    for (int i = 0; i < n; i++) {
        if (elevators[i].state == ELEV_FAULT) continue;
        int d = distance(&elevators[i], req->floor);
        if (d < best_d) { best_d = d; best = i; }
    }
    if (best < 0) {
        log_warn("SSTF: all elevators faulted – request floor %d dropped",
                 req->floor);
        return;
    }
    elevator_add_stop(&elevators[best], req->floor);
    log_info("SSTF: request floor %d -> Elevator %d (dist %d)",
             req->floor, best, best_d);
}

static const char *sstf_name(void) { return "SSTF"; }

static const SchedulerOps sstf_ops = {
    .name      = "SSTF",
    .assign    = sstf_assign,
    .tick_hook = NULL,
    .get_name  = sstf_name,
};

/* ═══════════════════════════════════════════════════════════
 * Registry
 * ═══════════════════════════════════════════════════════════ */

static const SchedulerOps *registry[SCHED_COUNT] = {
    [SCHED_FCFS] = &fcfs_ops,
    [SCHED_LOOK] = &look_ops,
    [SCHED_SSTF] = &sstf_ops,
};

const SchedulerOps *scheduler_get(SchedulerType type)
{
    if ((int)type < 0 || type >= SCHED_COUNT) return NULL;
    return registry[type];
}

void scheduler_list(void)
{
    printf("Available schedulers:\n");
    for (int i = 0; i < SCHED_COUNT; i++) {
        if (registry[i])
            printf("  [%d] %s\n", i, registry[i]->name);
    }
}
