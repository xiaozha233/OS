# Lab8 文件系统实验讲义 - 第三部分：ucore 文件系统架构

> **前置知识**：文件系统基础概念（第二部分）  
> **学习产出**：理解 ucore 文件系统的四层架构、关键数据结构、初始化流程

---

## 一、ucore 文件系统总体架构 \[S03]

### 1.1 ucore 管理的三类设备

在 ucore 中，虚拟文件系统管理三类"设备"：

| 设备 | 说明 | 支持操作 |
|-----|------|---------|
| **硬盘**（disk0） | 使用 Simple File System 管理 | 读写 |
| **标准输出**（stdout） | 控制台屏幕输出 | 只写 |
| **标准输入**（stdin） | 键盘输入 | 只读 |

> **注意**：ucore 的"硬盘"实际上是用一块**内存**来模拟的（ramdisk），这样便于在 QEMU 中运行和调试。

### 1.2 四层架构 \[S03]

ucore 的文件系统采用经典的**四层架构**设计：

```
┌─────────────────────────────────────────────────────────────────────────┐
│  第1层：通用文件系统访问接口层                                            │
│  ┌───────────────────────────────────────────────────────────────────┐ │
│  │ 用户程序接口: open(), read(), write(), close(), lseek() ...       │ │
│  │ 系统调用处理: sysfile_open(), sysfile_read(), sysfile_write() ... │ │
│  └───────────────────────────────────────────────────────────────────┘ │
│                                     │                                   │
│                                     ▼                                   │
├─────────────────────────────────────────────────────────────────────────┤
│  第2层：文件系统抽象层 (VFS)                                             │
│  ┌───────────────────────────────────────────────────────────────────┐ │
│  │ 核心接口: vfs_open(), vfs_read(), vfs_lookup()                    │ │
│  │ 核心抽象: struct inode, struct file, inode_ops                     │ │
│  │ 作用: 屏蔽不同文件系统的实现差异                                    │ │
│  └───────────────────────────────────────────────────────────────────┘ │
│                                     │                                   │
│                                     ▼                                   │
├─────────────────────────────────────────────────────────────────────────┤
│  第3层：具体文件系统层 (SFS)                                             │
│  ┌───────────────────────────────────────────────────────────────────┐ │
│  │ SFS接口: sfs_read(), sfs_write(), sfs_lookup()                    │ │
│  │ SFS结构: sfs_disk_inode, sfs_disk_entry, sfs_super                │ │
│  │ 作用: 实现具体的文件组织和索引方式                                  │ │
│  └───────────────────────────────────────────────────────────────────┘ │
│                                     │                                   │
│                                     ▼                                   │
├─────────────────────────────────────────────────────────────────────────┤
│  第4层：外设接口层                                                       │
│  ┌───────────────────────────────────────────────────────────────────┐ │
│  │ 设备抽象: struct device                                            │ │
│  │ 具体设备: disk0, stdin, stdout                                     │ │
│  │ 作用: 屏蔽硬件差异，提供统一的块读写接口                             │ │
│  └───────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
```
\[Fig·S03-1] ucore 文件系统四层架构

### 1.3 层次间的调用关系

以 `read()` 操作为例，调用链如下：

```
用户程序: read(fd, buf, len)
    │
    ▼ (系统调用)
第1层: sys_read() → sysfile_read()
    │
    ▼ (VFS 接口)
第2层: file_read() → vop_read() [通过 inode_ops 函数指针]
    │
    ▼ (具体实现)
第3层: sfs_read() → sfs_io() → sfs_io_nolock() [练习1在此！]
    │
    ▼ (设备 I/O)
第4层: sfs_rblock() / sfs_rbuf() → dop_io()
    │
    ▼ (硬件)
    ramdisk 读取数据
```
\[Fig·S03-2] read 操作的层次调用链

---

## 二、ucore 镜像构建过程 \[S03]

### 2.1 镜像组成

Lab8 的 Makefile 与之前不同，构建过程分为三部分：

```
┌─────────────────────────────────────────────────────────────┐
│                      ucore.img                               │
├────────────────┬────────────────┬───────────────────────────┤
│    sfs.img     │   swap.img     │     kernel objects        │
│  (用户程序)    │   (交换区)      │      (内核代码)           │
├────────────────┼────────────────┼───────────────────────────┤
│ 符合 SFS 格式  │ 初始化为 0     │ 编译后的内核目标文件       │
│ 存储编译好的   │ 用于页面置换    │ 包括文件系统代码           │
│ 用户程序       │                │                           │
└────────────────┴────────────────┴───────────────────────────┘
```
\[Fig·S03-3] ucore.img 的三部分组成

