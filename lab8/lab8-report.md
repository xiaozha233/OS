# Lab8 实验报告：文件系统

## 小组信息

**小组成员及分工：**

| 学号 | 姓名 | 分工 |
|------|------|------|
| 2314076 | 查许琴 | 练习0 (已有实验代码填入)、练习1 (读文件操作实现) |
| 2314035 | 陈翔 | 练习2 (加载执行程序机制实现)、补充修复 (fork/exec与copy_range修复) |
| 2313255 | 刘璇 | 扩展练习 Challenge1 (PIPE机制设计)、Challenge2 (软硬链接机制设计)、实验报告撰写 |

---

## 实验目的

通过完成本次实验，希望能够达到以下目标：

- 了解文件系统抽象层-VFS的设计与实现
- 了解基于索引节点组织方式的Simple FS文件系统与操作的设计与实现
- 了解"一切皆为文件"思想的设备文件设计
- 了解简单系统终端的实现

## 实验内容

本次实验涉及文件系统的实现，通过分析了解ucore文件系统的总体架构设计，完善读写文件操作（实现`sfs_io_nolock()`函数），重新实现基于文件系统的执行程序机制（实现`load_icode()`函数），从而可以完成执行存储在磁盘上的文件和实现文件读写等功能。

---

## 练习0：填写已有实验

本实验依赖实验2/3/4/5/6/7。需要将之前实验的代码填入本实验中相应位置。

### 主要修改内容

1. **`alloc_proc` 函数更新**：在Lab8中需要增加文件结构指针的初始化

```c
// kern/process/proc.c - alloc_proc函数中增加
proc->filesp = NULL; // 文件表指针初始化为空
```

2. **`do_fork` 函数更新**：需要调用`copy_files`复制父进程的文件结构

```c
// 在 do_fork 中增加文件系统复制
if (copy_files(clone_flags, proc) != 0) {
    goto bad_fork_cleanup_kstack;
}
```

3. **`proc_run` 函数更新**：在切换进程时需要刷新TLB

```c
// 切换页表后刷新TLB
lsatp(next->pgdir);
flush_tlb();
```

4. **`do_exit` 函数更新**：退出时需要释放文件结构

```c
put_files(current); // 释放文件结构
```

---

## 练习1：完成读文件操作的实现（需要编码）

### 1.1 设计实现过程

`sfs_io_nolock`函数是SFS文件系统中实现文件读写操作的核心函数。该函数需要处理三种情况：

1. **第一个块不对齐**：如果读写的起始位置不在块边界上，需要先处理从起始位置到第一个块末尾的部分数据
2. **中间的完整块**：处理中间完全对齐的块，可以进行整块读写
3. **最后一个块不对齐**：如果读写的结束位置不在块边界上，需要处理最后一个块的部分数据

### 1.2 代码实现

