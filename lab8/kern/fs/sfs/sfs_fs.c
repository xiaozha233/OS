#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <kmalloc.h>
#include <list.h>
#include <fs.h>
#include <vfs.h>
#include <dev.h>
#include <sfs.h>
#include <inode.h>
#include <iobuf.h>
#include <bitmap.h>
#include <error.h>
#include <assert.h>
#include <proc.h>
/*
 * sfs_sync - sync sfs's superblock and freemap in memroy into disk
 * 将 SFS 文件系统的数据（superblock, freemap, dirty inodes）同步到磁盘
 */
static int
sfs_sync(struct fs *fs) {
    struct sfs_fs *sfs = fsop_info(fs, sfs);
    lock_sfs_fs(sfs);
    {
        // 遍历所有 inode
        list_entry_t *list = &(sfs->inode_list), *le = list;
        while ((le = list_next(le)) != list) {
            struct sfs_inode *sin = le2sin(le, inode_link);
            // 同步每个 inode
            vop_fsync(info2node(sin, sfs_inode));
        }
    }
    unlock_sfs_fs(sfs);

    int ret;
    // 如果超级块标记为 dirty，则写回超级块和 freemap
    if (sfs->super_dirty) {
        sfs->super_dirty = 0;
        if ((ret = sfs_sync_super(sfs)) != 0) {
            sfs->super_dirty = 1;
            return ret;
        }
        if ((ret = sfs_sync_freemap(sfs)) != 0) {
            sfs->super_dirty = 1;
            return ret;
        }
    }
    return 0;
}

/*
 * sfs_get_root - get the root directory inode  from disk (SFS_BLKN_ROOT,1)
 * 获取根目录的 inode
 */
static struct inode *
sfs_get_root(struct fs *fs) {
    struct inode *node;
    int ret;
    // 加载根节点 inode (块号为 SFS_BLKN_ROOT)
    if ((ret = sfs_load_inode(fsop_info(fs, sfs), &node, SFS_BLKN_ROOT)) != 0) {
        panic("load sfs root failed: %e", ret);
    }
    return node;
}

/*
 * sfs_unmount - unmount sfs, and free the memorys contain sfs->freemap/sfs_buffer/hash_liskt and sfs itself.
 * 卸载 SFS 文件系统
 */
static int
sfs_unmount(struct fs *fs) {
    struct sfs_fs *sfs = fsop_info(fs, sfs);
    // 确保所有 inode 已被清理
    if (!list_empty(&(sfs->inode_list))) {
        return -E_BUSY;
    }
    assert(!sfs->super_dirty);
    // 释放资源
    bitmap_destroy(sfs->freemap);
    kfree(sfs->sfs_buffer);
    kfree(sfs->hash_list);
    kfree(sfs);
    return 0;
}

/*
 * sfs_cleanup - when sfs failed, then should call this function to sync sfs by calling sfs_sync
 *
 * NOTICE: nouse now.
 * 清理函数（发生错误时调用），尝试同步数据
 */
static void
sfs_cleanup(struct fs *fs) {
    struct sfs_fs *sfs = fsop_info(fs, sfs);
    uint32_t blocks = sfs->super.blocks, unused_blocks = sfs->super.unused_blocks;
    cprintf("sfs: cleanup: '%s' (%d/%d/%d)\n", sfs->super.info,
            blocks - unused_blocks, unused_blocks, blocks);
    int i, ret;
    // 尝试同步 32 次
    for (i = 0; i < 32; i ++) {
        if ((ret = fsop_sync(fs)) == 0) {
            break;
        }
    }
    if (ret != 0) {
        warn("sfs: sync error: '%s': %e.\n", sfs->super.info, ret);
    }
}

/*
 * sfs_init_read - used in sfs_do_mount to read disk block(blkno, 1) directly.
 *
 * @dev:        the block device
 * @blkno:      the NO. of disk block
 * @blk_buffer: the buffer used for read
 *
 *      (1) init iobuf
 *      (2) read dev into iobuf
 * 初始化读取辅助函数：直接从设备读取一个块到缓冲区
 */
static int
sfs_init_read(struct device *dev, uint32_t blkno, void *blk_buffer) {
    struct iobuf __iob, *iob = iobuf_init(&__iob, blk_buffer, SFS_BLKSIZE, blkno * SFS_BLKSIZE);
    return dop_io(dev, iob, 0);
}

/*
 * sfs_init_freemap - used in sfs_do_mount to read freemap data info in disk block(blkno, nblks) directly.
 *
 * @dev:        the block device
 * @bitmap:     the bitmap in memroy
 * @blkno:      the NO. of disk block
 * @nblks:      Rd number of disk block
 * @blk_buffer: the buffer used for read
 *
 *      (1) get data addr in bitmap
 *      (2) read dev into iobuf
 * 初始化 Freemap：从磁盘读取 Freemap 数据到内存 bitmap 中
 */
