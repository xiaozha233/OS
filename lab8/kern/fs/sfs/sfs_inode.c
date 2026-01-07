#include <defs.h>
#include <string.h>
#include <stdlib.h>
#include <list.h>
#include <stat.h>
#include <kmalloc.h>
#include <vfs.h>
#include <dev.h>
#include <sfs.h>
#include <inode.h>
#include <iobuf.h>
#include <bitmap.h>
#include <error.h>
#include <assert.h>

static const struct inode_ops sfs_node_dirops;  // dir operations
static const struct inode_ops sfs_node_fileops; // file operations

/*
 * lock_sin - lock the process of inode Rd/Wr
 * 锁定 inode 的读写操作，通过信号量实现互斥访问
 */
static void
lock_sin(struct sfs_inode *sin) {
    // 获取信号量，进入临界区
    down(&(sin->sem));
}

/*
 * unlock_sin - unlock the process of inode Rd/Wr
 * 解锁 inode 的读写操作
 */
static void
unlock_sin(struct sfs_inode *sin) {
    // 释放信号量，退出临界区
    up(&(sin->sem));
}

/*
 * sfs_get_ops - return function addr of fs_node_dirops/sfs_node_fileops
 * 根据文件类型获取对应的 inode 操作函数表指针
 */
static const struct inode_ops *
sfs_get_ops(uint16_t type) {
    switch (type) {
    case SFS_TYPE_DIR:
        // 如果是目录类型，返回目录操作函数集
        return &sfs_node_dirops;
    case SFS_TYPE_FILE:
        // 如果是普通文件类型，返回文件操作函数集
        return &sfs_node_fileops;
    }
    // 如果遇到非法文件类型，触发系统 panic
    panic("invalid file type %d.\n", type);
}

/*
 * sfs_hash_list - return inode entry in sfs->hash_list
 * 根据 inode 编号获取哈希链表中的对应项
 */
static list_entry_t *
sfs_hash_list(struct sfs_fs *sfs, uint32_t ino) {
    // 使用 inode 编号计算哈希值，返回对应的链表头指针
    return sfs->hash_list + sin_hashfn(ino);
}

/*
 * sfs_set_links - link inode sin in sfs->linked-list AND sfs->hash_link
 * 将 inode 插入到 SFS 的 inode 链表和哈希链表中
 */
static void
sfs_set_links(struct sfs_fs *sfs, struct sfs_inode *sin) {
    // 将 inode 节点加入到 SFS 的全局 inode 链表
    list_add(&(sfs->inode_list), &(sin->inode_link));
    // 将 inode 节点加入到 SFS 的哈希链表，方便快速查找
    list_add(sfs_hash_list(sfs, sin->ino), &(sin->hash_link));
}

/*
 * sfs_remove_links - unlink inode sin in sfs->linked-list AND sfs->hash_link
 * 将 inode 从 SFS 的 inode 链表和哈希链表中移除
 */
static void
sfs_remove_links(struct sfs_inode *sin) {
    // 从全局 inode 链表中删除该节点
    list_del(&(sin->inode_link));
    // 从哈希链表中删除该节点
    list_del(&(sin->hash_link));
}

/*
 * sfs_block_inuse - check the inode with NO. ino inuse info in bitmap
 * 检查指定编号的磁盘块是否已被占用
 */
static bool
sfs_block_inuse(struct sfs_fs *sfs, uint32_t ino) {
    // 检查块号是否合法（不为0且小于总块数）
    if (ino != 0 && ino < sfs->super.blocks) {
        // 通过位图检查该块是否已被占用，bitmap_test 返回 true 表示被占用
        return !bitmap_test(sfs->freemap, ino);
    }
    // 如果块号越界，触发 panic
    panic("sfs_block_inuse: called out of range (0, %u) %u.\n", sfs->super.blocks, ino);
}

/*
 * sfs_block_alloc -  check and get a free disk block
 * 分配一个新的空闲磁盘块
 */
static int
sfs_block_alloc(struct sfs_fs *sfs, uint32_t *ino_store) {
    int ret;
    // 在位图中查找一个空闲位，并标记为已占用，返回块号
    if ((ret = bitmap_alloc(sfs->freemap, ino_store)) != 0) {
        return ret;
    }
    // 确保还有空闲块
    assert(sfs->super.unused_blocks > 0);
    // 更新超级块中的空闲块计数，并标记超级块为脏
    sfs->super.unused_blocks --, sfs->super_dirty = 1;
    // 确保分配的块确实被标记为占用
    assert(sfs_block_inuse(sfs, *ino_store));
    // 将新分配的块内容清零，并写入磁盘
    return sfs_clear_block(sfs, *ino_store, 1);
}

/*
 * sfs_block_free - set related bits for ino block to 1(means free) in bitmap, add sfs->super.unused_blocks, set superblock dirty *
 * 释放一个磁盘块
 */
static void
sfs_block_free(struct sfs_fs *sfs, uint32_t ino) {
    // 确保要释放的块当前是被占用的
    assert(sfs_block_inuse(sfs, ino));
    // 在位图中将该块标记为空闲
    bitmap_free(sfs->freemap, ino);
    // 更新超级块中的空闲块计数，并标记超级块为脏
    sfs->super.unused_blocks ++, sfs->super_dirty = 1;
}