```c
static int
sfs_io_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, void *buf, off_t offset, size_t *alenp, bool write) {
    struct sfs_disk_inode *din = sin->din;
    assert(din->type != SFS_TYPE_DIR);
    // 计算结束位置
    off_t endpos = offset + *alenp, blkoff;
    *alenp = 0;
    // 检查偏移量是否合法
    if (offset < 0 || offset >= SFS_MAX_FILE_SIZE || offset > endpos) {
        return -E_INVAL;
    }
    if (offset == endpos) {
        return 0;
    }
    if (endpos > SFS_MAX_FILE_SIZE) {
        endpos = SFS_MAX_FILE_SIZE;
    }
    // 如果是读操作，检查是否超出文件实际大小
    if (!write) {
        if (offset >= din->size) {
            return 0;
        }
        if (endpos > din->size) {
            endpos = din->size;
        }
    }

    // 定义函数指针，支持读或写
    int (*sfs_buf_op)(struct sfs_fs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset);
    int (*sfs_block_op)(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks);
    if (write) {
        sfs_buf_op = sfs_wbuf, sfs_block_op = sfs_wblock;
    }
    else {
        sfs_buf_op = sfs_rbuf, sfs_block_op = sfs_rblock;
    }

    int ret = 0;
    size_t size, alen = 0;
    uint32_t ino;
    // 计算起始块号和涉及的块数
    uint32_t blkno = offset / SFS_BLKSIZE;          // 读写起始块号
    uint32_t nblks = endpos / SFS_BLKSIZE - blkno;  // 需要读写的块数

    // 计算起始块内偏移
    blkoff = offset % SFS_BLKSIZE;

    // 情况1：处理第一个不对齐的块
    if (blkoff != 0) {
        // 计算需要处理的大小
        size = (nblks != 0) ? (SFS_BLKSIZE - blkoff) : (endpos - offset);
        
        // 获取磁盘块号
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        
        // 读/写部分块
        if ((ret = sfs_buf_op(sfs, buf, size, ino, blkoff)) != 0) {
            goto out;
        }
        
        // 更新状态
        alen += size;
        buf += size;
        if (nblks == 0) {
            goto out;
        }
        blkno++;
        nblks--;
    }

    // 情况2：处理中间的完整块
    while (nblks > 0) {
        // 获取磁盘块号
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        
        // 读/写完整块
        if ((ret = sfs_block_op(sfs, buf, ino, 1)) != 0) {
            goto out;
        }
        
        // 更新状态
        alen += SFS_BLKSIZE;
        buf += SFS_BLKSIZE;
        blkno++;
        nblks--;
    }

    // 情况3：处理最后一个不对齐的块
    if (endpos % SFS_BLKSIZE != 0) {
        size = endpos % SFS_BLKSIZE;
        
        // 获取磁盘块号
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        
        // 读/写部分块
        if ((ret = sfs_buf_op(sfs, buf, size, ino, 0)) != 0) {
            goto out;
        }
        
        alen += size;
    }

out:
    *alenp = alen;
    // 如果是写操作且超出了原文件大小，更新文件大小
    if (offset + alen > sin->din->size) {
        sin->din->size = offset + alen;
        sin->dirty = 1;
    }
    return ret;
}
```

### 1.3 关键函数说明

| 函数名 | 功能说明 |
|--------|----------|
| `sfs_bmap_load_nolock` | 根据文件内的逻辑块索引，获取对应的物理磁盘块号 |
| `sfs_buf_op` | 部分块读写操作的函数指针（指向`sfs_rbuf`或`sfs_wbuf`） |
| `sfs_block_op` | 整块读写操作的函数指针（指向`sfs_rblock`或`sfs_wblock`） |

### 1.4 设计思路分析

1. **块对齐处理**：文件系统以块（4KB）为单位进行I/O操作，但用户请求的读写可能不对齐，需要分三种情况处理
2. **函数指针设计**：使用函数指针统一处理读和写操作，减少代码重复
3. **错误处理**：每一步操作都进行错误检查，出错时立即返回
4. **文件大小更新**：写操作可能扩展文件大小，需要及时更新inode信息

---

## 练习2：完成基于文件系统的执行程序机制的实现（需要编码）

### 2.1 设计实现过程

`load_icode`函数需要从文件系统中读取ELF格式的可执行文件，并加载到进程的内存空间中。相比Lab5中从内存加载，Lab8需要从文件系统读取。

### 2.2 代码实现

