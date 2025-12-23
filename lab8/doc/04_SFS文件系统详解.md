# Lab8 文件系统实验讲义 - 第四部分：SFS 文件系统详解

> **前置知识**：ucore 文件系统架构（第三部分）  
> **学习产出**：深入理解 SFS 的索引结构、目录组织、关键函数

---

## 一、SFS 磁盘布局详解 \[S05]

### 1.1 整体布局

```
┌────────────────────────────────────────────────────────────────────────────┐
│                           SFS 磁盘布局                                     │
├──────────┬──────────┬──────────────────┬──────────────────────────────────┤
│  Block 0 │  Block 1 │  Block 2 ~ N    │  Block N+1 ~ ...                 │
├──────────┼──────────┼──────────────────┼──────────────────────────────────┤
│ Super-   │ Root-dir │    Freemap       │  Inodes & Data Blocks            │
│ block    │  Inode   │  (空闲块位图)     │  (索引节点和数据块)               │
├──────────┼──────────┼──────────────────┼──────────────────────────────────┤
│  4KB     │   4KB    │ ceil(总块数/     │        剩余空间                   │
│          │          │     32768) 块    │                                  │
└──────────┴──────────┴──────────────────┴──────────────────────────────────┘
```
\[Fig·S05-1] SFS 磁盘布局详图

### 1.2 Superblock（超级块）详解

```c
// kern/fs/sfs/sfs.h
struct sfs_super {
    uint32_t magic;                         // 魔数: 0x2f8dbe2a
    uint32_t blocks;                        // 总块数
    uint32_t unused_blocks;                 // 未使用块数
    char info[SFS_MAX_INFO_LEN + 1];        // 描述信息: "simple file system"
};
```

**加载过程**：在 `sfs_do_mount()` 函数中，从磁盘读取 Block 0，解析为 `sfs_super` 结构。

### 1.3 Freemap（空闲块位图）详解

Freemap 用于追踪哪些块是空闲的、哪些已被使用。

**设计原理**：
- 每个块用 1 bit 表示状态：0 = 空闲，1 = 已使用
- 每个块 4KB = 32768 bits，可以表示 32768 个块的状态

```
假设总共有 10000 个块：
- Freemap 需要 10000 bits = 1250 bytes
- 1 个 Freemap 块（4KB）就够了

假设总共有 100000 个块：
- Freemap 需要 100000 bits = 12500 bytes ≈ 3.05 个块
- 需要 4 个 Freemap 块
```

**相关函数**（`kern/fs/sfs/bitmap.c`）：
```c
bitmap_alloc(bitmap, ino_store)  // 分配一个空闲块
bitmap_free(bitmap, ino)         // 释放一个块
bitmap_test(bitmap, ino)         // 测试块是否已使用
```

---

## 二、磁盘索引节点（sfs_disk_inode）详解 \[S05]

### 2.1 结构定义

```c
// kern/fs/sfs/sfs.h
struct sfs_disk_inode {
    uint32_t size;                        // 文件大小（字节）
    uint16_t type;                        // 类型：文件/目录/链接
    uint16_t nlinks;                      // 硬链接数
    uint32_t blocks;                      // 已使用的数据块数
    uint32_t direct[SFS_NDIRECT];         // 12 个直接索引
    uint32_t indirect;                    // 1 级间接索引
};
```

### 2.2 文件类型

```c
#define SFS_TYPE_INVAL  0   // 无效（不应出现在磁盘上）
#define SFS_TYPE_FILE   1   // 普通文件
#define SFS_TYPE_DIR    2   // 目录
#define SFS_TYPE_LINK   3   // 符号链接
```

