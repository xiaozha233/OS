# Lab5 实验报告：用户进程管理

## 小组信息

**小组成员及分工：**

| 学号 | 姓名 | 分工 |
|------|------|------|
| 2314076 | 查许琴 | 练习2（copy_range函数实现）、整体测试与调试 |
| 2314035 | 陈翔 | 练习1（load_icode函数实现）、分支任务（gdb调试） |
| 2313255 | 刘璇 | 练习3（源代码分析）、实验报告整理 |

---

## 实验目的

- 了解第一个用户进程创建过程
- 了解系统调用框架的实现机制
- 了解ucore如何实现系统调用sys_fork/sys_exec/sys_exit/sys_wait来进行进程管理

---

## 练习1：加载应用程序并执行

### 设计实现过程

在本练习中，我们需要完成`load_icode`函数的第6步，设置用户进程的trapframe，使得进程能够正确地从内核态返回到用户态开始执行。

#### 实现代码

```c
//(6) 为用户态环境设置 trapframe
struct trapframe *tf = current->tf;
// 保留 sstatus 的值
uintptr_t sstatus = tf->status;
memset(tf, 0, sizeof(struct trapframe));

// 设置用户栈指针：指向用户栈顶
tf->gpr.sp = USTACKTOP;

// 设置程序入口地址：ELF文件头中的 e_entry 字段
tf->epc = elf->e_entry;

// 设置 sstatus 寄存器：
// - SSTATUS_SPIE = 1: sret 返回后开启中断
// - SSTATUS_SPP = 0: sret 返回到用户态 (U mode)
// 由于前面 memset 已经清零，SPP 已经是 0，只需设置 SPIE
tf->status = (sstatus & ~SSTATUS_SPP) | SSTATUS_SPIE;
```

#### 关键点说明

1. **用户栈指针设置**：`tf->gpr.sp = USTACKTOP`
   - 将栈指针设置为用户栈的顶部地址
   - USTACKTOP在memlayout.h中定义，表示用户栈的最高地址

2. **程序入口地址设置**：`tf->epc = elf->e_entry`
   - epc（Exception Program Counter）寄存器保存了异常返回后要执行的指令地址
   - 从ELF文件头中读取程序入口地址e_entry
   - sret指令执行时会跳转到epc指向的地址

3. **状态寄存器设置**：`tf->status = (sstatus & ~SSTATUS_SPP) | SSTATUS_SPIE`
   - **SSTATUS_SPP = 0**：设置返回到用户态(U mode)
     - SPP位表示"Previous Privilege"，即进入trap前的特权级
     - SPP=0表示返回U mode，SPP=1表示返回S mode
   - **SSTATUS_SPIE = 1**：使能中断
     - SPIE位表示"Previous Interrupt Enable"
     - 设置为1使得返回用户态后能够响应中断

### 用户态进程执行的完整过程

从用户态进程被选择占用CPU到执行第一条指令的完整流程如下：

#### 1. 进程创建与初始化阶段

```
proc_init() 
  └─> kernel_thread(user_main, NULL, 0)  // 创建user_main内核线程
       └─> do_fork()
            ├─> alloc_proc()              // 分配进程控制块
            ├─> setup_kstack()            // 分配内核栈
            ├─> copy_mm()                 // 此时为内核线程，mm=NULL
            └─> wakeup_proc()             // 设置为RUNNABLE状态
```

#### 2. 调度器选择进程

```
schedule()
  └─> proc_run(next)
       ├─> lsatp(next->pgdir)            // 切换页表
       └─> switch_to(&prev->context, &next->context)  // 上下文切换
```

#### 3. user_main执行kernel_execve

```
user_main()
  └─> KERNEL_EXECVE(exit)
       └─> kernel_execve("exit", binary, size)
            ├─> 内联汇编设置寄存器
            │    ├─> a0 = SYS_exec
            │    ├─> a1-a4 = 参数
            │    └─> a7 = 10 (标识码)
            └─> ebreak                    // 触发断点异常  // 断点：Breakpoint 2 (__alltraps)；Breakpoint 1 (syscall)
```

#### 4. 异常处理与系统调用

```
__alltraps (trapentry.S)  // 断点：Breakpoint 2
  └─> SAVE_ALL                           // 保存trapframe
       └─> trap(tf)
            └─> trap_dispatch()
                 └─> exception_handler()
              ├─> (tf->cause == CAUSE_BREAKPOINT && tf->gpr.a7 == 10)
              │    ├─> tf->epc += 4
              │    ├─> syscall()  // 转发到 SYS_exec  // 断点：Breakpoint 1 (syscall)；对于具体系统调用实现可设置 Breakpoint 6 (do_fork) / Breakpoint 7 (do_exit)
              │    │    └─> sys_exec()
              │    │         └─> do_execve()
              │    │              ├─> 释放旧的mm
              │    │              └─> load_icode()  // 关键函数  // 断点：Breakpoint 8 (load_icode)
              │    │                   ├─> mm_create()
              │    │                   ├─> setup_pgdir()
              │    │                   ├─> 解析ELF文件
              │    │                   ├─> 加载代码段/数据段
              │    │                   ├─> 分配用户栈
              │    │                   └─> 设置trapframe ★★★
              │    │                        ├─> tf->gpr.sp = USTACKTOP
              │    │                        ├─> tf->epc = elf->e_entry
              │    │                        └─> tf->status = SSTATUS_SPIE
              │    └─> kernel_execve_ret(tf, current->kstack + KSTACKSIZE)
              │         // 位于 trapentry.S：把 trapframe 复制到“当前进程新的内核栈顶”，然后跳到 __trapret  // 断点：Breakpoint 3 (__trapret) 与 Breakpoint 4 (sret)
              └─> (其它异常/中断路径略)
```

#### 5. 返回用户态

```
__trapret (trapentry.S)  // 断点：Breakpoint 3
  └─> RESTORE_ALL                        // 恢复trapframe
       ├─> 恢复用户态寄存器
       ├─> csrw sstatus, s1              // 恢复status(SPP=0)
       ├─> csrw sepc, s2                 // 恢复epc=elf->e_entry
       └─> sret                          // 返回用户态 ★★★  // 断点：Breakpoint 4 (sret)
            ├─> PC ← sepc                // 跳转到用户程序入口
            ├─> 特权级 ← SPP(0)          // 切换到U mode
            └─> 中断使能 ← SPIE(1)       // 开启中断

注：对 `kernel_execve` 这条“首次进入用户态”的特殊路径，`trap.c` 不会直接 `return` 回到 `__trapret`，而是通过 `kernel_execve_ret` 把新 trapframe 放到正确的内核栈位置后，直接 `j __trapret`，确保 `RESTORE_ALL` 恢复的是新用户程序的上下文。
```

