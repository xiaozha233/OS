#ifndef __KERN_PROCESS_PROC_H__                   // 头文件防重包含开头
#define __KERN_PROCESS_PROC_H__                   // 定义头文件防重宏

#include <defs.h>                                 // ucore 常用类型/宏
#include <list.h>                                 // 双向链表结构
#include <trap.h>                                 // 中断陷阱帧定义
#include <memlayout.h>                            // 内存布局常量

// 进程在其生命周期中的状态
enum proc_state                                    // 枚举所有可能的进程状态
{
    PROC_UNINIT = 0, // 未初始化                  // 尚未准备好运行
    PROC_SLEEPING,   // 睡眠                       // 阻塞等待事件
    PROC_RUNNABLE,   // 可运行（可能正在运行）     // 就绪或正在 CPU 上
    PROC_ZOMBIE,     // 几乎死亡，等待父进程回收其资源 // 已退出待回收
};

struct context                                     // 保存上下文切换时需要的寄存器
{
    uintptr_t ra;                                  // 返回地址寄存器
    uintptr_t sp;                                  // 栈指针寄存器
    uintptr_t s0;                                  // 被调度寄存器 s0
    uintptr_t s1;                                  // 寄存器 s1
    uintptr_t s2;                                  // 寄存器 s2
    uintptr_t s3;                                  // 寄存器 s3
    uintptr_t s4;                                  // 寄存器 s4
    uintptr_t s5;                                  // 寄存器 s5
    uintptr_t s6;                                  // 寄存器 s6
    uintptr_t s7;                                  // 寄存器 s7
    uintptr_t s8;                                  // 寄存器 s8
    uintptr_t s9;                                  // 寄存器 s9
    uintptr_t s10;                                 // 寄存器 s10
    uintptr_t s11;                                 // 寄存器 s11
};

#define PROC_NAME_LEN 15                          // 进程名最大长度（不含结尾符）
#define MAX_PROCESS 4096                          // 支持的最大并发进程数
#define MAX_PID (MAX_PROCESS * 2)                 // pid 可用的上限

extern list_entry_t proc_list;                    // 全局进程链表头

struct proc_struct                                 // 进程控制块，描述单个进程
{
    enum proc_state state;                  // 进程状态
    int pid;                                // 进程 ID
    int runs;                               // 进程运行次数
    uintptr_t kstack;                       // 进程内核栈
    volatile bool need_resched;             // 布尔值：是否需要重新调度以释放 CPU？
    struct proc_struct *parent;             // 父进程
    struct mm_struct *mm;                   // 进程的内存管理结构
    struct context context;                 // 在此切换上下文以运行该进程
    struct trapframe *tf;                   // 当前中断的陷阱帧
    uintptr_t pgdir;                        // 页目录表（PDT）的基址
    uint32_t flags;                         // 进程标志
    char name[PROC_NAME_LEN + 1];           // 进程名
    list_entry_t list_link;                 // 进程链表
    list_entry_t hash_link;                 // 进程哈希链表
    // =======  lab5新增  ========
    int exit_code;                          // 退出码（发送给父进程）
    uint32_t wait_state;                    // 等待状态
    struct proc_struct *cptr, *yptr, *optr; // 进程之间的关系
};

#define PF_EXITING 0x00000001 // 正在退出        // 标记进程处于退出流程

#define WT_CHILD (0x00000001 | WT_INTERRUPTED)  // 等待子进程退出
#define WT_INTERRUPTED 0x80000000 // 等待状态可能被中断

#define le2proc(le, member) \                    // 根据链表节点还原 proc_struct
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *idleproc, *initproc, *current; // 常用进程指针

void proc_init(void);                               // 进程子系统初始化
void proc_run(struct proc_struct *proc);            // 切换到指定进程运行
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags); // 创建内核线程

char *set_proc_name(struct proc_struct *proc, const char *name); // 设置进程名称
char *get_proc_name(struct proc_struct *proc);                   // 读取进程名称
void cpu_idle(void) __attribute__((noreturn));                   // 空闲线程主体

struct proc_struct *find_proc(int pid);            // 通过 pid 查找进程
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf); // fork 实现
int do_exit(int error_code);                       // 进程退出
int do_yield(void);                                // 主动让出 CPU
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size); // 执行新程序
int do_wait(int pid, int *code_store);             // 等待子进程结束
int do_kill(int pid);                              // 杀死目标进程
#endif /* !__KERN_PROCESS_PROC_H__ */              // 头文件防重包含结束