### 2.2 sfs.img 的创建

`tools/mksfs.c` 程序负责创建 SFS 格式的磁盘镜像：

```bash
# 简化的构建流程
./mksfs sfs.img user/hello user/sh user/ls ...
# 将编译好的用户程序打包成 SFS 磁盘镜像
```

> **学习建议**：如果你想深入理解 SFS 的磁盘布局，`tools/mksfs.c` 是一个很好的参考（约500行）。

---

## 三、关键数据结构详解 \[S03]

### 3.1 数据结构关系图

当用户进程打开一个文件时，涉及以下数据结构：

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              进程 (proc_struct)                          │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │  filesp ─────────────────────────────────────────┐                 │ │
│  └──────────────────────────────────────────────────┼─────────────────┘ │
│                                                     ▼                   │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │                    files_struct                                    │ │
│  │  ┌──────────────────────────────────────────────────────────────┐ │ │
│  │  │  pwd (当前工作目录的 inode)                                   │ │ │
│  │  │  fd_array[NOFILE] ─────────────────────────────────────┐     │ │ │
│  │  │  files_count (引用计数)                                 │     │ │ │
│  │  │  files_sem (信号量)                                     │     │ │ │
│  │  └─────────────────────────────────────────────────────────┼─────┘ │ │
│  │                                                            ▼       │ │
│  │  ┌──────────────┬──────────────┬──────────────┬────────────────┐ │ │
│  │  │  file[0]     │   file[1]    │   file[2]    │     ...        │ │ │
│  │  │  (stdin)     │  (stdout)    │  (stderr)    │                │ │ │
│  │  │  ┌────────┐  │  ┌────────┐  │  ┌────────┐  │                │ │ │
│  │  │  │ fd=0   │  │  │ fd=1   │  │  │ fd=2   │  │                │ │ │
│  │  │  │ pos=0  │  │  │ pos=0  │  │  │ pos=0  │  │                │ │ │
│  │  │  │ node ──┼──┼──│ node ──┼──┼──│ node ──┼──┼───► inode      │ │ │
│  │  │  └────────┘  │  └────────┘  │  └────────┘  │                │ │ │
│  │  └──────────────┴──────────────┴──────────────┴────────────────┘ │ │
│  └────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
                                         │
                                         ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                             inode (VFS层)                                │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │  in_info (union)                                                   │ │
│  │    ├── __device_info (设备文件)                                    │ │
│  │    └── __sfs_inode_info (SFS文件)                                  │ │
│  │  in_type: inode_type_device_info 或 inode_type_sfs_inode_info     │ │
│  │  ref_count: 引用计数                                               │ │
│  │  open_count: 打开计数                                              │ │
│  │  in_fs: 所属文件系统                                               │ │
│  │  in_ops: 操作函数指针表 ────────────────────────────┐              │ │
│  └─────────────────────────────────────────────────────┼──────────────┘ │
│                                                        ▼                │
│  ┌────────────────────────────────────────────────────────────────────┐ │
│  │                    inode_ops (函数指针表)                           │ │
│  │  vop_open, vop_close, vop_read, vop_write, vop_lookup ...         │ │
│  └────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────┘
```
\[Fig·S03-4] 进程、files_struct、file、inode 的关系

### 3.2 知识卡片：files_struct

---
**名称**：files_struct

**要解决的问题**：管理一个进程打开的所有文件

**形式化表述**：
```c
// kern/fs/fs.h
struct files_struct {
    struct inode *pwd;           // 当前工作目录的 inode
    struct file *fd_array;       // 打开文件数组
    int files_count;             // 引用计数（用于 fork 时共享）
    semaphore_t files_sem;       // 互斥信号量
};
```

**关键点**：
- 每个进程有一个 `files_struct`
- `fd_array` 是一个数组，索引就是**文件描述符（fd）**
- fd 0, 1, 2 通常预留给 stdin, stdout, stderr
- `fork()` 时可以选择共享或复制 `files_struct`

**一句话带走**：`files_struct` 是进程的"文件管理簿"。

---

### 3.3 知识卡片：inode（VFS层）

---
**名称**：inode（VFS 层抽象）

**要解决的问题**：统一表示不同类型的文件（设备文件、SFS文件等）

**形式化表述**：
```c
// kern/fs/vfs/inode.h
struct inode {
    union {
        struct device __device_info;         // 设备文件信息
        struct sfs_inode __sfs_inode_info;   // SFS 文件信息
    } in_info;
    