#### 6. 执行用户程序第一条指令

```
用户程序入口 (elf->e_entry)
  └─> _start (initcode.S)
       └─> umain()
            └─> main()                   // 用户程序的main函数  // 用户态断点示例：Breakpoint 5 (用户 `ecall` 指令，例如 0x800104)
```

#### 调试断点位置（与流程对应）

下面列出本次调试会话中我们设置的断点，以及它们在上面流程中的对应位置（断点编号与地址取自会话记录）：

- KERNEL_EXECVE / `ebreak` 转发：`syscall`（Breakpoint 1）和 `__alltraps`（Breakpoint 2） — 用于观察 `ebreak`（`a7=10`）的陷入与内核转发。
- Trap 入口与 `sret` 返回：`__trapret`（Breakpoint 3）及 sret 指令地址 `*0xffffffffc0200f7a`（Breakpoint 4） — 用于观察内核如何恢复寄存器并执行 `sret` 返回用户态。
- 用户态 `ecall` 指令：用户地址 `0x800104`（Breakpoint 5） — 在用户态单步触发 `ecall`，观察陷入前后的寄存器与控制流变化。
- 进程创建：`do_fork`（Breakpoint 6） — 用于检查子进程的 PCB 分配与内存复制过程。
- 进程退出：`do_exit`（Breakpoint 7） — 用于观察退出流程及父子关系处理。
- exec 路径加载：`load_icode`（Breakpoint 8） — 用于观察 ELF 解析/加载和 `trapframe` 的最终设置。

注：断点编号与地址依调试会话的具体运行而定，上述为本次会话中的实际观察值；读者在自己的调试中可按对应位置设置断点（`break <symbol>` 或 `break *<address>`）。

### 关键机制说明

1. **第一次进入用户态的特殊性**
   - 不能直接调用do_execve，因为无法完成上下文切换
   - 通过ebreak触发异常，利用异常返回机制(sret)完成特权级切换
   - a7=10作为特殊标识，区分普通断点和系统调用转发
   - `kernel_execve_ret`（trapentry.S）用于把 trapframe 迁移到当前进程的内核栈顶并跳转到 `__trapret`

2. **trapframe的作用**
   - 保存了进程的完整执行上下文
   - sret指令从trapframe恢复寄存器状态
   - 是内核态和用户态切换的关键数据结构

3. **特权级切换的时机**
   - 进入内核：用户态执行ecall/发生异常/中断 → S mode
   - 返回用户：内核态执行sret → U mode（前提：SPP=0）

---

## 练习2：父进程复制自己的内存空间给子进程

### 设计实现过程

在本练习中，我们需要实现`copy_range`函数，该函数负责在`do_fork`过程中将父进程的内存内容复制到子进程中。这是实现进程创建的关键步骤。

#### 实现代码

```c
/* LAB5:EXERCISE2 2314076
 * 将 page 的内容复制到 npage，并在线性地址 start 处为 npage 建立物理地址映射
 */

// (1) 获取源页面（父进程）的内核虚拟地址
void *src_kvaddr = page2kva(page);

// (2) 获取目标页面（子进程）的内核虚拟地址
void *dst_kvaddr = page2kva(npage);

// (3) 将源页面的内容复制到目标页面，大小为一页 (PGSIZE = 4KB)
memcpy(dst_kvaddr, src_kvaddr, PGSIZE);

// (4) 在子进程的页表中建立虚拟地址 start 到物理页 npage 的映射
// perm 是从父进程页表项中提取的权限位
ret = page_insert(to, npage, start, perm);
```

#### 实现步骤详解

1. **获取源页面的内核虚拟地址**
   - `page2kva(page)`：将父进程的物理页面转换为内核可访问的虚拟地址
   - 这是因为内核需要通过虚拟地址来访问物理内存

2. **获取目标页面的内核虚拟地址**
   - `page2kva(npage)`：将新分配的物理页面转换为内核虚拟地址
   - 为子进程准备接收数据的目标地址

3. **内存复制**
   - `memcpy(dst_kvaddr, src_kvaddr, PGSIZE)`
   - 将一整页（4KB）的内容从父进程复制到子进程
   - 这包括了代码、数据、堆栈等所有内容

4. **建立页表映射**
   - `page_insert(to, npage, start, perm)`
   - 在子进程的页表中建立虚拟地址到物理页的映射
   - 确保子进程能够通过相同的虚拟地址访问到自己的数据副本

#### 函数调用关系

```
do_fork()
  └─> copy_mm(clone_flags, proc)
       └─> dup_mmap(mm, oldmm)
            └─> copy_range(to->pgdir, from->pgdir, vma->vm_start, vma->vm_end)
                 ├─> 遍历地址范围内的每一页
                 ├─> 为每一页分配新的物理页面
                 └─> 调用我们实现的代码进行复制和映射
```

### Copy on Write (COW) 机制设计

#### 基本概念

Copy-on-Write是一种延迟复制的优化策略：
- fork时不立即复制父进程的内存内容
- 父子进程共享相同的物理页面，但都标记为只读
- 任何一方尝试写入时触发page fault，此时才进行真正的复制

#### COW机制概要设计

##### 1. 数据结构修改

```c
// 在 kern/mm/mmu.h 中添加新的页表项标志
// 说明：本项目里 PTE_SOFT = 0x300（软件保留位），可选其中一位作为 COW 标记
// 例如使用 bit8（0x100）作为 PTE_COW
#define PTE_COW     0x100    // 标记页面为COW状态（属于 PTE_SOFT 的一部分）
```

##### 2. fork时的处理（修改copy_range函数）

```c
// 现有接口为 copy_range(to, from, start, end, share)
// 一个直接的做法：当 share==1 时走 COW 共享逻辑；当 share==0 时走“逐页复制”逻辑
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share) {
    uintptr_t addr = start;
    while (addr < end) {
        pte_t *ptep = get_pte(from, addr, 0);
        if (ptep == NULL || !(*ptep & PTE_V)) {
            addr += PGSIZE;
            continue;
        }
        
        // 获取父进程的物理页面
        struct Page *page = pte2page(*ptep);
        uint32_t perm = (*ptep & PTE_USER);
        
         if (share) {
            // COW：父子共享物理页，但都清除写权限并打上 COW 标记
            if (perm & PTE_W) {
               perm = (perm & ~PTE_W) | PTE_COW;
               // 父进程页表项改为只读 + COW（注意需要在父页表里更新）
               page_insert(from, page, addr, perm);
            }
            page_ref_inc(page);
            page_insert(to, page, addr, perm);
         } else {
            // 非 COW：维持本实验的“逐页复制”实现（略）
            // alloc_page + memcpy + page_insert(to, npage, addr, perm)
         }
        
        addr += PGSIZE;
    }
    return 0;
}
```

