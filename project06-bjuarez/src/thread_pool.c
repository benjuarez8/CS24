#include "thread_pool.h"
#include "queue.h"

#include <pthread.h>
#include <stdlib.h>

typedef struct thread_pool {
    pthread_t *p;
    queue_t *queue;
    size_t size;
} thread_pool_t;

typedef struct {
    work_function_t function;
    void *aux;
} work_t;

void *mythread(void *arg) {
    queue_t *queue = (queue_t *) arg;
    while (true) {
        void *task = queue_dequeue(queue);
        if (task == NULL) {
            return NULL;
        }
        work_t *work = (work_t *) task;
        void *aux = work->aux;
        (work->function)(aux);
        free(task);
    }
}

thread_pool_t *thread_pool_init(size_t num_worker_threads) {
    thread_pool_t *thread_pool = malloc(sizeof(thread_pool_t));
    pthread_t *p = malloc(sizeof(pthread_t) * num_worker_threads);
    thread_pool->p = p;
    thread_pool->queue = queue_init();
    thread_pool->size = num_worker_threads;
    for (size_t i = 0; i < thread_pool->size; i++) {
        pthread_create(&p[i], NULL, mythread, thread_pool->queue);
    }
    return thread_pool;
}

void thread_pool_add_work(thread_pool_t *pool, work_function_t function, void *aux) {
    work_t *work = malloc(sizeof(work_t));
    work->function = function;
    work->aux = aux;
    queue_t *queue = pool->queue;
    void *value = (void *) work;
    queue_enqueue(queue, value);
}

void thread_pool_finish(thread_pool_t *pool) {
    for (size_t i = 0; i < pool->size; i++) {
        queue_enqueue(pool->queue, NULL);
    }
    for (size_t i = 0; i < pool->size; i++) {
        pthread_join(pool->p[i], NULL);
    }
    queue_free(pool->queue);
    free(pool->p);
    free(pool);
}