# Lab8 文件系统实验讲义 - 第七部分：练习2 load_icode 实现

> **前置知识**：ELF 文件格式、进程加载、Lab5 的 load_icode  
> **学习产出**：完成基于文件系统的 `load_icode()` 函数，实现从文件加载可执行程序

---

## 一、练习2 任务说明 \[S09]

### 1.1 任务描述

> **练习2：完成基于文件系统的执行程序机制的实现（需要编码）**
>
> 改写 `proc.c` 中的 `load_icode` 函数和其他相关函数，实现基于文件系统的执行程序机制。执行 `make qemu`，如果能看到 sh 用户程序的执行界面，则基本成功了。如果在 sh 用户界面上可以执行 `ls`、`hello` 等其他放置在 sfs 文件系统中的执行程序，则可以认为本实验基本成功。

### 1.2 与 Lab5 的区别

| 对比项 | Lab5 | Lab8 |
|-------|------|------|
| 程序来源 | 直接嵌入内核（二进制数据） | 从 SFS 文件系统读取 |
| 读取方式 | 直接访问内存 | 通过文件描述符 fd 读取 |
| 参数传递 | 无需处理 argc/argv | 需要设置 argc/argv 到用户栈 |

### 1.3 函数签名变化

```c
// Lab5 的版本
static int load_icode(unsigned char *binary, size_t size);

// Lab8 的版本
static int load_icode(int fd, int argc, char **kargv);
```

---

## 二、ELF 文件格式回顾 \[C14]

### 2.1 ELF 头部（elfhdr）

```c
// libs/elf.h
struct elfhdr {
    uint32_t e_magic;     // 魔数：0x464C457F ("\x7FELF")
    uint8_t e_elf[12];    // ELF 标识信息
    uint16_t e_type;      // 文件类型：2=可执行文件
    uint16_t e_machine;   // 目标架构
    uint32_t e_version;   // ELF 版本
    uint64_t e_entry;     // 程序入口点地址
    uint64_t e_phoff;     // Program Header 在文件中的偏移
    uint64_t e_shoff;     // Section Header 在文件中的偏移
    uint32_t e_flags;     // 标志位
    uint16_t e_ehsize;    // ELF 头部大小
    uint16_t e_phentsize; // 每个 Program Header 的大小
    uint16_t e_phnum;     // Program Header 的数量
    // ...
};
```

### 2.2 程序头部（proghdr）

```c
// libs/elf.h
struct proghdr {
    uint32_t p_type;   // 段类型：1=可加载段
    uint32_t p_flags;  // 权限标志：R/W/X
    uint64_t p_offset; // 段在文件中的偏移
    uint64_t p_va;     // 段应加载到的虚拟地址
    uint64_t p_pa;     // 物理地址（通常不用）
    uint64_t p_filesz; // 段在文件中的大小
    uint64_t p_memsz;  // 段在内存中的大小（≥ p_filesz，差值是 BSS）
    uint64_t p_align;  // 对齐要求
};
```