##### 3. Page Fault处理（修改缺页异常处理路径）

```c
// RISC-V 下通常通过 scause 区分是 load/store/instruction page fault
// 因此更稳妥的判定方式是：仅在“写导致的缺页”（store page fault）里处理 COW
int do_pgfault_cow(struct mm_struct *mm, uintptr_t addr, int scause) {
   pte_t *ptep = get_pte(mm->pgdir, ROUNDDOWN(addr, PGSIZE), 0);
    
      // 检查是否是 COW 页面且是写触发（store page fault）
      if (scause == CAUSE_STORE_PAGE_FAULT && ptep && (*ptep & PTE_V) && (*ptep & PTE_COW)) {
        struct Page *page = pte2page(*ptep);
        
        // 情况1：只有一个进程引用，直接恢复写权限
        if (page_ref(page) == 1) {
            uint32_t perm = (*ptep & PTE_USER) | PTE_W;
            perm &= ~PTE_COW;  // 清除COW标志
            page_insert(mm->pgdir, page, ROUNDDOWN(addr, PGSIZE), perm);
            tlb_invalidate(mm->pgdir, addr);
            return 0;
        }
        // 情况2：多个进程引用，需要复制页面
        else {
            struct Page *npage = alloc_page();
            if (npage == NULL) {
               return -E_NO_MEM;
            }
            
            // 复制页面内容
            void *src = page2kva(page);
            void *dst = page2kva(npage);
            memcpy(dst, src, PGSIZE);
            
            // 建立新映射，恢复写权限
            uint32_t perm = (*ptep & PTE_USER) | PTE_W;
            perm &= ~PTE_COW;
            page_insert(mm->pgdir, npage, ROUNDDOWN(addr, PGSIZE), perm);
            tlb_invalidate(mm->pgdir, addr);
            return 0;
        }
    }

      return -E_INVAL;
}
```

##### 4. 状态转换图

```
正常页面状态：
    RW (可读写)
       │
       │ fork()
       ↓
    R + COW (只读+COW标志)  [父子共享]
       │
       │ 写操作触发Page Fault
       ↓
    ┌──────────────────┐
    │ 检查引用计数      │
    └──────────────────┘
       │          │
   ref=1│          │ref>1
       ↓          ↓
    直接恢复    复制页面
    RW权限      分配新页
       │          │
       └────┬─────┘
            ↓
         RW (可读写) [各自独立]
```

#### COW的优势

1. **内存节省**：避免fork时的大量内存复制
2. **性能提升**：减少fork的时间开销
3. **实用性强**：很多情况下fork后立即exec，不需要复制

#### 潜在问题与解决

**Dirty COW 竞态风险（高层讨论）**

参考 https://dirtycow.ninja/ 的高层描述：

- **风险点**：如果 COW fault handler 在“检查引用计数/页表项状态”和“完成复制与更新映射”之间存在竞争窗口，就可能出现状态不一致，从而破坏“只读共享”的语义

- **解决方案**：
  1. 在整个COW处理过程中持有页表锁
  2. 使用原子操作检查和修改引用计数
  3. 在复制前再次验证页面状态

```c
// 更稳妥的 COW 处理思路（示意）：关键是原子性与一致性，而不是某一行 API
void do_pgfault_cow_safe(struct mm_struct *mm, uintptr_t addr) {
    lock_mm(mm);  // 获取mm锁
    
    pte_t *ptep = get_pte(mm->pgdir, addr, 0);
    if (!ptep || !(*ptep & PTE_COW)) {
        unlock_mm(mm);
        return;
    }
    
    struct Page *page = pte2page(*ptep);
    
   // 需要保证：页表项检查 + 引用计数判断 + 页表项更新是一个一致的临界区
   if (page_ref(page) == 1) {
        // 唯一引用，恢复写权限
        *ptep |= PTE_W;
        *ptep &= ~PTE_COW;
    } else {
        // 多个引用，需要复制
        struct Page *npage = alloc_page();
        memcpy(page2kva(npage), page2kva(page), PGSIZE);
        
        // 再次检查页表项是否被修改
        if (*ptep == pte_create(page2ppn(page), (*ptep & 0xFFF))) {
            page_insert(mm->pgdir, npage, addr, PTE_W | PTE_R | PTE_U);
        } else {
            // 页表项已被修改，需要重试
            free_page(npage);
        }
    }
    
    unlock_mm(mm);
}
```

---

## 练习3：阅读分析源代码

### fork/exec/wait/exit执行流程分析

#### 1. fork执行流程

```
用户态：
  fork() (user/libs/ulib.c)
    └─> sys_fork() (user/libs/syscall.c)
         └─> syscall(SYS_fork)
              └─> ecall指令 ──────┐
                                 │ 进入内核态
内核态：                          │
  ┌─────────────────────────────┘
  ↓
  trap处理 (kern/trap/trapentry.S & trap.c)
    └─> syscall() (kern/syscall/syscall.c)
         └─> sys_fork()
              └─> do_fork() (kern/process/proc.c)
                   ├─> alloc_proc()          // 分配PCB
                   ├─> setup_kstack()        // 分配内核栈
                   ├─> copy_mm()             // 复制内存空间
                   │    └─> dup_mmap()
                   │         └─> copy_range() // [练习2实现]
                   ├─> copy_thread()         // 设置trapframe和context
                   ├─> get_pid()             // 分配PID
                   ├─> hash_proc()           // 加入hash表
                   ├─> set_links()           // 设置进程链接关系
                   └─> wakeup_proc()         // 唤醒子进程
  ┌─────────────────────────────┐
  │ sret返回用户态              │
  └─────────────────────────────┘
         │
用户态： ↓
  fork()返回
    ├─> 父进程：返回子进程PID
    └─> 子进程：返回0
```

**用户态/内核态划分**：
- 用户态：fork()库函数封装，系统调用接口
- 内核态：进程创建的实际工作（内存复制、PCB初始化等）
- 交错执行：通过ecall进入内核，通过sret返回用户态

#### 2. exec执行流程

