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

struct header
{
    char magic[4];
    int format_version;
    int snapshot_id;
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
    char tip_fisier[13];
    int hash;
};
void set_lock(int fd_db)
{
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_len = sizeof(struct header);
    lock.l_start = 0;
    fcntl(fd_db, F_SETLKW, &lock);
    struct stat info_db;
    fstat(fd_db, &info_db);
    struct header h_db;
    memset(&h_db, 0, sizeof(struct header));
    if (info_db.st_size == 0)
    {
        strncpy(h_db.magic, "IDX1", 4);
        h_db.active_writers = 1;
        h_db.record_count = 0;
        strcpy(h_db.snapshot_state, "OPEN");
        h_db.format_version = 1;
        h_db.snapshot_id = time(NULL);
        write(fd_db, &h_db, sizeof(struct header));
    }
    else if (info_db.st_size != 0)
    {
        lseek(fd_db, 0, SEEK_SET);
        read(fd_db, &h_db, sizeof(struct header));
        if (strcmp(h_db.snapshot_state, "SEALED") == 0)
        {
            printf("Baza de date este inchisa.Initializare nou snapshot:\n");
            ftruncate(fd_db, 0);
            lseek(fd_db, 0, SEEK_SET);
            h_db.snapshot_id = time(NULL);
            h_db.active_writers = 1;
            h_db.record_count = 0;
            strcpy(h_db.snapshot_state, "OPEN");
            write(fd_db, &h_db, sizeof(struct header));
        }
        else
        {
            h_db.active_writers++;
            lseek(fd_db, 0, SEEK_SET);
            write(fd_db, &h_db, sizeof(struct header));
        }
    }
    lock.l_type = F_UNLCK;
    fcntl(fd_db, F_SETLK, &lock);
}
void write_record(int fd_db, struct record *fisier)
{
    struct flock lock;
    off_t poz = lseek(fd_db, 0, SEEK_END);

    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_len = sizeof(struct record);
    lock.l_start = poz;

    fcntl(fd_db, F_SETLKW, &lock);
    write(fd_db, fisier, sizeof(struct record));

    lock.l_type = F_UNLCK;
    fcntl(fd_db, F_SETLK, &lock);

    struct header h_db_count;
    lock.l_type = F_WRLCK;
    lock.l_len = sizeof(struct header);
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    fcntl(fd_db, F_SETLKW, &lock);
    lseek(fd_db, 0, SEEK_SET);
    read(fd_db, &h_db_count, sizeof(struct header));
    h_db_count.record_count++;
    lseek(fd_db, 0, SEEK_SET);
    write(fd_db, &h_db_count, sizeof(struct header));
    lock.l_type = F_UNLCK;
    fcntl(fd_db, F_SETLK, &lock);
}
void set_bin_name(char *bin_name, int ino, int dev)
{
    char buff[1024];
    sprintf(buff, "%d", dev);
    strcpy(bin_name, buff);
    buff[0] = '\0';
    sprintf(buff, "%d", ino);
    strcat(bin_name, "_");
    strcat(bin_name, buff);
}
int hash_sum(char *fisier)
{
    int fd = open(fisier, O_RDONLY);
    int sum = 0;
    char ch;
    do
    {
        int fisier_eroare = read(fd, &ch, 1);
        if (fisier_eroare == -1)
        {
            perror("Eroare la deschiderea fisierului pentru hash\n");
            return -1;
        }
        if (fisier_eroare == 0)
            break;
        sum += (int)ch;
    } while (1);
    close(fd);
    return sum;
}
void file_index(const char *nume_d, int fd_db)
{
    DIR *dir_curent;
    struct dirent *intrare_d;
    struct stat informatii;
    if (!(dir_curent = opendir(nume_d)))
        return;
    intrare_d = readdir(dir_curent);
    while (intrare_d != NULL)
    {
        if (strcmp(intrare_d->d_name, ".") == 0 || strcmp(intrare_d->d_name, "..") == 0)
        {
            intrare_d = readdir(dir_curent);
            continue;
        }

        char cale_noua[1024];
        strcpy(cale_noua, nume_d);
        strcat(cale_noua, "/");
        strcat(cale_noua, intrare_d->d_name);

        if (lstat(cale_noua, &informatii) == -1)
        {
            intrare_d = readdir(dir_curent);
            continue;
        }
        struct record fisier;
        memset(&fisier, 0, sizeof(struct record));

        strncpy(fisier.path, cale_noua, 511);
        fisier.time = informatii.st_mtime;
        set_bin_name(fisier.nume_binar, informatii.st_ino, informatii.st_dev);

        if (S_ISDIR(informatii.st_mode))
        {
            fisier.size = 0;
            strncpy(fisier.tip_fisier, "DIRECTOR", sizeof(fisier.tip_fisier));
            write_record(fd_db, &fisier);
            file_index(cale_noua, fd_db);
        }
        else
        {
            if (S_ISREG(informatii.st_mode))
            {
                fisier.size = informatii.st_size;
                strncpy(fisier.tip_fisier, "REGULAR FILE", sizeof(fisier.tip_fisier));
                fisier.hash = hash_sum(fisier.path);
                write_record(fd_db, &fisier);
            }
            else if (S_ISLNK(informatii.st_mode))
            {
                fisier.size = 0;
                strncpy(fisier.tip_fisier, "SYMLINK", sizeof(fisier.tip_fisier));
                write_record(fd_db, &fisier);
            }
            else if (S_ISFIFO(informatii.st_mode))
            {
                fisier.size = 0;
                strncpy(fisier.tip_fisier, "FIFO", sizeof(fisier.tip_fisier));
                write_record(fd_db, &fisier);
            }
        }
        intrare_d = readdir(dir_curent);
    }
    closedir(dir_curent);
}
int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        perror("Nu exista argumente pentru aceasta comanda");
        exit(1);
    }
    char dir[256] = "";
    char db[256] = "data/index.db";
    for (int i = 1; i < argc; i++)
    {
        if (strcmp("--root", argv[i]) == 0 && (i + 1) < argc)
            strcpy(dir, argv[i + 1]);
        if (strcmp("--db", argv[i]) == 0 && (i + 1) < argc)
            strcpy(db, argv[i + 1]);
    }
    int fd_temp = open(db, O_RDWR | O_CREAT, 0644);
    int fd_db = -1;
    struct header h_verif;
    ssize_t n = read(fd_temp, &h_verif, sizeof(struct header));
    if (n == sizeof(struct header))
    {
        if (strcmp(h_verif.snapshot_state, "SEALED") == 0)
        {
            long nou_id = time(NULL);
            char nume_nou[512];
            sprintf(nume_nou, "data/index_%ld.db", nou_id);

            close(fd_temp);
            strcpy(db, nume_nou);
            fd_db = open(db, O_RDWR | O_CREAT, 0644);
        }
        else
        {
            fd_db = fd_temp;
            lseek(fd_db, 0, SEEK_SET);
        }
    }
    else
    {
        fd_db = fd_temp;
        lseek(fd_db, 0, SEEK_SET);
    }
    if (fd_db == -1)
    {
        perror("Eroare la deschidere fisier\n");
        exit(2);
    }

    set_lock(fd_db);
    file_index(dir, fd_db);
    struct header h_db_main;
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_len = sizeof(struct header);
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    fcntl(fd_db, F_SETLKW, &lock);
    lseek(fd_db, 0, SEEK_SET);
    read(fd_db, &h_db_main, sizeof(struct header));
    h_db_main.active_writers--;
    if (h_db_main.active_writers == 0)
        strcpy(h_db_main.snapshot_state, "SEALED");
    lseek(fd_db, 0, SEEK_SET);
    write(fd_db, &h_db_main, sizeof(struct header));
    lock.l_type = F_UNLCK;
    fcntl(fd_db, F_SETLK, &lock);
    close(fd_db);
    return 0;
}