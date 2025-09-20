
# Makefile for SeeFS

# Compiler and flags
CC = gcc
CFLAGS = -Wall `pkg-config fuse --cflags`
LDFLAGS = `pkg-config fuse --libs`

# Source files
SRCS = seefs.c super.c inode.c file.c proc_data.c
OBJS = $(SRCS:.c=.o)

# Target executable
TARGET = seefs

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)
