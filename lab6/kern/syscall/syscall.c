#include <unistd.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <assert.h>
#include <clock.h>

static int
sys_exit(uint64_t arg[]) {
    int error_code = (int)arg[0]; // 获取错误码
    return do_exit(error_code); // 调用do_exit退出进程
}

static int
sys_fork(uint64_t arg[]) {
    struct trapframe *tf = current->tf; // 获取当前进程的中断帧
    uintptr_t stack = tf->gpr.sp; // 获取栈指针
    return do_fork(0, stack, tf); // 调用do_fork创建子进程
}

static int
sys_wait(uint64_t arg[]) {
    int pid = (int)arg[0]; // 获取等待的PID
    int *store = (int *)arg[1]; // 获取存储状态码的地址
    return do_wait(pid, store); // 调用do_wait等待子进程
}

static int
sys_exec(uint64_t arg[]) {
    const char *name = (const char *)arg[0]; // 获取程序名称
    size_t len = (size_t)arg[1]; // 获取名字长度
    unsigned char *binary = (unsigned char *)arg[2]; // 获取二进制数据
    size_t size = (size_t)arg[3]; // 获取二进制大小
    return do_execve(name, len, binary, size); // 调用do_execve执行新程序
}

static int
sys_yield(uint64_t arg[]) {
    return do_yield(); // 调用do_yield让出CPU
}

static int
sys_kill(uint64_t arg[]) {
    int pid = (int)arg[0]; // 获取要杀死的PID
    return do_kill(pid); // 调用do_kill杀死进程
}

static int
sys_getpid(uint64_t arg[]) {
    return current->pid; // 返回当前进程的PID
}

static int
sys_putc(uint64_t arg[]) {
    int c = (int)arg[0]; // 获取字符
    cputchar(c); // 输出字符
    return 0;
}

static int
sys_pgdir(uint64_t arg[]) {
    //print_pgdir();
    return 0;
}
static int sys_gettime(uint64_t arg[]){
    return (int)ticks*10; // 返回系统时间（tick * 10 ms）
}
static int sys_lab6_set_priority(uint64_t arg[]){
    uint64_t priority = (uint64_t)arg[0]; // 获取优先级
    lab6_set_priority(priority); // 设置Lab6优先级
    return 0;
}
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
    [SYS_gettime]           sys_gettime,
    [SYS_lab6_set_priority]  sys_lab6_set_priority,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

void
syscall(void) {
    struct trapframe *tf = current->tf; // 获取当前进程的trapframe
    uint64_t arg[5];
    int num = tf->gpr.a0; // 获取系统调用号
    if (num >= 0 && num < NUM_SYSCALLS) {
        if (syscalls[num] != NULL) {
            arg[0] = tf->gpr.a1; // 获取参数1
            arg[1] = tf->gpr.a2; // 获取参数2
            arg[2] = tf->gpr.a3; // 获取参数3
            arg[3] = tf->gpr.a4; // 获取参数4
            arg[4] = tf->gpr.a5; // 获取参数5
            tf->gpr.a0 = syscalls[num](arg); // 执行系统调用并设置返回值
            return ;
        }
    }
    print_trapframe(tf);
    panic("undefined syscall %d, pid = %d, name = %s.\n",
            num, current->pid, current->name);
}

