#include <stdlib.h>

#include "darray.h"

void thread_handles_init(ThreadHandles *threadHandles) {
    threadHandles->len = 0;
    threadHandles->allocated = 0;
    threadHandles->ids = NULL;
}

void thread_handles_add(ThreadHandles *threadHandles, pthread_t ptid) {
    if (threadHandles->allocated == 0) {
        threadHandles->ids = (pthread_t *) malloc(sizeof(pthread_t));
        threadHandles->ids[threadHandles->len] = ptid;
        threadHandles->len++;
        threadHandles->allocated++;
        return;
    }
    if (threadHandles->allocated > threadHandles->len) {
        threadHandles->ids[threadHandles->len] = ptid;
        threadHandles->len++;
    }
    else {
        pthread_t *tmp = threadHandles->ids;
        threadHandles->allocated *= 2;
        threadHandles->ids = (pthread_t *) malloc(sizeof(pthread_t)*threadHandles->allocated);
        for (int i = 0; i < threadHandles->len; i++) {
            threadHandles->ids[i] = tmp[i];
        }
        threadHandles->ids[threadHandles->len] = ptid;
        threadHandles->len++;
    }
}

void thread_handles_delete(ThreadHandles *threadHandles) {
    free(threadHandles->ids);
}