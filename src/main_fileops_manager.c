#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/poll.h>
#include "ipc.h"

void do_dump(const char *db_path)
{
    int fd = open(db_path, O_RDONLY);
    if (fd < 0)
    {
        perror("Eroare deschidere DB pentru dump");
        exit(6);
    }
    struct header h;
    if (read(fd, &h, sizeof(h)) != sizeof(h))
    {
        close(fd);
        return;
    }
    printf("--- DATABASE DUMP (%s) ---\n", h.magic);
    printf("Files: %d | Workers: %d | Complete: %d\n", h.file_record_count, h.worker_count, h.complete_flag);
    struct file_records r;
    for (int i = 0; i < h.file_record_count; i++)
    {
        if (read(fd, &r, sizeof(r)) != sizeof(r))
        {
            break;
        }
        printf("[%d] %s (%ld bytes)\n", i + 1, r.path, (long)r.size);
    }
    close(fd);
}

void do_verify(const char *db_path)
{
    int fd = open(db_path, O_RDONLY);
    if (fd < 0)
    {
        perror("Eroare la deschiderea fisierului pentru verify!\n");
        exit(7);
    }
    struct header h;
    if (read(fd, &h, sizeof(h)) != sizeof(h))
    {
        printf("[CORRUPTED] Fisierul este prea mic pentru a contine un header valid.\n");
        close(fd);
        exit(8);
    }
    if (strncmp(h.magic, "INV4", 4) != 0)
    {
        printf("[CORRUPTED] Semnatura magica a bazei de date este invalida.\n");
        close(fd);
        exit(9);
    }
    struct file_records r;
    for (int i = 0; i < h.file_record_count; i++)
    {
        if (read(fd, &r, sizeof(r)) != sizeof(r))
        {
            close(fd);
            return;
        }
        struct stat st;
        if (lstat(r.path, &st) == -1)
        {
            printf("[MISSING] %s\n", r.path);
        }
        else if ((long)st.st_mtime != r.mtime)
        {
            printf("[CHANGED] %s\n", r.path);
        }
    }
    close(fd);
}

volatile __sig_atomic_t g_status = 0;
volatile __sig_atomic_t g_shutdown = 0;

void manager_signal_handler(int sig)
{
    if (sig == SIGUSR1)
        g_status = 1;
    if (sig == SIGINT || sig == SIGTERM)
        g_shutdown = 1;
}

int worker_pipes[MAX_WORKER_JOB];

