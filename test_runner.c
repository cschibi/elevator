/* =========================================================
 * test_runner.c  –  Self-contained unit test harness
 *
 *  No external test framework required.
 *  Build: make test
 *  Run:   ./elevator_test
 * ========================================================= */

#include "config.h"
#include "building.h"
#include "elevator.h"
#include "scheduler.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* ── Minimal test framework macros ──────────────────────── */
static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_BEGIN(name) \
    do { \
        g_tests_run++; \
        printf("  %-50s ", name); \
        fflush(stdout); \
    } while (0)

#define TEST_PASS() \
    do { \
        g_tests_passed++; \
        printf("PASS\n"); \
    } while (0)

#define TEST_FAIL(msg) \
    do { \
        g_tests_failed++; \
        printf("FAIL  (%s:%d) %s\n", __FILE__, __LINE__, (msg)); \
    } while (0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            TEST_FAIL(#a " != " #b); \
            return; \
        } \
    } while (0)

#define ASSERT_NEQ(a, b) \
    do { \
        if ((a) == (b)) { \
            TEST_FAIL(#a " == " #b " (expected different)"); \
            return; \
        } \
    } while (0)

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            TEST_FAIL(#expr " is false"); \
            return; \
        } \
    } while (0)

#define ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != NULL) { \
            TEST_FAIL(#ptr " is not NULL"); \
            return; \
        } \
    } while (0)

#define ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == NULL) { \
            TEST_FAIL(#ptr " is NULL"); \
            return; \
        } \
    } while (0)

/* ═══════════════════════════════════════════════════════════
 * Config tests
 * ═══════════════════════════════════════════════════════════ */

static void test_config_validate_valid(void)
{
    TEST_BEGIN("config: valid config passes validation");
    SimConfig cfg = (SimConfig)DEFAULT_CONFIG;
    ASSERT_EQ(config_validate(&cfg), 0);
    TEST_PASS();
}

static void test_config_validate_bad_floors(void)
{
    TEST_BEGIN("config: floors=0 fails validation");
    SimConfig cfg = (SimConfig)DEFAULT_CONFIG;
    cfg.num_floors = 0;
    ASSERT_EQ(config_validate(&cfg), -1);
    TEST_PASS();
}

static void test_config_validate_bad_elevators(void)
{
    TEST_BEGIN("config: num_elevators=0 fails validation");
    SimConfig cfg = (SimConfig)DEFAULT_CONFIG;
    cfg.num_elevators = 0;
    ASSERT_EQ(config_validate(&cfg), -1);
    TEST_PASS();
}

static void test_config_validate_null(void)
{
    TEST_BEGIN("config: NULL config fails validation");
    ASSERT_EQ(config_validate(NULL), -1);
    TEST_PASS();
}

static void test_config_validate_bad_scheduler(void)
{
    TEST_BEGIN("config: unknown scheduler id fails validation");
    SimConfig cfg = (SimConfig)DEFAULT_CONFIG;
    cfg.scheduler = (SchedulerType)99;
    ASSERT_EQ(config_validate(&cfg), -1);
    TEST_PASS();
}

/* ═══════════════════════════════════════════════════════════
 * Elevator unit tests
 * ═══════════════════════════════════════════════════════════ */

static void test_elevator_init(void)
{
    TEST_BEGIN("elevator: init sets floor=0, state=IDLE");
    Elevator e;
    elevator_init(&e, 0);
    ASSERT_EQ(e.current_floor, 0);
    ASSERT_EQ(e.state, ELEV_IDLE);
    ASSERT_EQ(e.direction, DIR_IDLE);
    ASSERT_EQ(e.door, DOOR_CLOSED);
    TEST_PASS();
}

static void test_elevator_add_stop_valid(void)
{
    TEST_BEGIN("elevator: add_stop registers floor in stop set");
    Elevator e;
    elevator_init(&e, 0);
    elevator_add_stop(&e, 3);
    ASSERT_EQ(e.stops[3], 1);
    ASSERT_TRUE(elevator_has_stops(&e));
    TEST_PASS();
}

