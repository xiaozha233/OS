#include <defs.h>
#include <string.h>
#include <vfs.h>
#include <inode.h>
#include <unistd.h>
#include <error.h>
#include <assert.h>


// open file in vfs, get/create inode for file with filename path.
// vfs_open - 在VFS中打开文件，根据文件名路径获取/创建inode。
int
vfs_open(char *path, uint32_t open_flags, struct inode **node_store) {
    bool can_write = 0;
    switch (open_flags & O_ACCMODE) { // 检查访问模式
    case O_RDONLY:
        break;
    case O_WRONLY:
    case O_RDWR:
        can_write = 1;
        break;
    default:
        return -E_INVAL;
    }

    if (open_flags & O_TRUNC) { // 如果需要截断
        if (!can_write) {
            return -E_INVAL; // 必须有写权限
        }
    }

    int ret; 
    struct inode *node;
    bool excl = (open_flags & O_EXCL) != 0; // 是否排他创建
    bool create = (open_flags & O_CREAT) != 0; // 是否创建新文件
    ret = vfs_lookup(path, &node); // 查找文件

    if (ret != 0) { // 如果查找失败
        if (ret == -16 && (create)) { // 如果是文件不存在且需要创建
            char *name;
            struct inode *dir;
            if ((ret = vfs_lookup_parent(path, &dir, &name)) != 0) { // 查找父目录
                return ret;
            }
            ret = vop_create(dir, name, excl, &node); // 在父目录下创建文件
        } else return ret;
    } else if (excl && create) { // 如果文件存在且要求排他创建
        return -E_EXISTS;
    }
    assert(node != NULL);
    
    if ((ret = vop_open(node, open_flags)) != 0) { // 打开文件节点
        vop_ref_dec(node); // 减少引用计数
        return ret;
    }

    vop_open_inc(node); // 增加打开计数
    if (open_flags & O_TRUNC || create) { // 如果需要截断或新建
        if ((ret = vop_truncate(node, 0)) != 0) { // 将文件截断为0
            vop_open_dec(node);
            vop_ref_dec(node);
            return ret;
        }
    }
    *node_store = node; // 返回节点
    return 0;
}

// close file in vfs
// vfs_close - 在VFS中关闭文件
int
vfs_close(struct inode *node) {
    vop_open_dec(node); // 减少打开计数
    vop_ref_dec(node); // 减少引用计数
    return 0;
}

// unimplement
// 未实现
int
vfs_unlink(char *path) {
    return -E_UNIMP;
}

// unimplement
// 未实现
int
vfs_rename(char *old_path, char *new_path) {
    return -E_UNIMP;
}

// unimplement
// 未实现
int
vfs_link(char *old_path, char *new_path) {
    return -E_UNIMP;
}

// unimplement
// 未实现
int
vfs_symlink(char *old_path, char *new_path) {
    return -E_UNIMP;
}

// unimplement
// 未实现
int
vfs_readlink(char *path, struct iobuf *iob) {
    return -E_UNIMP;
}

// unimplement
// 未实现
int
vfs_mkdir(char *path){
    return -E_UNIMP;
}