```c
static int
load_icode(int fd, int argc, char **kargv)
{
    assert(argc >= 0 && argc <= EXEC_MAX_ARG_NUM);
    
    // ==================== Step 1: 创建 mm ====================
    int ret = -E_NO_MEM;
    struct mm_struct *mm;
    
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }
    
    // ==================== Step 2: 读取 ELF 头部 ====================
    struct elfhdr __elf, *elf = &__elf;
    if ((ret = load_icode_read(fd, elf, sizeof(struct elfhdr), 0)) != 0) {
        goto bad_elf_cleanup_pgdir;
    }
    if (elf->e_magic != ELF_MAGIC) {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }
    
    // ==================== Step 3: 加载各个段 ====================
    struct proghdr __ph, *ph = &__ph;
    uint32_t vm_flags, perm;
    
    for (int i = 0; i < elf->e_phnum; i++) {
        off_t phoff = elf->e_phoff + sizeof(struct proghdr) * i;
        if ((ret = load_icode_read(fd, ph, sizeof(struct proghdr), phoff)) != 0) {
            goto bad_cleanup_mmap;
        }
        if (ph->p_type != ELF_PT_LOAD) {
            continue;
        }
        if (ph->p_filesz > ph->p_memsz) {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }
        // 注意：PT_LOAD 段可能是 pure-BSS（p_filesz==0 && p_memsz>0）。
        // 这种段也必须 mm_map + 分配页 + memset(0)，否则用户程序访问全局变量会缺页异常。
        if (ph->p_memsz == 0) {
            continue;
        }
        
        // 设置权限
        vm_flags = 0;
        perm = PTE_U;
        if (ph->p_flags & ELF_PF_X) { vm_flags |= VM_EXEC; perm |= PTE_X; }
        if (ph->p_flags & ELF_PF_W) { vm_flags |= VM_WRITE; perm |= PTE_W; }
        if (ph->p_flags & ELF_PF_R) { vm_flags |= VM_READ; perm |= PTE_R; }
        
        // 创建 VMA
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
            goto bad_cleanup_mmap;
        }

        // 纯 BSS 段：直接分配并清零（没有文件内容可读）
        if (ph->p_filesz == 0) {
            uintptr_t start = ph->p_va, end = ph->p_va + ph->p_memsz;
            uintptr_t la = ROUNDDOWN(start, PGSIZE);
            while (start < end) {
                struct Page *page = pgdir_alloc_page(mm->pgdir, la, perm);
                if (page == NULL) {
                    ret = -E_NO_MEM;
                    goto bad_cleanup_mmap;
                }
                size_t off = start - la;
                size_t size = PGSIZE - off;
                la += PGSIZE;
                if (end < la) { size -= la - end; }
                memset((void *)(page2kva(page) + off), 0, size);
                start += size;
            }
            continue;
        }
        
        // 分配页面并读取文件内容
        off_t offset = ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va, end, la = ROUNDDOWN(start, PGSIZE);
        
        end = ph->p_va + ph->p_filesz;
        while (start < end) {
            struct Page *page = pgdir_alloc_page(mm->pgdir, la, perm);
            if (page == NULL) {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            off = start - la;
            size = PGSIZE - off;
            la += PGSIZE;
            if (end < la) { size -= la - end; }
            if ((ret = load_icode_read(fd, page2kva(page) + off, size, offset)) != 0) {
                goto bad_cleanup_mmap;
            }
            start += size;
            offset += size;
        }
        
        // 处理 BSS 部分（清零）：包括最后一页未对齐的尾部，以及后续整页
        end = ph->p_va + ph->p_memsz;
        if (start < la) {
            if (start == end) {
                continue;
            }
            off = start + PGSIZE - la;
            size = PGSIZE - off;
            if (end < la) { size -= la - end; }
            memset(page2kva(get_page(mm->pgdir, start, NULL)) + off, 0, size);
            start += size;
        }
        while (start < end) {
            struct Page *page = pgdir_alloc_page(mm->pgdir, la, perm);
            if (page == NULL) {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            off = start - la;
            size = PGSIZE - off;
            la += PGSIZE;
            if (end < la) { size -= la - end; }
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }
    
    // ==================== Step 4: 设置用户栈 ====================
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }
    // 分配用户栈页面
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 2*PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 3*PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 4*PGSIZE, PTE_USER) != NULL);
    
    // ==================== Step 5: 更新进程状态 ====================
    mm_count_inc(mm);
    current->mm = mm;
    current->pgdir = PADDR(mm->pgdir);
    lsatp(PADDR(mm->pgdir));
    
    // ==================== Step 6: 设置 argc/argv ====================
    uintptr_t stacktop = USTACKTOP;
    uintptr_t argv_ptrs[EXEC_MAX_ARG_NUM];
    
    // 将参数字符串复制到用户栈
    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(kargv[i]) + 1;
        stacktop -= len;
        struct Page *page = get_page(mm->pgdir, stacktop, NULL);
        uintptr_t kva = (uintptr_t)page2kva(page) + (stacktop & (PGSIZE - 1));
        strcpy((char *)kva, kargv[i]);
        argv_ptrs[i] = stacktop;
    }
    
    // 对齐并压入 argv 指针数组
    stacktop = ROUNDDOWN(stacktop, sizeof(uintptr_t));
    stacktop -= sizeof(uintptr_t);  // argv[argc] = NULL
    // ... 压入argv指针 ...
    
    // ==================== Step 7: 设置 trapframe ====================
    struct trapframe *tf = current->tf;
    memset(tf, 0, sizeof(struct trapframe));
    tf->gpr.sp = stacktop;
    tf->gpr.a0 = argc;
    tf->gpr.a1 = uargv;
    tf->epc = elf->e_entry;
    tf->status = (read_csr(sstatus) | SSTATUS_SPIE) & ~SSTATUS_SPP;
    
    // ==================== Step 8: 关闭文件 ====================
    sysfile_close(fd);
    
    return 0;
    
bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    return ret;
}
```

