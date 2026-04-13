# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

---

## Commands

```bash
make              # build ./elevator_sim (default)
make test         # build & run all 37 unit tests → ./elevator_test
make all          # build simulator AND run tests
make clean        # remove objects, binaries, elevator.log

./elevator_sim                              # run with defaults (LOOK scheduler)
./elevator_sim --scheduler fcfs             # choose scheduler: fcfs | look | sstf
./elevator_sim --log elevator.log --debug   # verbose logging to file
./elevator_sim --tick-ms 200               # auto-paced ticks
```

There is no way to run a single test — the harness always runs all 37 tests.

---

## UI Server

A Node.js WebSocket server lives in `elevator_ui/`:

```bash
cd elevator_ui && node server.js   # starts on http://localhost:8080
```

The server spawns `../elevator_sim` as a child process, relays commands to its stdin, parses its stdout, and broadcasts JSON state updates to all connected browser clients over WebSocket.

**Key files:**
- `elevator_ui/server.js` — HTTP + WebSocket server, sim process management, state parser
- `elevator_ui/elevator_ui.html` — single-page frontend (SVG building, controls, log feed)

**WebSocket message types (client → server):**

| Type | Payload | Effect |
|------|---------|--------|
| `step` | — | Step sim one tick |
| `auto` | `{ n }` | Step sim N ticks |
| `request` | `{ floor, direction }` | Submit hall call |
| `scheduler` | `{ name }` | Swap scheduler |
| `startRandom` | — | Start request randomization |
| `stopRandom` | — | Stop request randomization |

### Request Randomization

Activated via the **▶ Start Randomization** button in the UI. Every 3 seconds the server:
1. Checks queue depth — stops automatically if `queueDepth >= 100`
2. Picks a random floor (0 .. `numFloors-1`) with constrained direction: floor 0 → always UP, top floor → always DOWN, all others 50/50
3. Enqueues `r <floor> <dir>`, `s` (tick), `p` (print) so the UI updates in real time

The **■ Stop Randomization** button (or the auto-stop at queue depth 100) cancels the interval and resets the button state. `MAX_REQUESTS` in `config.h` is set to 128 to accommodate the 100-item threshold.

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
    ▲    ╲                         ▲                                  │
    │     (idle≥IDLE_REPOSITION    └──────────(dwell expires)────────┘
    │      _TICKS, no stops)
    │          ▼
    └──── ELEV_REPOSITIONING ──(stop assigned)──► ELEV_MOVING
               │
               └──(arrived at zone centre)──► ELEV_IDLE
ELEV_FAULT  ←─── set e->state = ELEV_FAULT to take elevator out of service
```

Pending stops are tracked as a bitmask (`e->stops[floor]`). Direction is inferred from the lowest/highest set bit relative to current floor.

Each elevator tracks `idle_ticks` (increments every tick in `ELEV_IDLE` with no stops). After `IDLE_REPOSITION_TICKS` (default 10), `building_tick()` calls `elevator_start_reposition()` to move the elevator to its even-zone target floor. Repositioning is abandoned (state → `ELEV_MOVING`) the moment any real stop is assigned.

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