static void test_elevator_add_stop_out_of_range(void)
{
    TEST_BEGIN("elevator: add_stop ignores out-of-range floor");
    Elevator e;
    elevator_init(&e, 0);
    elevator_add_stop(&e, MAX_FLOORS + 10);
    ASSERT_TRUE(!elevator_has_stops(&e));
    TEST_PASS();
}

static void test_elevator_clear_stop(void)
{
    TEST_BEGIN("elevator: clear_stop removes floor from stop set");
    Elevator e;
    elevator_init(&e, 0);
    elevator_add_stop(&e, 2);
    elevator_clear_stop(&e, 2);
    ASSERT_EQ(e.stops[2], 0);
    ASSERT_TRUE(!elevator_has_stops(&e));
    TEST_PASS();
}

static void test_elevator_no_stops_initially(void)
{
    TEST_BEGIN("elevator: has_stops returns false after init");
    Elevator e;
    elevator_init(&e, 0);
    ASSERT_TRUE(!elevator_has_stops(&e));
    TEST_PASS();
}

static void test_elevator_travels_up(void)
{
    TEST_BEGIN("elevator: travels from floor 0 to floor 2");
    Elevator e;
    elevator_init(&e, 0);
    elevator_add_stop(&e, 2);

    /* Tick 1: IDLE -> MOVING transition; stays at floor 0 this tick */
    elevator_tick(&e);
    ASSERT_EQ(e.state, ELEV_MOVING);
    ASSERT_EQ(e.current_floor, 0);
    ASSERT_EQ(e.direction, DIR_UP);

    /* Tick 2: moves to floor 1 */
    elevator_tick(&e);
    ASSERT_EQ(e.current_floor, 1);
    ASSERT_EQ(e.state, ELEV_MOVING);

    /* Tick 3: moves to floor 2, arrives, opens doors */
    elevator_tick(&e);
    ASSERT_EQ(e.current_floor, 2);
    ASSERT_EQ(e.state, ELEV_DOOR_OPEN);
    ASSERT_EQ(e.door, DOOR_OPEN);
    TEST_PASS();
}

static void test_elevator_door_dwell_then_idle(void)
{
    TEST_BEGIN("elevator: doors close after dwell, returns to IDLE");
    Elevator e;
    elevator_init(&e, 0);
    elevator_add_stop(&e, 1);

    /* Tick 1: IDLE -> MOVING (stays at floor 0, transition tick) */
    elevator_tick(&e);
    ASSERT_EQ(e.state, ELEV_MOVING);

    /* Tick 2: moves to floor 1, arrives, opens doors */
    elevator_tick(&e);
    ASSERT_EQ(e.state, ELEV_DOOR_OPEN);
    ASSERT_EQ(e.door, DOOR_OPEN);

    /* DOOR_DWELL_TICKS ticks to close */
    for (int i = 0; i < DOOR_DWELL_TICKS; i++)
        elevator_tick(&e);

    ASSERT_EQ(e.door, DOOR_CLOSED);
    ASSERT_EQ(e.state, ELEV_IDLE);
    TEST_PASS();
}

static void test_elevator_metrics_increment(void)
{
    TEST_BEGIN("elevator: total_distance and total_passengers incremented");
    Elevator e;
    elevator_init(&e, 0);
    elevator_add_stop(&e, 2);

    /* Run until idle */
    for (int i = 0; i < 10; i++) elevator_tick(&e);

    ASSERT_EQ(e.total_distance, 2);
    ASSERT_EQ(e.total_passengers, 1);
    TEST_PASS();
}

static void test_elevator_fault_state_no_move(void)
{
    TEST_BEGIN("elevator: faulted elevator does not move");
    Elevator e;
    elevator_init(&e, 0);
    elevator_add_stop(&e, 3);
    e.state = ELEV_FAULT;

    elevator_tick(&e);
    ASSERT_EQ(e.current_floor, 0);  /* must not move */
    ASSERT_EQ(e.state, ELEV_FAULT);
    TEST_PASS();
}

