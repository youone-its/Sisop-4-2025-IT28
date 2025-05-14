/*
 * maimai_fs.c
 *
 * FUSE-based filesystem implementing full FS operations + intercept create/release
 * without changing existing logic or functionality.
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
 
 // Helpers
 static void make_dir(const char *path) {
     if (mkdir(path, 0755) == 0)
         printf("✔ Created: %s\n", path);
     else if (errno != EEXIST)
         fprintf(stderr, "✖ Error mkdir %s: %s\n", path, strerror(errno));
 }
 
 static const char* get_ext(const char *cat) {
     if (strcmp(cat, "starter") == 0)   return ".mai";
     if (strcmp(cat, "metro") == 0)     return ".ccc";
     if (strcmp(cat, "dragon") == 0)    return ".rot";
     if (strcmp(cat, "blackrose") == 0) return ".bin";
     if (strcmp(cat, "heaven") == 0)    return ".enc";
     if (strcmp(cat, "skystreet") == 0) return ".gz";
     return "";
 }
 static int needs_shift(const char *cat)   { return strcmp(cat, "metro")==0; }
 static int needs_rot13(const char *cat)   { return strcmp(cat, "dragon")==0; }
 static int needs_aes(const char *cat)     { return strcmp(cat, "heaven")==0; }
 static int needs_gzip(const char *cat)    { return strcmp(cat, "skystreet")==0; }
 
 static void shift_buffer(char *buf, size_t len) {
     for (size_t i=0;i<len;i++) buf[i] = (unsigned char)(buf[i] + (i % 256));
 }
 static void rot13_buffer(char *buf, size_t len) {
     for (size_t i=0;i<len;i++) {
         unsigned char c=buf[i];
         if (c>='a'&&c<='z') buf[i] = 'a'+(c-'a'+13)%26;
         else if (c>='A'&&c<='Z') buf[i] = 'A'+(c-'A'+13)%26;
     }
 }
 
 static int transform_copy(const char *src, const char *dst, const char *cat) {
     int infd=open(src,O_RDONLY); if (infd<0) return -errno;
     int outfd=open(dst,O_WRONLY|O_CREAT|O_TRUNC,0644); if(outfd<0){close(infd);return -errno;}
     char buf[4096]; ssize_t r;
     if (needs_aes(cat)) {
         unsigned char key[32],iv[16];
         RAND_bytes(key,sizeof(key)); RAND_bytes(iv,sizeof(iv));
         EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new();
         EVP_EncryptInit_ex(ctx,EVP_aes_256_cbc(),NULL,key,iv);
         write(outfd,iv,sizeof(iv));
         char outb[4096+EVP_MAX_BLOCK_LENGTH]; int outl;
         while((r=read(infd,buf,sizeof(buf)))>0){
             EVP_EncryptUpdate(ctx,(unsigned char*)outb,&outl,(unsigned char*)buf,r);
             write(outfd,outl?outb:NULL,outl);
         }
         EVP_EncryptFinal_ex(ctx,(unsigned char*)outb,&outl);
         write(outfd,outl?outb:NULL,outl);
         EVP_CIPHER_CTX_free(ctx);
     } else if(needs_gzip(cat)) {
         FILE *fin=fdopen(infd,"rb");
         gzFile gz=gzdopen(open(dst,O_WRONLY|O_CREAT|O_TRUNC,0644),"wb");
         while((r=fread(buf,1,sizeof(buf),fin))>0) gzwrite(gz,buf,r);
         fclose(fin); gzclose(gz);
     } else {
         while((r=read(infd,buf,sizeof(buf)))>0) {
             if(needs_shift(cat)) shift_buffer(buf,r);
             else if(needs_rot13(cat)) rot13_buffer(buf,r);
             write(outfd,buf,r);
         }
     }
     close(infd); close(outfd);
     return 0;
 }
 
 static void process_file(const char *cat, const char *fname) {
     char src[PATH_MAX],path7[PATH_MAX],dst[PATH_MAX];
     snprintf(src,sizeof(src),"%s/%s/%s",BASE_FUSE,cat,fname);
     snprintf(path7,sizeof(path7),"%s/%s/%s_%s",BASE_FUSE,REF_DIR,cat,fname);
     transform_copy(src,path7,"");
     snprintf(dst,sizeof(dst),"%s/%s/%s%s",BASE_CHIHO,cat,fname,get_ext(cat));
     transform_copy(src,dst,cat);
 }
 
 static void scan_existing(){
     for(int i=0;i<CAT_COUNT;i++){
         const char*cat=CATS[i]; char d[PATH_MAX];
         snprintf(d,sizeof(d),"%s/%s",BASE_FUSE,cat);
         DIR*dp=opendir(d); struct dirent*e;
         while(dp&&(e=readdir(dp))) if(e->d_type==DT_REG) process_file(cat,e->d_name);
         if(dp) closedir(dp);
     }
 }
 
 // Map path under mountpoint to real path in fuse_dir
 static void fullpath(char fpath[PATH_MAX], const char *path){
     snprintf(fpath, PATH_MAX, "%s%s", BASE_FUSE, path);
 }
 
 // FUSE operations
 static int fs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi){
     (void) fi;
     char fpath[PATH_MAX]; fullpath(fpath,path);
     int res = lstat(fpath, stbuf);
     return res==-1 ? -errno : 0;
 }
 
 static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags){
     DIR *dp; struct dirent *de;
     char fpath[PATH_MAX]; fullpath(fpath,path);
     dp = opendir(fpath);
     if (!dp) return -errno;
     while ((de = readdir(dp)) != NULL) {
         struct stat st;
         memset(&st,0,sizeof(st));
         st.st_ino = de->d_ino;
         st.st_mode = de->d_type << 12;
         filler(buf, de->d_name, &st, 0, 0);
     }
     closedir(dp);
     return 0;
 }
 
 static int fs_open(const char *path, struct fuse_file_info *fi){
     char fpath[PATH_MAX]; fullpath(fpath,path);
     int fd = open(fpath, fi->flags);
     return fd<0 ? -errno : (fi->fh = fd, 0);
 }
 
 static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
     (void) path;
     int res = pread(fi->fh, buf, size, offset);
     return res<0 ? -errno : res;
 }
 
 static int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi){
     (void) path;
     int res = pwrite(fi->fh, buf, size, offset);
     return res<0 ? -errno : res;
 }
 
 static int fs_truncate(const char *path, off_t size, struct fuse_file_info *fi){
     char fpath[PATH_MAX]; fullpath(fpath,path);
     int res = truncate(fpath, size);
     return res==-1 ? -errno : 0;
 }
 
 static int fs_unlink(const char *path){
     char fpath[PATH_MAX]; fullpath(fpath,path);
     int res = unlink(fpath);
     return res==-1 ? -errno : 0;
 }
 
 static int fs_mkdir(const char *path, mode_t mode){
     char fpath[PATH_MAX]; fullpath(fpath,path);
     int res = mkdir(fpath, mode);
     return res==-1 ? -errno : 0;
 }
 
 static int fs_rmdir(const char *path){
     return fs_unlink(path);
 }
 
 static int fs_rename(const char *from, const char *to, unsigned int flags){
     (void) flags;
     char ffrom[PATH_MAX], fto[PATH_MAX];
     fullpath(ffrom, from);
     fullpath(fto, to);
     int res = rename(ffrom, fto);
     return res==-1 ? -errno : 0;
 }
 
 static int fs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi){
     (void) fi;
     char fpath[PATH_MAX]; fullpath(fpath,path);
     int res = utimensat(0, fpath, tv, AT_SYMLINK_NOFOLLOW);
     return res==-1 ? -errno : 0;
 }
 
 static int fs_statfs(const char *path, struct statvfs *stbuf){
     char fpath[PATH_MAX]; fullpath(fpath,path);
     int res = statvfs(fpath, stbuf);
     return res==-1 ? -errno : 0;
 }
 
 // Intercept create & release as before
 static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi){
     char fpath[PATH_MAX]; fullpath(fpath,path);
     int fd = open(fpath, fi->flags, mode);
     if (fd<0) return -errno;
     fi->fh = fd;
     return 0;
 }
 
 static int fs_release(const char *path, struct fuse_file_info *fi){
     close(fi->fh);
     // extract category & filename
     size_t b = strlen(BASE_FUSE);
     if (!strncmp(path+1, BASE_FUSE, b)) {
         char tmp[PATH_MAX]; strcpy(tmp, path+1+b+1);
         char *cat = strtok(tmp, "/");
         char *f   = strtok(NULL, "/");
         if (cat && f) process_file(cat, f);
     }
     return 0;
 }
 
 static struct fuse_operations ops = {
     .getattr  = fs_getattr,
     .readdir  = fs_readdir,
     .open     = fs_open,
     .read     = fs_read,
     .write    = fs_write,
     .truncate = fs_truncate,
     .unlink   = fs_unlink,
     .mkdir    = fs_mkdir,
     .rmdir    = fs_rmdir,
     .rename   = fs_rename,
     .utimens  = fs_utimens,
     .statfs   = fs_statfs,
     .create   = fs_create,
     .release  = fs_release
 };
 
 int main(int argc, char *argv[]) {
     const char *mountpoint = "mnt";
     struct stat st = {0};
     if (stat(mountpoint, &st) == -1) mkdir(mountpoint, 0755);
     make_dir(BASE_CHIHO);
     make_dir(BASE_FUSE);
     for (int i=0;i<CAT_COUNT;i++) {
         char p1[PATH_MAX], p2[PATH_MAX];
         snprintf(p1,sizeof(p1), "%s/%s", BASE_CHIHO, CATS[i]); make_dir(p1);
         snprintf(p2,sizeof(p2), "%s/%s", BASE_FUSE, CATS[i]); make_dir(p2);
     }
     char r[PATH_MAX]; snprintf(r,sizeof(r), "%s/%s", BASE_FUSE, REF_DIR); make_dir(r);
     scan_existing();
     char *fuse_argv[] = { argv[0], (char*)mountpoint, "-o", "auto_unmount", "-o", "idle_timeout=3", NULL };
     int res = fuse_main(6, fuse_argv, &ops, NULL);
     rmdir(mountpoint);
     return res;
 }
 
