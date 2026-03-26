#ifndef BUILDING_H
#define BUILDING_H

/* =========================================================
 * building.h  –  Top-level simulation coordinator
 *
 *  The Building owns:
 *    • The elevator array
 *    • The pending request queue
 *    • The active scheduler strategy
 *    • The simulation tick counter
 * ========================================================= */

#include "config.h"
#include "elevator.h"
#include "scheduler.h"

/* ── Circular request queue ──────────────────────────────── */
typedef struct {
    Request   items[MAX_REQUESTS];
    int       head;
    int       tail;
    int       count;
} RequestQueue;

/* ── Building (simulation root) ──────────────────────────── */
typedef struct {
    SimConfig          config;
    Elevator           elevators[MAX_ELEVATORS];
    RequestQueue       queue;
    const SchedulerOps *sched;
    long               tick;

    /* Metrics */
    long               total_requests;
    long               total_served;
} Building;

/* ── Lifecycle ────────────────────────────────────────────── */
int  building_init(Building *b, const SimConfig *cfg);
void building_destroy(Building *b);

/* ── Request submission ──────────────────────────────────── */
/**
 * @brief Submit a hall call (floor button press).
 * @return 0 on success, -1 if queue full.
 */
int building_request(Building *b, int floor, Direction dir);

/* ── Simulation step ─────────────────────────────────────── */
/**
 * @brief Advance simulation by one tick.
 *        Dispatches queued requests, ticks all elevators.
 */
void building_tick(Building *b);

/* ── Display ─────────────────────────────────────────────── */
void building_print_status(const Building *b);
void building_print_metrics(const Building *b);

/* ── Scheduler swap (runtime) ────────────────────────────── */
int  building_set_scheduler(Building *b, SchedulerType type);

#endif /* BUILDING_H */
