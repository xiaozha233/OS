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
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// 基于 pid 的进程哈希链表
static list_entry_t hash_list[HASH_LIST_SIZE];

// 空闲进程 (idleproc)
struct proc_struct *idleproc = NULL;
// 初始化进程 (initproc)
struct proc_struct *initproc = NULL;
// 当前进程
struct proc_struct *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);

// alloc_proc - 分配一个 proc_struct 并初始化其所有字段
static struct proc_struct *
alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {
            // LAB4: 实验4 练习1 (2314076)
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
char *
set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - 获取进程的名称
char *
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
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

/* forkret -- 新线程/进程进入内核后的第一个入口点
    注: forkret 的地址在 copy_thread 函数中设置
    在 switch_to 之后，当前进程会从这里开始执行。 */
static void
forkret(void)
{
    forkrets(current->tf);
}

// hash_proc - 将进程加入按 pid 的哈希链表
static void
hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// unhash_proc - 从按 pid 的哈希链表中删除进程
static void
unhash_proc(struct proc_struct *proc)
{
    list_del(&(proc->hash_link));
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

// kernel_thread - 使用函数 "fn" 创建一个内核线程
// 注: 临时 trapframe (tf) 的内容会在 do_fork->copy_thread 中被复制到 proc->tf
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    // 构造临时 trapframe，将 fn 和 arg 写入寄存器（s0/s1）
    // 并把tf.epc 设为 kernel_thread_entry（汇编入口点）
    struct trapframe tf;
    memset(&tf, 0, sizeof(struct trapframe));
    tf.gpr.s0 = (uintptr_t)fn;
    tf.gpr.s1 = (uintptr_t)arg;
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
    tf.epc = (uintptr_t)kernel_thread_entry;
    // 调用 do_fork 创建真正的proc_struct
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

// setup_kstack - 为进程分配大小为 KSTACKPAGE 的内核栈页
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

// put_kstack - 释放进程内核栈占用的内存
static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// setup_pgdir - 分配一页作为页目录 (PDT)
static int
setup_pgdir(struct mm_struct *mm)
{
    // 首先使用alloc_page创建一页作为页目录（PDT，即最高级页表）
    struct Page *page;
    if ((page = alloc_page()) == NULL)
    {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    // 把 boot_pgdir_va （内核启动页目录）复制到新页目录中，确保新进程页表也包含内核的映射（内核地址永远映射）
    // 当 CPU 切换到新进程运行（CR3 指向新页目录）时，CPU 依然能够"看见"并访问内核的代码和数据。
    memcpy(pgdir, boot_pgdir_va, PGSIZE);

    mm->pgdir = pgdir;
    return 0;
}

// put_pgdir - 释放页目录 (PDT) 占用的内存
static void
put_pgdir(struct mm_struct *mm)
{
    free_page(kva2page(mm->pgdir));
}

// copy_mm - 根据 clone_flags 复制或共享当前进程的 mm
//         - 如果 clone_flags & CLONE_VM，则共享；否则复制
static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    struct mm_struct *mm, *oldmm = current->mm;

    /* 当前是一个内核线程 */
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
    if ((mm = mm_create()) == NULL)
    {
        goto bad_mm;
    }
    if (setup_pgdir(mm) != 0)
    {
        goto bad_pgdir_cleanup_mm;
    }
    lock_mm(oldmm);
    {
        ret = dup_mmap(mm, oldmm);
    }
    unlock_mm(oldmm);

    if (ret != 0)
    {
        goto bad_dup_cleanup_mmap;
    }

good_mm:
    mm_count_inc(mm);
    proc->mm = mm;
    proc->pgdir = PADDR(mm->pgdir);
    return 0;
bad_dup_cleanup_mmap:
    exit_mmap(mm);
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    return ret;
}

// copy_thread - 在进程的内核栈顶设置之前构造好的 trapframe
//             - 并设置进程的内核入口点和内核栈
static void
copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
{
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
    *(proc->tf) = *tf;

    // 将 a0 设为 0，以便子进程知道它是刚 fork 出来的
    proc->tf->gpr.a0 = 0;
    // esp 为 0 表示内核线程，为sp赋值为proc->tf，否则为用户栈指针
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
    
    // 5. 插入 hash_list 和 proc_list，并设置进程家族关系
    bool intr_flag;
    local_intr_save(intr_flag);
    {
            proc->pid = get_pid();
            hash_proc(proc);
            // LAB5: 使用 set_links 来设置进程家族关系（父子、兄弟）
            set_links(proc);
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
//   1. 调用 exit_mmap、put_pgdir 和 mm_destroy 来释放进程的大部分内存空间
//   2. 将进程状态设置为 PROC_ZOMBIE，然后调用 wakeup_proc(parent) 通知父进程回收其资源
//   3. 调用调度器切换到其他进程
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
    if (mm != NULL)
    {
        lsatp(boot_pgdir_pa);
        if (mm_count_dec(mm) == 0)
        {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
        current->mm = NULL;
    }
    current->state = PROC_ZOMBIE;
    current->exit_code = error_code;
    bool intr_flag;
    struct proc_struct *proc;
    local_intr_save(intr_flag);
    {
        proc = current->parent;
        if (proc->wait_state == WT_CHILD)
        {
            wakeup_proc(proc);
        }
        while (current->cptr != NULL)
        {
            proc = current->cptr;
            current->cptr = proc->optr;

            proc->yptr = NULL;
            if ((proc->optr = initproc->cptr) != NULL)
            {
                initproc->cptr->yptr = proc;
            }
            proc->parent = initproc;
            initproc->cptr = proc;
            if (proc->state == PROC_ZOMBIE)
            {
                if (initproc->wait_state == WT_CHILD)
                {
                    wakeup_proc(initproc);
                }
            }
        }
    }
    local_intr_restore(intr_flag);
    schedule();
    panic("do_exit will not return!! %d.\n", current->pid);
}

/* load_icode - 将二进制程序（ELF 格式）的内容加载为当前进程的新镜像
 * @binary:  二进制程序内容在内存中的地址
 * @size:    二进制程序内容的大小
 */
static int
load_icode(unsigned char *binary, size_t size)
{
    // 当前进程current->mm 必须为空，因为do_execve在调用load_icode之前已经释放了原来的mm
    if (current->mm != NULL)
    {
        panic("load_icode: current->mm must be empty.\n");
    }

    int ret = -E_NO_MEM;
    struct mm_struct *mm;
    //(1) 为当前进程创建新的 mm
    if ((mm = mm_create()) == NULL)
    {
        goto bad_mm;
    }
    //(2) 创建页目录（PDT，即最高级页表），并将 mm->pgdir 设为该页目录的内核虚拟地址
    if (setup_pgdir(mm) != 0)
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
    if (elf->e_magic != ELF_MAGIC)
    {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }

    uint32_t vm_flags, perm;
    struct proghdr *ph_end = ph + elf->e_phnum;
    for (; ph < ph_end; ph++)
    {
        //(3.4) 遍历每个程序段（program header）的可加载段（ELF_PT_LOAD）
        if (ph->p_type != ELF_PT_LOAD)
        {
            continue;
        }
        if (ph->p_filesz > ph->p_memsz)
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
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0)
        {
            goto bad_cleanup_mmap;
        }
        unsigned char *from = binary + ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);

        ret = -E_NO_MEM;

        //(3.6) 为每个程序段分配内存，并将段内容复制到进程地址空间 (la, la+end)
        // 也就是将文件中的内容复制到内存中
        end = ph->p_va + ph->p_filesz;
        //(3.6.1) 复制二进制程序的 TEXT/DATA 段
        while (start < end)
        {
            // 调用 pgdir_alloc_page 为 la 分配物理页
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

        //(3.6.2) 构建二进制程序的 BSS 段(文件中没有数据，内存中需要为其划零)
        end = ph->p_va + ph->p_memsz;
        if (start < la)
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
        while (start < end)
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
    // 先在 VMA 中注册栈区（mm_map()），再分配几页页面写入页表，使用户栈能够使用。
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0)
    {
        goto bad_cleanup_mmap;
    }
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 2 * PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 3 * PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 4 * PGSIZE, PTE_USER) != NULL);

    //(5) 设置当前进程的 mm、pgdir，并将 satp 寄存器切换到该页表
    mm_count_inc(mm);
    current->mm = mm;
    current->pgdir = PADDR(mm->pgdir);
    lsatp(PADDR(mm->pgdir));

    //(6) 为用户态环境设置 trapframe，因为是从内核态返回，所以我们通过设置 trapframe 来设置返回到用户态时的寄存器值
    struct trapframe *tf = current->tf;
    // 保留 sstatus 的值
    uintptr_t sstatus = tf->status;
    memset(tf, 0, sizeof(struct trapframe));
    /* LAB5: 实验5 练习1 - 2314035
     * 应设置 tf->gpr.sp, tf->epc, tf->status
     * 注意: 如果正确设置 trapframe，则用户态进程可以从内核返回到用户态。因此：
     *          tf->gpr.sp 应为用户栈顶（sp 的值）
     *          tf->epc 应为用户程序的入口点（sepc 的值）
     *          tf->status 应为用户程序合适的 sstatus 值
     *          提示: 查看 riscv.h 中 SSTATUS_SPP、SSTATUS_SPIE 的含义
     */
    
    // 设置用户栈指针：指向用户栈顶
    tf->gpr.sp = USTACKTOP;
    
    // 设置程序入口地址：ELF文件头中的 e_entry 字段
    tf->epc = elf->e_entry;
    
    // 设置 sstatus 寄存器：
    // - SSTATUS_SPIE = 1: sret 返回后开启中断
    // - SSTATUS_SPP = 0: sret 返回到用户态 (U mode)
    // 由于前面 memset 已经清零，SPP 已经是 0，只需设置 SPIE为1，这样sret之后就会开启中断
    tf->status = (sstatus & ~SSTATUS_SPP) | SSTATUS_SPIE;

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
    // 把name拷贝到内核栈上的local_name中，因为替换current->mm之后，用户空间的name可能不可访问
    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));
    memcpy(local_name, name, len);
    // 进程有用户地址空间，进行释放
    if (mm != NULL)
    {
        cputs("mm != NULL");
        lsatp(boot_pgdir_pa); // 用 kernel 的页目录，使内核可安全访问页表与内核空间（详见下）
        if (mm_count_dec(mm) == 0) // 没有人再使用
        {
            exit_mmap(mm); // 逐个VMA释放内存映射
            put_pgdir(mm); // 释放页目录
            mm_destroy(mm); // 销毁内存管理结构
        }
        current->mm = NULL;
    }
    int ret;
    // 使用 load_icode 装载新程序
    if ((ret = load_icode(binary, size)) != 0)
    {
        goto execve_exit; // 失败则调用 do_exit 退出进程
    }
    set_proc_name(current, local_name);// 设置进程名
    return 0;

