#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>

#define MAX_PATH 512

void get_timestamp(char* timestamp);
void log_message(const char* nama_file, const char* nama_file_image);
void download_and_extract();
void run_command(char *const argv[]);
int is_regular_file(const char *path);
char* read_hex_file(const char* filepath, long* out_length);
unsigned char* hex_to_binary(const char* hex_string, size_t* out_size);
void hexa_to_image();
int compare_file_name(const void *a, const void *b);

int main(int argc, char *argv[]) {
    download_and_extract();

    char timestamp[26];
    get_timestamp(timestamp);

    hexa_to_image();

    return 0;
}

void get_timestamp(char* timestamp) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(timestamp, 26, "%Y-%m-%d_%H:%M:%S", tm_info);
}

void log_message(const char* nama_file, const char* nama_file_image) {
    // [YYYY-mm-dd][HH:MM:SS]: Successfully converted hexadecimal text [nama file string] to [nama file image].
    // [2025-05-11][18:35:26]: Successfully converted hexadecimal text 1.txt to 1_image_2025-05-11_18:35:26.png.
    FILE *log_file = fopen("anomali/conversion.log", "a");
    if (log_file) {
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        char hari[26];
        char waktu[26];
        strftime(hari, sizeof(hari), "%Y-%m-%d", tm_info);
        strftime(waktu, sizeof(waktu), "%H:%M:%S", tm_info);
        fprintf(log_file, "[%s][%s]: Successfully converted hexadecimal text %s to %s\n", hari, waktu, nama_file, nama_file_image);
        fclose(log_file);
    } else {
        perror("Gagal membuka log file");
    }
}

void download_and_extract() {
    struct stat st = {0};
    if (stat("anomali", &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("Folder anomali sudah ada. Melewati download.\n");
        return;
    }

    printf("Mendownload dan mengekstrak anomali.zip...\n");

    char *wget_args[] = {
        "wget", "-q", "-O", "anomali.zip",
        "https://drive.usercontent.google.com/u/0/uc?id=1hi_GDdP51Kn2JJMw02WmCOxuc3qrXzh5&export=download",
        NULL
    };
    run_command(wget_args);

    char *unzip_args[] = {"unzip", "-q", "anomali.zip", NULL};
    run_command(unzip_args);

    unlink("anomali.zip");
}

void run_command(char *const argv[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        perror("exec gagal");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        perror("fork gagal");
    }
}

// Mengecek apakah file biasa
int is_regular_file(const char *path) {
    struct stat path_stat;
    if (stat(path, &path_stat) != 0) return 0;
    return S_ISREG(path_stat.st_mode);
}

// Membaca file hex menjadi string
char* read_hex_file(const char* filepath, long* out_length) {
    FILE* file = fopen(filepath, "r");
    if (!file) {
        perror("Gagal membuka file");
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long len = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = malloc(len + 1);
    if (!buffer) {
        perror("Gagal alokasi buffer");
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, len, file);
    buffer[len] = '\0';

    fclose(file);
    if (out_length) *out_length = len;
    return buffer;
}

// Mengubah hex string ke biner
unsigned char* hex_to_binary(const char* hex_string, size_t* out_size) {
    size_t hex_len = strlen(hex_string);
    if (hex_len % 2 != 0) return NULL;

    *out_size = hex_len / 2;
    unsigned char* bin = malloc(*out_size);
    if (!bin) {
        perror("Gagal alokasi biner");
        return NULL;
    }

    for (size_t i = 0; i < hex_len; i += 2) {
        sscanf(&hex_string[i], "%2hhx", &bin[i / 2]);
    }

    return bin;
}


void hexa_to_image() {
    const char folder_path[] = "anomali";
    char image_dir[MAX_PATH];
    snprintf(image_dir, sizeof(image_dir), "%s/image", folder_path);

    struct stat st = {0};
    if (stat(image_dir, &st) == -1) {
        mkdir(image_dir, 0700);
    }

    DIR *dir = opendir(folder_path);
    if (!dir) {
        perror("Gagal membuka direktori");
        return;
    }

    // Membaca semua file .txt ke dalam array untuk sorting
    struct dirent *entry;
    char *file_list[100];
    int file_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        char filepath[MAX_PATH];
        snprintf(filepath, sizeof(filepath), "%s/%s", folder_path, entry->d_name);
        
        if (!is_regular_file(filepath)) continue;

        char *ext = strrchr(entry->d_name, '.');
        if (!ext || strcmp(ext, ".txt") != 0) continue;

        file_list[file_count] = strdup(entry->d_name);
        file_count++;
    }
    
    closedir(dir);

    // Sort file list by numeric order (assuming filenames are numeric)
    qsort(file_list, file_count, sizeof(char*), compare_file_name);

    for (int i = 0; i < file_count; i++) {
        char filepath[MAX_PATH];
        snprintf(filepath, sizeof(filepath), "%s/%s", folder_path, file_list[i]);

        printf("Membaca file hex: %s\n", filepath);

        long hex_len;
        char* hex_string = read_hex_file(filepath, &hex_len);
        if (!hex_string) continue;

        size_t bin_size;
        unsigned char* bin_data = hex_to_binary(hex_string, &bin_size);
        if (!bin_data) {
            free(hex_string);
            continue;
        }

        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        char timestamp[64];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", tm_info);

        char base_name[256];
        strncpy(base_name, file_list[i], sizeof(base_name));
        base_name[sizeof(base_name) - 1] = '\0';
        char *dot = strrchr(base_name, '.');
        if (dot) *dot = '\0';

        char image_filename[512];
        snprintf(image_filename, sizeof(image_filename), "%s_image_%s.png", base_name, timestamp);

        char image_path[1024];
        snprintf(image_path, sizeof(image_path), "%s/%s", image_dir, image_filename);

        FILE *img_file = fopen(image_path, "wb");
        if (img_file) {
            fwrite(bin_data, 1, bin_size, img_file);
            fclose(img_file);
            log_message(file_list[i], image_filename);
            printf("File gambar berhasil disimpan: %s\n", image_path);
        } else {
            perror("Gagal menyimpan file gambar");
        }

        free(hex_string);
        free(bin_data);
        free(file_list[i]);
    }
}

int compare_file_name(const void *a, const void *b) {
    return atoi(*(const char**)a) - atoi(*(const char**)b);
}