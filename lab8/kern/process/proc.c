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
#include <unistd.h>
#include <fs.h>
#include <vfs.h>
#include <sysfile.h>
/* ------------- 进程/线程机制设计与实现 -------------
(简单的 Linux 进程/线程机制)
介绍：
  ucore 实现了一个简单的进程/线程机制。进程包含独立的内存空间，至少一个用于执行的线程，
  内核数据（用于管理），处理器状态（用于上下文切换），文件（在 lab6 中），等等。
  ucore 需要有效地管理所有这些细节。在 ucore 中，线程只是一种特殊的进程（共享进程的内存）。
------------------------------
进程状态        :     含义                    -- 原因
    PROC_UNINIT     :   未初始化                -- alloc_proc
    PROC_SLEEPING   :   睡眠                    -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   可运行（可能正在运行）   -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   几乎死亡                -- do_exit

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
父进程：          proc->parent  (proc 是子进程)
子进程：          proc->cptr    (proc 是父进程)
年长兄弟：        proc->optr    (proc 是更年轻的兄弟)
年轻兄弟：        proc->yptr    (proc 是更年长的兄弟)
-----------------------------
与进程相关的系统调用：
SYS_exit        : 进程退出,                               -->do_exit
SYS_fork        : 创建子进程, 复制 mm                      -->do_fork-->wakeup_proc
SYS_wait        : 等待进程                                -->do_wait
SYS_exec        : fork 后, 进程执行一个程序                 -->load a program and refresh the mm
SYS_clone       : 创建子线程                               -->do_fork-->wakeup_proc
SYS_yield       : 进程标记自己需要重新调度,                   -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : 进程睡眠                                 -->do_sleep
SYS_kill        : 杀死进程                                 -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : 获取进程的 pid

*/

// the process set's list
// 进程集合列表
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// has list for process set based on pid
// 基于pid的进程集合哈希列表
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
// 空闲进程
struct proc_struct *idleproc = NULL;
// init proc
// 初始化进程
struct proc_struct *initproc = NULL;
// current proc
// 当前进程
struct proc_struct *current = NULL;

static int nr_process = 0; // 进程数量

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

// alloc_proc - alloc a proc_struct and init all fields of proc_struct
// alloc_proc - 分配一个proc_struct并初始化所有字段
static struct proc_struct *
alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct)); // 分配内存
    if (proc != NULL)
    {
        // LAB4:填写你在lab4中实现的代码 已填写
        /*
         * proc_struct 中的以下字段需要初始化
         *       enum proc_state state;                      // 进程状态
         *       int pid;                                    // 进程 ID
         *       int runs;                                   // 进程运行时间
         *       uintptr_t kstack;                           // 进程内核栈
         *       volatile bool need_resched;                 // 布尔值: 是否需要重新调度以释放 CPU?
         *       struct proc_struct *parent;                 // 父进程
         *       struct mm_struct *mm;                       // 进程的内存管理字段
         *       struct context context;                     // 切换到这里运行进程
         *       struct trapframe *tf;                       // 当前中断的中断帧
         *       uintptr_t pgdir;                            // 页目录表 (PDT) 的基地址
         *       uint32_t flags;                             // 进程标志
         *       char name[PROC_NAME_LEN + 1];               // 进程名称
         */

        // LAB5:填写你在lab5中实现的代码 (update LAB4 steps)已填写
        /*
         * proc_struct 中的以下字段（在 LAB5 中添加）需要初始化
         *       uint32_t wait_state;                        // 等待状态
         *       struct proc_struct *cptr, *yptr, *optr;     // 进程间的关系
         */

        // LAB6:填写你在lab6中实现的代码 (update LAB5 steps)已填写
        /*
         * proc_struct 中的以下字段（在 LAB6 中添加）需要初始化
         *       struct run_queue *rq;                       // 包含进程的运行队列
         *       list_entry_t run_link;                      // 运行队列中的连接条目
         *       int time_slice;                             // 占用 CPU 的时间片
         *       skew_heap_entry_t lab6_run_pool;            // 运行池中的条目 (lab6 stride)
         *       uint32_t lab6_stride;                       // 步长值 (lab6 stride)
         *       uint32_t lab6_priority;                     // 优先级值 (lab6 stride)
         */

        //LAB8 2314076 : (更新 LAB6 步骤)
        /*
         * proc_struct 中的以下字段（在 LAB6 中添加）需要初始化
         *       struct files_struct * filesp;                文件结构指针        
         */
        proc->state = PROC_UNINIT; // 设置初始状态为未初始化
        proc->pid = -1; // PID设为-1
        proc->runs = 0; // 运行次数初始化为0
        proc->kstack = 0; // 内核栈指针初始化为0
        proc->need_resched = 0; // 是否需要调度设为否
        proc->parent = NULL; // 父进程指针
        proc->mm = NULL; // 内存管理结构指针
        memset(&(proc->context), 0, sizeof(struct context)); // 上下文清零
        proc->tf = NULL; // 中断帧指针
        proc->pgdir = boot_pgdir_pa; // 页目录表基址（物理地址）
        proc->flags = 0; // 标志位
        memset(proc->name, 0, PROC_NAME_LEN); // 进程名清零
        // lab5 add:
        proc->wait_state = 0; // 等待状态
        proc->cptr = proc->optr = proc->yptr = NULL; // 亲属关系指针初始化
        proc->rq = NULL;              // 初始化运行队列为空
        list_init(&(proc->run_link)); // 初始化运行队列的指针
        proc->time_slice = 0; // 时间片初始化
        proc->lab6_run_pool.left = proc->lab6_run_pool.right = proc->lab6_run_pool.parent = NULL; // 斜堆节点初始化
        proc->lab6_stride = 0; // 步长初始化
        proc->lab6_priority = 0; // 优先级初始化

        // lab8 add: 初始化文件结构指针
        proc->filesp = NULL; // 文件表指针初始化为空
    }
    return proc;
}