static int
sfs_init_freemap(struct device *dev, struct bitmap *freemap, uint32_t blkno, uint32_t nblks, void *blk_buffer) {
    size_t len;
    void *data = bitmap_getdata(freemap, &len);
    assert(data != NULL && len == nblks * SFS_BLKSIZE);
    while (nblks != 0) {
        int ret;
        // 读取一个块的数据到 buffer
        if ((ret = sfs_init_read(dev, blkno, data)) != 0) {
            return ret;
        }
        blkno ++, nblks --, data += SFS_BLKSIZE;
    }
    return 0;
}

/*
 * sfs_do_mount - mount sfs file system.
 *
 * @dev:        the block device contains sfs file system
 * @fs_store:   the fs struct in memroy
 * 执行具体的挂载操作
 */
static int
sfs_do_mount(struct device *dev, struct fs **fs_store) {
    static_assert(SFS_BLKSIZE >= sizeof(struct sfs_super));
    static_assert(SFS_BLKSIZE >= sizeof(struct sfs_disk_inode));
    static_assert(SFS_BLKSIZE >= sizeof(struct sfs_disk_entry));

    if (dev->d_blocksize != SFS_BLKSIZE) {
        return -E_NA_DEV;
    }

    /* allocate fs structure */
    struct fs *fs;
    // 分配 fs 结构
    if ((fs = alloc_fs(sfs)) == NULL) {
        return -E_NO_MEM;
    }
    struct sfs_fs *sfs = fsop_info(fs, sfs);
    sfs->dev = dev;

    int ret = -E_NO_MEM;

    void *sfs_buffer;
    // 分配 SFS 内部使用的临时 buffer
    if ((sfs->sfs_buffer = sfs_buffer = kmalloc(SFS_BLKSIZE)) == NULL) {
        goto failed_cleanup_fs;
    }

    /* load and check superblock */
    // 读取超级块
    if ((ret = sfs_init_read(dev, SFS_BLKN_SUPER, sfs_buffer)) != 0) {
        goto failed_cleanup_sfs_buffer;
    }

    ret = -E_INVAL;

    struct sfs_super *super = sfs_buffer;
    // 检查魔数
    if (super->magic != SFS_MAGIC) {
        cprintf("sfs: wrong magic in superblock. (%08x should be %08x).\n",
                super->magic, SFS_MAGIC);
        goto failed_cleanup_sfs_buffer;
    }
    // 检查块数
    if (super->blocks > dev->d_blocks) {
        cprintf("sfs: fs has %u blocks, device has %u blocks.\n",
                super->blocks, dev->d_blocks);
        goto failed_cleanup_sfs_buffer;
    }
    super->info[SFS_MAX_INFO_LEN] = '\0';
    sfs->super = *super;

    ret = -E_NO_MEM;

    uint32_t i;

    /* alloc and initialize hash list */
    // 初始化 inode 哈希表
    list_entry_t *hash_list;
    if ((sfs->hash_list = hash_list = kmalloc(sizeof(list_entry_t) * SFS_HLIST_SIZE)) == NULL) {
        goto failed_cleanup_sfs_buffer;
    }
    for (i = 0; i < SFS_HLIST_SIZE; i ++) {
        list_init(hash_list + i);
    }

    /* load and check freemap */
    // 创建和加载 freemap
    struct bitmap *freemap;
    uint32_t freemap_size_nbits = sfs_freemap_bits(super);
    if ((sfs->freemap = freemap = bitmap_create(freemap_size_nbits)) == NULL) {
        goto failed_cleanup_hash_list;
    }
    uint32_t freemap_size_nblks = sfs_freemap_blocks(super);
    if ((ret = sfs_init_freemap(dev, freemap, SFS_BLKN_FREEMAP, freemap_size_nblks, sfs_buffer)) != 0) {
        goto failed_cleanup_freemap;
    }

    // 验证 freemap 中的空闲块数是否与超级块一致
    uint32_t blocks = sfs->super.blocks, unused_blocks = 0;
    for (i = 0; i < freemap_size_nbits; i ++) {
        if (bitmap_test(freemap, i)) {
            unused_blocks ++;
        }
    }
    assert(unused_blocks == sfs->super.unused_blocks);

    /* and other fields */
    sfs->super_dirty = 0;
    // 初始化信号量和链表
    sem_init(&(sfs->fs_sem), 1);
    sem_init(&(sfs->io_sem), 1);
    sem_init(&(sfs->mutex_sem), 1);
    list_init(&(sfs->inode_list));
    cprintf("sfs: mount: '%s' (%d/%d/%d)\n", sfs->super.info,
            blocks - unused_blocks, unused_blocks, blocks);

    /* link addr of sync/get_root/unmount/cleanup funciton  fs's function pointers*/
    // 绑定函数指针
    fs->fs_sync = sfs_sync;
    fs->fs_get_root = sfs_get_root;
    fs->fs_unmount = sfs_unmount;
    fs->fs_cleanup = sfs_cleanup;
    *fs_store = fs;
    return 0;

failed_cleanup_freemap:
    bitmap_destroy(freemap);
failed_cleanup_hash_list:
    kfree(hash_list);
failed_cleanup_sfs_buffer:
    kfree(sfs_buffer);
failed_cleanup_fs:
    kfree(fs);
    return ret;
}

// 挂载 SFS 文件系统
int
sfs_mount(const char *devname) {
    return vfs_mount(devname, sfs_do_mount);
}