```
说明：本仓库的用户库 `user/libs` 未提供 `exec()`/`sys_exec()` 的用户态封装接口，但内核会通过 `kernel_execve()` 发起 `SYS_exec`，其后半段（`sys_exec -> do_execve -> load_icode`）与用户态 ecall 进入内核的路径一致。

内核态（通过 user_main -> kernel_execve 发起 SYS_exec）：
  kernel_execve() (kern/process/proc.c)
    └─> ebreak (a7=10)
         └─> trap/exception_handler() (kern/trap/trap.c)
              └─> syscall() (kern/syscall/syscall.c)
                   └─> sys_exec()
                        └─> do_execve() (kern/process/proc.c)
                   ├─> 检查参数合法性
                   ├─> user_mem_check()    // 检查内存访问
                   ├─> 释放旧的mm
                   │    ├─> lsatp(boot_pgdir_pa) // 切换到内核页表（RISC-V）
                   │    ├─> exit_mmap()    // 释放用户虚拟内存
                   │    ├─> put_pgdir()    // 释放页目录
                   │    └─> mm_destroy()   // 销毁mm_struct
                   └─> load_icode()        // 加载新程序[练习1]
                        ├─> mm_create()    // 创建新的mm
                        ├─> setup_pgdir()  // 创建页目录
                        ├─> 解析ELF文件
                        ├─> mm_map()       // 建立VMA
                        ├─> 加载代码段、数据段
                        ├─> 分配BSS段
                        ├─> 分配用户栈
                        └─> 设置trapframe  // [练习1实现]
  ┌─────────────────────────────┐
  │ sret返回用户态              │
  └─────────────────────────────┘
         │
用户态： ↓
  新程序的main()开始执行
```

**关键点**：
- exec不创建新进程，而是替换当前进程的内存空间
- 进程PID保持不变，但程序代码完全改变
- 成功执行后不返回到原程序，而是开始执行新程序

#### 3. wait执行流程

```
用户态：
  wait() / waitpid(pid, status) (user/libs/ulib.c)
    └─> sys_wait(pid, status)
         └─> syscall(SYS_wait, ...)
              └─> ecall指令 ──────┐
                                 │
内核态：                          │
  ┌─────────────────────────────┘
  ↓
  trap处理
    └─> syscall()
         └─> sys_wait()
              └─> do_wait() (kern/process/proc.c)
                   ├─> 查找符合条件的子进程
                   │    └─> 遍历子进程链表(cptr)
                   │
                   ├─> 情况1: 找到ZOMBIE状态的子进程
                   │    ├─> 回收子进程资源
                   │    │    ├─> 从进程链表移除
                   │    │    ├─> put_kstack()      // 释放内核栈
                   │    │    └─> kfree(proc)       // 释放PCB
                   │    └─> 返回子进程PID和退出码
                   │
                   ├─> 情况2: 有子进程但都在运行
                   │    ├─> current->state = SLEEPING
                   │    ├─> current->wait_state = WT_CHILD
                   │    ├─> schedule()             // 让出CPU
                   │    └─> 被唤醒后重新查找
                   │
                   └─> 情况3: 没有子进程
                        └─> 返回错误码
  ┌─────────────────────────────┐
  │ sret返回用户态              │
  └─────────────────────────────┘
         │
用户态： ↓
  wait()返回
    ├─> 成功：返回子进程PID，status保存退出码
    └─> 失败：返回-1
```

**关键点**：
- 父进程负责回收子进程的最后资源（内核栈、PCB）
- 如果没有ZOMBIE子进程，父进程会睡眠等待
- 子进程退出时会唤醒等待的父进程

#### 4. exit执行流程

```
用户态：
  exit(error_code) (user/libs/ulib.c)
    └─> sys_exit(error_code)
         └─> syscall(SYS_exit, error_code)
              └─> ecall指令 ──────┐
                                 │
内核态：                          │
  ┌─────────────────────────────┘
  ↓
  trap处理
    └─> syscall()
         └─> sys_exit()
              └─> do_exit() (kern/process/proc.c)
                   ├─> 检查不能是idle/init进程
                   │
                   ├─> 释放用户内存空间
                   │    ├─> lsatp(boot_pgdir_pa)   // 切换到内核页表（RISC-V）
                   │    ├─> exit_mmap(mm)          // 释放所有VMA
                   │    ├─> put_pgdir(mm)          // 释放页目录
                   │    └─> mm_destroy(mm)         // 销毁mm_struct
                   │
                   ├─> 设置进程状态
                   │    ├─> state = PROC_ZOMBIE    // 变为僵尸状态
                   │    └─> exit_code = error_code
                   │
                   ├─> 处理父子进程关系
                   │    ├─> 唤醒父进程(如果在等待)
                   │    │    └─> if (parent->wait_state == WT_CHILD)
                   │    │         wakeup_proc(parent)
                   │    │
                   │    └─> 将所有子进程过继给initproc
                   │         └─> 遍历子进程链表
                   │              ├─> proc->parent = initproc
                   │              └─> 如果子进程是ZOMBIE，唤醒initproc
                   │
                   └─> schedule()                  // 切换到其他进程
                        └─> 永不返回！
```

**关键点**：
- 进程自己只能释放用户内存空间，不能释放内核栈和PCB
- 进程进入ZOMBIE状态，等待父进程回收
- 如果父进程已退出，由initproc接管并回收
- 调用schedule()后永不返回，因为该进程已经结束

### 用户态与内核态交错执行机制

#### 1. 用户态 → 内核态

**触发方式**：
- **系统调用**：用户程序执行ecall指令（主动）
- **异常**：非法指令、缺页异常等（被动）
- **中断**：时钟中断、外设中断等（被动）

**执行过程**：
```
1. 硬件自动操作：
   - sepc ← 触发 trap 的指令地址（PC）
   - scause ← trap 原因；stval ← 辅助信息（如 fault 地址）
   - sstatus.SPP ← trap 前特权级（来自用户态则为 0）
   - sstatus.SPIE ← sstatus.SIE；sstatus.SIE ← 0（关中断）
   - CPU 切换到 S-mode，并跳转到 stvec（异常入口）

2. 软件保存现场（trapentry.S）：
   - 交换sp和sscratch（如果来自用户态）
   - 保存所有寄存器到trapframe
   - 调用trap()进行中断/异常/系统调用处理

3. 内核处理：
   - 根据scause判断原因
   - 执行相应的处理函数
   - 修改trapframe（如设置返回值）
```

#### 2. 内核态 → 用户态

**执行过程**：
```
1. 软件恢复现场（trapentry.S）：
   - 从trapframe恢复所有寄存器
   - 恢复sstatus和sepc
   - 如果返回用户态，将内核栈指针保存到sscratch

2. 硬件自动操作（sret指令）：
   - PC ← sepc（跳转到用户程序）
   - 特权级 ← sstatus.SPP（切换到U mode）
   - 中断使能 ← sstatus.SPIE
   - sstatus.SPP = 0（为下次准备）
```

#### 3. 返回值传递机制

内核态执行结果通过trapframe传递给用户程序：

