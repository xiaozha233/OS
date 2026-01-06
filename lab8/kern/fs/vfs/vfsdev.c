#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <vfs.h>
#include <dev.h>
#include <inode.h>
#include <sem.h>
#include <list.h>
#include <kmalloc.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>
#include <proc.h>
// device info entry in vdev_list 
// vdev_list中的设备信息条目
typedef struct {
    const char *devname; // 设备名称
    struct inode *devnode; // 设备inode节点
    struct fs *fs; // 文件系统结构指针
    bool mountable; // 是否可挂载
    list_entry_t vdev_link; // 链表节点
} vfs_dev_t;

#define le2vdev(le, member)                         \
    to_struct((le), vfs_dev_t, member)

static list_entry_t vdev_list;     // device info list in vfs layer // VFS层的设备信息列表
static semaphore_t vdev_list_sem; // 设备列表信号量

static void
lock_vdev_list(void) {
    down(&vdev_list_sem); // 获取设备列表锁
}

static void
unlock_vdev_list(void) {
    up(&vdev_list_sem); // 释放设备列表锁
}

void
vfs_devlist_init(void) {
    list_init(&vdev_list); // 初始化设备列表
    sem_init(&vdev_list_sem, 1); // 初始化信号量为1
}

// vfs_cleanup - finally clean (or sync) fs
// vfs_cleanup - 最终清理（或同步）文件系统
void
vfs_cleanup(void) {
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            list_entry_t *list = &vdev_list, *le = list;
            while ((le = list_next(le)) != list) {
                vfs_dev_t *vdev = le2vdev(le, vdev_link);
                if (vdev->fs != NULL) {
                    fsop_cleanup(vdev->fs); // 清理文件系统
                }
            }
        }
        unlock_vdev_list();
    }
}

/*
 * vfs_get_root - Given a device name (stdin, stdout, etc.), hand
 *                back an appropriate inode.
 */
/*
 * vfs_get_root - 给定设备名称（stdin, stdout等），返回
 *                相应的inode。
 */
int
vfs_get_root(const char *devname, struct inode **node_store) {
    assert(devname != NULL);
    int ret = -E_NO_DEV;
    if (!list_empty(&vdev_list)) {
        lock_vdev_list();
        {
            list_entry_t *list = &vdev_list, *le = list;
            while ((le = list_next(le)) != list) {
                vfs_dev_t *vdev = le2vdev(le, vdev_link);
                if (strcmp(devname, vdev->devname) == 0) { // 找到匹配的设备名
                    struct inode *found = NULL;
                    if (vdev->fs != NULL) {
                        found = fsop_get_root(vdev->fs); // 获取文件系统根节点
                    }
                    else if (!vdev->mountable) {
                        vop_ref_inc(vdev->devnode); // 增加引用计数
                        found = vdev->devnode; // 获取设备节点
                    }
                    if (found != NULL) {
                        ret = 0, *node_store = found;
                    }
                    else {
                        ret = -E_NA_DEV;
                    }
                    break;
                }
            }
        }
        unlock_vdev_list();
    }
    return ret;
}

/*
 * vfs_get_devname - Given a filesystem, hand back the name of the device it's mounted on.
 */
/*
 * vfs_get_devname - 给定一个文件系统，返回它挂载的设备名称。
 */
const char *
vfs_get_devname(struct fs *fs) {
    assert(fs != NULL);
    list_entry_t *list = &vdev_list, *le = list;
    while ((le = list_next(le)) != list) {
        vfs_dev_t *vdev = le2vdev(le, vdev_link);
        if (vdev->fs == fs) {
            return vdev->devname; // 返回设备名称
        }
    }
    return NULL;
}

/*
 * check_devname_confilct - Is there alreadily device which has the same name?
 */
/*
 * check_devname_confilct - 是否已经存在同名设备？
 */
static bool
check_devname_conflict(const char *devname) {
    list_entry_t *list = &vdev_list, *le = list;
    while ((le = list_next(le)) != list) {
        vfs_dev_t *vdev = le2vdev(le, vdev_link);
        if (strcmp(vdev->devname, devname) == 0) {
            return 0; // 冲突
        }
    }
    return 1; // 无冲突
}


/*
* vfs_do_add - Add a new device to the VFS layer's device table.
*
* If "mountable" is set, the device will be treated as one that expects
* to have a filesystem mounted on it, and a raw device will be created
* for direct access.
*/
/*
* vfs_do_add - 向VFS层的设备表添加新设备。
*
* 如果设置了"mountable"，则该设备将被视为期望在其上挂载文件系统的设备，
* 并且将创建一个原始设备用于直接访问。
*/
static int
vfs_do_add(const char *devname, struct inode *devnode, struct fs *fs, bool mountable) {
    assert(devname != NULL);
    assert((devnode == NULL && !mountable) || (devnode != NULL && check_inode_type(devnode, device)));
    if (strlen(devname) > FS_MAX_DNAME_LEN) {
        return -E_TOO_BIG;
    }

    int ret = -E_NO_MEM;
    char *s_devname;
    if ((s_devname = strdup(devname)) == NULL) {
        return ret;
    }

    vfs_dev_t *vdev;
    if ((vdev = kmalloc(sizeof(vfs_dev_t))) == NULL) {
        goto failed_cleanup_name;
    }

    ret = -E_EXISTS;
    lock_vdev_list(); // 加锁
    if (!check_devname_conflict(s_devname)) {
        unlock_vdev_list();
        goto failed_cleanup_vdev;
    }
    vdev->devname = s_devname;
    vdev->devnode = devnode;
    vdev->mountable = mountable;
    vdev->fs = fs;

    list_add(&vdev_list, &(vdev->vdev_link)); // 添加到列表
    unlock_vdev_list(); // 解锁
    return 0;

failed_cleanup_vdev:
    kfree(vdev);
failed_cleanup_name:
    kfree(s_devname);
    return ret;
}

