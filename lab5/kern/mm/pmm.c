#include <default_pmm.h>
#include <defs.h>
#include <error.h>
#include <kmalloc.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <dtb.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>
#include <vmm.h>
#include <riscv.h>

// 物理页面数组的虚拟地址
struct Page *pages;
// 物理内存总量（以页为单位）
size_t npage = 0;
// 内核镜像映射在 VA=KERNBASE 和 PA=info.base
uint_t va_pa_offset;
// 在 RISC-V 中内存从 0x80000000 开始
const size_t nbase = DRAM_BASE / PGSIZE;

// 启动时页目录的虚拟地址
pde_t *boot_pgdir_va = NULL;
// 启动时页目录的物理地址
uintptr_t boot_pgdir_pa;

// 物理内存管理
const struct pmm_manager *pmm_manager;

static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

// init_pmm_manager - 初始化物理内存管理器实例
static void init_pmm_manager(void)
{
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - 调用 pmm->init_memmap 为空闲内存构建 Page 结构
static void init_memmap(struct Page *base, size_t n)
{
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - 调用 pmm->alloc_pages 分配连续的 n*PAGESIZE 大小的内存
struct Page *alloc_pages(size_t n)
{
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - 调用 pmm->free_pages 释放连续的 n*PAGESIZE 大小的内存
void free_pages(struct Page *base, size_t n)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// nr_free_pages - 调用 pmm->nr_free_pages 获取当前空闲内存的大小（nr*PAGESIZE）
size_t nr_free_pages(void)
{
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

/* pmm_init - 初始化物理内存管理 */
static void page_init(void)
{
    extern char kern_entry[];

    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    uint64_t mem_begin = get_memory_base();
    uint64_t mem_size = get_memory_size();
    if (mem_size == 0)
    {
        panic("DTB memory info not available");
    }
    uint64_t mem_end = mem_begin + mem_size;

    cprintf("physcial memory map:\n");
    cprintf("  memory: 0x%08lx, [0x%08lx, 0x%08lx].\n", mem_size, mem_begin,
            mem_end - 1);

    uint64_t maxpa = mem_end;

    if (maxpa > KERNTOP)
    {
        maxpa = KERNTOP;
    }

    extern char end[];

    npage = maxpa / PGSIZE;
    // BBL 已将初始页表放置在内核之后的第一个可用页
    // 因此通过在 end 后添加额外偏移量来避开它
    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE);

    for (size_t i = 0; i < npage - nbase; i++)
    {
        SetPageReserved(pages + i);
    }

    uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * (npage - nbase));

    mem_begin = ROUNDUP(freemem, PGSIZE);
    mem_end = ROUNDDOWN(mem_end, PGSIZE);
    if (freemem < mem_end)
    {
        init_memmap(pa2page(mem_begin), (mem_end - mem_begin) / PGSIZE);
    }
    cprintf("vapaofset is %llu\n", va_pa_offset);
}

// boot_map_segment - 设置并启用分页机制
// 参数
//  la:   需要映射的内存的线性地址（x86 段映射之后）
//  size: 内存大小
//  pa:   该内存的物理地址
//  perm: 该内存的权限
static void boot_map_segment(pde_t *pgdir, uintptr_t la, size_t size,
                             uintptr_t pa, uint32_t perm)
{
    assert(PGOFF(la) == PGOFF(pa));
    size_t n = ROUNDUP(size + PGOFF(la), PGSIZE) / PGSIZE;
    la = ROUNDDOWN(la, PGSIZE);
    pa = ROUNDDOWN(pa, PGSIZE);
    for (; n > 0; n--, la += PGSIZE, pa += PGSIZE)
    {
        pte_t *ptep = get_pte(pgdir, la, 1);
        assert(ptep != NULL);
        *ptep = pte_create(pa >> PGSHIFT, PTE_V | perm);
    }
}

// boot_alloc_page - 使用 pmm->alloc_pages(1) 分配一个页面
// 返回值: 该分配页面的内核虚拟地址
// 注意: 此函数用于获取 PDT（页目录表）和 PT（页表）的内存
static void *boot_alloc_page(void)
{
    struct Page *p = alloc_page();
    if (p == NULL)
    {
        panic("boot_alloc_page failed.\n");
    }
    return page2kva(p);
}

// pmm_init - 设置物理内存管理器来管理物理内存，构建 PDT 和 PT 以建立分页机制
//         - 检查物理内存管理器和分页机制的正确性，打印 PDT 和 PT
void pmm_init(void)
{
    // 我们需要分配/释放物理内存（粒度为 4KB 或其他大小）。
    // 因此在 pmm.h 中定义了物理内存管理器框架（struct pmm_manager）
    // 首先我们应该基于该框架初始化一个物理内存管理器（pmm）。
    // 然后 pmm 就可以分配/释放物理内存。
    // 现在可用的 pmm 有 first_fit/best_fit/worst_fit/buddy_system。
    init_pmm_manager();

    // 检测物理内存空间，保留已使用的内存，
    // 然后使用 pmm->init_memmap 创建空闲页链表
    page_init();

    // 使用 pmm->check 验证物理内存管理器中分配/释放函数的正确性
    check_alloc_page();

    // 创建 boot_pgdir，一个初始的页目录（页目录表，PDT）
    extern char boot_page_table_sv39[];
    boot_pgdir_va = (pte_t *)boot_page_table_sv39;
    boot_pgdir_pa = PADDR(boot_pgdir_va);

    check_pgdir();

    static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0);

    // 现在基本的虚拟内存映射（参见 memalyout.h）已经建立。
    // 检查基本虚拟内存映射的正确性。
    check_boot_pgdir();

    kmalloc_init();
}

// get_pte - 获取页表项并返回该页表项对应线性地址 la 的内核虚拟地址
//        - 如果包含该页表项的页表不存在，则为页表分配一个页面
// 参数:
//  pgdir:  页目录表的内核虚拟基地址
//  la:     需要映射的线性地址
//  create: 一个逻辑值，决定是否为页表分配页面
// 返回值: 该页表项的内核虚拟地址
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    pde_t *pdep1 = &pgdir[PDX1(la)];
    if (!(*pdep1 & PTE_V))
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }

    pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
    if (!(*pdep0 & PTE_V))
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
        *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}

