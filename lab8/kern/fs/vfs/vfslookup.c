#include <defs.h>
#include <string.h>
#include <vfs.h>
#include <inode.h>
#include <error.h>
#include <assert.h>

/*
 * get_device- Common code to pull the device name, if any, off the front of a
 *             path and choose the inode to begin the name lookup relative to.
 */
/*
 * get_device - 通用代码，用于从路径开头提取设备名称（如果有），
 *              并选择开始名称查找的相对inode。
 */
static int
get_device(char *path, char **subpath, struct inode **node_store) {
    int i, slash = -1, colon = -1;
    for (i = 0; path[i] != '\0'; i ++) {
        if (path[i] == ':') { colon = i; break; } // 查找冒号
        if (path[i] == '/') { slash = i; break; } // 查找斜杠
    }
    if (colon < 0 && slash != 0) {
        /* *
         * No colon before a slash, so no device name specified, and the slash isn't leading
         * or is also absent, so this is a relative path or just a bare filename. Start from
         * the current directory, and use the whole thing as the subpath.
         * */
        /* *
         * 斜杠前没有冒号，因此未指定设备名称，且斜杠不是开头
         * 或者也不存在，所以这是一个相对路径或仅仅是一个裸文件名。
         * 从当前目录开始，并使用整个字符串作为子路径。
         * */
        *subpath = path;
        return vfs_get_curdir(node_store);
    }
    if (colon > 0) {
        /* device:path - get root of device's filesystem */
        /* device:path - 获取设备文件系统的根节点 */
        path[colon] = '\0';

        /* device:/path - skip slash, treat as device:path */
        /* device:/path - 跳过斜杠，视为 device:path */
        while (path[++ colon] == '/');
        *subpath = path + colon;
        return vfs_get_root(path, node_store);
    }

    /* *
     * we have either /path or :path
     * /path is a path relative to the root of the "boot filesystem"
     * :path is a path relative to the root of the current filesystem
     * */
    /* *
     * 我们有 /path 或 :path
     * /path 是相对于“启动文件系统”根目录的路径
     * :path 是相对于当前文件系统根目录的路径
     * */
    int ret;
    if (*path == '/') {
        if ((ret = vfs_get_bootfs(node_store)) != 0) {
            return ret;
        }
    }
    else {
        assert(*path == ':');
        struct inode *node;
        if ((ret = vfs_get_curdir(&node)) != 0) {
            return ret;
        }
        /* The current directory may not be a device, so it must have a fs. */
        /* 当前目录可能不是设备，所以它必须有一个fs。 */
        assert(node->in_fs != NULL);
        *node_store = fsop_get_root(node->in_fs);
        vop_ref_dec(node);
    }

    /* ///... or :/... */
    while (*(++ path) == '/');
    *subpath = path;
    return 0;
}

/*
 * vfs_lookup - get the inode according to the path filename
 */
/*
 * vfs_lookup - 根据路径文件名获取inode
 */
int
vfs_lookup(char *path, struct inode **node_store) {
    int ret;
    struct inode *node;
    if ((ret = get_device(path, &path, &node)) != 0) { // 获取起始inode
        return ret;
    }
    if (*path != '\0') {
        ret = vop_lookup(node, path, node_store); // 在起始inode中查找路径
        vop_ref_dec(node); // 减少起始inode的引用计数
        return ret;
    }
    *node_store = node;
    return 0;
}

/*
 * vfs_lookup_parent - Name-to-vnode translation.
 *  (In BSD, both of these are subsumed by namei().)
 */
/*
 * vfs_lookup_parent - 名称到vnode的转换。
 *  (在BSD中，这两个都包含在namei()中。)
 */
int
vfs_lookup_parent(char *path, struct inode **node_store, char **endp){
    int ret;
    struct inode *node;
    if ((ret = get_device(path, &path, &node)) != 0) { // 获取起始inode
        return ret;
    }
    *endp = path;
    *node_store = node;
    return 0;
}
