#include <proc.h>                                   // 进程控制相关结构与声明
#include <kmalloc.h>                                // 内核动态内存分配接口
#include <string.h>                                 // 字符串与内存帮助函数
#include <sync.h>                                   // 同步与中断控制工具
#include <pmm.h>                                    // 物理内存管理接口
#include <error.h>                                  // 错误码定义
#include <sched.h>                                  // 调度器与 schedule 接口
#include <elf.h>                                    // ELF 文件格式定义
#include <vmm.h>                                    // 虚拟内存管理接口
#include <trap.h>                                   // 中断陷阱帧定义
#include <stdio.h>                                  // 打印函数 cprintf 等
#include <stdlib.h>                                 // 标准库辅助（如 memset）
#include <assert.h>                                 // 断言宏 support
#include <unistd.h>                                 // 用户态接口常量（如 syscall 编号）

/* ------------- 进程/线程 机制设计与实现 -------------
(简化的 Linux 进程/线程 机制)
简介:
    ucore 实现了一个简单的进程/线程机制。进程包含独立的内存空间、至少一个用于执行的线程
用于执行，内核数据（用于管理）、处理器状态（用于上下文切换）、文件（在 lab6 中）等。ucore 需要高效地
 
管理这些细节。在 ucore 中，线程只是进程的一种特殊形式（共享进程的内存）。
进程状态       :     含义                 -- 触发原因
    PROC_UNINIT     :   未初始化               -- alloc_proc
    PROC_SLEEPING   :   睡眠中                 -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   可运行（可能在运行）    -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   僵尸态                 -- do_exit

-----------------------------
进程状态变化:

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
父进程:           proc->parent  （proc 的父进程）
子进程:           proc->cptr    （proc 的子进程）
年长兄弟:         proc->optr    （proc 的年长兄弟）
年幼兄弟:         proc->yptr    （proc 的年幼兄弟）
// 相关的进程系统调用:
相关的进程系统调用:
SYS_exit        : 进程退出，                           -->do_exit
SYS_fork        : 创建子进程，复制 mm                 -->do_fork-->wakeup_proc
SYS_wait        : 等待进程                              -->do_wait
SYS_exec        : fork 后，进程执行程序                  -->加载程序并刷新 mm
SYS_clone       : 创建子线程                           -->do_fork-->wakeup_proc
SYS_yield       : 进程主动请求重新调度，                 -- proc->need_sched=1, 调度器会重新调度该进程
SYS_sleep       : 进程睡眠                              -->do_sleep
SYS_kill        : 杀死进程                              -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : 获取进程的 pid

*/

// 进程集合链表
list_entry_t proc_list;                             // 维护所有进程的双向链表

#define HASH_SHIFT 10                                // pid 哈希表偏移位数
#define HASH_LIST_SIZE (1 << HASH_SHIFT)             // 哈希桶数量
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))        // 计算 pid 的哈希值

// 基于 pid 的进程哈希链表
static list_entry_t hash_list[HASH_LIST_SIZE];       // 每个桶一个链表头

// 空闲进程 (idleproc)
struct proc_struct *idleproc = NULL;                // 永远在运行的 idle 线程
// 初始化进程 (initproc)
struct proc_struct *initproc = NULL;                // 第一个用户态进程 init
// 当前进程
struct proc_struct *current = NULL;                 // CPU 当前绑定的进程

    list_add(&proc_list, &(proc->list_link));        // 插入全局进程链表头部
    proc->yptr = NULL;                              // 新进程默认没有更年轻兄弟
    if ((proc->optr = proc->parent->cptr) != NULL)  // 将老的头孩子挂到 optr
    {
        proc->optr->yptr = proc;                    // 维护双向兄弟关系
    }
    proc->parent->cptr = proc;                      // 把自己设为父进程最新孩子
    nr_process++;                                   // 全局进程计数+1
alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {
    list_del(&(proc->list_link));                   // 从全局链表摘除
    if (proc->optr != NULL)                         // 如存在年长兄弟
    {
        proc->optr->yptr = proc->yptr;              // 让其 younger 指向自己的 younger
    }
    if (proc->yptr != NULL)                         // 如存在年幼兄弟
    {
        proc->yptr->optr = proc->optr;              // 让其 older 指针连回年长兄弟
    }
    else                                            // 否则自己是父进程头孩子
    {
        proc->parent->cptr = proc->optr;            // 父进程头孩子改为年长兄弟
    }
    nr_process--;                                   // 全局进程数减 1
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
            
            // LAB5: 初始化新增的字段
            proc->exit_code = 0;         // 退出码
            proc->wait_state = 0;        // 等待状态
            proc->cptr = NULL;           // 子进程指针
            proc->yptr = NULL;           // 年轻兄弟进程指针
            proc->optr = NULL;           // 年长兄弟进程指针
    }
    return proc;
}