// get_page - 使用页目录 pgdir 获取线性地址 la 相关的 Page 结构
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep_store != NULL)
    {
        *ptep_store = ptep;
    }
    if (ptep != NULL && *ptep & PTE_V)
    {
        return pte2page(*ptep);
    }
    return NULL;
}

// page_remove_pte - 释放与线性地址 la 相关的 Page 结构
//                - 并清除（使无效）与线性地址 la 相关的页表项
// 注意: 页表已更改，因此需要使 TLB 无效
static inline void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep)
{
    if (*ptep & PTE_V)
    {
        struct Page *page = pte2page(*ptep);
        page_ref_dec(page);
        if (page_ref(page) == 0)
        {
            free_page(page);
        }
        *ptep = 0;
        tlb_invalidate(pgdir, la);
    }
}

void unmap_range(pde_t *pgdir, uintptr_t start, uintptr_t end)
{
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));

    do
    {
        pte_t *ptep = get_pte(pgdir, start, 0);
        if (ptep == NULL)
        {
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue;
        }
        if (*ptep != 0)
        {
            page_remove_pte(pgdir, start, ptep);
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
}

void exit_range(pde_t *pgdir, uintptr_t start, uintptr_t end)
{
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));

    uintptr_t d1start, d0start;
    int free_pt, free_pd0;
    pde_t *pd0, *pt, pde1, pde0;
    d1start = ROUNDDOWN(start, PDSIZE);
    d0start = ROUNDDOWN(start, PTSIZE);
    do
    {
        // 一级页目录项
        pde1 = pgdir[PDX1(d1start)];
        // 如果有有效的项，进入零级
        // 并尝试释放所有由零级页目录中所有有效项
        // 指向的页表，
        // 然后尝试释放此零级页目录
        // 并更新一级项
        if (pde1 & PTE_V)
        {
            pd0 = page2kva(pde2page(pde1));
            // 尝试释放所有页表
            free_pd0 = 1;
            do
            {
                pde0 = pd0[PDX0(d0start)];
                if (pde0 & PTE_V)
                {
                    pt = page2kva(pde2page(pde0));
                    // 尝试释放页表
                    free_pt = 1;
                    for (int i = 0; i < NPTEENTRY; i++)
                        if (pt[i] & PTE_V)
                        {
                            free_pt = 0;
                            break;
                        }
                    // 仅当所有项都已无效时才释放它
                    if (free_pt)
                    {
                        free_page(pde2page(pde0));
                        pd0[PDX0(d0start)] = 0;
                    }
                }
                else
                    free_pd0 = 0;
                d0start += PTSIZE;
            } while (d0start != 0 && d0start < d1start + PDSIZE && d0start < end);
            // 仅当其中所有 pde0 都已无效时才释放零级页目录
            if (free_pd0)
            {
                free_page(pde2page(pde1));
                pgdir[PDX1(d1start)] = 0;
            }
        }
        d1start += PDSIZE;
        d0start = d1start;
    } while (d1start != 0 && d1start < end);
}
/* copy_range - 将一个进程 A 的内存内容（start, end）复制到另一个进程 B
 * @to:    进程 B 的页目录地址
 * @from:  进程 A 的页目录地址
 * @share: 指示复制或共享的标志。我们只使用复制方法，所以它未被使用。
 *
 * 调用图: copy_mm-->dup_mmap-->copy_range
 */
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end,
               bool share)
{
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));
    // 按页单位复制内容。
    do
    {
        // 调用 get_pte 根据地址 start 找到进程 A 的页表项
        pte_t *ptep = get_pte(from, start, 0), *nptep;
        if (ptep == NULL)
        {
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue;
        }
        // 调用 get_pte 根据地址 start 找到进程 B 的页表项。如果
        // pte 为 NULL，则分配一个页表
        if (*ptep & PTE_V)
        {
            if ((nptep = get_pte(to, start, 1)) == NULL)
            {
                return -E_NO_MEM;
            }
            uint32_t perm = (*ptep & PTE_USER);
            // 从页表项获取页面
            struct Page *page = pte2page(*ptep);
            // 为进程 B 分配一个页面
            struct Page *npage = alloc_page();
            assert(page != NULL);
            assert(npage != NULL);
            int ret = 0;
            /* LAB5:EXERCISE2 2314076
             * 将 page 的内容复制到 npage，并在线性地址 start 处为 npage 建立物理地址映射
             *
             * 一些有用的宏和定义，你可以在下面的实现中使用：
             * 宏或函数：
             *    page2kva(struct Page *page)：返回 page 管理的内核虚拟地址（见 pmm.h）
             *    page_insert：在页表中建立 Page 的物理地址与线性地址 la 的映射
             *    memcpy：常见的内存拷贝函数
             *
             * (1) 获取 src_kvaddr：源页面 page 的内核虚拟地址
             * (2) 获取 dst_kvaddr：目标页面 npage 的内核虚拟地址
             * (3) 将 src_kvaddr 的内容拷贝到 dst_kvaddr，大小为 PGSIZE
             * (4) 在线性地址 start 处为 npage 建立物理地址映射
             */
            
            // (1) 获取源页面（父进程）的内核虚拟地址
            void *src_kvaddr = page2kva(page);
            
            // (2) 获取目标页面（子进程）的内核虚拟地址
            void *dst_kvaddr = page2kva(npage);
            
            // (3) 将源页面的内容复制到目标页面，大小为一页 (PGSIZE = 4KB)
            memcpy(dst_kvaddr, src_kvaddr, PGSIZE);
            
            // (4) 在子进程的页表中建立虚拟地址 start 到物理页 npage 的映射
            // perm 是从父进程页表项中提取的权限位
            ret = page_insert(to, npage, start, perm);

            assert(ret == 0);
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
    return 0;
}

// page_remove - 释放与线性地址 la 相关且具有有效页表项的 Page
void page_remove(pde_t *pgdir, uintptr_t la)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep != NULL)
    {
        page_remove_pte(pgdir, la, ptep);
    }
}

