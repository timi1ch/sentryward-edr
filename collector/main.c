#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "collector.h"
#include "process.h"


int main (void) {
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    int err = pthread_sigmask(SIG_BLOCK, &signals, NULL);
    if (err) {
        fprintf(stderr, "Error blocking signals: %s\n", strerror(err));
        return EXIT_FAILURE;
    }
    struct collector_context ctx;
    atomic_init(&ctx.stop_requested, false);

    struct process_context process_ctx = {
        .collector = &ctx,
        .result = EXIT_FAILURE,
    };
    atomic_init(&process_ctx.finished, false);

    pthread_t process;
    err = pthread_create(&process, NULL, process_run, &process_ctx);
    if (err != 0) {
        fprintf(stderr, "Error creating thread: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    struct timespec timeout = {
        .tv_sec = 1,
        .tv_nsec = 0,
    };

    bool failed = false;
    while (!atomic_load(&process_ctx.finished)) {
        int sig = sigtimedwait(&signals, NULL, &timeout);
        if (sig == SIGINT) { break; }
        if (sig == SIGTERM) { break; }
        if (sig == -1 ) {
            if (errno == EAGAIN) { continue; }
            if (errno == EINTR) { continue; }
            perror("Error waiting for signals");
            failed = true;
            break;
        }
    }
    atomic_store(&ctx.stop_requested, true);
    err = pthread_join(process, NULL);
    if (err!= 0) {
        fprintf(stderr, "Thread termination error: %s\n", strerror(err));
        return EXIT_FAILURE;
    }
    if (failed) {
        return EXIT_FAILURE;
    }
    return process_ctx.result;
} 