// set_proc_name - set the name of proc
// set_proc_name - 设置进程名称
char *
set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
// get_proc_name - 获取进程名称
char *
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// set_links - set the relation links of process
// set_links - 设置进程关系链接
static void
set_links(struct proc_struct *proc)
{
    list_add(&proc_list, &(proc->list_link)); // 添加到进程列表
    proc->yptr = NULL; // 初始化更年轻的兄弟指针
    if ((proc->optr = proc->parent->cptr) != NULL) // 设置更年长的兄弟指针为父进程当前的子进程
    {
        proc->optr->yptr = proc; // 更新该兄弟的younger指针指向自己
    }
    proc->parent->cptr = proc; // 将自己设置为父进程的子进程头
    nr_process++;
}

// remove_links - clean the relation links of process
// remove_links - 清除进程关系链接
static void
remove_links(struct proc_struct *proc)
{
    list_del(&(proc->list_link)); // 从进程列表删除
    if (proc->optr != NULL) // 如果有更年长的兄弟
    {
        proc->optr->yptr = proc->yptr; // 更新其younger指针
    }
    if (proc->yptr != NULL) // 如果有更年轻的兄弟
    {
        proc->yptr->optr = proc->optr; // 更新其older指针
    }
    else // 如果是父进程的第一个子进程
    {
        proc->parent->cptr = proc->optr; // 更新父进程的child指针
    }
    nr_process--;
}

// get_pid - alloc a unique pid for process
// get_pid - 为进程分配唯一PID
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

// proc_run - make process "proc" running on cpu
// NOTE: before call switch_to, should load  base addr of "proc"'s new PDT
// proc_run - 让进程"proc"在CPU上运行
// 注意：在调用switch_to之前，应加载"proc"的新PDT基地址
void proc_run(struct proc_struct *proc)
{
    // LAB4:填写你在lab4中实现的代码
        /*
        * 下面的实现中可以使用一些有用的宏、函数和定义。
        * 宏或函数：
        *   local_intr_save():        禁用中断
        *   local_intr_restore():     启用中断
        *   lcr3():                   修改 CR3 寄存器的值
        *   switch_to():              两个进程之间的上下文切换
        */
    //LAB8 2313255 : (更新 LAB4 步骤)
      /*
       * proc_struct 中的以下字段（在 LAB6 中添加）需要初始化
       *       在 switch_to() 之前；你应该刷新 tlb
       *        宏 or 函数：
       *       flush_tlb():          刷新 tlb        
       */
    if (proc != current) { // 如果要运行的进程不是当前进程
        bool intr_flag;
        struct proc_struct *prev = current, *next = proc;
        local_intr_save(intr_flag); // 关中断
        {
            current = proc; // 更新当前进程指针
            // 加载新进程的页目录表
            lsatp(next->pgdir); // 切换页表
            // 刷新 TLB
            flush_tlb();
            // 进行上下文切换
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag); // 开中断
    }
}

// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
// forkret -- 新线程/进程的第一个内核入口点
// 注意：forkret的地址在copy_thread函数中设置
//       在switch_to之后，当前进程将在此处执行。
static void
forkret(void)
{
    forkrets(current->tf); // 跳转到forkrets，从中断帧恢复
}

