#include "collector.h"
#include "process.h"
#include "process.skel.h"
#include "events.h"
#include "json.h"
#include <cjson/cJSON.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>
#include <inttypes.h>

static int process_handle_event(void *ctx, void *data, size_t size) {
    (void)ctx;
    if (size != sizeof(struct event)) {
        fprintf(stderr, "Unexpected event size: %zu, expected: %zu\n", size, sizeof(struct event));    
        return 0;
    }
    const struct event *event = data;
    if (event->type == EVENT_PROCESS_EXEC) {
       char *json = process_event_to_json(event);
       if (!json) {
           fprintf(stderr, "JSON serialization error\n");
           return -EIO;
       }
       int result = puts(json);
       cJSON_free(json);
       if (result < 0 ) { return -EIO; }
    }
    else {
        fprintf(stderr, "Unknown record type: %u\n", event->type);
    }
    return 0;
}

void *process_run(void *arg) {  
    struct process_context *ctx = arg;

    struct process_bpf *skel = process_bpf__open();
    if (!skel) {
        perror("Error opening the process.bpf object");
        ctx->result = EXIT_FAILURE;
        atomic_store(&ctx->finished ,true);
        return NULL;
    }
    fprintf(stderr, "process.bpf object has been opened\n");
    
    int err = process_bpf__load(skel); 
    if (err != 0) {
        fprintf(stderr, "Error loading the process.bpf object into the kernel: %s\n", strerror(-err));
        process_bpf__destroy(skel);
        ctx->result = EXIT_FAILURE;
        atomic_store(&ctx->finished ,true);
        return NULL;
    }
    fprintf(stderr, "process.bpf object has been loaded into the kernel\n");
    
    err = process_bpf__attach(skel);
    if (err != 0) {
        fprintf(stderr, "Error attaching 'handle_exec' program to tracepoint: %s\n", strerror(-err));
        process_bpf__destroy(skel);
        ctx->result = EXIT_FAILURE;
        atomic_store(&ctx->finished, true);
        return NULL;
    }
    fprintf(stderr, "'handle_exec' has been attached to tracepoint\n");
       
    struct ring_buffer *rb = ring_buffer__new(bpf_map__fd(skel->maps.events), process_handle_event, NULL, NULL);
    if (!rb) {
        fprintf(stderr, "Unable to access ringbuf\n");
        process_bpf__destroy(skel);
        ctx->result = EXIT_FAILURE;
        atomic_store(&ctx->finished, true);
        return NULL;
    }

    bool failed = false;
    while(!atomic_load(&ctx->collector->stop_requested)) {
        err = ring_buffer__poll(rb, 200);
        if (err == -EINTR) { continue; }
        if (err < 0 ) {
            fprintf(stderr, "Ringbuf read error: %s\n", strerror(-err));
            failed = true;
            break;
        }
    }
    ring_buffer__free(rb);

    fprintf(stderr, "Normal termination the collector\n");
    process_bpf__destroy(skel);
    if (failed) { ctx->result = EXIT_FAILURE; }
    else { ctx->result = EXIT_SUCCESS; }
    atomic_store(&ctx->finished ,true);
    return NULL;
}


