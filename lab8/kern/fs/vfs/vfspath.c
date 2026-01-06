#include <defs.h>
#include <string.h>
#include <vfs.h>
#include <inode.h>
#include <iobuf.h>
#include <stat.h>
#include <proc.h>
#include <error.h>
#include <assert.h>

/*
 * get_cwd_nolock - retrieve current process's working directory. without lock protect
 */
/*
 * get_cwd_nolock - 获取当前进程的工作目录。无锁保护
 */
static struct inode *
get_cwd_nolock(void) {
    return current->filesp->pwd;
}
/*
 * set_cwd_nolock - set current working directory.
 */
/*
 * set_cwd_nolock - 设置当前工作目录
 */
static void
set_cwd_nolock(struct inode *pwd) {
    current->filesp->pwd = pwd;
}

/*
 * lock_cfs - lock the fs related process on current process 
 */
/*
 * lock_cfs - 锁定当前进程的文件系统相关部分
 */
static void
lock_cfs(void) {
    lock_files(current->filesp);
}
/*
 * unlock_cfs - unlock the fs related process on current process 
 */
/*
 * unlock_cfs - 解锁当前进程的文件系统相关部分
 */
static void
unlock_cfs(void) {
    unlock_files(current->filesp);
}

/*
 *  vfs_get_curdir - Get current directory as a inode.
 */
/*
 *  vfs_get_curdir - 获取当前目录作为inode
 */
int
vfs_get_curdir(struct inode **dir_store) {
    struct inode *node;
    if ((node = get_cwd_nolock()) != NULL) { // 获取当前工作目录
        vop_ref_inc(node); // 增加引用计数
        *dir_store = node;
        return 0;
    }
    return -E_NOENT; // 未找到
}

/*
 * vfs_set_curdir - Set current directory as a inode.
 *                  The passed inode must in fact be a directory.
 */
/*
 * vfs_set_curdir - 设置当前目录为指定的inode。
 *                  传入的inode实际上必须是一个目录。
 */
int
vfs_set_curdir(struct inode *dir) {
    int ret = 0;
    lock_cfs(); // 加锁
    struct inode *old_dir;
    if ((old_dir = get_cwd_nolock()) != dir) { // 如果新目录不同于旧目录
        if (dir != NULL) {
            uint32_t type;
            if ((ret = vop_gettype(dir, &type)) != 0) { // 获取inode类型
                goto out;
            }
            if (!S_ISDIR(type)) { // 检查是否为目录
                ret = -E_NOTDIR;
                goto out;
            }
            vop_ref_inc(dir); // 增加新目录的引用计数
        }
        set_cwd_nolock(dir); // 设置新目录
        if (old_dir != NULL) {
            vop_ref_dec(old_dir); // 减少旧目录的引用计数
        }
    }
out:
    unlock_cfs(); // 解锁
    return ret;
}

/*
 * vfs_chdir - Set current directory, as a pathname. Use vfs_lookup to translate
 *             it to a inode.
 */
/*
 * vfs_chdir - 设置当前目录，参数为路径名。使用vfs_lookup将其转换为inode。
 */
int
vfs_chdir(char *path) {
    int ret;
    struct inode *node;
    if ((ret = vfs_lookup(path, &node)) == 0) { // 查找路径对应的inode
        ret = vfs_set_curdir(node); // 设置为当前目录
        vop_ref_dec(node);
    }
    return ret;
}
/*
 * vfs_getcwd - retrieve current working directory(cwd).
 */
/*
 * vfs_getcwd - 获取当前工作目录(cwd)。
 */
int
vfs_getcwd(struct iobuf *iob) {
    int ret;
    struct inode *node;
    if ((ret = vfs_get_curdir(&node)) != 0) { // 获取当前目录inode
        return ret;
    }
    assert(node->in_fs != NULL);

    const char *devname = vfs_get_devname(node->in_fs); // 获取设备名
    if ((ret = iobuf_move(iob, (char *)devname, strlen(devname), 1, NULL)) != 0) { // 写入设备名
        goto out;
    }
    char colon = ':';
    if ((ret = iobuf_move(iob, &colon, sizeof(colon), 1, NULL)) != 0) { // 写入冒号
        goto out;
    }
    ret = vop_namefile(node, iob); // 获取文件名

out:
    vop_ref_dec(node); // 减少引用计数
    return ret;
}