// hash_proc - add proc into proc hash_list
// hash_proc - 将proc添加到proc哈希列表中
static void
hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// unhash_proc - delete proc from proc hash_list
// unhash_proc - 从proc哈希列表中删除proc
static void
unhash_proc(struct proc_struct *proc)
{
    list_del(&(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
// find_proc - 根据pid从proc哈希列表中查找proc
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

// kernel_thread - create a kernel thread using "fn" function
// NOTE: the contents of temp trapframe tf will be copied to
//       proc->tf in do_fork-->copy_thread function
// kernel_thread - 使用"fn"函数创建一个内核线程
// 注意：临时中断帧tf的内容将在do_fork-->copy_thread函数中复制到proc->tf
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    tf.gpr.s0 = (uintptr_t)fn; // s0存放函数地址
    tf.gpr.s1 = (uintptr_t)arg; // s1存放参数
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE; // 设置状态寄存器
    tf.epc = (uintptr_t)kernel_thread_entry; // 设置入口点
    return do_fork(clone_flags | CLONE_VM, 0, &tf); // 调用do_fork
}

// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
// setup_kstack - 分配KSTACKPAGE大小的页面作为进程内核栈
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

// put_kstack - free the memory space of process kernel stack
// put_kstack - 释放进程内核栈的内存空间
static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// setup_pgdir - alloc one page as PDT
// setup_pgdir - 分配一页作为PDT（页目录表）
static int
setup_pgdir(struct mm_struct *mm)
{
    struct Page *page;
    if ((page = alloc_page()) == NULL)
    {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    memcpy(pgdir, boot_pgdir_va, PGSIZE); // 复制内核页目录表

    mm->pgdir = pgdir;
    return 0;
}

// put_pgdir - free the memory space of PDT
// put_pgdir - 释放PDT的内存空间
static void
put_pgdir(struct mm_struct *mm)
{
    free_page(kva2page(mm->pgdir));
}

// copy_mm - process "proc" duplicate OR share process "current"'s mm according clone_flags
//         - if clone_flags & CLONE_VM, then "share" ; else "duplicate"
// copy_mm - 根据clone_flags复制或共享"current"进程的mm给"proc"进程
//         - 如果clone_flags & CLONE_VM，则"共享"；否则"复制"
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    struct mm_struct *mm, *oldmm = current->mm;

    /* current is a kernel thread */
    /* 当前进程是内核线程 */
    if (oldmm == NULL)
    {
        return 0;
    }
    if (clone_flags & CLONE_VM)
    {
        mm = oldmm;
        goto good_mm;
    }
    int ret = -E_NO_MEM;
    if ((mm = mm_create()) == NULL) // 创建新的mm结构
    {
        goto bad_mm;
    }
    if (setup_pgdir(mm) != 0) // 设置页目录
    {
        goto bad_pgdir_cleanup_mm;
    }
    lock_mm(oldmm);
    {
        ret = dup_mmap(mm, oldmm); // 复制内存映射
    }
    unlock_mm(oldmm);

    if (ret != 0)
    {
        goto bad_dup_cleanup_mmap;
    }

good_mm:
    mm_count_inc(mm); // 增加引用计数
    proc->mm = mm;
    proc->pgdir = PADDR(mm->pgdir); // 设置CR3寄存器的物理地址
    return 0;
bad_dup_cleanup_mmap:
    exit_mmap(mm);
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    return ret;
}

// copy_thread - setup the trapframe on the  process's kernel stack top and
//             - setup the kernel entry point and stack of process
// copy_thread - 在进程的内核栈顶设置trapframe
//             - 设置进程的内核入口点和栈
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
{
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1; // 设置trapframe位置
    *(proc->tf) = *tf; // 复制trapframe

    // Set a0 to 0 so a child process knows it's just forked
    // 将a0设置为0，以便子进程知道它是刚刚fork出来的
    proc->tf->gpr.a0 = 0;
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp; // 设置栈指针

    proc->context.ra = (uintptr_t)forkret; // 设置返回地址为forkret
    proc->context.sp = (uintptr_t)(proc->tf); // 设置上下文栈指针
}
// copy_files&put_files function used by do_fork in LAB8
// copy the files_struct from current to proc
// LAB8中do_fork使用的copy_files和put_files函数
// 将files_struct从current复制到proc
static int
copy_files(uint32_t clone_flags, struct proc_struct *proc)
{
    struct files_struct *filesp, *old_filesp = current->filesp;
    assert(old_filesp != NULL);

    if (clone_flags & CLONE_FS) // 如果共享文件系统
    {
        filesp = old_filesp;
        goto good_files_struct;
    }

    int ret = -E_NO_MEM;
    if ((filesp = files_create()) == NULL) // 创建新的文件结构
    {
        goto bad_files_struct;
    }

    if ((ret = dup_files(filesp, old_filesp)) != 0) // 复制文件描述符表
    {
        goto bad_dup_cleanup_fs;
    }

good_files_struct:
    files_count_inc(filesp); // 增加引用计数
    proc->filesp = filesp;
    return 0;

bad_dup_cleanup_fs:
    files_destroy(filesp);
bad_files_struct:
    return ret;
}

// decrease the ref_count of files, and if ref_count==0, then destroy files_struct
// 减少文件的引用计数，如果ref_count==0，则销毁files_struct
static void
put_files(struct proc_struct *proc)
{
    struct files_struct *filesp = proc->filesp;
    if (filesp != NULL)
    {
        if (files_count_dec(filesp) == 0)
        {
            files_destroy(filesp);
        }
    }
}

/* do_fork -     parent process for a new child process
 * @clone_flags: used to guide how to clone the child process
 * @stack:       the parent's user stack pointer. if stack==0, It means to fork a kernel thread.
 * @tf:          the trapframe info, which will be copied to child process's proc->tf
 */
/* do_fork -     为新的子进程创建父进程
 * @clone_flags: 用于指导如何克隆子进程
 * @stack:       父进程的用户栈指针。如果stack==0，表示fork一个内核线程。
 * @tf:          中断帧信息，将被复制到子进程的proc->tf
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
    // LAB8:EXERCISE2 2313255 提示：如何复制父进程 proc_struct 中的 fs？
    // LAB4:填写你在lab4中实现的代码
    /*
     * 下面的实现中可以使用一些有用的宏、函数和定义。
     * 宏或函数：
     *   alloc_proc:   创建一个 proc 结构并初始化字段 (lab4:exercise1)
     *   setup_kstack: 分配大小为 KSTACKPAGE 的页面作为进程内核栈
     *   copy_mm:      根据 clone_flags 复制或共享进程 "proc" 与进程 "current" 的 mm
     *                 如果 clone_flags & CLONE_VM，则 "共享"；否则 "复制"
     *   copy_thread:  在进程的内核栈顶设置中断帧，并
     *                 设置内核入口点和进程栈
     *   hash_proc:    将进程添加到进程哈希列表中
     *   get_pid:      为进程分配唯一的 pid
     *   wakeup_proc:  设置 proc->state = PROC_RUNNABLE
     * 变量：
     *   proc_list:    进程集合列表
     *   nr_process:   进程集合的数量
     */

    //    1. 调用 alloc_proc 分配一个 proc_struct
    //    2. 调用 setup_kstack 为子进程分配内核栈
    //    3. 根据 clone_flag 调用 copy_mm 复制或共享 mm
    //    4. 调用 copy_thread 在 proc_struct 中设置 tf 和 context
    //    5. 将 proc_struct 插入 hash_list 和 proc_list
    //    6. 调用 wakeup_proc 使新的子进程变为 RUNNABLE
    //    7. 使用子进程的 pid 设置返回值 ret

    // LAB5:填写你在lab5中实现的代码 (更新 LAB4 步骤)
    /* 一些函数
     *    set_links:  设置进程的关系链接。 另见：remove_links: 清除进程的关系链接
     *    -------------------
     *    更新步骤 1: 设置子进程的父进程为当前进程，确保当前进程的 wait_state 为 0
     *    更新步骤 5: 将 proc_struct 插入 hash_list && proc_list，设置进程的关系链接
     */
    
    // 1. call alloc_proc to allocate a proc_struct
    // 1. 调用alloc_proc分配一个proc_struct
    if ((proc = alloc_proc()) == NULL) {
        goto fork_out;
    }
    
    // set child proc's parent to current process
    // 设置子进程的父进程为当前进程
    proc->parent = current;
    assert(current->wait_state == 0);
    
    // 2. call setup_kstack to allocate a kernel stack for child process
    // 2. 调用setup_kstack为子进程分配内核栈
    if (setup_kstack(proc) != 0) {
        goto bad_fork_cleanup_proc;
    }
    
    // LAB8: copy the fs in parent's proc_struct
    // LAB8: 复制父进程proc_struct中的文件系统信息
    if (copy_files(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_kstack;
    }
    
    // 3. call copy_mm to dup OR share mm according clone_flag
    // 3. 根据clone_flag调用copy_mm复制或共享内存管理结构
    if (copy_mm(clone_flags, proc) != 0) {
        goto bad_fork_cleanup_fs;
    }
    
    // 4. call copy_thread to setup tf & context in proc_struct
    // 4. 调用copy_thread在proc_struct中设置trapframe和上下文
    copy_thread(proc, stack, tf);
    
    // 5. insert proc_struct into hash_list && proc_list
    // 5. 将proc_struct插入hash_list和proc_list
    bool intr_flag;
    local_intr_save(intr_flag); // 关中断
    {
        proc->pid = get_pid(); // 获取PID
        hash_proc(proc); // 插入哈希表
        set_links(proc); // 设置进程关系
    }
    local_intr_restore(intr_flag); // 开中断
    
    // 6. call wakeup_proc to make the new child process RUNNABLE
    // 6. 调用wakeup_proc使新的子进程变为RUNNABLE
    wakeup_proc(proc);
    
    // 7. set ret vaule using child proc's pid
    // 7. 使用子进程的pid设置返回值ret
    ret = proc->pid;
    
fork_out:
    return ret;

bad_fork_cleanup_fs: // for LAB8
    put_files(proc); // 释放文件结构
bad_fork_cleanup_kstack:
    put_kstack(proc); // 释放内核栈
bad_fork_cleanup_proc:
    kfree(proc); // 释放进程结构
    goto fork_out;
}

// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
//   3. call scheduler to switch to other process
// do_exit - 由sys_exit调用
//   1. 调用exit_mmap & put_pgdir & mm_destroy释放进程几乎所有的内存空间
//   2. 设置进程状态为PROC_ZOMBIE，然后调用wakeup_proc(parent)请求父进程回收自己。
//   3. 调用调度程序切换到其他进程
int do_exit(int error_code)
{
    if (current == idleproc)
    {
        panic("idleproc exit.\n");
    }
    if (current == initproc)
    {
        panic("initproc exit.\n");
    }
    struct mm_struct *mm = current->mm;
    if (mm != NULL) // 如果是用户进程
    {
        lsatp(boot_pgdir_pa); // 切换到内核页表
        if (mm_count_dec(mm) == 0) // 如果引用计数为0
        {
            exit_mmap(mm); // 释放内存映射
            put_pgdir(mm); // 释放页表
            mm_destroy(mm); // 销毁mm结构
        }
        current->mm = NULL;
        put_files(current); // 释放文件结构
    }
    current->state = PROC_ZOMBIE; // 设置状态为僵尸
    current->exit_code = error_code; // 设置退出码
    bool intr_flag;
    struct proc_struct *proc;
    local_intr_save(intr_flag); // 关中断
    {
        proc = current->parent;
        if (proc->wait_state == WT_CHILD) // 如果父进程在等待子进程
        {
            wakeup_proc(proc); // 唤醒父进程
        }
        while (current->cptr != NULL) // 如果当前进程有子进程
        {
            proc = current->cptr;
            current->cptr = proc->optr; // 取出一个子进程

            proc->yptr = NULL;
            if ((proc->optr = initproc->cptr) != NULL) // 挂到initproc下
            {
                initproc->cptr->yptr = proc;
            }
            proc->parent = initproc; // 父进程改为initproc
            initproc->cptr = proc;
            if (proc->state == PROC_ZOMBIE) // 如果子进程已经是僵尸
            {
                if (initproc->wait_state == WT_CHILD)
                {
                    wakeup_proc(initproc); // 唤醒initproc
                }
            }
        }
    }
    local_intr_restore(intr_flag); // 开中断
    schedule(); // 调度
    panic("do_exit will not return!! %d.\n", current->pid);
}

// LAB8中load_icode使用的load_icode_read函数
// 该函数用于从文件描述符fd中读取指定偏移offset处长度为len的数据到buf中
static int
load_icode_read(int fd, void *buf, size_t len, off_t offset)
{
    int ret;
    // 使用sysfile_seek定位到文件的指定偏移位置
    if ((ret = sysfile_seek(fd, offset, LSEEK_SET)) != 0)
    {
        return ret; // 定位失败返回错误码
    }
    // 使用sysfile_read读取指定长度的数据
    if ((ret = sysfile_read(fd, buf, len)) != len)
    {
        // 如果读取的长度不等于请求的长度，返回错误
        // 如果sysfile_read返回负数则为错误码，否则返回-1表示读取不完整
        return (ret < 0) ? ret : -1;
    }
    return 0; // 读取成功
}

// load_icode -  由sys_exec-->do_execve调用
// 该函数负责将ELF二进制程序加载到当前进程的内存空间中，并设置好用户栈和参数
// 命令行参数数组就是来自于 kargv
static int
load_icode(int fd, int argc, char **kargv)
{
    /* LAB8:EXERCISE2 2314076  提示：如何将句柄 fd 指向的文件加载到进程内存中？如何设置 argc/argv？
     * 宏或函数：
     *  mm_create        - 创建一个 mm
     *  setup_pgdir      - 在 mm 中设置 pgdir
     *  load_icode_read  - 读取程序文件的原始内容
     *  mm_map           - 建立新的 vma
     *  pgdir_alloc_page - 为 TEXT/DATA/BSS/stack 部分分配新内存
     *  lsatp            - 更新页目录地址寄存器 -- CR3
     */
    //你可以按照你已经完成的 LAB5 的代码来完成
    /* (1) 为当前进程创建一个新的 mm
     * (2) 创建一个新的 PDT，并且 mm->pgdir= PDT 的内核虚拟地址
     * (3) 将二进制文件中的 TEXT/DATA/BSS 部分复制到进程的内存空间
     *    (3.1) 读取文件中的原始内容并解析 elfhdr
     *    (3.2) 读取文件中的原始内容并根据 elfhdr 中的信息解析 proghdr
     *    (3.3) 调用 mm_map 建立与 TEXT/DATA 相关的 vma
     *    (3.4) 调用 pgdir_alloc_page 为 TEXT/DATA 分配页面，读取文件中的内容
     *          并将它们复制到新分配的页面中
     *    (3.5) 调用 pgdir_alloc_page 为 BSS 分配页面，将这些页面清零
     * (4) 调用 mm_map 设置用户栈，并将参数放入用户栈
     * (5) 设置当前进程的 mm, cr3, 重置 pgdir (使用 lsatp 宏)
     * (6) 在用户栈中设置 uargc 和 uargv
     * (7) 为用户环境设置中断帧
     * (8) 如果上述步骤失败，你应该清理环境。
     */
    
    assert(argc >= 0 && argc <= EXEC_MAX_ARG_NUM);
    
    // ==================== Step 1: 创建 mm ====================
    int ret = -E_NO_MEM;
    struct mm_struct *mm;
    
    // 创建一个新的内存管理结构 mm
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    // 为这个 mm 分配并初始化页目录表
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }
    
    // ==================== Step 2: 读取 ELF 头部 ====================
    struct elfhdr __elf, *elf = &__elf;
    // 从文件中读取 ELF 头部信息
    if ((ret = load_icode_read(fd, elf, sizeof(struct elfhdr), 0)) != 0) {
        goto bad_elf_cleanup_pgdir; // 读取失败
    }
    // 检查 ELF 魔数，确认是否为合法的 ELF 文件
    if (elf->e_magic != ELF_MAGIC) {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir; // 文件格式错误
    }
    
    // ==================== Step 3: 加载各个段 ====================
    struct proghdr __ph, *ph = &__ph;
    uint32_t vm_flags, perm;
    
    // 遍历所有的程序头（Section Headers）
    for (int i = 0; i < elf->e_phnum; i++) {
        // 计算当前程序头的偏移量
        off_t phoff = elf->e_phoff + sizeof(struct proghdr) * i;
        // 读取程序头信息
        if ((ret = load_icode_read(fd, ph, sizeof(struct proghdr), phoff)) != 0) {
            goto bad_cleanup_mmap;
        }
        // 我们只关心 LOAD 类型的段，这是需要加载到内存中的段
        if (ph->p_type != ELF_PT_LOAD) {
            continue;
        }
        // 检查文件大小是否超过内存大小，这是非法的
        if (ph->p_filesz > ph->p_memsz) {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }
        // 注意：PT_LOAD 段可能是纯 BSS 段 (p_filesz == 0, p_memsz > 0)。
        // 我们必须仍然映射它并分配/清零页面，否则用户程序在访问全局变量时会出错。
        if (ph->p_memsz == 0) {
            continue; // 如果内存大小为0，则忽略
        }
        
        // 根据段的标志设置 VMA 的权限标志
        vm_flags = 0;
        perm = PTE_U;  // 用户态可访问
        if (ph->p_flags & ELF_PF_X) {
            vm_flags |= VM_EXEC; // 可执行及其对应的页表项权限
            perm |= PTE_X;  // 可执行
        }
        if (ph->p_flags & ELF_PF_W) {
            vm_flags |= VM_WRITE; // 可写及其对应的页表项权限
            perm |= PTE_W;  // 可写
        }
        if (ph->p_flags & ELF_PF_R) {
            vm_flags |= VM_READ; // 可读及其对应的页表项权限
            perm |= PTE_R;  // 可读
        }
        
        // 创建 VMA（虚拟内存区域），建立虚拟地址与该段的映射关系
        // 这一步设置了 TEXT/DATA/BSS 段的虚拟地址范围和权限
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
            goto bad_cleanup_mmap;
        }

        // 处理纯 BSS 段：只分配内存并清零，无需从文件读取
        if (ph->p_filesz == 0) {
            uintptr_t start = ph->p_va, end = ph->p_va + ph->p_memsz;
            uintptr_t la = ROUNDDOWN(start, PGSIZE); // 向下对齐到页边界
            while (start < end) {
                // 分配物理页面并建立映射
                struct Page *page = pgdir_alloc_page(mm->pgdir, la, perm);
                if (page == NULL) {
                    ret = -E_NO_MEM; // 内存分配失败
                    goto bad_cleanup_mmap;
                }
                // 计算需要清零的偏移和大小
                size_t off = start - la;
                size_t size = PGSIZE - off;
                la += PGSIZE;
                if (end < la) {
                    size -= la - end;
                }
                // 将页面内容清零
                memset((void *)(page2kva(page) + off), 0, size);
                start += size;
            }
            continue; // 处理下一个段
        }
        
        // --- 对于非纯 BSS 段：分配页面并从文件读取内容 ---
        off_t offset = ph->p_offset;// 获取该段在文件中的偏移量
        size_t off, size;
        // start: 段的起始虚拟地址
        // la: 将起始地址向下对齐到页边界 (Page Align)，作为分配页面的基准
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);
        
        // --- 处理文件内容部分 (TEXT/DATA) ---
        // --- 将文件中的数据（代码或已初始化的全局变量）读入内存页 ---
        end = ph->p_va + ph->p_filesz;//end 指向文件中实际数据的结束位置
        // 循环读取文件中的数据
        while (start < end) {
            // 为虚拟地址la分配物理页面并建立映射
            struct Page *page = pgdir_alloc_page(mm->pgdir, la, perm);
            if (page == NULL) {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            // 计算页内偏移和读取大小
            off = start - la;
            size = PGSIZE - off; // 默认读满剩下的半页或整页
            la += PGSIZE; // 更新 la 到下一页的边界
            if (end < la) {
                size -= la - end;
            }
            // 从文件的offset位置中读取size字节的数据到新分配的内存页
            if ((ret = load_icode_read(fd, page2kva(page) + off, size, offset)) != 0) {
                goto bad_cleanup_mmap; // 读取失败
            }
            // 更新 start 和 offset 以继续读取下一个块
            start += size;
            offset += size;
        }
        
        // --- 处理 BSS 部分（如果有）：内存大小大于文件大小的部分 ---
        // 重新定义 end 为该段在内存中的最终结束位置
        end = ph->p_va + ph->p_memsz;
        
        // 处理上一个页面的剩余部分，如果文件大小不是页对齐的，最后一页还没填满
        // 但是 BSS 紧接着开始，那么这页剩余的空间必须清零
        if (start < la) {
            if (start == end) {
                continue; // 刚好结束
            }
            off = start + PGSIZE - la;// 计算在当前页内的起始清零位置
            size = PGSIZE - off; // 计算需要清零的大小
            if (end < la) {
                size -= la - end;
            }
            // 将文件内容之后的内存部分清零（BSS 起始部分）
            memset(page2kva(get_page(mm->pgdir, start, NULL)) + off, 0, size);
            start += size;
            assert((end < la && start == end) || (end >= la && start == la));
        }
        // 如果 BSS 很大，超出了刚才提到的“最后一页”
        // 则处理剩余的 BSS 页面：分配新页并全清零
        while (start < end) {
            // 分配物理页面并建立映射
            struct Page *page = pgdir_alloc_page(mm->pgdir, la, perm);
            if (page == NULL) {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            // 整页都属于 BSS，所以计算出本页内需要清零的范围（通常是整页，除非是该段的最后几字节）
            off = start - la;
            size = PGSIZE - off;
            la += PGSIZE;
            if (end < la) {
                size -= la - end;
            }
            // 将本页内的 BSS 部分清零
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }

    
    // ==================== Step 4: 设置用户栈 ====================
    // 用户栈通常位于用户空间的最高地址附近
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    // 建立用户栈的 VMA，大约 1MB 大小
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }
    // 立即分配几页物理内存给用户栈，防止缺页异常（虽然也可以按需分配，但这里为了简单直接分配了）
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 2*PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 3*PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 4*PGSIZE, PTE_USER) != NULL);
    
    // ==================== Step 5: 更新进程状态 ====================
    mm_count_inc(mm); // 增加 mm 的引用计数
    current->mm = mm; // 设置当前进程的 mm 结构
    current->pgdir = PADDR(mm->pgdir); // 设置页目录物理地址
    lsatp(PADDR(mm->pgdir)); // 切换页表，激活新的地址空间
    // 刷新 TLB 已经在 lsatp 中隐含或者是必要的下一步操作，但这里 lsatp 写入 satp 寄存器会生效
    
    // ==================== Step 6: 设置命令行参数 argc/argv 到用户栈 ====================
    // 从而当用户程序启动时，可以通过 main 函数的参数获取命令行参数
    // 注意：此时已经切换页表，当前是在内核态，但我们需要写入用户栈
    // 这里的 stacktop 是用户虚拟地址
    
    uintptr_t stacktop = USTACKTOP;
    
    // 6.1: 将参数字符串复制到用户栈（从栈顶向下增长）
    uintptr_t argv_ptrs[EXEC_MAX_ARG_NUM]; // 用于保存每个参数字符串在用户栈中的地址
    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(kargv[i]) + 1;  // 包含 '\0' 的长度
        stacktop -= len;
        // 使用 get_page 获取对应的物理页，再转换为内核虚拟地址以便写入
        struct Page *page = get_page(mm->pgdir, stacktop, NULL);
        uintptr_t kva = (uintptr_t)page2kva(page) + (stacktop & (PGSIZE - 1));
        strcpy((char *)kva, kargv[i]); // 复制字符串内容
        argv_ptrs[i] = stacktop;  // 记录该参数在用户栈中的起始地址，用于构造 argv 数组
    }
    
    // 6.2: 参数指针数组需要指针对齐 (通常为 8 字节/64位系统)
    stacktop = ROUNDDOWN(stacktop, sizeof(uintptr_t));
    
    // 6.3: 压入 argv[argc] = NULL (C 语言约定：argv 最后一个元素必须是 NULL，表示参数结束)
    stacktop -= sizeof(uintptr_t);
    {
        struct Page *page = get_page(mm->pgdir, stacktop, NULL);
        uintptr_t kva = (uintptr_t)page2kva(page) + (stacktop & (PGSIZE - 1));
        *(uintptr_t *)kva = 0;
    }
    
    // 6.4: 压入 argv 指针数组（从后往前压，这样 argv[0] 在最低地址）
    // 实际上是构建 argv 数组的内容。argv[0] 指向第一个字符串，argv[1] 指向第二个...
    // 这样 main 函数的 argv 参数就能正确指向每个字符串
    for (int i = argc - 1; i >= 0; i--) {
        stacktop -= sizeof(uintptr_t);
        struct Page *page = get_page(mm->pgdir, stacktop, NULL);
        uintptr_t kva = (uintptr_t)page2kva(page) + (stacktop & (PGSIZE - 1));
        *(uintptr_t *)kva = argv_ptrs[i]; // 写入字符串的地址
    }
    
    // 记录 argv 数组本身的起始地址（即二级指针 char **argv 的值）
    uintptr_t uargv = stacktop;
    
    // 6.5: 压入 argc (虽然 RISC-V 通过寄存器 a0 传递，但有些实现也压栈)
    // 这里为了兼容性或者特定的 ABI 规范
    stacktop -= sizeof(uintptr_t);
    {
        struct Page *page = get_page(mm->pgdir, stacktop, NULL);
        uintptr_t kva = (uintptr_t)page2kva(page) + (stacktop & (PGSIZE - 1));
        *(uintptr_t *)kva = argc;
    }
    
    // ==================== Step 7: 设置 trapframe ====================
    struct trapframe *tf = current->tf;
    
    // 清空中断帧，准备新的上下文
    memset(tf, 0, sizeof(struct trapframe));
    tf->gpr.sp = stacktop; // 设置用户栈指针
    tf->gpr.a0 = argc;     // 设置系统调用参数/程序入口参数 a0 = argc
    tf->gpr.a1 = uargv;    // 设置系统调用参数/程序入口参数 a1 = argv
    tf->epc = elf->e_entry; // 设置程序入口点 (Entry Point Counter)
    // 设置处理器状态：SSTATUS_SPIE 表示之前中断是开启的，SSTATUS_SPP=0 表示返回用户模式
    tf->status = (read_csr(sstatus) | SSTATUS_SPIE) & ~SSTATUS_SPP; 
    
    // ==================== Step 8: 关闭文件 ====================
    sysfile_close(fd); // 加载完毕，关闭文件描述符
    
    ret = 0;
    return ret;
    
