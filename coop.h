#include <setjmp.h>
#include "sys/types.h"
#include "list.h"

#define STACK_SIZE 2 * 1024
#define INIT 0
#define EXIT 1
#define SCHED 2

enum status {
    CREATED,
    RUNNABLE,
    RUNNING,
    WAITING_IO,
};

struct coroutine {
    int id;
    enum status status;
    jmp_buf context;
    void (*func)(void*);
    void* args;
    void* stack_top;
    void* stack_bottom;
    void* io_response;
};

void coop(void (*func)(void*), void* args);
ssize_t coop_read(int fd, void *buf, size_t count);
void yield();

struct scheduler {
    jmp_buf context;
    struct coroutine* current;
    struct list coop_list;

    pthread_t worker_thread;
    struct list io_queue;
};

enum io_type {
    IO_READ,
    IO_WRITE,
};

struct io_read_request {
    int fd;
    void *buf;
    size_t count;
};

struct io_read_response {
    ssize_t n;
};

struct io_request {
    struct coroutine* coop;
    io_type type;
    void *args;
};
