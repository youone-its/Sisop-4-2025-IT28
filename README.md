# Sisop-4-2025-IT28
# Laporan Praktikum IT28 Modul 4
## Anggota Kelompok
| No |             Nama              |     NRP     |
|----|-------------------------------|-------------|
| 1  | Yuan Banny Albyan             | 5027241027  |
| 2  | Ica Zika Hamizah              | 5027241058  |
| 3  | Nafis Faqih Allmuzaky Maolidi | 5027241095  |

## Soal_1
### Oleh: Ica Zika Hamizah
### Overview Program
- Mengunduh dan mengekstrak file ZIP bernama `anomali.zip`.
- Membaca semua file `.txt` dari folder anomali/ yang berisi representasi gambar dalam bentuk string hexadecimal.
- Mengubah string hex menjadi file gambar `(.png)`.
- Menyimpan hasil gambar ke dalam folder `anomali/image/`.
- Mencatat log setiap konversi ke `anomali/conversion.log`.

### Program Utama
```
int main(int argc, char *argv[]) {
    download_and_extract();
    char timestamp[26];
    get_timestamp(timestamp);
    hexa_to_image();
    return 0;
}
```
Pertama memanggil `download_and_extract();` untuk download dan extract ZIP jika belum ada folder anomali, lalu mulai konversi semua file yang ada di dalam folder anomali, dari hexa menjadi image.
### Fungsi download dan extract
```
void download_and_extract() {
    if (stat("anomali", &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("Folder anomali sudah ada. Melewati download.\n");
        return;
    }

    char *wget_args[] = {
        "wget", "-q", "-O", "anomali.zip",
        "https://.../export=download",
        NULL
    };
    run_command(wget_args);

    char *unzip_args[] = {"unzip", "-q", "anomali.zip", NULL};
    run_command(unzip_args);

    unlink("anomali.zip");
}
```
Pertama cek apakah folder anomali sudah tersedia, jika belum tersedia maka mulai download file ZIP dengan cara memanggil `run_command();`. Setelah di download maka ZIP di extract (menggunakan run command juga), terakhir file ZIP di hapus dengan `unlink("anomali.zip");`

```
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
```
Tujuan fungsi run command adalah menjalankan perintah shell `(wget, unzip)` dari program C dengan `fork() + execvp()`

### Fungsi hexa to image
```
char image_dir[MAX_PATH];
snprintf(image_dir, sizeof(image_dir), "%s/image", folder_path);
```
- `image_dir` menyimpan path direktori output gambar.
- `snprintf` membuat path lengkap: `"anomali/image"`.

```
struct stat st = {0};
    if (stat(image_dir, &st) == -1) {
        mkdir(image_dir, 0700);
    }
```
- Mengecek apakah direktori `image/` sudah ada.
- Jika belum ada (`stat` gagal), maka dibuat dengan `mkdir()`.
- `0700` berarti: hanya user yang bisa membaca, menulis, dan mengeksekusi folder.

```
DIR *dir = opendir(folder_path);
if (!dir) {
    perror("Gagal membuka direktori");
    return;
}
```
- Membuka folder `anomali/` untuk membaca isi file di dalamnya.
- Jika gagal (`opendir` mengembalikan `NULL`), program mencetak pesan error dan keluar dari fungsi.

```
struct dirent *entry;
    char *file_list[100];
    int file_count = 0;
```
- `entry` akan digunakan untuk membaca file satu per satu.
- `file_list` menyimpan nama-nama file `.txt` yang valid.
- `file_count` untuk menghitung jumlah file yang ditemukan.

```
while ((entry = readdir(dir)) != NULL) {
```
Looping membaca seluruh entri dalam folder `anomali/`.

```
char filepath[MAX_PATH];
        snprintf(filepath, sizeof(filepath), "%s/%s", folder_path, entry->d_name);
```
`filepath` menyimpan path lengkap ke file: misalnya `anomali/1.txt`.

```
    if (!is_regular_file(filepath)) continue;
```
Lewati file jika bukan file biasa (misalnya folder, symlink, dll.).

```
char *ext = strrchr(entry->d_name, '.');
        if (!ext || strcmp(ext, ".txt") != 0) continue;
```
Mengecek ekstensi file dan hanya file `.txt` yang diproses.

