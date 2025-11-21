#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

#ifndef __ASSEMBLER__
#include <defs.h>
#endif /* !__ASSEMBLER__ */

/*
 * 线性地址 la 的结构如下：
 *
 * +--------10------+-------10-------+---------12----------+
 * | 页目录索引     | 页表索引       | 页内偏移            |
 * | Page Directory | Page Table     | Offset within Page  |
 * |      Index     | Index          |                     |
 * +----------------+----------------+---------------------+
 *  \--- PDX(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/
 *  \----------- PPN(la) -----------/
 *
 * PDX、PTX、PGOFF、PPN 这些宏用于分解线性地址。
 * 如果需要通过 PDX(la)、PTX(la)、PGOFF(la) 构造线性地址 la，
 * 可以使用 PGADDR(PDX(la), PTX(la), PGOFF(la)) 宏。
 *
 * RISC-V 架构使用 32 位虚拟地址访问 34 位物理地址。
 * Sv32 页表项结构如下：
 * +---------12----------+--------10-------+---2----+-------8-------+
 * |     PPN[1]          |   PPN[0]        |保留位  |D|A|G|U|X|W|R|V|
 * +---------12----------+-----------------+--------+---------------+
 * 其中 PPN 表示物理页号，后面的各个位表示不同的权限和状态。
 */

// 页目录第一级索引（高 9 位）
#define PDX1(la) ((((uintptr_t)(la)) >> PDX1SHIFT) & 0x1FF)
// 页目录第二级索引（次高 9 位）
#define PDX0(la) ((((uintptr_t)(la)) >> PDX0SHIFT) & 0x1FF)

// 页表索引（中间 9 位）
#define PTX(la) ((((uintptr_t)(la)) >> PTXSHIFT) & 0x1FF)

// 物理页号字段（去掉页内偏移后的高位）
#define PPN(la) (((uintptr_t)(la)) >> PTXSHIFT)

// 页内偏移（低 12 位）
#define PGOFF(la) (((uintptr_t)(la)) & 0xFFF)

// 通过页目录索引、页表索引和页内偏移构造线性地址
#define PGADDR(d1, d0, t, o) ((uintptr_t)((d1) << PDX1SHIFT |(d0) << PDX0SHIFT | (t) << PTXSHIFT | (o)))

// 从页表项或页目录项中获取物理地址（去掉低 10 位标志位后左移）
#define PTE_ADDR(pte)   (((uintptr_t)(pte) & ~0x3FF) << (PTXSHIFT - PTE_PPN_SHIFT))
#define PDE_ADDR(pde)   PTE_ADDR(pde)

/* 页目录和页表相关常量 */
// 每个页目录包含的项数（512 项）
#define NPDEENTRY       512
// 每个页表包含的项数（512 项）
#define NPTEENTRY       512

// 每页大小（4096 字节）
#define PGSIZE          4096
// 页大小的对数（log2(PGSIZE) = 12）
#define PGSHIFT         12
// 一个页目录项映射的字节数（4096 * 512 = 2MB）
#define PTSIZE          (PGSIZE * NPTEENTRY)
// 页目录项映射字节数的对数（log2(PTSIZE) = 21）
#define PTSHIFT         21

// 页表索引在地址中的偏移（12 位）
#define PTXSHIFT        12
// 页目录第二级索引在地址中的偏移（21 位）
#define PDX0SHIFT       21
// 页目录第一级索引在地址中的偏移（30 位）
#define PDX1SHIFT		30
// 物理页号在物理地址中的偏移（10 位）
#define PTE_PPN_SHIFT   10

// 页表项（PTE）各字段含义
#define PTE_V     0x001 // 有效位（Valid）
#define PTE_R     0x002 // 读权限（Read）
#define PTE_W     0x004 // 写权限（Write）
#define PTE_X     0x008 // 执行权限（Execute）
#define PTE_U     0x010 // 用户态访问（User）
#define PTE_G     0x020 // 全局映射（Global）
#define PTE_A     0x040 // 已访问（Accessed）
#define PTE_D     0x080 // 已修改（Dirty）
#define PTE_SOFT  0x300 // 软件保留位（Reserved for Software）

// 常用权限组合宏
#define PAGE_TABLE_DIR (PTE_V) // 仅有效位
#define READ_ONLY (PTE_R | PTE_V) // 只读
#define READ_WRITE (PTE_R | PTE_W | PTE_V) // 读写
#define EXEC_ONLY (PTE_X | PTE_V) // 只执行
#define READ_EXEC (PTE_R | PTE_X | PTE_V) // 读+执行
#define READ_WRITE_EXEC (PTE_R | PTE_W | PTE_X | PTE_V) // 读写执行

#define PTE_USER (PTE_R | PTE_W | PTE_X | PTE_U | PTE_V) // 用户态读写执行有效

#endif /* !__KERN_MM_MMU_H__ */