bad_cleanup_mmap:
    exit_mmap(mm); // 释放映射
bad_elf_cleanup_pgdir:
    put_pgdir(mm); // 释放页目录
bad_pgdir_cleanup_mm:
    mm_destroy(mm); // 销毁 mm 结构
bad_mm:
    return ret;
}

// this function isn't very correct in LAB8
// 这个函数在LAB8中不太正确
static void
put_kargv(int argc, char **kargv)
{
    while (argc > 0)
    {
        kfree(kargv[--argc]);
    }
}

static int
copy_kargv(struct mm_struct *mm, int argc, char **kargv, const char **argv)
{
    int i, ret = -E_INVAL;
    if (!user_mem_check(mm, (uintptr_t)argv, sizeof(const char *) * argc, 0))
    {
        return ret;
    }
    for (i = 0; i < argc; i++)
    {
        char *buffer;
        if ((buffer = kmalloc(EXEC_MAX_ARG_LEN + 1)) == NULL)
        {
            goto failed_nomem;
        }
        if (!copy_string(mm, buffer, argv[i], EXEC_MAX_ARG_LEN + 1))
        {
            kfree(buffer);
            goto failed_cleanup;
        }
        kargv[i] = buffer;
    }
    return 0;

failed_nomem:
    ret = -E_NO_MEM;
failed_cleanup:
    put_kargv(i, kargv);
    return ret;
}