// set_proc_name - 设置进程的名称
char *                                             // 返回被设置的进程名缓冲区
set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));      // 先清空旧名称
    return memcpy(proc->name, name, PROC_NAME_LEN); // 复制至多 PROC_NAME_LEN 字节
}

// get_proc_name - 获取进程的名称
char *                                             // 返回静态缓冲区地址
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];            // 线程不安全的共享缓冲
    memset(name, 0, sizeof(name));                  // 清零缓冲区
    return memcpy(name, proc->name, PROC_NAME_LEN); // 复制真实进程名
}

// set_links - 设置进程的亲缘关系链接 (父子/兄弟)
static void
set_links(struct proc_struct *proc)
{
    list_add(&proc_list, &(proc->list_link));
    proc->yptr = NULL;
    if ((proc->optr = proc->parent->cptr) != NULL)
    {
        proc->optr->yptr = proc;
    }
    proc->parent->cptr = proc;
    nr_process++;
}

// remove_links - 清理进程的亲缘关系链接
static void
remove_links(struct proc_struct *proc)
{
    list_del(&(proc->list_link));
    if (proc->optr != NULL)
    {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL)
    {
        proc->yptr->optr = proc->optr;
    }
    else
    {
        proc->parent->cptr = proc->optr;
    }
    nr_process--;
}

// get_pid - 为进程分配唯一的 pid
static int
get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);            // 确保 pid 空间够大
    struct proc_struct *proc;                        // 临时遍历指针
    list_entry_t *list = &proc_list, *le;            // 遍历全局链表
    static int next_safe = MAX_PID, last_pid = MAX_PID; // 记住下一个安全上界
    if (++last_pid >= MAX_PID)                       // 默认递增 pid
    {
        last_pid = 1;                                // 回绕到 1 避免 0/负数
        goto inside;                                 // 需要重新扫描
    }
    if (last_pid >= next_safe)                       // 如果达到不安全区
    {
    inside:
        next_safe = MAX_PID;                         // 重置安全上界
    repeat:
        le = list;                                   // 从表头开始
        while ((le = list_next(le)) != list)         // 遍历所有进程
        {
            proc = le2proc(le, list_link);           // 取出进程指针
            if (proc->pid == last_pid)               // 如冲突
            {
                if (++last_pid >= next_safe)         // 尝试下一个 pid
                {
                    if (last_pid >= MAX_PID)         // 再次超界就回绕
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;             // 重新设定安全上限
                    goto repeat;                     // 重新检查整个表
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid) // 找到更近的安全边界
            {
                next_safe = proc->pid;               // 下次无需遍历到那么远
            }
        }
    }
    return last_pid;                                 // 返回最终分配的 pid
}

// proc_run - 将指定进程调度到 CPU 上运行
// 注意：在调用 switch_to 之前应加载新进程的页目录基址
void proc_run(struct proc_struct *proc)
{
    if (proc != current)
    {
        // LAB4: 实验4 练习3 (2313255)
        /*
            * 一些有用的宏和函数，你可以在下面实现中使用。
            * 宏或函数：
            *   local_intr_save():        关闭中断
            *   local_intr_restore():     开启中断
            *   lsatp():                  修改 satp 寄存器的值
            *   switch_to():              进程上下文切换
            */
        
        bool intr_flag; // 保存中断状态的标志变量
        struct proc_struct *prev = current, *next = proc; // 记录切换前后进程
        
        // 1. 禁用中断
        local_intr_save(intr_flag);                  // 关中断防止上下文切换被打断
        {
                // 2. 切换当前进程为要运行的进程
            current = proc;                      // 更新全局 current 指针
                
                // 3. 切换页表，使用新进程的地址空间
            lsatp(next->pgdir);                  // 设置 satp 指向新页表
                
                // 4. 实现上下文切换
            switch_to(&(prev->context), &(next->context)); // 调用汇编切换寄存器
        }
        // 5. 允许中断
        local_intr_restore(intr_flag);               // 恢复原中断状态
    }
}

