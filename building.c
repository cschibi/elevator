/* =========================================================
 * building.c  –  Simulation coordinator
 * ========================================================= */

#include "building.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>

/* ── building_init ───────────────────────────────────────── */
int building_init(Building *b, const SimConfig *cfg)
{
    if (!b || !cfg) return -1;
    if (config_validate(cfg) != 0) return -1;

    memset(b, 0, sizeof(*b));
    b->config = *cfg;
    b->tick   = 0;

    for (int i = 0; i < cfg->num_elevators; i++)
        elevator_init(&b->elevators[i], i);

    b->sched = scheduler_get(cfg->scheduler);
    if (!b->sched) {
        log_error("building_init: unknown scheduler %d", cfg->scheduler);
        return -1;
    }

    log_info("Building initialised: %d floors, %d elevators, scheduler=%s",
             cfg->num_floors, cfg->num_elevators, b->sched->name);
    return 0;
}

/* ── building_destroy ────────────────────────────────────── */
void building_destroy(Building *b)
{
    if (!b) return;
    log_info("Building destroyed after %ld ticks", b->tick);
    memset(b, 0, sizeof(*b));
}

/* ── building_set_scheduler ──────────────────────────────── */
int building_set_scheduler(Building *b, SchedulerType type)
{
    if (!b) return -1;
    const SchedulerOps *ops = scheduler_get(type);
    if (!ops) {
        log_error("building_set_scheduler: unknown type %d", type);
        return -1;
    }
    b->sched = ops;
    b->config.scheduler = type;
    log_info("Scheduler switched to %s", ops->name);
    return 0;
}

/* ── Queue helpers ───────────────────────────────────────── */
static int queue_push(RequestQueue *q, const Request *r)
{
    if (q->count >= MAX_REQUESTS) return -1;
    q->items[q->tail] = *r;
    q->tail = (q->tail + 1) % MAX_REQUESTS;
    q->count++;
    return 0;
}

static int queue_pop(RequestQueue *q, Request *r)
{
    if (q->count == 0) return -1;
    *r = q->items[q->head];
    q->head = (q->head + 1) % MAX_REQUESTS;
    q->count--;
    return 0;
}

/* ── building_request ────────────────────────────────────── */
int building_request(Building *b, int floor, Direction dir)
{
    if (!b) return -1;
    if (floor < 0 || floor >= b->config.num_floors) {
        log_warn("building_request: floor %d out of range", floor);
        return -1;
    }
    if (dir == DIR_IDLE) {
        log_warn("building_request: DIR_IDLE is not a valid hall call direction");
        return -1;
    }

    Request r = { .floor = floor, .direction = dir, .tick = b->tick };
    if (queue_push(&b->queue, &r) != 0) {
        log_warn("building_request: queue full – request dropped");
        return -1;
    }
    b->total_requests++;
    log_info("Request queued: floor %d dir=%s (queue depth %d)",
             floor, dir_name(dir), b->queue.count);
    return 0;
}

/* ── building_tick ───────────────────────────────────────── */
void building_tick(Building *b)
{
    if (!b) return;

    logger_set_tick(b->tick);

    /* 1. Dispatch all pending requests via the active scheduler */
    Request r;
    while (queue_pop(&b->queue, &r) == 0) {
        b->sched->assign(b->elevators, b->config.num_elevators, &r);
    }

    /* 2. Optional per-tick hook (e.g. rebalancing) */
    if (b->sched->tick_hook)
        b->sched->tick_hook(b->elevators, b->config.num_elevators);

    /* 3. Tick every elevator */
    for (int i = 0; i < b->config.num_elevators; i++) {
        int served = elevator_tick(&b->elevators[i]);
        b->total_served += served;
    }

    b->tick++;
}

/* ── building_print_status ───────────────────────────────── */
void building_print_status(const Building *b)
{
    if (!b) return;
    printf("\n── Tick %ld ─────────────────────────────\n", b->tick);
    for (int i = 0; i < b->config.num_elevators; i++)
        elevator_print(&b->elevators[i]);
    printf("  Queue depth: %d\n", b->queue.count);
}

/* ── building_print_metrics ──────────────────────────────── */
void building_print_metrics(const Building *b)
{
    if (!b) return;
    printf("\n=== Simulation Metrics ===\n");
    printf("  Ticks elapsed  : %ld\n",  b->tick);
    printf("  Total requests : %ld\n",  b->total_requests);
    printf("  Total served   : %ld\n",  b->total_served);
    printf("  Scheduler      : %s\n",   b->sched->name);
    for (int i = 0; i < b->config.num_elevators; i++) {
        const Elevator *e = &b->elevators[i];
        printf("  Elevator %d     : dist=%ld  pax=%ld\n",
               i, e->total_distance, e->total_passengers);
    }
    printf("==========================\n");
}
