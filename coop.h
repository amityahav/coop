#include <setjmp.h>

#define STACK_SIZE 2 * 1024
#define EXIT 1
#define SCHED 2

enum status {
    CREATED,
    RUNNABLE,
    RUNNING,
};

struct coroutine {
    int id;
    enum status status;
    jmp_buf context;
    void (*func)(void*);
    void* args;
    void* stack_top;
    void* stack_bottom;
    coroutine* next;
};

void co_create(void (*func)(void*), void* args);
void yield();

struct scheduler {
    coroutine *current;
    coroutine *tail;
    jmp_buf context;
};

