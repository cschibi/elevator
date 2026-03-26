================================================================================
  ELEVATOR SIMULATOR  —  README
  A discrete-tick, multi-elevator simulator in C99
================================================================================

Latest Session ID: 3-24-26

claude --resume aa7ec8f8-ef95-491a-bb5a-5c6a64f8e3ef

To run in Debug Mode:  
 ./elevator_sim --log elevator.log --debug 


REQUIREMENTS
------------
  - GCC (or Clang) with C99 support
  - GNU Make
  - POSIX-compatible OS (Linux, macOS, WSL on Windows)

No external libraries required.


--------------------------------------------------------------------------------
QUICK START
--------------------------------------------------------------------------------

  1. Unzip the archive and enter the project directory:

       unzip elevator_sim.zip
       cd elevator_sim

  2. Build everything (simulator + tests):

       make all

  3. Run the tests:

       make test

  4. Launch the interactive simulator:

       ./elevator_sim
     	 ./elevator_sim --log elevator.log --debug 

--------------------------------------------------------------------------------
MAKEFILE TARGETS
--------------------------------------------------------------------------------

  make                Build the simulator binary only  →  ./elevator_sim
  make sim            Same as above (explicit target)
  make test           Build the test binary and run all 30 unit tests
  make check          Alias for: make test
  make all            Build simulator AND run tests
  make clean          Remove all compiled objects, binaries, and elevator.log

  Examples:
    make all                  # full build + test run
    make clean && make all    # clean rebuild


--------------------------------------------------------------------------------
SIMULATOR COMMAND-LINE ARGUMENTS
--------------------------------------------------------------------------------

  ./elevator_sim [OPTIONS]

  OPTIONS:

    --scheduler <name>
        Select the dispatch algorithm at startup.
        Valid names (case-insensitive):
          fcfs   First Come First Served — assigns to least-loaded elevator
          look   LOOK algorithm (default) — prefers elevators already
                 travelling toward the requested floor
          sstf   Shortest Seek Time First — assigns to nearest elevator

        Example:
          ./elevator_sim --scheduler fcfs

    --log <filepath>
        Tee all log output to the specified file in addition to stdout.
        The file is opened in append mode; it is created if it does not exist.

        Example:
          ./elevator_sim --log sim_run.log

    --tick-ms <milliseconds>
        Pause for <ms> milliseconds between each simulation tick.
        Use 0 (default) for instant/manual stepping.
        Useful with the 'a' (auto-step) command for visual pacing.

        Example:
          ./elevator_sim --tick-ms 200

    --debug
        Set log level to DEBUG (very verbose).
        Default level is INFO.

        Example:
          ./elevator_sim --debug

    --help  /  -h
        Print usage summary and exit.

  Combined example:
    ./elevator_sim --scheduler sstf --log run.log --tick-ms 100 --debug


--------------------------------------------------------------------------------
INTERACTIVE CLI COMMANDS
--------------------------------------------------------------------------------

  Once the simulator is running, type commands at the [TXXXX]> prompt.

  COMMAND           DESCRIPTION
  ─────────────     ─────────────────────────────────────────────────────────
  r <floor> <dir>   Submit a hall call (floor button press).
                      <floor>  Floor number: 0 (ground) to 4 (top)
                      <dir>    u = going Up  |  d = going Down
                    Examples:
                      r 3 u    → request at floor 3, heading up
                      r 0 u    → request at ground floor, heading up
                      r 4 d    → request at top floor, heading down

  s                 Step the simulation forward by ONE tick.
                    Prints the building status after the step.

  a <N>             Auto-step forward by N ticks.
                    Prints the building status after all steps.
                    Example:
                      a 10     → advance 10 ticks

  p                 Print current elevator status (floor, direction,
                    state, door, pending stops, distance, passengers).

  m                 Print simulation metrics summary:
                      - Total ticks elapsed
                      - Total requests submitted
                      - Total passengers served
                      - Per-elevator distance and passenger counts

  S <name>          Swap the scheduling algorithm at runtime (hot-swap).
                    The new algorithm applies to all subsequent requests.
                    Valid names: fcfs | look | sstf
                    Example:
                      S sstf   → switch to Shortest Seek Time First

  q                 Quit the simulator (prints final metrics first).

  ?                 Display the command help summary.


--------------------------------------------------------------------------------
EXAMPLE SESSION
--------------------------------------------------------------------------------

  $ ./elevator_sim --scheduler look

  === Simulator Configuration ===
    Floors      : 5
    Elevators   : 2
    Scheduler   : LOOK
    Tick (ms)   : 0
    Log to file : no
  ===============================

  Elevator Simulator ready. Type '?' for help.

  [T0000]> r 3 u
  Request submitted: floor 3 UP

  [T0000]> r 1 d
  Request submitted: floor 1 DOWN

  [T0000]> a 10
  [T0001][INF] LOOK: request floor 3 dir=UP -> Elevator 0 (cost 3)
  [T0001][INF] LOOK: request floor 1 dir=DOWN -> Elevator 1 (cost 1)
  ...
  [T0010][INF] Elevator 0: arrived at floor 3 – doors open
  [T0010][INF] Elevator 1: arrived at floor 1 – doors open

  ── Tick 10 ─────────────────────────────
    [E0] Floor:3 Dir:UP    State:DOOR_OPEN  Door:OPEN   Stops:[] dist:3 pax:1
    [E1] Floor:1 Dir:DOWN  State:DOOR_OPEN  Door:OPEN   Stops:[] dist:1 pax:1
    Queue depth: 0

  [T0010]> m

  === Simulation Metrics ===
    Ticks elapsed  : 10
    Total requests : 2
    Total served   : 2
    Scheduler      : LOOK
    Elevator 0     : dist=3  pax=1
    Elevator 1     : dist=1  pax=1
  ==========================

  [T0010]> q
  Exiting simulator.


--------------------------------------------------------------------------------
PROJECT STRUCTURE
--------------------------------------------------------------------------------

  config.h / config.c       SimConfig struct, constants, validation
  logger.h / logger.c       Levelled logger (DEBUG/INFO/WARN/ERROR)
  elevator.h / elevator.c   Per-elevator state machine
  scheduler.h / scheduler.c Strategy-pattern vtable + FCFS, LOOK, SSTF
  building.h / building.c   Simulation coordinator and request queue
  main.c                    Interactive CLI entry point
  test_runner.c             30 self-contained unit tests
  Makefile                  Build system
  CLAUDE.md                 AI-assisted development guide
  README.txt                This file


--------------------------------------------------------------------------------
EXTENDING THE SIMULATOR
--------------------------------------------------------------------------------

  See CLAUDE.md for detailed instructions on:
    - Adding a new scheduling algorithm
    - Increasing floors or elevators (edit config.h constants)
    - Adding a passenger capacity limit
    - Exporting tick-level CSV metrics


================================================================================