```
    file_list[file_count] = strdup(entry->d_name);
    file_count++;
```
- Menyimpan nama file ke array `file_list` dan menambah jumlah file.
- `strdup` membuat salinan string agar bisa diedit/dibebaskan nanti.

```
qsort(file_list, file_count, sizeof(char*), compare_file_name);
```
- Mengurutkan file `.txt` berdasarkan nama numerik menggunakan fungsi `compare_file_name`.
- Misalnya: `1.txt, 2.txt, 10.txt` bukan `1.txt, 10.txt, 2.txt`.

```
    char filepath[MAX_PATH];
    snprintf(filepath, sizeof(filepath), "%s/%s", folder_path, file_list[i]);
```
Membuat path lengkap dari file yang akan dikonversi.

```
    long hex_len;
    char* hex_string = read_hex_file(filepath, &hex_len);
    if (!hex_string) continue;
```
- Membaca isi file dan menyimpannya sebagai string hexadecimal.
- Jika gagal, lewati file ini dan lanjut ke file berikutnya.

```
size_t bin_size;
        unsigned char* bin_data = hex_to_binary(hex_string, &bin_size);
        if (!bin_data) {
            free(hex_string);
            continue;
        }
```
- Mengonversi hex string ke data biner (array byte).
- Jika konversi gagal, bebaskan memori `hex_string` dan lanjut ke file lain.

```
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", tm_info);
```
- Mendapatkan timestamp saat ini dalam format `YYYY-MM-DD_HH-MM-SS`.
- Akan digunakan sebagai bagian dari nama file output.

```
char base_name[256];
        strncpy(base_name, file_list[i], sizeof(base_name));
        base_name[sizeof(base_name) - 1] = '\0';
        char *dot = strrchr(base_name, '.');
        if (dot) *dot = '\0';
```
Mengambil nama file tanpa ekstensi .txt.

```
    char image_filename[512];
    snprintf(image_filename, sizeof(image_filename), "%s_image_%s.png", base_name, timestamp);
```
Menyiapkan nama file gambar hasil: contoh `1_image_2025-05-20_11-00-00.png`.

```
char image_path[1024];
        snprintf(image_path, sizeof(image_path), "%s/%s", image_dir, image_filename);
```
Menyusun path lengkap dari file hasil gambar di folder `anomali/image/`.

```
    FILE *img_file = fopen(image_path, "wb");
```
Membuka file output dalam mode write binary (`"wb"`).

```
if (img_file) {
            fwrite(bin_data, 1, bin_size, img_file);
            fclose(img_file);
            log_message(file_list[i], image_filename);
            printf("File gambar berhasil disimpan: %s\n", image_path);
```
Jika file berhasil dibuka:
1. Tulis data biner ke file `.png`.
2. Tutup file.
3. Log aktivitas ke file `conversion.log`.
4. Cetak pesan sukses ke terminal.

```
    } else {
        perror("Gagal menyimpan file gambar");
    }
```
Jika file tidak bisa dibuka, tampilkan error di terminal.

```
free(hex_string);
        free(bin_data);
        free(file_list[i]);
    }
```
Bebaskan memori yang sudah dialokasikan sebelumnya agar tidak terjadi memory leak:
1. Hex string.
2. Data biner.
3. Nama file yang disalin dengan `strdup`.

## Soal_2
### Oleh: Nafis Faqih Allmuzaky Maolidi

## Soal_3
### Oleh: Ica Zika Hamizah

### Dockerfile
```
FROM ubuntu:20.04
```
Menggunakan base image Ubuntu versi 20.04

```
ENV DEBIAN_FRONTEND=noninteractive
```
Menonaktifkan prompt interaktif saat instalasi package (agar tidak macet saat `apt install`)

```
RUN apt-get update && apt-get install -y --no-install-recommends \
    fuse3 \
    gcc \
    make \
    pkg-config \
    libfuse3-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
```
#### Meng-update package list dan menginstal:

1. fuse3 → Library dan tools FUSE 3 (Filesystem in Userspace).
2. gcc, make → Untuk kompilasi program C.
3. pkg-config → Digunakan agar gcc tahu di mana mencari flag kompilasi (misalnya -lfuse3).
4. libfuse3-dev → Header dan library untuk development FUSE3.

#### Membersihkan cache untuk menghemat ukuran image.

```
WORKDIR /app
```
Mengatur direktori kerja di dalam container ke /app

```
COPY antink.c
```
Menyalin file antink.c dari direktori lokal ke dalam container (`/app`)

