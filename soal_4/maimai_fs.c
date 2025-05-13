/*
 * maimai_fs.c
 * 
 * FUSE-based filesystem tool implementing:
 *   - Directory creation: mountpoint (mnt), chiho/, fuse_dir/ (with subfolders), fuse_dir/7sref
 *   - On startup: scan existing files in fuse_dir/<category>/ and process each
 *   - Runtime: intercept file creation in mountpoint/<category>/ and process new files
 *
 * Processing steps for each file fuse_dir/<cat>/<fname>:
 *   1. Copy raw to fuse_dir/7sref/<cat>_<fname>
 *   2. Copy transformed to chiho/<cat>/<fname>.<ext>
 *
 * Transformation rules:
 *   starter   -> .mai   (no transform)
 *   metro     -> .ccc   (+i mod 256 shift)
 *   dragon    -> .rot   (ROT13 text)
 *   blackrose -> .bin   (raw binary)
 *   heaven    -> .enc   (AES-256-CBC via OpenSSL; IV prepended)
 *   youth     -> .mai   (no transform)
 *   skystreet -> .gz    (gzip via zlib)
 *
 * Compile:
 *   gcc maimai_fs.c $(pkg-config fuse3 openssl zlib --cflags --libs) -o maimai_fs
 *
 * Usage:
 *   ./maimai_fs <mountpoint>
 *   # in another shell:
 *   echo "..." > <mountpoint>/metro/file.txt
 *   cp sample.png <mountpoint>/blackrose/img.png
 */

 #define FUSE_USE_VERSION 35
 #include <fuse3/fuse.h>
 #include <stdio.h>
 #include <string.h>
 #include <errno.h>
 #include <stdlib.h>
 #include <fcntl.h>
 #include <unistd.h>
 #include <sys/stat.h>
 #include <limits.h>
 #include <dirent.h>
 #include <openssl/evp.h>
 #include <openssl/rand.h>
 #include <zlib.h>
 
 static const char *BASE_CHIHO = "chiho";
 static const char *BASE_FUSE = "fuse_dir";
 static const char *REF_DIR   = "7sref";
 static const char *CATS[] = {"starter","metro","dragon","blackrose","heaven","skystreet"};
 static const int CAT_COUNT = sizeof(CATS)/sizeof(CATS[0]);
 
 void make_dir(const char *path) {
     if (mkdir(path, 0755) == 0)
         printf("✔ Created: %s\n", path);
     else if (errno != EEXIST)
         fprintf(stderr, "✖ Error mkdir %s: %s\n", path, strerror(errno));
 }
 
 const char* get_ext(const char *cat) {
     if (strcmp(cat, "starter") == 0)   return ".mai";
     if (strcmp(cat, "metro") == 0)     return ".ccc";
     if (strcmp(cat, "dragon") == 0)    return ".rot";
     if (strcmp(cat, "blackrose") == 0) return ".bin";
     if (strcmp(cat, "heaven") == 0)    return ".enc";
     if (strcmp(cat, "skystreet") == 0) return ".gz";
     return "";
 }
 
 int needs_shift(const char *cat)   { return strcmp(cat, "metro")==0; }
 int needs_rot13(const char *cat)   { return strcmp(cat, "dragon")==0; }
 int needs_aes(const char *cat)     { return strcmp(cat, "heaven")==0; }
 int needs_gzip(const char *cat)    { return strcmp(cat, "skystreet")==0; }
 
 void shift_buffer(char *buf, size_t len) {
     for (size_t i=0; i<len; i++)
         buf[i] = (unsigned char)(buf[i] + (i % 256));
 }
 
 void rot13_buffer(char *buf, size_t len) {
     for (size_t i=0; i<len; i++) {
         unsigned char c = buf[i];
         if (c>='a'&&c<='z') buf[i] = 'a' + (c-'a'+13)%26;
         else if (c>='A'&&c<='Z') buf[i] = 'A' + (c-'A'+13)%26;
     }
 }
 
 int transform_copy(const char *src, const char *dst, const char *cat) {
     int infd = open(src, O_RDONLY);
     if (infd<0) return -errno;
     int outfd = open(dst, O_WRONLY|O_CREAT|O_TRUNC, 0644);
     if (outfd<0) { close(infd); return -errno; }
 
     char buf[4096]; ssize_t r;
     // For AES, handle separately
     if (needs_aes(cat)) {
         unsigned char key[32], iv[16];
         RAND_bytes(key, sizeof(key)); RAND_bytes(iv, sizeof(iv));
         EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
         EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv);
         // write IV first
         write(outfd, iv, sizeof(iv));
         char outb[4096+EVP_MAX_BLOCK_LENGTH]; int outl;
         while ((r=read(infd,buf,sizeof(buf)))>0) {
             EVP_EncryptUpdate(ctx,(unsigned char*)outb,&outl,(unsigned char*)buf,r);
             write(outfd,outb,outl);
         }
         EVP_EncryptFinal_ex(ctx,(unsigned char*)outb,&outl);
         write(outfd,outb,outl);
         EVP_CIPHER_CTX_free(ctx);
     } else if(needs_gzip(cat)) {
         FILE *fin = fdopen(infd, "rb");
         gzFile gz = gzdopen(open(dst,O_WRONLY|O_CREAT|O_TRUNC,0644), "wb");
         while ((r=fread(buf,1,sizeof(buf),fin))>0) gzwrite(gz,buf,r);
         fclose(fin); gzclose(gz);
     } else {
         while ((r=read(infd,buf,sizeof(buf)))>0) {
             if (needs_shift(cat)) shift_buffer(buf,r);
             else if(needs_rot13(cat)) rot13_buffer(buf,r);
             write(outfd,buf,r);
         }
     }
     close(infd); close(outfd);
     return 0;
 }
 
 void process_file(const char *cat, const char *fname) {
     char src[PATH_MAX], path7[PATH_MAX], dst[PATH_MAX];
     snprintf(src, sizeof(src), "%s/%s/%s", BASE_FUSE, cat, fname);
     // raw to 7sref
     snprintf(path7,sizeof(path7), "%s/%s/%s_%s", BASE_FUSE, REF_DIR, cat, fname);
     transform_copy(src,path7,"");
     // transformed to chiho
     snprintf(dst,sizeof(dst), "%s/%s/%s%s",
              BASE_CHIHO, cat, fname, get_ext(cat));
     transform_copy(src,dst,cat);
     printf("Processed %s/%s -> %s\n", cat,fname,dst);
 }
 
 void scan_existing(){
     for(int i=0;i<CAT_COUNT;i++){
         const char*cat=CATS[i]; char d[PATH_MAX];
         snprintf(d,sizeof(d),"%s/%s",BASE_FUSE,cat);
         DIR*dp=opendir(d); struct dirent*e;
         while(dp&&(e=readdir(dp))) if(e->d_type==DT_REG) process_file(cat,e->d_name);
         if(dp) closedir(dp);
     }
 }
 
 static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi){
     int fd=open(path+1,fi->flags,mode);
     if(fd<0) return -errno; fi->fh=fd; return 0;
 }
 
 static int fs_release(const char *path, struct fuse_file_info *fi){
     close(fi->fh);
     size_t b=strlen(BASE_FUSE);
     if(!strncmp(path+1,BASE_FUSE,b)&&path[1+b]=='/'){
         char tmp[PATH_MAX]; strcpy(tmp,path+1+b+1);
         char *cat=strtok(tmp,"/"),*f=strtok(NULL,"/");
         if(cat&&f) process_file(cat,f);
     }
     return 0;
 }
 
 static struct fuse_operations ops={.create=fs_create,.release=fs_release};


