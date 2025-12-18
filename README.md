# SeeFS (See File System)

SeeFS is a FUSE-based user-space filesystem that provides a structured, human-readable view of the Linux `/proc` filesystem. 

Instead of a flat list of PIDs, SeeFS organizes processes by **User** and **Application Name**, making it easier to inspect running tasks and their resources.

## Features

- **User-Centric View**: Processes are grouped by the user who owns them (e.g., `/see/users/root/`).
- **Application Grouping**: Processes are further grouped by their executable name (e.g., `/see/users/Jane/applications/firefox/`).
- **Simplified Metadata**: Exposes key process information like command line arguments, status, and memory usage in a clean format.
- **Historical Snapshots**: View past states (cmdline, status) of processes via a rolling history buffer.
- **FUSE Implementation**: Runs entirely in user space using the FUSE (Filesystem in Userspace) library.

## Directory Structure

SeeFS transforms the chaotic `/proc` structure into a clean hierarchy:

```
/see
├── users/
│   ├── root/
│   │   ├── applications/
│   │   │   ├── bash/
│   │   │   │   ├── 1234/          # PID
│   │   │   │   │   ├── cmdline
│   │   │   │   │   ├── status
│   │   │   │   │   ├── memory
│   │   │   │   │   └── history/   # Historical snapshots
│   │   │   │   │       └── 2023-10-27_10-00-00/
│   │   │   │   │           ├── cmdline
│   │   │   │   │           └── status
│   │   │   ├── sshd/
│   │   │   │   └── ...
│   │   └── kernel_threads/
│   │       └── ...
│   └── [username]/
│       └── ...
```

## Prerequisites

To build and run SeeFS, you need a Linux environment with FUSE installed.

*   **GCC** (GNU Compiler Collection)
*   **Make**
*   **libfuse-dev** (FUSE development libraries, version 3.x recommended)

On Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install build-essential libfuse-dev pkg-config
```

## Building

1.  Navigate to the source directory:
    ```bash
    cd seefs
    ```

2.  Compile the project:
    ```bash
    make
    ```

    This will create the executable at `seefs/build/seefs`.

## Usage

1.  **Create a mount point**:
    ```bash
    mkdir /tmp/see
    ```

2.  **Mount SeeFS**:
    ```bash
    ./build/seefs /tmp/see
    ```
    *(Note: If you want to run it in the foreground to see debug output, use the `-f` flag: `./build/seefs -f /tmp/see`)*

3.  **Explore the filesystem**:
    ```bash
    # List users
    ls -l /tmp/see/users/

    # See your own applications
    ls -l /tmp/see/users/$USER/applications/

    # Check a specific process
    cat /tmp/see/users/$USER/applications/bash/<PID>/cmdline
    ```

4.  **Unmount**:
    ```bash
    fusermount -u /tmp/see
    ```

## Architecture

SeeFS acts as a translation layer. When you access a file in SeeFS:
1.  The kernel VFS passes the request to the FUSE kernel module.
2.  FUSE forwards the request to the SeeFS user-space application.
3.  SeeFS parses the virtual path (e.g., `/users/root/...`).
4.  SeeFS maps this path to the corresponding entry in `/proc` (e.g., `/proc/1234/...`).
5.  Data is read from `/proc`, formatted, and returned to the user.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
