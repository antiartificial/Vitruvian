# Standalone Makefile for network backend tests.
# Runs on Linux or macOS without the full Vitruvian CMake build.
#
# Usage:
#   make          - build and run tests
#   make build    - build only
#   make test     - build and run
#   make clean    - remove build artifacts
#   make valgrind - run under valgrind (Linux)

ROOT := $(shell git -C $(dir $(lastword $(MAKEFILE_LIST))) rev-parse --show-toplevel)

CXX      ?= g++
CXXFLAGS := -std=c++14 -Wall -Wextra -Wno-unused-parameter -g -O0

# Only needs NetworkBackend.h (self-contained) and MockNetworkBackend.h
INCLUDES := \
	-I$(ROOT)/headers/os/net \
	-I$(ROOT)/src/kits/network/libnetapi

SRCS := \
	test_backend.cpp \
	$(ROOT)/src/kits/network/libnetapi/MockNetworkBackend.cpp

TARGET := test_network_backend

.PHONY: all build test clean valgrind

all: test

build: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

test: $(TARGET)
	@echo ""
	@./$(TARGET)
	@echo ""

valgrind: $(TARGET)
	valgrind --leak-check=full --error-exitcode=1 ./$(TARGET)

clean:
	rm -f $(TARGET)
