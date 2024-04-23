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

void coop(void (*func)(void*), void* args);
void yield();

struct coop_list {
    coroutine* head;
    coroutine* tail;
};

struct scheduler {
    coroutine* current;
    coroutine* head;
    coroutine* tail; // ciruclar linked list
    jmp_buf context;
};

