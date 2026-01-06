#include <defs.h>
#include <sfs.h>
#include <error.h>
#include <assert.h>

/*
 * sfs_init - mount sfs on disk0
 *
 * CALL GRAPH:
 *   kern_init-->fs_init-->sfs_init
 * 初始化 SFS 文件系统，挂载 disk0
 */
void
sfs_init(void) {
    int ret;
    // 尝试挂载名为 "disk0" 的设备
    if ((ret = sfs_mount("disk0")) != 0) {
        // 如果挂载失败，触发 panic
        panic("failed: sfs: sfs_mount: %e.\n", ret);
    }
}

