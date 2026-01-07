#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <atomic.h>
#include <vfs.h>
#include <inode.h>
#include <error.h>
#include <assert.h>
#include <kmalloc.h>

/* *
 * __alloc_inode - 分配一个inode结构并初始化in_type
 * */
struct inode *
__alloc_inode(int type) {
    struct inode *node;
    if ((node = kmalloc(sizeof(struct inode))) != NULL) {
        node->in_type = type; // 设置inode类型
    }
    return node;
}

/* *
 * inode_init - 初始化inode结构
 * 由vop_init调用
 * */
void
inode_init(struct inode *node, const struct inode_ops *ops, struct fs *fs) {
    node->ref_count = 0; // 引用计数初始化为0
    node->open_count = 0; // 打开计数初始化为0
    node->in_ops = ops, node->in_fs = fs; // 设置操作函数表和所属文件系统
    vop_ref_inc(node); // 增加引用计数
}

/* *
 * inode_kill - 销毁inode结构
 * 由vop_kill调用
 * */
void
inode_kill(struct inode *node) {
    assert(inode_ref_count(node) == 0); // 确保引用计数为0
    assert(inode_open_count(node) == 0); // 确保打开计数为0
    kfree(node); // 释放内存
}

/* *
 * inode_ref_inc - 增加引用计数
 * 由vop_ref_inc调用
 * */
int
inode_ref_inc(struct inode *node) {
    node->ref_count += 1; // 引用计数加1
    return node->ref_count;
}

/* *
 * inode_ref_dec - 减少引用计数
 * 由vop_ref_dec调用
 * 如果引用计数降为0，则调用vop_reclaim
 * */
int
inode_ref_dec(struct inode *node) {
    assert(inode_ref_count(node) > 0);
    int ref_count, ret;
    node->ref_count-= 1; // 引用计数减1
    ref_count = node->ref_count;
    if (ref_count == 0) { // 如果引用计数为0
        if ((ret = vop_reclaim(node)) != 0 && ret != -E_BUSY) { // 回收inode
            cprintf("vfs: warning: vop_reclaim: %e.\n", ret);
        }
    }
    return ref_count;
}

/* *
 * inode_open_inc - 增加打开计数
 * 由vop_open_inc调用
 * */
int
inode_open_inc(struct inode *node) {
    node->open_count += 1; // 打开计数加1
    return node->open_count;
}

/* *
 * inode_open_dec - 减少打开计数
 * 由vop_open_dec调用
 * 如果打开计数降为0，则调用vop_close
 * */
int
inode_open_dec(struct inode *node) {
    assert(inode_open_count(node) > 0);
    int open_count, ret;
    node->open_count -= 1; // 打开计数减1
    open_count = node->open_count;
    if (open_count == 0) { // 如果打开计数为0
        if ((ret = vop_close(node)) != 0) { // 关闭inode
            cprintf("vfs: warning: vop_close: %e.\n", ret);
        }
    }
    return open_count;
}

/* *
 * inode_check - 检查各种事项是否有效
 * 在所有vop_*调用之前调用
 * */
void
inode_check(struct inode *node, const char *opstr) {
    assert(node != NULL && node->in_ops != NULL); // 确保node和操作表不为空
    assert(node->in_ops->vop_magic == VOP_MAGIC); // 检查magic number
    int ref_count = inode_ref_count(node), open_count = inode_open_count(node);
    assert(ref_count >= open_count && open_count >= 0); // 确保引用计数>=打开计数且打开计数>=0
    assert(ref_count < MAX_INODE_COUNT && open_count < MAX_INODE_COUNT); // 确保计数未溢出
}