```
RUN gcc antink.c -o antink `pkg-config fuse3 --cflags --libs` -pthread \
    && rm antink.c
```
- Mengompilasi `antink.c` menjadi binary `antink` menggunakan flag FUSE3 dan pthread
- Menghapus file source setelah dikompilasi untuk menghemat ruang

```
RUN mkdir -p /it24_host /antink_mount /var/log \
    && touch /var/log/it24.log
```
Membuat tiga direktori:
- `/it24_host` → Mount point ke folder host.
- `/antink_mount` → Mount point output dari FUSE.
- `/var/log` → Lokasi log file `it24.log`.

Membuat file log kosong agar siap digunakan.

```
ENTRYPOINT ["./antink", "/it24_host", "/antink_mount"]
```
Perintah yang dijalankan saat container start: menjalankan binary `antink` dengan argumen mount source dan mount target.

### docker-compose.yml

```
version: '3.8'
```
Versi Compose file schema yang digunakan

#### Service: antink-server
```
services:
  antink-server:
    build: .
```
Membangun image dari `Dockerfile` di direktori saat ini (`.`)

```
    container_name: antink-server
```
Menetapkan nama container agar lebih mudah dikenali

```
    privileged: true
```
Memberikan hak akses penuh ke container (diperlukan agar FUSE dapat dijalankan)

```
    devices:
      - /dev/fuse
```
Memberikan akses ke device `/dev/fuse` di host agar container bisa menggunakan FUSE

```
    cap_add:
      - SYS_ADMIN
```
Menambahkan kapabilitas `SYS_ADMIN` (juga dibutuhkan untuk FUSE)

```
    security_opt:
      - apparmor:unconfined
```
Mematikan profil AppArmor untuk container agar FUSE tidak dibatasi

```
    volumes:
      - ./it24_host:/it24_host
      - ./antink_mount:/antink_mount
      - ./antink_logs:/var/log/
```
Volume mapping:

- `./it24_host` → Mount point sumber (file asli).
- `./antink_mount` → Mount point hasil dari FUSE.
- `./antink_logs` → Sinkronisasi log `/var/log/it24.log` ke host.

```
    restart: always
```
Container akan otomatis restart jika crash atau saat reboot

#### Service: antink-logger
```
  antink-logger:
    image: ubuntu:20.04
```
Menggunakan image Ubuntu biasa, tanpa build

```
    container_name: antink-logger
```
Nama container logging

```
    volumes:
      - ./antink_logs:/var/log/
```
Share volume log dengan `antink-server`, agar bisa melihat file log `it24.log`.

```
    command: tail -f /var/log/it24.log
```
Perintah untuk membaca log secara real-time dari container (mirip `tail -f` di terminal).

### antink.c
#### Fungsi write_log(const char *msg)
```
time_t now = time(NULL);
struct tm *t = localtime(&now);
strftime(...) -> format waktu log
fprintf(...) -> tulis [timestamp] pesan
```
Menulis log timestamped ke `/var/log/it24.log`

#### Fungsi is_malicious(const char *filename)
```
return (strstr(filename, "nafis") || strstr(filename, "kimcun"));
```
Mengembalikan `1` jika nama file mengandung `nafis` atau `kimcun`

#### Fungsi reverse_name(const char *src, char *dst)
```
for (i = 0; i < len; ++i) dst[i] = src[len - 1 - i];
```
Membalik string, contoh: "nafis.txt" → "txt.sifan"

#### map_path(const char *path, char *real_path)
```
if (is_malicious(rel_path)) {
    reverse_name(rel_path, temp); // pakai nama dibalik
} else {
    temp = rel_path;
}
snprintf(real_path, "%s/%s", src_dir, temp);
```
Mapping nama file di mount point (`/antink_mount`) ke file asli di `src_dir`


#### antink_getattr()
```
lstat(real_path, stbuf);
```
Mengambil metadata file (ukuran, waktu akses, dsb). Ini dipanggil setiap kali pengguna menyentuh file (ls, stat, dsb)

#### antink_readdir()
Membaca isi direktori asli (`src_dir`) dan menampilkan ke mount point dengan nama:
- dibalik jika berbahaya
- asli jika tidak

```
filler(buf, fname, NULL, 0, 0);
```
Menambahkan nama file (asli / dibalik) ke output mount

