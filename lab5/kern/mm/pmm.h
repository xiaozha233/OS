#ifndef __KERN_MM_PMM_H__ // 头文件多重包含保护开始
#define __KERN_MM_PMM_H__ // 定义多重包含保护宏

#include <defs.h> // 引入内核通用宏与类型
#include <mmu.h> // 引入页表/地址转换相关宏定义
#include <memlayout.h> // 引入整体内存布局信息
#include <atomic.h> // 引入原子操作支持
#include <assert.h> // 引入断言宏

// pmm_manager is a physical memory management class. A special pmm manager - XXX_pmm_manager
// only needs to implement the methods in pmm_manager class, then XXX_pmm_manager can be used
// by ucore to manage the total physical memory space.
struct pmm_manager
{
    const char *name;                                 // XXX_pmm_manager's name // 管理器名字，方便日志展示
    void (*init)(void);                               // initialize internal description&management data structure // 初始化分配器内部状态
                                                      // (free block list, number of free block) of XXX_pmm_manager
    void (*init_memmap)(struct Page *base, size_t n); // setup description&management data structcure according to // 初始化一段空闲物理区间
                                                      // the initial free physical memory space
    struct Page *(*alloc_pages)(size_t n);            // allocate >=n pages, depend on the allocation algorithm // 分配 n 个连续页
    void (*free_pages)(struct Page *base, size_t n);  // free >=n pages with "base" addr of Page descriptor structures(memlayout.h) // 释放 n 个连续页
    size_t (*nr_free_pages)(void);                    // return the number of free pages // 查询剩余页数
    void (*check)(void);                              // check the correctness of XXX_pmm_manager // 自检接口
};

extern const struct pmm_manager *pmm_manager; // 当前正在使用的物理内存管理器指针
extern pde_t *boot_pgdir_va; // 启动时页目录的内核虚拟地址
extern const size_t nbase; // 物理页号基址，等于 DRAM_BASE/PGSIZE
extern uintptr_t boot_pgdir_pa; // 启动页目录的物理地址

void pmm_init(void); // 完成物理内存管理初始化的入口

struct Page *alloc_pages(size_t n); // 分配 n 个连续物理页
void free_pages(struct Page *base, size_t n); // 释放从 base 起的 n 个页
size_t nr_free_pages(void); // 查询空闲页数量

#define alloc_page() alloc_pages(1) // 便利宏：分配单页
#define free_page(page) free_pages(page, 1) // 便利宏：释放单页

pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create); // 获取/创建线性地址对应的 PTE
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store); // 查找线性地址对应 Page
void page_remove(pde_t *pgdir, uintptr_t la); // 取消映射并处理引用计数
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm); // 建立映射

void load_esp0(uintptr_t esp0); // 设置内核栈指针 esp0（与任务切换相关）
void tlb_invalidate(pde_t *pgdir, uintptr_t la); // 失效某个线性地址对应 TLB
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm); // 分配并映射一页
void unmap_range(pde_t *pgdir, uintptr_t start, uintptr_t end); // 取消一段地址区间映射
void exit_range(pde_t *pgdir, uintptr_t start, uintptr_t end); // 释放页表并清理区间
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share); // 拷贝一段用户空间

void print_pgdir(void); // 打印当前页表结构

/* *
* PADDR – 接受一个内核虚拟地址（指向位于 KERNBASE 之上的地址）
此处显示了机器最大 256MB 物理内存的映射情况，并返回了相关信息
*对应的物理地址。如果向其传入非内核虚拟地址，则会引发异常。
 * */
#define PADDR(kva)                                                 \
    ({                                                             \
        uintptr_t __m_kva = (uintptr_t)(kva);                      \
        if (__m_kva < KERNBASE)                                    \
        {                                                          \
            panic("PADDR called with invalid kva %08lx", __m_kva); \
        }                                                          \
        __m_kva - va_pa_offset;                                    \
    }) // 将内核虚拟地址减去偏移得到物理地址，并校验其合法性

/* *
* KADDR – 接受一个物理地址，并返回对应的内核虚拟地址
* 地址。如果传入一个无效的物理地址，它会陷入恐慌状态。
 * */
#define KADDR(pa)                                                \
    ({                                                           \
        uintptr_t __m_pa = (pa);                                 \
        size_t __m_ppn = PPN(__m_pa);                            \
        if (__m_ppn >= npage)                                    \
        {                                                        \
            panic("KADDR called with invalid pa %08lx", __m_pa); \
        }                                                        \
        (void *)(__m_pa + va_pa_offset);                         \
    }) // 将物理地址加上偏移取得内核虚拟地址，同时确保页号在范围内

extern struct Page *pages; // 全局 Page 数组首地址
extern size_t npage; // 物理页总数
extern uint_t va_pa_offset; // VA 与 PA 之间的偏移量

static inline ppn_t // 返回类型为物理页号
page2ppn(struct Page *page)
{
    return page - pages + nbase; // 根据 Page 指针计算物理页号
}

static inline uintptr_t // 返回类型为物理地址
page2pa(struct Page *page)
{
    return page2ppn(page) << PGSHIFT; // 页号左移页大小即对应物理地址
}

static inline struct Page * // 返回类型为 Page 指针
pa2page(uintptr_t pa)
{
    if (PPN(pa) >= npage)
    {
        panic("pa2page called with invalid pa"); // 超出范围直接 panic
    }
    return &pages[PPN(pa) - nbase]; // 将物理地址转换成 Page 描述符
}

static inline void * // 返回 KVA 指针
page2kva(struct Page *page)
{
    return KADDR(page2pa(page)); // 先得物理地址再转内核虚拟地址
}

static inline struct Page * // 返回 Page 指针
kva2page(void *kva)
{
    return pa2page(PADDR(kva)); // 通过内核虚拟地址求出对应 Page
}

static inline struct Page * // 返回 Page 指针
pte2page(pte_t pte)
{
    if (!(pte & PTE_V))
    {
        panic("pte2page called with invalid pte"); // PTE 无效则报错
    }
    return pa2page(PTE_ADDR(pte)); // 取出物理地址并映射为 Page
}

static inline struct Page * // 返回 Page 指针
pde2page(pde_t pde)
{
    return pa2page(PDE_ADDR(pde)); // 将页目录项转换为 Page
}

static inline int // 返回当前引用计数
page_ref(struct Page *page)
{
    return page->ref; // 读取 ref 字段
}

static inline void // 无返回值
set_page_ref(struct Page *page, int val)
{
    page->ref = val; // 设置引用计数为指定值
}

static inline int // 返回自增后的引用计数
page_ref_inc(struct Page *page)
{
    page->ref += 1; // 引用计数加一
    return page->ref; // 返回更新后的值
}

static inline int // 返回自减后的引用计数
page_ref_dec(struct Page *page)
{
    page->ref -= 1; // 引用计数减一
    return page->ref; // 返回更新后的值
}

static inline void flush_tlb()
{
    asm volatile("sfence.vma"); // 通过 sfence.vma 刷新整个 TLB
}

// construct PTE from a page and permission bits
static inline pte_t pte_create(uintptr_t ppn, int type)
{
    return (ppn << PTE_PPN_SHIFT) | PTE_V | type; // 组合页号与权限位生成 PTE
}

static inline pte_t ptd_create(uintptr_t ppn)
{
    return pte_create(ppn, PTE_V); // 目录项只需保证有效位即可
}

extern char bootstack[], bootstacktop[]; // 内核启动栈的底和顶

#endif /* !__KERN_MM_PMM_H__ */ // 头文件多重包含保护结束
