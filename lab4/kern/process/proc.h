#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include <defs.h>
#include <list.h>
#include <trap.h>
#include <memlayout.h>

// 进程生命周期中的状态
enum proc_state
{
    PROC_UNINIT = 0, // 未初始化
    PROC_SLEEPING,   // 睡眠中
    PROC_RUNNABLE,   // 可运行（可能正在运行）
    PROC_ZOMBIE,     // 即将死亡，等待父进程回收资源
};

struct context
{
    uintptr_t ra;
    uintptr_t sp;
    uintptr_t s0;
    uintptr_t s1;
    uintptr_t s2;
    uintptr_t s3;
    uintptr_t s4;
    uintptr_t s5;
    uintptr_t s6;
    uintptr_t s7;
    uintptr_t s8;
    uintptr_t s9;
    uintptr_t s10;
    uintptr_t s11;
};

#define PROC_NAME_LEN 15
#define MAX_PROCESS 4096
#define MAX_PID (MAX_PROCESS * 2)

extern list_entry_t proc_list;

struct proc_struct
{
    enum proc_state state;        // 进程状态
    int pid;                      // 进程ID
    int runs;                     // 进程运行次数
    uintptr_t kstack;             // 进程内核栈
    volatile bool need_resched;   // 是否需要重新调度以释放CPU
    struct proc_struct *parent;   // 父进程
    struct mm_struct *mm;         // 进程的内存管理字段
    struct context context;       // 进程切换上下文
    struct trapframe *tf;         // 当前中断的陷阱帧
    uintptr_t pgdir;              // 页目录表(PDT)的基地址
    uint32_t flags;               // 进程标志
    char name[PROC_NAME_LEN + 1]; // 进程名
    list_entry_t list_link;       // 进程链表
    list_entry_t hash_link;       // 进程哈希链表
};

#define le2proc(le, member) \
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *idleproc, *initproc, *current;

void proc_init(void); // 初始化进程
void proc_run(struct proc_struct *proc); // 运行指定进程
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags); // 创建内核线程

char *set_proc_name(struct proc_struct *proc, const char *name); // 设置进程名
char *get_proc_name(struct proc_struct *proc); // 获取进程名
void cpu_idle(void) __attribute__((noreturn)); // CPU空闲时执行

struct proc_struct *find_proc(int pid); // 根据PID查找进程
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf); // 执行fork操作
int do_exit(int error_code); // 进程退出

#endif /* !__KERN_PROCESS_PROC_H__ */
