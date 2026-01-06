#include <defs.h>
#include <string.h>
#include <vmm.h>
#include <proc.h>
#include <kmalloc.h>
#include <vfs.h>
#include <file.h>
#include <iobuf.h>
#include <sysfile.h>
#include <stat.h>
#include <dirent.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>

#define IOBUF_SIZE                          4096

/* copy_path - copy path name */
// copy_path - 复制路径名
static int
copy_path(char **to, const char *from) {
    struct mm_struct *mm = current->mm; // 获取当前进程的内存管理结构
    char *buffer;
    if ((buffer = kmalloc(FS_MAX_FPATH_LEN + 1)) == NULL) { // 分配缓冲区用于存储路径
        return -E_NO_MEM; // 如果内存分配失败，返回内存不足错误
    }
    lock_mm(mm); // 锁定内存管理结构，保护用户空间内存访问
    if (!copy_string(mm, buffer, from, FS_MAX_FPATH_LEN + 1)) { // 从用户空间复制字符串到内核空间
        unlock_mm(mm); // 如果复制失败，解锁内存管理结构
        goto failed_cleanup; // 跳转到错误处理
    }
    unlock_mm(mm); // 复制成功，解锁内存管理结构
    *to = buffer; // 将分配并填充的缓冲区指针赋值给输出参数
    return 0; // 返回成功

failed_cleanup:
    kfree(buffer); // 释放已分配的缓冲区
    return -E_INVAL; // 返回无效参数错误
}

/* sysfile_open - open file */
// sysfile_open - 打开文件
int
sysfile_open(const char *__path, uint32_t open_flags) {
    int ret;
    char *path;
    if ((ret = copy_path(&path, __path)) != 0) { // 将用户空间的路径复制到内核空间
        return ret; // 如果复制失败，直接返回错误码
    }
    ret = file_open(path, open_flags); // 调用文件系统接口打开文件
    kfree(path); // 释放内核空间的路径缓冲区
    return ret; // 返回打开文件的结果
}

/* sysfile_close - close file */
// sysfile_close - 关闭文件
int
sysfile_close(int fd) {
    return file_close(fd); // 直接调用文件系统接口关闭文件描述符
}

/* sysfile_read - read file */
// sysfile_read - 读取文件
int
sysfile_read(int fd, void *base, size_t len) {
    struct mm_struct *mm = current->mm; // 获取当前进程的内存管理结构
    if (len == 0) { // 如果读取长度为0
        return 0; // 直接返回0
    }
    if (!file_testfd(fd, 1, 0)) { // 检查文件描述符是否可读
        return -E_INVAL; // 如果不可读，返回无效参数错误
    }
    void *buffer;
    if ((buffer = kmalloc(IOBUF_SIZE)) == NULL) { // 分配内核缓冲区用于临时存储读取的数据
        return -E_NO_MEM; // 如果内存分配失败，返回内存不足错误
    }

    int ret = 0;
    size_t copied = 0, alen;
    while (len != 0) { // 循环读取直到满足请求的长度
        if ((alen = IOBUF_SIZE) > len) { // 计算本次循环读取的长度，不能超过缓冲区大小
            alen = len;
        }
        ret = file_read(fd, buffer, alen, &alen); // 从文件读取数据到内核缓冲区
        if (alen != 0) { // 如果实际读取到了数据
            lock_mm(mm); // 锁定内存管理结构
            {
                if (copy_to_user(mm, base, buffer, alen)) { // 将数据从内核缓冲区复制到用户空间缓冲区
                    assert(len >= alen); // 断言剩余需要的长度大于等于已读取的长度
                    base += alen, len -= alen, copied += alen; // 更新用户缓冲区指针、剩余长度和总已复制长度
                }
                else if (ret == 0) { // 如果复制到用户空间失败且之前文件读取成功
                    ret = -E_INVAL; // 设置因为用户内存问题导致的无效参数错误
                }
            }
            unlock_mm(mm); // 解锁内存管理结构
        }
        if (ret != 0 || alen == 0) { // 如果文件读取出错或者读到了文件末尾（读取长度为0）
            goto out; // 跳转到退出处理
        }
    }

out:
    kfree(buffer); // 释放内核缓冲区
    if (copied != 0) { // 如果已经成功读取并复制了一些数据
        return copied; // 返回实际读取的字节数
    }
    return ret; // 否则返回错误码
}

