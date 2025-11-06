#ifndef _THREAD_POOL_H_
#define _THREAD_POOL_H_ 

#include<pthread.h>
#include<stdint.h>
#include<stdlib.h>

typedef void (*handler_pt)(void*);

typedef struct task_t{
    handler_pt func;
    void*arg;
}task_t;

typedef struct task_queue_t{
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    task_t *queue;
}task_queue_t;

typedef struct thread_pool_t{
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    pthread_t *threads;
    task_queue_t task_queue;

    int closed;
    int started;

    int thrd_count;
    int queue_size;
}thread_pool_t;


thread_pool_t *thread_pool_creat(int thrd_count,int queue_size);

int thread_pool_post(thread_pool_t *pool,handler_pt func,void *arg);

int thread_pool_destroy(thread_pool_t *pool);

#endif