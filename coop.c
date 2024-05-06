#include <stdlib.h>
#include <stdio.h>
#include "coop.h"

struct scheduler* __scheduler;

struct coroutine* __list_pop(struct coop_list* l) {
    struct coroutine* curr = l->head;
    if (!curr) {
        // list is empty
        return NULL;
    }

    if (l->head == l->tail) {
        // list is empty after the pop
        l->head = l->tail = NULL;
    } else {
        l->head = l->head->next;
    }

    curr->next = NULL;

    return curr;
}

void __list_append(struct coop_list* l, struct coroutine* curr) {
    if (!l->tail) {
        // list is empty
        l->head = l->tail = curr;
    } else {
        l->tail->next = curr;
        l->tail = curr;
    }
}

void __exit_current_coop() {
    // wrapping this call inside a function solves 
    // a weird segfault that i couldnt figure its cause
	longjmp(__scheduler->context, EXIT);
}

void __schedule() {
    if (__scheduler->current) {
        __scheduler->current->status = RUNNABLE;
        // append last yielded running coroutine to the end of the list
        __list_append(&__scheduler->list, __scheduler->current);
        __scheduler->current = NULL;
    }

    // pick next coroutine to run
    struct coroutine* selected = __list_pop(&__scheduler->list);
    if (!selected) {
        // all coroutines are done
        return;
    }

    __scheduler->current = selected;

    if (__scheduler->current->status == CREATED) {
        // modifying the stack pointer to point
        // to the to-be-run coroutine's stack
        // ARM specific for now
        asm volatile(
            "mov sp, %0"
            : "+r" (selected->stack_top)
            : 
            :
        );

        __scheduler->current->status = RUNNING;
        __scheduler->current->func(__scheduler->current->args);
        // coroutine finished its execution, 
        // its resources can be safely freed
        __exit_current_coop();
    } else if (__scheduler->current->status == RUNNABLE) {
        // coroutine was already running before,
        // resume execution
        __scheduler->current->status = RUNNING;
        longjmp(selected->context, 1);
    }
}

void __curr_co_free() {
    free(__scheduler->current->stack_bottom);
    free(__scheduler->current);
    __scheduler->current = NULL;
}

void __scheduler_entry() {
    switch (setjmp(__scheduler->context)) {
    case EXIT:
        __curr_co_free();
    case INIT: 
    case SCHED:
        __schedule();
        break;
    default:
        printf("invalid scheduler code\n");
        break;
    }
}

void coop(void (*func)(void*), void* args) {
    static int co_id;

    struct coroutine* new_coroutine = (struct coroutine*)malloc(sizeof(struct coroutine));
    new_coroutine->func = func;
    new_coroutine->args = args;
    new_coroutine->id = co_id++;
    new_coroutine->stack_bottom = malloc(STACK_SIZE);
    new_coroutine->stack_top = new_coroutine->stack_bottom + STACK_SIZE;
    new_coroutine->status = CREATED;

    if (!__scheduler) {
        // initiate scheduler when invoking main coroutine
        __scheduler = (struct scheduler*)malloc(sizeof(struct scheduler));
    }

    char is_main = !__scheduler->list.head && !__scheduler->current;

    __list_append(&__scheduler->list, new_coroutine);
    
    if (is_main) {
        // should block until all coroutines including the parent coroutine are done
        __scheduler_entry();
    } 
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

void coop3(void *args) {
    for (int i = 0; i < 6; i++) {
        printf("hi from coop3 %d\n", i);
        yield();
    }
}

void coop2(void *args) {
    coop(coop3, NULL);
    for (int i = 0; i < 5; i++) {
        printf("hi from coop2 %d\n", i);
        yield();
    }
}

void coop1(void* args) {
    coop(coop2, NULL);
    for (int i = 0; i < 4; i++) {
        printf("hi from coop1 %d \n", i);
        yield();
    }
}

int main(int argc, char**argv) {
    // example usage
    yield();
    coop(coop1, NULL);
    //coop(coop3, NULL);
}