```
if (is_malicious(de->d_name)) {
    reverse_name -> fname
    write_log("WARNING ...")
}
```

#### antink_open()
Menangani pembukaan file, tidak melakukan operasi berat, hanya log

```
write_log("INFO: Membuka file xxx");
```

#### rot13(char *s)
```
's' → 'f', 'n' → 'a', dsb
```
Enkripsi ROT13: a–z dan A–Z digeser 13 karakter.

#### antink_read()
Membaca isi file. Jika `.txt` dan tidak berbahaya, terapkan ROT13
```
if (strstr(path, ".txt") && !is_malicious(path + 1)) {
    write_log("Membaca dan mengenkripsi ...")
    rot13(buf);
}
```
`path + 1` → menghilangkan '/' di awal (karena FUSE path seperti `/abc.txt`)

#### main()
Validasi argumen, ambil realpath direktori sumber (/it24_host), lalu mount ke mount point (/antink_mount).
```
src_dir = realpath(argv[1], NULL); // ambil path absolut sumber
char *fuse_argv[] = { argv[0], argv[2], "-f" };
return fuse_main(3, fuse_argv, &antink_oper, NULL);
```

## Soal_4
### Oleh: Yuan Banny Albyan

- compile nya: gcc -Wall -O2 -D_POSIX_C_SOURCE=200809L maimai_fs.c -o maimai_fs \
    -lfuse3 -lssl -lcrypto -lz
- run nya: ./maimai_fs
- otomatis membuat
.
├── chiho/           # Direktori output asli (setiap area punya subfolder)
│   ├── starter/
│   ├── metro/
│   ├── dragon/
│   ├── blackrose/
│   ├── heaven/
│   └── youth/
└── fuse_dir/        # Mountpoint FUSE untuk interaksi user
    ├── starter/
    ├── metro/
    ├── dragon/
    ├── blackrose/
    ├── heaven/
    ├── youth/
    └── 7sref/

```c
#define _POSIX_C_SOURCE 200809L
#define FUSE_USE_VERSION 35
```
Menjamin fitur POSIX (misal pread, pwrite).
Memilih versi API FUSE (v3.5).

```c
#include <fuse3/fuse.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <zlib.h>
```
fuse.h: callback filesystem.
openssl: implementasi AES-256-CBC.
zlib: gzip compression.

- alur program:
1. Buat direktori awal (make_dir) untuk output (chiho/) dan mountpoint (fuse_dir/).
2. Buat subfolder sesuai area (starter,metro,…youth) dan 7sref.
3. Scan file existing di fuse_dir dan backup ke chiho/[area]/ tanpa transformasi (7sref).
4. Mount FUSE pada fuse_dir/7sref dengan opsi auto_unmount dan idle_timeout=3.


```c
static void make_dir(const char *path) {
    if (mkdir(path, 0755) == 0)
        printf("✔ Created: %s\n", path);
    else if (errno != EEXIST)
        fprintf(stderr, "✖ Error mkdir %s: %s\n", path, strerror(errno));
}
```
Buat folder root & subfolder tiap area || Cek errno, ignore EEXIST

```c
static void scan_existing() {
    for (int i = 0; i < CAT_COUNT; i++) {
        const char *cat = CATS[i];
        char d[PATH_MAX];
        snprintf(d, sizeof(d), "%s/%s", BASE_FUSE, cat);
        DIR *dp = opendir(d);
        struct dirent *e;
        while (dp && (e = readdir(dp))) {
            if (e->d_type == DT_REG)
                process_file(cat, e->d_name);
        }
        if (dp) closedir(dp);
    }
}
```
Backup file lama tanpa transform || Iterasi tiap subdir & panggil process_file()

```c
static void process_file(const char *cat, const char *fname) {
    char src[PATH_MAX], bak[PATH_MAX], dst[PATH_MAX];
    snprintf(src, sizeof(src), "%s/%s/%s", BASE_FUSE, cat, fname);
    snprintf(bak, sizeof(bak), "%s/%s/%s_%s",
             BASE_FUSE, REF_DIR, cat, fname);
    transform_copy(src, bak, "");
    snprintf(dst, sizeof(dst), "%s/%s/%s%s",
             BASE_CHIHO, cat, fname, get_ext(cat));
    transform_copy(src, dst, cat);
}

```
Soal: implement backup & transform || 2x transform_copy(): backup (.7sref) + area

