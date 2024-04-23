#include <stdlib.h>
#include <stdio.h>
#include "coop.h"

struct scheduler* __scheduler;

void __schedule() {
    struct coroutine* selected;

    if (!__scheduler->current) {
        // last running coroutine finished its execution.
        // just pick the tail to run next
        __scheduler->current = __scheduler->tail;
        selected = __scheduler->current;
    } else {
        // TODO: should consider iterating the list in order
        // to find the next coroutine to run in cases where the next
        // is waiting for IO for example
        selected = __scheduler->current->next;
    }

    if (!selected) {
        // all coroutines are done
        return;
    }

    if (selected->status == CREATED) {
        // TODO: point rsp to the stack

        __scheduler->current->status = RUNNABLE;
        __scheduler->current = selected;
        __scheduler->current->status = RUNNING;
        __scheduler->current->func(__scheduler->current->args);
        // coroutine finished its execution, 
        // its resources can be safely freed
        longjmp(__scheduler->context, EXIT);
    } else {
        // coroutine was already running before,
        // resume execution
        __scheduler->current->status = RUNNABLE;
        __scheduler->current = selected;
        __scheduler->current->status = RUNNING;
        longjmp(selected->context, 1);
    }
}

// solely for allowing usage before declaration
void __co_free();

void __scheduler_entry() {
    switch (setjmp(__scheduler->context)) {
    case EXIT:
        __co_free();
    case 0: 
    case SCHED:
        __schedule();
        break;
    default:
        printf("invalid scheduler code\n");
        break;
    }
}

void co_create(void (*func)(void*), void* args) {
    static int co_id;

    struct coroutine* new_coroutine = (struct coroutine*)malloc(sizeof(struct coroutine));
    new_coroutine->func = func;
    new_coroutine->args = args;
    new_coroutine->id = co_id++;
    new_coroutine->stack_bottom = malloc(STACK_SIZE);
    new_coroutine->stack_top = (char*)new_coroutine->stack_bottom + STACK_SIZE; // TODO: is this correct?
    new_coroutine->status = CREATED;

    if (!__scheduler) {
        // initiate scheduler when invoking main coroutine
        __scheduler = (struct scheduler*)(sizeof(struct scheduler));
        __scheduler->tail = new_coroutine;
        __scheduler->tail->next = new_coroutine;
        __scheduler->current = new_coroutine;

        // should block until all coroutines are done
        __scheduler_entry();
    } else {
        new_coroutine->next = __scheduler->tail->next;
        __scheduler->tail->next = new_coroutine;
        __scheduler->tail = new_coroutine;
    }
}

void __co_free() {
    free(__scheduler->current->stack_bottom);
    // TODO: unlink from coroutine list
    
    free(__scheduler->current);
    __scheduler->current = NULL;
}

void yield() {
    if (!__scheduler || !__scheduler->current) {
        printf("unexpected yield\n");
        return;
    }

    if (!setjmp(__scheduler->current->context)) {
        // save current coroutine's context and let the 
        // scheduler execute
        longjmp(__scheduler->context, SCHED);
    }

    // resume execution
}

void co1(void* args) {

}

int main(int argc, char**argv) {
    // example usage
    co_create(co1, NULL);
}