/* ═══════════════════════════════════════════════════════════
 * Scheduler tests
 * ═══════════════════════════════════════════════════════════ */

static void test_scheduler_get_fcfs(void)
{
    TEST_BEGIN("scheduler: get(SCHED_FCFS) returns non-NULL");
    ASSERT_NOT_NULL(scheduler_get(SCHED_FCFS));
    TEST_PASS();
}

static void test_scheduler_get_look(void)
{
    TEST_BEGIN("scheduler: get(SCHED_LOOK) returns non-NULL");
    ASSERT_NOT_NULL(scheduler_get(SCHED_LOOK));
    TEST_PASS();
}

static void test_scheduler_get_sstf(void)
{
    TEST_BEGIN("scheduler: get(SCHED_SSTF) returns non-NULL");
    ASSERT_NOT_NULL(scheduler_get(SCHED_SSTF));
    TEST_PASS();
}

static void test_scheduler_get_invalid(void)
{
    TEST_BEGIN("scheduler: get(invalid) returns NULL");
    ASSERT_NULL(scheduler_get((SchedulerType)99));
    TEST_PASS();
}

static void test_scheduler_fcfs_assigns_to_least_loaded(void)
{
    TEST_BEGIN("scheduler/FCFS: assigns to elevator with fewer stops");
    Elevator elevs[2];
    elevator_init(&elevs[0], 0);
    elevator_init(&elevs[1], 1);

    /* Pre-load elevator 0 with 3 stops */
    elevator_add_stop(&elevs[0], 1);
    elevator_add_stop(&elevs[0], 2);
    elevator_add_stop(&elevs[0], 3);

    Request r = { .floor = 4, .direction = DIR_UP, .tick = 0 };
    scheduler_get(SCHED_FCFS)->assign(elevs, 2, &r);

    /* Elevator 1 should have gotten the request */
    ASSERT_EQ(elevs[1].stops[4], 1);
    TEST_PASS();
}

static void test_scheduler_look_prefers_same_direction(void)
{
    TEST_BEGIN("scheduler/LOOK: prefers elevator already heading same dir");
    Elevator elevs[2];
    elevator_init(&elevs[0], 0);
    elevator_init(&elevs[1], 1);

    /* Elevator 0 heading UP at floor 1 */
    elevs[0].current_floor = 1;
    elevs[0].direction     = DIR_UP;
    elevs[0].state         = ELEV_MOVING;

    /* Elevator 1 heading DOWN at floor 3 */
    elevs[1].current_floor = 3;
    elevs[1].direction     = DIR_DOWN;
    elevs[1].state         = ELEV_MOVING;

    /* Request: floor 2 going UP */
    Request r = { .floor = 2, .direction = DIR_UP, .tick = 0 };
    scheduler_get(SCHED_LOOK)->assign(elevs, 2, &r);

    /* Elevator 0 (heading UP, closer) should win */
    ASSERT_EQ(elevs[0].stops[2], 1);
    ASSERT_EQ(elevs[1].stops[2], 0);
    TEST_PASS();
}

static void test_scheduler_sstf_picks_nearest(void)
{
    TEST_BEGIN("scheduler/SSTF: assigns to closest elevator");
    Elevator elevs[2];
    elevator_init(&elevs[0], 0);
    elevator_init(&elevs[1], 1);

    elevs[0].current_floor = 0;
    elevs[1].current_floor = 4;

    /* Request floor 3 – elevator 1 is closer */
    Request r = { .floor = 3, .direction = DIR_DOWN, .tick = 0 };
    scheduler_get(SCHED_SSTF)->assign(elevs, 2, &r);

    ASSERT_EQ(elevs[1].stops[3], 1);
    ASSERT_EQ(elevs[0].stops[3], 0);
    TEST_PASS();
}

