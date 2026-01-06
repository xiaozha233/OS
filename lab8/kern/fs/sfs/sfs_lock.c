#include <defs.h>
#include <sem.h>
#include <sfs.h>


/*
 * lock_sfs_fs - lock the process of  SFS Filesystem Rd/Wr Disk Block
 *
 * called by: sfs_load_inode, sfs_sync, sfs_reclaim
 * 锁定 SFS 文件系统（互斥访问文件系统元数据，如 inode 链表）
 */
void
lock_sfs_fs(struct sfs_fs *sfs) {
    down(&(sfs->fs_sem));
}

/*
 * lock_sfs_io - lock the process of SFS File Rd/Wr Disk Block
 *
 * called by: sfs_rwblock, sfs_clear_block, sfs_sync_super
 * 锁定 SFS IO 操作（互斥访问 sfs_buffer 等共享 IO 资源）
 */
void
lock_sfs_io(struct sfs_fs *sfs) {
    down(&(sfs->io_sem));
}

/*
 * unlock_sfs_fs - unlock the process of  SFS Filesystem Rd/Wr Disk Block
 *
 * called by: sfs_load_inode, sfs_sync, sfs_reclaim
 * 解锁 SFS 文件系统
 */
void
unlock_sfs_fs(struct sfs_fs *sfs) {
    up(&(sfs->fs_sem));
}

/*
 * unlock_sfs_io - unlock the process of sfs Rd/Wr Disk Block
 *
 * called by: sfs_rwblock sfs_clear_block sfs_sync_super
 * 解锁 SFS IO 操作
 */
void
unlock_sfs_io(struct sfs_fs *sfs) {
    up(&(sfs->io_sem));
}
