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

#define HASH_SHIFT 10 // 哈希链表大小为 2^10 = 1024
#define HASH_LIST_SIZE (1 << HASH_SHIFT) // 哈希链表大小为 1024
#define pid_hashfn(x) (hash32(x, HASH_SHIFT)) // 计算PID的哈希值

// 基于 pid 的进程集合哈希链表
static list_entry_t hash_list[HASH_LIST_SIZE];

// 空闲进程
struct proc_struct *idleproc = NULL;
// 初始化进程
struct proc_struct *initproc = NULL;
// 当前进程
struct proc_struct *current = NULL;
// 当前进程数
static int nr_process = 0;

void kernel_thread_entry(void); // 内核线程入口函数
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

// alloc_proc - 分配一个 proc_struct 并初始化所有字段
static struct proc_struct *
alloc_proc(void)
{
        // 调用 kmalloc 为proc分配内存
        struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
        if (proc != NULL)
        {
                // LAB4:EXERCISE1 2314076
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
        /* 静态断言：保证可用的 PID 最大值大于进程表能容纳的最大进程数。
         * 这保证在最坏情况（所有可用 PID 都被占用）下，MAX_PID 的范围仍然足够，
         * 以避免逻辑上出现无法分配 PID 的矛盾。
         */
        static_assert(MAX_PID > MAX_PROCESS);

        /* 用于遍历进程集合的临时指针变量 */
        struct proc_struct *proc;

        /* 获取指向进程链表头的指针，proc_list 是全局保存所有进程的链表 */
        list_entry_t *list = &proc_list, *le;

        /* 两个静态局部变量用于记录下一个候选 PID 的范围：
         * - next_safe 表示当前已知的下一个冲突 PID 上界（exclusive），
         *   任何小于 next_safe 的 PID 经过本次扫描后可以被安全使用。
         * - last_pid 表示最后返回过的 PID，搜索将从它的下一个值开始以避免重复。
         *
         * 初始值为 MAX_PID 表示初次调用会从 1 开始搜索（见后续自增与边界处理）。
         */
        static int next_safe = MAX_PID, last_pid = MAX_PID;

        /* 先将 last_pid 自增以查找下一个候选 PID。当 last_pid 增加到等于或超过 MAX_PID 时，
         * 表示到达 PID 上限，需要从 1 重新开始循环查找（PID=0 保留给 idleproc）。
         */
        if (++last_pid >= MAX_PID)
        {
                last_pid = 1;
                goto inside;
        }

        /* 如果 last_pid 已经到达或超过 next_safe，说明我们已经不确定 [last_pid, next_safe)
         * 这个区间是否存在被占用的 PID，需要进入遍历 proc_list 的循环以确定一个安全的 PID。
         * 使用 goto label 使控制流更直接：当需要扫描链表时跳转到 inside 块进行处理。
         */
        if (last_pid >= next_safe)
        {
        inside:
                /* 进入扫描前，重置 next_safe 为最大值，表示尚未发现更小的被占用 PID 上界 */
                next_safe = MAX_PID;
        repeat:
                /* 从 proc_list 的第一个元素开始遍历整个链表，查找是否存在与 last_pid 冲突的 pid，
                 * 并同时更新 next_safe（查找比 last_pid 更小的存在的 pid，作为新上界）。
                 */
                le = list;
                while ((le = list_next(le)) != list)
                {
                        proc = le2proc(le, list_link);

                        /* 如果发现某个进程的 pid 正好等于当前候选 last_pid，
                         * 则该候选不可用，必须将 last_pid 自增以尝试下一个值。
                         * 同时检查自增后是否又越过 next_safe；若越界则需要重新从链表头扫描，
                         * 因为之前计算的 next_safe 可能已不再适用。
                         */
                        if (proc->pid == last_pid)
                        {
                                if (++last_pid >= next_safe)
                                {
                                        if (last_pid >= MAX_PID)
                                        {
                                                /* 若越过最大 PID，回绕到 1 继续检查 */
                                                last_pid = 1;
                                        }
                                        /* 既然越过了当前上界，重置上界并重新扫描整个链表 */
                                        next_safe = MAX_PID;
                                        goto repeat;
                                }
                        }
                        else if (proc->pid > last_pid && next_safe > proc->pid)
                        {
                                /* 如果当前进程 pid 大于候选 last_pid，则它为一个可能的上界。
                                 * 更新 next_safe 为链表中遇见的最小的那类 pid，
                                 * 使得我们在下一次扫描前能跳过不必要的值范围，提高效率。
                                 */
                                next_safe = proc->pid;
                        }
                }
        }
        /* 遍历结束或无需扫描时，last_pid 此时为一个未被占用的有效 PID（且不为 0）。
         * 将其返回作为新分配的 PID 值。
         */
        return last_pid;
}

// proc_run - 让指定进程在 CPU 上运行
// 注意：调用 switch_to 前应加载新进程的页目录基址
void proc_run(struct proc_struct *proc)
{
        if (proc != current)
        {
                // LAB4:EXERCISE3 2313255
                /*
                 * 一些有用的宏和函数，你可以在下面实现中使用。
                 * 宏或函数：
                 *   local_intr_save():        关闭中断
                 *   local_intr_restore():     开启中断
                 *   lsatp():                  修改 satp 寄存器的值
                 *   switch_to():              进程上下文切换
                 */
                
                bool intr_flag; // 保存中断状态的标志变量
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
        // gpr代表通用寄存器
        tf.gpr.s0 = (uintptr_t)fn;// s0保存函数指针
        tf.gpr.s1 = (uintptr_t)arg;// s1保存函数参数
        // 分别设置SPP和SPIE位，表示内核态执行和中断使能，并清除SIE位，暂时禁止中断
        tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
        // 保存中断返回地址为 kernel_thread_entry
        tf.epc = (uintptr_t)kernel_thread_entry;
        return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

// setup_kstack - 分配 KSTACKPAGE 大小的页作为进程内核栈
static int
setup_kstack(struct proc_struct *proc)
{
        // 调用 alloc_pages 分配 KSTACKPAGE 个连续的物理页面
        // KSTACKPAGE 定义了内核栈所需的页面数量
        struct Page *page = alloc_pages(KSTACKPAGE);
        
        // 检查页面分配是否成功
        if (page != NULL)
        {
                // 将分配的物理页面转换为内核虚拟地址
                // page2kva 函数将 Page 结构体指针转换为对应的内核虚拟地址
                // 将转换后的地址保存到进程的 kstack 字段中，作为内核栈的起始地址
                proc->kstack = (uintptr_t)page2kva(page);
                
                // 返回 0 表示成功分配内核栈
                return 0;
        }
        
        // 如果页面分配失败，返回内存不足错误码
        return -E_NO_MEM;
}

// put_kstack - 释放进程内核栈的内存空间
static void
put_kstack(struct proc_struct *proc)
{
        // kva2page 将内核虚拟地址转换回 Page 结构体指针
        // 将 proc->kstack（uintptr_t 类型）强制转换为 void* 类型传递给 kva2page
        // 然后调用 free_pages 释放 KSTACKPAGE 个页面
        // 这是 setup_kstack 的逆操作，用于回收进程不再使用的内核栈内存
        free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// copy_mm - 按照 clone_flags 复制或共享当前进程的 mm
//         - 如果 clone_flags & CLONE_VM，则共享，否则复制
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
        // 断言当前进程的内存管理结构为空
        // 在当前实现中，所有进程都是内核线程，不需要用户态虚拟内存管理
        assert(current->mm == NULL);
        
        /* 本项目无需处理 */
        // 在实际的用户进程实现中，这里会根据 clone_flags 判断：
        // - 如果设置了 CLONE_VM 标志，子进程与父进程共享同一个 mm_struct（创建线程）
        // - 否则，需要复制父进程的 mm_struct 给子进程（创建进程）
        // 但在 lab4 中只处理内核线程，因此直接返回成功
        return 0;
}

// copy_thread - 在进程内核栈顶设置 trapframe
//             - 设置进程的内核入口点和栈
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
{
        // 在进程内核栈的顶部（高地址端）预留 trapframe 的空间
        // proc->kstack 是内核栈的起始地址（低地址）
        // KSTACKSIZE 是内核栈的总大小
        // trapframe 放置在栈顶，所以地址为：栈起始地址 + 栈大小 - trapframe大小
        proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
        
        // 将父进程传入的 trapframe 内容复制到子进程的 trapframe 中
        // 这样子进程会继承父进程的寄存器状态和执行上下文
        *(proc->tf) = *tf;

        // 设置子进程的返回值寄存器 a0 为 0
        // 这是 fork 系统调用的约定：父进程返回子进程的 pid，子进程返回 0
        // 通过这个返回值，进程可以判断自己是父进程还是子进程
        proc->tf->gpr.a0 = 0;
        
        // 设置子进程的栈指针 sp
        // 如果 esp 为 0，说明是创建内核线程，栈指针指向 trapframe 的位置
        // 如果 esp 不为 0，说明是用户进程，使用传入的用户栈指针
        proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;

        // 设置子进程的上下文切换返回地址为 forkret 函数
        // 当调度器第一次调度到这个新进程时，switch_to 会跳转到这个地址
        // forkret 是新进程的第一个执行点，它会调用 forkrets 进入中断返回流程
        proc->context.ra = (uintptr_t)forkret;
        
        // 设置子进程上下文切换时的栈指针，指向 trapframe 的位置
        // 这样在 switch_to 恢复上下文后，栈指针会正确指向准备好的 trapframe
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
        // LAB4:EXERCISE2 2314035
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
// init_main - 第二个内核线程的主函数,用于创建 user_main 内核线程
// 这是 initproc 进程的入口函数
static int
init_main(void *arg)
{
        // 打印当前进程(initproc)的 pid 和名称
        cprintf("这是 initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
        // 打印传入的参数字符串
        cprintf("To U: \"%s\".\n", (const char *)arg);
        // 打印结束信息
        cprintf("To U: \"en.., Bye, Bye. :)\"\n");
        // 返回 0 表示正常结束
        return 0;
}

// proc_init - 初始化进程管理系统
// 设置第一个内核线程 idleproc(空闲进程),并创建第二个内核线程 init_main(初始化进程)
void proc_init(void)
{
        int i;

        // 初始化全局进程链表 proc_list
        list_init(&proc_list);
        
        // 初始化进程哈希表的所有桶(bucket)
        // 哈希表用于快速根据 pid 查找进程
        for (i = 0; i < HASH_LIST_SIZE; i++)
        {
                list_init(hash_list + i);
        }

        // 分配并初始化第一个进程 idleproc(空闲进程)
        // idleproc 是系统中 pid 为 0 的特殊进程,当没有其他进程可运行时执行
        if ((idleproc = alloc_proc()) == NULL)
        {
                panic("无法分配 idleproc。\n");
        }

        // 以下代码用于检查 alloc_proc() 函数是否正确初始化了 proc_struct 结构体
        
        // 分配一块内存用于比对 context 字段是否正确初始化为全 0
        int *context_mem = (int *)kmalloc(sizeof(struct context));
        memset(context_mem, 0, sizeof(struct context));
        // 比较 idleproc->context 与全 0 内存,返回 0 表示相等(正确初始化)
        int context_init_flag = memcmp(&(idleproc->context), context_mem, sizeof(struct context));

        // 分配一块内存用于比对 name 字段是否正确初始化为全 0
        int *proc_name_mem = (int *)kmalloc(PROC_NAME_LEN);
        memset(proc_name_mem, 0, PROC_NAME_LEN);
        // 比较 idleproc->name 与全 0 内存,返回 0 表示相等(正确初始化)
        int proc_name_flag = memcmp(&(idleproc->name), proc_name_mem, PROC_NAME_LEN);

        // 检查 alloc_proc() 是否正确初始化了所有字段:
        // - pgdir 应为 boot_pgdir_pa(内核页目录表物理地址)
        // - tf 应为 NULL(尚未设置中断帧)
        // - context 应全为 0(尚未设置上下文)
        // - state 应为 PROC_UNINIT(未初始化状态)
        // - pid 应为 -1(尚未分配 pid)
        // - runs 应为 0(尚未运行)
        // - kstack 应为 0(尚未分配内核栈)
        // - need_resched 应为 0(不需要调度)
        // - parent 应为 NULL(没有父进程)
        // - mm 应为 NULL(内核线程没有用户内存管理)
        // - flags 应为 0(没有特殊标志)
        // - name 应全为 0(尚未设置名称)
        if (idleproc->pgdir == boot_pgdir_pa && idleproc->tf == NULL && !context_init_flag && idleproc->state == PROC_UNINIT && idleproc->pid == -1 && idleproc->runs == 0 && idleproc->kstack == 0 && idleproc->need_resched == 0 && idleproc->parent == NULL && idleproc->mm == NULL && idleproc->flags == 0 && !proc_name_flag)
        {
                cprintf("alloc_proc() 正确!\n");
        }

        // 手动设置 idleproc 的各个字段,因为它是特殊的第 0 号进程
        idleproc->pid = 0;                              // 设置 pid 为 0
        idleproc->state = PROC_RUNNABLE;                // 设置为可运行状态
        idleproc->kstack = (uintptr_t)bootstack;       // 使用启动时的内核栈
        idleproc->need_resched = 1;                     // 标记需要调度(让出 CPU)
        set_proc_name(idleproc, "idle");                // 设置进程名为 "idle"
        nr_process++;                                    // 进程数加 1

        // 将当前进程设置为 idleproc
        // 此时 proc_init 在 idleproc 的上下文中执行
        current = idleproc;

        // 创建第二个内核线程 init_main
        // kernel_thread 会调用 do_fork 创建新进程
        // 参数: init_main 函数指针, "Hello world!!" 参数字符串, 0 标志位
        int pid = kernel_thread(init_main, "Hello world!!", 0);
        if (pid <= 0)
        {
                panic("创建 init_main 失败。\n");
        }

        // 根据返回的 pid 在哈希表中查找刚创建的进程
        initproc = find_proc(pid);
        // 设置该进程的名称为 "init"
        set_proc_name(initproc, "init");

        // 断言检查: idleproc 不为空且 pid 为 0
        assert(idleproc != NULL && idleproc->pid == 0);
        // 断言检查: initproc 不为空且 pid 为 1
        assert(initproc != NULL && initproc->pid == 1);
}

// cpu_idle - 空闲进程的主循环
// 在 kern_init 结束后,第一个内核线程 idleproc 会执行此函数
// 这是一个无限循环,当系统中没有其他进程需要运行时,CPU 就在这里空转
void cpu_idle(void)
{
        // 无限循环
        while (1)
        {
                // 检查当前进程是否需要重新调度
                // need_resched 标志由其他代码(如时钟中断)设置
                if (current->need_resched)
                {
                        // 调用调度器选择下一个要运行的进程
                        // schedule() 会进行进程切换
                        schedule();
                }
        }
}
