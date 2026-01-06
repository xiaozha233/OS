#include <defs.h>
#include <mmu.h>
#include <sem.h>
#include <ide.h>
#include <inode.h>
#include <kmalloc.h>
#include <dev.h>
#include <vfs.h>
#include <iobuf.h>
#include <error.h>
#include <assert.h>

#define DISK0_BLKSIZE                   PGSIZE // 磁盘0的块大小为一页大小
#define DISK0_BUFSIZE                   (4 * DISK0_BLKSIZE) // 磁盘0的缓冲区大小，4个块
#define DISK0_BLK_NSECT                 (DISK0_BLKSIZE / SECTSIZE) // 每个块包含的扇区数

static char *disk0_buffer; // 磁盘0的内部缓冲区
static semaphore_t disk0_sem; // 磁盘0的互斥信号量

// lock_disk0 - 获取磁盘锁
static void
lock_disk0(void) {
    down(&(disk0_sem)); // P操作获取信号量
}

// unlock_disk0 - 释放磁盘锁
static void
unlock_disk0(void) {
    up(&(disk0_sem)); // V操作释放信号量
}

// disk0_open - 打开磁盘0（空操作）
static int
disk0_open(struct device *dev, uint32_t open_flags) {
    return 0; // 总是成功
}

// disk0_close - 关闭磁盘0（空操作）
static int
disk0_close(struct device *dev) {
    return 0; // 总是成功
}

// disk0_read_blks_nolock - 读磁盘块（无需锁，由调用者保证）
static void
disk0_read_blks_nolock(uint32_t blkno, uint32_t nblks) {
    int ret;
    uint32_t sectno = blkno * DISK0_BLK_NSECT, nsecs = nblks * DISK0_BLK_NSECT; // 计算起始扇区号和总扇区数
    if ((ret = ide_read_secs(DISK0_DEV_NO, sectno, disk0_buffer, nsecs)) != 0) { // 调用IDE驱动读取扇区到缓冲区
        panic("disk0: read blkno = %d (sectno = %d), nblks = %d (nsecs = %d): 0x%08x.\n",
                blkno, sectno, nblks, nsecs, ret); // 如果出错则panic
    }
}

// disk0_write_blks_nolock - 写磁盘块（无需锁，由调用者保证）
static void
disk0_write_blks_nolock(uint32_t blkno, uint32_t nblks) {
    int ret;
    uint32_t sectno = blkno * DISK0_BLK_NSECT, nsecs = nblks * DISK0_BLK_NSECT; // 计算起始扇区号和总扇区数
    if ((ret = ide_write_secs(DISK0_DEV_NO, sectno, disk0_buffer, nsecs)) != 0) { // 调用IDE驱动将缓冲区数据写入扇区
        panic("disk0: write blkno = %d (sectno = %d), nblks = %d (nsecs = %d): 0x%08x.\n",
                blkno, sectno, nblks, nsecs, ret); // 如果出错则panic
    }
}

// disk0_io - 磁盘IO操作主函数
static int
disk0_io(struct device *dev, struct iobuf *iob, bool write) {
    off_t offset = iob->io_offset; // 获取IO偏移量
    size_t resid = iob->io_resid; // 获取剩余要传输的字节数
    uint32_t blkno = offset / DISK0_BLKSIZE; // 计算起始块号
    uint32_t nblks = resid / DISK0_BLKSIZE; // 计算要操作的块数

    /* don't allow I/O that isn't block-aligned */
    if ((offset % DISK0_BLKSIZE) != 0 || (resid % DISK0_BLKSIZE) != 0) { // 检查偏移和长度是否块对齐
        return -E_INVAL; // 如果不对齐，返回无效参数错误
    }

    /* don't allow I/O past the end of disk0 */
    if (blkno + nblks > dev->d_blocks) { // 检查是否越界
        return -E_INVAL; // 越界则返回错误
    }

    /* read/write nothing ? */
    if (nblks == 0) { // 如果操作块数为0
        return 0; // 直接返回成功
    }

    lock_disk0(); // 加锁
    while (resid != 0) { // 循环直到所有数据传输完毕
        size_t copied, alen = DISK0_BUFSIZE; // 设置单次最大传输量为缓冲区大小
        if (write) { // 如果是写操作
            iobuf_move(iob, disk0_buffer, alen, 0, &copied); // 从iobuf复制数据到disk0_buffer
            assert(copied != 0 && copied <= resid && copied % DISK0_BLKSIZE == 0); // 断言检查复制量合法性
            nblks = copied / DISK0_BLKSIZE; // 计算实际复制的块数
            disk0_write_blks_nolock(blkno, nblks); // 将disk0_buffer数据写入磁盘
        }
        else { // 如果是读操作
            if (alen > resid) { // 如果缓冲区大小大于剩余数据量
                alen = resid; // 调整读取量
            }
            nblks = alen / DISK0_BLKSIZE; // 计算要读取的块数
            disk0_read_blks_nolock(blkno, nblks); // 从磁盘读取数据到disk0_buffer
            iobuf_move(iob, disk0_buffer, alen, 1, &copied); // 将disk0_buffer数据复制到iobuf
            assert(copied == alen && copied % DISK0_BLKSIZE == 0); // 断言检查
        }
        resid -= copied, blkno += nblks; // 更新剩余字节数和当前块号
    }
    unlock_disk0(); // 解锁
    return 0; // 返回成功
}

// disk0_ioctl - ioctl操作（未实现）
static int
disk0_ioctl(struct device *dev, int op, void *data) {
    return -E_UNIMP; // 返回未实现错误
}

// disk0_device_init - 初始化磁盘0设备结构
static void
disk0_device_init(struct device *dev) {
    static_assert(DISK0_BLKSIZE % SECTSIZE == 0); // 静态断言：块大小必须是扇区大小的倍数
    if (!ide_device_valid(DISK0_DEV_NO)) { // 检查IDE设备是否有效
        panic("disk0 device isn't available.\n");
    }
    dev->d_blocks = ide_device_size(DISK0_DEV_NO) / DISK0_BLK_NSECT; // 设置设备总块数
    dev->d_blocksize = DISK0_BLKSIZE; // 设置设备块大小
    dev->d_open = disk0_open; // 设置open函数
    dev->d_close = disk0_close; // 设置close函数
    dev->d_io = disk0_io; // 设置io函数
    dev->d_ioctl = disk0_ioctl; // 设置ioctl函数
    sem_init(&(disk0_sem), 1); // 初始化信号量为1

    static_assert(DISK0_BUFSIZE % DISK0_BLKSIZE == 0); // 静态断言：缓冲区大小是块大小倍数
    if ((disk0_buffer = kmalloc(DISK0_BUFSIZE)) == NULL) { // 分配内核缓冲区
        panic("disk0 alloc buffer failed.\n");
    }
}

// dev_init_disk0 - 初始化整个磁盘0设备（创建inode并注册）
void
dev_init_disk0(void) {
    struct inode *node;
    if ((node = dev_create_inode()) == NULL) { // 创建设备文件的inode
        panic("disk0: dev_create_node.\n");
    }
    disk0_device_init(vop_info(node, device)); // 初始化设备特定的数据结构

    int ret;
    if ((ret = vfs_add_dev("disk0", node, 1)) != 0) { // 将设备添加到VFS，名称为"disk0"
        panic("disk0: vfs_add_dev: %e.\n", ret);
    }
}

