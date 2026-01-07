/*
 * inode.h 定义了抽象的索引节点（inode）结构体和相关操作。
 * Inode 是文件（包括设备文件）的抽象表示，它提供了一个接口，
 * 使得内核的文件系统无关代码能够与多种底层文件系统代码进行交互。
 */

#ifndef __KERN_FS_VFS_INODE_H__
#define __KERN_FS_VFS_INODE_H__

#include <defs.h>
#include <dev.h>
#include <sfs.h>
#include <atomic.h>
#include <assert.h>

struct stat;
struct iobuf;

/*
 * 结构体 inode 是文件的抽象表示。
 *
 * 它是一个接口，允许内核的文件系统无关代码有效地与多组文件系统代码交互。
 */

/*
 * 抽象的低级文件。
 *
 * 注意：in_info 是特定于文件系统的数据，in_type 是 inode 类型
 *
 * open_count 由 vfs_open() 和 vfs_close() 使用 VOP_INCOPEN 和 VOP_DECOPEN 进行管理。
 * VFS 层之上的代码不应该担心它。
 */
struct inode {
    union {
        struct device __device_info;
        struct sfs_inode __sfs_inode_info;
    } in_info;
    enum {
        inode_type_device_info = 0x1234,
        inode_type_sfs_inode_info,
    } in_type;
    int ref_count;
    int open_count;
    struct fs *in_fs;
    const struct inode_ops *in_ops;
};

#define __in_type(type)                                             inode_type_##type##_info

#define check_inode_type(node, type)                                ((node)->in_type == __in_type(type))