/* forkret -- 新线程/进程进入内核后的第一个入口点
    注: forkret 的地址在 copy_thread 函数中设置
    在 switch_to 之后，当前进程会从这里开始执行。 */
static void
forkret(void)
{
    forkrets(current->tf);                           // 跳转到汇编入口并恢复陷阱帧
}

// hash_proc - 将进程加入按 pid 的哈希链表
static void
hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link)); // 插入对应哈希桶
}

// unhash_proc - 从按 pid 的哈希链表中删除进程
static void
unhash_proc(struct proc_struct *proc)
{
    list_del(&(proc->hash_link));                   // 从哈希桶摘除节点
}

// find_proc - 根据 pid 在哈希链表中查找进程
struct proc_struct *
find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list; // 找到桶并初始化遍历指针
        while ((le = list_next(le)) != list)         // 遍历桶内所有进程
        {
            struct proc_struct *proc = le2proc(le, hash_link); // 复原 proc 指针
            if (proc->pid == pid)                 // 比较 pid 是否匹配
            {
                return proc;                      // 命中则返回
            }
        }
    }
    return NULL;                                     // 未找到返回 NULL
}

// kernel_thread - 使用函数 "fn" 创建一个内核线程
// 注: 临时 trapframe (tf) 的内容会在 do_fork->copy_thread 中被复制到 proc->tf
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    struct trapframe tf;                             // 临时构造的内核态陷阱帧
    memset(&tf, 0, sizeof(struct trapframe));        // 初始化为 0
    tf.gpr.s0 = (uintptr_t)fn;                       // 第一个参数为函数指针
    tf.gpr.s1 = (uintptr_t)arg;                      // 第二个参数为传入参数
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE; // 保证在内核态启动
    tf.epc = (uintptr_t)kernel_thread_entry;         // 入口设为内核线程启动函数
    return do_fork(clone_flags | CLONE_VM, 0, &tf);  // 通过 do_fork 创建线程，共享内核地址空间
}

// setup_kstack - 为进程分配大小为 KSTACKPAGE 的内核栈页
static int
setup_kstack(struct proc_struct *proc)
{
    struct Page *page = alloc_pages(KSTACKPAGE);     // 分配连续的内核栈页
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);    // 记录栈的内核虚拟地址
        return 0;                                    // 成功返回 0
    }
    return -E_NO_MEM;                                // 否则返回内存不足
}

// put_kstack - 释放进程内核栈占用的内存
static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE); // 释放此前分配的栈页
}

// setup_pgdir - 分配一页作为页目录 (PDT)
static int
setup_pgdir(struct mm_struct *mm)
{
    struct Page *page;                               // 将被用作页目录的物理页
    if ((page = alloc_page()) == NULL)               // 分配失败直接返回
    {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);                   // 转换为内核可访问的虚拟地址
    memcpy(pgdir, boot_pgdir_va, PGSIZE);            // 拷贝内核页表模板

    mm->pgdir = pgdir;                               // 记录到 mm 结构
    return 0;                                        // 成功
}

// put_pgdir - 释放页目录 (PDT) 占用的内存
static void
put_pgdir(struct mm_struct *mm)
{
    free_page(kva2page(mm->pgdir));                  // 释放页目录对应的物理页
}

// copy_mm - 根据 clone_flags 复制或共享当前进程的 mm
//         - 如果 clone_flags & CLONE_VM，则共享；否则复制
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    struct mm_struct *mm, *oldmm = current->mm;      // 当前进程的地址空间指针

    /* 当前是一个内核线程 */
    if (oldmm == NULL)
    {
        return 0;                                    // 内核线程直接共享内核地址空间
    }
    if (clone_flags & CLONE_VM)
    {
        mm = oldmm;                                  // 共享父进程 mm
        goto good_mm;                                // 直接走共享分支
    }
    int ret = -E_NO_MEM;                             // 默认错误码
    if ((mm = mm_create()) == NULL)                  // 创建新的 mm 结构
    {
        goto bad_mm;
    }
    if (setup_pgdir(mm) != 0)                        // 初始化页目录
    {
        goto bad_pgdir_cleanup_mm;
    }
    lock_mm(oldmm);                                  // 复制父进程映射前加锁
    {
        ret = dup_mmap(mm, oldmm);                   // 复制所有 VMA/页表
    }
    unlock_mm(oldmm);                                // 复制完成释放锁

    if (ret != 0)
    {
        goto bad_dup_cleanup_mmap;                   // 复制失败则回滚
    }