int main(int argc, char *argv[])
{
    struct args arg = {.workers = 1, .simulate_work_ms = 0, .graceful_timeout = 5};
    strcpy(arg.db_path, "data/inventory.db");
    strcpy(arg.ipc_path, "data/ipc.mmap");
    strcpy(arg.dir_root, ".");
    strcpy(arg.pid_file, "");
    int mode = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--root") == 0)
            strcpy(arg.dir_root, argv[++i]);
        else if (strcmp(argv[i], "--workers") == 0)
            arg.workers = atoi(argv[++i]);
        else if (strcmp(argv[i], "--db") == 0)
            strcpy(arg.db_path, argv[++i]);
        else if (strcmp(argv[i], "--ipc") == 0)
            strcpy(arg.ipc_path, argv[++i]);
        else if (strcmp(argv[i], "--dump") == 0)
            mode = 1;
        else if (strcmp(argv[i], "--verify") == 0)
            mode = 2;
        else if (strcmp(argv[i], "--simulate-work-ms") == 0)
            arg.simulate_work_ms = atoi(argv[++i]);
        else if (strcmp(argv[i], "--graceful-timeout") == 0)
            arg.graceful_timeout = atoi(argv[++i]);
        else if (strcmp(argv[i], "--pid-file") == 0)
            strcpy(arg.pid_file, argv[++i]);
    }

    if (mode == 1)
    {
        do_dump(arg.db_path);
        return 0;
    }
    if (mode == 2)
    {
        do_verify(arg.db_path);
        return 0;
    }

    int has_root = 0;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--root") == 0)
        {
            has_root = 1;
        }
    }

    if (!has_root)
    {
        fprintf(stderr, "EROARE: Argumentul --root este obligatoriu în modul inventariere!\n");
        exit(1);
    }
    if (strncmp(arg.dir_root, "--", 2) == 0)
    {
        fprintf(stderr, "EROARE: Valoarea pentru --root nu poate fi o altă cheie (%s). Ordine greșită a argumentelor!\n", arg.dir_root);
        exit(2);
    }

    if (arg.workers < 1)
    {
        fprintf(stderr, "EROARE: Numărul de workeri trebuie să fie cel puțin 1!\n");
        exit(3);
    }

    struct stat dir_st;
    if (stat(arg.dir_root, &dir_st) != 0 || !S_ISDIR(dir_st.st_mode))
    {
        fprintf(stderr, "EROARE: Directorul '%s' nu există sau nu este valid!\n", arg.dir_root);
        exit(4);
    }
    if (access(arg.dir_root, R_OK | X_OK) != 0)
    {
        fprintf(stderr, "EROARE: Permisiuni insuficiente pentru directorul '%s'!\n", arg.dir_root);
        exit(5);
    }

    mkdir("data", 0777);

    if (strlen(arg.pid_file) > 0 && strcmp(arg.pid_file, "NULL") != 0)
    {
        FILE *f = fopen(arg.pid_file, "w");
        if (f != NULL)
        {
            fprintf(f, "%d\n", getpid());
            fflush(f);
            fclose(f);
        }
    }

    struct sigaction sa;
    sa.sa_handler = manager_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int fd = open(arg.ipc_path, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd < 0)
        return 1;
    ftruncate(fd, sizeof(struct sharedmemoryregion));
    struct sharedmemoryregion *shm = mmap(NULL, sizeof(struct sharedmemoryregion), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    memset(shm, 0, sizeof(struct sharedmemoryregion));
    strcpy(shm->info.magic, "INV4");
    shm->info.worker_count = arg.workers;
    shm->info.active_jobs = 1;

    sem_init(&shm->info.counter_mutex, 1, 1);
    sem_init(&shm->job_queue.mutex, 1, 1);
    sem_init(&shm->job_queue.empty, 1, MAX_SIZE);
    sem_init(&shm->job_queue.full, 1, 0);

    for (int i = 0; i < arg.workers; i++)
    {
        sem_init(&shm->rezultate[i].mutex, 1, 1);
        sem_init(&shm->rezultate[i].empty, 1, MAX_SIZE);
        sem_init(&shm->rezultate[i].full, 1, 0);
    }

    strcpy(shm->job_queue.files_buffer[0].path, arg.dir_root);
    shm->job_queue.head = 1;
    sem_post(&shm->job_queue.full);

    pid_t pids[MAX_WORKER_JOB];
    for (int i = 0; i < arg.workers; i++)
    {
        int pids_fds[2];
        if (pipe(pids_fds) < 0)
            exit(1);

        pids[i] = fork();
        if (pids[i] == 0)
        {
            close(pids_fds[0]);
            char buff_fd[20], id_s[10], sim_s[10];
            snprintf(buff_fd, sizeof(buff_fd), "%d", pids_fds[1]);
            sprintf(id_s, "%d", i);
            sprintf(sim_s, "%d", arg.simulate_work_ms);

            execl("bin/fileops_worker", "fileops_worker",
                  "--id", id_s,
                  "--ipc", arg.ipc_path,
                  "--simulate-work-ms", sim_s,
                  "--control-fd", buff_fd,
                  NULL);
            exit(1);
        }
        else
        {
            close(pids_fds[1]);
            worker_pipes[i] = pids_fds[0];
        }
    }

    int db_fd = open(arg.db_path, O_CREAT | O_RDWR | O_TRUNC, 0666);
    lseek(db_fd, sizeof(struct header), SEEK_SET);

    struct pollfd fds[MAX_WORKER_JOB];
    for (int i = 0; i < arg.workers; i++)
    {
        fds[i].fd = worker_pipes[i];
        fds[i].events = POLLIN;
    }

    int active_procs = arg.workers;
    while (active_procs > 0)
    {
        int found_anything = 0;
        if (g_status == 1)
        {
            g_status = 0;
            int queued_jobs = 0;
            sem_wait(&shm->job_queue.mutex);
            if (shm->job_queue.head >= shm->job_queue.tail)
                queued_jobs = shm->job_queue.head - shm->job_queue.tail;
            else
                queued_jobs = (MAX_SIZE - shm->job_queue.tail) + shm->job_queue.head;
            sem_post(&shm->job_queue.mutex);

            printf("STATUS queued_jobs=%d active_jobs=%d files=%d\n",
                   queued_jobs, shm->info.active_jobs, shm->info.file_record_count);
            fflush(stdout);
        }

        if (g_shutdown == 1)
            break;

        int ret = poll(fds, arg.workers, 10);
        if (ret > 0)
        {
            for (int i = 0; i < arg.workers; i++)
            {
                if (fds[i].fd != -1 && (fds[i].revents & POLLIN))
                {
                    char buffer[256] = {0};
                    int bytes_read = read(fds[i].fd, buffer, sizeof(buffer) - 1);
                    if (bytes_read > 0)
                    {
                        printf("%s", buffer);
                        fflush(stdout);
                    }
                }
            }
        }

        for (int i = 0; i < arg.workers; i++)
        {
            while (sem_trywait(&shm->rezultate[i].full) == 0)
            {
                found_anything = 1;
                sem_wait(&shm->rezultate[i].mutex);
                struct file_records rec = shm->rezultate[i].files_buffer[shm->rezultate[i].tail];
                shm->rezultate[i].tail = (shm->rezultate[i].tail + 1) % MAX_SIZE;
                sem_post(&shm->rezultate[i].mutex);
                sem_post(&shm->rezultate[i].empty);

                write(db_fd, &rec, sizeof(struct file_records));
                shm->info.file_record_count++;
            }
        }

        for (int i = 0; i < arg.workers; i++)
        {
            if (pids[i] != -1)
            {
                int status;
                if (waitpid(pids[i], &status, WNOHANG) > 0)
                {
                    pids[i] = -1;
                    active_procs--;
                    found_anything = 1;
                }
            }
        }
        if (!found_anything)
            usleep(1000);
    }

    if (g_shutdown == 1 || active_procs > 0)
    {
        shm->info.shutdown_flag = 1;
        for (int i = 0; i < arg.workers; i++)
            if (pids[i] != -1)
                kill(pids[i], SIGTERM);

        int elapsed_ms = 0;
        int timeout_ms = arg.graceful_timeout * 1000;
        while (active_procs > 0 && elapsed_ms < timeout_ms)
        {
            int ret = poll(fds, arg.workers, 10);
            if (ret > 0)
            {
                for (int i = 0; i < arg.workers; i++)
                {
                    if (fds[i].fd != -1 && (fds[i].revents & POLLIN))
                    {
                        char buffer[256] = {0};
                        int bytes_read = read(fds[i].fd, buffer, sizeof(buffer) - 1);
                        if (bytes_read > 0)
                        {
                            printf("%s", buffer);
                            fflush(stdout);
                        }
                    }
                }
            }

            for (int i = 0; i < arg.workers; i++)
            {
                if (pids[i] != -1)
                {
                    int status;
                    if (waitpid(pids[i], &status, WNOHANG) > 0)
                    {
                        pids[i] = -1;
                        active_procs--;
                    }
                }
            }
            usleep(10000);
            elapsed_ms += 10;
        }

        if (active_procs > 0)
        {
            for (int i = 0; i < arg.workers; i++)
            {
                if (pids[i] != -1)
                {
                    kill(pids[i], SIGKILL);
                    int status;
                    waitpid(pids[i], &status, 0);
                    pids[i] = -1;
                    active_procs--;
                }
            }
        }
    }

    for (int i = 0; i < arg.workers; i++)
    {
        if (worker_pipes[i] != -1)
        {
            char buffer[256] = {0};
            int bytes_read;
            while ((bytes_read = read(worker_pipes[i], buffer, sizeof(buffer) - 1)) > 0)
            {
                buffer[bytes_read] = '\0';
                printf("%s", buffer);
                fflush(stdout);
            }
            close(worker_pipes[i]);
        }
    }

    shm->info.complete_flag = (g_shutdown == 1) ? 0 : 1;
    lseek(db_fd, 0, SEEK_SET);
    write(db_fd, &shm->info, sizeof(struct header));
    close(db_fd);
    munmap(shm, sizeof(struct sharedmemoryregion));
    unlink(arg.ipc_path);
    return 0;
}