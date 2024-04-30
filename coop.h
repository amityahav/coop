#include <setjmp.h>

#define STACK_SIZE 1024 * 1024 // 1 MB
#define INIT 0
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
    struct coroutine* next;
};

void coop(void (*func)(void*), void* args);
void yield();

struct coop_list {
    struct coroutine* head;
    struct coroutine* tail;
};

struct scheduler {
    struct coroutine* current;
    struct coop_list list;
    jmp_buf context;
};

