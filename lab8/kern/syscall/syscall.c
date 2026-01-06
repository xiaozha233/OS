#include <unistd.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <assert.h>
#include <clock.h>
#include <sysfile.h>
// sys_exit - 进程退出系统调用
static int
sys_exit(uint64_t arg[])
{
    int error_code = (int)arg[0]; // 获取错误码
    return do_exit(error_code); // 调用内核处理函数退出进程
}

// sys_fork - 创建子进程系统调用
static int
sys_fork(uint64_t arg[])
{
    struct trapframe *tf = current->tf; // 获取当前进程的trapframe
    uintptr_t stack = tf->gpr.sp; // 获取当前栈指针
    return do_fork(0, stack, tf); // 调用do_fork创建子进程
}

// sys_wait - 等待子进程退出系统调用
static int
sys_wait(uint64_t arg[])
{
    int pid = (int)arg[0]; // 获取要等待的子进程PID
    int *store = (int *)arg[1]; // 获取存储退出码的地址
    return do_wait(pid, store); // 调用do_wait等待子进程
}

// sys_exec - 执行新程序系统调用
static int
sys_exec(uint64_t arg[])
{
    const char *name = (const char *)(arg[0]); // 获取程序名
    int argc = (int)arg[1]; // 获取参数个数
    const char **argv = (const char **)arg[2]; // 获取参数列表
    return do_execve(name, argc, argv); // 调用do_execve加载并执行新程序
}

// sys_yield - 让出CPU系统调用
static int
sys_yield(uint64_t arg[])
{
    return do_yield(); // 调用do_yield让出CPU
}

// sys_kill - 杀死进程系统调用
static int
sys_kill(uint64_t arg[])
{
    int pid = (int)arg[0]; // 获取目标进程PID
    return do_kill(pid); // 调用do_kill杀死进程
}

// sys_getpid - 获取当前进程ID系统调用
static int
sys_getpid(uint64_t arg[])
{
    return current->pid; // 返回当前进程的PID
}

// sys_putc - 输出字符系统调用
static int
sys_putc(uint64_t arg[])
{
    int c = (int)arg[0]; // 获取要输出的字符
    cputchar(c); // 调用内核函数输出字符
    return 0;
}

// sys_pgdir - 打印页目录信息（调试用）
static int
sys_pgdir(uint64_t arg[])
{
    // print_pgdir();
    return 0;
}

// sys_gettime - 获取系统时间系统调用
static int sys_gettime(uint64_t arg[])
{
    return (int)ticks * 10; // 返回系统启动以来的时间（毫秒）
}

// sys_lab6_set_priority - Lab6设置优先级系统调用
static int sys_lab6_set_priority(uint64_t arg[])
{
    uint64_t priority = (uint64_t)arg[0]; // 获取优先级
    lab6_set_priority(priority); // 设置优先级
    return 0;
}

// sys_sleep - 进程睡眠系统调用
static int
sys_sleep(uint64_t arg[])
{
    unsigned int time = (unsigned int)arg[0]; // 获取睡眠时间
    return do_sleep(time); // 调用do_sleep使进程睡眠
}

// sys_open - 打开文件系统调用
static int
sys_open(uint64_t arg[])
{
    const char *path = (const char *)arg[0]; // 获取文件路径
    uint32_t open_flags = (uint32_t)arg[1]; // 获取打开标志
    return sysfile_open(path, open_flags); // 调用sysfile_open打开文件
}

// sys_close - 关闭文件系统调用
static int
sys_close(uint64_t arg[])
{
    int fd = (int)arg[0]; // 获取文件描述符
    return sysfile_close(fd); // 调用sysfile_close关闭文件
}

// sys_read - 读取文件系统调用
static int
sys_read(uint64_t arg[])
{
    int fd = (int)arg[0]; // 获取文件描述符
    void *base = (void *)arg[1]; // 获取读取缓冲区地址
    size_t len = (size_t)arg[2]; // 获取读取长度
    return sysfile_read(fd, base, len); // 调用sysfile_read读取文件
}

// sys_write - 写入文件系统调用
static int
sys_write(uint64_t arg[])
{
    int fd = (int)arg[0]; // 获取文件描述符
    void *base = (void *)arg[1]; // 获取写入数据地址
    size_t len = (size_t)arg[2]; // 获取写入长度
    return sysfile_write(fd, base, len); // 调用sysfile_write写入文件
}