// page_insert - 建立 Page 的物理地址与线性地址 la 的映射
// 参数:
//  pgdir: 页目录表的内核虚拟基地址
//  page:  需要映射的 Page
//  la:    需要映射的线性地址
//  perm:  在相关页表项中设置的该 Page 的权限
// 返回值: 始终为 0
// 注意: 页表已更改，因此需要使 TLB 无效
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
{
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL)
    {
        return -E_NO_MEM;
    }
    page_ref_inc(page);
    if (*ptep & PTE_V)
    {
        struct Page *p = pte2page(*ptep);
        if (p == page)
        {
            page_ref_dec(page);
        }
        else
        {
            page_remove_pte(pgdir, la, ptep);
        }
    }
    *ptep = pte_create(page2ppn(page), PTE_V | perm);
    tlb_invalidate(pgdir, la);
    return 0;
}

// 使 TLB 项无效，但仅当正在编辑的页表是
// 处理器当前正在使用的页表时才执行此操作。
void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{
    asm volatile("sfence.vma %0" : : "r"(la));
}

// pgdir_alloc_page - 调用 alloc_page 和 page_insert 函数来
//                  - 分配一个页面大小的内存并建立地址映射
//                  - 在线性地址 la 和页目录 pgdir 之间建立 pa<->la 映射
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm)
{
    struct Page *page = alloc_page();
    if (page != NULL)
    {
        if (page_insert(pgdir, page, la, perm) != 0)
        {
            free_page(page);
            return NULL;
        }
        // swap_map_swappable(check_mm_struct, la, page, 0);
        page->pra_vaddr = la;
        assert(page_ref(page) == 1);
        // cprintf("在 pgdir_alloc_page 中获取第 %d 个页面: pra_vaddr %x, pra_link.prev %x,
        // pra_link_next %x\n", (page-pages),
        // page->pra_vaddr,page->pra_page_link.prev,
        // page->pra_page_link.next);
    }

    return page;
}

