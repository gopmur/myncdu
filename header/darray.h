#pragma once

#include <pthread.h>

typedef struct _ThreadHandles {
    int allocated;
    int len;
    pthread_t *ids;
} ThreadHandles;

void thread_handles_init(ThreadHandles *threadHandles);
void thread_handles_add(ThreadHandles *threadHandles, pthread_t ptid);
void thread_handles_delete(ThreadHandles *threadHandles);