#include <setjmp.h>
#include "sys/types.h"
#include <fcntl.h>
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
ssize_t coop_write(int fd, void *buf, size_t count);
int coop_open(const char* path, int oflag, mode_t mode);
int coop_close(int fd);
void coop_print(const char* str);
void yield();

struct scheduler {
    jmp_buf context;
    struct coroutine* current;
    struct list coop_list;

    pthread_t worker_thread;
    struct blocking_queue io_queue;
};

enum io_type {
    IO_READ,
    IO_WRITE,
    IO_OPEN,
    IO_CLOSE,
};

struct io_rw_request {
    int fd;
    void *buf;
    size_t count;
};

struct io_rw_response {
    ssize_t n;
};

struct io_open_request {
    const char* path;
    int oflag;
    mode_t mode;
};

struct io_open_response {
    int fd;
};

struct io_close_request {
    int fd;
};

struct io_close_response {
    int r;
};

struct io_request {
    struct coroutine* coop;
    enum io_type type;
    void *args;
};