```c
// 内核态：syscall()函数
void syscall(void) {
    struct trapframe *tf = current->tf;
    uint64_t arg[5];
    int num = tf->gpr.a0;  // 获取系统调用号
    
    // 获取参数
    arg[0] = tf->gpr.a1;
    arg[1] = tf->gpr.a2;
    // ...
    
    // 调用对应的系统调用处理函数，并将返回值写入a0
    tf->gpr.a0 = syscalls[num](arg);  // ← 返回值写入trapframe
}

// 用户态：syscall()函数 (user/libs/syscall.c)
static inline int syscall(int num, ...) {
    // ... 设置参数到寄存器 ...
    asm volatile (
        "ecall\n"              // 触发系统调用
        "sd a0, %0"            // ecall返回后，a0已被内核修改
        : "=m" (ret)           // ← 返回值从a0读取
        : "m"(num), ...
    );
    return ret;
}
```

**关键点**：
- 参数传递：通过a0-a5寄存器
- 返回值传递：通过a0寄存器
- trapframe是连接用户态和内核态的桥梁

### 用户态进程执行状态生命周期图

```
                    alloc_proc
                        │
                        ↓
                  ┌──────────┐
                  │ UNINIT   │ (新建但未初始化)
                  └──────────┘
                        │
                        │ proc_init / wakeup_proc
                        ↓
                  ┌──────────┐
          ┌──────→│ RUNNABLE │←──────┐
          │       └──────────┘       │
          │             │             │
          │ wakeup_proc │             │ 时间片用完
          │             │ schedule()  │ / yield()
          │             ↓             │
          │       ┌──────────┐       │
          │       │ RUNNING  │───────┘
          │       └──────────┘
          │             │
          │             │ do_wait / do_sleep
          │             │ / try_free_pages
          │             ↓
          │       ┌──────────┐
          └───────│ SLEEPING │ (等待资源/事件)
                  └──────────┘
                        │
                        │ do_exit
                        ↓
                  ┌──────────┐
                  │  ZOMBIE  │ (等待父进程回收)
                  └──────────┘
                        │
                        │ do_wait (父进程回收)
                        ↓
                   [进程终止]
```

#### 状态转换说明

| 转换 | 触发条件/函数 | 说明 |
|------|--------------|------|
| UNINIT → RUNNABLE | proc_init / wakeup_proc | 进程初始化完成 |
| RUNNABLE → RUNNING | schedule() / proc_run() | 调度器选中该进程 |
| RUNNING → RUNNABLE | 时间片到期 / sys_yield | 进程主动让出CPU或被抢占 |
| RUNNING → SLEEPING | do_wait / do_sleep | 等待子进程或主动睡眠 |
| SLEEPING → RUNNABLE | wakeup_proc | 等待的事件发生 |
| RUNNING → ZOMBIE | do_exit | 进程退出 |
| ZOMBIE → 终止 | do_wait(父进程) | 父进程回收资源 |

#### 关键系统调用对状态的影响

```
fork:   父进程 RUNNING → RUNNING
        子进程 UNINIT → RUNNABLE

exec:   当前进程 RUNNING → RUNNING (更换执行内容)

wait:   父进程 RUNNING → SLEEPING (如果没有ZOMBIE子进程)
        父进程 RUNNING → RUNNING (如果找到ZOMBIE子进程并回收)

exit:   当前进程 RUNNING → ZOMBIE
        父进程 SLEEPING → RUNNABLE (如果父进程在等待)

yield:  当前进程 RUNNING → RUNNABLE
```

---

## 分支任务：GDB调试系统调用

本任务采用**双重 GDB（Double GDB）** 方案，通过三个终端协同工作，分别从“内核视角”和“模拟器视角”观察系统调用的完整流程。

### 调试准备：三终端协同

我们需要开启三个终端窗口，分工如下：

*   **Terminal 1 (QEMU 运行)**：负责运行 QEMU 模拟器。
*   **Terminal 2 (Guest GDB - 调试内核)**：连接 QEMU 的 gdbstub (localhost:1234)，用于调试 ucore 内核及用户程序。
*   **Terminal 3 (Host GDB - 调试模拟器)**：Attach 到 QEMU 进程，用于调试 QEMU 自身的指令模拟与 TCG 翻译过程。

#### 启动步骤

1.  **Terminal 1**：使用指定 QEMU 路径启动调试
    ```bash
    make build-exit
    # 使用绝对路径指向带有符号的 QEMU
    make debug QEMU=/root/build/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64
    ```

2.  **Terminal 2**：启动客体 GDB 并连接
    ```bash
    make gdb
    ```
    此终端将作为我们控制 ucore 执行的主控制台。

3.  **Terminal 2**：加载用户程序符号表
    由于用户程序是链接进内核的，需手动加载符号以便调试：
    ```gdb
    (gdb) add-symbol-file obj/__user_exit.out
    ```

### 第一阶段：从内核视角观察（Guest GDB）

本阶段主要在 **Terminal 2** 中操作，观察 ucore 内核如何处理系统调用。

#### 1. 在内核 syscall 处设置断点

在 **Terminal 2** 中执行：
```gdb
(gdb) break syscall
(gdb) continue
```
程序将停在 `kern/syscall/syscall.c` 的 `syscall()` 函数入口。

**观察结果（Terminal 2）**：
```gdb
Breakpoint 1, syscall () at kern/syscall/syscall.c:85
85          struct trapframe *tf = current->tf;
```
**分析**：此时 CPU 已处于内核态（S-mode）。通过 `bt` 命令可以看到调用栈：
```
(gdb) bt
#0  syscall () at kern/syscall/syscall.c:85
#1  0xffffffffc0200e10 in exception_handler (tf=0xffffffffc04b0d80)
    at kern/trap/trap.c:185
#2  0xffffffffc0200e58 in trap_dispatch (tf=<optimized out>)
    at kern/trap/trap.c:253
#3  trap (tf=<optimized out>) at kern/trap/trap.c:277
#4  0xffffffffc0200f24 in __alltraps () at kern/trap/trapentry.S:126
Backtrace stopped: frame did not save the PC
```

`syscall` <- `exception_handler` <- `trap_dispatch` <- `trap` <- `__alltraps`。这验证了系统调用是通过异常机制（Trap）进入内核的。

#### 2. 观察用户态 ecall 陷入内核

为了观察 `ecall` 指令的具体行为，我们需要在 Trap 入口处拦截。

在 **Terminal 2** 中执行：
```gdb
(gdb) delete 1          # 删除之前的断点
(gdb) break __alltraps  # 在 trapentry.S 的入口处下断点
(gdb) continue
```

**观察结果（Terminal 2）**：
```gdb
Breakpoint 2, __alltraps () at kern/trap/trapentry.S:123
123         SAVE_ALL
```
查看关键寄存器：
```gdb
(gdb) info reg scause sepc
scause         0x8      8   (CAUSE_USER_ECALL)
sepc           0x800104     (用户态 ecall 指令地址)
```
**分析**：`scause=8` 表明这是用户态环境调用（User Ecall）。`sepc` 记录了触发异常的用户指令地址。

