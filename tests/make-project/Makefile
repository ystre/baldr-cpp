# Minimal Makefile-based C++ project, used to manually exercise `baldr build`
# against a non-CMake build system.

CXX ?= c++
CXXFLAGS ?= -std=c++20 -Wall -Wextra
BUILD_DIR ?= build

hello: main.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $(BUILD_DIR)/hello main.cpp

.PHONY: clean
clean:
	rm -f hello
