#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "coop.h"

// --- utils ----

void* __list_pop(struct list* l) {
    struct node* curr = l->head;
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
    void* data = curr->data;
    free(curr);

    return data;
}

void __list_append(struct list* l, void* data) {
    struct node* new_node = (struct node*)malloc(sizeof(struct node));
    new_node->data = data;

    if (!l->tail) {
        // list is empty
        l->head = l->tail = new_node;
    } else {
        l->tail->next = new_node;
        l->tail = new_node;
    }
}

void __init_blocking_queue(struct blocking_queue* bq) {
    pthread_mutex_init(&bq->mu, NULL);
    pthread_cond_init(&bq->empty, NULL);
}

void __enqueue(struct blocking_queue* bq, void* data) {
    pthread_mutex_lock(&bq->mu);

    __list_append(&bq->l, data);
    pthread_cond_signal(&bq->empty);

    pthread_mutex_unlock(&bq->mu);
}

void* __dequeue(struct blocking_queue* bq) {
    pthread_mutex_lock(&bq->mu);

    if (!bq->l.head) {
        pthread_cond_wait(&bq->empty, &bq->mu);
    }

   void* res =  __list_pop(&bq->l);

    pthread_mutex_unlock(&bq->mu);

    return res;
}

// ---- utils ----

struct scheduler* __scheduler;

void* __worker_loop(void* arg);

void __init_scheduler() {
    __scheduler = (struct scheduler*)malloc(sizeof(struct scheduler));
}

void __init_io_worker() {
    if (!__scheduler) {
        return;
    }

    __init_blocking_queue(&__scheduler->io_queue);
    pthread_create(&__scheduler->worker_thread, NULL, __worker_loop, NULL);
}

void __exit_current_coop() {
    // wrapping this call inside a function solves 
    // a weird segfault that i couldnt figure its cause
	longjmp(__scheduler->context, EXIT);
}

struct coroutine* __pick_next_coop() {
    for (;;) {
        struct coroutine* next = (struct coroutine*)__list_pop(&__scheduler->coop_list);
        if (!next) {
            return NULL;
        }

        if (next->status == RUNNABLE || next->status == CREATED) {
            return next;
        }

        __list_append(&__scheduler->coop_list, next);
    }
}

void __schedule() {
    if (__scheduler->current) {
        if (__scheduler->current->status == RUNNING) {
            __scheduler->current->status = RUNNABLE;
        }

        __list_append(&__scheduler->coop_list, __scheduler->current);
        __scheduler->current = NULL;
    }

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

// ---- IO Handlers ----

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

void* __io_open_handler(struct io_open_request* req) {
    int fd = open(req->path, req->oflag, req->mode);

    struct io_open_response* res = (struct io_open_response*)malloc(sizeof(struct io_open_response));
    res->fd = fd;

    return res;
}

void* __io_close_handler(struct io_close_request* req) {
    int r = close(req->fd);

    struct io_close_response* res = (struct io_close_response*)malloc(sizeof(struct io_close_response));
    res->r = r;

    return res;
}

// ---- IO Handers ----

void* __worker_loop(void* arg) {
    for (;;) {
        struct io_request* req = (struct io_request*)__dequeue(&__scheduler->io_queue);

        void* res;

        switch (req->type) {
            case IO_READ:
                res = __io_read_handler((struct io_rw_request*)req->args);
                break;
            case IO_WRITE:
                res = __io_write_handler((struct io_rw_request*)req->args);
                break;
            case IO_OPEN:
                res = __io_open_handler((struct io_open_request*)req->args);
                break;
            case IO_CLOSE:
                res = __io_close_handler((struct io_close_request*)req->args);
                break;
            default:
                printf("invalid IO type");
                break;
        }

        req->coop->io_response = res;
        req->coop->status = RUNNABLE;
    }
}

void __submit_io(enum io_type type, void* args) {
    static char once = 0;
    if (!once) {
        once = 1;
        __init_io_worker();
    }

    struct io_request* req = (struct io_request*)malloc(sizeof(struct io_request));
    req->type = type;
    req->coop = __scheduler->current;
    req->args = args;

    __scheduler->current->status = WAITING_IO;
    __enqueue(&__scheduler->io_queue, req);

    yield();

    free(req);
}

ssize_t __coop_rw(enum io_type type, int fd, void *buf, size_t count) {
    struct io_rw_request* req = (struct io_rw_request*)malloc(sizeof(struct io_rw_request));
    
    req->fd = fd;
    req->buf = buf;
    req->count = count;

    __submit_io(type, req);

    struct io_rw_response *res = (struct io_rw_response*)__scheduler->current->io_response;
    ssize_t n = res->n;

    free(req);
    free(res);
    __scheduler->current->io_response = NULL;

    return n;    
}

// ---- IO API ----

ssize_t coop_read(int fd, void *buf, size_t count) {
    return __coop_rw(IO_READ, fd, buf, count);
}

ssize_t coop_write(int fd, void *buf, size_t count) {
    return __coop_rw(IO_WRITE, fd, buf, count);
}

void coop_print(const char* str) {
    coop_write(STDOUT_FILENO, (void*)str, strlen(str));
}

int coop_open(const char* path, int oflag, mode_t mode) {
    struct io_open_request* req = (struct io_open_request*)malloc(sizeof(struct io_open_request));

    req->path = path;
    req->oflag = oflag;
    req->mode = mode;

    __submit_io(IO_OPEN, req);

    struct io_open_response* res = (struct io_open_response*)__scheduler->current->io_response;
    int fd = res->fd;

    free(req);
    free(res);
    __scheduler->current->io_response = NULL;
    
    return fd;
}

int coop_close(int fd) {
    struct io_close_request* req = (struct io_close_request*)malloc(sizeof(struct io_close_request));

    req->fd = fd;

    __submit_io(IO_CLOSE, req);

    struct io_close_response* res = (struct io_close_response*)__scheduler->current->io_response;
    int r = res->r;

    free(req);
    free(res);
    __scheduler->current->io_response = NULL;

    return r;
}

// ---- IO API ----

void coop3(void *args) {
    int fd = coop_open("example.txt", O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    coop_print("coop3: reading from file\n");

    char res[5];
    coop_read(fd, res, 5);
    coop_close(fd);

    coop_print(res);
}

void coop2(void *args) {
    coop(coop3, NULL);

    int fd = coop_open("example.txt", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    coop_print("coop2: writing to file\n");

    const char* buf = "Hello";
    coop_write(fd, buf, 5);

    coop_close(fd);
}

void coop1(void* args) {
    coop(coop2, NULL);

    for (int i = 0; i < 3; i++) {
        coop_print("coop1: Hey\n");
    }
}

int main(int argc, char**argv) {
    coop(coop1, NULL);
}