### 2.3 ELF 文件结构图

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ELF 可执行文件结构                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌───────────────────────┐  offset = 0                             │
│  │      ELF Header       │  e_magic = 0x464C457F                   │
│  │    (struct elfhdr)    │  e_entry = 程序入口地址                  │
│  │                       │  e_phoff = Program Header 偏移           │
│  │                       │  e_phnum = Program Header 数量           │
│  └───────────────────────┘                                          │
│                                                                     │
│  ┌───────────────────────┐  offset = e_phoff                       │
│  │   Program Header 0    │  p_type = PT_LOAD                       │
│  │   (struct proghdr)    │  p_va = 0x10000 (代码段起始)             │
│  │                       │  p_offset = 代码在文件中的位置            │
│  │                       │  p_filesz = 代码大小                     │
│  │                       │  p_memsz = 内存大小                      │
│  ├───────────────────────┤                                          │
│  │   Program Header 1    │  p_type = PT_LOAD                       │
│  │                       │  p_va = 0x20000 (数据段起始)             │
│  │                       │  ...                                    │
│  └───────────────────────┘                                          │
│                                                                     │
│  ┌───────────────────────┐  offset = ph[0].p_offset                │
│  │    代码段 (.text)      │  实际的程序代码                          │
│  │                       │                                          │
│  └───────────────────────┘                                          │
│                                                                     │
│  ┌───────────────────────┐  offset = ph[1].p_offset                │
│  │    数据段 (.data)      │  已初始化的全局变量                       │
│  │                       │                                          │
│  └───────────────────────┘                                          │
│                                                                     │
│  注意：BSS 段不占用文件空间，但 p_memsz > p_filesz 时                 │
│       需要在内存中分配并清零                                          │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```
\[Fig·C14-1] ELF 可执行文件结构

---

## 三、load_icode_read 辅助函数 \[S09]

Lab8 提供了一个读取文件的辅助函数：

```c
// kern/process/proc.c
static int
load_icode_read(int fd, void *buf, size_t len, off_t offset)
{
    int ret;
    // 先移动文件指针到 offset 位置
    if ((ret = sysfile_seek(fd, offset, LSEEK_SET)) != 0) {
        return ret;
    }
    // 读取 len 字节到 buf
    if ((ret = sysfile_read(fd, buf, len)) != len) {
        return (ret < 0) ? ret : -1;
    }
    return 0;
}
```

**使用方式**：
```c
struct elfhdr elf;
// 从文件 fd 的偏移 0 处读取 sizeof(elfhdr) 字节到 elf
load_icode_read(fd, &elf, sizeof(struct elfhdr), 0);
```

---

## 四、用户栈的 argc/argv 布局 \[S09]

Lab8 需要在用户栈上设置 argc 和 argv，这是 Lab5 没有的新要求。

### 4.1 用户栈布局

```
高地址
┌─────────────────────────────────────────────┐  USTACKTOP (0x80000000)
│                                             │
├─────────────────────────────────────────────┤
│  argv[argc-1] 的字符串内容 "arg2\0"          │  ← kargv[argc-1]
├─────────────────────────────────────────────┤
│  argv[1] 的字符串内容 "arg1\0"               │  ← kargv[1]
├─────────────────────────────────────────────┤
│  argv[0] 的字符串内容 "program_name\0"       │  ← kargv[0]
├─────────────────────────────────────────────┤
│  argv[argc] = NULL                          │  结束标记
├─────────────────────────────────────────────┤
│  argv[argc-1] (指向上面字符串的指针)          │
├─────────────────────────────────────────────┤
│  ...                                        │
├─────────────────────────────────────────────┤
│  argv[1] (指针)                              │
├─────────────────────────────────────────────┤
│  argv[0] (指针)                              │  ← uargv (argv 数组起始)
├─────────────────────────────────────────────┤
│  argc (整数值)                               │  ← 栈顶 sp
└─────────────────────────────────────────────┘
低地址
```
\[Fig·S09-1] 用户栈上的 argc/argv 布局

### 4.2 传递给 main 的调用约定

在 RISC-V 中，函数参数通过寄存器传递：
- `a0` = argc
- `a1` = argv（指向 argv 数组的指针）

---

## 五、实现步骤详解

### 5.1 整体流程

```
┌─────────────────────────────────────────────────────────────────────────┐
│                   load_icode 实现流程                                   │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  Step 1: 创建内存管理结构                                               │
│      mm = mm_create()                                                  │
│      setup_pgdir(mm)                                                   │
│                                                                         │
│  Step 2: 读取并解析 ELF 头部                                            │
│      load_icode_read(fd, &elf, sizeof(elfhdr), 0)                      │
│      验证魔数 elf.e_magic == ELF_MAGIC                                  │
│                                                                         │
│  Step 3: 遍历 Program Header，加载每个可加载段                          │
│      for (i = 0; i < elf.e_phnum; i++) {                               │
│          读取 proghdr                                                   │
│          if (ph.p_type == ELF_PT_LOAD) {                               │
│              mm_map() 建立 VMA                                          │
│              分配页面，读取文件内容到页面                                 │
│              清零 BSS 部分                                              │
│          }                                                              │
│      }                                                                  │
│                                                                         │
│  Step 4: 建立用户栈                                                     │
│      mm_map(USTACKTOP - USTACKSIZE, USTACKSIZE, ...)                   │
│      分配栈页面                                                         │
│                                                                         │
│  Step 5: 设置 argc/argv 到用户栈                                        │
│      将 kargv 中的字符串复制到用户栈                                     │
│      设置 argv 指针数组                                                 │
│      将 argc 压入栈顶                                                   │
│                                                                         │
│  Step 6: 更新进程的内存管理结构                                          │
│      current->mm = mm                                                  │
│      current->pgdir = PADDR(mm->pgdir)                                 │
│      lsatp(PADDR(mm->pgdir))                                           │
│                                                                         │
│  Step 7: 设置 trapframe                                                │
│      tf->gpr.sp = 用户栈指针                                            │
│      tf->epc = elf.e_entry                                             │
│      tf->status = 用户态 + 中断使能                                     │
│                                                                         │
│  Step 8: 关闭文件描述符                                                 │
│      sysfile_close(fd)                                                 │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```
\[Fig·S09-2] load_icode 实现流程

### 5.2 Step 1: 创建内存管理结构

```c
int ret = -E_NO_MEM;
struct mm_struct *mm;