#### 3. 定位并单步执行用户态 ecall

为了亲眼目睹从 User 到 Kernel 的跳变，我们直接在用户态指令处下断点。

在 **Terminal 2** 中执行：
```gdb
(gdb) break *0x800104   # 在 sepc 指向的地址（ecall）下断点
(gdb) continue
```

**观察结果（Terminal 2）**：
```gdb
(gdb) c
Continuing.

Breakpoint 3, 0x0000000000800104 in syscall (num=num@entry=30)
    at user/libs/syscall.c:23
23          asm volatile (
(gdb) x/i $pc
=> 0x800104 <syscall+44>:       ecall
```
此时 CPU 处于用户态（U-mode）。
接下来，先删除断点2：
```gdb
(gdb) delete 2
```

执行单步指令 `si`：
```gdb
(gdb) si
0xffffffffc0200eb8 in __alltraps () at kern/trap/trapentry.S:123
123         SAVE_ALL
```
**分析**：执行 `ecall` 后，PC 瞬间从 `0x800104`（用户态）跳转到了 `0xffffffffc0200eb8`（内核 Trap 入口）。这证明了 `ecall` 指令触发了硬件的特权级切换和跳转。

#### 4. 观察 sret 返回用户态

系统调用处理完毕后，内核通过 `sret` 指令返回用户态。

在 **Terminal 2** 中执行：
```gdb
(gdb) break kern/trap/trapentry.S:133
(gdb) continue
```

**观察结果（Terminal 2）**：
停在 `sret` 指令处。查看状态：
```gdb
(gdb) info reg sepc sstatus
sepc           0x800108 8388872
sstatus        0x8000000000046020       -9223372036854489056
```
执行 `si`：
```gdb
(gdb) si
(gdb) si
0x0000000000800108 in syscall (num=0, num@entry=30)
    at user/libs/syscall.c:23
23          asm volatile (
```
**分析**：执行 `sret` 后，PC 恢复为 `sepc` 的值（`0x800108`），CPU 切回用户态，继续执行用户程序的下一条指令。

### 第二阶段：从模拟器视角观察（Host GDB）

本阶段引入 **Terminal 3**，观察 QEMU 模拟器是如何“模拟”上述 `ecall` 和 `sret` 指令的。

**注意：不需要重启 Terminal 1 和 Terminal 2。** 我们直接在第一阶段的调试基础上继续进行。此时 Terminal 1 的 QEMU 进程仍在运行（或暂停中），Terminal 2 的 Guest GDB 保持连接状态。

**（如果遇到 QEMU 意外退出或断点无法命中，建议完全重启三个终端的调试会话，并严格按照下述顺序操作）**

#### 1. 连接 QEMU 进程

在 **Terminal 3** 中执行：
```bash
# 查找 qemu-system-riscv64 的 PID
ps aux | grep qemu
# 启动主机 GDB 并 attach
gdb -p <qemu-pid>
```

**注意**：由于我们在 Terminal 1 中使用了带有符号的 QEMU 二进制文件，Host GDB 会自动加载符号表，**不需要**再手动执行 `symbol-file` 命令。

#### 2. 在 QEMU 源码中设置断点

我们需要拦截 QEMU 处理 RISC-V `ecall` 指令的函数。

在 **Terminal 3 (Host GDB)** 中执行：
```gdb
(gdb) break helper_raise_exception
Breakpoint 1 at 0x5e02c068b9d1: file /root/build/qemu-4.1.1/target/riscv/op_helper.c, line 39.
(gdb) break riscv_cpu_do_interrupt
Breakpoint 2 at 0x5e02c068d5e9: file /root/build/qemu-4.1.1/target/riscv/cpu_helper.c, line 507.
(gdb) break riscv_raise_exception
Breakpoint 3 at 0x5e02c068b959: file /root/build/qemu-4.1.1/target/riscv/op_helper.c, line 31.
(gdb) continue
```

#### 3. 触发拦截

首先在终端2中删除断点：

```gdb
delete 3
delete 4
```


回到 **Terminal 2 (Guest GDB)**，让 ucore 继续运行（触发下一次系统调用）：
```gdb
(gdb) continue
```

**如果 Terminal 1 显示 `initproc exit` 且 QEMU 退出：**
这说明程序执行速度过快，在 Host GDB 断点生效前就已经完成了所有系统调用。
**解决方法**：请完全重启实验。
1. 关闭所有终端。
2. **Terminal 1**：重新执行 `make debug QEMU=/root/build/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64`。
3. **Terminal 2**：重新执行 `make gdb`。
4. **Terminal 3**：重新 attach 并设置断点。
5. 最后在 **Terminal 2** 执行 `continue`。

**观察结果（Terminal 3）**：
Host GDB 会命中断点，暂停 QEMU 的执行。
```gdb
(gdb) c
Continuing.
[Switching to Thread 0x7163599fe640 (LWP 230988)]

Thread 3 "qemu-system-ris" hit Breakpoint 1, helper_raise_exception (env=0x5e02c1cc92a0, exception=8) at /root/build/qemu-4.1.1/target/riscv/op_helper.c:39
39          riscv_raise_exception(env, exception, 0);
```
**分析**：
此时 **Terminal 1** (QEMU) 界面冻结，**Terminal 2** (Guest GDB) 也会阻塞等待。
在 **Terminal 3** 中查看调用栈 (`bt`)，可以看到 QEMU 的 TCG 引擎（`tcg_cpu_exec`, `cpu_exec`）调用了 RISC-V 的异常处理逻辑。
```gdb
(gdb) bt
#0  helper_raise_exception (env=0x5e02c1cc92a0, 
    exception=8)
    at /root/build/qemu-4.1.1/target/riscv/op_helper.c:39
#1  0x0000716359a51fbf in code_gen_buffer ()
#2  0x00005e02c05f82fb in cpu_tb_exec (
    cpu=0x5e02c1cc0890, 
    itb=0x716359a51ec0 <code_gen_buffer+339603>)
    at /root/build/qemu-4.1.1/accel/tcg/cpu-exec.c:173
#3  0x00005e02c05f9141 in cpu_loop_exec_tb (
    cpu=0x5e02c1cc0890, 
    tb=0x716359a51ec0 <code_gen_buffer+339603>, 
    last_tb=0x7163599fd918, 
    tb_exit=0x7163599fd910)
    at /root/build/qemu-4.1.1/accel/tcg/cpu-exec.c:621
#4  0x00005e02c05f9476 in cpu_exec (
    cpu=0x5e02c1cc0890)
    at /root/build/qemu-4.1.1/accel/tcg/cpu-exec.c:732
--Type <RET> for more, q to quit, c to continue without paging--
#5  0x00005e02c05abad6 in tcg_cpu_exec (
    cpu=0x5e02c1cc0890)
    at /root/build/qemu-4.1.1/cpus.c:1435
#6  0x00005e02c05ac38f in qemu_tcg_cpu_thread_fn
    (arg=0x5e02c1cc0890)
    at /root/build/qemu-4.1.1/cpus.c:1743
#7  0x00005e02c0a2e457 in qemu_thread_start (
    args=0x5e02c1cd6f30)
    at util/qemu-thread-posix.c:502
#8  0x000071635c294ac3 in start_thread (
    arg=<optimized out>)
    at ./nptl/pthread_create.c:442
#9  0x000071635c3268c0 in clone3 ()
    at ../sysdeps/unix/sysv/linux/x86_64/clone3.S:81
```

