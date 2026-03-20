# Simple Makefile for Lab 4 Scheduler
#make        -compile, generate ./groupX
#make clean  -remove ./groupX

CC = gcc
CFLAGS = -Wall -Wextra -std=c11

TARGET = groupX_scheduler
SRCS = groupX_scheduler.c queue.c
HEADERS = queue.h

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)