execve_exit:
    do_exit(ret);
    panic("already exit: %e.\n", ret);
}

// do_yield - 请求调度器重新调度
int do_yield(void)
{
    current->need_resched = 1;
    return 0;
}

// do_wait - 等待一个或任意处于 PROC_ZOMBIE 状态的子进程，并释放该子进程的内核栈和 proc_struct
// 注: 只有在 do_wait 函数返回后，子进程的所有资源才会被释放。
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
    haskid = 0; // 标记是否找到子进程
    if (pid != 0)
    {
        proc = find_proc(pid);// pid 非 0，查找指定 pid 的子进程
        if (proc != NULL && proc->parent == current)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found;
            }
        }
    }
    else
    {
        proc = current->cptr;
        // pid 为 0，遍历所有子进程
        for (; proc != NULL; proc = proc->optr)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found;
            }
        }
    }
    // 找到子进程但是没有处于 ZOMBIE 状态，进入睡眠等待子进程退出
    if (haskid)
    {
        current->state = PROC_SLEEPING;
        current->wait_state = WT_CHILD; // 设置等待状态为等待子进程
        schedule();// 切换去运行其他进程
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

// do_kill - 通过设置进程的 flags 为 PF_EXITING 来终止指定 pid 的进程
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

// kernel_execve - 由 user_main 内核线程调用，用于通过 SYS_exec 系统调用执行用户程序
static int
kernel_execve(const char *name, unsigned char *binary, size_t size)
{
    int64_t ret = 0, len = strlen(name);
    //   ret = do_execve(name, len, binary, size); // 注释掉的原始调用：执行 do_execve
    // 不直接调用do_execve，而是通过设置寄存器并发出ebreak来触发异常处理路径（因此执行会经过 exception_handler）
    /*
    原因是：
    do_execve / load_icode 的核心工作是“构造或修改当前进程的 trapframe（保存的寄存器、sepc、sstatus 等）”，并加载用户程序的页表、用户栈、入口点等
    但“从内核态跳转到用户态执行”需要 CPU 使用 sret 指令才能由 CPU 的控制逻辑改变特权级（S-mode → U-mode）并把寄存器从 trapframe 恢复到实际的寄存器。
    */
    asm volatile(
        "li a0, %1\n" // 系统调用号
        "lw a1, %2\n"
        "lw a2, %3\n"
        "lw a3, %4\n"
        "lw a4, %5\n" // 几个syscall参数
        "li a7, 10\n" // 把a7设为10作为哨兵，以便在 exception handler 中区分：如果 BREAK（ebreak）触发且 a7==10，则这是从 kernel_execve 发出的特殊 ebreak。
        "ebreak\n" // 触发断点异常，CPU进入异常处理路径
        "sw a0, %0\n"
        : "=m"(ret)
        // 注意这里"i"(SYS_exec)就是sys_exec的系统调用号
        : "i"(SYS_exec), "m"(name), "m"(len), "m"(binary), "m"(size)
        : "memory");
    cprintf("ret = %d\n", ret);
    return ret;
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
    KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);
#else
    KERNEL_EXECVE(exit); // 执行用户程序 "exit"
#endif
    panic("user_main execve failed.\n");
}

