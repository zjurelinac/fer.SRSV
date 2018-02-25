#include "util.h"
#include <errno.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>


#define MAXTHREADS 64

int terminate_server = FALSE;

queue_t job_queue;
int total_queue_duration;
pthread_t dispatcher;
pthread_mutex_t job_queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t job_wait_cond = PTHREAD_COND_INITIALIZER;

int pid, N, M;
mqd_t mq;

void sigterm_dispatcher_handler(int signo){
    LOG(INFO, "Handling a SIGTERM");
    terminate_server = TRUE;
    return;
}

void *worker_thread(void *id){
    int worker_id = *((int*) id);
    job_t *job;
    int job_shm_fd, *job_data, i, sum;
    LOG(INFO, "<%d/Worker %d> started", pid, worker_id);

    while(TRUE){
        pthread_mutex_lock(&job_queue_lock);
        while(q_empty(&job_queue) && !terminate_server)
            pthread_cond_wait(&job_wait_cond, &job_queue_lock);

        if(q_empty(&job_queue) && terminate_server){
            pthread_mutex_unlock(&job_queue_lock);
            return NULL;
        }
        job = (job_t*) q_pop(&job_queue);
        LOG(INFO, "<%d/Worker %d> processing <Job %d> of duration %d", pid, worker_id, job->id, job->duration);

        total_queue_duration -= job->duration;
        pthread_mutex_unlock(&job_queue_lock);

        ////////////////////////////////////////////////////////////////////////

        job_shm_fd = shm_open(job->shm_name, O_RDWR, 0666);
        ftruncate(job_shm_fd, SHM_BLOCK_SIZE);

        job_data = (int*) mmap(NULL, SHM_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, job_shm_fd, 0);

        sum = 0;
        for(i = 0; i < job->duration; ++i)
            sum += job_data[i];

        munmap(job_data, SHM_BLOCK_SIZE);
        shm_unlink(job->shm_name);

        ////////////////////////////////////////////////////////////////////////

        do_timedwait(job->duration);
        LOG(INFO, "<%d/Worker %d> done with <Job %d> -> sum = %d", pid, worker_id, job->id, sum);

        free(job);
    }

    LOG(INFO, "<%d/Worker> %d terminated.", pid, worker_id);

    return NULL;
}

void *dispatcher_thread(void *none){
    job_t job, *tjob;
    struct timespec wait_time, ttime;
    sigset_t sigmask;
    struct sigaction action;

    action.sa_flags = 0;
    action.sa_handler = sigterm_dispatcher_handler;

    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGTERM);
    pthread_sigmask(SIG_UNBLOCK, &sigmask, NULL);

    sigaction(SIGTERM, &action, NULL);

    ////////////////////////////////////////////////////////////////////////////

    LOG(INFO, "<%d/Dispatcher> started", pid);

    while(!terminate_server){
        clock_gettime(CLOCK_REALTIME, &ttime);
        wait_time.tv_sec = ttime.tv_sec + 30;
        wait_time.tv_nsec = ttime.tv_nsec;

        if(mq_timedreceive(mq, (char*) &job, sizeof(job_t), NULL, &wait_time) == -1){
            if(errno == ETIMEDOUT){
                LOG(INFO, "No job received in 30 seconds");

                pthread_mutex_lock(&job_queue_lock);

                if(!q_empty(&job_queue)){
                    LOG(INFO, "<%d/Dispatcher> waking sleeping workers - periodic", pid);
                    pthread_cond_broadcast(&job_wait_cond);
                }

                pthread_mutex_unlock(&job_queue_lock);
            } else if(errno == EINTR){
                pthread_cond_broadcast(&job_wait_cond);
            } else
                LOG(ERROR, "Unexpected error occured :: %d -> %s", errno, strerror(errno));
        } else {
            LOG(INFO, "<%d/Dispatcher> received <Job %d> of duration %d (data at %s)", pid, job.id, job.duration, job.shm_name);

            tjob = (job_t*) malloc(sizeof(job_t));
            memcpy(tjob, &job, sizeof(job_t));

            pthread_mutex_lock(&job_queue_lock);
            total_queue_duration += job.duration;
            q_add(&job_queue, (void*) tjob);
            pthread_mutex_unlock(&job_queue_lock);

            LOG(INFO, "Current total jobs duration = %d", total_queue_duration);

            if(total_queue_duration > M){
                LOG(INFO, "<%d/Dispatcher> waking sleeping workers - incoming jobs", pid);
                pthread_cond_broadcast(&job_wait_cond);
            }
        }
    }

    pthread_cond_broadcast(&job_wait_cond);
    LOG(INFO, "<%d/Dispatcher> terminated.", pid);

    return NULL;
}


int main(int argc, char* argv[]){
    pthread_t workers[MAXTHREADS];
    pthread_attr_t workers_attr, dispatcher_attr;
    int i, ids[MAXTHREADS];
    char mq_name[32], *base_resource_path;
    struct mq_attr attr;
    sigset_t sigmask;

    if(argc != 3){
        puts("ERROR :: expected posluzitelj program call:  `posluzitelj N M`");
        exit(1);
    }

    ////////////////////////////////////////////////////////////////////////////

    sscanf(argv[1], "%d", &N);
    sscanf(argv[2], "%d", &M);

    base_resource_path = getenv("SRSV_LAB5");

    job_queue.first = job_queue.last = NULL;
    total_queue_duration = 0;

    pid = getpid();

    init_timedwait();

    ////////////////////////////////////////////////////////////////////////////

    sigfillset(&sigmask);
    pthread_sigmask(SIG_BLOCK, &sigmask, NULL);

    ////////////////////////////////////////////////////////////////////////////

    sprintf(mq_name, "/%s", base_resource_path);

    attr.mq_flags = 0;
    attr.mq_maxmsg = MQ_MAX_MSGS;
    attr.mq_msgsize = sizeof(job_t);
    attr.mq_curmsgs = 0;

    mq = mq_open(mq_name, O_CREAT | O_RDONLY, 0644, &attr);
    if(mq == -1)
        FAIL("Cannot open mq, %d -> %s", errno, strerror(errno));

    ////////////////////////////////////////////////////////////////////////////

    set_thread_attrs(&workers_attr, SCHED_RR, 40);
    set_thread_attrs(&dispatcher_attr, SCHED_RR, 60);

    for(i = 0; i < N; ++i){
        ids[i] = i;
        pthread_create(&workers[i], &workers_attr, worker_thread, (void*) &ids[i]);
    }

    pthread_create(&dispatcher, &dispatcher_attr, dispatcher_thread, NULL);

    ////////////////////////////////////////////////////////////////////////////

    pthread_join(dispatcher, NULL);

    for(i = 0; i < N; ++i)
        pthread_join(workers[i], NULL);

    ////////////////////////////////////////////////////////////////////////////

    mq_close(mq);

    return 0;
}