static void check_alloc_page(void)
{
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

static void check_pgdir(void)
{
    // assert(npage <= KMEMSIZE / PGSIZE);
    // 在 RISC-V 中内存从 2GB 开始
    // 所以 npage 总是大于 KMEMSIZE / PGSIZE
    size_t nr_free_store;

    nr_free_store = nr_free_pages();

    assert(npage <= KERNTOP / PGSIZE);
    assert(boot_pgdir_va != NULL && (uint32_t)PGOFF(boot_pgdir_va) == 0);
    assert(get_page(boot_pgdir_va, 0x0, NULL) == NULL);

    struct Page *p1, *p2;
    p1 = alloc_page();
    assert(page_insert(boot_pgdir_va, p1, 0x0, 0) == 0);

    pte_t *ptep;
    assert((ptep = get_pte(boot_pgdir_va, 0x0, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert(page_ref(p1) == 1);

    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir_va[0]));
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
    assert(get_pte(boot_pgdir_va, PGSIZE, 0) == ptep);

    p2 = alloc_page();
    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(*ptep & PTE_U);
    assert(*ptep & PTE_W);
    assert(boot_pgdir_va[0] & PTE_U);
    assert(page_ref(p2) == 1);

    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
    assert(page_ref(p1) == 2);
    assert(page_ref(p2) == 0);
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
    assert(pte2page(*ptep) == p1);
    assert((*ptep & PTE_U) == 0);

    page_remove(boot_pgdir_va, 0x0);
    assert(page_ref(p1) == 1);
    assert(page_ref(p2) == 0);

    page_remove(boot_pgdir_va, PGSIZE);
    assert(page_ref(p1) == 0);
    assert(page_ref(p2) == 0);

    assert(page_ref(pde2page(boot_pgdir_va[0])) == 1);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_pgdir() succeeded!\n");
}

static void check_boot_pgdir(void)
{
    size_t nr_free_store;
    pte_t *ptep;
    int i;

    nr_free_store = nr_free_pages();

    for (i = ROUNDDOWN(KERNBASE, PGSIZE); i < npage * PGSIZE; i += PGSIZE)
    {
        assert((ptep = get_pte(boot_pgdir_va, (uintptr_t)KADDR(i), 0)) != NULL);
        assert(PTE_ADDR(*ptep) == i);
    }

    assert(boot_pgdir_va[0] == 0);

    struct Page *p;
    p = alloc_page();
    assert(page_insert(boot_pgdir_va, p, 0x100, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 1);
    assert(page_insert(boot_pgdir_va, p, 0x100 + PGSIZE, PTE_W | PTE_R) == 0);
    assert(page_ref(p) == 2);

    const char *str = "ucore: Hello world!!";
    strcpy((void *)0x100, str);
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);

    *(char *)(page2kva(p) + 0x100) = '\0';
    assert(strlen((const char *)0x100) == 0);

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(p);
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
    flush_tlb();

    assert(nr_free_store == nr_free_pages());

    cprintf("check_boot_pgdir() succeeded!\n");
}

// perm2str - 使用字符串 'u,r,w,-' 来表示权限
static const char *perm2str(int perm)
{
    static char str[4];
    str[0] = (perm & PTE_U) ? 'u' : '-';
    str[1] = 'r';
    str[2] = (perm & PTE_W) ? 'w' : '-';
    str[3] = '\0';
    return str;
}

// get_pgtable_items - 在页目录表或页表的 [left, right] 范围内，找到连续的
// 线性地址空间
//                  - (left_store*X_SIZE~right_store*X_SIZE) 对于页目录表或页表
//                  - 如果是页目录表，X_SIZE=PTSIZE=4M；如果是页表，X_SIZE=PGSIZE=4K
// 参数:
//  left:        未使用 ???
//  right:       表范围的高端
//  start:       表范围的低端
//  table:       表的起始地址
//  left_store:  表下一个范围的高端的指针
//  right_store: 表下一个范围的低端的指针
//  返回值: 0 - 不是有效项范围，perm - 具有 perm 权限的有效项范围
static int get_pgtable_items(size_t left, size_t right, size_t start,
                             uintptr_t *table, size_t *left_store,
                             size_t *right_store)
{
    if (start >= right)
    {
        return 0;
    }
    while (start < right && !(table[start] & PTE_V))
    {
        start++;
    }
    if (start < right)
    {
        if (left_store != NULL)
        {
            *left_store = start;
        }
        int perm = (table[start++] & PTE_USER);
        while (start < right && (table[start] & PTE_USER) == perm)
        {
            start++;
        }
        if (right_store != NULL)
        {
            *right_store = start;
        }
        return perm;
    }
    return 0;
}