// init_main - 第二个内核线程，用来创建 user_main 内核线程
static int
init_main(void *arg)
{
    // 这两个用于检测内存泄漏，不必深究
    size_t nr_free_pages_store = nr_free_pages();
    size_t kernel_allocated_store = kallocated();
    // 通过 kernel_thread 创建 user_main 内核线程
    int pid = kernel_thread(user_main, NULL, 0);
    // user_main在kernel_thread 的回调中被执行：user_main -> kernel_execve -> do_execve -> load_icode
    if (pid <= 0)
    {
        panic("create user_main failed.\n");
    }
    // 等待任意子进程退出（pid == 0 表示 any child）
    // 直到 do_wait 返回非 0（即没有子进程）为止
    while (do_wait(0, NULL) == 0)
    {
        schedule();// 使 init_main 放弃 CPU
    }

    cprintf("all user-mode processes have quit.\n");
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
    set_proc_name(idleproc, "idle");
    nr_process++;

    current = idleproc;

    int pid = kernel_thread(init_main, NULL, 0);
    if (pid <= 0)
    {
        panic("create init_main failed.\n");
    }

    initproc = find_proc(pid);
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
    assert(initproc != NULL && initproc->pid == 1);
}

// cpu_idle - 在 kern_init 结束时，第一个内核线程 idleproc 会执行以下工作
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
