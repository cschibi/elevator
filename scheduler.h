#ifndef SCHEDULER_H
#define SCHEDULER_H

/* =========================================================
 * scheduler.h  –  Pluggable scheduling strategy interface
 *
 *  Strategy pattern: each algorithm implements the same
 *  vtable (SchedulerOps) so the building can swap them at
 *  runtime without touching elevator or building code.
 *
 *  To add a new algorithm:
 *    1. Implement the three functions below.
 *    2. Register a SchedulerOps entry in scheduler.c.
 *    3. Add a SCHED_* constant in config.h.
 * ========================================================= */

#include "config.h"
#include "elevator.h"

/* ── Request descriptor (floor call) ─────────────────────── */
typedef struct {
    int       floor;      /* which floor pressed the button  */
    Direction direction;  /* desired travel direction         */
    long      tick;       /* sim tick when request was made   */
} Request;

/* ── Scheduler vtable ─────────────────────────────────────── */
typedef struct {
    const char *name;

    /**
     * @brief Assign a single new request to the best elevator.
     * @param elevators  Array of elevator objects.
     * @param n          Number of elevators.
     * @param req        The new hall call to service.
     */
    void (*assign)(Elevator *elevators, int n, const Request *req);

    /**
     * @brief Per-tick hook – rebalance/re-route if needed.
     *        May be NULL for stateless algorithms.
     */
    void (*tick_hook)(Elevator *elevators, int n);

    /**
     * @brief Return name string.
     */
    const char *(*get_name)(void);

} SchedulerOps;

/* ── Public API ──────────────────────────────────────────── */

/**
 * @brief Retrieve the scheduler vtable for a given type.
 * @return Pointer to ops table, or NULL if unknown type.
 */
const SchedulerOps *scheduler_get(SchedulerType type);

/** Print a summary of all available schedulers. */
void scheduler_list(void);

#endif /* SCHEDULER_H */