/*
 * sfs_create_inode - alloc a inode in memroy, and init din/ino/dirty/reclian_count/sem fields in sfs_inode in inode
 * 在内存中创建一个新的 inode，并初始化相关字段
 */
static int
sfs_create_inode(struct sfs_fs *sfs, struct sfs_disk_inode *din, uint32_t ino, struct inode **node_store) {
    struct inode *node;
    // 分配一个新的 inode 结构体（包含 sfs_inode）
    if ((node = alloc_inode(sfs_inode)) != NULL) {
        // 初始化 VFS inode 层面的信息（操作函数表、文件系统指针）
        vop_init(node, sfs_get_ops(din->type), info2fs(sfs, sfs));
        // 获取 sfs_inode 指针
        struct sfs_inode *sin = vop_info(node, sfs_inode);
        // 初始化 sfs_inode 的字段：磁盘 inode 数据、块号、脏标志、回收计数
        sin->din = din, sin->ino = ino, sin->dirty = 0, sin->reclaim_count = 1;
        // 初始化信号量
        sem_init(&(sin->sem), 1);
        *node_store = node;
        return 0;
    }
    return -E_NO_MEM;
}

/*
 * lookup_sfs_nolock - according ino, find related inode
 * 根据 inode 编号查找内存中已存在的 inode，无需加锁
 *
 * NOTICE: le2sin, info2node MACRO
 */
static struct inode *
lookup_sfs_nolock(struct sfs_fs *sfs, uint32_t ino) {
    struct inode *node;
    // 根据块号获取哈希链表头，并遍历链表
    list_entry_t *list = sfs_hash_list(sfs, ino), *le = list;
    while ((le = list_next(le)) != list) {
        // 获取链表项对应的 sfs_inode
        struct sfs_inode *sin = le2sin(le, hash_link);
        // 如果找到匹配的 ino
        if (sin->ino == ino) {
            // 获取对应的通用 inode
            node = info2node(sin, sfs_inode);
            // 尝试增加引用计数，如果是第一次引用（从0变为1），则增加回收计数
            if (vop_ref_inc(node) == 1) {
                sin->reclaim_count ++;
            }
            return node;
        }
    }
    return NULL;
}

/*
 * sfs_load_inode - If the inode isn't existed, load inode related ino disk block data into a new created inode.
 *                  If the inode is in memory alreadily, then do nothing
 * 加载指定编号的 inode 到内存中。如果已存在则直接返回，否则从磁盘读取
 */
int
sfs_load_inode(struct sfs_fs *sfs, struct inode **node_store, uint32_t ino) {
    // 锁定文件系统，防止并发修改
    lock_sfs_fs(sfs);
    struct inode *node;
    // 首先在内存哈希表中查找是否已存在该 inode
    if ((node = lookup_sfs_nolock(sfs, ino)) != NULL) {
        // 如果已存在，直接跳转到解锁退出
        goto out_unlock;
    }

    int ret = -E_NO_MEM;
    struct sfs_disk_inode *din;
    // 分配内存用于存储磁盘 inode 数据
    if ((din = kmalloc(sizeof(struct sfs_disk_inode))) == NULL) {
        goto failed_unlock;
    }

    // 确保该 inode 对应的块是被占用的
    assert(sfs_block_inuse(sfs, ino));
    // 从磁盘读取 inode 数据到内存
    if ((ret = sfs_rbuf(sfs, din, sizeof(struct sfs_disk_inode), ino, 0)) != 0) {
        goto failed_cleanup_din;
    }

    // 检查读取到的 inode 数据的链接数是否合法
    assert(din->nlinks != 0);
    // 创建一个新的 inode 结构并初始化
    if ((ret = sfs_create_inode(sfs, din, ino, &node)) != 0) {
        goto failed_cleanup_din;
    }
    // 将新创建的 inode 加入到链表和哈希表中管理
    sfs_set_links(sfs, vop_info(node, sfs_inode));

out_unlock:
    // 解锁文件系统
    unlock_sfs_fs(sfs);
    *node_store = node;
    return 0;

failed_cleanup_din:
    kfree(din);
failed_unlock:
    unlock_sfs_fs(sfs);
    return ret;
}

/*
 * sfs_bmap_get_sub_nolock - according entry pointer entp and index, find the index of indrect disk block
 *                           return the index of indrect disk block to ino_store. no lock protect
 * 处理间接块索引，获取或分配下一级块的编号
 * @sfs:      sfs 文件系统结构
 * @entp:     指向入口块号的指针（例如一级间接块的入口）
 * @index:    在间接块中的索引
 * @create:   是否创建新块
 * @ino_store: 返回获取到的块号
 */