### 2.3 与Lab5实现的区别

| 对比项 | Lab5 | Lab8 |
|--------|------|------|
| 数据来源 | 内存中的二进制数据 | 文件系统中的ELF文件 |
| 读取方式 | 直接内存拷贝 | 通过`load_icode_read`从文件读取 |
| 文件操作 | 无 | 需要打开、读取、关闭文件 |
| 参数传递 | 无argc/argv | 需要设置argc/argv到用户栈 |
| BSS段处理 | 需要处理 BSS 清零 | 除常规 BSS 外，还需处理 pure-BSS PT_LOAD（p_filesz==0） |

### 2.4 argc/argv设置过程

1. **复制参数字符串**：将每个参数字符串从内核空间复制到用户栈
2. **记录参数地址**：保存每个参数在用户空间的地址
3. **构建argv数组**：在用户栈上构建指向各参数的指针数组
4. **设置寄存器**：将argc放入a0寄存器，argv指针放入a1寄存器

---

## （补充）与当前代码一致的关键修复点：fork/exec 后取指缺页

在实际调试中发现：如果 `copy_range()` 未正确实现，`fork` 出来的子进程无法得到父进程的用户态页内容（用户页表对应 PTE 为 0），子进程从 `fork` 返回到用户态继续执行时会触发 *Instruction page fault* 并刷屏。

当前代码在 `kern/mm/pmm.c` 中补全了 `copy_range()`：为子进程分配新页、`memcpy` 复制内容、并通过 `page_insert()` 建立映射，从而保证 `fork`/`exec`/shell 命令执行路径稳定。

---

## 扩展练习 Challenge1：UNIX的PIPE机制设计方案

### 3.1 概述

管道（Pipe）是UNIX系统中一种重要的进程间通信机制，它提供了一种单向的数据流通道。

### 3.2 数据结构设计

```c
// 管道缓冲区大小
#define PIPE_BUF_SIZE 4096

// 管道结构体
struct pipe {
    char buffer[PIPE_BUF_SIZE];     // 环形缓冲区
    uint32_t read_pos;              // 读位置
    uint32_t write_pos;             // 写位置
    uint32_t count;                 // 缓冲区中的数据量
    
    int read_open;                  // 读端是否打开
    int write_open;                 // 写端是否打开
    
    semaphore_t mutex;              // 互斥访问信号量
    semaphore_t not_full;           // 缓冲区非满信号量
    semaphore_t not_empty;          // 缓冲区非空信号量
    
    int ref_count;                  // 引用计数
};

// 管道文件结构
struct pipe_file {
    struct pipe *pipe;              // 指向管道
    int is_read_end;                // 是否是读端
};
```

### 3.3 接口设计

| 接口函数 | 语义说明 |
|----------|----------|
| `int pipe(int fd[2])` | 创建管道，fd[0]为读端，fd[1]为写端 |
| `int pipe_read(struct pipe *p, void *buf, size_t n)` | 从管道读取数据 |
| `int pipe_write(struct pipe *p, void *buf, size_t n)` | 向管道写入数据 |
| `int pipe_close(struct pipe *p, int is_write)` | 关闭管道的一端 |

### 3.4 同步互斥处理

1. **互斥访问**：使用`mutex`信号量保护对缓冲区的并发访问
2. **生产者-消费者模型**：
   - 写入时：若缓冲区满，等待`not_full`信号量
   - 读取时：若缓冲区空，等待`not_empty`信号量
3. **边界条件**：
   - 读端关闭时，写入返回错误（SIGPIPE）
   - 写端关闭时，读取返回EOF

---

## 扩展练习 Challenge2：UNIX的软连接和硬连接机制设计方案

### 4.1 概述

- **硬链接（Hard Link）**：多个目录项指向同一个inode
- **软链接/符号链接（Symbolic Link）**：一个特殊文件，内容是另一个文件的路径

### 4.2 数据结构设计