#define __vop_info(node, type)                                      \
    ({                                                              \
        struct inode *__node = (node);                              \
        assert(__node != NULL && check_inode_type(__node, type));   \
        &(__node->in_info.__##type##_info);                         \
     })

#define vop_info(node, type)                                        __vop_info(node, type)

#define info2node(info, type)                                       \
    to_struct((info), struct inode, in_info.__##type##_info)

struct inode *__alloc_inode(int type);

#define alloc_inode(type)                                           __alloc_inode(__in_type(type))

#define MAX_INODE_COUNT                     0x10000

int inode_ref_inc(struct inode *node);
int inode_ref_dec(struct inode *node);
int inode_open_inc(struct inode *node);
int inode_open_dec(struct inode *node);

void inode_init(struct inode *node, const struct inode_ops *ops, struct fs *fs);
void inode_kill(struct inode *node);

#define VOP_MAGIC                           0x8c4ba476

/*
 * 对 inode 的抽象操作。
 *
 * 这些操作以 VOP_FOO(inode, args) 的形式使用，它们是宏，
 * 扩展为 inode->inode_ops->vop_foo(inode, args)。操作 "foo" 包括：
 *
 *    vop_open        - 在 open() 文件时调用。可用于拒绝非法或不期望的打开模式。
 *                      注意，可以在不实际打开文件的情况下执行各种操作。
 *                      inode 不需要查看 O_CREAT、O_EXCL 或 O_TRUNC，
 *                      因为这些是在 VFS 层处理的。
 *
 *                      VOP_EACHOPEN 不应从 VFS 层之上直接调用 - 使用 vfs_open() 打开 inode。
 *                      这维护了打开计数，以便可以在正确的时间调用 VOP_LASTCLOSE。
 *
 *    vop_close       - 在文件的 *最后一次* close() 时调用。
 *
 *                      VOP_LASTCLOSE 不应从 VFS 层之上直接调用 - 使用 vfs_close() 关闭
 *                      用 vfs_open() 打开的 inode。
 *
 *    vop_reclaim     - 当 inode 不再使用时调用。注意，这可能在 vop_lastclose 被调用后很久才发生。
 *
 *****************************************
 *
 *    vop_read        - 将数据从文件读入 uio，偏移量在 uio 中指定，
 *                      更新 uio_resid 以反映读取的数量，并更新 uio_offset 以匹配。
 *                      不允许在目录或符号链接上使用。
 *
 *    vop_getdirentry - 将单个文件名从目录读入 uio，根据 uio 中的 offset 字段选择名称，
 *                      并更新该字段。
 *                      与普通文件上的 I/O 不同，offset 字段的值不在文件系统之外解释，
 *                      因此不必是字节计数。但是，uio_resid 字段应该以正常方式处理。
 *                      在非目录对象上，返回 ENOTDIR。
 *
 *    vop_write       - 将数据从 uio 写入文件，偏移量在 uio 中指定，
 *                      更新 uio_resid 以反映写入的数量，并更新 uio_offset 以匹配。
 *                      不允许在目录或符号链接上使用。
 *
 *    vop_ioctl       - 使用数据 DATA 对文件执行 ioctl 操作 OP。
 *                      数据的解释主要取决于每个 ioctl。
 *
 *    vop_fstat       - 返回有关文件的信息。指针是指向 struct stat 的指针；参见 stat.h。
 *
 *    vop_gettype     - 返回文件类型。文件类型的值在 sfs.h 中。
 *
 *    vop_tryseek     - 检查查找到文件内的指定位置是否合法。（例如，所有查找
 *                      在串口设备上都是非法的，并且在大小固定的文件上查找超过 EOF
 *                      可能也是非法的。）
 *
 *    vop_fsync       - 强制与此文件关联的所有脏缓冲区写入稳定存储。
 *
 *    vop_truncate    - 强制将文件大小设置为传入的长度，丢弃任何多余的块。
 *
 *    vop_namefile    - 计算文件相对于文件系统根目录的路径名，并复制到指定的 io 缓冲区。
 *                      不需要在非目录对象上工作。
 *
 *****************************************
 *
 *    vop_creat       - 在传递的目录 DIR 中创建一个名为 NAME 的常规文件。
 *                      如果布尔值 EXCL 为 true，如果文件已存在则失败；否则，
 *                      如果有现有文件则使用它。按照 vop_lookup 返回文件的 inode。
 *
 *****************************************
 *
 *    vop_lookup      - 解析相对于传递的目录 DIR 的 PATHNAME，并返回它引用的文件的 inode。
 *                      可能会破坏 PATHNAME。应该增加返回的 inode 的引用计数。
 */
// 索引节点操作表
struct inode_ops {
    unsigned long vop_magic;   // 魔数，验证结构体有效性
    int (*vop_open)(struct inode *node, uint32_t open_flags);
    int (*vop_close)(struct inode *node);
    int (*vop_read)(struct inode *node, struct iobuf *iob);
    int (*vop_write)(struct inode *node, struct iobuf *iob);
    int (*vop_fstat)(struct inode *node, struct stat *stat);
    int (*vop_fsync)(struct inode *node);
    int (*vop_namefile)(struct inode *node, struct iobuf *iob);
    int (*vop_getdirentry)(struct inode *node, struct iobuf *iob);
    int (*vop_reclaim)(struct inode *node);
    int (*vop_gettype)(struct inode *node, uint32_t *type_store);
    int (*vop_tryseek)(struct inode *node, off_t pos);
    int (*vop_truncate)(struct inode *node, off_t len);
    int (*vop_create)(struct inode *node, const char *name, bool excl, struct inode **node_store);
    int (*vop_lookup)(struct inode *node, char *path, struct inode **node_store);
    int (*vop_ioctl)(struct inode *node, int op, void *data);
};

/*
 * 一致性检查
 */
void inode_check(struct inode *node, const char *opstr);

#define __vop_op(node, sym)                                                                         \
    ({                                                                                              \
        struct inode *__node = (node);                                                              \
        assert(__node != NULL && __node->in_ops != NULL && __node->in_ops->vop_##sym != NULL);      \
        inode_check(__node, #sym);                                                                  \
        __node->in_ops->vop_##sym;                                                                  \
     })

#define vop_open(node, open_flags)                                  (__vop_op(node, open)(node, open_flags))
#define vop_close(node)                                             (__vop_op(node, close)(node))
#define vop_read(node, iob)                                         (__vop_op(node, read)(node, iob))
#define vop_write(node, iob)                                        (__vop_op(node, write)(node, iob))
#define vop_fstat(node, stat)                                       (__vop_op(node, fstat)(node, stat))
#define vop_fsync(node)                                             (__vop_op(node, fsync)(node))
#define vop_namefile(node, iob)                                     (__vop_op(node, namefile)(node, iob))
#define vop_getdirentry(node, iob)                                  (__vop_op(node, getdirentry)(node, iob))
#define vop_reclaim(node)                                           (__vop_op(node, reclaim)(node))
#define vop_ioctl(node, op, data)                                   (__vop_op(node, ioctl)(node, op, data))
#define vop_gettype(node, type_store)                               (__vop_op(node, gettype)(node, type_store))
#define vop_tryseek(node, pos)                                      (__vop_op(node, tryseek)(node, pos))
#define vop_truncate(node, len)                                     (__vop_op(node, truncate)(node, len))
#define vop_create(node, name, excl, node_store)                    (__vop_op(node, create)(node, name, excl, node_store))
#define vop_lookup(node, path, node_store)                          (__vop_op(node, lookup)(node, path, node_store))


#define vop_fs(node)                                                ((node)->in_fs)
#define vop_init(node, ops, fs)                                     inode_init(node, ops, fs)
#define vop_kill(node)                                              inode_kill(node)

/*
 * 引用计数操作（在文件系统层之上处理）
 */
#define vop_ref_inc(node)                                           inode_ref_inc(node)
#define vop_ref_dec(node)                                           inode_ref_dec(node)
/*
 * 打开计数操作（在文件系统层之上处理）
 *
 * VOP_INCOPEN 由 vfs_open 调用。VOP_DECOPEN 由 vfs_close 调用。
 * 这两个都不应该需要在 vfs 层之上被调用。
 */
#define vop_open_inc(node)                                          inode_open_inc(node)
#define vop_open_dec(node)                                          inode_open_dec(node)


static inline int
inode_ref_count(struct inode *node) {
    return node->ref_count;
}

static inline int
inode_open_count(struct inode *node) {
    return node->open_count;
}

#endif /* !__KERN_FS_VFS_INODE_H__ */