### 2.3 索引结构详解

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        sfs_disk_inode                                   │
├─────────────────────────────────────────────────────────────────────────┤
│  size: 50000 (文件大小)                                                 │
│  type: SFS_TYPE_FILE                                                   │
│  nlinks: 1                                                             │
│  blocks: 15 (已使用 15 个数据块)                                        │
├─────────────────────────────────────────────────────────────────────────┤
│  直接索引 (每个指向一个 4KB 数据块)                                      │
│  ┌─────────┬─────────┬─────────┬─────────┬─────────┬─────────┐        │
│  │direct[0]│direct[1]│direct[2]│  ...    │direct[10│direct[11│        │
│  │  → #101 │  → #102 │  → #103 │         │  → #111 │  → #112 │        │
│  └────┬────┴────┬────┴────┬────┴─────────┴────┬────┴────┬────┘        │
│       │         │         │                   │         │              │
│       ▼         ▼         ▼                   ▼         ▼              │
│    ┌─────┐   ┌─────┐   ┌─────┐            ┌─────┐   ┌─────┐           │
│    │Data │   │Data │   │Data │    ...     │Data │   │Data │           │
│    │Block│   │Block│   │Block│            │Block│   │Block│           │
│    │#101 │   │#102 │   │#103 │            │#111 │   │#112 │           │
│    │4KB  │   │4KB  │   │4KB  │            │4KB  │   │4KB  │           │
│    └─────┘   └─────┘   └─────┘            └─────┘   └─────┘           │
├─────────────────────────────────────────────────────────────────────────┤
│  一级间接索引                                                           │
│  ┌──────────┐                                                          │
│  │ indirect │ → #200 (间接块)                                          │
│  └────┬─────┘                                                          │
│       │                                                                │
│       ▼                                                                │
│  ┌──────────────────────────────────────────────────────────┐         │
│  │                    间接块 #200                            │         │
│  │  ┌──────┬──────┬──────┬──────┬──────┬─────────────────┐ │         │
│  │  │ [0]  │ [1]  │ [2]  │ ...  │[1023]│                 │ │         │
│  │  │→#301 │→#302 │→#303 │      │→#1324│   (1024个指针)  │ │         │
│  │  └──┬───┴──┬───┴──┬───┴──────┴──┬───┴─────────────────┘ │         │
│  │     ▼      ▼      ▼             ▼                       │         │
│  │  ┌─────┐┌─────┐┌─────┐      ┌─────┐                    │         │
│  │  │#301 ││#302 ││#303 │ ...  │#1324│                    │         │
│  │  │4KB  ││4KB  ││4KB  │      │4KB  │                    │         │
│  │  └─────┘└─────┘└─────┘      └─────┘                    │         │
│  └──────────────────────────────────────────────────────────┘         │
└─────────────────────────────────────────────────────────────────────────┘
```
\[Fig·S05-2] SFS 索引结构：12 个直接索引 + 1 个一级间接索引

### 2.4 文件大小计算

| 索引类型 | 数量 | 可寻址块数 | 可寻址大小 |
|---------|------|-----------|-----------|
| 直接索引 | 12 个 | 12 块 | 12 × 4KB = **48KB** |
| 一级间接索引 | 1 个 | 1024 块 | 1024 × 4KB = **4MB** |
| **总计** | - | 1036 块 | **48KB + 4MB ≈ 4.05MB** |

**计算过程**：
```
一个间接块可存储的指针数 = 块大小 / 指针大小
                        = 4096 / 4 = 1024 个

最大文件大小 = 直接索引容量 + 间接索引容量
            = 12 × 4KB + 1024 × 4KB
            = 48KB + 4096KB
            ≈ 4.05MB
```

### 2.5 特殊值的含义

- **块号 = 0**：表示无效/未分配（因为 Block 0 是超级块，不可能被文件使用）
- **indirect = 0**：表示不使用间接索引
- **entry->ino = 0**：目录项无效（已删除）

---

## 三、目录项（sfs_disk_entry）详解 \[S05]

### 3.1 结构定义

```c
// kern/fs/sfs/sfs.h
struct sfs_disk_entry {
    uint32_t ino;                             // inode 编号（= 块号）
    char name[SFS_MAX_FNAME_LEN + 1];         // 文件名
};

#define SFS_MAX_FNAME_LEN  255  // 最大文件名长度
```

### 3.2 目录结构

在 SFS 中：
- 目录本身也是一个文件（type = SFS_TYPE_DIR）
- 目录的数据内容是一个**目录项数组**
- **每个目录项占用一个完整的块**（简化实现）

```
目录 "/" (根目录) 的结构：
┌────────────────────────────────────────────────────────────────┐
│  Root Directory Inode (Block #1)                               │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │  size: 4096 × 3 = 12288                                  │ │
│  │  type: SFS_TYPE_DIR                                      │ │
│  │  blocks: 3                                               │ │
│  │  direct[0] → Block #10 (entry for "bin")                │ │
│  │  direct[1] → Block #11 (entry for "home")               │ │
│  │  direct[2] → Block #12 (entry for "hello")              │ │
│  └──────────────────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────────────────┘
                │              │              │
                ▼              ▼              ▼
        ┌───────────┐  ┌───────────┐  ┌───────────┐
        │ Block #10 │  │ Block #11 │  │ Block #12 │
        │ ino: 20   │  │ ino: 25   │  │ ino: 30   │
        │ name:"bin"│  │name:"home"│  │name:"hello│
        └───────────┘  └───────────┘  └───────────┘
                │              │              │
                ▼              ▼              ▼
          指向 bin       指向 home      指向 hello
          目录的inode    目录的inode    文件的inode
          (Block #20)   (Block #25)   (Block #30)
```
\[Fig·S05-3] 目录结构示例

### 3.3 目录项的空闲管理

- **删除文件**：不是物理删除目录项，而是将 `entry->ino` 设为 0
- **添加文件**：优先使用 ino=0 的空闲项，否则追加新项

```
删除前：                        删除 "bob" 后：
┌───────────────────┐          ┌───────────────────┐
│ ino=10, "alice"   │          │ ino=10, "alice"   │
├───────────────────┤          ├───────────────────┤
│ ino=11, "bob"     │  ──────► │ ino=0,  "bob"     │ ← 标记为空闲
├───────────────────┤          ├───────────────────┤
│ ino=12, "carol"   │          │ ino=12, "carol"   │
└───────────────────┘          └───────────────────┘
```

---

## 四、SFS 关键函数解析 \[S05]

### 4.1 块索引函数族

这组函数负责将**文件内的逻辑块号**映射到**磁盘上的物理块号**：

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        块索引函数调用链                                  │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  sfs_bmap_load_nolock(sfs, sin, index, &ino)                           │
│       │                                                                 │
│       │ 作用：获取文件第 index 个块的磁盘块号                           │
│       │       如果 index == blocks，则分配新块                          │
│       ▼                                                                 │
│  sfs_bmap_get_nolock(sfs, sin, index, create, &ino)                    │
│       │                                                                 │
│       │ 作用：实际执行索引查找或分配                                    │
│       │                                                                 │
│       ├──► index < 12 (直接索引)                                       │
│       │       返回 din->direct[index]                                   │
│       │       如果 create 且为 0，则分配新块                            │
│       │                                                                 │
│       └──► index >= 12 (间接索引)                                      │
│               │                                                         │
│               ▼                                                         │
│          sfs_bmap_get_sub_nolock(sfs, &ent, index-12, create, &ino)    │
│               │                                                         │
│               │ 作用：在间接块中查找/分配                               │
│               │                                                         │
│               └──► 读取间接块的第 (index-12) 项                         │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```
\[Fig·S05-4] 块索引函数调用链

### 4.2 sfs_bmap_load_nolock 详解

```c
// kern/fs/sfs/sfs_inode.c
static int
sfs_bmap_load_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, 
                     uint32_t index, uint32_t *ino_store) {
    struct sfs_disk_inode *din = sin->din;
    
    // 参数校验：index 不能超过当前块数太多
    assert(index <= din->blocks);
    
    int ret;
    uint32_t ino;
    
    // 如果 index == blocks，表示需要为文件增长一个块
    bool create = (index == din->blocks);
    
    // 调用底层函数获取块号
    if ((ret = sfs_bmap_get_nolock(sfs, sin, index, create, &ino)) != 0) {
        return ret;
    }
    
    assert(sfs_block_inuse(sfs, ino));
    
    // 如果分配了新块，更新 blocks 计数
    if (create) {
        din->blocks++;
    }
    
    if (ino_store != NULL) {
        *ino_store = ino;
    }
    return 0;
}
```

**使用示例**：
```c
uint32_t blkno;
// 获取文件的第 5 个数据块的物理块号
sfs_bmap_load_nolock(sfs, sin, 5, &blkno);
// 现在可以用 blkno 读取实际数据了
```

### 4.3 sfs_bmap_get_nolock 详解

```c
static int
sfs_bmap_get_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, 
                    uint32_t index, bool create, uint32_t *ino_store) {
    struct sfs_disk_inode *din = sin->din;
    int ret;
    uint32_t ent, ino;
    
    // 情况1：直接索引（index < 12）
    if (index < SFS_NDIRECT) {
        if ((ino = din->direct[index]) == 0 && create) {
            // 需要分配新块
            if ((ret = sfs_block_alloc(sfs, &ino)) != 0) {
                return ret;
            }
            din->direct[index] = ino;
            sin->dirty = 1;  // 标记 inode 已修改
        }
        goto out;
    }
    
    // 情况2：一级间接索引（index >= 12）
    index -= SFS_NDIRECT;  // 调整索引：从 0 开始
    if (index < SFS_BLK_NENTRY) {  // SFS_BLK_NENTRY = 1024
        ent = din->indirect;
        if ((ret = sfs_bmap_get_sub_nolock(sfs, &ent, index, create, &ino)) != 0) {
            return ret;
        }
        if (ent != din->indirect) {
            // 间接块刚刚被分配
            din->indirect = ent;
            sin->dirty = 1;
        }
        goto out;
    } else {
        // 超出支持范围
        panic("sfs_bmap_get_nolock - index out of range");
    }
    
out:
    assert(ino == 0 || sfs_block_inuse(sfs, ino));
    *ino_store = ino;
    return 0;
}
```

### 4.4 目录操作函数

```c
// 读取目录的第 slot 个目录项
static int
sfs_dirent_read_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, 
                       int slot, struct sfs_disk_entry *entry) {
    // 确保是目录且 slot 合法
    assert(sin->din->type == SFS_TYPE_DIR);
    assert(slot >= 0 && slot < sin->din->blocks);
    
    uint32_t ino;
    int ret;
    
    // 获取第 slot 项对应的块号
    if ((ret = sfs_bmap_load_nolock(sfs, sin, slot, &ino)) != 0) {
        return ret;
    }
    
    // 读取该块的内容到 entry
    if ((ret = sfs_rbuf(sfs, entry, sizeof(struct sfs_disk_entry), ino, 0)) != 0) {
        return ret;
    }
    
    entry->name[SFS_MAX_FNAME_LEN] = '\0';  // 确保字符串结尾
    return 0;
}
```

```c
// 在目录中搜索指定文件名
static int
sfs_dirent_search_nolock(struct sfs_fs *sfs, struct sfs_inode *sin,
                         const char *name, uint32_t *ino_store, 
                         int *slot, int *empty_slot) {
    struct sfs_disk_entry *entry = kmalloc(sizeof(struct sfs_disk_entry));
    
    int nslots = sin->din->blocks;  // 目录项数量
    
    // 遍历所有目录项
    for (int i = 0; i < nslots; i++) {
        sfs_dirent_read_nolock(sfs, sin, i, entry);
        
        if (entry->ino == 0) {
            // 空闲项，记录位置以便后续插入
            *empty_slot = i;
            continue;
        }
        
        if (strcmp(name, entry->name) == 0) {
            // 找到了！
            *slot = i;
            *ino_store = entry->ino;
            kfree(entry);
            return 0;
        }
    }
    
    kfree(entry);
    return -E_NOENT;  // 未找到
}
```

---

## 五、SFS IO 操作 \[S05]

### 5.1 IO 函数层次

```
用户空间：read(fd, buf, len)
     │
     ▼
系统调用：sysfile_read()
     │
     ▼
文件层：file_read() → vop_read()
     │
     ▼
SFS 层：sfs_read() → sfs_io()
     │
     ▼
核心函数：sfs_io_nolock()  ← 【练习1 在此实现】
     │
     ├──► sfs_bmap_load_nolock()  获取块号
     │
     ├──► sfs_rbuf() / sfs_rblock()  读取数据
     │
     └──► sfs_wbuf() / sfs_wblock()  写入数据
```
\[Fig·S05-5] SFS IO 函数调用层次

### 5.2 底层读写函数

```c
// 读取一个完整块（4KB）
int sfs_rblock(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks);

// 读取块的一部分
int sfs_rbuf(struct sfs_fs *sfs, void *buf, size_t len, 
             uint32_t blkno, off_t offset);

// 写入一个完整块
int sfs_wblock(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks);

// 写入块的一部分
int sfs_wbuf(struct sfs_fs *sfs, void *buf, size_t len, 
             uint32_t blkno, off_t offset);
```

---

## 六、sfs_io_nolock 函数框架 \[S05]

这是**练习1**需要完成的核心函数：

```c
static int
sfs_io_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, void *buf, 
              off_t offset, size_t *alenp, bool write) {
    struct sfs_disk_inode *din = sin->din;
    assert(din->type != SFS_TYPE_DIR);  // 不能对目录使用此函数
    
    off_t endpos = offset + *alenp;  // 读写结束位置
    *alenp = 0;
    
    // 边界检查
    if (offset < 0 || offset >= SFS_MAX_FILE_SIZE || offset > endpos) {
        return -E_INVAL;
    }
    if (offset == endpos) {
        return 0;
    }
    if (endpos > SFS_MAX_FILE_SIZE) {
        endpos = SFS_MAX_FILE_SIZE;
    }
    
    // 对于读操作，不能超过文件当前大小
    if (!write) {
        if (offset >= din->size) {
            return 0;
        }
        if (endpos > din->size) {
            endpos = din->size;
        }
    }
    
    // 选择读/写操作函数
    int (*sfs_buf_op)(struct sfs_fs*, void*, size_t, uint32_t, off_t);
    int (*sfs_block_op)(struct sfs_fs*, void*, uint32_t, uint32_t);
    if (write) {
        sfs_buf_op = sfs_wbuf;
        sfs_block_op = sfs_wblock;
    } else {
        sfs_buf_op = sfs_rbuf;
        sfs_block_op = sfs_rblock;
    }
    
    int ret = 0;
    size_t size, alen = 0;
    uint32_t ino;
    uint32_t blkno = offset / SFS_BLKSIZE;           // 起始块号
    uint32_t nblks = endpos / SFS_BLKSIZE - blkno;   // 需要处理的块数
    
    // ===========================================
    // LAB8: 练习1 - 在此实现读写逻辑
    // ===========================================
    
out:
    *alenp = alen;
    if (offset + alen > sin->din->size) {
        sin->din->size = offset + alen;
        sin->dirty = 1;
    }
    return ret;
}
```

---

## 七、易错点与注意事项 \[C09]

### 7.1 常见误区

| 误区 | 正确理解 |
|-----|---------|
| inode 编号是连续分配的 | ❌ 在 SFS 中，inode 编号 = 块号，可能不连续 |
| 目录项紧密排列 | ❌ 每个目录项占一个完整块（4KB） |
| sfs_io_nolock 处理目录 | ❌ 专门用于普通文件，目录有专门的函数 |
| 间接索引可以无限嵌套 | ❌ SFS 只支持一级间接索引 |

### 7.2 锁的使用

- 所有 `_nolock` 函数调用前必须持有 `sin->sem` 信号量
- `sfs_io` 函数会自动加锁再调用 `sfs_io_nolock`

```c
static inline int
sfs_io(struct inode *node, struct iobuf *iob, bool write) {
    // ...
    lock_sin(sin);       // 加锁
    {
        ret = sfs_io_nolock(sfs, sin, ...);
    }
    unlock_sin(sin);     // 解锁
    return ret;
}
```

### 7.3 dirty 标记

当修改 inode 数据时，必须设置 `dirty = 1`：
```c
din->direct[index] = ino;
sin->dirty = 1;  // 不要忘记！
```

这样在 `sfs_close` 时会将修改写回磁盘。

---

## 八、自测题 \[C10]

**Q1（填空题）**：SFS 支持的最大文件大小约为 ______ MB。
> **答案**：约 4 MB（精确值：12 × 4KB + 1024 × 4KB = 4144KB ≈ 4.05MB）

**Q2（判断题）**：在 SFS 中，如果一个文件只有 10KB，也会使用间接索引。
> **答案**：❌ 错误。10KB 只需要 3 个块，完全可以用直接索引（12个）容纳，不需要间接索引。

**Q3（代码题）**：以下代码有什么问题？
```c
sfs_bmap_load_nolock(sfs, sin, 5, &blkno);  // 没有先加锁！
```
> **答案**：调用 `_nolock` 函数前必须先获取 `sin->sem` 信号量。正确做法：
> ```c
> lock_sin(sin);
> sfs_bmap_load_nolock(sfs, sin, 5, &blkno);
> unlock_sin(sin);
> ```

---

## 九、下一步

完成本部分后，你已深入理解了 SFS 的内部实现！接下来请阅读：

📖 **[05_设备与系统调用.md](05_设备与系统调用.md)** - 了解设备文件和系统调用流程

---

**Covered**: S05（SFS 磁盘布局、索引结构、目录项、关键函数）
