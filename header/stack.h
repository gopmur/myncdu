#pragma once

#include <stdbool.h>

typedef struct _ShmStack {
    int shmid;
    int n;
    int sp;
    int *d;
} ShmStack;

ShmStack *shm_stack_init(int n);
void shm_stack_delete(ShmStack *stack);
bool stack_is_full(ShmStack *stack);
bool stack_is_empty(ShmStack *stack);
void stack_push(ShmStack *stack, int v);
int stack_pop(ShmStack *stack);

