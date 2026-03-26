/* =========================================================
 * elevator.c  –  Single elevator state machine
 * ========================================================= */

#include "elevator.h"
#include "logger.h"
#include <string.h>
#include <stdio.h>

/* ── String helpers ──────────────────────────────────────── */
const char *dir_name(Direction d)
{
    switch (d) {
        case DIR_IDLE: return "IDLE";
        case DIR_UP:   return "UP";
        case DIR_DOWN: return "DOWN";
        default:       return "???";
    }
}

const char *elev_state_name(ElevatorState s)
{
    switch (s) {
        case ELEV_IDLE:      return "IDLE";
        case ELEV_MOVING:    return "MOVING";
        case ELEV_DOOR_OPEN: return "DOOR_OPEN";
        case ELEV_FAULT:     return "FAULT";
        default:             return "???";
    }
}

/* ── elevator_init ───────────────────────────────────────── */
void elevator_init(Elevator *e, int id)
{
    if (!e) return;
    memset(e, 0, sizeof(*e));
    e->id            = id;
    e->current_floor = 0;
    e->target_floor  = -1;
    e->direction     = DIR_IDLE;
    e->door          = DOOR_CLOSED;
    e->state         = ELEV_IDLE;
    log_info("Elevator %d initialised at floor 0", id);
}

/* ── elevator_add_stop ───────────────────────────────────── */
void elevator_add_stop(Elevator *e, int floor)
{
    if (!e) return;
    if (floor < 0 || floor >= MAX_FLOORS) {
        log_warn("Elevator %d: add_stop floor %d out of range", e->id, floor);
        return;
    }
    if (!e->stops[floor]) {
        e->stops[floor] = 1;
        log_debug("Elevator %d: stop added at floor %d", e->id, floor);
    }
}

/* ── elevator_clear_stop ─────────────────────────────────── */
void elevator_clear_stop(Elevator *e, int floor)
{
    if (!e) return;
    if (floor >= 0 && floor < MAX_FLOORS)
        e->stops[floor] = 0;
}

/* ── elevator_has_stops ──────────────────────────────────── */
int elevator_has_stops(const Elevator *e)
{
    if (!e) return 0;
    for (int i = 0; i < MAX_FLOORS; i++)
        if (e->stops[i]) return 1;
    return 0;
}

/* ── Internal: choose next target based on direction ──────── */
static int next_stop_in_direction(const Elevator *e)
{
    if (e->direction == DIR_UP) {
        for (int f = e->current_floor + 1; f < MAX_FLOORS; f++)
            if (e->stops[f]) return f;
        /* no stops above – reverse */
        for (int f = e->current_floor - 1; f >= 0; f--)
            if (e->stops[f]) return f;
    } else if (e->direction == DIR_DOWN) {
        for (int f = e->current_floor - 1; f >= 0; f--)
            if (e->stops[f]) return f;
        /* no stops below – reverse */
        for (int f = e->current_floor + 1; f < MAX_FLOORS; f++)
            if (e->stops[f]) return f;
    } else {
        /* IDLE – pick nearest */
        int best = -1, best_dist = MAX_FLOORS + 1;
        for (int f = 0; f < MAX_FLOORS; f++) {
            if (e->stops[f]) {
                int dist = f - e->current_floor;
                if (dist < 0) dist = -dist;
                if (dist < best_dist) { best_dist = dist; best = f; }
            }
        }
        return best;
    }
    return -1;
}

/* ── elevator_tick ───────────────────────────────────────── */
int elevator_tick(Elevator *e)
{
    if (!e) return 0;
    if (e->state == ELEV_FAULT) return 0;

    int passenger_event = 0;

    switch (e->state) {

    /* ── Doors open: dwell, then close ──────────────────── */
    case ELEV_DOOR_OPEN:
        e->door_dwell--;
        if (e->door_dwell <= 0) {
            e->door  = DOOR_CLOSED;
            e->state = elevator_has_stops(e) ? ELEV_MOVING : ELEV_IDLE;
            log_info("Elevator %d: doors closed at floor %d",
                     e->id, e->current_floor);
            if (e->state == ELEV_MOVING) {
                e->target_floor = next_stop_in_direction(e);
                if (e->target_floor > e->current_floor)
                    e->direction = DIR_UP;
                else if (e->target_floor < e->current_floor)
                    e->direction = DIR_DOWN;
            } else {
                e->direction = DIR_IDLE;
            }
        }
        break;

    /* ── Idle: check if a stop was added ────────────────── */
    case ELEV_IDLE:
        if (elevator_has_stops(e)) {
            e->target_floor = next_stop_in_direction(e);
            if (e->target_floor == e->current_floor) {
                /* already here */
                goto arrive;
            }
            e->direction = (e->target_floor > e->current_floor)
                           ? DIR_UP : DIR_DOWN;
            e->state     = ELEV_MOVING;
            log_info("Elevator %d: departing floor %d -> %d (%s)",
                     e->id, e->current_floor, e->target_floor,
                     dir_name(e->direction));
        }
        break;

    /* ── Moving: advance one floor per tick ─────────────── */
    case ELEV_MOVING:
        if (e->target_floor < 0) {
            e->state     = ELEV_IDLE;
            e->direction = DIR_IDLE;
            break;
        }

        /* Move one floor */
        if (e->direction == DIR_UP)
            e->current_floor++;
        else
            e->current_floor--;

        e->total_distance++;

        log_info("Elevator %d: at floor %d (heading %s, target %d)",
                 e->id, e->current_floor,
                 dir_name(e->direction), e->target_floor);

    arrive:
        /* Did we reach a stop? */
        if (e->stops[e->current_floor]) {
            elevator_clear_stop(e, e->current_floor);
            e->door        = DOOR_OPEN;
            e->door_dwell  = DOOR_DWELL_TICKS;
            e->state       = ELEV_DOOR_OPEN;
            e->total_passengers++;
            passenger_event = 1;
            log_info("Elevator %d: arrived at floor %d – doors open",
                     e->id, e->current_floor);

            /* Recalculate target after clearing this stop */
            e->target_floor = next_stop_in_direction(e);
        } else if (e->current_floor == e->target_floor) {
            /* Reached target but stop already cleared (edge case) */
            e->target_floor = next_stop_in_direction(e);
            if (e->target_floor < 0) {
                e->state     = ELEV_IDLE;
                e->direction = DIR_IDLE;
            }
        }
        break;

    default:
        break;
    }

    return passenger_event;
}

/* ── elevator_print ──────────────────────────────────────── */
void elevator_print(const Elevator *e)
{
    if (!e) return;
    printf("  [E%d] Floor:%d Dir:%-5s State:%-10s Door:%-8s Stops:[",
           e->id,
           e->current_floor,
           dir_name(e->direction),
           elev_state_name(e->state),
           e->door == DOOR_OPEN ? "OPEN" : "CLOSED");
    for (int i = 0; i < MAX_FLOORS; i++)
        if (e->stops[i]) printf("%d ", i);
    printf("] dist:%ld pax:%ld\n",
           e->total_distance, e->total_passengers);
}
