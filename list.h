#include <pthread.h>

struct node {
    void* data;
    struct node* next;
};

struct list {
    pthread_mutex_t mu;
    struct node* head;
    struct node* tail;
};

void __list_append(struct list* l, void* data);
void* __list_pop(struct list* l);