good_mm:
    mm_count_inc(mm);                                // 增加共享引用
    proc->mm = mm;                                   // 绑定新地址空间
    proc->pgdir = PADDR(mm->pgdir);                  // 记录页目录物理地址
    return 0;
bad_dup_cleanup_mmap:
    exit_mmap(mm);                                   // 清理复制出的映射
bad_pgdir_cleanup_mm:
    mm_destroy(mm);                                  // 销毁 mm 结构
bad_mm:
    return ret;                                      // 返回错误码
}

// copy_thread - 在进程的内核栈顶设置 trapframe
//             - 并设置进程的内核入口点和内核栈
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
{
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1; // 栈顶预留一帧作为 trapframe
    *(proc->tf) = *tf;                              // 复制父进程寄存器状态

    // 将 a0 设为 0，以便子进程知道它是刚 fork 出来的
    proc->tf->gpr.a0 = 0;                           // 子进程看到返回值 0
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp; // 设定用户/内核栈指针

    proc->context.ra = (uintptr_t)forkret;          // 内核态返回地址指向 forkret
    proc->context.sp = (uintptr_t)(proc->tf);       // 内核态栈指针指向 trapframe
}

/* do_fork -     父进程为新子进程创建资源
 * @clone_flags: 指导如何克隆子进程
 * @stack:       父进程用户栈指针。若 stack==0，表示 fork 内核线程。
 * @tf:          trapframe 信息，会复制到子进程的 proc->tf
 */
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    int ret = -E_NO_FREE_PROC;                        // 默认提示进程数超限
        struct proc_struct *proc;
        if (nr_process >= MAX_PROCESS)
        {
                goto fork_out;
        }
    ret = -E_NO_MEM;                                 // 后续错误均视为内存不足
        // LAB4: 实验4 练习2 (2314035)
        /*
         * 一些有用的宏和函数，你可以在下面实现中使用。
         * 宏或函数：
         *   alloc_proc:   创建并初始化 proc_struct（实验4:练习1）
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
        if ((proc = alloc_proc()) == NULL) {             // 1. 创建新的进程控制块
                goto fork_out;
        }
        
        // 设置父进程为当前进程
        proc->parent = current;                          // 建立父子关系
        
        // 2. 调用 setup_kstack 分配内核栈
        if (setup_kstack(proc) != 0) {                   // 分配失败回滚
                goto bad_fork_cleanup_proc;
        }
        
        // 3. 调用 copy_mm 复制或共享 mm
        if (copy_mm(clone_flags, proc) != 0) {           // 复制地址空间失败
                goto bad_fork_cleanup_kstack;
        }
        
        // 4. 调用 copy_thread 设置 trapframe 和 context
        copy_thread(proc, stack, tf);                    // 初始化子进程上下文
        
        // 5. 插入 hash_list 和 proc_list，并设置进程家族关系
        bool intr_flag;
        local_intr_save(intr_flag);
        {
                proc->pid = get_pid();                         // 分配唯一 pid
                hash_proc(proc);                              // 加入 pid 哈希表
                // LAB5: 使用 set_links 来设置进程家族关系（父子、兄弟）
                set_links(proc);
        }
        local_intr_restore(intr_flag);
        
        // 6. 调用 wakeup_proc 使新进程变为 RUNNABLE
        wakeup_proc(proc);                                // 设置为 RUNNABLE 并加入调度
        
        // 7. 用子进程 pid 设置返回值
        ret = proc->pid;                                 // 返回子进程 PID
        
fork_out:
        return ret;

bad_fork_cleanup_kstack:
        put_kstack(proc);
bad_fork_cleanup_proc:
        kfree(proc);
        goto fork_out;
}


// do_exit - 由 sys_exit 调用
//   1. 调用 exit_mmap、put_pgdir 和 mm_destroy 来释放进程的大部分内存空间
//   2. 将进程状态设置为 PROC_ZOMBIE，然后调用 wakeup_proc(parent) 通知父进程回收其资源
//   3. 调用调度器切换到其他进程
int do_exit(int error_code)
{
    if (current == idleproc)
    {
        panic("idleproc exit.\n");                  // idleproc 不允许退出
    }
    if (current == initproc)
    {
        panic("initproc exit.\n");                  // initproc 也不可退出
    }
    struct mm_struct *mm = current->mm;
    if (mm != NULL)
    {
        lsatp(boot_pgdir_pa);                         // 切换回内核页表
        if (mm_count_dec(mm) == 0)                    // 仅最后一个引用需要释放
        {
            exit_mmap(mm);                            // 解除所有 VMA 映射
            put_pgdir(mm);                            // 释放页目录物理页
            mm_destroy(mm);                           // 销毁 mm 结构体
        }
        current->mm = NULL;                          // 清除指针
    }
    current->state = PROC_ZOMBIE;                     // 进入僵尸态
    current->exit_code = error_code;                  // 记录退出码
    bool intr_flag;
    struct proc_struct *proc;
    local_intr_save(intr_flag);                       // 操作进程关系需关中断
    {
        proc = current->parent;
        if (proc->wait_state == WT_CHILD)
        {
            wakeup_proc(proc);                        // 唤醒等待的父进程
        }
        while (current->cptr != NULL)
        {
            proc = current->cptr;
            current->cptr = proc->optr;

            proc->yptr = NULL;                        // 断开孩子的 younger 链
            if ((proc->optr = initproc->cptr) != NULL)
            {
                initproc->cptr->yptr = proc;          // 挂到 initproc 的孩子链
            }
            proc->parent = initproc;                  // 改父指针为 initproc
            initproc->cptr = proc;                    // 插入为 initproc 最新孩子
            if (proc->state == PROC_ZOMBIE)
            {
                if (initproc->wait_state == WT_CHILD)
                {
                    wakeup_proc(initproc);            // 若 init 正在 wait 则唤醒
                }
            }
        }
    }
    local_intr_restore(intr_flag);                    // 恢复中断
    schedule();                                       // 切换到其他进程
    panic("do_exit will not return!! %d.\n", current->pid); // 理论上不可到达
}

/* load_icode - 将二进制程序（ELF 格式）的内容加载为当前进程的新镜像
 * @binary:  二进制程序内容在内存中的地址
 * @size:    二进制程序内容的大小
 */
