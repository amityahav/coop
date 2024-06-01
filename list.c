#include <stdio.h>
#include <stdlib.h>
#include "list.h"

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
