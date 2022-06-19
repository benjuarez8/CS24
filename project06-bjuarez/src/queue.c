#include "queue.h"

#include <pthread.h>
#include <stdlib.h>

typedef struct node {
    void *value;
    struct node *next;
} node_t;

typedef struct queue {
    node_t *head;
    node_t *tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} queue_t;

queue_t *queue_init(void) {
    queue_t *new_q = malloc(sizeof(queue_t));
    if (new_q == NULL) {
        return NULL;
    }
    new_q->head = NULL;
    new_q->tail = NULL;
    pthread_mutex_init(&new_q->lock, NULL);
    pthread_cond_init(&new_q->cond, NULL);
    return new_q;
}

void queue_enqueue(queue_t *queue, void *value) {
    pthread_mutex_lock(&queue->lock);
    node_t *node = malloc(sizeof(node_t));
    node->value = value;
    node->next = NULL;
    if ((queue->head == NULL) && (queue->tail == NULL)) {
        queue->head = node;
    } else {
        queue->tail->next = node;
    }
    queue->tail = node;
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->lock);
}

void *queue_dequeue(queue_t *queue) {
    pthread_mutex_lock(&queue->lock);
    while ((queue->head == NULL) && (queue->tail == NULL)) {
        pthread_cond_wait(&queue->cond, &queue->lock);
    }
    node_t *head = queue->head;
    void *value = head->value;
    if (queue->tail == head) {
        queue->head = NULL;
        queue->tail = NULL;
    } else {
        queue->head = head->next;
    }
    free(head);
    pthread_mutex_unlock(&queue->lock);
    return value;
}

void queue_free(queue_t *queue) {
    free(queue);
}