这揭示了“虚拟机”的本质：Guest 的一条 `ecall` 指令，在 Host 侧对应着一系列复杂的 C 函数调用，用于更新虚拟 CPU 的状态（如 `scause`, `sepc`, `pc` 等）。

#### 4. 恢复执行

在 **Terminal 3** 中执行删除所有断点并执行 `continue`，QEMU 恢复运行，**Terminal 2** 重新获得控制权。

### 调试结果总结

| 观察视角 | 终端 | 操作 | 观察到的现象 | 结论 |
| :--- | :--- | :--- | :--- | :--- |
| **内核视角** | Terminal 2 | 单步 `ecall` | PC 从 `0x800...` 跳变到 `0xffff...` | 硬件（模拟器）完成了特权级切换和跳转 |
| **内核视角** | Terminal 2 | 查看 `scause` | 值变为 8 | 硬件正确识别了异常类型 |
| **模拟器视角** | Terminal 3 | 断点 `do_interrupt` | Host GDB 捕获到函数调用 | Guest 的硬件行为实为 Host 的软件模拟 |

---

### TCG (Tiny Code Generator) 翻译机制（概念补充）

#### 什么是TCG

QEMU使用TCG进行指令翻译：
- **动态二进制翻译**：将客户机指令（如RISC-V）翻译为主机指令（如x86）
- **翻译块（TB）**：以基本块为单位进行翻译和缓存
- **JIT编译**：第一次执行时翻译，后续直接执行翻译后的代码

#### 调试中观察到的TCG过程

在lab2的地址翻译调试中，我们实际上也遇到了TCG：

```gdb
# 在QEMU中设置断点时
(gdb) break get_physical_address
Breakpoint 1 at ...

# 执行时可以看到：
(gdb) bt
#0  get_physical_address ()
#1  riscv_cpu_tlb_fill ()
#2  tcg_gen_code ()          # ← TCG代码生成
#3  tb_gen_code ()           # ← 翻译块生成
#4  cpu_exec ()
```

**TCG翻译流程**：

```
1. 取指令（Fetch）
   ├─> 从客户机PC读取RISC-V指令
   └─> 如ecall、sret、ld等

2. 翻译（Translate）
   ├─> 将RISC-V指令转换为TCG中间表示（IR）
   ├─> TCG IR是与平台无关的操作序列
   └─> 例如：ecall → 调用helper_ecall函数

3. 代码生成（Generate）
   ├─> 将TCG IR转换为主机机器码（如x86）
   └─> 缓存到翻译块（Translation Block）

4. 执行（Execute）
   └─> 直接执行主机机器码（高性能）

5. 特殊情况处理
   ├─> 特权指令（ecall/sret）→ 调用helper函数
   ├─> 内存访问 → 触发TLB查找
   └─> 异常 → 退出翻译块，进入异常处理
```

#### TCG与地址翻译的关系

在lab2调试地址翻译时：

```
用户程序访问虚拟地址 va
  ↓
TCG翻译内存访问指令（如ld）
  ↓
生成主机代码：调用TLB查找
  ↓
TLB miss → get_physical_address()  ← 我们在这里设置断点
  ↓
查页表，返回物理地址 pa
  ↓
完成内存访问
```

### 调试过程中的有趣发现

#### 1. sscratch寄存器的巧妙用法

在`trapentry.S`中，sscratch用于判断来自用户态还是内核态：

```asm
csrrw sp, sscratch, sp  # 交换sp和sscratch
bnez sp, _save_context  # sp非0说明来自用户态
```

**为什么这样设计？**
- 用户态时：sscratch保存内核栈指针，交换后sp指向内核栈
- 内核态时：sscratch为0，交换后sp变为0，需要恢复

这是RISC-V架构的经典技巧！

#### 2. QEMU模拟硬件的精细度

如果 host 侧 QEMU 带符号并可 attach，确实能观察到它在处理异常/特权指令时会更新“客户机的 CSR 状态”（如 `sepc/scause/sstatus`）并跳转到 `stvec` 指定入口。由于不同版本 QEMU 内部实现细节差异较大（函数名/数据结构会变化），这里以“现象可验证”为准，不再粘贴特定版本的内部代码片段。

#### 3. 断点的本质

在QEMU GDB调试时，我们设置的断点实际上是在**主机代码**中：

```
源码：RISC-V ecall指令
  ↓ TCG翻译
主机码：call helper_ecall (x86指令)
  ↑
GDB断点实际打在这里！
```

这解释了为什么需要attach到QEMU进程才能观察指令处理。

### 使用大模型解决的问题记录

#### 问题1：用户程序符号表加载失败

**情景**：
```gdb
(gdb) b user/libs/syscall.c:26
No source file named user/libs/syscall.c.
```

**思路**：用户程序的调试信息没有被加载

**与大模型交互**：
```
Q: 为什么GDB找不到用户程序的源文件？make debug只加载了内核符号表吗？
A: 是的。用户程序被编译为独立的ELF文件，需要手动加载：
   add-symbol-file obj/__user_exit.out
```

**解决**：按照大模型建议加载符号表，问题解决

#### 问题2：QEMU断点设置位置不明确

**情景**：不知道应该在QEMU的哪个函数设置断点来观察ecall处理

**与大模型交互**：
```
Q: QEMU源码中处理RISC-V ecall指令的关键函数是什么？
A: 主要是以下几个函数：
   - riscv_cpu_do_interrupt: 异常处理入口
   - helper_ecall: ecall指令的helper函数
   - riscv_cpu_tlb_fill: TLB填充（用于地址翻译）
   
   建议在riscv_cpu_do_interrupt设置断点。
```

**解决**：按照建议设置断点，成功观察到ecall处理流程

#### 问题3：理解TCG机制

**情景**：在调试中看到`tcg_gen_code`等函数，不理解其作用

