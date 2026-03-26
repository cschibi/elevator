# =========================================================
# Makefile  –  Elevator Simulator
#
#  Targets:
#    make          – build ./elevator_sim (default)
#    make test     – build & run ./elevator_test
#    make check    – alias for test
#    make clean    – remove build artefacts
#    make all      – sim + test
# =========================================================

CC      := gcc
CFLAGS  := -std=c99 -Wall -Wextra -Wpedantic \
           -Wshadow -Wformat=2 -Wstrict-prototypes \
           -fstack-protector-strong \
           -D_POSIX_C_SOURCE=200809L \
           -g3 -O2
LDFLAGS :=

# ── Source groups ─────────────────────────────────────────
COMMON_SRCS := config.c logger.c elevator.c scheduler.c building.c
SIM_SRCS    := $(COMMON_SRCS) main.c
TEST_SRCS   := $(COMMON_SRCS) test_runner.c

SIM_OBJS  := $(SIM_SRCS:.c=.o)
TEST_OBJS := $(TEST_SRCS:.c=.o)

SIM_BIN   := elevator_sim
TEST_BIN  := elevator_test

# ── Default target ────────────────────────────────────────
.PHONY: all sim test check clean

all: sim test

sim: $(SIM_BIN)

$(SIM_BIN): $(SIM_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built: $@"

# ── Test target ───────────────────────────────────────────
test: $(TEST_BIN)
	./$(TEST_BIN)

check: test

$(TEST_BIN): $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built: $@"

# ── Generic compile rule ──────────────────────────────────
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Dependencies (manual – avoids makedepend requirement) ─
config.o:    config.c config.h logger.h
logger.o:    logger.c logger.h
elevator.o:  elevator.c elevator.h config.h logger.h
scheduler.o: scheduler.c scheduler.h elevator.h building.h logger.h
building.o:  building.c building.h elevator.h scheduler.h logger.h config.h
main.o:      main.c config.h building.h logger.h scheduler.h
test_runner.o: test_runner.c config.h building.h elevator.h scheduler.h logger.h

# ── Clean ─────────────────────────────────────────────────
clean:
	rm -f $(SIM_OBJS) $(TEST_OBJS) $(SIM_BIN) $(TEST_BIN) elevator.log
	@echo "Cleaned."