```c
static int transform_copy(const char *src, const char *dst, const char *cat) {
    int infd = open(src, O_RDONLY);
    if (infd < 0) return -errno;
    int outfd = open(dst, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (outfd < 0) { close(infd); return -errno; }

    char buf[4096];
    ssize_t r;

    if (needs_aes(cat)) {
        unsigned char key[32], iv[16];
        RAND_bytes(key, sizeof(key));
        RAND_bytes(iv, sizeof(iv));
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
        write(outfd, iv, sizeof(iv));

        char outb[4096 + EVP_MAX_BLOCK_LENGTH];
        int outl;
        while ((r = read(infd, buf, sizeof(buf))) > 0) {
            EVP_EncryptUpdate(ctx, (unsigned char*)outb, &outl, (unsigned char*)buf, r);
            if (outl) write(outfd, outb, outl);
        }
        EVP_EncryptFinal_ex(ctx, (unsigned char*)outb, &outl);
        if (outl) write(outfd, outb, outl);
        EVP_CIPHER_CTX_free(ctx);

    } else if (needs_gzip(cat)) {
        FILE *fin = fdopen(infd, "rb");
        gzFile gz = gzdopen(open(dst, O_WRONLY|O_CREAT|O_TRUNC, 0644), "wb");
        while ((r = fread(buf, 1, sizeof(buf), fin)) > 0) gzwrite(gz, buf, r);
        fclose(fin);
        gzclose(gz);

    } else {
        while ((r = read(infd, buf, sizeof(buf))) > 0) {
            if (needs_shift(cat)) shift_buffer(buf, r);
            else if (needs_rot13(cat)) rot13_buffer(buf, r);
            write(outfd, buf, r);
        }
    }

    close(infd);
    close(outfd);
    return 0;
}
```
Soal: manipulasi file sesuai jenis area || Kondisional: shift, ROT13, AES, gzip, atau biasa

```c
static void shift_buffer(char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) buf[i] = (unsigned char)(buf[i] + (i % 256));
}
```
Metropolis (metro): shift byte sesuai indeks modul 256 || Tambah (i % 256)

```c
static void rot13_buffer(char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = buf[i];
        if (c >= 'a' && c <= 'z') buf[i] = 'a' + (c - 'a' + 13) % 26;
        else if (c >= 'A' && c <= 'Z') buf[i] = 'A' + (c - 'A' + 13) % 26;
    }
}
```
Dragon: enkripsi ROT13

```c
if (needs_aes(cat)) {
        unsigned char key[32], iv[16];
        RAND_bytes(key, sizeof(key));
        RAND_bytes(iv, sizeof(iv));
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
        write(outfd, iv, sizeof(iv));

        char outb[4096 + EVP_MAX_BLOCK_LENGTH];
        int outl;
        while ((r = read(infd, buf, sizeof(buf))) > 0) {
            EVP_EncryptUpdate(ctx, (unsigned char*)outb, &outl, (unsigned char*)buf, r);
            if (outl) write(outfd, outb, outl);
        }
        EVP_EncryptFinal_ex(ctx, (unsigned char*)outb, &outl);
        if (outl) write(outfd, outb, outl);
        EVP_CIPHER_CTX_free(ctx);

    }
```
Heaven: enkripsi AES-256-CBC

```c
 else if (needs_gzip(cat)) {
        FILE *fin = fdopen(infd, "rb");
        gzFile gz = gzdopen(open(dst, O_WRONLY|O_CREAT|O_TRUNC, 0644), "wb");
        while ((r = fread(buf, 1, sizeof(buf), fin)) > 0) gzwrite(gz, buf, r);
        fclose(fin);
        gzclose(gz);

    } 
```
Skystreet: kompresi gzip || zlib: gzwrite()

```c
static int fs_release(const char *path, struct fuse_file_info *fi) {
    close(fi->fh);
    // setelah file ditutup, proses backup+transform
    size_t b = strlen(BASE_FUSE);
    if (!strncmp(path+1, BASE_FUSE, b)) {
        char tmp[PATH_MAX];
        strcpy(tmp, path+1+b+1);
        char *cat = strtok(tmp, "/");
        char *f   = strtok(NULL, "/");
        if (cat && f) process_file(cat, f);
    }
    return 0;
}
```
Trigger backup & transform setelah file ditutup
Cek path, parse area & nama file








