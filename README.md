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