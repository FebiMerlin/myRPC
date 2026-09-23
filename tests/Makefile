# Makefile -- unit and integration tests of myRPC.
# Copyright (C) 2026 myRPC project.

CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -std=gnu11 -O1 -g
CPPFLAGS += -I../common -D_GNU_SOURCE

COMMON_DIR = ../common
COMMON_SRC = $(COMMON_DIR)/myrpc_log.c \
             $(COMMON_DIR)/myrpc_config.c \
             $(COMMON_DIR)/myrpc_proto.c

.PHONY: all test clean

all: test_proto

test_proto: test_proto.c $(COMMON_SRC) $(COMMON_DIR)/myrpc_common.h
	$(CC) $(CFLAGS) $(CPPFLAGS) test_proto.c $(COMMON_SRC) -o test_proto

test: test_proto
	./test_proto
	./run_integration.sh

clean:
	rm -f test_proto
