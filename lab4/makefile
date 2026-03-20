# =============================================================
# Makefile for Lab 4 - HOST Scheduler
# =============================================================
# make           - compile release build  -> ./groupX_scheduler
# make debug     - compile with debug flags (AddressSanitizer)
# make run       - compile and run with test_input.txt
# make check     - run and diff against test_result.txt
# make clean     - remove build artifacts
# =============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11
DBGFLAGS = $(CFLAGS) -g -O0 -fsanitize=address,undefined

TARGET  = groupX_scheduler
SRCS    = groupX_scheduler.c queue.c
HEADERS = queue.h
OBJS    = $(SRCS:.c=.o)

# -----------------------------------------------
# Default: release build using separate compilation
# -----------------------------------------------
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# -----------------------------------------------
# Debug build (AddressSanitizer + UBSan)
# -----------------------------------------------
debug: CFLAGS = $(DBGFLAGS)
debug: clean $(TARGET)

# -----------------------------------------------
# Run against the provided test input
# -----------------------------------------------
run: all
	./$(TARGET) < test_input.txt

# -----------------------------------------------
# Run and diff against expected output
# -----------------------------------------------
check: all
	./$(TARGET) < test_input.txt > /tmp/my_result.txt
	diff /tmp/my_result.txt test_result.txt && echo "✓ Output matches expected" || echo "✗ Differences found"

# -----------------------------------------------
# Remove all build artifacts
# -----------------------------------------------
clean:
	rm -f $(TARGET) $(OBJS) /tmp/my_result.txt

.PHONY: all debug run check clean