#include <pthread.h>

struct node {
    void* data;
    struct node* next;
};

struct list {
    struct node* head;
    struct node* tail;
};

struct blocking_queue {
    struct list l;
    pthread_mutex_t mu;
    pthread_cond_t empty;
};

void __list_append(struct list* l, void* data);
void* __list_pop(struct list* l);

void __init_blocking_queue(struct blocking_queue* bq);
void __enqueue(struct blocking_queue* bq, void* data);
void* __dequeue(struct blocking_queue* bq);