static int
load_icode(unsigned char *binary, size_t size)
{
    if (current->mm != NULL)
    {
        panic("load_icode: current->mm must be empty.\n"); // 调用前需确保无旧 mm
    }

    int ret = -E_NO_MEM;
    struct mm_struct *mm;
    //(1) 为当前进程创建新的 mm
    if ((mm = mm_create()) == NULL)                 // 创建新的 mm 结构体
    {
        goto bad_mm;
    }
    //(2) 创建页目录（PDT），并将 mm->pgdir 设为该页目录的内核虚拟地址
    if (setup_pgdir(mm) != 0)                       // 分配页目录失败
    {
        goto bad_pgdir_cleanup_mm;
    }
    //(3) 复制 TEXT/DATA 段，并为 BSS 段分配内存
    struct Page *page = NULL;
    //(3.1) 获取二进制程序的 ELF 文件头
    struct elfhdr *elf = (struct elfhdr *)binary;
    //(3.2) 获取程序头表的起始地址（ELF）
    struct proghdr *ph = (struct proghdr *)(binary + elf->e_phoff);
    //(3.3) 检查程序是否合法
    if (elf->e_magic != ELF_MAGIC)                  // 魔数不符说明非法 ELF
    {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }

    uint32_t vm_flags, perm;
    struct proghdr *ph_end = ph + elf->e_phnum;
    for (; ph < ph_end; ph++)                       // 遍历所有程序头
    {
        //(3.4) 遍历每个程序段（program header）
        if (ph->p_type != ELF_PT_LOAD)
        {
            continue;
        }
        if (ph->p_filesz > ph->p_memsz)             // 文件段大小不能超过内存段
        {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }
        if (ph->p_filesz == 0)
        {
            // 跳过
        }
        //(3.5) 调用 mm_map 为 (ph->p_va, ph->p_memsz) 设置新的 VMA
        vm_flags = 0, perm = PTE_U | PTE_V;
        if (ph->p_flags & ELF_PF_X)
            vm_flags |= VM_EXEC;
        if (ph->p_flags & ELF_PF_W)
            vm_flags |= VM_WRITE;
        if (ph->p_flags & ELF_PF_R)
            vm_flags |= VM_READ;
        // 根据 RISC-V 的要求修改权限位（perm）
        if (vm_flags & VM_READ)
            perm |= PTE_R;
        if (vm_flags & VM_WRITE)
            perm |= (PTE_W | PTE_R);
        if (vm_flags & VM_EXEC)
            perm |= PTE_X;
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) // 建立 VMA
        {
            goto bad_cleanup_mmap;
        }
        unsigned char *from = binary + ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);

        ret = -E_NO_MEM;

        //(3.6) 为每个程序段分配内存，并将段内容复制到进程地址空间 (la, la+end)
        end = ph->p_va + ph->p_filesz;
        //(3.6.1) 复制二进制程序的 TEXT/DATA 段
        while (start < end)                         // 复制 TEXT/DATA 到物理页
        {
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL)
            {
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la)
            {
                size -= la - end;
            }
            memcpy(page2kva(page) + off, from, size);
            start += size, from += size;
        }

        //(3.6.2) 构建二进制程序的 BSS 段
        end = ph->p_va + ph->p_memsz;
        if (start < la)                             // 处理同一页内剩余 BSS 空洞
        {
            /* ph->p_memsz == ph->p_filesz */
            if (start == end)
            {
                continue;
            }
            off = start + PGSIZE - la, size = PGSIZE - off;
            if (end < la)
            {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
            assert((end < la && start == end) || (end >= la && start == la));
        }
        while (start < end)                         // 后续页填充 BSS 为 0
        {
            if ((page = pgdir_alloc_page(mm->pgdir, la, perm)) == NULL)
            {
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PGSIZE - off, la += PGSIZE;
            if (end < la)
            {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }
    //(4) 构建用户栈内存
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) // 建立用户栈 VMA
    {
        goto bad_cleanup_mmap;
    }
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 2 * PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 3 * PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 4 * PGSIZE, PTE_USER) != NULL);

    //(5) 设置当前进程的 mm、sr3，并将 satp 寄存器设为页目录的物理地址
    mm_count_inc(mm);                                // 记录 mm 被当前进程独占
    current->mm = mm;                                // 绑定 mm 指针
    current->pgdir = PADDR(mm->pgdir);               // 记录页目录物理地址
    lsatp(PADDR(mm->pgdir));                         // 切换 satp

    //(6) 为用户态环境设置 trapframe
    struct trapframe *tf = current->tf;
    // 保留 sstatus 的值
    uintptr_t sstatus = tf->status;
    memset(tf, 0, sizeof(struct trapframe));         // 清空旧 trapframe
    /* LAB5: 实验5 练习1 - 2314035
     * 应设置 tf->gpr.sp, tf->epc, tf->status
     * 注意: 如果正确设置 trapframe，则用户态进程可以从内核返回到用户态。因此：
     *          tf->gpr.sp 应为用户栈顶（sp 的值）
     *          tf->epc 应为用户程序的入口点（sepc 的值）
     *          tf->status 应为用户程序合适的 sstatus 值
     *          提示: 查看 riscv.h 中 SSTATUS_SPP、SSTATUS_SPIE 的含义
     */
    
    // 设置用户栈指针：指向用户栈顶
    tf->gpr.sp = USTACKTOP;                          // 设置用户栈顶
    
    // 设置程序入口地址：ELF文件头中的 e_entry 字段
    tf->epc = elf->e_entry;                          // 入口为 ELF e_entry
    
    // 设置 sstatus 寄存器：
    // - SSTATUS_SPIE = 1: sret 返回后开启中断
    // - SSTATUS_SPP = 0: sret 返回到用户态 (U mode)
    // 由于前面 memset 已经清零，SPP 已经是 0，只需设置 SPIE
    tf->status = (sstatus & ~SSTATUS_SPP) | SSTATUS_SPIE; // 返回用户态并开启中断

    ret = 0;