// sys_seek - 定位文件读写位置系统调用
static int
sys_seek(uint64_t arg[])
{
    int fd = (int)arg[0]; // 获取文件描述符
    off_t pos = (off_t)arg[1]; // 获取目标位置
    int whence = (int)arg[2]; // 获取定位参考点
    return sysfile_seek(fd, pos, whence); // 调用sysfile_seek定位
}

// sys_fstat - 获取文件状态系统调用
static int
sys_fstat(uint64_t arg[])
{
    int fd = (int)arg[0]; // 获取文件描述符
    struct stat *stat = (struct stat *)arg[1]; // 获取用于存储状态的结构体指针
    return sysfile_fstat(fd, stat); // 调用sysfile_fstat获取状态
}

// sys_fsync - 同步文件到磁盘系统调用
static int
sys_fsync(uint64_t arg[])
{
    int fd = (int)arg[0]; // 获取文件描述符
    return sysfile_fsync(fd); // 调用sysfile_fsync同步文件
}

// sys_getcwd - 获取当前工作目录系统调用
static int
sys_getcwd(uint64_t arg[])
{
    char *buf = (char *)arg[0]; // 获取缓冲区
    size_t len = (size_t)arg[1]; // 获取缓冲区长度
    return sysfile_getcwd(buf, len); // 调用sysfile_getcwd获取CWD
}

// sys_getdirentry - 获取目录项系统调用
static int
sys_getdirentry(uint64_t arg[])
{
    int fd = (int)arg[0]; // 获取目录的文件描述符
    struct dirent *direntp = (struct dirent *)arg[1]; // 获取dirent结构指针
    return sysfile_getdirentry(fd, direntp); // 调用sysfile_getdirentry获取目录项
}

// sys_dup - 复制文件描述符系统调用
static int
sys_dup(uint64_t arg[])
{
    int fd1 = (int)arg[0]; // 获取源文件描述符
    int fd2 = (int)arg[1]; // 获取目标文件描述符
    return sysfile_dup(fd1, fd2); // 调用sysfile_dup复制文件描述符
}

// 系统调用表
static int (*syscalls[])(uint64_t arg[]) = {
    [SYS_exit] sys_exit,
    [SYS_fork] sys_fork,
    [SYS_wait] sys_wait,
    [SYS_exec] sys_exec,
    [SYS_yield] sys_yield,
    [SYS_kill] sys_kill,
    [SYS_getpid] sys_getpid,
    [SYS_putc] sys_putc,
    [SYS_pgdir] sys_pgdir,
    [SYS_gettime] sys_gettime,
    [SYS_lab6_set_priority] sys_lab6_set_priority,
    [SYS_sleep] sys_sleep,
    [SYS_open] sys_open,
    [SYS_close] sys_close,
    [SYS_read] sys_read,
    [SYS_write] sys_write,
    [SYS_seek] sys_seek,
    [SYS_fstat] sys_fstat,
    [SYS_fsync] sys_fsync,
    [SYS_getcwd] sys_getcwd,
    [SYS_getdirentry] sys_getdirentry,
    [SYS_dup] sys_dup,
};

#define NUM_SYSCALLS ((sizeof(syscalls)) / (sizeof(syscalls[0])))

// syscall - 系统调用分发函数
void syscall(void)
{
    struct trapframe *tf = current->tf; // 获取当前进程的中断帧
    uint64_t arg[5]; // 参数数组
    int num = tf->gpr.a0; // 从a0寄存器获取系统调用号
    if (num >= 0 && num < NUM_SYSCALLS) // 检查系统调用号是否合法
    {
        if (syscalls[num] != NULL) // 检查对应的处理函数是否存在
        {
            // 从a1-a5寄存器获取参数
            arg[0] = tf->gpr.a1;
            arg[1] = tf->gpr.a2;
            arg[2] = tf->gpr.a3;
            arg[3] = tf->gpr.a4;
            arg[4] = tf->gpr.a5;
            // 调用对应的系统调用处理函数，并将返回值存入a0寄存器
            tf->gpr.a0 = syscalls[num](arg);
            return;
        }
    }
    print_trapframe(tf); // 如果是非法系统调用，打印中断帧
    panic("undefined syscall %d, pid = %d, name = %s.\n",
          num, current->pid, current->name); // panic报错
}