// 创建新的内存管理结构
if ((mm = mm_create()) == NULL) {
    goto bad_mm;
}

// 创建页目录表
if (setup_pgdir(mm) != 0) {
    goto bad_pgdir_cleanup_mm;
}
```

### 5.3 Step 2: 读取并解析 ELF 头部

```c
struct elfhdr __elf, *elf = &__elf;

// 从文件读取 ELF 头部
if ((ret = load_icode_read(fd, elf, sizeof(struct elfhdr), 0)) != 0) {
    goto bad_elf_cleanup_pgdir;
}

// 验证 ELF 魔数
if (elf->e_magic != ELF_MAGIC) {
    ret = -E_INVAL_ELF;
    goto bad_elf_cleanup_pgdir;
}
```

### 5.4 Step 3: 遍历 Program Header 加载段

```c
struct proghdr __ph, *ph = &__ph;

// 遍历所有 Program Header
for (int i = 0; i < elf->e_phnum; i++) {
    // 读取第 i 个 Program Header
    off_t phoff = elf->e_phoff + sizeof(struct proghdr) * i;
    if ((ret = load_icode_read(fd, ph, sizeof(struct proghdr), phoff)) != 0) {
        goto bad_cleanup_mmap;
    }
    
    // 只处理可加载段
    if (ph->p_type != ELF_PT_LOAD) {
        continue;
    }
    
    // 验证段大小
    if (ph->p_filesz > ph->p_memsz) {
        ret = -E_INVAL_ELF;
        goto bad_cleanup_mmap;
    }
    if (ph->p_filesz == 0) {
        continue;  // 空段，跳过
    }
    
    // 根据 p_flags 设置权限
    uint32_t vm_flags = 0, perm = PTE_U;
    if (ph->p_flags & ELF_PF_X) vm_flags |= VM_EXEC;
    if (ph->p_flags & ELF_PF_W) vm_flags |= VM_WRITE;
    if (ph->p_flags & ELF_PF_R) vm_flags |= VM_READ;
    if (vm_flags & VM_WRITE) perm |= PTE_W;
    
    // 创建 VMA
    if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }
    
    // 分配物理页并复制文件内容
    // 这部分比较复杂，需要处理页边界
    // ...（详见后续代码）
}
```

### 5.5 Step 3.4-3.5: 分配页面并读取文件内容

这是最复杂的部分，需要处理：
1. 段起始地址可能不是页对齐的
2. 需要逐页分配内存
3. 从文件读取 `p_filesz` 字节
4. 剩余的 `p_memsz - p_filesz` 字节（BSS）需要清零

```c
// 计算段的起始和结束地址（页对齐）
uintptr_t start = ph->p_va, end = ph->p_va + ph->p_filesz;
uintptr_t la = ROUNDDOWN(start, PGSIZE);
uintptr_t page_start = la;
off_t file_offset = ph->p_offset;

// 逐页处理
while (start < end) {
    // 分配一个物理页
    struct Page *page = pgdir_alloc_page(mm->pgdir, la, perm);
    if (page == NULL) {
        ret = -E_NO_MEM;
        goto bad_cleanup_mmap;
    }
    
    // 计算本页需要读取的数据范围
    uintptr_t page_end = la + PGSIZE;
    uintptr_t read_start = (start > la) ? start : la;
    uintptr_t read_end = (end < page_end) ? end : page_end;
    size_t read_size = read_end - read_start;
    
    // 计算文件偏移
    off_t offset_in_file = file_offset + (read_start - ph->p_va);
    
    // 读取文件内容到页面
    void *dst = page2kva(page) + (read_start - la);
    if ((ret = load_icode_read(fd, dst, read_size, offset_in_file)) != 0) {
        goto bad_cleanup_mmap;
    }
    
    // 如果页面开头部分不属于段，清零
    if (la < start) {
        memset(page2kva(page), 0, start - la);
    }
    
    // 移动到下一页
    start = page_end;
    la += PGSIZE;
}

// 处理 BSS 部分（p_filesz < p_memsz 的情况）
end = ph->p_va + ph->p_memsz;
while (la < ROUNDUP(end, PGSIZE)) {
    struct Page *page = pgdir_alloc_page(mm->pgdir, la, perm);
    if (page == NULL) {
        ret = -E_NO_MEM;
        goto bad_cleanup_mmap;
    }
    memset(page2kva(page), 0, PGSIZE);
    la += PGSIZE;
}
```

### 5.6 Step 4: 建立用户栈

```c
// 设置用户栈的 VMA
vm_flags = VM_READ | VM_WRITE | VM_STACK;
if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) {
    goto bad_cleanup_mmap;
}

