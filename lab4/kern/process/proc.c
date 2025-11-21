#include <proc.h>
#include <kmalloc.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* ------------- 进程/线程机制设计与实现 -------------
（简化版的 Linux 进程/线程机制）
简介：
    ucore 实现了一个简单的进程/线程机制。进程包含独立的内存空间，至少一个用于执行的线程，
内核数据（用于管理）、处理器状态（用于上下文切换）、文件（lab6）、等。ucore 需要高效地
管理所有这些细节。在 ucore 中，线程只是进程的一种特殊类型（共享进程的内存）。
------------------------------
进程状态       :     含义               -- 原因
        PROC_UNINIT     :   未初始化           -- alloc_proc
        PROC_SLEEPING   :   睡眠               -- try_free_pages, do_wait, do_sleep
        PROC_RUNNABLE   :   可运行（可能正在运行） -- proc_init, wakeup_proc,
        PROC_ZOMBIE     :   即将死亡           -- do_exit

-----------------------------
进程状态变化：

    alloc_proc                                 RUNNING
            +                                   +--<----<--+
            +                                   + proc_run +
            V                                   +-->---->--+
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                                                                     A      +                                                           +
                                                                                     |      +--- do_exit --> PROC_ZOMBIE                                +
                                                                                     +                                                                  +
                                                                                     -----------------------wakeup_proc----------------------------------
-----------------------------
进程关系
父进程:           proc->parent  （proc 是子进程）
子进程:           proc->cptr    （proc 是父进程）
年长兄弟:         proc->optr    （proc 是年幼兄弟）
年幼兄弟:         proc->yptr    （proc 是年长兄弟）
-----------------------------
相关进程系统调用:
SYS_exit        : 进程退出,                           -->do_exit
SYS_fork        : 创建子进程, 复制 mm                 -->do_fork-->wakeup_proc
SYS_wait        : 等待进程                            -->do_wait
SYS_exec        : fork 后执行新程序                   -->加载程序并刷新 mm
SYS_clone       : 创建子线程                         -->do_fork-->wakeup_proc
SYS_yield       : 进程主动让出调度,                  -- proc->need_sched=1, 调度器重新调度该进程
SYS_sleep       : 进程睡眠                           -->do_sleep
SYS_kill        : 杀死进程                            -->do_kill-->proc->flags |= PF_EXITING
                                                                                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : 获取进程的 pid

*/

// 进程集合的链表
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// 基于 pid 的进程集合哈希链表
static list_entry_t hash_list[HASH_LIST_SIZE];

// 空闲进程
struct proc_struct *idleproc = NULL;
// 初始化进程
struct proc_struct *initproc = NULL;
// 当前进程
struct proc_struct *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

// alloc_proc - 分配一个 proc_struct 并初始化所有字段
static struct proc_struct *
alloc_proc(void)
{
        struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
        if (proc != NULL)
        {
                // LAB4:EXERCISE1 你的代码
                /*
                 * 下面这些字段需要初始化
                 *       enum proc_state state;                      // 进程状态
                 *       int pid;                                    // 进程 ID
                 *       int runs;                                   // 进程运行次数
                 *       uintptr_t kstack;                           // 进程内核栈
                 *       volatile bool need_resched;                 // 是否需要调度
                 *       struct proc_struct *parent;                 // 父进程
                 *       struct mm_struct *mm;                       // 虚拟内存管理
                 *       struct context context;                     // 上下文切换信息
                 *       struct trapframe *tf;                       // 当前中断帧
                 *       uintptr_t pgdir;                            // 页目录表基址
                 *       uint32_t flags;                             // 进程标志
                 *       char name[PROC_NAME_LEN + 1];               // 进程名
                 */
                proc->state = PROC_UNINIT;  // 设置为未初始化状态
                proc->pid = -1;              // 未初始化的进程ID
                proc->runs = 0;              // 初始化运行时间
                proc->kstack = 0;            // 内核栈地址
                proc->need_resched = 0;      // 不需要调度
                proc->parent = NULL;         // 父进程为空
                proc->mm = NULL;             // 虚拟内存管理为空
                memset(&(proc->context), 0, sizeof(struct context));  // 初始化上下文
                proc->tf = NULL;             // 中断帧指针为空
                proc->pgdir = boot_pgdir_pa; // 使用内核页目录表物理地址
                proc->flags = 0;             // 标志位为0
                memset(proc->name, 0, PROC_NAME_LEN + 1);  // 清空进程名
        }
        return proc;
}

