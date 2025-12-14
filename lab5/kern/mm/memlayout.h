#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

/* 本文件集中定义内核的内存布局与物理页框描述结构，便于内核与用户态共同遵循同一映射规则。 */

/* *
 * Virtual memory map / 虚拟内存布局示意 (高地址在上)：                      Permissions
 *                                                                      kernel/user
 *
 *     4G ------------------> +---------------------------------+
 *                            |                                 |
 *                            |         Empty Memory (*)        |   未使用区域，可按需映射
 *                            |                                 |
 *                            +---------------------------------+ 0xFB000000
 *                            |   Cur. Page Table (Kern, RW)    | RW/-- PTSIZE
 *     VPT -----------------> +---------------------------------+ 0xFAC00000
 *                            |        Invalid Memory (*)       | --/-- 永不映射
 *     KERNTOP -------------> +---------------------------------+ 0xF8000000
 *                            |                                 |
 *                            |    Remapped Physical Memory     | RW/-- KMEMSIZE
 *                            |            物理内存线性映射区    |
 *     KERNBASE ------------> +---------------------------------+ 0xC0000000
 *                            |        Invalid Memory (*)       | --/-- 永不映射
 *     USERTOP -------------> +---------------------------------+ 0xB0000000
 *                            |           User stack            |   用户栈向下生长
 *                            +---------------------------------+
 *                            |                                 |
 *                            :                                 :
 *                            |         ~~~~~~~~~~~~~~~~        |
 *                            :                                 :
 *                            |                                 |
 *                            ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                            |       User Program & Heap       |   用户代码/堆
 *     UTEXT ---------------> +---------------------------------+ 0x00800000
 *                            |        Invalid Memory (*)       | --/--
 *                            |  - - - - - - - - - - - - - - -  |
 *                            |    User STAB Data (optional)    |   可选符号表
 *     USERBASE, USTAB------> +---------------------------------+ 0x00200000
 *                            |        Invalid Memory (*)       | --/--
 *     0 -------------------> +---------------------------------+ 0x00000000
 * (*) Note: kernel 永不映射 Invalid Memory；Empty Memory 默认不映射，但用户可按需映射。
 * */

/* All physical memory mapped at this address */
#define KERNBASE 0xFFFFFFFFC0200000      // 内核线性映射起始虚拟地址，映射物理0
#define KMEMSIZE 0x7E00000               // 最大物理内存大小（线性映射窗口大小）
#define KERNTOP (KERNBASE + KMEMSIZE)    // 线性映射上界（开区间右端）

#define PHYSICAL_MEMORY_OFFSET 0xFFFFFFFF40000000 // 物理地址到内核虚拟地址的偏移
/* *
 * Virtual page table. Entry PDX[VPT] in the PD (Page Directory) contains
 * a pointer to the page directory itself, thereby turning the PD into a page
 * table, which maps all the PTEs (Page Table Entry) containing the page mappings
 * for the entire virtual address space into that 4 Meg region starting at VPT.
 * */

#define KSTACKPAGE 2                     // 每个内核线程的栈页数
#define KSTACKSIZE (KSTACKPAGE * PGSIZE) // 内核栈大小

#define USERTOP 0x80000000               // 用户空间最高虚拟地址（栈顶）
#define USTACKTOP USERTOP
#define USTACKPAGE 256                   // 用户栈的页数（向下增长）
#define USTACKSIZE (USTACKPAGE * PGSIZE) // 用户栈总大小

#define USERBASE 0x00200000              // 用户地址空间起点，前面保留不用
#define UTEXT 0x00800000                 // 用户代码与初始堆起始地址
#define USTAB USERBASE                   // 可选的用户符号表位置

#define USER_ACCESS(start, end) \
    (USERBASE <= (start) && (start) < (end) && (end) <= USERTOP) // 校验区间是否完全落在用户空间

#define KERN_ACCESS(start, end) \
    (KERNBASE <= (start) && (start) < (end) && (end) <= KERNTOP) // 校验区间是否完全落在内核线性映射区

#ifndef __ASSEMBLER__

#include <defs.h>
#include <atomic.h>
#include <list.h>

typedef uintptr_t pte_t;           // 页表项类型
typedef uintptr_t pde_t;           // 页目录项类型（Sv39下仍沿用命名）
typedef pte_t swap_entry_t;        // 页表项也可编码交换分区信息

/* *
 * struct Page - Page descriptor structures. Each Page describes one
 * physical page. In kern/mm/pmm.h, you can find lots of useful functions
 * that convert Page to other data types, such as physical address.
 * */
struct Page
{
    int ref;                    // 引用计数：被映射/共享的次数
    uint64_t flags;             // 状态位集合：保留/空闲块头等
    unsigned int property;      // 连续空闲块大小（页数），仅块头页有效
    list_entry_t page_link;     // 连接到物理页空闲链表
    list_entry_t pra_page_link; // 页面替换算法使用的链表节点
    uintptr_t pra_vaddr;        // 页面替换时记录的虚拟地址
};

/* Flags describing the status of a page frame */
#define PG_reserved 0 // 1 表示内核保留页，alloc/free_pages 不可使用
#define PG_property 1 // 1 表示空闲块头页（保存 property 大小，可分配）

#define SetPageReserved(page) set_bit(PG_reserved, &((page)->flags))     // 标记为保留
#define ClearPageReserved(page) clear_bit(PG_reserved, &((page)->flags)) // 取消保留
#define PageReserved(page) test_bit(PG_reserved, &((page)->flags))       // 是否保留
#define SetPageProperty(page) set_bit(PG_property, &((page)->flags))     // 标记为空闲块头
#define ClearPageProperty(page) clear_bit(PG_property, &((page)->flags)) // 取消空闲块头标记
#define PageProperty(page) test_bit(PG_property, &((page)->flags))       // 是否为空闲块头

// convert list entry to page
#define le2page(le, member) \
    to_struct((le), struct Page, member)

/* free_area_t - maintains a doubly linked list to record free (unused) pages */
typedef struct
{
    list_entry_t free_list; // 空闲页块链表头
    unsigned int nr_free;   // 空闲页总数
} free_area_t;

#endif /* !__ASSEMBLER__ */

#endif /* !__KERN_MM_MEMLAYOUT_H__ */