static int
sfs_bmap_get_sub_nolock(struct sfs_fs *sfs, uint32_t *entp, uint32_t index, bool create, uint32_t *ino_store) {
    assert(index < SFS_BLK_NENTRY);
    int ret;
    uint32_t ent, ino = 0;
    // 计算在间接块中的字节偏移量
    off_t offset = index * sizeof(uint32_t);  
    // 如果入口块号存在，读取其中的内容（即下一级块号）
    if ((ent = *entp) != 0) {
        if ((ret = sfs_rbuf(sfs, &ino, sizeof(uint32_t), ent, offset)) != 0) {
            return ret;
        }
        // 如果读取到了非零块号，或者不需要创建新块，则直接返回结果
        if (ino != 0 || !create) {
            goto out;
        }
    }
    else {
        // 如果入口块号不存在且不要求创建，则退出
        if (!create) {
            goto out;
        }
        // 如果要求创建，则分配一个新的块作为间接块
        if ((ret = sfs_block_alloc(sfs, &ent)) != 0) {
            return ret;
        }
    }
    
    // 分配实际的数据块（或者下下一级块）
    if ((ret = sfs_block_alloc(sfs, &ino)) != 0) {
        goto failed_cleanup;
    }
    // 将新分配的块号写入到间接块中
    if ((ret = sfs_wbuf(sfs, &ino, sizeof(uint32_t), ent, offset)) != 0) {
        sfs_block_free(sfs, ino);
        goto failed_cleanup;
    }

out:
    // 如果入口块号发生了变化（新分配了间接块），更新传入的指针
    if (ent != *entp) {
        *entp = ent;
    }
    *ino_store = ino;
    return 0;

failed_cleanup:
    // 如果失败且新分配了间接块，释放该间接块
    if (ent != *entp) {
        sfs_block_free(sfs, ent);
    }
    return ret;
}

/*
 * sfs_bmap_get_nolock - according sfs_inode and index of block, find the NO. of disk block
 *                       no lock protect
 * 获取指定逻辑块索引（index）对应的磁盘块号（ino_store），支持自动创建
 * @sfs:      sfs 文件系统结构
 * @sin:      sfs inode 结构
 * @index:    文件内的逻辑块索引号
 * @create:   如果该块不存在，是否自动分配新块
 * @ino_store: 返回获取到的磁盘块号
 */
static int
sfs_bmap_get_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, uint32_t index, bool create, uint32_t *ino_store) {
    struct sfs_disk_inode *din = sin->din;
    int ret;
    uint32_t ent, ino;
	// 如果索引在直接块范围内 (前 SFS_NDIRECT 个块)
    if (index < SFS_NDIRECT) {
        // 如果该块尚未分配（块号为0）且需要创建
        if ((ino = din->direct[index]) == 0 && create) {
            // 分配一个新的磁盘块
            if ((ret = sfs_block_alloc(sfs, &ino)) != 0) {
                return ret;
            }
            // 更新 inode 中的直接块数组
            din->direct[index] = ino;
            // 标记 inode 为 dirty
            sin->dirty = 1;
        }
        goto out;
    }
    // 如果索引超出直接块范围，计算在间接块中的索引
    index -= SFS_NDIRECT;
    if (index < SFS_BLK_NENTRY) {
        // 处理一级间接块
        ent = din->indirect;
        // 调用子函数在间接块中查找或分配
        if ((ret = sfs_bmap_get_sub_nolock(sfs, &ent, index, create, &ino)) != 0) {
            return ret;
        }
        // 如果间接块本身是新分配的，更新 inode 中的 indirect 字段
        if (ent != din->indirect) {
            assert(din->indirect == 0);
            din->indirect = ent;
            sin->dirty = 1;
        }
        goto out;
    } else {
		panic ("sfs_bmap_get_nolock - index out of range");
	}
out:
    assert(ino == 0 || sfs_block_inuse(sfs, ino));
    *ino_store = ino;
    return 0;
}

/*
 * sfs_bmap_free_sub_nolock - set the entry item to 0 (free) in the indirect block
 * 释放间接块中的某一项（释放其指向的数据块）
 */
static int
sfs_bmap_free_sub_nolock(struct sfs_fs *sfs, uint32_t ent, uint32_t index) {
    assert(sfs_block_inuse(sfs, ent) && index < SFS_BLK_NENTRY);
    int ret;
    uint32_t ino, zero = 0;
    // 计算在间接块中的偏移量
    off_t offset = index * sizeof(uint32_t);
    // 读取间接块中该索引指向的块号
    if ((ret = sfs_rbuf(sfs, &ino, sizeof(uint32_t), ent, offset)) != 0) {
        return ret;
    }
    if (ino != 0) {
        // 将间接块中该位置清零
        if ((ret = sfs_wbuf(sfs, &zero, sizeof(uint32_t), ent, offset)) != 0) {
            return ret;
        }
        // 释放实际的数据块
        sfs_block_free(sfs, ino);
    }
    return 0;
}

/*
 * sfs_bmap_free_nolock - free a block with logical index in inode and reset the inode's fields
 * 释放文件内指定逻辑索引的磁盘块
 */
