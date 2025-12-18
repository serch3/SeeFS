#!/bin/bash
set -e

# Configuration
MOUNT_POINT="/tmp/see"
BUILD_DIR="seefs/build"
EXECUTABLE="$BUILD_DIR/seefs"

# Ensure we are in the project root
if [ ! -d "seefs" ]; then
    echo "Error: Please run this script from the project root."
    exit 1
fi

# Build if necessary
if [ ! -f "$EXECUTABLE" ]; then
    echo "Building SeeFS..."
    make -C seefs
fi

# Create mount point
if [ ! -d "$MOUNT_POINT" ]; then
    echo "Creating mount point at $MOUNT_POINT..."
    mkdir -p "$MOUNT_POINT"
fi

# Check if already mounted
if mount | grep -q "$MOUNT_POINT"; then
    echo "Unmounting existing instance..."
    fusermount -u "$MOUNT_POINT"
fi

# Run
echo "Mounting SeeFS at $MOUNT_POINT..."
echo "Running in foreground. Press Ctrl+C to stop."
"$EXECUTABLE" -f "$MOUNT_POINT"
