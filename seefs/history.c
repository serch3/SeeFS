#include "include/seefs.h"
#include "include/history.h"
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#define HISTORY_INTERVAL 5 // seconds
#define MAX_SNAPSHOTS 10

struct snapshot {
    char timestamp[32];
    char *cmdline;
    size_t cmdline_len;
    char *status;
    size_t status_len;
    struct snapshot *next;
};

struct process_history {
    pid_t pid;
    struct snapshot *snapshots;
    struct process_history *next;
};

static struct process_history *history_head = NULL;
static pthread_mutex_t history_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t history_thread;
static bool history_running = false;

static void free_snapshot(struct snapshot *s) {
    if (s->cmdline) free(s->cmdline);
    if (s->status) free(s->status);
    free(s);
}

static void free_process_history(struct process_history *ph) {
    struct snapshot *s = ph->snapshots;
    while (s) {
        struct snapshot *next = s->next;
        free_snapshot(s);
        s = next;
    }
    free(ph);
}

/**
 * @brief Captures a new snapshot for a given process.
 * 
 * Reads the current cmdline and status from /proc and prepends a new snapshot
 * to the process's history. Also enforces the MAX_SNAPSHOTS limit by pruning
 * old snapshots.
 */
static void take_snapshot(struct process_history *ph) {
    struct snapshot *s = calloc(1, sizeof(struct snapshot));
    if (!s) return;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(s->timestamp, sizeof(s->timestamp), "%Y-%m-%d_%H-%M-%S", tm);

    seefs_proc_read_cmdline(ph->pid, &s->cmdline, &s->cmdline_len);
    seefs_proc_read_status(ph->pid, &s->status, &s->status_len);

    // Prepend
    s->next = ph->snapshots;
    ph->snapshots = s;

    // Prune
    int count = 0;
    struct snapshot *curr = ph->snapshots;
    while (curr) {
        count++;
        if (count >= MAX_SNAPSHOTS && curr->next) {
            struct snapshot *to_delete = curr->next;
            curr->next = to_delete->next;
            free_snapshot(to_delete);
        } else {
            curr = curr->next;
        }
    }
}

static int history_scan_cb(const struct seefs_proc_info *info, void *ctx) {
    // Check if we are already tracking this PID
    struct process_history *ph = history_head;
    while (ph) {
        if (ph->pid == info->pid) {
            take_snapshot(ph);
            return 0;
        }
        ph = ph->next;
    }

    // New process
    ph = calloc(1, sizeof(struct process_history));
    if (!ph) return 0;
    ph->pid = info->pid;
    ph->next = history_head;
    history_head = ph;
    take_snapshot(ph);

    return 0;
}

static void *history_worker(void *arg) {
    while (history_running) {
        pthread_mutex_lock(&history_lock);
        seefs_proc_iterate(history_scan_cb, NULL);
        pthread_mutex_unlock(&history_lock);
        sleep(HISTORY_INTERVAL);
    }
    return NULL;
}

void seefs_history_init(void) {
    history_running = true;
    pthread_create(&history_thread, NULL, history_worker, NULL);
}

void seefs_history_shutdown(void) {
    history_running = false;
    pthread_join(history_thread, NULL);
    
    pthread_mutex_lock(&history_lock);
    struct process_history *ph = history_head;
    while (ph) {
        struct process_history *next = ph->next;
        free_process_history(ph);
        ph = next;
    }
    history_head = NULL;
    pthread_mutex_unlock(&history_lock);
}

int seefs_history_get_timestamps(pid_t pid, char ***timestamps, size_t *count) {
    pthread_mutex_lock(&history_lock);
    struct process_history *ph = history_head;
    while (ph) {
        if (ph->pid == pid) break;
        ph = ph->next;
    }
    
    if (!ph) {
        pthread_mutex_unlock(&history_lock);
        return -ENOENT;
    }
    
    size_t n = 0;
    struct snapshot *s = ph->snapshots;
    while (s) {
        n++;
        s = s->next;
    }
    
    *timestamps = calloc(n, sizeof(char*));
    if (!*timestamps) {
        pthread_mutex_unlock(&history_lock);
        return -ENOMEM;
    }
    
    s = ph->snapshots;
    for (size_t i = 0; i < n; ++i) {
        (*timestamps)[i] = strdup(s->timestamp);
        s = s->next;
    }
    *count = n;
    
    pthread_mutex_unlock(&history_lock);
    return 0;
}

int seefs_history_get_data(pid_t pid, const char *timestamp, const char *filename, char **buffer, size_t *size) {
    pthread_mutex_lock(&history_lock);
    struct process_history *ph = history_head;
    while (ph) {
        if (ph->pid == pid) break;
        ph = ph->next;
    }
    
    if (!ph) {
        pthread_mutex_unlock(&history_lock);
        return -ENOENT;
    }
    
    struct snapshot *s = ph->snapshots;
    while (s) {
        if (strcmp(s->timestamp, timestamp) == 0) break;
        s = s->next;
    }
    
    if (!s) {
        pthread_mutex_unlock(&history_lock);
        return -ENOENT;
    }
    
    if (strcmp(filename, "cmdline") == 0) {
        if (s->cmdline) {
            *buffer = malloc(s->cmdline_len);
            memcpy(*buffer, s->cmdline, s->cmdline_len);
            *size = s->cmdline_len;
        } else {
            *buffer = NULL;
            *size = 0;
        }
    } else if (strcmp(filename, "status") == 0) {
        if (s->status) {
            *buffer = malloc(s->status_len);
            memcpy(*buffer, s->status, s->status_len);
            *size = s->status_len;
        } else {
            *buffer = NULL;
            *size = 0;
        }
    } else {
        pthread_mutex_unlock(&history_lock);
        return -ENOENT;
    }
    
    pthread_mutex_unlock(&history_lock);
    return 0;
}
