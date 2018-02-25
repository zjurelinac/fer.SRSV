#include "util.h"
#include <malloc.h>
#include <pthread.h>
#include <time.h>

// thread management functions

void set_thread_attrs(pthread_attr_t *attr, int sched_policy, int sched_prio){
    struct sched_param params;
    params.sched_priority = sched_prio;

    pthread_attr_init(attr);
    pthread_attr_setinheritsched(attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(attr, sched_policy);
    pthread_attr_setschedparam(attr, &params);
}


// queue functions

int q_empty(queue_t *queue){
    return queue->first == NULL;
}

void q_add(queue_t *queue, void *item){
    item_t *new = (item_t *) malloc(sizeof(item_t));
    new->item = item;

    new->prev = NULL;
    new->next = queue->first;

    if(queue->first == NULL)
        queue->last = new;
    else
        queue->first->prev = new;

    queue->first = new;
}

void *q_pop(queue_t *queue){
    item_t *old;
    void *item;

    if(queue->last == NULL)
        return NULL;

    old = queue->last;
    item = old->item;
    queue->last = queue->last->prev;

    if(queue->last == NULL)
        queue->first = NULL;
    else
        queue->last->next = NULL;

    free(old);

    return item;
}


// timing functions

static long long iters_per_second = 385252000;

void init_timedwait(){
    struct timespec starttime, endtime;
    long long ms_diff = 0, j;

    do{
        iters_per_second *= 2;

        clock_gettime(CLOCK_MONOTONIC, &starttime);

        for(j = 0; j < iters_per_second; ++j)
            asm volatile ("":::"memory");

        clock_gettime(CLOCK_MONOTONIC, &endtime);

        ms_diff = (endtime.tv_sec - starttime.tv_sec) * 1000 +
                 ((endtime.tv_nsec - starttime.tv_nsec + 1000000000) % 1000000000) / 1000000 -
                  (endtime.tv_nsec < starttime.tv_nsec ? 1000 : 0);

    } while(ms_diff < 1000);

    iters_per_second = (iters_per_second / ms_diff) * 1000;
}

void do_timedwait(int seconds){
    int i, j;
    for(i = 0; i < seconds; ++i)
        for(j = 0; j < iters_per_second; ++j)
            asm volatile ("":::"memory");
}