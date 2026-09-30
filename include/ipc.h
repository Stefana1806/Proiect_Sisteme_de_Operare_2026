#ifndef IPC_H
#define IPC_H

#include <sys/types.h>
#include <time.h>
#include <semaphore.h>

#define MAX_SIZE 66000       
#define MAX_WORKER_JOB 32

struct header {
    char magic[4];
    int format_version;
    int complete_flag;
    int file_record_count;
    int worker_count;
    int active_jobs;
    sem_t counter_mutex;
    int shutdown_flag;
};

struct file_records {
    char path[256];
    long size;
    long mtime;
    int uid;
    int gid;
    unsigned int sha256_sum;
};

struct worker_stats {
    int worker_id;
    int pid;
    int jobs_processed;
};

struct file_buff {
    struct file_records files_buffer[MAX_SIZE];
    int head;
    int tail;
    sem_t empty;
    sem_t full;
    sem_t mutex;
};

struct args {
    char dir_root[256];
    int workers;
    char ipc_path[256];
    char db_path[256];
    int simulate_work_ms;
    int graceful_timeout;
    char pid_file[256];
};

struct sharedmemoryregion {
    struct header info;
    struct file_buff job_queue;
    struct worker_stats stats[MAX_WORKER_JOB];
    struct file_buff rezultate[MAX_WORKER_JOB];

};

#endif