#include <default_pmm.h>
#include <defs.h>
#include <error.h>
#include <kmalloc.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <sync.h>
#include <vmm.h>
#include <riscv.h>
#include <dtb.h>

// 物理页结构体数组的虚拟地址
struct Page *pages;
// 物理内存的总页数
size_t npage = 0;
// 内核镜像映射的虚拟地址与物理地址的偏移量
uint_t va_pa_offset;
// RISC-V架构下内存起始地址为0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// 启动时页目录的虚拟地址
pde_t *boot_pgdir_va = NULL;
// 启动时页目录的物理地址
uintptr_t boot_pgdir_pa;

// 物理内存管理器
const struct pmm_manager *pmm_manager;

static void check_alloc_page(void);
static void check_pgdir(void);
static void check_boot_pgdir(void);

// 初始化物理内存管理器
static void init_pmm_manager(void)
{
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// 初始化物理页结构体，构建空闲页链表
static void init_memmap(struct Page *base, size_t n)
{
    pmm_manager->init_memmap(base, n);
}

// 分配连续n页物理内存
struct Page *alloc_pages(size_t n)
{
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag); // 关闭中断，保证原子性
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag); // 恢复中断
    return page;
}

// 释放连续n页物理内存
void free_pages(struct Page *base, size_t n)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// 获取当前空闲物理页数
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

/* 初始化物理页结构体和空闲页链表 */
static void page_init(void)
{
    extern char kern_entry[];

    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    uint64_t mem_begin = get_memory_base();
    uint64_t mem_size  = get_memory_size();
    if (mem_size == 0) {
        panic("DTB memory info not available");
    }
    uint64_t mem_end   = mem_begin + mem_size;

    cprintf("physcial memory map:\n");
    cprintf("  memory: 0x%08lx, [0x%08lx, 0x%08lx].\n", mem_size, mem_begin,
            mem_end - 1);

    uint64_t maxpa = mem_end;

    // 限制最大物理地址不超过KERNTOP
    if (maxpa > KERNTOP)
    {
        maxpa = KERNTOP;
    }

    extern char end[];

    npage = maxpa / PGSIZE;
    // BBL已将初始页表放在内核结束后的第一个可用页
    // 所以pages数组要避开内核和页表空间
    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE);

    // 标记所有页为保留状态
    for (size_t i = 0; i < npage - nbase; i++)
    {
        SetPageReserved(pages + i);
    }

    // 计算空闲内存起始地址
    uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * (npage - nbase));

    mem_begin = ROUNDUP(freemem, PGSIZE);
    mem_end = ROUNDDOWN(mem_end, PGSIZE);
    // 初始化空闲页链表
    if (freemem < mem_end)
    {
        init_memmap(pa2page(mem_begin), (mem_end - mem_begin) / PGSIZE);
    }
    cprintf("vapaofset is %llu\n", va_pa_offset);
}

// 启用分页机制，设置satp寄存器
static void enable_paging(void)
{
    write_csr(satp, 0x8000000000000000 | (boot_pgdir_pa >> RISCV_PGSHIFT));
}

// 建立虚拟地址到物理地址的映射关系
// 参数说明：
//  la:   需要映射的线性地址
//  size: 映射的内存大小
//  pa:   物理地址
//  perm: 权限
static void boot_map_segment(pde_t *pgdir, uintptr_t la, size_t size,
                             uintptr_t pa, uint32_t perm)
{
    assert(PGOFF(la) == PGOFF(pa));
    size_t n = ROUNDUP(size + PGOFF(la), PGSIZE) / PGSIZE;
    la = ROUNDDOWN(la, PGSIZE);
    pa = ROUNDDOWN(pa, PGSIZE);
    for (; n > 0; n--, la += PGSIZE, pa += PGSIZE)
    {
        pte_t *ptep = get_pte(pgdir, la, 1); // 获取页表项指针
        assert(ptep != NULL);
        *ptep = pte_create(pa >> PGSHIFT, PTE_V | perm); // 设置页表项
    }
}

// 分配一个页，返回该页的内核虚拟地址
// 用于分配页目录表或页表
static void *boot_alloc_page(void)
{
    struct Page *p = alloc_page();
    if (p == NULL)
    {
        panic("boot_alloc_page failed.\n");
    }
    return page2kva(p);
}

// 物理内存管理初始化，建立页表并检测正确性
void pmm_init(void)
{
    // 初始化物理内存管理器
    init_pmm_manager();

    // 检测物理内存空间，保留已用内存，初始化空闲页链表
    page_init();

    // 检查分配/释放页功能
    check_alloc_page();

    // 创建启动页目录
    extern char boot_page_table_sv39[];
    boot_pgdir_va = (pte_t *)boot_page_table_sv39;
    boot_pgdir_pa = PADDR(boot_pgdir_va);

    // 检查页目录正确性
    check_pgdir();

    static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0);

    // 检查虚拟内存映射正确性
    check_boot_pgdir();

    // 初始化内核动态内存分配
    kmalloc_init();
}

