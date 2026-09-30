#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>

#define MAX_PIDS 32768

struct header
{
    char magic[4];
    int format_version;
    int snapshot_id;
    int active_writers;
    int record_count;
    char snapshot_state[10];
};

struct process
{
    int pid;
    int ppid;
    char state;
    char comm[16];
    char cmdline[1024];
    int rss;
    double cpu;
    char rss_cale[256];
};

void lock_region(int fd, short type, off_t start, off_t len)
{
    struct flock lock;
    lock.l_type = type;
    lock.l_whence = SEEK_SET;
    lock.l_start = start;
    lock.l_len = len;
    fcntl(fd, F_SETLKW, &lock);
}

void unlock_region(int fd, off_t start, off_t len)
{
    struct flock lock;
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = start;
    lock.l_len = len;
    fcntl(fd, F_SETLK, &lock);
}

void set_lock_and_init(int pd_db)
{
    lock_region(pd_db, F_WRLCK, 0, sizeof(struct header));

    struct stat info_db;
    fstat(pd_db, &info_db);
    struct header h_db;
    memset(&h_db, 0, sizeof(struct header));

    if (info_db.st_size == 0)
    {
        strncpy(h_db.magic, "PRC1", 4);
        h_db.active_writers = 1;
        h_db.record_count = 0;
        strcpy(h_db.snapshot_state, "OPEN");
        h_db.format_version = 1;
        h_db.snapshot_id = time(NULL);
        write(pd_db, &h_db, sizeof(struct header));
    }
    else
    {
        lseek(pd_db, 0, SEEK_SET);
        read(pd_db, &h_db, sizeof(struct header));
        h_db.active_writers++;
        strcpy(h_db.snapshot_state, "OPEN");
        lseek(pd_db, 0, SEEK_SET);
        write(pd_db, &h_db, sizeof(struct header));
    }
    unlock_region(pd_db, 0, sizeof(struct header));
}

void write_proc(int pd_db, struct process *ps)
{
    lock_region(pd_db, F_WRLCK, 0, 0);

    lseek(pd_db, 0, SEEK_END);
    write(pd_db, ps, sizeof(struct process));

    struct header h;
    lseek(pd_db, 0, SEEK_SET);
    read(pd_db, &h, sizeof(struct header));
    h.record_count++;
    lseek(pd_db, 0, SEEK_SET);
    write(pd_db, &h, sizeof(struct header));

    unlock_region(pd_db, 0, 0);
}

void proc_pid_save(int pid_proc[], int *n_pid)
{
    DIR *dir_proc = opendir("/proc");
    if (!dir_proc)
        return;
    struct dirent *info_dir;
    while ((info_dir = readdir(dir_proc)) != NULL)
    {
        if (info_dir->d_name[0] >= '0' && info_dir->d_name[0] <= '9')
        {
            int curent_pid = atoi(info_dir->d_name);
            int found = 0;
            for (int i = 0; i < *n_pid; i++)
            {
                if (pid_proc[i] == curent_pid)
                {
                    found = 1;
                    break;
                }
            }
            if (!found && *n_pid < MAX_PIDS)
            {
                pid_proc[(*n_pid)++] = curent_pid;
            }
        }
    }
    closedir(dir_proc);
}

void proc_snapshot(int pid_proc[], int n_pid, int pd_db)
{
    for (int i = 0; i < n_pid; i++)
    {
        char cale_stat[256];
        sprintf(cale_stat, "/proc/%d/stat", pid_proc[i]);

        FILE *f_stat = fopen(cale_stat, "r");
        if (f_stat == NULL)
            continue;

        struct process ps;
        memset(&ps, 0, sizeof(struct process));
        long utime, stime;
        int check = fscanf(f_stat, "%d (%511[^)]) %c %d %*d %*d %*d %*d %*d %*d %*d %*d %*d %ld %ld",
                           &ps.pid, ps.comm, &ps.state, &ps.ppid, &utime, &stime);
        fclose(f_stat);

        if (check == 6)
        {
            long ticks = sysconf(_SC_CLK_TCK);
            ps.cpu = (double)(utime + stime) / ticks;
        }

        sprintf(ps.rss_cale, "/proc/%d/status", pid_proc[i]);
        FILE *f_status = fopen(ps.rss_cale, "r");
        if (f_status)
        {
            char linie[256];
            while (fgets(linie, sizeof(linie), f_status))
            {
                if (strncmp(linie, "VmRSS:", 6) == 0)
                {
                    long rss_kb;
                    sscanf(linie + 6, "%ld", &rss_kb);
                    ps.rss = (int)(rss_kb * 1024);
                    break;
                }
            }
            fclose(f_status);
        }

        char cale_cmd[256];
        sprintf(cale_cmd, "/proc/%d/cmdline", pid_proc[i]);
        int fd_cmd = open(cale_cmd, O_RDONLY);
        if (fd_cmd != -1)
        {
            int r = read(fd_cmd, ps.cmdline, sizeof(ps.cmdline) - 1);
            if (r > 0)
            {
                ps.cmdline[r] = '\0';
                for (int j = 0; j < r; j++)
                    if (ps.cmdline[j] == '\0')
                        ps.cmdline[j] = ' ';
            }
            close(fd_cmd);
        }
        write_proc(pd_db, &ps);
    }
}

int main(int argc, char *argv[])
{
    int pid_proc[MAX_PIDS], n_pid = 0;
    char db_path[256] = "data/proc.db";

    for (int i = 0; i < argc; i++)
    {
        if (strcmp("--db", argv[i]) == 0 && (i + 1) < argc)
        {
            strcpy(db_path, argv[i + 1]);
        }
    }

    int fd = open(db_path, O_RDWR | O_CREAT, 0644);
    if (fd == -1)
        return 1;

    set_lock_and_init(fd);
    proc_pid_save(pid_proc, &n_pid);
    proc_snapshot(pid_proc, n_pid, fd);

    lock_region(fd, F_WRLCK, 0, sizeof(struct header));
    struct header h;
    lseek(fd, 0, SEEK_SET);
    if (read(fd, &h, sizeof(struct header)) == sizeof(struct header))
    {
        h.active_writers--;
        if (h.active_writers <= 0)
        {
            h.active_writers = 0;
            strcpy(h.snapshot_state, "SEALED");
        }
        lseek(fd, 0, SEEK_SET);
        write(fd, &h, sizeof(struct header));
    }
    unlock_region(fd, 0, sizeof(struct header));

    close(fd);
    return 0;
}