// do_execve - 调用exit_mmap(mm)&put_pgdir(mm)来回收当前进程的内存空间
//           - 调用load_icode根据二进制程序设置新的内存空间。
int do_execve(const char *name, int argc, const char **argv)
{
    static_assert(EXEC_MAX_ARG_LEN >= FS_MAX_FPATH_LEN);
    struct mm_struct *mm = current->mm;
    if (!(argc >= 1 && argc <= EXEC_MAX_ARG_NUM))
    {
        return -E_INVAL;
    }

    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));

    char *kargv[EXEC_MAX_ARG_NUM];
    const char *path;

    int ret = -E_INVAL;

    lock_mm(mm);
    if (name == NULL)
    {
        snprintf(local_name, sizeof(local_name), "<null> %d", current->pid);
    }
    else
    {
        if (!copy_string(mm, local_name, name, sizeof(local_name)))
        {
            unlock_mm(mm);
            return ret;
        }
    }
    if ((ret = copy_kargv(mm, argc, kargv, argv)) != 0)
    {
        unlock_mm(mm);
        return ret;
    }
    path = argv[0];
    unlock_mm(mm);
    files_closeall(current->filesp);

    /* sysfile_open will check the first argument path, thus we have to use a user-space pointer, and argv[0] may be incorrect */
    /* sysfile_open将检查第一个参数path，因此我们必须使用用户空间指针，argv[0]可能不正确 */
    int fd;
    if ((ret = fd = sysfile_open(path, O_RDONLY)) < 0)
    {
        goto execve_exit;
    }
    if (mm != NULL) // 如果有内存管理结构（非内核线程）
    {
        lsatp(boot_pgdir_pa); // 切换回内核页表
        if (mm_count_dec(mm) == 0)
        {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
        current->mm = NULL;
    }
    ret = -E_NO_MEM;
    ;
    if ((ret = load_icode(fd, argc, kargv)) != 0) // 加载新程序
    {
        goto execve_exit;
    }
    put_kargv(argc, kargv);
    set_proc_name(current, local_name);
    return 0;

execve_exit:
    put_kargv(argc, kargv);
    do_exit(ret);
    panic("already exit: %e.\n", ret);
}