out:
    return ret;
bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    goto out;
}

// do_execve - 调用 exit_mmap(mm) & put_pgdir(mm) 回收当前进程的内存空间
//           - 调用 load_icode 根据二进制程序设置新的进程内存空间
int do_execve(const char *name, size_t len, unsigned char *binary, size_t size)
{
    struct mm_struct *mm = current->mm;
    if (!user_mem_check(mm, (uintptr_t)name, len, 0))
    {
        return -E_INVAL;
    }
    if (len > PROC_NAME_LEN)
    {
        len = PROC_NAME_LEN;
    }

    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));
    memcpy(local_name, name, len);

    if (mm != NULL)
    {
        cputs("mm != NULL");                         // 调试输出，提示仍有旧地址空间
        lsatp(boot_pgdir_pa);                         // 切换回内核页表
        if (mm_count_dec(mm) == 0)                    // 引用为 0 才释放
        {
            exit_mmap(mm);                            // 解除用户映射
            put_pgdir(mm);                            // 释放页目录
            mm_destroy(mm);                           // 销毁 mm 结构
        }
        current->mm = NULL;                          // 清空当前 mm 指针
    }
    int ret;
    if ((ret = load_icode(binary, size)) != 0)
    {
        goto execve_exit;
    }
    set_proc_name(current, local_name);
    return 0;