/* ═══════════════════════════════════════════════════════════
 * Building integration tests
 * ═══════════════════════════════════════════════════════════ */

static void test_building_init_valid(void)
{
    TEST_BEGIN("building: init with valid config succeeds");
    SimConfig cfg = (SimConfig)DEFAULT_CONFIG;
    Building b;
    ASSERT_EQ(building_init(&b, &cfg), 0);
    building_destroy(&b);
    TEST_PASS();
}

static void test_building_init_null(void)
{
    TEST_BEGIN("building: init with NULL config fails");
    Building b;
    ASSERT_EQ(building_init(&b, NULL), -1);
    TEST_PASS();
}

static void test_building_request_valid(void)
{
    TEST_BEGIN("building: valid request accepted, increments total_requests");
    SimConfig cfg = (SimConfig)DEFAULT_CONFIG;
    Building b;
    building_init(&b, &cfg);
    ASSERT_EQ(building_request(&b, 2, DIR_UP), 0);
    ASSERT_EQ(b.total_requests, 1);
    building_destroy(&b);
    TEST_PASS();
}

static void test_building_request_out_of_range(void)
{
    TEST_BEGIN("building: request floor out of range rejected");
    SimConfig cfg = (SimConfig)DEFAULT_CONFIG;
    Building b;
    building_init(&b, &cfg);
    ASSERT_EQ(building_request(&b, MAX_FLOORS + 5, DIR_UP), -1);
    ASSERT_EQ(b.total_requests, 0);
    building_destroy(&b);
    TEST_PASS();
}

static void test_building_request_idle_direction(void)
{
    TEST_BEGIN("building: request with DIR_IDLE rejected");
    SimConfig cfg = (SimConfig)DEFAULT_CONFIG;
    Building b;
    building_init(&b, &cfg);
    ASSERT_EQ(building_request(&b, 1, DIR_IDLE), -1);
    building_destroy(&b);
    TEST_PASS();
}

static void test_building_tick_dispatches_and_serves(void)
{
    TEST_BEGIN("building: tick dispatches request and elevator serves it");
    SimConfig cfg = (SimConfig)DEFAULT_CONFIG;
    Building b;
    building_init(&b, &cfg);
    building_request(&b, 2, DIR_UP);

    /* Run enough ticks to serve a floor-2 request from floor 0 */
    for (int i = 0; i < 15; i++) building_tick(&b);

    ASSERT_TRUE(b.total_served > 0);
    building_destroy(&b);
    TEST_PASS();
}

static void test_building_set_scheduler_runtime(void)
{
    TEST_BEGIN("building: scheduler can be swapped at runtime");
    SimConfig cfg = (SimConfig)DEFAULT_CONFIG;
    Building b;
    building_init(&b, &cfg);

    ASSERT_EQ(building_set_scheduler(&b, SCHED_FCFS), 0);
    ASSERT_EQ(b.config.scheduler, SCHED_FCFS);

    ASSERT_EQ(building_set_scheduler(&b, SCHED_SSTF), 0);
    ASSERT_EQ(b.config.scheduler, SCHED_SSTF);

    ASSERT_EQ(building_set_scheduler(&b, (SchedulerType)99), -1);

    building_destroy(&b);
    TEST_PASS();
}

static void test_building_queue_full(void)
{
    TEST_BEGIN("building: overfilling request queue is rejected gracefully");
    SimConfig cfg = (SimConfig)DEFAULT_CONFIG;
    Building b;
    building_init(&b, &cfg);

    /* Fill the queue without ticking (no dispatch) */
    for (int i = 0; i < MAX_REQUESTS; i++)
        building_request(&b, 1, DIR_UP);

    /* Next request must be rejected */
    int rc = building_request(&b, 1, DIR_UP);
    ASSERT_EQ(rc, -1);

    building_destroy(&b);
    TEST_PASS();
}

/* ═══════════════════════════════════════════════════════════
 * Multi-request integration scenario
 * ═══════════════════════════════════════════════════════════ */

