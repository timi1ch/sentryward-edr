#ifndef EVENT_H
#define EVENT_H

#ifndef __VMLINUX_H__
#include <linux/types.h>
#endif

#define TASK_LEN 16
#define FILENAME_LEN 256
#define ARGS_LEN 1024

enum event_type {
    EVENT_PROCESS_EXEC = 1,
    EVENT_PROCESS_EXIT = 2,
    // EVENT_NETWORK = 3,
    // EVENT_FS = 4
};

struct event {
    __u32 type;
    __u32 pid;
    __u32 tid;
    __u32 ppid;
    __u32 uid;
    __u32 gid;
    __u64 monotonic_ns;
    char task[TASK_LEN];
};

struct process_exec_event {
    struct event event;
    char filename[FILENAME_LEN];
    char args[ARGS_LEN];
};

#endif
