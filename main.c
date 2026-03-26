/* =========================================================
 * main.c  –  Interactive CLI for the elevator simulator
 *
 *  Usage: ./elevator_sim [--scheduler <fcfs|look|sstf>]
 *                        [--log <path>]
 *                        [--tick-ms <ms>]
 *                        [--debug]
 *
 *  Runtime commands:
 *    r <floor> <u|d>   – request (hall call) at floor, direction
 *    s                 – step one tick
 *    a <N>             – auto-step N ticks
 *    p                 – print building status
 *    m                 – print metrics
 *    S <fcfs|look|sstf>– swap scheduler at runtime
 *    q                 – quit
 *    ?                 – help
 * ========================================================= */

#include "config.h"
#include "building.h"
#include "logger.h"
#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Forward declarations ────────────────────────────────── */
static void print_help(void);
static int  parse_direction(const char *s, Direction *out);
static int  parse_scheduler(const char *s, SchedulerType *out);
static void run_cli(Building *b);

/* ── main ────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    SimConfig cfg = (SimConfig)DEFAULT_CONFIG;
    LogLevel  log_level = LOG_INFO;

    /* ── Argument parsing ──────────────────────────────── */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            log_level = LOG_DEBUG;
        } else if (strcmp(argv[i], "--scheduler") == 0 && i + 1 < argc) {
            SchedulerType st;
            if (parse_scheduler(argv[++i], &st) == 0)
                cfg.scheduler = st;
            else {
                fprintf(stderr, "Unknown scheduler '%s'\n", argv[i]);
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
            cfg.log_to_file = 1;
            strncpy(cfg.log_path, argv[++i], sizeof(cfg.log_path) - 1);
        } else if (strcmp(argv[i], "--tick-ms") == 0 && i + 1 < argc) {
            cfg.tick_ms = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return EXIT_SUCCESS;
        } else {
            fprintf(stderr, "Unknown argument '%s'. Use --help.\n", argv[i]);
            return EXIT_FAILURE;
        }
    }

    /* ── Logger init ───────────────────────────────────── */
    logger_init(log_level, cfg.log_to_file ? cfg.log_path : NULL);

    /* ── Config display & validation ───────────────────── */
    config_print(&cfg);
    if (config_validate(&cfg) != 0) {
        log_error("Invalid configuration – aborting");
        logger_close();
        return EXIT_FAILURE;
    }

    /* ── Building init ─────────────────────────────────── */
    Building b;
    if (building_init(&b, &cfg) != 0) {
        log_error("Failed to initialise building");
        logger_close();
        return EXIT_FAILURE;
    }

    /* ── Interactive loop ──────────────────────────────── */
    printf("\nElevator Simulator ready. Type '?' for help.\n\n");
    run_cli(&b);

    /* ── Teardown ───────────────────────────────────────── */
    building_print_metrics(&b);
    building_destroy(&b);
    logger_close();
    return EXIT_SUCCESS;
}

/* ── CLI loop ────────────────────────────────────────────── */
static void run_cli(Building *b)
{
    char line[128];
    while (1) {
        printf("[T%04ld]> ", b->tick);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;  /* EOF */

        /* strip newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[--len] = '\0';
        if (len == 0) continue;

        char cmd = line[0];

        /* ── r <floor> <u|d> ── request ── */
        if (cmd == 'r') {
            int floor;
            char dir_ch[4] = {0};
            if (sscanf(line + 1, "%d %3s", &floor, dir_ch) != 2) {
                printf("Usage: r <floor> <u|d>\n");
                continue;
            }
            Direction dir;
            if (parse_direction(dir_ch, &dir) != 0) {
                printf("Direction must be 'u' (up) or 'd' (down)\n");
                continue;
            }
            if (building_request(b, floor, dir) == 0)
                printf("Request submitted: floor %d %s\n",
                       floor, dir_name(dir));
            else
                printf("Request rejected (check floor range / queue full)\n");

        /* ── s ── step one tick ── */
        } else if (cmd == 's') {
            building_tick(b);
            building_print_status(b);

        /* ── a <N> ── auto-step N ticks ── */
        } else if (cmd == 'a') {
            int n = 1;
            sscanf(line + 1, "%d", &n);
            if (n <= 0) { printf("N must be > 0\n"); continue; }
            for (int i = 0; i < n; i++) {
                building_tick(b);
            }
            building_print_status(b);

        /* ── p ── print status ── */
        } else if (cmd == 'p') {
            building_print_status(b);

        /* ── m ── print metrics ── */
        } else if (cmd == 'm') {
            building_print_metrics(b);

        /* ── S <name> ── swap scheduler ── */
        } else if (cmd == 'S') {
            char name[16] = {0};
            if (sscanf(line + 1, "%15s", name) != 1) {
                printf("Usage: S <fcfs|look|sstf>\n");
                continue;
            }
            SchedulerType st;
            if (parse_scheduler(name, &st) == 0) {
                building_set_scheduler(b, st);
                printf("Scheduler changed to %s\n", name);
            } else {
                printf("Unknown scheduler '%s'\n", name);
                scheduler_list();
            }

        /* ── q ── quit ── */
        } else if (cmd == 'q') {
            printf("Exiting simulator.\n");
            break;

        /* ── ? ── help ── */
        } else if (cmd == '?') {
            print_help();

        } else {
            printf("Unknown command '%c'. Type '?' for help.\n", cmd);
        }
    }
}

/* ── Parsers ─────────────────────────────────────────────── */

/** Portable case-insensitive string compare (avoids POSIX strcasecmp). */
static int str_iequal(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (*a + 32) : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (*b + 32) : *b;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static int parse_direction(const char *s, Direction *out)
{
    if (!s || !out) return -1;
    if (s[0] == 'u' || s[0] == 'U') { *out = DIR_UP;   return 0; }
    if (s[0] == 'd' || s[0] == 'D') { *out = DIR_DOWN; return 0; }
    return -1;
}

static int parse_scheduler(const char *s, SchedulerType *out)
{
    if (!s || !out) return -1;
    if (str_iequal(s, "fcfs")) { *out = SCHED_FCFS; return 0; }
    if (str_iequal(s, "look")) { *out = SCHED_LOOK; return 0; }
    if (str_iequal(s, "sstf")) { *out = SCHED_SSTF; return 0; }
    return -1;
}

/* ── Help text ───────────────────────────────────────────── */
static void print_help(void)
{
    printf(
        "\n─── Elevator Simulator Commands ───────────────────\n"
        "  r <floor> <u|d>    Hall call: floor 0-%d, u=up d=down\n"
        "  s                  Step one simulation tick\n"
        "  a <N>              Auto-step N ticks\n"
        "  p                  Print elevator status\n"
        "  m                  Print simulation metrics\n"
        "  S <fcfs|look|sstf> Swap scheduling algorithm\n"
        "  q                  Quit\n"
        "  ?                  This help\n"
        "────────────────────────────────────────────────────\n\n",
        MAX_FLOORS - 1
    );
}