static int
sfs_bmap_free_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, uint32_t index) {
    struct sfs_disk_inode *din = sin->din;
    int ret;
    uint32_t ent, ino;
    // 如果是直接块范围
    if (index < SFS_NDIRECT) {
        if ((ino = din->direct[index]) != 0) {
			// 释放该块
            sfs_block_free(sfs, ino);
            din->direct[index] = 0;
            sin->dirty = 1;
        }
        return 0;
    }

    // 如果是间接块范围
    index -= SFS_NDIRECT;
    if (index < SFS_BLK_NENTRY) {
        if ((ent = din->indirect) != 0) {
			// 在间接块中释放该块
            if ((ret = sfs_bmap_free_sub_nolock(sfs, ent, index)) != 0) {
                return ret;
            }
        }
        return 0;
    }
    return 0;
}

/*
 * sfs_bmap_load_nolock - according to the DIR's inode and the logical index of block in inode, find the NO. of disk block.
 * @sfs:      sfs file system
 * @sin:      sfs inode in memory
 * @index:    the logical index of disk block in inode
 * @ino_store:the NO. of disk block
 * 加载（获取）文件逻辑块对应的物理磁盘块号。如果是追加写，可能会创建新块。
 */
static int
sfs_bmap_load_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, uint32_t index, uint32_t *ino_store) {
    struct sfs_disk_inode *din = sin->din;
    // index 不能超过当前文件块数（允许等于 din->blocks，此时为追加新块）
    assert(index <= din->blocks);
    int ret;
    uint32_t ino;
    // 如果 index 等于当前块数，说明是需要在末尾创建新块
    bool create = (index == din->blocks);
    if ((ret = sfs_bmap_get_nolock(sfs, sin, index, create, &ino)) != 0) {
        return ret;
    }
    assert(sfs_block_inuse(sfs, ino));
    // 如果创建了新块，更新总块数
    if (create) {
        din->blocks ++;
    }
    if (ino_store != NULL) {
        *ino_store = ino;
    }
    return 0;
}

/*
 * sfs_bmap_truncate_nolock - free the disk block at the end of file
 * 截断文件，移除文件末尾的一个块
 */
static int
sfs_bmap_truncate_nolock(struct sfs_fs *sfs, struct sfs_inode *sin) {
    struct sfs_disk_inode *din = sin->din;
    assert(din->blocks != 0);
    int ret;
    // 释放最后一个逻辑块 (din->blocks - 1)
    if ((ret = sfs_bmap_free_nolock(sfs, sin, din->blocks - 1)) != 0) {
        return ret;
    }
    // 减少文件块数计数
    din->blocks --;
    sin->dirty = 1;
    return 0;
}

/*
 * sfs_dirent_read_nolock - read the file entry from disk block which contains this entry
 * @sfs:      sfs file system
 * @sin:      sfs inode in memory
 * @slot:     the index of file entry
 * @entry:    file entry
 * 读取目录中的第 slot 个目录项
 */
static int
sfs_dirent_read_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, int slot, struct sfs_disk_entry *entry) {
    assert(sin->din->type == SFS_TYPE_DIR && (slot >= 0 && slot < sin->din->blocks));
    int ret;
    uint32_t ino;
	// 根据目录的 inode 和目录项的逻辑索引，找到包含该目录项的磁盘块号
    if ((ret = sfs_bmap_load_nolock(sfs, sin, slot, &ino)) != 0) {
        return ret;
    }
    assert(sfs_block_inuse(sfs, ino));
	// 读取该磁盘块中的目录项数据
    if ((ret = sfs_rbuf(sfs, entry, sizeof(struct sfs_disk_entry), ino, 0)) != 0) {
        return ret;
    }
    // 确保目录项名称以 null 结尾
    entry->name[SFS_MAX_FNAME_LEN] = '\0';
    return 0;
}

#define sfs_dirent_link_nolock_check(sfs, sin, slot, lnksin, name)                  \
    do {                                                                            \
        int err;                                                                    \
        if ((err = sfs_dirent_link_nolock(sfs, sin, slot, lnksin, name)) != 0) {    \
            warn("sfs_dirent_link error: %e.\n", err);                              \
        }                                                                           \
    } while (0)

#define sfs_dirent_unlink_nolock_check(sfs, sin, slot, lnksin)                      \
    do {                                                                            \
        int err;                                                                    \
        if ((err = sfs_dirent_unlink_nolock(sfs, sin, slot, lnksin)) != 0) {        \
            warn("sfs_dirent_unlink error: %e.\n", err);                            \
        }                                                                           \
    } while (0)

/*
 * sfs_dirent_search_nolock - read every file entry in the DIR, compare file name with each entry->name
 *                            If equal, then return slot and NO. of disk of this file's inode
 * @sfs:        sfs file system
 * @sin:        sfs inode in memory
 * @name:       the filename
 * @ino_store:  NO. of disk of this file (with the filename)'s inode
 * @slot:       logical index of file entry (NOTICE: each file entry ocupied one  disk block)
 * @empty_slot: the empty logical index of file entry.
 * 在目录中搜索文件名，返回对应的 inode 编号和目录项槽位
 */