/* sysfile_write - write file */
// sysfile_write - 写文件
int
sysfile_write(int fd, void *base, size_t len) {
    struct mm_struct *mm = current->mm; // 获取当前进程的内存管理结构
    if (len == 0) { // 如果写入长度为0
        return 0; // 直接返回0
    }
    if (!file_testfd(fd, 0, 1)) { // 检查文件描述符是否可写
        return -E_INVAL; // 如果不可写，返回无效参数错误
    }
    void *buffer;
    if ((buffer = kmalloc(IOBUF_SIZE)) == NULL) { // 分配内核缓冲区
        return -E_NO_MEM; // 如果内存分配失败，返回内存不足错误
    }

    int ret = 0;
    size_t copied = 0, alen;
    while (len != 0) { // 循环写入直到完成请求
        if ((alen = IOBUF_SIZE) > len) { // 计算本次处理的数据长度
            alen = len;
        }
        lock_mm(mm); // 锁定内存管理结构
        {
            if (!copy_from_user(mm, buffer, base, alen, 0)) { // 从用户空间复制数据到内核缓冲区
                ret = -E_INVAL; // 如果复制失败，设置错误码
            }
        }
        unlock_mm(mm); // 解锁内存管理结构
        if (ret == 0) { // 如果数据复制成功
            ret = file_write(fd, buffer, alen, &alen); // 将内核缓冲区的数据写入文件
            if (alen != 0) { // 如果实际写入了数据
                assert(len >= alen); // 断言剩余长度合法
                base += alen, len -= alen, copied += alen; // 更新指针和计数器
            }
        }
        if (ret != 0 || alen == 0) { // 如果出错或没写入任何数据
            goto out; // 跳转到退出
        }
    }

out:
    kfree(buffer); // 释放内核缓冲区
    if (copied != 0) { // 如果有成功写入的数据
        return copied; // 返回已写入的字节数
    }
    return ret; // 否则返回错误码
}

/* sysfile_seek - seek file */
// sysfile_seek - 定位文件读写位置
int
sysfile_seek(int fd, off_t pos, int whence) {
    return file_seek(fd, pos, whence); // 直接调用底层文件系统接口进行seek操作
}

/* sysfile_fstat - stat file */
// sysfile_fstat - 获取文件状态信息
int
sysfile_fstat(int fd, struct stat *__stat) {
    struct mm_struct *mm = current->mm; // 获取当前进程的内存管理结构
    int ret;
    struct stat __local_stat, *stat = &__local_stat; // 在内核栈上分配临时stat结构
    if ((ret = file_fstat(fd, stat)) != 0) { // 获取文件状态到内核临时变量
        return ret; // 如果失败返回错误
    }

    lock_mm(mm); // 锁定内存管理结构
    {
        if (!copy_to_user(mm, __stat, stat, sizeof(struct stat))) { // 将内核的stat结构复制到用户空间
            ret = -E_INVAL; // 如果复制失败，返回无效参数错误
        }
    }
    unlock_mm(mm); // 解锁
    return ret; // 返回结果
}

/* sysfile_fsync - sync file */
// sysfile_fsync - 同步文件数据到磁盘
int
sysfile_fsync(int fd) {
    return file_fsync(fd); // 直接调用底层fsync接口
}

/* sysfile_chdir - change dir */
// sysfile_chdir - 改变当前工作目录
int
sysfile_chdir(const char *__path) {
    int ret;
    char *path;
    if ((ret = copy_path(&path, __path)) != 0) { // 将路径从用户空间复制到内核空间
        return ret; // 复制失败返回错误
    }
    ret = vfs_chdir(path); // 调用VFS接口改变当前目录
    kfree(path); // 释放内核路径缓冲区
    return ret;
}

