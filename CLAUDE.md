# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Commands

```bash
make              # build ./elevator_sim (default)
make test         # build & run all 30 unit tests → ./elevator_test
make all          # build simulator AND run tests
make clean        # remove objects, binaries, elevator.log

./elevator_sim                              # run with defaults (LOOK scheduler)
./elevator_sim --scheduler fcfs             # choose scheduler: fcfs | look | sstf
./elevator_sim --log elevator.log --debug   # verbose logging to file
./elevator_sim --tick-ms 200               # auto-paced ticks
```

There is no way to run a single test — the harness always runs all 30 tests.

---

## Architecture

A discrete-tick elevator simulator in **C99**. No external dependencies.

### Simulation loop

Each call to `building_tick()` (`building.c`):
1. Drains the `RequestQueue` (circular buffer), calling `sched->assign()` for each request
2. Calls `sched->tick_hook()` — optional per-tick rebalancing (may be NULL)
3. Calls `elevator_tick()` on every elevator

One tick = one floor of travel. Door dwell = `DOOR_DWELL_TICKS` (default: 2 ticks).

### Elevator state machine

```
ELEV_IDLE ──(stop added)──► ELEV_MOVING ──(arrive at stop)──► ELEV_DOOR_OPEN
    ▲                              ▲                                  │
    └──(no more stops)─────────────┴──────────(dwell expires)────────┘
ELEV_FAULT  ←─── set e->state = ELEV_FAULT to take elevator out of service
```

Pending stops are tracked as a bitmask (`e->stops[floor]`). Direction is inferred from the lowest/highest set bit relative to current floor.

### Scheduler strategy pattern

Each algorithm implements `SchedulerOps` (`scheduler.h`):

```c
typedef struct {
    const char *name;
    void (*assign)(Elevator *elevators, int n, const Request *req);
    void (*tick_hook)(Elevator *elevators, int n);  // may be NULL
    const char *(*get_name)(void);
} SchedulerOps;
```

All algorithms share `dispatch_cost()` in `scheduler.c`: base cost = floor distance, with a `+MAX_FLOORS` penalty when the elevator is traveling in the opposite direction. FCFS ignores cost entirely and picks the least-loaded elevator. Algorithms are registered in `registry[]` keyed by `SchedulerType` enum (`config.h`).

---

## How to Add a New Scheduling Algorithm

1. Add a constant to `SchedulerType` enum in `config.h` (before `SCHED_COUNT`).
2. Implement `assign`, `tick_hook` (or NULL), and `get_name` in `scheduler.c`; register in `registry[]`.
3. Add CLI parsing in `parse_scheduler()` in `main.c`.
4. Write tests in `test_runner.c` using the `TEST_BEGIN` / `ASSERT_EQ` / `TEST_PASS` pattern.

---

## Scaling Floors / Elevators

Edit compile-time constants in `config.h`:
```c
#define MAX_FLOORS     10   // was 5
#define MAX_ELEVATORS   4   // was 2
```
All runtime loops use `cfg.num_floors` / `cfg.num_elevators` bounded by these maximums — no other files need changes.

---

## Coding Conventions

| Aspect | Convention |
|---|---|
| Standard | C99 (`-std=c99`) |
| Naming | `snake_case` for all identifiers |
| Logging | `log_debug/info/warn/error()` macros only — never `printf` in library code |
| Error returns | `0` = success, `-1` = error (POSIX style) |
| NULL safety | Every public function guards against NULL args |