static int
sfs_dirent_search_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, const char *name, uint32_t *ino_store, int *slot, int *empty_slot) {
    assert(strlen(name) <= SFS_MAX_FNAME_LEN);
    struct sfs_disk_entry *entry;
    // 分配内存用于临时存储读取到的目录项
    if ((entry = kmalloc(sizeof(struct sfs_disk_entry))) == NULL) {
        return -E_NO_MEM;
    }

#define set_pvalue(x, v)            do { if ((x) != NULL) { *(x) = (v); } } while (0)
    int ret, i, nslots = sin->din->blocks;
    // 初始化 empty_slot 为 nslots（即末尾）
    set_pvalue(empty_slot, nslots);
    //遍历所有目录项
    for (i = 0; i < nslots; i ++) {
        // 读取第 i 个目录项
        if ((ret = sfs_dirent_read_nolock(sfs, sin, i, entry)) != 0) {
            goto out;
        }
        // 如果该目录项的 inode 编号为 0，表示该槽位为空
        if (entry->ino == 0) {
            // 记录第一个找到的空闲槽位
            set_pvalue(empty_slot, i);
            continue ;
        }
        // 如果名字匹配
        if (strcmp(name, entry->name) == 0) {
            // 设置返回的槽位索引和 inode 编号
            set_pvalue(slot, i);
            set_pvalue(ino_store, entry->ino);
            goto out;
        }
    }
#undef set_pvalue
    ret = -E_NOENT;
out:
    kfree(entry);
    return ret;
}

/*
 * sfs_dirent_findino_nolock - read all file entries in DIR's inode and find a entry->ino == ino
 * 在目录中查找具有指定 inode 编号的目录项
 */

static int
sfs_dirent_findino_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, uint32_t ino, struct sfs_disk_entry *entry) {
    int ret, i, nslots = sin->din->blocks;
    // 遍历目录中所有槽位
    for (i = 0; i < nslots; i ++) {
        // 读取目录项
        if ((ret = sfs_dirent_read_nolock(sfs, sin, i, entry)) != 0) {
            return ret;
        }
        // 检查 inode 编号是否匹配
        if (entry->ino == ino) {
            return 0;
        }
    }
    return -E_NOENT;
}

/*
 * sfs_lookup_once - find inode corresponding the file name in DIR's sin inode 
 * @sfs:        sfs file system
 * @sin:        DIR sfs inode in memory
 * @name:       the file name in DIR
 * @node_store: the inode corresponding the file name in DIR
 * @slot:       the logical index of file entry
 * 在目录 inode (sin) 中查找名为 name 的文件，并返回其 inode
 */
static int
sfs_lookup_once(struct sfs_fs *sfs, struct sfs_inode *sin, const char *name, struct inode **node_store, int *slot) {
    int ret;
    uint32_t ino;
    lock_sin(sin);
    {   
        // 调用搜索函数，查找文件名对应的磁盘块号（ino）和槽位（slot）
        ret = sfs_dirent_search_nolock(sfs, sin, name, &ino, slot, NULL);
    }
    unlock_sin(sin);
    if (ret == 0) {
		// 如果找到，加载该 indoe 到内存
        ret = sfs_load_inode(sfs, node_store, ino);
    }
    return ret;
}

// sfs_opendir - just check the opne_flags, now support readonly
// 打开目录，目前只支持只读模式检查
static int
sfs_opendir(struct inode *node, uint32_t open_flags) {
    switch (open_flags & O_ACCMODE) {
    case O_RDONLY:
        // 只允许只读打开
        break;
    case O_WRONLY:
    case O_RDWR:
    default:
        return -E_ISDIR;
    }
    if (open_flags & O_APPEND) {
        return -E_ISDIR;
    }
    return 0;
}

// sfs_openfile - open file (no use)
// 打开文件（目前没有特殊操作）
static int
sfs_openfile(struct inode *node, uint32_t open_flags) {
    return 0;
}

// sfs_close - close file
// 关闭文件，尝试同步数据
static int
sfs_close(struct inode *node) {
    return vop_fsync(node);
}