// 分配用户栈的物理页（这里分配 4 页作为初始栈）
// 注意：ucore 使用按需分配，这里至少分配一页供 argc/argv 使用
assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - PGSIZE, PTE_USER) != NULL);
```

### 5.7 Step 5-6: 设置 argc/argv 到用户栈

```c
// 计算参数占用的总大小
uintptr_t stacktop = USTACKTOP;

// 将参数字符串复制到用户栈
// 从栈顶向下复制每个参数字符串
uintptr_t argv_strs[EXEC_MAX_ARG_NUM];
for (int i = argc - 1; i >= 0; i--) {
    size_t len = strlen(kargv[i]) + 1;  // 包含 '\0'
    stacktop -= len;
    
    // 这里需要将字符串复制到用户空间
    // 由于还没切换页表，需要使用内核态的虚拟地址
    char *dest = (char *)(page2kva(get_page(mm->pgdir, stacktop, NULL)) 
                          + (stacktop & 0xFFF));
    strcpy(dest, kargv[i]);
    argv_strs[i] = stacktop;  // 记录用户态地址
}

// 对齐到 8 字节边界
stacktop = ROUNDDOWN(stacktop, 8);

// 压入 argv[argc] = NULL
stacktop -= sizeof(uintptr_t);
// *(uintptr_t *)kernel_addr(stacktop) = 0;

// 压入 argv 指针数组
stacktop -= sizeof(uintptr_t) * argc;
uintptr_t uargv = stacktop;
for (int i = 0; i < argc; i++) {
    // *(uintptr_t *)kernel_addr(stacktop + i * sizeof(uintptr_t)) = argv_strs[i];
}

// 压入 argc
stacktop -= sizeof(uintptr_t);
// *(uintptr_t *)kernel_addr(stacktop) = argc;
```

**注意**：上述代码是简化的逻辑示意，实际实现需要正确处理用户空间地址与内核地址的转换。

### 5.8 Step 7: 更新进程状态和设置 trapframe

```c
// 更新进程的内存管理结构
mm_count_inc(mm);
current->mm = mm;
current->pgdir = PADDR(mm->pgdir);
lsatp(PADDR(mm->pgdir));

// 设置 trapframe
struct trapframe *tf = current->tf;
memset(tf, 0, sizeof(struct trapframe));

tf->gpr.sp = stacktop;        // 用户栈指针
tf->epc = elf->e_entry;        // 程序入口点
tf->status = sstatus_read();   // 读取当前 sstatus
tf->status |= SSTATUS_SPP;     // 设置为 S 模式（但 sret 后会切到 U 模式）
tf->status |= SSTATUS_SPIE;    // 使能中断

// 如果你的实现需要通过寄存器传递 argc/argv：
tf->gpr.a0 = argc;
tf->gpr.a1 = uargv;
```

### 5.9 Step 8: 关闭文件描述符

```c
// 文件已加载完成，关闭文件
sysfile_close(fd);

