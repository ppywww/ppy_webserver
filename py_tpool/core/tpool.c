#include"tpool.h"


static void* thread_worker(void *thrd_pool);

thread_pool_t *thread_pool_creat(int thrd_count,int queue_size){
    thread_pool_t *pool;
    if(thrd_count<=0 || queue_size<=0){
        return NULL;
    }

    pool = (thread_pool_t*)malloc(sizeof(*pool));
    if(pool == NULL){
        return NULL;
    };

    pool->thrd_count =0;
    pool->queue_size = queue_size;
    pool->task_queue.head = 0;
    pool->task_queue.tail = 0;
    pool->task_queue.count = 0;
    pool->started = pool->closed = 0;

    pool->task_queue.queue = (task_t*)malloc(sizeof(task_t) * queue_size);
    if(pool->task_queue.queue){
        //free pool
        return NULL;
    }
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t)*thrd_count);
       if(pool->threads){
        //free pool
        return NULL;
    }
    for(int i = 0;i < thrd_count;i++){
        if(pthread_create(&(pool->threads[i]),NULL,thread_worker,(void*)pool)!=0){
            //
            return NULL;
        }
        pool->thrd_count++;
        pool->started++;
    }
    return pool;
};

 static void* thread_worker(void *thrd_pool){
    thread_pool_t *pool = (thread_pool_t*)thrd_pool;//
    task_queue_t *que;
    task_t task;
    while(1){ 
        pthread_mutex_lock(&(pool->mutex));
        que = &pool->task_queue;
        while(que->count == 0 && pool->closed == 0){
            //休眠
            pthread_cond_wait(&(pool->condition),&(pool->mutex));

        }
        if(pool->closed == 1)break;
        task = que->queue[que->head];
        que->head = (que->head+1)%pool->queue_size;
        que->count--;
        pthread_mutex_unlock(&(pool->mutex));
        task.func(task.arg);
    }
    pool->started--;
    pthread_mutex_unlock(&(pool->mutex));
    pthread_exit(NULL);
    return NULL;
 };


int thread_pool_post(thread_pool_t *pool,handler_pt func,void *arg){

};



int thread_pool_destroy(thread_pool_t *pool){ 

};

static void thread_pool_free(thread_pool_t *pool){

};

int wait_all_done(thread_pool_t *pool){

};
