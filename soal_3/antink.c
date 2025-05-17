#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

static const char *src_dir;

void write_log(const char *msg) {
    FILE *log = fopen("/var/log/it24.log", "a");
    if (log) {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char timebuf[64];
        strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", t);
        fprintf(log, "[%s] %s\n", timebuf, msg);
        fclose(log);
    }
}

int is_malicious(const char *filename) {
    return (strstr(filename, "nafis") || strstr(filename, "kimcun"));
}

void reverse_name(const char *src, char *dst) {
    int len = strlen(src);
    for (int i = 0; i < len; ++i)
        dst[i] = src[len - 1 - i];
    dst[len] = '\0';
}

// Mapping nama file dari mount point -> nama asli
void map_path(const char *path, char *real_path) {
    const char *rel_path = path + 1; // skip leading '/'
    char temp[512];

    if (is_malicious(rel_path)) {
        reverse_name(rel_path, temp);
        snprintf(real_path, 1024, "%s/%s", src_dir, temp);
    } else {
        snprintf(real_path, 1024, "%s/%s", src_dir, rel_path);
    }
}

static int antink_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    char real_path[1024];
    map_path(path, real_path);
    int res = lstat(real_path, stbuf);
    if (res == -1) return -errno;
    return 0;
}

static int antink_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
                          struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    DIR *dp;
    struct dirent *de;
    (void) offset;
    (void) fi;

    dp = opendir(src_dir);
    if (dp == NULL) return -errno;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    while ((de = readdir(dp)) != NULL) {
        // if (de->d_type == DT_DIR && 
        if ((strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)) continue;

        char fname[256];
        if (is_malicious(de->d_name)) {
            reverse_name(de->d_name, fname);

            char msg[300];
            snprintf(msg, sizeof(msg), "WARNING: File berbahaya terdeteksi: %s (ditampilkan sebagai %s)", de->d_name, fname);
            write_log(msg);
        } else {
            strcpy(fname, de->d_name);
        }
        filler(buf, fname, NULL, 0, 0);
    }

    closedir(dp);
    return 0;
}

void rot13(char *s) {
    for (int i = 0; s[i]; i++) {
        if ('a' <= s[i] && s[i] <= 'z') s[i] = ((s[i] - 'a' + 13) % 26) + 'a';
        else if ('A' <= s[i] && s[i] <= 'Z') s[i] = ((s[i] - 'A' + 13) % 26) + 'A';
    }
}

static int antink_open(const char *path, struct fuse_file_info *fi) {
    char msg[300];
    snprintf(msg, sizeof(msg), "INFO: Membuka file %s", path + 1);
    write_log(msg);
    char real_path[1024];
    map_path(path, real_path);

    int res = open(real_path, fi->flags);
    if (res == -1) return -errno;
    close(res);
    return 0;
}

static int antink_read(const char *path, char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    char real_path[1024];
    map_path(path, real_path);

    FILE *fp = fopen(real_path, "r");
    if (!fp) return -errno;

    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (offset < len) {
        if (offset + size > len) size = len - offset;
        fseek(fp, offset, SEEK_SET);
        fread(buf, 1, size, fp);

        // jika .txt dan bukan file berbahaya -> ROT13
        if (strstr(path, ".txt") && !is_malicious(path + 1)) {
            char logmsg[300];
            snprintf(logmsg, sizeof(logmsg), "INFO: Membaca dan mengenkripsi isi file %s", path + 1);
            write_log(logmsg);
            rot13(buf);
        }
    } else {
        size = 0;
    }

    fclose(fp);
    return size;
}

static const struct fuse_operations antink_oper = {
    .getattr = antink_getattr,
    .readdir = antink_readdir,
    .open    = antink_open,
    .read    = antink_read,
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_dir> <mount_point>\n", argv[0]);
        exit(1);
    }
    src_dir = realpath(argv[1], NULL);
    char *fuse_argv[] = { argv[0], argv[2], "-f" };
    return fuse_main(3, fuse_argv, &antink_oper, NULL);
}