// do_yield - 请求调度器重新调度
int do_yield(void)
{
    current->need_resched = 1;
    return 0;
}

// do_wait - 等待一个或任意一个状态为PROC_ZOMBIE的子进程，并释放内核栈的内存空间
//         - 该子进程的proc struct。
// 注意：只有在do_wait函数之后，子进程的所有资源才被释放。
int do_wait(int pid, int *code_store)
{
    struct mm_struct *mm = current->mm;
    if (code_store != NULL)
    {
        if (!user_mem_check(mm, (uintptr_t)code_store, sizeof(int), 1))
        {
            return -E_INVAL;
        }
    }

    struct proc_struct *proc;
    bool intr_flag, haskid;
repeat:
    haskid = 0;
    if (pid != 0) // 如果指定了pid
    {
        proc = find_proc(pid);
        if (proc != NULL && proc->parent == current)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found;
            }
        }
    }
    else // 如果pid==0，等待任意子进程
    {
        proc = current->cptr;
        for (; proc != NULL; proc = proc->optr)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found;
            }
        }
    }
    if (haskid) // 如果有子进程但没死
    {
        current->state = PROC_SLEEPING;
        current->wait_state = WT_CHILD;
        schedule(); // 调度
        if (current->flags & PF_EXITING)
        {
            do_exit(-E_KILLED);
        }
        goto repeat;
    }
    return -E_BAD_PROC;