    enum {
        inode_type_device_info = 0x1234,
        inode_type_sfs_inode_info,
    } in_type;                               // inode 类型
    
    int ref_count;                           // 引用计数
    int open_count;                          // 打开计数
    struct fs *in_fs;                        // 所属文件系统
    const struct inode_ops *in_ops;          // 操作函数表
};
```

**设计亮点**：
- 使用 **union** 来存储不同类型文件的信息，节省内存
- 使用 **in_type** 来标识当前是哪种类型
- 使用 **in_ops** 函数指针表实现多态（不同类型文件有不同的操作实现）

**类型判断宏**：
```c
#define check_inode_type(node, type)  ((node)->in_type == __in_type(type))
// 例如：check_inode_type(node, device) 检查是否是设备文件
```

**获取具体信息的宏**：
```c
#define vop_info(node, type)  __vop_info(node, type)
// 例如：vop_info(node, sfs_inode) 获取 SFS inode 信息
```

**一句话带走**：VFS 层的 inode 用 union 和函数指针实现了"多态"。

---

### 3.4 知识卡片：inode_ops

---
**名称**：inode_ops（索引节点操作表）

**要解决的问题**：定义 inode 可以执行的所有操作接口

**形式化表述**：
```c
// kern/fs/vfs/inode.h
struct inode_ops {
    unsigned long vop_magic;                            // 魔数验证
    int (*vop_open)(struct inode *node, uint32_t open_flags);
    int (*vop_close)(struct inode *node);
    int (*vop_read)(struct inode *node, struct iobuf *iob);
    int (*vop_write)(struct inode *node, struct iobuf *iob);
    int (*vop_fstat)(struct inode *node, struct stat *stat);
    int (*vop_fsync)(struct inode *node);
    int (*vop_namefile)(struct inode *node, struct iobuf *iob);
    int (*vop_getdirentry)(struct inode *node, struct iobuf *iob);
    int (*vop_reclaim)(struct inode *node);
    int (*vop_gettype)(struct inode *node, uint32_t *type_store);
    int (*vop_tryseek)(struct inode *node, off_t pos);
    int (*vop_truncate)(struct inode *node, off_t len);
    int (*vop_create)(struct inode *node, const char *name, 
                      bool excl, struct inode **node_store);
    int (*vop_lookup)(struct inode *node, char *path, 
                      struct inode **node_store);
    int (*vop_ioctl)(struct inode *node, int op, void *data);
};
```

**调用宏**：
```c
// 通过宏调用函数指针，更加直观
#define vop_open(node, flags)    ((node)->in_ops->vop_open(node, flags))
#define vop_read(node, iob)      ((node)->in_ops->vop_read(node, iob))
// ...
```

**不同文件类型的实现**：

| 函数 | SFS 普通文件 | SFS 目录 | 设备文件 |
|-----|-------------|---------|---------|
| vop_open | sfs_openfile | sfs_opendir | dev_open |
| vop_read | sfs_read | 返回错误 | dev_read |
| vop_write | sfs_write | 返回错误 | dev_write |
| vop_lookup | 返回错误 | sfs_lookup | dev_lookup |

**一句话带走**：`inode_ops` 通过函数指针表实现了接口的多态。

---

## 四、VFS 层详解 \[S04]

### 4.1 file 接口层

`kern/fs/file.c` 提供了文件操作的核心接口：

```c
// 主要函数
int file_open(char *path, uint32_t open_flags);   // 打开文件
int file_close(int fd);                            // 关闭文件
int file_read(int fd, void *base, size_t len, size_t *copied_store);
int file_write(int fd, void *base, size_t len, size_t *copied_store);
int file_seek(int fd, off_t pos, int whence);      // 移动读写位置
int file_fstat(int fd, struct stat *stat);         // 获取文件信息
```

### 4.2 VFS 操作层

`kern/fs/vfs/*.c` 提供了 VFS 层的操作：

| 文件 | 功能 |
|-----|------|
| `vfs.c` | VFS 初始化、根文件系统管理 |
| `vfsfile.c` | vfs_open, vfs_close 等文件操作 |
| `vfslookup.c` | vfs_lookup 路径查找 |
| `vfspath.c` | 路径字符串处理 |
| `vfsdev.c` | 设备链表管理 |
| `inode.c` | inode 创建、引用计数管理 |

---

## 五、SFS 层详解 \[S05]

### 5.1 SFS 磁盘布局

SFS 文件系统在磁盘上的布局如下：

```
┌─────────────┬─────────────────┬─────────────────┬────────────────────────┐
│   Block 0   │     Block 1     │   Block 2...N   │    Block N+1...       │
├─────────────┼─────────────────┼─────────────────┼────────────────────────┤
│  Superblock │  Root-dir Inode │     Freemap     │  Inode & Data Blocks  │
│  (超级块)    │   (根目录节点)   │   (空闲位图)    │   (数据和索引节点)     │
├─────────────┼─────────────────┼─────────────────┼────────────────────────┤
│   4KB       │      4KB        │    若干块        │      剩余空间          │
│  全局信息    │  根目录的inode  │  1 bit/block   │   实际文件内容          │
└─────────────┴─────────────────┴─────────────────┴────────────────────────┘
```
\[Fig·S05-1] SFS 磁盘布局

**关键常量定义**：
```c
// kern/fs/sfs/sfs.h
#define SFS_MAGIC           0x2f8dbe2a  // SFS 魔数
#define SFS_BLKSIZE         PGSIZE      // 块大小 = 页大小 = 4096 字节
#define SFS_NDIRECT         12          // 直接索引数量
#define SFS_BLKN_SUPER      0           // 超级块在第 0 块
#define SFS_BLKN_ROOT       1           // 根目录在第 1 块
#define SFS_BLKN_FREEMAP    2           // Freemap 从第 2 块开始
```

### 5.2 SFS 内存中的数据结构

```c
// kern/fs/sfs/sfs.h - 内存中的 SFS 文件系统结构
struct sfs_fs {
    struct sfs_super super;       // 超级块副本（从磁盘加载）
    struct device *dev;           // 挂载的设备
    struct bitmap *freemap;       // 空闲块位图（从磁盘加载）
    bool super_dirty;             // 超级块是否被修改
    void *sfs_buffer;             // IO 缓冲区
    semaphore_t fs_sem;           // 文件系统信号量
    semaphore_t io_sem;           // IO 信号量
    semaphore_t mutex_sem;        // 互斥信号量
    list_entry_t inode_list;      // inode 链表
    list_entry_t *hash_list;      // inode 哈希表（快速查找）
};
```

### 5.3 SFS inode（内存版本）

```c
// kern/fs/sfs/sfs.h - 内存中的 SFS inode
struct sfs_inode {
    struct sfs_disk_inode *din;   // 指向磁盘 inode 的副本
    uint32_t ino;                 // inode 编号（= 块号）
    bool dirty;                   // 是否被修改
    int reclaim_count;            // 回收计数
    semaphore_t sem;              // 互斥信号量
    list_entry_t inode_link;      // 在 inode_list 中的链接
    list_entry_t hash_link;       // 在 hash_list 中的链接
};
```

### 5.4 SFS inode 操作函数表

```c
// kern/fs/sfs/sfs_inode.c - 文件操作
static const struct inode_ops sfs_node_fileops = {
    .vop_magic      = VOP_MAGIC,
    .vop_open       = sfs_openfile,
    .vop_close      = sfs_close,
    .vop_read       = sfs_read,      // → sfs_io → sfs_io_nolock
    .vop_write      = sfs_write,     // → sfs_io → sfs_io_nolock
    .vop_fstat      = sfs_fstat,
    .vop_fsync      = sfs_fsync,
    .vop_reclaim    = sfs_reclaim,
    .vop_gettype    = sfs_gettype,
    .vop_tryseek    = sfs_tryseek,
    .vop_truncate   = sfs_truncfile,
    // ...
};

// 目录操作
static const struct inode_ops sfs_node_dirops = {
    .vop_magic      = VOP_MAGIC,
    .vop_open       = sfs_opendir,
    .vop_close      = sfs_close,
    .vop_getdirentry = sfs_getdirentry,
    .vop_lookup     = sfs_lookup,
    // ...
};
```

### 5.5 SFS 辅助函数说明

以下函数在 `sfs_inode.c` 中实现，对理解练习1很重要：

| 函数 | 作用 |
|-----|------|
| `sfs_bmap_load_nolock` | 获取文件第 index 个数据块的块号 |
| `sfs_bmap_get_nolock` | 底层实现：处理直接/间接索引 |
| `sfs_bmap_truncate_nolock` | 释放文件末尾的数据块 |
| `sfs_dirent_read_nolock` | 读取目录的第 slot 个目录项 |
| `sfs_dirent_search_nolock` | 在目录中搜索指定文件名 |

**重要提示**：`nolock` 后缀表示调用前必须已获得相应的信号量锁！

---

## 六、设备层详解 \[S06]

### 6.1 设备抽象

```c
// kern/fs/devs/dev.h
struct device {
    size_t d_blocks;                                    // 设备块数
    size_t d_blocksize;                                 // 块大小
    int (*d_open)(struct device *dev, uint32_t open_flags);
    int (*d_close)(struct device *dev);
    int (*d_io)(struct device *dev, struct iobuf *iob, bool write);
    int (*d_ioctl)(struct device *dev, int op, void *data);
};
```

### 6.2 设备链表

```c
// kern/fs/vfs/vfsdev.c
typedef struct {
    const char *devname;          // 设备名（如 "disk0", "stdin"）
    struct inode *devnode;        // 设备对应的 inode
    struct fs *fs;                // 关联的文件系统
    bool mountable;               // 是否可挂载
    list_entry_t vdev_link;       // 链表节点
} vfs_dev_t;

static list_entry_t vdev_list;    // 全局设备链表
static semaphore_t vdev_list_sem; // 互斥访问
```

### 6.3 三种设备的实现

| 设备 | 实现文件 | d_io 行为 |
|-----|---------|----------|
| stdin | dev_stdin.c | 从键盘缓冲区读取 |
| stdout | dev_stdout.c | 调用 cputchar 输出 |
| disk0 | dev_disk0.c | 读写 ramdisk |

---

## 七、易错点与常见问题 \[C07]

### 7.1 常见误区

| 误区 | 正确理解 |
|-----|---------|
| SFS 可以支持任意大文件 | ❌ 最大约 4MB（12直接 + 1间接） |
| inode 编号是任意的 | ❌ 在 SFS 中，inode 编号 = 块号 |
| 每个目录项可以很小 | ❌ 在 SFS 中，每个目录项占一个完整块 |
| sfs_io_nolock 可直接调用 | ❌ 必须先获得 sin->sem 信号量 |

### 7.2 函数命名规则

| 后缀 | 含义 |
|-----|------|
| `_nolock` | 调用前需已持有锁 |
| `vop_` | VFS 层的虚拟操作 |
| `sfs_` | SFS 层的具体实现 |
| `dev_` | 设备层操作 |

---

## 八、自测题 \[C08]

**Q1（单选题）**：在 SFS 中，一个文件最多可以使用多少个直接索引块？
- A) 10
- B) 12
- C) 16
- D) 1024

> **答案**：B) 12。由 `SFS_NDIRECT = 12` 定义。

**Q2（判断题）**：在 ucore 中，打开 stdin 和打开普通文件使用的是完全不同的系统调用。
> **答案**：❌ 错误。都使用 `open()` 系统调用，只是 VFS 层会根据 inode 类型调用不同的底层实现。

**Q3（开放题）**：解释为什么 ucore 使用 ramdisk 而不是真实硬盘？
> **答案要点**：
> 1. QEMU 模拟器中访问真实硬盘比较复杂
> 2. ramdisk 速度快，便于调试
> 3. 可以在启动前静态构建文件系统镜像
> 4. 原理与真实硬盘相同，只是数据存储在内存中

---

## 九、下一步

完成本部分后，你已理解了 ucore 文件系统的整体架构！接下来请阅读：

📖 **[04_SFS文件系统详解.md](04_SFS文件系统详解.md)** - 深入 Simple File System 的实现细节

---

**Covered**: S03（ucore文件系统架构）, S04（VFS层）, S05（SFS层）, S06（设备层）
