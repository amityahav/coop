#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "coop.h"

struct scheduler* __scheduler;

void* __worker_loop(void* arg);

void __init_scheduler() {
    __scheduler = (struct scheduler*)malloc(sizeof(struct scheduler));
    pthread_mutex_init(&__scheduler->coop_list.mu, NULL);
    pthread_mutex_init(&__scheduler->io_queue.mu, NULL);
    pthread_create(&__scheduler->worker_thread, NULL, __worker_loop, NULL);
}

void __exit_current_coop() {
    // wrapping this call inside a function solves 
    // a weird segfault that i couldnt figure its cause
	longjmp(__scheduler->context, EXIT);
}

struct coroutine* __pick_next_coop() {
    // TODO: should check if the io queue is not empty/ worker is processing IO
    // before deciding that there are no more coops to run solely based on the coop list
    // it should be done carefully 
    return NULL;
}

void __schedule() {
    if (__scheduler->current) {
        if (__scheduler->current->status != WAITING_IO) {
            // don't append the yielded coroutine in case it is 
            // waiting for an IO to complete. the IO worker is responsible 
            // to append it when completed.
            __scheduler->current->status = RUNNABLE;
            __list_append(&__scheduler->coop_list, __scheduler->current);
        }

        __scheduler->current = NULL;
    }

    // pick next coroutine to run
    // should block if theres IO being queued/ processed and coop list is empty
    struct coroutine* selected = __pick_next_coop();
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
        __init_scheduler();
    }

    char is_main = !__scheduler->coop_list.head && !__scheduler->current;

    __list_append(&__scheduler->coop_list, new_coroutine);
    
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

void* __io_read_handler(struct io_rw_request* req) {
    ssize_t n = read(req->fd, req->buf, req->count);
    struct io_rw_response* res = (struct io_rw_response*)malloc(sizeof(struct io_rw_response));
    res->n = n;

    return res;
}

void* __io_write_handler(struct io_rw_request* req) {
    ssize_t n = write(req->fd, req->buf, req->count);
    struct io_rw_response* res = (struct io_rw_response*)malloc(sizeof(struct io_rw_response));
    res->n = n;

    return res;   
}

void* __worker_loop(void* arg) {
    for (;;) {
        // should be optimized obviously
        struct io_request* req = (struct io_request*)__list_pop(&__scheduler->io_queue);
        if (!req) {
            continue;
        }

        void* res;

        switch (req->type) {
            case IO_READ:
                res = __io_read_handler((struct io_rw_request*)req->args);
                break;
            case IO_WRITE:
                res = __io_write_handler((struct io_rw_request*)req->args);
                break;
            default:
                printf("invalid IO type");
                break;
        }

        req->coop->io_response = res;
        req->coop->status = RUNNABLE;
        __list_append(&__scheduler->coop_list, req->coop);
    }
}

void __submit_io(enum io_type type, void* args) {
    struct io_request* req = (struct io_request*)malloc(sizeof(struct io_request));
    req->type = type;
    req->coop = __scheduler->current;
    req->args = args;

    __scheduler->current->status = WAITING_IO;
    __list_append(&__scheduler->io_queue, req);
    yield();

    free(req);
}

ssize_t __coop_rw(enum io_type type, int fd, void *buf, size_t count) {
    struct io_rw_request* req = (struct io_rw_request*)malloc(sizeof(struct io_rw_request));
    
    req->fd = fd;
    req->buf = buf;
    req->count = count;

    __submit_io(type, req);

    // IO completed
    struct io_rw_response *res = (struct io_rw_response*)__scheduler->current->io_response;
    ssize_t n = res->n;

    free(req);
    free(res);
    __scheduler->current->io_response = NULL;

    return n;    
}

ssize_t coop_read(int fd, void *buf, size_t count) {
    return __coop_rw(IO_READ, fd, buf, count);
}

ssize_t coop_write(int fd, void *buf, size_t count) {
    return __coop_rw(IO_WRITE, fd, buf, count);
}

void coop_print(const char* str) {
    coop_write(STDOUT_FILENO, (void*)str, strlen(str));
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