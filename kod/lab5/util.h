#ifndef _UTIL_H_
#define _UTIL_H_

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>


// constants

#define TRUE 1
#define FALSE 0

#define MQ_MAX_MSGS 10

#define SHM_BLOCK_SIZE 128

// helper macros

#define LOG(LEVEL, FMT, ...)                                                \
    do{                                                                     \
        printf( "[" # LEVEL "] (%s:%d/%s) :: " FMT "\n", __FILE__, __LINE__, __FUNCTION__, ## __VA_ARGS__ ); \
        fflush(stdout);                                                     \
    } while(0)

#define FAIL(FMT, ...)                                                      \
    do{                                                                     \
        LOG(ERROR, FMT, ## __VA_ARGS__);                                    \
        exit(1);                                                            \
    } while(0)


// thread management functions

void set_thread_attrs(pthread_attr_t *attr, int sched_policy, int sched_prio);

// helper datastructures

typedef struct {
    int id;
    int duration;
    char shm_name[64];
} job_t;

typedef struct {
    pthread_mutex_t lock;
    int curr_job_id;
} generator_data_t;

// queue type

typedef struct __item_t__{
    struct __item_t__ *prev, *next;
    void *item;
} item_t;

typedef struct __queue_t__{
    item_t *first, *last;
} queue_t;

int q_empty(queue_t *queue);
void q_add(queue_t *queue, void *item);
void *q_pop(queue_t *queue);

// various functions

void init_timedwait();
void do_timedwait(int seconds);

#endif
