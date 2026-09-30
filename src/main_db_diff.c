#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

struct header
{
    char magic[4];
    int format_version;
    long snapshot_id;
    int active_writers;
    int record_count;
    char snapshot_state[10];
};

struct record
{
    char path[512];
    char nume_binar[512];
    off_t size;
    time_t time;
    char tip_fisier[100];
    int hash;
};

struct process
{
    int pid;
    int ppid;
    char state;
    char comm[512];
    char cmdline[1024];
    int rss;
    double cpu;
    char rss_cale[256];
};

void compare_files(int fd_new, int fd_old, int argc, char *argv[])
{
    char out_path[256] = "reports/T3_filediff.txt";
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--out") == 0 && (i + 1) < argc)
            strcpy(out_path, argv[i + 1]);
    }

    int fd_out = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0)
    {
        perror("Eroare la deschidere.\n");
        exit(4);
    }

    struct header h_old, h_new;
    lseek(fd_new, 0, SEEK_SET);
    read(fd_new, &h_new, sizeof(struct header));
    lseek(fd_old, 0, SEEK_SET);
    read(fd_old, &h_old, sizeof(struct header));

    struct record r_old, r_new;
    for (int i = 0; i < h_new.record_count; i++)
    {
        lseek(fd_new, sizeof(struct header) + i * sizeof(struct record), SEEK_SET);
        read(fd_new, &r_new, sizeof(struct record));
        int found = 0;
        for (int j = 0; j < h_old.record_count; j++)
        {
            lseek(fd_old, sizeof(struct header) + j * sizeof(struct record), SEEK_SET);
            read(fd_old, &r_old, sizeof(struct record));
            if (strcmp(r_new.path, r_old.path) == 0)
            {
                found = 1;
                if (r_old.size != r_new.size || r_old.time != r_new.time || r_old.hash != r_new.hash || strcmp(r_old.tip_fisier, r_new.tip_fisier) != 0)
                    dprintf(fd_out, "Modificat: %s\n", r_new.path);
                break;
            }
        }
        if (!found)
            dprintf(fd_out, "INTRARE APARUTA: %s\n", r_new.path);
    }

    for (int i = 0; i < h_old.record_count; i++)
    {
        lseek(fd_old, sizeof(struct header) + i * sizeof(struct record), SEEK_SET);
        read(fd_old, &r_old, sizeof(struct record));
        int found = 0;
        for (int j = 0; j < h_new.record_count; j++)
        {
            lseek(fd_new, sizeof(struct header) + j * sizeof(struct record), SEEK_SET);
            read(fd_new, &r_new, sizeof(struct record));
            if (strcmp(r_old.path, r_new.path) == 0)
            {
                found = 1;
                break;
            }
        }
        if (!found)
            dprintf(fd_out, "INTRARE DISPARUTA: %s\n", r_old.path);
    }
    close(fd_out);
}

void compare_proc(int fd_new, int fd_old, int argc, char *argv[])
{
    char out_path[256] = "reports/T3_procdiff.txt";
    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--out") == 0 && (i + 1) < argc)
            strcpy(out_path, argv[i + 1]);
    }

    int fd_out = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out < 0)
        return;

    struct header h_old, h_new;
    lseek(fd_new, 0, SEEK_SET);
    read(fd_new, &h_new, sizeof(struct header));
    lseek(fd_old, 0, SEEK_SET);
    read(fd_old, &h_old, sizeof(struct header));

    struct process p_old, p_new;
    for (int i = 0; i < h_new.record_count; i++)
    {
        lseek(fd_new, sizeof(struct header) + i * sizeof(struct process), SEEK_SET);
        read(fd_new, &p_new, sizeof(struct process));
        int found = 0;
        for (int j = 0; j < h_old.record_count; j++)
        {
            lseek(fd_old, sizeof(struct header) + j * sizeof(struct process), SEEK_SET);
            read(fd_old, &p_old, sizeof(struct process));
            if (p_new.pid == p_old.pid)
            {
                found = 1;
                if (p_new.state != p_old.state || p_new.rss != p_old.rss || p_new.cpu != p_old.cpu)
                    dprintf(fd_out, "Modificat: %d\n", p_new.pid);
                break;
            }
        }
        if (!found)
            dprintf(fd_out, "INTRARE APARUTA: %d\n", p_new.pid);
    }

    for (int i = 0; i < h_old.record_count; i++)
    {
        lseek(fd_old, sizeof(struct header) + i * sizeof(struct process), SEEK_SET);
        read(fd_old, &p_old, sizeof(struct process));
        int found = 0;
        for (int j = 0; j < h_new.record_count; j++)
        {
            lseek(fd_new, sizeof(struct header) + j * sizeof(struct process), SEEK_SET);
            read(fd_new, &p_new, sizeof(struct process));
            if (p_old.pid == p_new.pid)
            {
                found = 1;
                break;
            }
        }
        if (!found)
            dprintf(fd_out, "INTRARE DISPARUTA: %d\n", p_old.pid);
    }
    close(fd_out);
}

int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        perror("Nu exista destule argumente pentru comanda.\n");
        exit(1);
    }
    char old_db[256] = "", new_db[256] = "";
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--old") == 0 && (i + 1) < argc)
            strcpy(old_db, argv[++i]);
        if (strcmp(argv[i], "--new") == 0 && (i + 1) < argc)
            strcpy(new_db, argv[++i]);
    }

    int fd_old = open(old_db, O_RDONLY);
    int fd_new = open(new_db, O_RDONLY);
    if (fd_old < 0 || fd_new < 0)
    {
        perror("Eroare la deschiderea fisierulu!\n");
        exit(2);
    }

    struct header h1, h2;
    read(fd_old, &h1, sizeof(struct header));
    read(fd_new, &h2, sizeof(struct header));

    if (strncmp(h1.magic, h2.magic, 4) == 0)
    {
        if (strncmp(h1.magic, "IDX1", 4) == 0)
            compare_files(fd_new, fd_old, argc, argv);
        else if (strncmp(h1.magic, "PRC1", 4) == 0)
            compare_proc(fd_new, fd_old, argc, argv);
    }
    else
    {
        perror("Eroare la compararea a doua fisiere care contin date diferite.\n");
        exit(3);
    }

    close(fd_old);
    close(fd_new);
    return 0;
}