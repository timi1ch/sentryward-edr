#ifndef JSON_H
#define JSON_H

struct event;

char *process_event_to_json(const struct event *event);

#endif