execve_exit:
    do_exit(ret);
    panic("already exit: %e.\n", ret);
}

// do_yield - 请求调度器重新调度
int do_yield(void)
{
    current->need_resched = 1;                       // 标记需要重新调度
    return 0;                                        // 立即返回让调度器运行
}

// do_wait - 等待一个或任意处于 PROC_ZOMBIE 状态的子进程，并释放该子进程的内核栈和 proc_struct
// 注: 只有在 do_wait 函数返回后，子进程的所有资源才会被释放。
int do_wait(int pid, int *code_store)
{
    struct mm_struct *mm = current->mm;             // 当前进程地址空间
    if (code_store != NULL)
    {
        if (!user_mem_check(mm, (uintptr_t)code_store, sizeof(int), 1))
        {
            return -E_INVAL;                        // 结果缓冲区无效
        }
    }

    struct proc_struct *proc;
    bool intr_flag, haskid;
repeat:
    haskid = 0;                                     // 重置标记
    if (pid != 0)
    {
        proc = find_proc(pid);                      // 查找指定 pid
        if (proc != NULL && proc->parent == current)
        {
            haskid = 1;                             // 确认存在子进程
            if (proc->state == PROC_ZOMBIE)
            {
                goto found;                         // 已退出直接收尸
            }
        }
    }
    else
    {
        proc = current->cptr;                       // 遍历所有子进程
        for (; proc != NULL; proc = proc->optr)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found;                         // 找到僵尸
            }
        }
    }
    if (haskid)
    {
        current->state = PROC_SLEEPING;             // 进入睡眠等待
        current->wait_state = WT_CHILD;             // 标记等待子进程
        schedule();                                 // 让出 CPU
        if (current->flags & PF_EXITING)
        {
            do_exit(-E_KILLED);                     // 如果被杀则直接退出
        }
        goto repeat;                                // 被唤醒后重新检查
    }
    return -E_BAD_PROC;                             // 没有子进程可等

found:
    if (proc == idleproc || proc == initproc)
    {
        panic("wait idleproc or initproc.\n");      // 防止等待关键进程
    }
    if (code_store != NULL)
    {
        *code_store = proc->exit_code;              // 返回子进程退出码
    }
    local_intr_save(intr_flag);                     // 修改链表需关中断
    {
        unhash_proc(proc);                          // 从哈希表去除
        remove_links(proc);                         // 断开亲缘关系
    }
    local_intr_restore(intr_flag);                  // 恢复中断
    put_kstack(proc);                               // 释放内核栈
    kfree(proc);                                    // 释放 PCB
    return 0;
}

// do_kill - 通过设置进程的 flags 为 PF_EXITING 来终止指定 pid 的进程
int do_kill(int pid)
{
    struct proc_struct *proc;
    if ((proc = find_proc(pid)) != NULL)            // 查找目标进程
    {
        if (!(proc->flags & PF_EXITING))            // 若尚未退出
        {
            proc->flags |= PF_EXITING;              // 设置退出标志
            if (proc->wait_state & WT_INTERRUPTED)  // 若在可中断等待
            {
                wakeup_proc(proc);                  // 唤醒以便退出
            }
            return 0;
        }
        return -E_KILLED;
    }
    return -E_INVAL;
}

