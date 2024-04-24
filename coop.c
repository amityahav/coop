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
        // TODO: point rsp to the stack

        __scheduler->current->status = RUNNING;
        __scheduler->current->func(__scheduler->current->args);
        // coroutine finished its execution, 
        // its resources can be safely freed
        longjmp(__scheduler->context, EXIT);
    } else if (__scheduler->current->status == RUNNABLE) {
        // coroutine was already running before,
        // resume execution
        __scheduler->current->status = RUNNING;
        longjmp(selected->context, 1);
    }
}

// solely for allowing usage before declaration
void __curr_co_free();

void __scheduler_entry() {
    switch (setjmp(__scheduler->context)) {
    case EXIT:
        __curr_co_free();
    case 0: 
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
    new_coroutine->stack_top = (char*)new_coroutine->stack_bottom + STACK_SIZE;
    new_coroutine->status = CREATED;

    if (!__scheduler) {
        // initiate scheduler when invoking main coroutine
        __scheduler = (struct scheduler*)malloc(sizeof(struct scheduler));
    }

    char is_main = !__scheduler->list.head;

    __list_append(&__scheduler->list, new_coroutine);
    
    if (is_main) {
        // should block until all coroutines are done
        __scheduler_entry();
    } 
}

void __curr_co_free() {
    free(__scheduler->current->stack_bottom);
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
    coop(co1, NULL);
}