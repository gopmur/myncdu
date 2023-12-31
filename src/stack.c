#include <stdlib.h>
#include <sys/shm.h>

#include "stack.h"

ShmStack *shm_stack_init(int n) {
    key_t key = ("myncdu", 65);
    int shmid = shmget(key, sizeof(int)*n + sizeof(ShmStack), 0666|IPC_CREAT);
    ShmStack *shmStack = (ShmStack *) shmat(shmid, NULL, 0);
    shmStack->shmid = shmid;
    shmStack->sp = -1;
    shmStack->d = (int *) ((void *) shmStack + sizeof(ShmStack));
    shmStack->n = n;
    return shmStack;
}

void shm_stack_delete(ShmStack *stack) {
    shmctl(stack->shmid, IPC_RMID, NULL);
}

bool stack_is_full(ShmStack *stack) {
    return stack->sp == stack->n - 1;
}
bool stack_is_empty(ShmStack *stack) {
    return stack->sp == -1;
}

void stack_push(ShmStack *stack, int v) {
    if (!stack_is_full(stack)) {
        stack->d[++stack->sp] = v;
    }
}

int stack_pop(ShmStack *stack) {
    if (!stack_is_empty(stack)) {
        return stack->d[stack->sp--];
    }
}