// kernel_execve - 由 user_main 内核线程调用，用于通过 SYS_exec 系统调用执行用户程序
static int
kernel_execve(const char *name, unsigned char *binary, size_t size)
{
    int64_t ret = 0, len = strlen(name);            // 记录系统调用返回值及长度
    //   ret = do_execve(name, len, binary, size); // 注释掉的原始调用：执行 do_execve
    asm volatile(
        "li a0, %1\n"
        "lw a1, %2\n"
        "lw a2, %3\n"
        "lw a3, %4\n"
        "lw a4, %5\n"
        "li a7, 10\n"
        "ebreak\n"
        "sw a0, %0\n"
        : "=m"(ret)
        : "i"(SYS_exec), "m"(name), "m"(len), "m"(binary), "m"(size)
        : "memory");
    cprintf("ret = %d\n", ret);                   // 打印执行结果
    return ret;                                     // 返回 syscall 返回码
}

#define __KERNEL_EXECVE(name, binary, size) ({           \
    cprintf("kernel_execve: pid = %d, name = \"%s\".\n", \
            current->pid, name);                         \
    kernel_execve(name, binary, (size_t)(size));         \
})

#define KERNEL_EXECVE(x) ({                                    \
    extern unsigned char _binary_obj___user_##x##_out_start[], \
        _binary_obj___user_##x##_out_size[];                   \
    __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,    \
                    _binary_obj___user_##x##_out_size);        \
})

#define __KERNEL_EXECVE2(x, xstart, xsize) ({   \
    extern unsigned char xstart[], xsize[];     \
    __KERNEL_EXECVE(#x, xstart, (size_t)xsize); \
})

#define KERNEL_EXECVE2(x, xstart, xsize) __KERNEL_EXECVE2(x, xstart, xsize)

// user_main - 用于执行用户程序的内核线程
static int
user_main(void *arg)
{
#ifdef TEST
    KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);       // 根据宏指定的程序运行
#else
    KERNEL_EXECVE(exit);                             // 默认执行 user/exit 程序
#endif
    panic("user_main execve failed.\n");            // 理论上不会返回
}

// init_main - 第二个内核线程，用来创建 user_main 内核线程
static int
init_main(void *arg)
{
    size_t nr_free_pages_store = nr_free_pages();     // 记录初始空闲页数
    size_t kernel_allocated_store = kallocated();     // 记录内核分配量

    int pid = kernel_thread(user_main, NULL, 0);     // 启动 user_main 线程
    if (pid <= 0)
    {
        panic("create user_main failed.\n");
    }

    while (do_wait(0, NULL) == 0)                   // 等待所有子进程结束
    {
        schedule();
    }

    cprintf("all user-mode processes have quit.\n"); // 所有用户态进程退出
    assert(initproc->cptr == NULL && initproc->yptr == NULL && initproc->optr == NULL);
    assert(nr_process == 2);
    assert(list_next(&proc_list) == &(initproc->list_link));
    assert(list_prev(&proc_list) == &(initproc->list_link));

    cprintf("init check memory pass.\n");
    return 0;
}

// proc_init - 初始化第一个内核线程 idleproc（"idle"）并
//           - 创建第二个内核线程 init_main
void proc_init(void)
{
    int i;

    list_init(&proc_list);                           // 初始化全局进程链表
    for (i = 0; i < HASH_LIST_SIZE; i++)             // 初始化每个 pid 哈希桶
    {
        list_init(hash_list + i);
    }

    if ((idleproc = alloc_proc()) == NULL)          // 创建 idleproc
    {
        panic("cannot alloc idleproc.\n");
    }

    idleproc->pid = 0;                              // pid 为 0
    idleproc->state = PROC_RUNNABLE;                // 默认可运行
    idleproc->kstack = (uintptr_t)bootstack;        // 使用引导栈
    idleproc->need_resched = 1;                     // 允许调度
    set_proc_name(idleproc, "idle");                // 命名 idle
    nr_process++;                                   // 计入总数

    current = idleproc;                             // 当前进程指向 idle

    int pid = kernel_thread(init_main, NULL, 0);    // 创建 init_main
    if (pid <= 0)
    {
        panic("create init_main failed.\n");
    }

    initproc = find_proc(pid);                      // 保存 initproc 指针
    set_proc_name(initproc, "init");               // 命名 init

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}

// cpu_idle - 在 kern_init 结束时，第一个内核线程 idleproc 会执行以下工作
void cpu_idle(void)
{
    while (1)
    {
        if (current->need_resched)                  // 需要调度时调用调度器
        {
            schedule();                             // 切换到可运行任务
        }
    }
}
