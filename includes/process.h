#ifndef PROCESS_H
#define PROCESS_H

#include <stdatomic.h>

struct collector_context;

struct process_context {
    struct collector_context *collector;
    int result;
    atomic_bool finished;
};

void *process_run(void *arg);
#endif
