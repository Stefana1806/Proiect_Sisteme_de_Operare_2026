#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "ipc.h"
#include <signal.h>
#include <errno.h>

volatile __sig_atomic_t g_worker = 0;

void worker_signal_handler(int sig)
{
    if (sig == SIGTERM)
        g_worker = 1;
}

int main(int argc, char *argv[])
{
    struct sigaction sa;
    sa.sa_handler = worker_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, NULL);

    int id = 0;
    char ipc_path[256] = "";
    int simulate_ms = 0;
    int control_fd = 1;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--id") == 0)
            id = atoi(argv[++i]);
        else if (strcmp(argv[i], "--ipc") == 0)
            strcpy(ipc_path, argv[++i]);
        else if (strcmp(argv[i], "--simulate-work-ms") == 0)
            simulate_ms = atoi(argv[++i]);
        else if (strcmp(argv[i], "--control-fd") == 0)
            control_fd = atoi(argv[++i]);
    }

    int fd = open(ipc_path, O_RDWR);
    if (fd < 0)
        return 1;
    struct sharedmemoryregion *shm = mmap(NULL, sizeof(struct sharedmemoryregion), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    while (1)
    {
        if (g_worker == 1 || shm->info.shutdown_flag == 1)
        {
            char mesaj[128];
            int l = snprintf(mesaj, sizeof(mesaj), "T5MSG type=WORKER_EXITING worker_id=%d reason=shutdown\n", id);
            if (l > 0 && l < (int)sizeof(mesaj))
            {
                write(control_fd, mesaj, l);
            }
            break;
        }

        if (sem_trywait(&shm->job_queue.full) != 0)
        {
            sem_wait(&shm->info.counter_mutex);
            if (shm->info.active_jobs == 0)
            {
                sem_post(&shm->info.counter_mutex);
                break;
            }
            sem_post(&shm->info.counter_mutex);
            usleep(1000);
            continue;
        }

        sem_wait(&shm->job_queue.mutex);
        char current_path[256];
        strcpy(current_path, shm->job_queue.files_buffer[shm->job_queue.tail].path);
        shm->job_queue.tail = (shm->job_queue.tail + 1) % MAX_SIZE;
        sem_post(&shm->job_queue.mutex);
        sem_post(&shm->job_queue.empty);

        DIR *dir = opendir(current_path);
        if (dir == NULL)
        {
            char mesaj[128];
            int lungime = snprintf(mesaj, sizeof(mesaj), "T5MSG type=ERROR worker_id=%d errno=%d where=opendir\n", id, errno);
            if (lungime > 0 && lungime < (int)sizeof(mesaj))
                write(control_fd, mesaj, lungime);

            sem_wait(&shm->info.counter_mutex);
            shm->info.active_jobs--;
            sem_post(&shm->info.counter_mutex);
            continue;
        }

        struct dirent *e;
        while ((e = readdir(dir)))
        {
            if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
                continue;

            char full[512];
            snprintf(full, sizeof(full), "%s/%s", current_path, e->d_name);
            struct stat st;
            if (lstat(full, &st) == -1)
            {
                char mesaj[128];
                int lungime = snprintf(mesaj, sizeof(mesaj), "T5MSG type=ERROR worker_id=%d errno=%d where=lstat\n", id, errno);
                if (lungime > 0 && lungime < (int)sizeof(mesaj))
                    write(control_fd, mesaj, lungime);
                continue;
            }

            if (S_ISDIR(st.st_mode))
            {
                sem_wait(&shm->job_queue.empty);
                sem_wait(&shm->job_queue.mutex);
                strcpy(shm->job_queue.files_buffer[shm->job_queue.head].path, full);
                shm->job_queue.head = (shm->job_queue.head + 1) % MAX_SIZE;
                sem_wait(&shm->info.counter_mutex);
                shm->info.active_jobs++;
                sem_post(&shm->info.counter_mutex);
                sem_post(&shm->job_queue.mutex);
                sem_post(&shm->job_queue.full);
            }
            else if (S_ISREG(st.st_mode))
            {
                if (simulate_ms > 0)
                    usleep(simulate_ms * 1000);

                struct file_records r = {0};
                strncpy(r.path, full, 255);
                r.size = st.st_size;
                r.mtime = st.st_mtime;

                sem_wait(&shm->rezultate[id].empty);
                sem_wait(&shm->rezultate[id].mutex);
                shm->rezultate[id].files_buffer[shm->rezultate[id].head] = r;
                shm->rezultate[id].head = (shm->rezultate[id].head + 1) % MAX_SIZE;
                sem_post(&shm->rezultate[id].mutex);
                sem_post(&shm->rezultate[id].full);
            }
        }
        closedir(dir);

        shm->stats[id].jobs_processed++;

        char mesaj[256];
        int lungime = snprintf(mesaj, sizeof(mesaj), "T5MSG type=JOB_DONE worker_id=%d jobs=%d files=0 bytes=0\n",
                               id, shm->stats[id].jobs_processed);
        if (lungime > 0 && lungime < (int)sizeof(mesaj))
            write(control_fd, mesaj, lungime);

        sem_wait(&shm->info.counter_mutex);
        shm->info.active_jobs--;
        sem_post(&shm->info.counter_mutex);
    }

    char mesaj_final[128];
    int l = snprintf(mesaj_final, sizeof(mesaj_final), "T5MSG type=WORKER_EXITING worker_id=%d reason=normal_exit\n", id);
    if (l > 0 && l < (int)sizeof(mesaj_final))
    {
        write(control_fd, mesaj_final, l);
    }
    close(control_fd);

    return 0;
}