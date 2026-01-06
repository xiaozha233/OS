#include <defs.h>
#include <string.h>
#include <stat.h>
#include <dev.h>
#include <inode.h>
#include <unistd.h>
#include <error.h>

/*
 * dev_open - Called for each open().
 */
// dev_open - 每次open()都会调用
static int
dev_open(struct inode *node, uint32_t open_flags) {
    if (open_flags & (O_CREAT | O_TRUNC | O_EXCL | O_APPEND)) { // 检查打开标志，设备文件不支持创建、截断或追加
        return -E_INVAL; // 返回无效参数错误
    }
    struct device *dev = vop_info(node, device); // 获取inode对应的device结构
    return dop_open(dev, open_flags); // 调用设备特定的open操作
}

/*
 * dev_close - Called on the last close(). Just pass through.
 */
// dev_close - 最后一个close()时调用。直接传递给设备
static int
dev_close(struct inode *node) {
    struct device *dev = vop_info(node, device); // 获取设备结构
    return dop_close(dev); // 调用设备特定的close操作
}

/*
 * dev_read -Called for read. Hand off to iobuf.
 */
// dev_read - 读取操作。交给iobuf处理
static int
dev_read(struct inode *node, struct iobuf *iob) {
    struct device *dev = vop_info(node, device); // 获取设备结构
    return dop_io(dev, iob, 0); // 调用设备IO读取接口（0表示读）
}

/*
 * dev_write -Called for write. Hand off to iobuf.
 */
// dev_write - 写入操作。交给iobuf处理
static int
dev_write(struct inode *node, struct iobuf *iob) {
    struct device *dev = vop_info(node, device); // 获取设备结构
    return dop_io(dev, iob, 1); // 调用设备IO写入接口（1表示写）
}

/*
 * dev_ioctl - Called for ioctl(). Just pass through.
 */
// dev_ioctl - ioctl操作。直接传递
static int
dev_ioctl(struct inode *node, int op, void *data) {
    struct device *dev = vop_info(node, device); // 获取设备结构
    return dop_ioctl(dev, op, data); // 调用设备特定的ioctl操作
}

/*
 * dev_fstat - Called for stat().
 *             Set the type and the size (block devices only).
 *             The link count for a device is always 1.
 */
// dev_fstat - 获取状态信息。
//             设置类型和大小（仅限块设备）。
//             设备的链接数始终为1。
static int
dev_fstat(struct inode *node, struct stat *stat) {
    int ret;
    memset(stat, 0, sizeof(struct stat)); // 清零stat结构
    if ((ret = vop_gettype(node, &(stat->st_mode))) != 0) { // 获取设备类型并设置模式
        return ret;
    }
    struct device *dev = vop_info(node, device); // 获取设备结构
    stat->st_nlinks = 1; // 链接数设为1
    stat->st_blocks = dev->d_blocks; // 设置块数
    stat->st_size = stat->st_blocks * dev->d_blocksize; // 计算并设置总大小
    return 0; // 返回成功
}

/*
 * dev_gettype - Return the type. A device is a "block device" if it has a known
 *               length. A device that generates data in a stream is a "character
 *               device".
 */
// dev_gettype - 返回类型。如果有已知长度，则是"块设备"。
//               流式生成数据的设备是"字符设备"。
static int
dev_gettype(struct inode *node, uint32_t *type_store) {
    struct device *dev = vop_info(node, device); // 获取设备结构
    *type_store = (dev->d_blocks > 0) ? S_IFBLK : S_IFCHR; // 根据块数是否大于0判断是块设备还是字符设备
    return 0;
}

/*
 * dev_tryseek - Attempt a seek.
 *               For block devices, require block alignment.
 *               For character devices, prohibit seeking entirely.
 */
// dev_tryseek - 尝试定位（seek）。
//               对于块设备，要求块对齐。
//               对于字符设备，完全禁止定位。
static int
dev_tryseek(struct inode *node, off_t pos) {
    struct device *dev = vop_info(node, device); // 获取设备结构
    if (dev->d_blocks > 0) { // 如果是块设备
        if ((pos % dev->d_blocksize) == 0) { // 检查位置是否块对齐
            if (pos >= 0 && pos < dev->d_blocks * dev->d_blocksize) { // 检查位置是否在有效范围内
                return 0; // 成功
            }
        }
    }
    return -E_INVAL; // 如果不满足条件，返回无效参数错误
}

/*
 * dev_lookup - Name lookup.
 *
 * One interesting feature of device:name pathname syntax is that you
 * can implement pathnames on arbitrary devices. For instance, if you
 * had a graphics device that supported multiple resolutions (which we
 * don't), you might arrange things so that you could open it with
 * pathnames like "video:800x600/24bpp" in order to select the operating
 * mode.
 *
 * However, we have no support for this in the base system.
 */
// dev_lookup - 名称查找。
//
// device:name 路径名语法的一个有趣特性是你可以
// 在任意设备上实现路径名。例如，如果你有一个支持多种分辨率的图形设备
// （虽然我们没有），你可以安排通过类似 "video:800x600/24bpp" 的路径名
// 来打开它，以选择操作模式。
//
// 但是，基础系统中不支持此功能。
static int
dev_lookup(struct inode *node, char *path, struct inode **node_store) {
    if (*path != '\0') { // 如果路径不为空（即试图查找设备下的子路径）
        return -E_NOENT; // 返回不存在错误
    }
    vop_ref_inc(node); // 增加inode引用计数
    *node_store = node; // 返回当前inode
    return 0;
}

/*
 * Function table for device inodes.
 */
// 设备inode的函数表
static const struct inode_ops dev_node_ops = {
    .vop_magic                      = VOP_MAGIC, // 魔数
    .vop_open                       = dev_open, // 打开
    .vop_close                      = dev_close, // 关闭
    .vop_read                       = dev_read, // 读取
    .vop_write                      = dev_write, // 写入
    .vop_fstat                      = dev_fstat, // 获取状态
    .vop_ioctl                      = dev_ioctl, // ioctl
    .vop_gettype                    = dev_gettype, // 获取类型
    .vop_tryseek                    = dev_tryseek, // seek
    .vop_lookup                     = dev_lookup, // 查找
};

#define init_device(x)                                  \
    do {                                                \
        extern void dev_init_##x(void);                 \
        dev_init_##x();                                 \
    } while (0)

/* dev_init - Initialization functions for builtin vfs-level devices. */
// dev_init - 内置VFS级设备的初始化函数
void
dev_init(void) {
   // init_device(null);
    init_device(stdin); // 初始化标准输入
    init_device(stdout); // 初始化标准输出
    init_device(disk0); // 初始化磁盘0
}
/* dev_create_inode - Create inode for a vfs-level device. */
// dev_create_inode - 为VFS级设备创建inode
struct inode *
dev_create_inode(void) {
    struct inode *node;
    if ((node = alloc_inode(device)) != NULL) { // 分配inode
        vop_init(node, &dev_node_ops, NULL); // 初始化inode操作
    }
    return node; // 返回inode
}

