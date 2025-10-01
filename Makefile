# Makefile for grpc-c library
# Supports Linux and macOS

# Compiler and flags
CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -Werror -O2 -fPIC -pthread -std=c99 -fstack-protector-strong -D_FORTIFY_SOURCE=2
INCLUDES = -Iinclude -Isrc
LDFLAGS = -pthread -lz

# Detect OS
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    OS = linux
    SHARED_EXT = so
    LDFLAGS += -lrt
endif
ifeq ($(UNAME_S),Darwin)
    OS = macos
    SHARED_EXT = dylib
endif

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
LIB_DIR = lib
BIN_DIR = bin
TEST_DIR = test
EXAMPLE_DIR = examples

# Library name
LIB_NAME = grpc-c
STATIC_LIB = $(LIB_DIR)/lib$(LIB_NAME).a
SHARED_LIB = $(LIB_DIR)/lib$(LIB_NAME).$(SHARED_EXT)

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Test sources
TEST_SOURCES = $(wildcard $(TEST_DIR)/*_test.c)
TEST_BINARIES = $(TEST_SOURCES:$(TEST_DIR)/%.c=$(BIN_DIR)/%)

# Example sources
EXAMPLE_SOURCES = $(wildcard $(EXAMPLE_DIR)/*.c)
EXAMPLE_BINARIES = $(EXAMPLE_SOURCES:$(EXAMPLE_DIR)/%.c=$(BIN_DIR)/%)

# Default target
all: directories static shared

# Create necessary directories
directories:
	@mkdir -p $(BUILD_DIR) $(LIB_DIR) $(BIN_DIR)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build static library
static: $(OBJECTS)
	@echo "Creating static library $(STATIC_LIB)..."
	@$(AR) rcs $(STATIC_LIB) $(OBJECTS)
	@echo "Static library created successfully."

# Build shared library
shared: $(OBJECTS)
	@echo "Creating shared library $(SHARED_LIB)..."
	@$(CC) -shared $(OBJECTS) $(LDFLAGS) -o $(SHARED_LIB)
	@echo "Shared library created successfully."

# Build tests
tests: static $(TEST_BINARIES)

$(BIN_DIR)/%_test: $(TEST_DIR)/%_test.c
	@echo "Building test $@..."
	@$(CC) $(CFLAGS) $(INCLUDES) $< -L$(LIB_DIR) -l$(LIB_NAME) $(LDFLAGS) -o $@

# Build examples
examples: static $(EXAMPLE_BINARIES)

$(BIN_DIR)/%: $(EXAMPLE_DIR)/%.c
	@echo "Building example $@..."
	@$(CC) $(CFLAGS) $(INCLUDES) $< -L$(LIB_DIR) -l$(LIB_NAME) $(LDFLAGS) -o $@

# Run tests
check: tests
	@echo "Running tests..."
	@for test in $(TEST_BINARIES); do \
		echo "Running $$test..."; \
		LD_LIBRARY_PATH=$(LIB_DIR):$$LD_LIBRARY_PATH ./$$test || exit 1; \
	done
	@echo "All tests passed!"

# Install library and headers
install: all
	@echo "Installing grpc-c..."
	@install -d $(DESTDIR)/usr/local/lib
	@install -d $(DESTDIR)/usr/local/include/grpc
	@install -m 644 $(STATIC_LIB) $(DESTDIR)/usr/local/lib/
	@install -m 755 $(SHARED_LIB) $(DESTDIR)/usr/local/lib/
	@install -m 644 $(INC_DIR)/grpc/*.h $(DESTDIR)/usr/local/include/grpc/
	@echo "Installation complete."

# Uninstall
uninstall:
	@echo "Uninstalling grpc-c..."
	@rm -f $(DESTDIR)/usr/local/lib/lib$(LIB_NAME).*
	@rm -rf $(DESTDIR)/usr/local/include/grpc
	@echo "Uninstallation complete."

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(LIB_DIR) $(BIN_DIR)
	@echo "Clean complete."

# Format code (if clang-format is available)
format:
	@if command -v clang-format >/dev/null 2>&1; then \
		echo "Formatting code..."; \
		find $(SRC_DIR) $(INC_DIR) $(TEST_DIR) $(EXAMPLE_DIR) -name "*.c" -o -name "*.h" | xargs clang-format -i; \
		echo "Formatting complete."; \
	else \
		echo "clang-format not found, skipping formatting."; \
	fi

# Help target
help:
	@echo "Available targets:"
	@echo "  all       - Build static and shared libraries (default)"
	@echo "  static    - Build static library only"
	@echo "  shared    - Build shared library only"
	@echo "  tests     - Build test suite"
	@echo "  examples  - Build example programs"
	@echo "  check     - Run test suite"
	@echo "  install   - Install library and headers"
	@echo "  uninstall - Uninstall library and headers"
	@echo "  clean     - Remove build artifacts"
	@echo "  format    - Format code with clang-format"
	@echo "  help      - Show this help message"

.PHONY: all directories static shared tests examples check install uninstall clean format help