**与大模型交互**：
```
Q: QEMU中的TCG是什么？在模拟指令执行中起什么作用？
A: TCG (Tiny Code Generator) 是QEMU的动态二进制翻译器：
   1. 将客户机指令翻译为中间表示（IR）
   2. 将IR编译为主机机器码
   3. 缓存翻译块以提高性能
   
   这使得QEMU能在x86主机上高效模拟RISC-V等其他架构。
```

**收获**：理解了QEMU的核心机制，也明白了lab2地址翻译调试中看到的相关函数

#### 问题4：gdb命令的巧用

**情景**：想在sret指令执行前暂停，但不知道如何定位到确切位置

**与大模型交互**：
```
Q: 如何在GDB中定位到汇编文件中的特定指令？
A: 可以使用以下方法：
   - break *函数名：在函数入口设置断点
   - break 文件名:行号：在源码行设置断点
   - info line 函数名：查看函数的地址范围
   - x/10i $pc：查看当前PC后的10条指令
   - disassemble 函数名：反汇编函数
```

**应用**：使用`x/10i $pc`配合`si`精确控制执行流程

---

## 实验总结

### 关键知识点

#### 与OS原理对应的知识点

1. **进程管理**
   - 进程的创建（fork）
   - 程序的加载与执行（exec）
   - 进程的终止（exit）
   - 进程的同步（wait）

2. **内存管理**
   - 虚拟内存空间的建立
   - 页表的复制与管理
   - COW机制（Copy-on-Write）

3. **特权级切换**
   - 用户态与内核态的切换
   - 系统调用机制
   - 中断和异常处理

4. **进程调度**
   - 进程状态转换
   - 调度器的工作原理

#### 实验中的具体实现

1. **系统调用的实现**
   - 用户态：ecall指令触发
   - 内核态：trap处理和syscall分发
   - 返回值通过trapframe传递

2. **进程空间的管理**
   - mm_struct结构管理虚拟内存
   - VMA描述内存区域
   - 页表管理物理-虚拟地址映射

3. **第一次进入用户态的特殊处理**
   - 通过ebreak+a7=10的技巧
   - 利用trap返回机制完成特权级切换

### OS原理中重要但实验未涉及的知识点

1. **进程间通信（IPC）**
   - 管道（Pipe）
   - 消息队列（Message Queue）
   - 共享内存（Shared Memory）
   - 信号量（Semaphore）

2. **线程管理**
   - 线程的创建和销毁
   - 线程同步机制（互斥锁、条件变量）
   - 线程局部存储

3. **高级调度算法**
   - 多级反馈队列
   - 实时调度（EDF、RM）
   - 多处理器调度

4. **内存管理高级特性**
   - 页面置换算法（LRU、Clock等）
   - 工作集模型
   - 内存压缩和回收

5. **文件系统**
   - 文件的组织和管理
   - 目录结构
   - 磁盘调度

6. **安全与保护**
   - 访问控制
   - 能力机制
   - 安全策略

### 实验心得

1. **理论与实践结合**：通过实际编码实现了fork/exec/wait/exit等核心系统调用，深刻理解了进程管理的原理

2. **调试技巧的重要性**：使用双重GDB方案观察系统调用流程，理解了QEMU模拟硬件的机制

3. **代码阅读能力提升**：通过阅读ucore源码，学会了如何理解大型系统软件的架构

4. **大模型的辅助作用**：在遇到困难时，大模型提供了有效的指导和思路，提高了学习效率

---

## 测试结果

执行`make grade`后的输出：

```bash
$ make grade
......
badsegment:              (1.8s)
  -check result:                             OK
  -check output:                             OK
divzero:                 (1.8s)
  -check result:                             OK
  -check output:                             OK
softint:                 (1.7s)
  -check result:                             OK
  -check output:                             OK
faultread:               (1.8s)
  -check result:                             OK
  -check output:                             OK
faultreadkernel:         (1.8s)
  -check result:                             OK
  -check output:                             OK
hello:                   (1.7s)
  -check result:                             OK
  -check output:                             OK
testbss:                 (1.8s)
  -check result:                             OK
  -check output:                             OK
pgdir:                   (1.7s)
  -check result:                             OK
  -check output:                             OK
yield:                   (1.8s)
  -check result:                             OK
  -check output:                             OK
badarg:                  (1.7s)
  -check result:                             OK
  -check output:                             OK
exit:                    (1.8s)
  -check result:                             OK
  -check output:                             OK
spin:                    (1.9s)
  -check result:                             OK
  -check output:                             OK
waitkill:                (2.1s)
  -check result:                             OK
  -check output:                             OK
forktest:                (1.9s)
  -check result:                             OK
  -check output:                             OK
forktree:                (1.9s)
  -check result:                             OK
  -check output:                             OK
Total Score: 150/150
```

所有测试用例通过！✅

---

## 扩展练习 Challenge：用户程序何时被预先加载到内存？与常见 OS 有何区别？

### 1) 本实验里“预先加载”发生在什么时候？

本仓库采用 **Link-in-Kernel**：用户程序（如 `user/exit.c`）会先被编译成独立 ELF（例如 `obj/__user_exit.out`），随后以**二进制数据**的形式被链接进内核镜像，并通过符号（如 `_binary_obj___user_exit_out_start/_size`）在内核中可直接引用。

因此：

- **在构建阶段**：用户程序已被“打包进”`bin/kernel`；
- **在启动阶段**：QEMU 把 `bin/kernel` 整体加载到内存（本仓库 `Makefile` 使用 `-device loader,file=...,addr=0x80200000`），用户程序的二进制内容随内核镜像一起进入内存；
- **在执行阶段**：`user_main()` 通过 `KERNEL_EXECVE/KERNEL_EXECVE2` 把这段“已在内核内存中的 ELF 内容”指针传给 `kernel_execve -> sys_exec -> do_execve -> load_icode`，由 `load_icode` 把 ELF 段映射到用户虚拟地址空间并设置好 trapframe。

### 2) 与常见操作系统的区别与原因

常见 OS 的 `execve` 通常从**文件系统**读取可执行文件，按需把段加载到内存（常配合缓存、按需分页/demand paging、VFS、权限检查等）。

而本实验：

- 没有依赖完整的文件系统路径来读取 ELF；
- 把“用户程序载体”简化为“内核镜像里的静态字节数组”；
- 目的是让实验聚焦在 **页表构建、用户栈映射、trapframe 设置、以及 U/S 特权级切换** 这些核心机制上，降低 I/O 与文件系统实现带来的复杂度。

---

## 参考资料

1. uCore实验指导书 - Lab5
2. RISC-V Privileged Specification
3. QEMU源码（target/riscv/）
4. GDB调试手册
5. 《Operating Systems: Three Easy Pieces》

