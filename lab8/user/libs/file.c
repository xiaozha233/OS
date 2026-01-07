#include <defs.h>
#include <string.h>
#include <syscall.h>
#include <stdio.h>
#include <stat.h>
#include <error.h>
#include <unistd.h>

// open - 打开文件
int
open(const char *path, uint32_t open_flags) {
    return sys_open(path, open_flags); // 调用sys_open系统调用
}

// close - 关闭文件
int
close(int fd) {
    return sys_close(fd); // 调用sys_close系统调用
}

// read - 读取文件
int
read(int fd, void *base, size_t len) {
    // 调用sys_read系统调用，跳转到syscall.c的sys_read
    return sys_read(fd, base, len); 
}

// write - 写入文件
int
write(int fd, void *base, size_t len) {
    return sys_write(fd, base, len); // 调用sys_write系统调用
}

// seek - 定位文件读写位置
int
seek(int fd, off_t pos, int whence) {
    return sys_seek(fd, pos, whence); // 调用sys_seek系统调用
}

// fstat - 获取文件状态
int
fstat(int fd, struct stat *stat) {
    return sys_fstat(fd, stat); // 调用sys_fstat系统调用
}

// fsync - 同步文件到磁盘
int
fsync(int fd) {
    return sys_fsync(fd); // 调用sys_fsync系统调用
}

// dup2 - 复制文件描述符
int
dup2(int fd1, int fd2) {
    return sys_dup(fd1, fd2); // 调用sys_dup系统调用
}

// transmode - 转换文件模式为字符标识
static char
transmode(struct stat *stat) {
    uint32_t mode = stat->st_mode;
    if (S_ISREG(mode)) return 'r'; // 普通文件
    if (S_ISDIR(mode)) return 'd'; // 目录
    if (S_ISLNK(mode)) return 'l'; // 符号链接
    if (S_ISCHR(mode)) return 'c'; // 字符设备
    if (S_ISBLK(mode)) return 'b'; // 块设备
    return '-'; // 未知
}

// print_stat - 打印文件状态信息
void
print_stat(const char *name, int fd, struct stat *stat) {
    cprintf("[%03d] %s\n", fd, name); // 打印文件描述符和文件名
    cprintf("    mode    : %c\n", transmode(stat)); // 打印文件类型
    cprintf("    links   : %lu\n", stat->st_nlinks); // 打印硬链接数
    cprintf("    blocks  : %lu\n", stat->st_blocks); // 打印占用块数
    cprintf("    size    : %lu\n", stat->st_size); // 打印文件大小
}