found:
    if (proc == idleproc || proc == initproc)
    {
        panic("wait idleproc or initproc.\n");
    }
    if (code_store != NULL)
    {
        *code_store = proc->exit_code;
    }
    local_intr_save(intr_flag);
    {
        unhash_proc(proc);
        remove_links(proc);
    }
    local_intr_restore(intr_flag);
    put_kstack(proc);
    kfree(proc);
    return 0;
}
// do_kill - 通过设置进程标志为PF_EXITING来杀死指定pid的进程
int do_kill(int pid)
{
    struct proc_struct *proc;
    if ((proc = find_proc(pid)) != NULL)
    {
        if (!(proc->flags & PF_EXITING))
        {
            proc->flags |= PF_EXITING;
            if (proc->wait_state & WT_INTERRUPTED)
            {
                wakeup_proc(proc);
            }
            return 0;
        }
        return -E_KILLED;
    }
    return -E_INVAL;
}

// kernel_execve - 构建新的中断帧，在内核中执行do_execve，并通过__trapret返回用户模式
static int
kernel_execve(const char *name, const char **argv)
{
    int64_t argc = 0, ret;
    while (argv[argc] != NULL)
    {
        argc++;
    }
    struct trapframe *old_tf = current->tf;
    struct trapframe *new_tf = (struct trapframe *)(current->kstack + KSTACKSIZE - sizeof(struct trapframe));
    memcpy(new_tf, old_tf, sizeof(struct trapframe));
    current->tf = new_tf;
    ret = do_execve(name, argc, argv);
    asm volatile(
        "mv sp, %0\n"
        "j __trapret\n"
        :
        : "r"(new_tf)
        : "memory");
    return ret;
}