// set_proc_name - 设置进程名
char *
set_proc_name(struct proc_struct *proc, const char *name)
{
        memset(proc->name, 0, sizeof(proc->name));
        return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - 获取进程名
char *
get_proc_name(struct proc_struct *proc)
{
        static char name[PROC_NAME_LEN + 1];
        memset(name, 0, sizeof(name));
        return memcpy(name, proc->name, PROC_NAME_LEN);
}

// get_pid - 分配一个唯一的 pid
static int
get_pid(void)
{
        static_assert(MAX_PID > MAX_PROCESS);
        struct proc_struct *proc;
        list_entry_t *list = &proc_list, *le;
        static int next_safe = MAX_PID, last_pid = MAX_PID;
        if (++last_pid >= MAX_PID)
        {
                last_pid = 1;
                goto inside;
        }
        if (last_pid >= next_safe)
        {
        inside:
                next_safe = MAX_PID;
        repeat:
                le = list;
                while ((le = list_next(le)) != list)
                {
                        proc = le2proc(le, list_link);
                        if (proc->pid == last_pid)
                        {
                                if (++last_pid >= next_safe)
                                {
                                        if (last_pid >= MAX_PID)
                                        {
                                                last_pid = 1;
                                        }
                                        next_safe = MAX_PID;
                                        goto repeat;
                                }
                        }
                        else if (proc->pid > last_pid && next_safe > proc->pid)
                        {
                                next_safe = proc->pid;
                        }
                }
        }
        return last_pid;
}

// proc_run - 让指定进程在 CPU 上运行
// 注意：调用 switch_to 前应加载新进程的页目录基址
void proc_run(struct proc_struct *proc)
{
        if (proc != current)
        {
                // LAB4:EXERCISE3 你的代码
                /*
                 * 一些有用的宏和函数，你可以在下面实现中使用。
                 * 宏或函数：
                 *   local_intr_save():        关闭中断
                 *   local_intr_restore():     开启中断
                 *   lsatp():                  修改 satp 寄存器的值
                 *   switch_to():              进程上下文切换
                 */
                
                bool intr_flag;
                struct proc_struct *prev = current, *next = proc;
                
                // 1. 禁用中断
                local_intr_save(intr_flag);
                {
                        // 2. 切换当前进程为要运行的进程
                        current = proc;
                        
                        // 3. 切换页表，使用新进程的地址空间
                        lsatp(next->pgdir);
                        
                        // 4. 实现上下文切换
                        switch_to(&(prev->context), &(next->context));
                }
                // 5. 允许中断
                local_intr_restore(intr_flag);
        }
}

// forkret -- 新线程/进程的第一个内核入口点
// 注意：forkret 的地址在 copy_thread 函数中设置
//       switch_to 后，当前进程会执行这里
static void
forkret(void)
{
        forkrets(current->tf);
}

// hash_proc - 将进程加入哈希链表
static void
hash_proc(struct proc_struct *proc)
{
        list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// find_proc - 根据 pid 在哈希链表中查找进程
struct proc_struct *
find_proc(int pid)
{
        if (0 < pid && pid < MAX_PID)
        {
                list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
                while ((le = list_next(le)) != list)
                {
                        struct proc_struct *proc = le2proc(le, hash_link);
                        if (proc->pid == pid)
                        {
                                return proc;
                        }
                }
        }
        return NULL;
}

// kernel_thread - 使用指定函数创建一个内核线程
// 注意：临时 trapframe tf 的内容会在 do_fork-->copy_thread 复制到 proc->tf
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
        struct trapframe tf;
        memset(&tf, 0, sizeof(struct trapframe));
        tf.gpr.s0 = (uintptr_t)fn;
        tf.gpr.s1 = (uintptr_t)arg;
        tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
        tf.epc = (uintptr_t)kernel_thread_entry;
        return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

// setup_kstack - 分配 KSTACKPAGE 大小的页作为进程内核栈
static int
setup_kstack(struct proc_struct *proc)
{
        struct Page *page = alloc_pages(KSTACKPAGE);
        if (page != NULL)
        {
                proc->kstack = (uintptr_t)page2kva(page);
                return 0;
        }
        return -E_NO_MEM;
}

// put_kstack - 释放进程内核栈的内存空间
static void
put_kstack(struct proc_struct *proc)
{
        free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// copy_mm - 按照 clone_flags 复制或共享当前进程的 mm
//         - 如果 clone_flags & CLONE_VM，则共享，否则复制
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
        assert(current->mm == NULL);
        /* 本项目无需处理 */
        return 0;
}

// copy_thread - 在进程内核栈顶设置 trapframe
//             - 设置进程的内核入口点和栈
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
{
        proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
        *(proc->tf) = *tf;

        // 设置 a0 为 0，子进程可知刚 fork
        proc->tf->gpr.a0 = 0;
        proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;

        proc->context.ra = (uintptr_t)forkret;
        proc->context.sp = (uintptr_t)(proc->tf);
}

/* do_fork -     父进程为新子进程创建资源
 * @clone_flags: 指导如何克隆子进程
 * @stack:       父进程用户栈指针。若 stack==0，表示 fork 内核线程。
 * @tf:          trapframe 信息，会复制到子进程的 proc->tf
 */
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
        int ret = -E_NO_FREE_PROC;
        struct proc_struct *proc;
        if (nr_process >= MAX_PROCESS)
        {
                goto fork_out;
        }
        ret = -E_NO_MEM;
        // LAB4:EXERCISE2 你的代码
        /*
         * 一些有用的宏和函数，你可以在下面实现中使用。
         * 宏或函数：
         *   alloc_proc:   创建并初始化 proc_struct（lab4:exercise1）
         *   setup_kstack: 分配 KSTACKPAGE 大小的页作为进程内核栈
         *   copy_mm:      按照 clone_flags 复制或共享 mm
         *                 如果 clone_flags & CLONE_VM，则共享，否则复制
         *   copy_thread:  设置 trapframe 和 context
         *   hash_proc:    加入哈希链表
         *   get_pid:      分配唯一 pid
         *   wakeup_proc:  设置 proc->state = PROC_RUNNABLE
         * 变量:
         *   proc_list:    进程集合链表
         *   nr_process:   进程数
         */

        //    1. 调用 alloc_proc 分配 proc_struct
        //    2. 调用 setup_kstack 分配内核栈
        //    3. 调用 copy_mm 复制或共享 mm
        //    4. 调用 copy_thread 设置 trapframe 和 context
        //    5. 插入 hash_list 和 proc_list
        //    6. 调用 wakeup_proc 使新进程变为 RUNNABLE
        //    7. 用子进程 pid 设置返回值
        
        // 1. 调用 alloc_proc 分配 proc_struct
        if ((proc = alloc_proc()) == NULL) {
                goto fork_out;
        }
        
        // 设置父进程为当前进程
        proc->parent = current;
        
        // 2. 调用 setup_kstack 分配内核栈
        if (setup_kstack(proc) != 0) {
                goto bad_fork_cleanup_proc;
        }
        
        // 3. 调用 copy_mm 复制或共享 mm
        if (copy_mm(clone_flags, proc) != 0) {
                goto bad_fork_cleanup_kstack;
        }
        
        // 4. 调用 copy_thread 设置 trapframe 和 context
        copy_thread(proc, stack, tf);
        
        // 5. 插入 hash_list 和 proc_list
        bool intr_flag;
        local_intr_save(intr_flag);
        {
                proc->pid = get_pid();
                hash_proc(proc);
                list_add(&proc_list, &(proc->list_link));
                nr_process++;
        }
        local_intr_restore(intr_flag);
        
        // 6. 调用 wakeup_proc 使新进程变为 RUNNABLE
        wakeup_proc(proc);
        
        // 7. 用子进程 pid 设置返回值
        ret = proc->pid;
        
fork_out:
        return ret;

bad_fork_cleanup_kstack:
        put_kstack(proc);
bad_fork_cleanup_proc:
        kfree(proc);
        goto fork_out;
}

// do_exit - 由 sys_exit 调用
//   1. 调用 exit_mmap、put_pgdir、mm_destroy 释放几乎所有进程内存空间
//   2. 设置进程状态为 PROC_ZOMBIE，然后唤醒父进程让其回收自己
//   3. 调用调度器切换到其他进程
int do_exit(int error_code)
{
        panic("process exit!!.\n");
}

// init_main - 第二个内核线程，用于创建 user_main 内核线程
static int
init_main(void *arg)
{
        cprintf("这是 initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
        cprintf("To U: \"%s\".\n", (const char *)arg);
        cprintf("To U: \"en.., Bye, Bye. :)\"\n");
        return 0;
}

// proc_init - 设置第一个内核线程 idleproc，并创建第二个内核线程 init_main
void proc_init(void)
{
        int i;

        list_init(&proc_list);
        for (i = 0; i < HASH_LIST_SIZE; i++)
        {
                list_init(hash_list + i);
        }

        if ((idleproc = alloc_proc()) == NULL)
        {
                panic("无法分配 idleproc。\n");
        }

        // 检查 proc 结构体
        int *context_mem = (int *)kmalloc(sizeof(struct context));
        memset(context_mem, 0, sizeof(struct context));
        int context_init_flag = memcmp(&(idleproc->context), context_mem, sizeof(struct context));

        int *proc_name_mem = (int *)kmalloc(PROC_NAME_LEN);
        memset(proc_name_mem, 0, PROC_NAME_LEN);
        int proc_name_flag = memcmp(&(idleproc->name), proc_name_mem, PROC_NAME_LEN);

        if (idleproc->pgdir == boot_pgdir_pa && idleproc->tf == NULL && !context_init_flag && idleproc->state == PROC_UNINIT && idleproc->pid == -1 && idleproc->runs == 0 && idleproc->kstack == 0 && idleproc->need_resched == 0 && idleproc->parent == NULL && idleproc->mm == NULL && idleproc->flags == 0 && !proc_name_flag)
        {
                cprintf("alloc_proc() 正确!\n");
        }

        idleproc->pid = 0;
        idleproc->state = PROC_RUNNABLE;
        idleproc->kstack = (uintptr_t)bootstack;
        idleproc->need_resched = 1;
        set_proc_name(idleproc, "idle");
        nr_process++;

        current = idleproc;

        int pid = kernel_thread(init_main, "Hello world!!", 0);
        if (pid <= 0)
        {
                panic("创建 init_main 失败。\n");
        }

        initproc = find_proc(pid);
        set_proc_name(initproc, "init");

        assert(idleproc != NULL && idleproc->pid == 0);
        assert(initproc != NULL && initproc->pid == 1);
}

// cpu_idle - 在 kern_init 结束时，第一个内核线程 idleproc 会执行如下操作
void cpu_idle(void)
{
        while (1)
        {
                if (current->need_resched)
                {
                        schedule();
                }
        }
}
