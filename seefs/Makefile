# Compiler and flags
CC = gcc
CFLAGS = -Wall `pkg-config fuse --cflags`
LDFLAGS = `pkg-config fuse --libs`

# Source and build directories
SRCDIR = .
BUILDDIR = build

# Source files
SRCS = seefs_main.c super.c inode.c file.c proc_data.c
OBJS = $(SRCS:%.c=$(BUILDDIR)/%.o)

# Target executable
TARGET = seefs

.PHONY: all clean

all: $(TARGET)

# Link the target
$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

# Compile source files into build directory
$(BUILDDIR)/%.o: %.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure build directory exists
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# Clean up
clean:
	rm -rf $(BUILDDIR) $(TARGET)