```c
// 修改 sfs_disk_inode，增加链接类型支持
struct sfs_disk_inode {
    uint32_t size;                 // 文件大小
    uint16_t type;                 // 文件类型（增加 SFS_TYPE_LINK）
    uint16_t nlinks;               // 硬链接数
    uint32_t blocks;               // 数据块数
    uint32_t direct[SFS_NDIRECT];  // 直接索引
    uint32_t indirect;             // 间接索引
};

// 符号链接结构（存储在数据块中）
struct sfs_symlink {
    char target_path[SFS_MAX_FPATH_LEN];  // 目标路径
};

// 文件类型定义
#define SFS_TYPE_LINK    3    // 符号链接类型
```

### 4.3 接口设计

| 接口函数 | 语义说明 |
|----------|----------|
| `int sfs_link(struct inode *dir, const char *name, struct inode *target)` | 创建硬链接 |
| `int sfs_symlink(struct inode *dir, const char *name, const char *target_path)` | 创建符号链接 |
| `int sfs_readlink(struct inode *node, char *buf, size_t len)` | 读取符号链接目标 |
| `int sfs_unlink(struct inode *dir, const char *name)` | 删除链接 |

### 4.4 硬链接实现要点

1. **创建硬链接**：
   - 在目标目录创建新目录项，指向同一inode
   - 增加目标inode的`nlinks`计数
   
2. **删除硬链接**：
   - 删除目录项
   - 减少inode的`nlinks`计数
   - 当`nlinks`为0时，回收inode和数据块

### 4.5 软链接实现要点

1. **创建软链接**：
   - 创建新的inode，类型为`SFS_TYPE_LINK`
   - 在数据块中存储目标路径字符串
   
2. **路径解析**：
   - 遇到符号链接时，读取目标路径
   - 递归解析目标路径（需要设置递归深度限制，防止循环链接）

### 4.6 同步互斥处理

```c
// 在创建/删除链接时使用信号量保护
semaphore_t mutex_sem;  // 在 sfs_fs 结构中

// 硬链接操作
void sfs_link_nolock(struct sfs_fs *sfs, ...) {
    down(&sfs->mutex_sem);
    // 执行链接操作
    // 更新 nlinks
    up(&sfs->mutex_sem);
}
```

---

## 重要知识点总结

### 实验中的知识点

| 知识点 | 说明 |
|--------|------|
| 虚拟文件系统（VFS） | 提供统一的文件操作接口，屏蔽底层文件系统差异 |
| 索引节点（inode） | 存储文件元数据，通过直接/间接索引定位数据块 |
| 目录项（dentry） | 维护文件名到inode的映射关系 |
| 文件描述符 | 进程访问打开文件的句柄 |
| SFS文件系统 | 基于索引的简单文件系统实现 |
| 块I/O操作 | 以块为单位进行磁盘读写，处理对齐问题 |

### 与操作系统原理对应

| 实验知识点 | OS原理知识点 | 关系说明 |
|------------|--------------|----------|
| SFS的inode结构 | 文件控制块（FCB） | inode是FCB在SFS中的具体实现 |
| VFS层 | 文件系统接口 | VFS实现了教材中描述的文件系统分层架构 |
| sfs_io_nolock | 文件读写操作 | 实现了教材中的顺序读写逻辑 |
| 目录结构 | 文件目录 | SFS使用线性表存储目录项 |

### OS原理中重要但实验未涉及的知识点

1. **文件共享与保护**：实验中未实现完整的权限控制机制
2. **磁盘调度算法**：实验使用内存模拟磁盘，未涉及真实磁盘调度
3. **文件系统一致性**：未实现日志文件系统等保证一致性的机制
4. **磁盘空间分配策略**：SFS使用简单的位图分配，未涉及更高级的分配策略

---

## 实验结果

执行`make qemu`后，成功进入shell界面，可以执行以下命令：

- `ls`：列出当前目录文件
- `hello`：执行hello程序，输出 "Hello world!!"
- 其他用户程序如`forktest`、`matrix`等均可正常执行

执行make grade得到：
```
ucore.img gmake[1]: Leaving directory '/home/albus_os/labcode/lab8'
  -sh execve:                                OK
  -user sh :                                 OK
Total Score: 100/100
```
这表明文件系统的读写操作和基于文件系统的程序执行机制已正确实现。