/*
 * vfs_add_fs - Add a new fs,  by name. See  vfs_do_add information for the description of
 *              mountable.
 */
/*
 * vfs_add_fs - 按名称添加新文件系统。关于mountable的描述见vfs_do_add信息。
 */
int
vfs_add_fs(const char *devname, struct fs *fs) {
    return vfs_do_add(devname, NULL, fs, 0);
}

/*
 * vfs_add_dev - Add a new device, by name. See  vfs_do_add information for the description of
 *               mountable.
 */
/*
 * vfs_add_dev - 按名称添加新设备。关于mountable的描述见vfs_do_add信息。
 */
int
vfs_add_dev(const char *devname, struct inode *devnode, bool mountable) {
    return vfs_do_add(devname, devnode, NULL, mountable);
}

/*
 * find_mount - Look for a mountable device named DEVNAME.
 *              Should already hold vdev_list lock.
 */
/*
 * find_mount - 查找名为DEVNAME的可挂载设备。
 *              应已持有vdev_list锁。
 */
static int
find_mount(const char *devname, vfs_dev_t **vdev_store) {
    assert(devname != NULL);
    list_entry_t *list = &vdev_list, *le = list;
    while ((le = list_next(le)) != list) {
        vfs_dev_t *vdev = le2vdev(le, vdev_link);
        if (vdev->mountable && strcmp(vdev->devname, devname) == 0) {
            *vdev_store = vdev;
            return 0;
        }
    }
    return -E_NO_DEV;
}

/*
 * vfs_mount - Mount a filesystem. Once we've found the device, call MOUNTFUNC to
 *             set up the filesystem and hand back a struct fs.
 *
 * The DATA argument is passed through unchanged to MOUNTFUNC.
 */
/*
 * vfs_mount - 挂载文件系统。一旦找到设备，调用MOUNTFUNC以
 *             设置文件系统并返回struct fs。
 *
 * DATA参数原样传递给MOUNTFUNC。
 */
int
vfs_mount(const char *devname, int (*mountfunc)(struct device *dev, struct fs **fs_store)) {
    int ret;
    lock_vdev_list(); // 加锁
    vfs_dev_t *vdev;
    if ((ret = find_mount(devname, &vdev)) != 0) {
        goto out;
    }
    if (vdev->fs != NULL) {
        ret = -E_BUSY;
        goto out;
    }
    assert(vdev->devname != NULL && vdev->mountable);

    struct device *dev = vop_info(vdev->devnode, device);
    if ((ret = mountfunc(dev, &(vdev->fs))) == 0) { // 调用挂载函数
        assert(vdev->fs != NULL);
        cprintf("vfs: mount %s.\n", vdev->devname);
    }

out:
    unlock_vdev_list(); // 解锁
    return ret;
}

/*
 * vfs_unmount - Unmount a filesystem/device by name.
 *               First calls FSOP_SYNC on the filesystem; then calls FSOP_UNMOUNT.
 */
/*
 * vfs_unmount - 按名称卸载文件系统/设备。
 *               首先在文件系统上调用FSOP_SYNC；然后调用FSOP_UNMOUNT。
 */
int
vfs_unmount(const char *devname) {
    int ret;
    lock_vdev_list(); // 加锁
    vfs_dev_t *vdev;
    if ((ret = find_mount(devname, &vdev)) != 0) {
        goto out;
    }
    if (vdev->fs == NULL) {
        ret = -E_INVAL;
        goto out;
    }
    assert(vdev->devname != NULL && vdev->mountable);

    if ((ret = fsop_sync(vdev->fs)) != 0) { // 同步文件系统
        goto out;
    }
    if ((ret = fsop_unmount(vdev->fs)) == 0) { // 卸载文件系统
        vdev->fs = NULL;
        cprintf("vfs: unmount %s.\n", vdev->devname);
    }

out:
    unlock_vdev_list(); // 解锁
    return ret;
}

/*
 * vfs_unmount_all - Global unmount function.
 */
/*
 * vfs_unmount_all - 全局卸载函数。
 */
int
vfs_unmount_all(void) {
    if (!list_empty(&vdev_list)) {
        lock_vdev_list(); // 加锁
        {
            list_entry_t *list = &vdev_list, *le = list;
            while ((le = list_next(le)) != list) {
                vfs_dev_t *vdev = le2vdev(le, vdev_link);
                if (vdev->mountable && vdev->fs != NULL) {
                    int ret;
                    if ((ret = fsop_sync(vdev->fs)) != 0) { // 同步失败
                        cprintf("vfs: warning: sync failed for %s: %e.\n", vdev->devname, ret);
                        continue ;
                    }
                    if ((ret = fsop_unmount(vdev->fs)) != 0) { // 卸载失败
                        cprintf("vfs: warning: unmount failed for %s: %e.\n", vdev->devname, ret);
                        continue ;
                    }
                    vdev->fs = NULL;
                    cprintf("vfs: unmount %s.\n", vdev->devname);
                }
            }
        }
        unlock_vdev_list(); // 解锁
    }
    return 0;
}

