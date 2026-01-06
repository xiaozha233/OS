/*
 * file.h 定义了文件描述符结构体（struct file）以及相关的操作函数。
 * 在 ucore 中，每个进程都有一个文件描述符表，用于管理该进程打开的文件。
 * struct file 记录了文件的打开状态、读写权限、当前偏移量以及指向 inode 的指针。
 * 此文件还声明了内核层面的文件操作接口，如 file_open, file_read, file_write 等。
 */

#ifndef __KERN_FS_FILE_H__
#define __KERN_FS_FILE_H__

//#include <types.h>
#include <fs.h>
#include <proc.h>
#include <atomic.h>
#include <assert.h>

struct inode;
struct stat;
struct dirent;

struct file {
    enum {
        FD_NONE, FD_INIT, FD_OPENED, FD_CLOSED,
    } status;                   // 文件状态
    bool readable;              // 是否可读
    bool writable;              // 是否可写
    int fd;                     // 文件描述符
    off_t pos;                  // 当前读写位置
    struct inode *node;         // 指向的 inode
    int open_count;             // 打开计数
};

void fd_array_init(struct file *fd_array);
void fd_array_open(struct file *file);
void fd_array_close(struct file *file);
void fd_array_dup(struct file *to, struct file *from);
bool file_testfd(int fd, bool readable, bool writable);

int file_open(char *path, uint32_t open_flags);
int file_close(int fd);
int file_read(int fd, void *base, size_t len, size_t *copied_store);
int file_write(int fd, void *base, size_t len, size_t *copied_store);
int file_seek(int fd, off_t pos, int whence);
int file_fstat(int fd, struct stat *stat);
int file_fsync(int fd);
int file_getdirentry(int fd, struct dirent *dirent);
int file_dup(int fd1, int fd2);
int file_pipe(int fd[]);
int file_mkfifo(const char *name, uint32_t open_flags);

static inline int
fopen_count(struct file *file) {
    return file->open_count;
}

static inline int
fopen_count_inc(struct file *file) {
    file->open_count += 1;
    return file->open_count;
}

static inline int
fopen_count_dec(struct file *file) {
    file->open_count -= 1;
    return file->open_count;
}

#endif /* !__KERN_FS_FILE_H__ */