/*  
 * sfs_io_nolock - Rd/Wr a file contentfrom offset position to offset+ length  disk blocks<-->buffer (in memroy)
 * @sfs:      sfs file system
 * @sin:      sfs inode in memory
 * @buf:      the buffer Rd/Wr
 * @offset:   the offset of file
 * @alenp:    the length need to read (is a pointer). and will RETURN the really Rd/Wr lenght
 * @write:    BOOL, 0 read, 1 write
 * 无锁的文件 IO 操作核心函数，处理不对齐读写和块读写
 */
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
    uint32_t blkno = offset / SFS_BLKSIZE;          // The NO. of Rd/Wr begin block
    uint32_t nblks = endpos / SFS_BLKSIZE - blkno;  // The size of Rd/Wr blocks

  //LAB8:EXERCISE1 2314035 HINT: call sfs_bmap_load_nolock, sfs_rbuf, sfs_rblock,etc. read different kind of blocks in file
	/*
	 * (1) If offset isn't aligned with the first block, Rd/Wr some content from offset to the end of the first block
	 *       NOTICE: useful function: sfs_bmap_load_nolock, sfs_buf_op
	 *               Rd/Wr size = (nblks != 0) ? (SFS_BLKSIZE - blkoff) : (endpos - offset)
	 * (2) Rd/Wr aligned blocks 
	 *       NOTICE: useful function: sfs_bmap_load_nolock, sfs_block_op
     * (3) If end position isn't aligned with the last block, Rd/Wr some content from begin to the (endpos % SFS_BLKSIZE) of the last block
	 *       NOTICE: useful function: sfs_bmap_load_nolock, sfs_buf_op	
	*/

    // 计算起始块内偏移
    blkoff = offset % SFS_BLKSIZE;

    // 情况1：处理第一个不对齐的块（从 blkoff 开始）
    if (blkoff != 0) {
        // 计算需要处理的大小：
        // - 如果还有后续块(nblks > 0)，处理到本块末尾
        // - 如果没有后续块(nblks == 0)，只处理 endpos - offset（全部在一块内）
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
        // 如果没有后续块，说明全部处理完毕，直接退出
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

    // 情况3：处理最后一个不对齐的块（从块起始开始，到 endpos % SFS_BLKSIZE）
    // 只有在 endpos 不是块对齐时才需要处理
    // 注意：如果第一块已经处理了全部内容（在情况1中 goto out），这里不会执行
    if (endpos % SFS_BLKSIZE != 0) {
        // 计算最后块需要处理的大小
        size = endpos % SFS_BLKSIZE;
        
        // 获取磁盘块号
        if ((ret = sfs_bmap_load_nolock(sfs, sin, blkno, &ino)) != 0) {
            goto out;
        }
        
        // 读/写部分块（从块起始开始）
        if ((ret = sfs_buf_op(sfs, buf, size, ino, 0)) != 0) {
            goto out;
        }
        
        // 更新状态
        alen += size;
    }

out:
    // 返回实际读写的字节数
    *alenp = alen;
    // 如果是写操作且超出了原文件大小，更新文件大小
    if (offset + alen > sin->din->size) {
        sin->din->size = offset + alen;
        sin->dirty = 1;
    }
    return ret;
}

/*
 * sfs_io - Rd/Wr file. the wrapper of sfs_io_nolock
            with lock protect
 * sfs_io_nolock 的加锁封装
 */
static inline int
sfs_io(struct inode *node, struct iobuf *iob, bool write) {
    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    int ret;
    lock_sin(sin);
    {
        size_t alen = iob->io_resid;
        // 调用无锁 IO 函数
        ret = sfs_io_nolock(sfs, sin, iob->io_base, iob->io_offset, &alen, write);
        if (alen != 0) {
            // 更新 io buffer 状态 (io_offset += alen, etc.)
            iobuf_skip(iob, alen);
        }
    }
    unlock_sin(sin);
    return ret;
}

// sfs_read - read file
// 读文件：调用通用的 IO 函数
static int
sfs_read(struct inode *node, struct iobuf *iob) {
    return sfs_io(node, iob, 0);
}

// sfs_write - write file
// 写文件：调用通用的 IO 函数
static int
sfs_write(struct inode *node, struct iobuf *iob) {
    return sfs_io(node, iob, 1);
}

/*
 * sfs_fstat - Return nlinks/block/size, etc. info about a file. The pointer is a pointer to struct stat;
 * 获取文件状态信息（大小、类型、链接数等）
 */
static int
sfs_fstat(struct inode *node, struct stat *stat) {
    int ret;
    memset(stat, 0, sizeof(struct stat));
     // 获取文件类型
    if ((ret = vop_gettype(node, &(stat->st_mode))) != 0) {
        return ret;
    }
    struct sfs_disk_inode *din = vop_info(node, sfs_inode)->din;
    stat->st_nlinks = din->nlinks; // 链接数
    stat->st_blocks = din->blocks; // 占用的块数
    stat->st_size = din->size;     // 文件大小
    return 0;
}

/*
 * sfs_fsync - Force any dirty inode info associated with this file to stable storage.
 * 同步文件：将 inode 的修改写入磁盘
 */
static int
sfs_fsync(struct inode *node) {
    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    int ret = 0;
    if (sin->dirty) {
        lock_sin(sin);
        {
            if (sin->dirty) {
                sin->dirty = 0;
                // 将 inode 内容写回磁盘
                if ((ret = sfs_wbuf(sfs, sin->din, sizeof(struct sfs_disk_inode), sin->ino, 0)) != 0) {
                    sin->dirty = 1;
                }
            }
        }
        unlock_sin(sin);
    }
    return ret;
}

/*
 *sfs_namefile -Compute pathname relative to filesystem root of the file and copy to the specified io buffer.
 * 获取文件的绝对路径名（相对于文件系统根目录）
 */
