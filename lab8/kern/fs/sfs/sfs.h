/*
 * SFS (Simple File System) 是一种简单的文件系统，用于 ucore 操作系统。
 * 该文件定义了 SFS 的数据结构、常量和宏，包括磁盘上的布局（超级块、inode、目录项）
 * 以及内存中的表示。
 */

#ifndef __KERN_FS_SFS_SFS_H__
#define __KERN_FS_SFS_SFS_H__

#include <defs.h>
#include <mmu.h>
#include <list.h>
#include <sem.h>
#include <unistd.h>

/*
 * Simple FS (SFS) 定义，对 ucore 可见。这涵盖了磁盘上的格式，
 * 并被用于处理 SFS 卷的工具（如 mksfs）使用。
 */

#define SFS_MAGIC            0x2f8dbe2a              /* sfs 的魔数 */
#define SFS_BLKSIZE          PGSIZE                  /* 块的大小 */
#define SFS_NDIRECT          12                      /* inode 中直接块的数量 */
#define SFS_MAX_INFO_LEN     31                      /* 信息的最大长度 */
#define SFS_MAX_FNAME_LEN    FS_MAX_FNAME_LEN        /* 文件名的最大长度 */
#define SFS_MAX_FILE_SIZE    (1024UL * 1024 * 128)   /* 最大文件大小 (128M) */
#define SFS_BLKN_SUPER       0                       /* 超级块所在的块号 */
#define SFS_BLKN_ROOT        1                       /* 根目录 inode 的位置 */
#define SFS_BLKN_FREEMAP     2                       /* 空闲映射表的第一个块 */

/* 一个块中的位数 */
#define SFS_BLKBITS          (SFS_BLKSIZE * CHAR_BIT)

/* 一个块中的条目数 */
#define SFS_BLK_NENTRY       (SFS_BLKSIZE / sizeof(uint32_t))

/* 文件类型 */
#define SFS_TYPE_INVAL       0       /* 不应出现在磁盘上 */
#define SFS_TYPE_FILE        1
#define SFS_TYPE_DIR         2
#define SFS_TYPE_LINK        3

/*
 * 磁盘上的超级块，位于磁盘第0个块
 */
struct sfs_super {
    uint32_t magic;                    /* 魔数，应为 SFS_MAGIC，用于验证文件系统类型 */
    uint32_t blocks;                   /* 文件系统中的总块数 */
    uint32_t unused_blocks;            /* 文件系统中的未使用块数 */
    char info[SFS_MAX_INFO_LEN + 1];   /* 文件系统的描述信息 */
};

/* 索引节点：inode (磁盘上) ，记录一个文件的所有元数据，注意不记录文件名*/
struct sfs_disk_inode {
    uint32_t size;                 /* 文件大小 (字节) */
    uint16_t type;                 /* 文件类型，上述 SYS_TYPE_* 之一 */
    uint16_t nlinks;               /* 指向此文件的硬链接数 */
    uint32_t blocks;               /* 占用的数据块数 */
    uint32_t direct[SFS_NDIRECT];  /* 直接索引（12个） */
    uint32_t indirect;             /* 一级间接索引 */
//    uint32_t db_indirect;        /* 双重间接块 */
//   unused
};

/* 目录项的文件条目 (磁盘上)，目录本身也是一个文件（类型为 SFS_TYPE_DIR） */
struct sfs_disk_entry {
    uint32_t ino;                       /* 指向的inode 编号 */
    char name[SFS_MAX_FNAME_LEN + 1];   /* 文件名 */
};

#define sfs_dentry_size                             \
    sizeof(((struct sfs_disk_entry *)0)->name)

/* sfs 的 inode */
struct sfs_inode {
    struct sfs_disk_inode *din;                     /* 磁盘上的 inode */
    uint32_t ino;                                   /* inode 编号 */
    bool dirty;                                     /* 如果 inode 被修改则为 true */
    int reclaim_count;                              /* 如果减为零则回收 inode */
    semaphore_t sem;                                /* din 的信号量 */
    list_entry_t inode_link;                        /* sfs_fs 中链表的条目 */
    list_entry_t hash_link;                         /* sfs_fs 中哈希链表的条目 */
};

#define le2sin(le, member)                          \
    to_struct((le), struct sfs_inode, member)

/* sfs 文件系统 */
struct sfs_fs {
    struct sfs_super super;                         /* 磁盘上的超级块 */
    struct device *dev;                             /* 挂载的设备 */
    struct bitmap *freemap;                         /* 使用中的块标记为 0 */
    bool super_dirty;                               /* 如果超级块/空闲映射表被修改则为 true */
    void *sfs_buffer;                               /* 用于非块对齐 IO 的缓冲区 */
    semaphore_t fs_sem;                             /* fs 的信号量 */
    semaphore_t io_sem;                             /* io 的信号量 */
    semaphore_t mutex_sem;                          /* 用于 link/unlink 和 rename 的信号量 */
    list_entry_t inode_list;                        /* inode 链表 */
    list_entry_t *hash_list;                        /* inode 哈希链表 */
};

/* sfs 的哈希 */
#define SFS_HLIST_SHIFT                             10
#define SFS_HLIST_SIZE                              (1 << SFS_HLIST_SHIFT)
#define sin_hashfn(x)                               (hash32(x, SFS_HLIST_SHIFT))

/* 空闲映射表的大小 (以位为单位) */
#define sfs_freemap_bits(super)                     ROUNDUP((super)->blocks, SFS_BLKBITS)

/* 空闲映射表的大小 (以块为单位) */
#define sfs_freemap_blocks(super)                   ROUNDUP_DIV((super)->blocks, SFS_BLKBITS)

struct fs;
struct inode;

void sfs_init(void);
int sfs_mount(const char *devname);

void lock_sfs_fs(struct sfs_fs *sfs);
void lock_sfs_io(struct sfs_fs *sfs);
void unlock_sfs_fs(struct sfs_fs *sfs);
void unlock_sfs_io(struct sfs_fs *sfs);

int sfs_rblock(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks);
int sfs_wblock(struct sfs_fs *sfs, void *buf, uint32_t blkno, uint32_t nblks);
int sfs_rbuf(struct sfs_fs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset);
int sfs_wbuf(struct sfs_fs *sfs, void *buf, size_t len, uint32_t blkno, off_t offset);
int sfs_sync_super(struct sfs_fs *sfs);
int sfs_sync_freemap(struct sfs_fs *sfs);
int sfs_clear_block(struct sfs_fs *sfs, uint32_t blkno, uint32_t nblks);

int sfs_load_inode(struct sfs_fs *sfs, struct inode **node_store, uint32_t ino);

#endif /* !__KERN_FS_SFS_SFS_H__ */