#define __KERNEL_EXECVE(name, path, ...) ({              \
    const char *argv[] = {path, ##__VA_ARGS__, NULL};    \
    cprintf("kernel_execve: pid = %d, name = \"%s\".\n", \
            current->pid, name);                         \
    kernel_execve(name, argv);                           \
})

#define KERNEL_EXECVE(x, ...) __KERNEL_EXECVE(#x, #x, ##__VA_ARGS__)

#define KERNEL_EXECVE2(x, ...) KERNEL_EXECVE(x, ##__VA_ARGS__)

#define __KERNEL_EXECVE3(x, s, ...) KERNEL_EXECVE(x, #s, ##__VA_ARGS__)

#define KERNEL_EXECVE3(x, s, ...) __KERNEL_EXECVE3(x, s, ##__VA_ARGS__)

// user_main - 用于执行用户程序的内核线程
static int
user_main(void *arg)
{
#ifdef TEST
#ifdef TESTSCRIPT
    KERNEL_EXECVE3(TEST, TESTSCRIPT);
#else
    KERNEL_EXECVE2(TEST);
#endif
#else
    KERNEL_EXECVE(sh);
#endif
    panic("user_main execve failed.\n");
}

// init_main - 第二个内核线程，用于创建user_main内核线程
static int
init_main(void *arg)
{
    int ret;
    if ((ret = vfs_set_bootfs("disk0:")) != 0)
    {
        panic("set boot fs failed: %e.\n", ret);
    }
    size_t nr_free_pages_store = nr_free_pages();
    size_t kernel_allocated_store = kallocated();

    int pid = kernel_thread(user_main, NULL, 0);
    if (pid <= 0)
    {
        panic("create user_main failed.\n");
    }
    extern void check_sync(void);
    // check_sync();                // check philosopher sync problem

    while (do_wait(0, NULL) == 0)
    {
        schedule();
    }

    fs_cleanup();

    cprintf("all user-mode processes have quit.\n");
    assert(initproc->cptr == NULL && initproc->yptr == NULL && initproc->optr == NULL);
    assert(nr_process == 2);
    assert(list_next(&proc_list) == &(initproc->list_link));
    assert(list_prev(&proc_list) == &(initproc->list_link));

    cprintf("init check memory pass.\n");
    return 0;
}

// proc_init - 设置第一个内核线程idleproc "idle"
//           - 创建第二个内核线程init_main
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
        panic("cannot alloc idleproc.\n");
    }

    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;

    if ((idleproc->filesp = files_create()) == NULL)
    {
        panic("create filesp (idleproc) failed.\n");
    }
    files_count_inc(idleproc->filesp);

    set_proc_name(idleproc, "idle");
    nr_process++;

    current = idleproc;
    
    cprintf("proc_init: creating init_main kernel thread\n");

    int pid = kernel_thread(init_main, NULL, 0);
    if (pid <= 0)
    {
        panic("create init_main failed.\n");
    }
    
    cprintf("proc_init: init_main created with pid = %d\n", pid);

    initproc = find_proc(pid);
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}

// cpu_idle - 在kern_init结束时，第一个内核线程idleproc将做以下工作
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
// 对于LAB6，设置进程的优先级（值越大获得的CPU时间越多）
void lab6_set_priority(uint32_t priority)
{
    cprintf("set priority to %d\n", priority);
    if (priority == 0)
        current->lab6_priority = 1;
    else
        current->lab6_priority = priority;
}
// do_sleep - 设置当前进程状态为睡眠，并添加带有"time"的定时器
//          - 然后调用调度器。如果进程再次运行，首先删除定时器。
int do_sleep(unsigned int time)
{
    if (time == 0)
    {
        return 0;
    }
    bool intr_flag;
    local_intr_save(intr_flag); // 关中断
    timer_t __timer, *timer = timer_init(&__timer, current, time); // 初始化定时器
    current->state = PROC_SLEEPING; // 设置进程状态为睡眠
    current->wait_state = WT_TIMER; // 设置等待状态为等待定时器
    add_timer(timer); // 添加定时器
    local_intr_restore(intr_flag); // 开中断

    schedule(); // 调度，放弃CPU

    del_timer(timer); // 删除定时器
    return 0;
}
