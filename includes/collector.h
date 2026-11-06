#ifndef COLLECTOR_H
#define COLLECTOR_H

#include <stdatomic.h>

struct collector_context {
    atomic_bool stop_requested;
};
#endif
