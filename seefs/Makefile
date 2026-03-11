# Compiler and flags
CC = gcc

FUSE_PKG := $(shell pkg-config --exists fuse3 && echo fuse3 || echo fuse)
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -D_FILE_OFFSET_BITS=64 -D_POSIX_C_SOURCE=200809L $(shell pkg-config $(FUSE_PKG) --cflags) -pthread
LDFLAGS = $(shell pkg-config $(FUSE_PKG) --libs) -pthread

# Source and build directories
SRCDIR = .
BUILDDIR = build

# Source files
SRCS = seefs_main.c fuse_ops.c path_ops.c file.c proc_data.c history.c path_parser.c
OBJS = $(SRCS:%.c=$(BUILDDIR)/%.o)

# Target executable
TARGET = $(BUILDDIR)/seefs

.PHONY: all clean

all: $(TARGET)

# Link the target
$(TARGET): $(OBJS) | $(BUILDDIR)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

# Compile source files into build directory
$(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure build directory exists
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Clean up
clean:
	rm -rf $(BUILDDIR)

# Tests
test: $(BUILDDIR)/path_parser.o | $(BUILDDIR)
	$(CC) $(CFLAGS) tests/test_path_parser.c $(BUILDDIR)/path_parser.o -o $(BUILDDIR)/test_path_parser $(LDFLAGS)
	$(BUILDDIR)/test_path_parser