static void test_integration_multi_request(void)
{
    TEST_BEGIN("integration: multiple requests served within 30 ticks");
    SimConfig cfg = (SimConfig)DEFAULT_CONFIG;
    cfg.scheduler = SCHED_LOOK;
    Building b;
    building_init(&b, &cfg);

    building_request(&b, 1, DIR_UP);
    building_request(&b, 3, DIR_DOWN);
    building_request(&b, 4, DIR_UP);

    for (int i = 0; i < 30; i++) building_tick(&b);

    ASSERT_TRUE(b.total_served >= 3);
    building_destroy(&b);
    TEST_PASS();
}

/* ═══════════════════════════════════════════════════════════
 * Idle-repositioning tests
 * ═══════════════════════════════════════════════════════════ */

static void test_reposition_idle_ticks_increment(void)
{
    TEST_BEGIN("reposition: idle_ticks increments each tick with no stops");
    Elevator e;
    elevator_init(&e, 0);
    elevator_tick(&e);
    elevator_tick(&e);
    elevator_tick(&e);
    ASSERT_EQ(e.idle_ticks, 3);
    ASSERT_EQ(e.state, ELEV_IDLE);
    TEST_PASS();
}

static void test_reposition_idle_ticks_reset_on_stop(void)
{
    TEST_BEGIN("reposition: idle_ticks resets to 0 when a stop is assigned");
    Elevator e;
    elevator_init(&e, 0);
    /* Accumulate 5 idle ticks */
    for (int i = 0; i < 5; i++) elevator_tick(&e);
    ASSERT_EQ(e.idle_ticks, 5);
    /* Assign a stop and tick once to trigger IDLE->MOVING transition */
    elevator_add_stop(&e, 3);
    elevator_tick(&e);
    ASSERT_EQ(e.idle_ticks, 0);
    ASSERT_EQ(e.state, ELEV_MOVING);
    TEST_PASS();
}

static void test_reposition_start_sets_state(void)
{
    TEST_BEGIN("reposition: elevator_start_reposition sets REPOSITIONING state");
    Elevator e;
    elevator_init(&e, 0);
    elevator_start_reposition(&e, 5);
    ASSERT_EQ(e.state, ELEV_REPOSITIONING);
    ASSERT_EQ(e.target_floor, 5);
    ASSERT_EQ(e.direction, DIR_UP);
    ASSERT_EQ(e.idle_ticks, 0);
    TEST_PASS();
}

static void test_reposition_moves_toward_target(void)
{
    TEST_BEGIN("reposition: REPOSITIONING advances one floor per tick");
    Elevator e;
    elevator_init(&e, 0);
    elevator_start_reposition(&e, 3);
    elevator_tick(&e);
    ASSERT_EQ(e.current_floor, 1);
    ASSERT_EQ(e.state, ELEV_REPOSITIONING);
    ASSERT_EQ(e.total_distance, 1);
    TEST_PASS();
}

static void test_reposition_completes_to_idle(void)
{
    TEST_BEGIN("reposition: elevator returns to ELEV_IDLE on arrival at target");
    Elevator e;
    elevator_init(&e, 0);
    e.current_floor = 2;
    elevator_start_reposition(&e, 3);
    elevator_tick(&e);   /* moves from 2 to 3, arrives */
    ASSERT_EQ(e.current_floor, 3);
    ASSERT_EQ(e.state, ELEV_IDLE);
    ASSERT_EQ(e.direction, DIR_IDLE);
    ASSERT_EQ(e.idle_ticks, 0);
    TEST_PASS();
}