// 获取线性地址la对应的页表项指针
// 如果页表不存在且create为真，则分配新页表
// 参数：
//  pgdir: 页目录虚拟地址（顶级页表）
//  la:    线性地址
//  create: 是否需要分配新页表（为真则自动分配）
// 返回：页表项指针（pte_t*），失败返回NULL
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    // 1. 获取第一级页表项指针（页目录项）
    pde_t *pdep1 = &pgdir[PDX1(la)];
    // 2. 检查第一级页表项是否有效，通过有效位（PTE_V）判断
    if (!(*pdep1 & PTE_V))
    {
        // 3. 如果无效且需要分配(create为真)，则分配一个新的页表页
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            // 4. 分配失败或不需要分配则返回NULL
            return NULL;
        }
        // 5. 设置新分配页的引用计数为1
        set_page_ref(page, 1);
        // 6. 获取新页的物理地址
        uintptr_t pa = page2pa(page);
        // 7. 将新页表页内容清零
        memset(KADDR(pa), 0, PGSIZE);
        // 8. 设置第一级页表项为新页表页的物理页号，并标记有效和用户权限
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    // 9. 获取第二级页表项指针
    pde_t *pdep0 = &((pte_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
    // 10. 检查第二级页表项是否有效
    if (!(*pdep0 & PTE_V))
    {
        // 11. 如果无效且需要分配，则分配一个新的页表页
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            // 12. 分配失败或不需要分配则返回NULL
            return NULL;
        }
        // 13. 设置新分配页的引用计数为1
        set_page_ref(page, 1);
        // 14. 获取新页的物理地址
        uintptr_t pa = page2pa(page);
        // 15. 将新页表页内容清零
        memset(KADDR(pa), 0, PGSIZE);
        // 16. 设置第二级页表项为新页表页的物理页号，并标记有效和用户权限
        *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
    }
    // 17. 返回第三级页表项指针（最终的pte项）
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
}

// 根据线性地址la和页目录pgdir获取对应的Page结构体
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

// 释放线性地址la对应的页，并清除页表项
// 注意：页表改变后需要刷新TLB
static inline void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep)
{
    if (*ptep & PTE_V)
    { // (1) 检查页表项是否有效
        struct Page *page =
            pte2page(*ptep); // (2) 获取对应的Page结构体
        page_ref_dec(page);  // (3) 减少引用计数
        if (page_ref(page) ==
            0)
        { // (4) 如果引用计数为0则释放该页
            free_page(page);
        }
        *ptep = 0;                 // (5) 清除页表项
        tlb_invalidate(pgdir, la); // (6) 刷新TLB
    }
}

// 释放线性地址la对应的页
void page_remove(pde_t *pgdir, uintptr_t la)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep != NULL)
    {
        page_remove_pte(pgdir, la, ptep);
    }
}

// 建立Page结构体与线性地址la的映射关系
// 参数：
//  pgdir: 页目录虚拟地址
//  page:  需要映射的Page结构体
//  la:    线性地址
//  perm:  权限
// 返回值：总是0
// 注意：页表改变后需要刷新TLB
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
{
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL)
    {
        return -E_NO_MEM;
    }
    page_ref_inc(page); // 增加引用计数
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

// 刷新TLB条目，仅在当前处理器使用的页表被修改时
void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{
    // flush_tlb();
    // flush_tlb会刷新整个TLB，有没有更好的方法？
    asm volatile("sfence.vma %0" : : "r"(la));
}

// 检查分配页功能是否正确
static void check_alloc_page(void)
{
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}

// 检查页目录功能是否正确
static void check_pgdir(void)
{
    // assert(npage <= KMEMSIZE / PGSIZE);
    // RISC-V内存起始地址为2GB
    // 所以npage总是大于KMEMSIZE / PGSIZE
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

// 检查启动页目录功能是否正确
static void check_boot_pgdir(void)
{
    size_t nr_free_store;
    pte_t *ptep;
    int i;

    nr_free_store = nr_free_pages();

    // 检查内核空间的虚拟地址映射
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

// 权限转换为字符串表示，'u,r,w,-'分别表示用户、读、写、无权限
static const char *perm2str(int perm)
{
    static char str[4];
    str[0] = (perm & PTE_U) ? 'u' : '-';
    str[1] = 'r';
    str[2] = (perm & PTE_W) ? 'w' : '-';
    str[3] = '\0';
    return str;
}

// 在页目录或页表的[left, right]范围内，查找连续的有效项
// 参数：
//  left:        起始索引（未使用）
//  right:       结束索引
//  start:       查找起始索引
//  table:       页目录或页表指针
//  left_store:  返回有效项起始索引
//  right_store: 返回有效项结束索引
// 返回值：0表示无效区间，perm表示有效区间的权限
static int get_pgtable_items(size_t left, size_t right, size_t start,
                             uintptr_t *table, size_t *left_store,
                             size_t *right_store)
{
    if (start >= right)
    {
        return 0;
    }
    // 跳过无效项
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
        // 查找权限相同的连续区间
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
