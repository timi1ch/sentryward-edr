#ifndef JSON_H
#define JSON_H
#include <linux/types.h>

struct event;

char *process_event_to_json(const struct event *event);

char *count_of_errwr_to_rb(__u64 cnt);

#endif