static void test_reposition_abandoned_when_stop_assigned(void)
{
    TEST_BEGIN("reposition: real stop preempts repositioning");
    Elevator e;
    elevator_init(&e, 0);
    elevator_start_reposition(&e, 8);   /* heading far up */
    elevator_tick(&e);                  /* now at floor 1 */
    ASSERT_EQ(e.current_floor, 1);
    elevator_add_stop(&e, 2);           /* real stop assigned */
    elevator_tick(&e);                  /* abandon: switch to MOVING toward 2 */
    ASSERT_EQ(e.state, ELEV_MOVING);
    ASSERT_EQ(e.target_floor, 2);
    ASSERT_EQ(e.idle_ticks, 0);
    TEST_PASS();
}

static void test_reposition_triggered_by_building_tick(void)
{
    TEST_BEGIN("building: idle elevator repositions after IDLE_REPOSITION_TICKS ticks");
    SimConfig cfg = (SimConfig)DEFAULT_CONFIG;
    cfg.num_floors    = 10;
    cfg.num_elevators = 1;
    Building b;
    building_init(&b, &cfg);

    /* Run exactly IDLE_REPOSITION_TICKS ticks with no requests */
    for (int i = 0; i < IDLE_REPOSITION_TICKS; i++)
        building_tick(&b);

    /* Elevator 0 should now be repositioning.
     * zone = 10/1 = 10, target = 0*10 + 10/2 = 5 */
    ASSERT_EQ(b.elevators[0].state, ELEV_REPOSITIONING);
    ASSERT_EQ(b.elevators[0].target_floor, 5);

    building_destroy(&b);
    TEST_PASS();
}

/* ═══════════════════════════════════════════════════════════
 * Test registry & runner
 * ═══════════════════════════════════════════════════════════ */

typedef void (*TestFn)(void);

static const TestFn all_tests[] = {
    /* Config */
    test_config_validate_valid,
    test_config_validate_bad_floors,
    test_config_validate_bad_elevators,
    test_config_validate_null,
    test_config_validate_bad_scheduler,
    /* Elevator */
    test_elevator_init,
    test_elevator_add_stop_valid,
    test_elevator_add_stop_out_of_range,
    test_elevator_clear_stop,
    test_elevator_no_stops_initially,
    test_elevator_travels_up,
    test_elevator_door_dwell_then_idle,
    test_elevator_metrics_increment,
    test_elevator_fault_state_no_move,
    /* Scheduler */
    test_scheduler_get_fcfs,
    test_scheduler_get_look,
    test_scheduler_get_sstf,
    test_scheduler_get_invalid,
    test_scheduler_fcfs_assigns_to_least_loaded,
    test_scheduler_look_prefers_same_direction,
    test_scheduler_sstf_picks_nearest,
    /* Building */
    test_building_init_valid,
    test_building_init_null,
    test_building_request_valid,
    test_building_request_out_of_range,
    test_building_request_idle_direction,
    test_building_tick_dispatches_and_serves,
    test_building_set_scheduler_runtime,
    test_building_queue_full,
    /* Integration */
    test_integration_multi_request,
    /* Idle repositioning */
    test_reposition_idle_ticks_increment,
    test_reposition_idle_ticks_reset_on_stop,
    test_reposition_start_sets_state,
    test_reposition_moves_toward_target,
    test_reposition_completes_to_idle,
    test_reposition_abandoned_when_stop_assigned,
    test_reposition_triggered_by_building_tick,
};

int main(void)
{
    /* Suppress log noise during tests */
    logger_init(LOG_ERROR, NULL);

    printf("\n════════════════════════════════════════════════════\n");
    printf("  Elevator Simulator – Unit Test Suite\n");
    printf("════════════════════════════════════════════════════\n\n");

    size_t n = sizeof(all_tests) / sizeof(all_tests[0]);
    for (size_t i = 0; i < n; i++)
        all_tests[i]();

    printf("\n────────────────────────────────────────────────────\n");
    printf("  Results: %d/%d passed", g_tests_passed, g_tests_run);
    if (g_tests_failed > 0)
        printf("  (%d FAILED)\n", g_tests_failed);
    else
        printf("  ✓ All tests passed\n");
    printf("════════════════════════════════════════════════════\n\n");

    logger_close();
    return g_tests_failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
