#ifndef ELEVATOR_H
#define ELEVATOR_H

/* =========================================================
 * elevator.h  –  Single elevator state machine
 * ========================================================= */

#include "config.h"
#include <stdint.h>

/* ── Direction ───────────────────────────────────────────── */
typedef enum {
    DIR_IDLE = 0,
    DIR_UP,
    DIR_DOWN
} Direction;

/* ── Door state ──────────────────────────────────────────── */
typedef enum {
    DOOR_CLOSED = 0,
    DOOR_OPEN,
    DOOR_OPENING,
    DOOR_CLOSING
} DoorState;

/* ── Elevator operational state ──────────────────────────── */
typedef enum {
    ELEV_IDLE          = 0,  /* parked, no pending requests      */
    ELEV_MOVING,             /* travelling between floors         */
    ELEV_REPOSITIONING,      /* moving to optimal idle position   */
    ELEV_DOOR_OPEN,          /* stopped, doors open (dwell)       */
    ELEV_FAULT               /* out-of-service                    */
} ElevatorState;

/* ── Per-elevator data ───────────────────────────────────── */
typedef struct {
    int            id;                       /* 0-based index         */
    int            current_floor;            /* 0-based               */
    int            target_floor;             /* -1 = none             */
    Direction      direction;
    DoorState      door;
    ElevatorState  state;
    int            door_dwell;               /* ticks remaining (dwell)           */
    int            idle_ticks;              /* ticks spent IDLE with no stops    */

    /* Stop set: floors this elevator is committed to visit */
    uint8_t        stops[MAX_FLOORS];        /* 1 = stop requested               */

    /* Metrics */
    long           total_distance;           /* floors travelled                  */
    long           total_passengers;         /* passengers served                 */
} Elevator;

/* ── Public API ──────────────────────────────────────────── */

/** Initialise an elevator at floor 0. */
void elevator_init(Elevator *e, int id);

/** Add floor f to this elevator's stop set. */
void elevator_add_stop(Elevator *e, int floor);

/** Remove floor f from stop set (called on arrival). */
void elevator_clear_stop(Elevator *e, int floor);

/** Returns 1 if elevator has any pending stops. */
int  elevator_has_stops(const Elevator *e);

/**
 * Advance the elevator by one simulation tick.
 * Returns 1 if the elevator opened its doors this tick (passenger event).
 */
int  elevator_tick(Elevator *e);

/**
 * Begin repositioning to target_floor.
 * Caller must only invoke when state == ELEV_IDLE and no pending stops.
 */
void elevator_start_reposition(Elevator *e, int target_floor);

/** Human-readable state dump (for CLI / debug). */
void elevator_print(const Elevator *e);

/** Return string name of direction. */
const char *dir_name(Direction d);

/** Return string name of elevator state. */
const char *elev_state_name(ElevatorState s);

#endif /* ELEVATOR_H */