static int
sfs_namefile(struct inode *node, struct iobuf *iob) {
    struct sfs_disk_entry *entry;
    // 检查缓冲区大小并分配内存
    if (iob->io_resid <= 2 || (entry = kmalloc(sizeof(struct sfs_disk_entry))) == NULL) {
        return -E_NO_MEM;
    }

    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    struct sfs_inode *sin = vop_info(node, sfs_inode);

    int ret;
    char *ptr = iob->io_base + iob->io_resid;
    size_t alen, resid = iob->io_resid - 2;
    vop_ref_inc(node);
    // 向上遍历直到根目录
    while (1) {
        struct inode *parent;
        // 查找父目录 ".."
        if ((ret = sfs_lookup_once(sfs, sin, "..", &parent, NULL)) != 0) {
            goto failed;
        }

        uint32_t ino = sin->ino;
        vop_ref_dec(node);
        // 如果当前节点就是父节点，说明到达根目录
        if (node == parent) {
            vop_ref_dec(node);
            break;
        }

        node = parent, sin = vop_info(node, sfs_inode);
        assert(ino != sin->ino && sin->din->type == SFS_TYPE_DIR);

        lock_sin(sin);
        {
            // 在父目录中查找当前节点的名称
            ret = sfs_dirent_findino_nolock(sfs, sin, ino, entry);
        }
        unlock_sin(sin);

        if (ret != 0) {
            goto failed;
        }

        // 检查路径长度是否超出缓冲区
        if ((alen = strlen(entry->name) + 1) > resid) {
            goto failed_nomem;
        }
        resid -= alen, ptr -= alen;
        // 复制名称到缓冲区（从后往前）
        memcpy(ptr, entry->name, alen - 1);
        ptr[alen - 1] = '/';
    }
    // 移动数据到缓冲区头部
    alen = iob->io_resid - resid - 2;
    ptr = memmove(iob->io_base + 1, ptr, alen);
    ptr[-1] = '/', ptr[alen] = '\0';
    iobuf_skip(iob, alen);
    kfree(entry);
    return 0;

failed_nomem:
    ret = -E_NO_MEM;
failed:
    vop_ref_dec(node);
    kfree(entry);
    return ret;
}

/*
 * sfs_getdirentry_sub_noblock - get the content of file entry in DIR
 * 获取目录中指定位置的目录项内容（不加锁）
 */
static int
sfs_getdirentry_sub_nolock(struct sfs_fs *sfs, struct sfs_inode *sin, int slot, struct sfs_disk_entry *entry) {
    int ret, i, nslots = sin->din->blocks;
    // 遍历所有目录块
    for (i = 0; i < nslots; i ++) {
        // 读取目录项
        if ((ret = sfs_dirent_read_nolock(sfs, sin, i, entry)) != 0) {
            return ret;
        }
        // 如果目录项有效（ino != 0）
        if (entry->ino != 0) {
            if (slot == 0) {
                return 0;
            }
            slot --;
        }
    }
    return -E_NOENT;
}

/*
 * sfs_getdirentry - according to the iob->io_offset, calculate the dir entry's slot in disk block,
                     get dir entry content from the disk 
 * 获取目录项信息（带锁保护）
 */
static int
sfs_getdirentry(struct inode *node, struct iobuf *iob) {
    struct sfs_disk_entry *entry;
    // 分配内存
    if ((entry = kmalloc(sizeof(struct sfs_disk_entry))) == NULL) {
        return -E_NO_MEM;
    }

    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    struct sfs_inode *sin = vop_info(node, sfs_inode);

    int ret, slot;
    off_t offset = iob->io_offset;
    // 检查 offset 是否对齐
    if (offset < 0 || offset % sfs_dentry_size != 0) {
        kfree(entry);
        return -E_INVAL;
    }
    // 计算 slot 索引
    if ((slot = offset / sfs_dentry_size) > sin->din->blocks) {
        kfree(entry);
        return -E_NOENT;
    }
    lock_sin(sin);
    // 调用无锁函数获取目录项
    if ((ret = sfs_getdirentry_sub_nolock(sfs, sin, slot, entry)) != 0) {
        unlock_sin(sin);
        goto out;
    }
    unlock_sin(sin);
    // 将目录名复制到 iobuf
    ret = iobuf_move(iob, entry->name, sfs_dentry_size, 1, NULL);
out:
    kfree(entry);
    return ret;
}

/*
 * sfs_reclaim - Free all resources inode occupied . Called when inode is no longer in use. 
 * 回收 inode 占用的资源
 */
static int
sfs_reclaim(struct inode *node) {
    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    struct sfs_inode *sin = vop_info(node, sfs_inode);

    int  ret = -E_BUSY;
    uint32_t ent;
    lock_sfs_fs(sfs); // 锁定整个文件系统结构，确保 inode 链表操作安全
    assert(sin->reclaim_count > 0);
    // 减少 reclaim 计数，如果仍被引用则不能回收
    if ((-- sin->reclaim_count) != 0 || inode_ref_count(node) != 0) {
        goto failed_unlock;
    }
    // 如果硬链接数为 0，截断文件（释放数据块）
    if (sin->din->nlinks == 0) {
        if ((ret = vop_truncate(node, 0)) != 0) {
            goto failed_unlock;
        }
    }
    // 如果是 dirty，同步到磁盘
    if (sin->dirty) {
        if ((ret = vop_fsync(node)) != 0) {
            goto failed_unlock;
        }
    }
    // 从 inode 哈希链表中移除
    sfs_remove_links(sin);
    unlock_sfs_fs(sfs);

    // 如果硬链接数为 0，释放 inode 块和间接块
    if (sin->din->nlinks == 0) {
        sfs_block_free(sfs, sin->ino);
        if ((ent = sin->din->indirect) != 0) {
            sfs_block_free(sfs, ent);
        }
    }
    // 释放内存
    kfree(sin->din);
    vop_kill(node); // 销毁 VFS inode
    return 0;

failed_unlock:
    unlock_sfs_fs(sfs);
    return ret;
}