/* sysfile_link - link file */
// sysfile_link - 创建硬链接
int
sysfile_link(const char *__path1, const char *__path2) {
    int ret;
    char *old_path, *new_path;
    if ((ret = copy_path(&old_path, __path1)) != 0) { // 复制源路径
        return ret;
    }
    if ((ret = copy_path(&new_path, __path2)) != 0) { // 复制目标路径
        kfree(old_path); // 如果失败释放源路径
        return ret;
    }
    ret = vfs_link(old_path, new_path); // 调用VFS接口创建链接
    kfree(old_path), kfree(new_path); // 释放两个路径缓冲区
    return ret;
}

/* sysfile_rename - rename file */
// sysfile_rename - 重命名文件
int
sysfile_rename(const char *__path1, const char *__path2) {
    int ret;
    char *old_path, *new_path;
    if ((ret = copy_path(&old_path, __path1)) != 0) { // 复制旧路径
        return ret;
    }
    if ((ret = copy_path(&new_path, __path2)) != 0) { // 复制新路径
        kfree(old_path); // 失败释放旧路径
        return ret;
    }
    ret = vfs_rename(old_path, new_path); // 调用VFS重命名接口
    kfree(old_path), kfree(new_path); // 释放路径缓冲区
    return ret;
}

/* sysfile_unlink - unlink file */
// sysfile_unlink - 删除文件链接（删除文件）
int
sysfile_unlink(const char *__path) {
    int ret;
    char *path;
    if ((ret = copy_path(&path, __path)) != 0) { // 复制路径
        return ret;
    }
    ret = vfs_unlink(path); // 调用VFS接口删除文件
    kfree(path); // 释放路径缓冲区
    return ret;
}

/* sysfile_get cwd - get current working directory */
// sysfile_getcwd - 获取当前工作目录
int
sysfile_getcwd(char *buf, size_t len) {
    struct mm_struct *mm = current->mm; // 获取当前进程内存管理结构
    if (len == 0) {
        return -E_INVAL; // 长度为0返回无效参数
    }

    int ret = -E_INVAL;
    lock_mm(mm); // 锁定内存
    {
        if (user_mem_check(mm, (uintptr_t)buf, len, 1)) { // 检查用户提供的缓冲区是否可写
            struct iobuf __iob, *iob = iobuf_init(&__iob, buf, len, 0); // 初始化iobuf结构封装用户缓冲区
            ret = vfs_getcwd(iob); // 调用VFS接口获取CWD写入iobuf
        }
    }
    unlock_mm(mm); // 解锁
    return ret;
}

/* sysfile_getdirentry - get the file entry in DIR */
// sysfile_getdirentry - 获取目录中的文件项
int
sysfile_getdirentry(int fd, struct dirent *__direntp) {
    struct mm_struct *mm = current->mm; // 获取当前进程内存结构
    struct dirent *direntp;
    if ((direntp = kmalloc(sizeof(struct dirent))) == NULL) { // 在内核分配dirent结构
        return -E_NO_MEM;
    }

    int ret = 0;
    lock_mm(mm); // 锁定内存
    {
        // 从用户结构中复制offset字段到内核dirent，以便知道读哪里
        if (!copy_from_user(mm, &(direntp->offset), &(__direntp->offset), sizeof(direntp->offset), 1)) {
            ret = -E_INVAL;
        }
    }
    unlock_mm(mm);

    if (ret != 0 || (ret = file_getdirentry(fd, direntp)) != 0) { // 调用文件接口读取目录项
        goto out;
    }

    lock_mm(mm);
    {
        // 将读取到的完整dirent结构复制回用户空间
        if (!copy_to_user(mm, __direntp, direntp, sizeof(struct dirent))) {
            ret = -E_INVAL;
        }
    }
    unlock_mm(mm);

out:
    kfree(direntp); // 释放内核dirent结构
    return ret;
}

/* sysfile_dup -  duplicate fd1 to fd2 */
// sysfile_dup - 复制文件描述符
int
sysfile_dup(int fd1, int fd2) {
    return file_dup(fd1, fd2); // 调用文件接口复制文件描述符
}

// sysfile_pipe - 创建管道（未实现）
int
sysfile_pipe(int *fd_store) {
    return -E_UNIMP; // 返回未实现错误
}

// sysfile_mkfifo - 创建命名管道（未实现）
int
sysfile_mkfifo(const char *__name, uint32_t open_flags) {
    return -E_UNIMP; // 返回未实现错误
}