int main(int argc, char *argv[]) {
    const char *mountpoint = "mnt";

    // 1. Buat folder mnt
    struct stat st = {0};
    if (stat(mountpoint, &st) == -1) {
        if (mkdir(mountpoint, 0755) == 0)
            printf("✔ Created: %s\n", mountpoint);
        else {
            perror("✖ Failed to create mountpoint");
            return 1;
        }
    } else if (S_ISDIR(st.st_mode)) {
        printf("ℹ Reusing existing mountpoint: %s\n", mountpoint);
    } else {
        fprintf(stderr, "✖ %s exists but is not a directory\n", mountpoint);
        return 1;
    }
    

    // 2. Buat struktur folder
    make_dir(BASE_CHIHO);
    make_dir(BASE_FUSE);
    for (int i = 0; i < CAT_COUNT; i++) {
        char p1[PATH_MAX], p2[PATH_MAX];
        snprintf(p1, sizeof(p1), "%s/%s", BASE_CHIHO, CATS[i]); make_dir(p1);
        snprintf(p2, sizeof(p2), "%s/%s", BASE_FUSE, CATS[i]); make_dir(p2);
    }

    char r[PATH_MAX];
    snprintf(r, sizeof(r), "%s/%s", BASE_FUSE, REF_DIR); make_dir(r);

    printf("\n✅ Setup done. Scanning...\n");
    scan_existing();
    printf("🔧 Mounting FUSE at %s...\n", mountpoint);

    // 3. Jalankan fuse_main
    char *fuse_argv[] = {
        argv[0],
        (char *)mountpoint,
        "-o", "auto_unmount",
        "-o", "idle_timeout=3",
        NULL
    };
    int fuse_argc = sizeof(fuse_argv)/sizeof(fuse_argv[0]) - 1;
    int res = fuse_main(fuse_argc, fuse_argv, &ops, NULL);
    if (rmdir(mountpoint) == 0)
        printf("");
    else
        perror("✖ Failed to remove mnt");
    return res;

    return res;

}
