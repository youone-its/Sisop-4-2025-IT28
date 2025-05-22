#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_PART 10000
#define PART_SIZE 1024
#define RELIC_DIR "/home/zika/Documents/sisop/shift4/soal_2/relics"
#define LOG_FILE "/home/zika/Documents/sisop/shift4/soal_2/activity.log"

void write_log(const char *action, const char *info) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log, "[%04d-%02d-%02d %02d:%02d:%02d] %s: %s\n",
            t->tm_year+1900, t->tm_mon+1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec,
            action, info);
    fclose(log);
}

static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    char name[256];
    sscanf(path, "/%s", name);

    char part_path[512];
    snprintf(part_path, sizeof(part_path), "%s/%s.000", RELIC_DIR, name);
    if (access(part_path, F_OK) == 0) {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;

        int total = 0;
        for (int i = 0; ; i++) {
            snprintf(part_path, sizeof(part_path), "%s/%s.%03d", RELIC_DIR, name, i);
            FILE *f = fopen(part_path, "rb");
            if (!f) break;
            fseek(f, 0, SEEK_END);
            total += ftell(f);
            fclose(f);
        }
        stbuf->st_size = total;
        return 0;
    }

    return -ENOENT;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    DIR *d = opendir(RELIC_DIR);
    if (!d) return 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (strstr(de->d_name, ".000")) {
            char filename[256];
            strncpy(filename, de->d_name, strlen(de->d_name) - 4);
            filename[strlen(de->d_name) - 4] = '\0';
            filler(buf, filename, NULL, 0, 0);
        }
    }
    closedir(d);
    return 0;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    char name[256];
    sscanf(path, "/%s", name);
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s.000", RELIC_DIR, name);
    if (access(filepath, F_OK) == -1)
        return -ENOENT;

    char logmsg[256];
    snprintf(logmsg, sizeof(logmsg), "%s", name);
    write_log("READ", logmsg);

    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi) {
    (void) fi;
    char name[256];
    sscanf(path, "/%s", name);

    size_t total_read = 0;
    int i = 0, current_offset = 0;
    while (total_read < size) {
        char part_path[512];
        snprintf(part_path, sizeof(part_path), "%s/%s.%03d", RELIC_DIR, name, i);
        FILE *f = fopen(part_path, "rb");
        if (!f) break;

        char part_buf[PART_SIZE];
        size_t part_size = fread(part_buf, 1, PART_SIZE, f);
        fclose(f);

        if (offset >= current_offset + part_size) {
            current_offset += part_size;
            i++;
            continue;
        }

        size_t start = (offset > current_offset) ? offset - current_offset : 0;
        size_t to_copy = (part_size - start < size - total_read) ? part_size - start : size - total_read;
        memcpy(buf + total_read, part_buf + start, to_copy);
        total_read += to_copy;
        current_offset += part_size;
        i++;
    }

    // Tambahan: Log COPY
    // cp akan menggunakan read (bisa dikenali jika offset = 0 dan total_read > 0)
    if (offset == 0 && total_read > 0) {
        char logmsg[256];
        snprintf(logmsg, sizeof(logmsg), "%s", name);
        write_log("COPY", logmsg);
    }

    return total_read;
}

static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) mode;
    (void) fi;
    char name[256];
    sscanf(path, "/%s", name);

    char part_path[512];
    snprintf(part_path, sizeof(part_path), "%s/%s.000", RELIC_DIR, name);

    FILE *f = fopen(part_path, "wb"); // buat file kosong
    if (!f) return -EACCES;
    fclose(f);

    char logmsg[256];
    snprintf(logmsg, sizeof(logmsg), "%s.000", name);
    write_log("CREATE", logmsg);

    return 0;
}

static int fs_write(const char *path, const char *buf, size_t size, off_t offset,
             struct fuse_file_info *fi) {
    (void) fi;
    char name[256];
    sscanf(path, "/%s", name);

    int part_num = 0;
    size_t remaining = size;
    size_t written = 0;

    char loginfo[1024];
    snprintf(loginfo, sizeof(loginfo), "%s -> ", name);

    while (remaining > 0) {
        char part_path[512];
        snprintf(part_path, sizeof(part_path), "%s/%s.%03d", RELIC_DIR, name, part_num);
        FILE *f = fopen(part_path, "wb");
        if (!f) break;

        size_t to_write = (remaining < PART_SIZE) ? remaining : PART_SIZE;
        fwrite(buf + written, 1, to_write, f);
        fclose(f);

        char part_name[64];
        snprintf(part_name, sizeof(part_name), "%s.%03d, ", name, part_num);
        strcat(loginfo, part_name);

        written += to_write;
        remaining -= to_write;
        part_num++;
    }

    loginfo[strlen(loginfo) - 2] = '\0';
    write_log("WRITE", loginfo);

    return size;
}

static int fs_unlink(const char *path) {
    char name[256];
    sscanf(path, "/%s", name);

    int i = 0;
    char loginfo[512];
    snprintf(loginfo, sizeof(loginfo), "%s.000 - ", name);

    while (1) {
        char part_path[512];
        snprintf(part_path, sizeof(part_path), "%s/%s.%03d", RELIC_DIR, name, i);
        if (access(part_path, F_OK) == -1) break;
        remove(part_path);
        i++;
    }

    snprintf(loginfo + strlen(loginfo), 512 - strlen(loginfo), "%s.%03d", name, i - 1);
    write_log("DELETE", loginfo);

    return 0;
}

static const struct fuse_operations fs_oper = {
    .getattr = fs_getattr,
    .readdir = fs_readdir,
    .open    = fs_open,
    .read    = fs_read,
    .create  = fs_create,
    .write   = fs_write,
    .unlink  = fs_unlink,
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &fs_oper, NULL);
}
