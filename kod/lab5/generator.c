#include "util.h"
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <mqueue.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>


static generator_data_t *generator_data;
static char *base_resource_path;
mqd_t mq;
pid_t pid;


void create_job(int id){
    job_t *job = (job_t*) malloc(sizeof(job_t));
    int job_shm_fd, *job_data, i, sum = 0;

    job->id = id;
    job->duration = rand() % 10 + 1;
    sprintf(job->shm_name, "/%s-%d-%d", base_resource_path, pid, id);

    ////////////////////////////////////////////////////////////////////////////

    job_shm_fd = shm_open(job->shm_name, O_CREAT | O_RDWR, 0666);
    ftruncate(job_shm_fd, SHM_BLOCK_SIZE);

    job_data = (int*) mmap(NULL, SHM_BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, job_shm_fd, 0);

    for(i = 0; i < job->duration; ++i){
        job_data[i] = rand() % 100;
        sum += job_data[i];
    }

    munmap(job_data, SHM_BLOCK_SIZE);
    close(job_shm_fd);

    ////////////////////////////////////////////////////////////////////////////

    LOG(INFO, "Creating <Job %d> of duration %d (data at %s) -> sum = %d", id, job->duration, job->shm_name, sum);

    if(mq_send(mq, (const char*) job, sizeof(job_t), 0) != 0)
        FAIL("Cannot send a message :: %d -> %s", errno, strerror(errno));
}

int main(int argc, char* argv[]){
    int J, base_index, i, shm_fd,
        is_created = FALSE;
    char generator_data_shm_name[32], mq_name[32];
    struct mq_attr attr;
    struct sched_param param = {50};

    if(argc != 2){
        puts("ERROR :: expected generator program call:  `generator J`");
        exit(1);
    }

    ////////////////////////////////////////////////////////////////////////////

    pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    pid = getpid();
    sscanf(argv[1], "%d", &J);
    base_resource_path = getenv("SRSV_LAB5");

    srand(time(NULL) + J);

    ////////////////////////////////////////////////////////////////////////////

    sprintf(mq_name, "/%s", base_resource_path);

    attr.mq_flags = 0;
    attr.mq_maxmsg = MQ_MAX_MSGS;
    attr.mq_msgsize = sizeof(job_t);
    attr.mq_curmsgs = 0;

    mq = mq_open(mq_name, O_CREAT | O_WRONLY, 0644, &attr);
    if(mq == -1)
        FAIL("Cannot open mq");

    ////////////////////////////////////////////////////////////////////////////

    sprintf(generator_data_shm_name, "/%s", base_resource_path);

    shm_fd = shm_open(generator_data_shm_name, O_RDWR, 0666);
    if(shm_fd == -1 && errno == ENOENT){
        shm_fd = shm_open(generator_data_shm_name, O_CREAT | O_RDWR, 0666);
        is_created = TRUE;
    }

    ftruncate(shm_fd, sizeof(generator_data_t));
    generator_data = (generator_data_t*) mmap(NULL, sizeof(generator_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if(is_created){
        pthread_mutex_init(&generator_data->lock, NULL);
        generator_data->curr_job_id = 0;
    }

    ////////////////////////////////////////////////////////////////////////////

    LOG(INFO, "<Process %d> set up.", pid);

    pthread_mutex_lock(&generator_data->lock);

    base_index = generator_data->curr_job_id;
    generator_data->curr_job_id += J;
    LOG(INFO, "<Process %d> will generate %d jobs with ids <%d, %d>", pid, J, base_index, base_index + J - 1);

    pthread_mutex_unlock(&generator_data->lock);

    ////////////////////////////////////////////////////////////////////////////

    munmap(generator_data, sizeof(generator_data_t));
    close(shm_fd);

    ////////////////////////////////////////////////////////////////////////////

    for(i = 0; i < J; ++i){
        create_job(base_index++);
        sleep(1);
    }

    ////////////////////////////////////////////////////////////////////////////

    mq_close(mq);

    return 0;
}