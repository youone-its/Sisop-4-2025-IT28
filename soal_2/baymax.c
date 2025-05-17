#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <time.h>

#define MAX_CHUNK_SIZE 1024
#define RELICS_DIR "relics"
#define LOG_FILE "activity.log"

const char *virtual_filename = "Baymax.jpeg";

void write_log(const char *action, const char *filename, const char *detail) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    fprintf(log, "[%04d-%02d-%02d %02d:%02d:%02d] %s: %s%s%s\n",
        t->tm_year+1900, t->tm_mon+1, t->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec,
        action, filename,
        detail ? " -> " : "",
        detail ? detail : "");
    fclose(log);
}

static int baymax_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    if (strcmp(path+1, virtual_filename) == 0 || strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        size_t total = 0;
        char partpath[256];
        for (int i = 0;; i++) {
            sprintf(partpath, "%s/%s.%03d", RELICS_DIR, virtual_filename, i);
            FILE *f = fopen(partpath, "rb");
            if (!f) break;
            fseek(f, 0, SEEK_END);
            total += ftell(f);
            fclose(f);
        }
        stbuf->st_size = total;
        return 0;
    }

    // Simulate other files (new writes)
    if (strstr(path, ".")) {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        return 0;
    }

    return -ENOENT;
}

static int baymax_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                          off_t offset, struct fuse_file_info *fi) {
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, virtual_filename, NULL, 0);

    return 0;
}

static int baymax_open(const char *path, struct fuse_file_info *fi) {
    if (strcmp(path+1, virtual_filename) != 0)
        return -ENOENT;

    write_log("READ", virtual_filename, NULL);
    return 0;
}

static int baymax_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    if (strcmp(path+1, virtual_filename) != 0)
        return -ENOENT;

    size_t read_bytes = 0, total = 0;
    char partpath[256];
    for (int i = 0;; i++) {
        sprintf(partpath, "%s/%s.%03d", RELICS_DIR, virtual_filename, i);
        FILE *f = fopen(partpath, "rb");
        if (!f) break;

        char temp[MAX_CHUNK_SIZE];
        size_t len = fread(temp, 1, MAX_CHUNK_SIZE, f);
        fclose(f);

        if (total + len <= offset) {
            total += len;
            continue;
        }

        size_t start = offset > total ? offset - total : 0;
        size_t to_copy = len - start;
        if (to_copy > size - read_bytes) to_copy = size - read_bytes;

        memcpy(buf + read_bytes, temp + start, to_copy);
        read_bytes += to_copy;
        total += len;

        if (read_bytes >= size) break;
    }

    return read_bytes;
}

static int baymax_write(const char *path, const char *buf, size_t size, off_t offset,
                        struct fuse_file_info *fi) {
    char fname[256];
    sscanf(path, "/%s", fname);

    int part = 0;
    size_t written = 0;
    while (written < size) {
        char partpath[256];
        sprintf(partpath, "%s/%s.%03d", RELICS_DIR, fname, part++);
        FILE *f = fopen(partpath, "wb");
        if (!f) return -EIO;
        size_t chunk = (size - written > MAX_CHUNK_SIZE) ? MAX_CHUNK_SIZE : size - written;
        fwrite(buf + written, 1, chunk, f);
        fclose(f);
        written += chunk;
    }

    char detail[256];
    sprintf(detail, "%s.000 - %s.%03d", fname, fname, part-1);
    write_log("WRITE", fname, detail);
    return size;
}

static int baymax_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    return 0; // allow file creation
}

static int baymax_unlink(const char *path) {
    char fname[256];
    sscanf(path, "/%s", fname);
    char partpath[256];
    int i = 0;
    while (1) {
        sprintf(partpath, "%s/%s.%03d", RELICS_DIR, fname, i);
        if (access(partpath, F_OK) != 0) break;
        remove(partpath);
        i++;
    }

    char detail[256];
    sprintf(detail, "%s.000 - %s.%03d", fname, fname, i-1);
    write_log("DELETE", fname, detail);
    return 0;
}

static struct fuse_operations baymax_oper = {
    .getattr = baymax_getattr,
    .readdir = baymax_readdir,
    .open = baymax_open,
    .read = baymax_read,
    .write = baymax_write,
    .unlink = baymax_unlink,
    .create = baymax_create
};

int main(int argc, char *argv[]) {
    return fuse_main(argc, argv, &baymax_oper, NULL);
}
