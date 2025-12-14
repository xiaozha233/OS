#include <unistd.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <assert.h>

static int
sys_exit(uint64_t arg[]) {
    int error_code = (int)arg[0];
    return do_exit(error_code);
}

static int
sys_fork(uint64_t arg[]) {
    struct trapframe *tf = current->tf; // 父进程的 trapframe
    uintptr_t stack = tf->gpr.sp; // 父进程的用户栈指针
    return do_fork(0, stack, tf); // 真正创建子进程
}

static int
sys_wait(uint64_t arg[]) {
    int pid = (int)arg[0];
    int *store = (int *)arg[1];
    return do_wait(pid, store);
}

static int
sys_exec(uint64_t arg[]) {
    const char *name = (const char *)arg[0];
    size_t len = (size_t)arg[1];
    unsigned char *binary = (unsigned char *)arg[2];
    size_t size = (size_t)arg[3];
    return do_execve(name, len, binary, size);
}

static int
sys_yield(uint64_t arg[]) {
    return do_yield();
}

static int
sys_kill(uint64_t arg[]) {
    int pid = (int)arg[0];
    return do_kill(pid);
}

static int
sys_getpid(uint64_t arg[]) {
    return current->pid;
}

static int
sys_putc(uint64_t arg[]) {
    int c = (int)arg[0];
    cputchar(c);
    return 0;
}

static int
sys_pgdir(uint64_t arg[]) {
    //print_pgdir();
    return 0;
}
// 系统调用分发表，根据系统调用号索引到对应的处理函数
static int (*syscalls[])(uint64_t arg[]) = {
    [SYS_exit]              sys_exit,
    [SYS_fork]              sys_fork,
    [SYS_wait]              sys_wait,
    [SYS_exec]              sys_exec,
    [SYS_yield]             sys_yield,
    [SYS_kill]              sys_kill,
    [SYS_getpid]            sys_getpid,
    [SYS_putc]              sys_putc,
    [SYS_pgdir]             sys_pgdir,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

// 内核的系统调用分发入口，由异常/中断处理流程中的 trap() 或 exception_handler() 调用
// 上下文：调用时在内核态且处于中断处理流程中（CPU 寄存器已被 trapentry.S 保存到内核栈上的 trapframe）
void
syscall(void) {
    struct trapframe *tf = current->tf;
    uint64_t arg[5]; // 为最多5个syscall参数预分配缓存数组
    int num = tf->gpr.a0; // 读取syscall号
    if (num >= 0 && num < NUM_SYSCALLS) {
        if (syscalls[num] != NULL) {
            // 将最多 5 个参数从 trapframe 的寄存器 a1..a5 复制到 arg[]。
            arg[0] = tf->gpr.a1;
            arg[1] = tf->gpr.a2;
            arg[2] = tf->gpr.a3;
            arg[3] = tf->gpr.a4;
            arg[4] = tf->gpr.a5;
            // 根据 syscall 号调用对应的处理函数，返回值存回 a0
            tf->gpr.a0 = syscalls[num](arg);
            return ;
        }
    }
    // 如果执行到这里表示syscall非法，打印trapframe供调试
    print_trapframe(tf);
    // 内核遇到非法 syscall 编号会调用 panic 打印错误并停止内核（终止整个 OS）
    panic("undefined syscall %d, pid = %d, name = %s.\n",
            num, current->pid, current->name);
}