/*
 * sfs_gettype - Return type of file. The values for file types are in sfs.h.
 * 获取文件类型
 */
static int
sfs_gettype(struct inode *node, uint32_t *type_store) {
    struct sfs_disk_inode *din = vop_info(node, sfs_inode)->din;
    switch (din->type) {
    case SFS_TYPE_DIR:
        *type_store = S_IFDIR;
        return 0;
    case SFS_TYPE_FILE:
        *type_store = S_IFREG;
        return 0;
    case SFS_TYPE_LINK:
        *type_store = S_IFLNK;
        return 0;
    }
    panic("invalid file type %d.\n", din->type);
}

/* 
 * sfs_tryseek - Check if seeking to the specified position within the file is legal.
 * 检查 seek 操作是否合法
 */
static int
sfs_tryseek(struct inode *node, off_t pos) {
    if (pos < 0 || pos >= SFS_MAX_FILE_SIZE) {
        return -E_INVAL;
    }
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    // 允许 seek 超过当前文件大小（产生空洞），这时会自动扩展文件
    if (pos > sin->din->size) {
        return vop_truncate(node, pos);
    }
    return 0;
}

/*
 * sfs_truncfile : reszie the file with new length
 * 调整文件大小（截断或扩展）
 */
static int
sfs_truncfile(struct inode *node, off_t len) {
    if (len < 0 || len > SFS_MAX_FILE_SIZE) {
        return -E_INVAL;
    }
    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    struct sfs_disk_inode *din = sin->din;

    int ret = 0;
	// 计算新的块数
    uint32_t nblks, tblks = ROUNDUP_DIV(len, SFS_BLKSIZE);
    if (din->size == len) {
        assert(tblks == din->blocks);
        return 0;
    }

    lock_sin(sin);
	// 获取当前文件占用的块数
    nblks = din->blocks;
    if (nblks < tblks) {
		// 扩展文件：在文件末尾添加新的磁盘块
        while (nblks != tblks) {
            // 加载块（传入 nblks 表示分配第 nblks 块，即追加）
            if ((ret = sfs_bmap_load_nolock(sfs, sin, nblks, NULL)) != 0) {
                goto out_unlock;
            }
            nblks ++;
        }
    }
    else if (tblks < nblks) {
		// 缩小文件：释放文件末尾的块
        while (tblks != nblks) {
            if ((ret = sfs_bmap_truncate_nolock(sfs, sin)) != 0) {
                goto out_unlock;
            }
            nblks --;
        }
    }
    assert(din->blocks == tblks);
    // 更新文件大小
    din->size = len;
    sin->dirty = 1;

out_unlock:
    unlock_sin(sin);
    return ret;
}

/*
 * sfs_lookup - Parse path relative to the passed directory
 *              DIR, and hand back the inode for the file it
 *              refers to.
 * 在目录中查找文件并返回 inode
 */
static int
sfs_lookup(struct inode *node, char *path, struct inode **node_store) {
    struct sfs_fs *sfs = fsop_info(vop_fs(node), sfs);
    assert(*path != '\0' && *path != '/');
    vop_ref_inc(node);
    struct sfs_inode *sin = vop_info(node, sfs_inode);
    // 确保当前节点是目录
    if (sin->din->type != SFS_TYPE_DIR) {
        vop_ref_dec(node);
        return -E_NOTDIR;
    }
    struct inode *subnode;
    // 调用查找辅助函数
    int ret = sfs_lookup_once(sfs, sin, path, &subnode, NULL);

    vop_ref_dec(node);
    if (ret != 0) {
        return ret;
    }
    *node_store = subnode;
    return 0;
}

// The sfs specific DIR operations correspond to the abstract operations on a inode.
// 目录 inode 操作函数表
static const struct inode_ops sfs_node_dirops = {
    .vop_magic                      = VOP_MAGIC,
    .vop_open                       = sfs_opendir,
    .vop_close                      = sfs_close,
    .vop_fstat                      = sfs_fstat,
    .vop_fsync                      = sfs_fsync,
    .vop_namefile                   = sfs_namefile,
    .vop_getdirentry                = sfs_getdirentry,
    .vop_reclaim                    = sfs_reclaim,
    .vop_gettype                    = sfs_gettype,
    .vop_lookup                     = sfs_lookup,
};
/// The sfs specific FILE operations correspond to the abstract operations on a inode.
// 文件 inode 操作函数表
static const struct inode_ops sfs_node_fileops = {
    .vop_magic                      = VOP_MAGIC,
    .vop_open                       = sfs_openfile,
    .vop_close                      = sfs_close,
    .vop_read                       = sfs_read,
    .vop_write                      = sfs_write,
    .vop_fstat                      = sfs_fstat,
    .vop_fsync                      = sfs_fsync,
    .vop_reclaim                    = sfs_reclaim,
    .vop_gettype                    = sfs_gettype,
    .vop_tryseek                    = sfs_tryseek,
    .vop_truncate                   = sfs_truncfile,
};