ret = 0;  // 成功
```

---

## 六、错误处理

```c
// 各种错误清理标签
bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    return ret;
```

---

## 七、完整代码框架（带注释）

以下是完整的实现框架。**请先尝试自己实现，再对照参考！**

```c
static int
load_icode(int fd, int argc, char **kargv) {
    // 断言检查
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
        
        // 设置权限
        vm_flags = 0;
        perm = PTE_U;
        if (ph->p_flags & ELF_PF_X) vm_flags |= VM_EXEC;
        if (ph->p_flags & ELF_PF_W) vm_flags |= VM_WRITE;
        if (ph->p_flags & ELF_PF_R) vm_flags |= VM_READ;
        if (vm_flags & VM_WRITE) perm |= PTE_W;
        
        // 创建 VMA
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0) {
            goto bad_cleanup_mmap;
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
            if (end < la) {
                size -= la - end;
            }
            if ((ret = load_icode_read(fd, page2kva(page) + off, size, 
                                        offset)) != 0) {
                goto bad_cleanup_mmap;
            }
            start += size;
            offset += size;
        }
        
        // 处理 BSS 部分
        end = ph->p_va + ph->p_memsz;
        if (start < la) {
            // 如果最后一页还有 BSS 部分需要清零
            if (start == end) {
                continue;
            }
            off = start + PGSIZE - la;
            size = PGSIZE - off;
            if (end < la) {
                size -= la - end;
            }
            memset(page2kva(get_page(mm->pgdir, start, NULL)) + off, 0, size);
            start += size;
            assert((end < la && start == end) || (end >= la && start == la));
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
            if (end < la) {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }
    
    // ==================== Step 4: 设置用户栈 ====================
    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACKTOP - USTACKSIZE, USTACKSIZE, vm_flags, NULL)) != 0) {
        goto bad_cleanup_mmap;
    }
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 2*PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 3*PGSIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm->pgdir, USTACKTOP - 4*PGSIZE, PTE_USER) != NULL);
    
    // ==================== Step 5: 设置 argc/argv ====================
    // 计算用户栈顶
    uintptr_t stacktop = USTACKTOP - argc * EXEC_MAX_ARG_LEN - sizeof(void *) * (argc + 1);
    stacktop = ROUNDDOWN(stacktop, 8);
    
    // （简化版）将 argc、argv 放入栈中
    // 具体实现需要将 kargv 中的字符串复制到用户栈
    // 这里给出简化思路，具体实现参考答案
    
    // ==================== Step 6: 更新进程状态 ====================
    mm_count_inc(mm);
    current->mm = mm;
    current->pgdir = PADDR(mm->pgdir);
    lsatp(PADDR(mm->pgdir));
    
    // ==================== Step 7: 设置 trapframe ====================
    struct trapframe *tf = current->tf;
    uintptr_t uargv = stacktop + sizeof(uintptr_t);  // argv 数组位置
    
    memset(tf, 0, sizeof(struct trapframe));
    tf->gpr.sp = stacktop;
    tf->gpr.a0 = argc;
    tf->gpr.a1 = uargv;
    tf->epc = elf->e_entry;
    tf->status = (sstatus_read() | SSTATUS_SPIE) & ~SSTATUS_SPP;
    
    // ==================== Step 8: 关闭文件 ====================
    sysfile_close(fd);
    
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
    return ret;
}
```

---

## 八、do_fork 中的修改 \[S09]

在 Lab8 中，`do_fork` 函数也需要处理文件系统相关的内容。查看 `proc.c` 中的 `do_fork` 函数，你会看到：

```c
// 需要复制父进程的文件结构
if (copy_files(clone_flags, proc) != 0) {
    goto bad_fork_cleanup_kstack;
}
```

确保这行代码在正确的位置（在 `setup_kstack` 之后，在 `copy_mm` 之前）。

---

## 九、调试建议

### 9.1 编译测试

```bash
cd /home/albus_os/labcode/lab8
make clean
make qemu
```

### 9.2 预期输出

```
sfs: mount: 'simple file system' (bindnode, binddevice): ok.
vfs: binddevice: bindnode 'disk0'.
kernel_execve: pid = 2, name = "sh".
$ ls
badarg       faultread     hello        matrix       softint      waitkill
badsegment   faultreadkernel  pgdir       priority    spin         yield
divzero      forktree      sh           sleep        testbss
exit         forktest      sleepkill                               
$ hello
Hello world!!.
I am process 3.
hello pass.
$
```

### 9.3 常见问题

| 问题 | 可能原因 |
|-----|---------|
| 启动后立即 panic | ELF 解析错误或段加载失败 |
| 执行程序时 crash | argc/argv 设置错误 |
| 程序无输出 | trapframe 设置错误，程序没有正确启动 |
| 文件找不到 | 文件系统初始化问题 |

---

## 十、自测题 \[C15]

**Q1（判断题）**：Lab8 的 `load_icode` 可以直接复用 Lab5 的代码，无需修改。
> **答案**：❌ 错误。需要修改读取方式（使用 `load_icode_read` 从文件读取），并且需要处理 argc/argv 参数传递。

**Q2（单选题）**：在 RISC-V 中，main 函数的 argc 参数通过哪个寄存器传递？
- A) a0
- B) a1
- C) sp
- D) ra

> **答案**：A) a0。argc 作为第一个参数，通过 a0 传递；argv 作为第二个参数，通过 a1 传递。

**Q3（开放题）**：解释为什么 BSS 段不占用文件空间，但需要在内存中分配？
> **答案要点**：
> 1. BSS 段存放未初始化的全局变量，它们的初始值都是 0
> 2. 不需要在文件中存储这些 0，节省文件空间
> 3. 但运行时必须在内存中分配空间，并将其清零
> 4. 这就是 `p_memsz > p_filesz` 的原因，差值就是 BSS 段大小

---

## 十一、下一步

完成练习2后，请阅读：

📖 **[08_扩展练习与复习.md](08_扩展练习与复习.md)** - Challenge 练习和总复习

---

**Covered**: S09（练习2 load_icode 实现、argc/argv 处理）
