#include "json.h"
#include "events.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

char *process_event_to_json(const struct event *event) {
    
    if (event == NULL) { return NULL; }
    if (event->type != EVENT_PROCESS_EXEC) { return NULL; }

    char monotonic_ns[21];
    int length = snprintf(monotonic_ns, sizeof(monotonic_ns), "%" PRIu64, (uint64_t)event->monotonic_ns);
    if (length < 0 || (size_t)length >= sizeof(monotonic_ns)) { return NULL; }

    const char *name_end = memchr(event->task, '\0', sizeof(event->task));
    if (!name_end) { return NULL; }
    const int name_length = name_end - event->task;
    char name_hex[2 * sizeof(event->task) + 1];
    static const char hex_digits[] = "0123456789abcdef";
    for (int i = 0; i < name_length; ++i) {
        unsigned char byte = event->task[i];
        name_hex[2*i] = hex_digits[byte >> 4];
        name_hex[2*i+1] = hex_digits[byte & 0x0F];
    }
    name_hex[2*name_length] = '\0';

    cJSON *root = cJSON_CreateObject();
    if (!root) { return NULL; }

    if (!cJSON_AddNumberToObject(root, "version", 1)) { cJSON_Delete(root); return NULL; }
    if (!cJSON_AddStringToObject(root, "platform", "linux")) { cJSON_Delete(root); return NULL; }
    if (!cJSON_AddStringToObject(root, "event_type", "process.exec")) { cJSON_Delete(root); return NULL; }
    if (!cJSON_AddStringToObject(root, "monotonic_ns", monotonic_ns)) { cJSON_Delete(root); return NULL; }

    cJSON *process = cJSON_AddObjectToObject(root, "process");
    if (!process) { cJSON_Delete(root); return NULL; }

    if (!cJSON_AddNumberToObject(process, "pid", event->pid)) { cJSON_Delete(root); return NULL; }
    if (!cJSON_AddNumberToObject(process, "tid", event->tid)) { cJSON_Delete(root); return NULL; }
    if (!cJSON_AddNumberToObject(process, "parent_pid", event->ppid)){ cJSON_Delete(root); return NULL; }
    if (!cJSON_AddStringToObject(process, "name_bytes_hex", name_hex)) { cJSON_Delete(root); return NULL; }

    cJSON *user = cJSON_AddObjectToObject(root, "user");
    if (!user) { cJSON_Delete(root); return NULL; }

    if (!cJSON_AddNumberToObject(user, "uid", event->uid)){ cJSON_Delete(root); return NULL; }
    if (!cJSON_AddNumberToObject(user, "gid", event->gid)){ cJSON_Delete(root); return NULL; }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}
