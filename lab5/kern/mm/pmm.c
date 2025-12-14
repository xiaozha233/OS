#include <default_pmm.h>   // 默认物理内存管理器的声明
#include <defs.h>           // 核心宏与类型定义
#include <error.h>          // 通用错误码与 panic 支持
#include <kmalloc.h>        // 内核堆分配接口
#include <memlayout.h>      // 内存布局常量与结构
#include <mmu.h>            // MMU/PTE 相关宏
#include <pmm.h>            // 物理内存管理接口
#include <sbi.h>            // 与 SBI 调用相关的定义
#include <dtb.h>            // 设备树读取接口
#include <stdio.h>          // cprintf 等格式化输出
#include <string.h>         // memset/memcpy 等字符串函数
#include <sync.h>           // 中断开关等同步原语
#include <vmm.h>            // 虚拟内存管理原型
#include <riscv.h>          // RISC-V 架构专用宏

// 物理页面数组的虚拟地址
struct Page *pages;         // 物理页面数组的虚拟地址基指针
// 物理内存总量（以页为单位）
size_t npage = 0;           // 物理内存总页数
// 内核镜像映射在 VA=KERNBASE 和 PA=info.base
uint_t va_pa_offset;        // VA 与 PA 的偏移量（恒等映射）
// 在 RISC-V 中内存从 0x80000000 开始
const size_t nbase = DRAM_BASE / PGSIZE; // DRAM 起始页号

// 启动时页目录的虚拟地址
pde_t *boot_pgdir_va = NULL;        // 引导页目录的内核虚拟地址
// 启动时页目录的物理地址
uintptr_t boot_pgdir_pa;            // 引导页目录的物理地址

// 物理内存管理
const struct pmm_manager *pmm_manager; // 当前选用的物理内存管理器

static void check_alloc_page(void);   // 自检物理页分配器
static void check_pgdir(void);        // 自检页目录操作
static void check_boot_pgdir(void);   // 自检引导页目录映射

// init_pmm_manager - 初始化物理内存管理器实例
/*
 * 功能: 选择具体的物理内存管理器实现并完成一次性初始化。
 * 步骤:
 * 1. 指向默认实现（first-fit/buddy 等会通过 default_pmm_manager 决定）。
 * 2. 打印当前选择的管理器名称，方便启动日志定位。
 * 3. 调用管理器的 init 钩子，建立其内部数据结构（空闲链表、伙伴树等）。
 */
static void init_pmm_manager(void) // 初始化物理内存管理器实例
{
    pmm_manager = &default_pmm_manager;                    // 选择默认的管理器实现
    cprintf("memory management: %s\n", pmm_manager->name); // 打印所使用的管理器名称
    pmm_manager->init();                                   // 调用其 init 钩子进行内部初始化
}

// init_memmap - 调用 pmm->init_memmap 为空闲内存构建 Page 结构
/*
 * 功能: 把一段物理地址范围注册给物理内存管理器，纳入其空闲页数据结构。
 * 说明: 入口参数 base/n 通常来自 page_init 探测出的未占用区间，由特定 pmm 实现决定
 *       Page 的状态字段（引用计数、链表指针等）如何初始化。
 */
static void init_memmap(struct Page *base, size_t n) // 交由管理器初始化空闲页元数据
{
    pmm_manager->init_memmap(base, n); // 调用管理器特定实现建立空闲结构
}

// alloc_pages - 调用 pmm->alloc_pages 分配连续的 n*PAGESIZE 大小的内存
/*
 * 功能: 在内核态申请 n 个连续页并屏蔽临界区，防止并发修改空闲链表。
 * 细节:
 * 1. 采用 local_intr_save/restore 关中断，保证 pmm_manager 内部结构一致。
 * 2. 将真正的分配逻辑完全交给具体 pmm，实现策略解耦。
 * 3. 返回 Page* 供后续转换为物理/虚拟地址。
 */
struct Page *alloc_pages(size_t n)     // 申请 n 个连续物理页
{
    struct Page *page = NULL;          // 存放返回的 Page 指针
    bool intr_flag;                    // 保存中断标志
    local_intr_save(intr_flag);        // 关中断保护临界区
    {
        page = pmm_manager->alloc_pages(n); // 委托管理器执行实际分配
    }
    local_intr_restore(intr_flag);     // 恢复中断状态
    return page;                       // 返回分配到的页（可能为 NULL）
}

// free_pages - 调用 pmm->free_pages 释放连续的 n*PAGESIZE 大小的内存
/*
 * 功能: 将一段连续物理页重新挂回空闲链表，并保证释放过程中无并发。
 * 注意: base 必须来自 alloc_pages，且引用计数已归零，否则可能破坏 pmm 状态。
 */
void free_pages(struct Page *base, size_t n) // 释放 n 个连续物理页
{
    bool intr_flag;                         // 保存中断状态
    local_intr_save(intr_flag);             // 关中断进入临界区
    {
        pmm_manager->free_pages(base, n);   // 委托管理器执行释放
    }
    local_intr_restore(intr_flag);          // 恢复中断状态
}

// nr_free_pages - 调用 pmm->nr_free_pages 获取当前空闲内存的大小（nr*PAGESIZE）
/*
 * 功能: 查询剩余可用页数，常用于自检或内存压力判断。
 * 策略: 同样通过关中断避免在统计时遇到竞态，保证返回值一致。
 */
size_t nr_free_pages(void)   // 查询当前空闲页数
{
    size_t ret;              // 保存统计结果
    bool intr_flag;          // 中断状态
    local_intr_save(intr_flag);      // 关中断防止竞态
    {
        ret = pmm_manager->nr_free_pages(); // 委托管理器统计
    }
    local_intr_restore(intr_flag);   // 恢复中断
    return ret;             // 返回空闲页数
}

/* pmm_init - 初始化物理内存管理 */
/*
 * page_init 功能: 解析 DTB/BBL 提供的物理内存布局，构造 Page 元数据并将空闲区间
 *                交给 pmm_manager。
 * 关键步骤:
 * 1. 读取设备树的物理内存起止地址，与 KERNTOP 取最小值，得到可管理范围。
 * 2. 将链接脚本末尾 (end) 往上取整作为 pages 数组的起始虚拟地址，并把内核占用区
 *    标记为保留页，避免被再次分配。
 * 3. 计算第一个可分配的物理地址 freemem，舍弃未对齐部分后调用 init_memmap 注册。
 * 4. 最终保存 va_pa_offset，供 KADDR/PADDR 系列宏运行时使用。
 */
static void page_init(void)
{
    extern char kern_entry[];                        // 来自链接脚本的内核入口符号

    va_pa_offset = PHYSICAL_MEMORY_OFFSET;          // 设置 VA/PA 恒等映射偏移

    uint64_t mem_begin = get_memory_base();         // 从 DTB 读取内存起始地址
    uint64_t mem_size = get_memory_size();          // 从 DTB 读取内存总大小
    if (mem_size == 0)                              // 若 DTB 未提供信息
    {
        panic("DTB memory info not available");    // 无法继续，直接 panic
    }
    uint64_t mem_end = mem_begin + mem_size;        // 计算物理内存结束地址

    cprintf("physcial memory map:\n");             // 打印内存映射标题
    cprintf("  memory: 0x%08lx, [0x%08lx, 0x%08lx].\n", mem_size, mem_begin,
            mem_end - 1);                            // 输出具体范围

    uint64_t maxpa = mem_end;                       // 初始可管理上界

    if (maxpa > KERNTOP)                            // 若超出线性映射窗口
    {
        maxpa = KERNTOP;                            // 截断到 KERNTOP
    }

    extern char end[];                              // 链接脚本中的内核末尾符号

    npage = maxpa / PGSIZE;                         // 计算最大物理页数量
    // BBL 已将初始页表放置在内核之后的第一个可用页
    // 因此通过在 end 后添加额外偏移量来避开它
    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE); // 将 pages 数组放到内核末尾上方

    for (size_t i = 0; i < npage - nbase; i++)      // 遍历所有 Page 元素
    {
        SetPageReserved(pages + i);                 // 默认标记为保留，稍后再释放
    }

    uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * (npage - nbase)); // 计算 pages 数组占用的物理位置

    mem_begin = ROUNDUP(freemem, PGSIZE);           // 对齐第一个可用物理地址
    mem_end = ROUNDDOWN(mem_end, PGSIZE);           // 对齐物理内存末尾
    if (freemem < mem_end)                          // 若仍有可用范围
    {
        init_memmap(pa2page(mem_begin), (mem_end - mem_begin) / PGSIZE); // 注册空闲页
    }
    cprintf("vapaofset is %llu\n", va_pa_offset); // 打印 VA/PA 偏移值
}

// boot_map_segment - 设置并启用分页机制
// 参数
//  la:   需要映射的内存的线性地址（x86 段映射之后）
//  size: 内存大小
//  pa:   该内存的物理地址
//  perm: 该内存的权限
/*
 * 功能: 以页粒度批量建立线性地址到物理地址的映射，常用于内核静态区域映射。
 * 过程:
 * 1. 对齐传入的 la/pa/size，保证页表项的页内偏移一致。
 * 2. 逐页调用 get_pte 获取/创建页表，再用 pte_create 写入最终条目。
 * 3. perm 允许传入组合位 (PTE_W/PTE_U)，函数统一加上有效位 PTE_V。
 */
static void boot_map_segment(pde_t *pgdir, uintptr_t la, size_t size,
                             uintptr_t pa, uint32_t perm)
{
    assert(PGOFF(la) == PGOFF(pa));                         // 确保线性/物理偏移一致
    size_t n = ROUNDUP(size + PGOFF(la), PGSIZE) / PGSIZE;  // 计算需要映射的页数
    la = ROUNDDOWN(la, PGSIZE);                             // 起始线性地址下对齐
    pa = ROUNDDOWN(pa, PGSIZE);                             // 起始物理地址下对齐
    for (; n > 0; n--, la += PGSIZE, pa += PGSIZE)          // 逐页建立映射
    {
        pte_t *ptep = get_pte(pgdir, la, 1);                // 获取/创建目标 PTE
        assert(ptep != NULL);                               // 要求必须成功
        *ptep = pte_create(pa >> PGSHIFT, PTE_V | perm);    // 填写物理页号与权限
    }
}

// boot_alloc_page - 使用 pmm->alloc_pages(1) 分配一个页面
// 返回值: 该分配页面的内核虚拟地址
// 注意: 此函数用于获取 PDT（页目录表）和 PT（页表）的内存
/*
 * 功能: 在早期内存管理尚未完全启动时为页表分配页框，并立即返回其 KVA。
 * 说明:
 * 1. 失败直接 panic，因为启动期缺页无法恢复。
 * 2. 使用 page2kva 以便直接对页表进行清零或填充。
 */
static void *boot_alloc_page(void)
{
    struct Page *p = alloc_page();          // 尝试获取单个物理页
    if (p == NULL)                          // 早期若失败则无法继续
    {
        panic("boot_alloc_page failed.\n"); // 直接终止
    }
    return page2kva(p);                     // 返回对应的内核虚拟地址
}

// pmm_init - 设置物理内存管理器来管理物理内存，构建 PDT 和 PT 以建立分页机制
//         - 检查物理内存管理器和分页机制的正确性，打印 PDT 和 PT
/*
 * 功能: 物理内存子系统的总入口，串联所有初始化阶段。
 * 执行顺序:
 * 1. 初始化 pmm 管理实例，准备分配算法。
 * 2. 调用 page_init 枚举物理内存并注册空闲页。
 * 3. 运行 check_alloc_page 自检基础分配/释放流程。
 * 4. 设置 boot_pgdir_va/pa，运行 check_pgdir 与 check_boot_pgdir 验证页表.
 * 5. 最后调用 kmalloc_init，提供更高层的堆分配能力。
 */
void pmm_init(void)
{
    // 我们需要分配/释放物理内存（粒度为 4KB 或其他大小）。
    // 因此在 pmm.h 中定义了物理内存管理器框架（struct pmm_manager）
    // 首先我们应该基于该框架初始化一个物理内存管理器（pmm）。
    // 然后 pmm 就可以分配/释放物理内存。
    // 现在可用的 pmm 有 first_fit/best_fit/worst_fit/buddy_system。
    init_pmm_manager();                     // 选择并初始化具体的 pmm 实现

    // 检测物理内存空间，保留已使用的内存，
    // 然后使用 pmm->init_memmap 创建空闲页链表
    page_init();                            // 探测物理内存并建立 Page 数组

    // 使用 pmm->check 验证物理内存管理器中分配/释放函数的正确性
    check_alloc_page();                     // 运行 pmm 自检

    // 创建 boot_pgdir，一个初始的页目录（页目录表，PDT）
    extern char boot_page_table_sv39[];     // 链接脚本中的预制页表
    boot_pgdir_va = (pte_t *)boot_page_table_sv39; // 保存其虚拟地址
    boot_pgdir_pa = PADDR(boot_pgdir_va);   // 计算对应物理地址

    check_pgdir();                          // 验证页表基本操作

    static_assert(KERNBASE % PTSIZE == 0 && KERNTOP % PTSIZE == 0); // 确保线性映射对齐到页表大小

    // 现在基本的虚拟内存映射（参见 memalyout.h）已经建立。
    // 检查基本虚拟内存映射的正确性。
    check_boot_pgdir();                     // 检查内核恒等映射

    kmalloc_init();                         // 初始化基于物理页的内核堆
}

// get_pte - 获取页表项并返回该页表项对应线性地址 la 的内核虚拟地址
//        - 如果包含该页表项的页表不存在，则为页表分配一个页面
// 参数:
//  pgdir:  页目录表的内核虚拟基地址
//  la:     需要映射的线性地址
//  create: 一个逻辑值，决定是否为页表分配页面
// 返回值: 该页表项的内核虚拟地址
/*
 * 功能: 在三级 (SV39) 页表结构中逐层定位到最终 PTE，必要时按需分配新页表。
 * 细节:
 * 1. 先访问一级目录，若无效且允许 create，则申请新页并清零，设置用户/有效位。
 * 2. 再访问零级目录，重复同样流程，保证叶子页表存在。
 * 3. 返回叶子页表中 PTX(la) 对应项的内核虚拟地址，调用者可读写其内容。
 */
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    pde_t *pdep1 = &pgdir[PDX1(la)];                // 取得一级目录项指针
    if (!(*pdep1 & PTE_V))                          // 若一级目录无效
    {
        struct Page *page;                          // 准备新的页表页
        if (!create || (page = alloc_page()) == NULL) // 不允许创建或分配失败
        {
            return NULL;                            // 返回空指针
        }
        set_page_ref(page, 1);                      // 初始化引用计数
        uintptr_t pa = page2pa(page);               // 获取物理地址
        memset(KADDR(pa), 0, PGSIZE);               // 将新页清零
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V); // 写入目录项
    }

    pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)]; // 定位零级目录项
    if (!(*pdep0 & PTE_V))                          // 若零级目录缺失
    {
        struct Page *page;                          // 分配页表页
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;                            // 无法创建则返回 NULL
        }
        set_page_ref(page, 1);                      // 设置引用计数
        uintptr_t pa = page2pa(page);               // 计算物理地址
        memset(KADDR(pa), 0, PGSIZE);               // 清零页表
        *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V); // 写入零级目录项
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)]; // 返回最终 PTE 地址
}

// get_page - 使用页目录 pgdir 获取线性地址 la 相关的 Page 结构
/*
 * 功能: 基于 get_pte 查找 PTE，并在存在有效映射时将其转换为 Page 结构。
 * 用法: 常用于缺页处理/复制页表等场景下获取物理页描述符。
 */
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
{
    pte_t *ptep = get_pte(pgdir, la, 0);     // 获取线性地址对应的 PTE
    if (ptep_store != NULL)                  // 若需要返回 PTE 地址
    {
        *ptep_store = ptep;                  // 写入调用者提供的指针
    }
    if (ptep != NULL && *ptep & PTE_V)       // PTE 存在且有效
    {
        return pte2page(*ptep);              // 转换为 Page 结构
    }
    return NULL;                             // 否则返回空
}

// page_remove_pte - 释放与线性地址 la 相关的 Page 结构
//                - 并清除（使无效）与线性地址 la 相关的页表项
// 注意: 页表已更改，因此需要使 TLB 无效
/*
 * 功能: 根据指定 PTE 执行一次完整的“去映射”流程。
 * 步骤:
 * 1. 如果 PTE 有效，取出对应 Page 并将引用计数减一；为零时释放物理页。
 * 2. 将 PTE 清零，确保软件视图一致。
 * 3. 调用 tlb_invalidate，避免处理器继续缓存旧的映射。
 */
static inline void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep)
{
    if (*ptep & PTE_V)                     // 仅处理有效 PTE
    {
        struct Page *page = pte2page(*ptep); // 找到对应的物理页
        page_ref_dec(page);                 // 引用计数减一
        if (page_ref(page) == 0)            // 若无人引用
        {
            free_page(page);                // 释放物理页
        }
        *ptep = 0;                         // 清空 PTE
        tlb_invalidate(pgdir, la);          // 失效 TLB 项
    }
}

void unmap_range(pde_t *pgdir, uintptr_t start, uintptr_t end)
{
    /*
     * 功能: 逐页撤销 [start, end) 区间内的用户态映射，常用于进程退出或重建。
     * 过程: 按页扫描，如果对应的页表还没被创建则快速跳过一个大页 (PTSIZE)。
     */
    assert(start % PGSIZE == 0 && end % PGSIZE == 0); // 必须页对齐
    assert(USER_ACCESS(start, end));                  // 区间需位于用户空间

    do
    {
        pte_t *ptep = get_pte(pgdir, start, 0);       // 获取当前页的 PTE
        if (ptep == NULL)                             // 若页表不存在
        {
            start = ROUNDDOWN(start + PTSIZE, PTSIZE); // 跳到下一个大页
            continue;                                 // 继续循环
        }
        if (*ptep != 0)                               // 存在映射
        {
            page_remove_pte(pgdir, start, ptep);      // 移除并释放
        }
        start += PGSIZE;                              // 处理下一页
    } while (start != 0 && start < end);              // 避免溢出并限制范围
}

void exit_range(pde_t *pgdir, uintptr_t start, uintptr_t end)
{
    /*
     * 功能: 在解除用户态映射后，尝试进一步回收空闲的页表页，减少内存占用。
     * 过程: 以一级目录为外层、零级目录为内层，检查其子表是否全无有效项，若是则
     *       释放对应 Page，并把更上层目录项清零。
     */
    assert(start % PGSIZE == 0 && end % PGSIZE == 0); // 区间页对齐
    assert(USER_ACCESS(start, end));                  // 必须在用户空间内

    uintptr_t d1start, d0start;                      // 分别表示一级/零级扫描起点
    int free_pt, free_pd0;                           // 标记是否可释放页表/目录
    pde_t *pd0, *pt, pde1, pde0;                     // 中间指针与目录项
    d1start = ROUNDDOWN(start, PDSIZE);              // 一级目录起点对齐
    d0start = ROUNDDOWN(start, PTSIZE);              // 零级目录起点对齐
    do
    {
        // 一级页目录项
        pde1 = pgdir[PDX1(d1start)];                 // 读取一级目录项
        // 如果有有效的项，进入零级
        // 并尝试释放所有由零级页目录中所有有效项
        // 指向的页表，
        // 然后尝试释放此零级页目录
        // 并更新一级项
        if (pde1 & PTE_V)                            // 仅处理有效一级目录
        {
            pd0 = page2kva(pde2page(pde1));          // 获取零级目录基地址
            // 尝试释放所有页表
            free_pd0 = 1;                            // 假设可释放零级目录
            do
            {
                pde0 = pd0[PDX0(d0start)];           // 读取零级目录项
                if (pde0 & PTE_V)                    // 若零级目录项有效
                {
                    pt = page2kva(pde2page(pde0));  // 获取叶子页表
                    // 尝试释放页表
                    free_pt = 1;                     // 假设可释放整个页表
                    for (int i = 0; i < NPTEENTRY; i++) // 遍历所有 PTE
                        if (pt[i] & PTE_V)           // 发现仍有映射
                        {
                            free_pt = 0;             // 不能释放该页表
                            break;                   // 跳出循环
                        }
                    // 仅当所有项都已无效时才释放它
                    if (free_pt)                     // 确认无映射
                    {
                        free_page(pde2page(pde0));  // 释放叶子页表页
                        pd0[PDX0(d0start)] = 0;     // 清空零级目录项
                    }
                }
                else
                    free_pd0 = 0;                    // 存在未分配的项则不能释放零级目录
                d0start += PTSIZE;                   // 前进到下一个零级目录块
            } while (d0start != 0 && d0start < d1start + PDSIZE && d0start < end); // 控制范围并防溢出
            // 仅当其中所有 pde0 都已无效时才释放零级页目录
            if (free_pd0)                            // 若标记仍为 1
            {
                free_page(pde2page(pde1));           // 释放零级目录页
                pgdir[PDX1(d1start)] = 0;            // 清空一级目录项
            }
        }
        d1start += PDSIZE;                           // 处理下一个一级目录块
        d0start = d1start;                           // 重置零级起点
    } while (d1start != 0 && d1start < end);         // 直到越界或完成区间
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
    /*
     * 功能: 将父进程 [start, end) 的用户页逐页深拷贝到子进程，保证独立地址空间。
     * 关键点:
     * 1. 遇到缺失的页表直接跳到下一个 PTSIZE，避免不必要的分页。
     * 2. 对每个有效页，重新分配一页物理内存，复制内容并调用 page_insert 建立映射。
     * 3. share 参数暂未启用，如需写时复制可在此扩展。
     */
    assert(start % PGSIZE == 0 && end % PGSIZE == 0); // 起止地址需页对齐
    assert(USER_ACCESS(start, end));                  // 仅允许用户空间区间
    // 按页单位复制内容。
    do
    {
        // 调用 get_pte 根据地址 start 找到进程 A 的页表项
        pte_t *ptep = get_pte(from, start, 0), *nptep; // 获取父进程 PTE
        if (ptep == NULL)                              // 页表缺失
        {
            start = ROUNDDOWN(start + PTSIZE, PTSIZE); // 跳到下一段
            continue;                                  // 继续循环
        }
        // 调用 get_pte 根据地址 start 找到进程 B 的页表项。如果
        // pte 为 NULL，则分配一个页表
        if (*ptep & PTE_V)                            // 仅复制有效页
        {
            if ((nptep = get_pte(to, start, 1)) == NULL) // 确保子进程页表存在
            {
                return -E_NO_MEM;                      // 分配失败立即返回
            }
            uint32_t perm = (*ptep & PTE_USER);        // 复制权限位
            // 从页表项获取页面
            struct Page *page = pte2page(*ptep);       // 父进程物理页
            // 为进程 B 分配一个页面
            struct Page *npage = alloc_page();         // 子进程新物理页
            assert(page != NULL);                      // 断言合法
            assert(npage != NULL);
            int ret = 0;                               // 保存插入结果
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
            void *src_kvaddr = page2kva(page);         // 源页的内核虚拟地址
            
            // (2) 获取目标页面（子进程）的内核虚拟地址
            void *dst_kvaddr = page2kva(npage);        // 目标页的内核虚拟地址
            
            // (3) 将源页面的内容复制到目标页面，大小为一页 (PGSIZE = 4KB)
            memcpy(dst_kvaddr, src_kvaddr, PGSIZE);    // 逐页复制内容
            
            // (4) 在子进程的页表中建立虚拟地址 start 到物理页 npage 的映射
            // perm 是从父进程页表项中提取的权限位
            ret = page_insert(to, npage, start, perm); // 将新页映射到子进程

            assert(ret == 0);                          // 确保插入成功
        }
        start += PGSIZE;                               // 前进到下一页
    } while (start != 0 && start < end);
    return 0;                                      // 复制成功
}

// page_remove - 释放与线性地址 la 相关且具有有效页表项的 Page
/*
 * 功能: 用户接口形式的“去映射”，内部调用 page_remove_pte 执行实际逻辑。
 * 适用于调用者只持有线性地址而不关心 PTE 的场景。
 */
void page_remove(pde_t *pgdir, uintptr_t la)
{
    pte_t *ptep = get_pte(pgdir, la, 0); // 查找对应 PTE
    if (ptep != NULL)                    // 若存在页表
    {
        page_remove_pte(pgdir, la, ptep); // 进行去映射
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
    /*
     * 功能: 构建一次 la->page 的映射，若目标线性地址已有其它页则先释放。
     * 步骤:
     * 1. get_pte(create=1) 确保页表存在；失败返回 -E_NO_MEM。
     * 2. 提前增加 page 引用计数，避免与自映射冲突时过早释放。
     * 3. 如果原 PTE 有效且指向其他页，调用 page_remove_pte 清理旧映射。
     * 4. 写入新的 PTE 并刷新 TLB。
     */
    pte_t *ptep = get_pte(pgdir, la, 1);  // 获取/创建目标 PTE
    if (ptep == NULL)                     // 分配页表失败
    {
        return -E_NO_MEM;                // 返回内存不足
    }
    page_ref_inc(page);                   // 预增引用计数
    if (*ptep & PTE_V)                    // 若已有映射
    {
        struct Page *p = pte2page(*ptep); // 获取旧页
        if (p == page)                    // 自己映射自己
        {
            page_ref_dec(page);           // 抵消多加的引用
        }
        else
        {
            page_remove_pte(pgdir, la, ptep); // 先移除旧映射
        }
    }
    *ptep = pte_create(page2ppn(page), PTE_V | perm); // 写入新 PTE
    tlb_invalidate(pgdir, la);          // 刷新对应 TLB
    return 0;                           // 成功
}

// 使 TLB 项无效，但仅当正在编辑的页表是
// 处理器当前正在使用的页表时才执行此操作。
void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{
    /*
     * 功能: 触发一次 sfence.vma 指令，以线性地址为粒度丢弃 TLB 缓存项。
     * 说明: 这里没有比较 pgdir，只要调用就立刻刷新，简化实现但可能多刷新几次。
     */
    asm volatile("sfence.vma %0" : : "r"(la)); // RISC-V 指令刷新单条 TLB
}

// pgdir_alloc_page - 调用 alloc_page 和 page_insert 函数来
//                  - 分配一个页面大小的内存并建立地址映射
//                  - 在线性地址 la 和页目录 pgdir 之间建立 pa<->la 映射
struct Page *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm)
{
    /*
     * 功能: 常规的“分配+映射”组合操作，返回已经建立映射的 Page*
     * 流程: 先 alloc_page，再用 page_insert 绑定到指定线性地址；失败时保证回收。
     */
    struct Page *page = alloc_page();               // 申请新的物理页
    if (page != NULL)                               // 分配成功
    {
        if (page_insert(pgdir, page, la, perm) != 0) // 尝试建立映射
        {
            free_page(page);                        // 失败则回收
            return NULL;                            // 返回空
        }
        // swap_map_swappable(check_mm_struct, la, page, 0);
        page->pra_vaddr = la;                       // 记录对应虚拟地址
        assert(page_ref(page) == 1);                 // 断言引用计数为 1
        // cprintf("在 pgdir_alloc_page 中获取第 %d 个页面: pra_vaddr %x, pra_link.prev %x,
        // pra_link_next %x\n", (page-pages),
        // page->pra_vaddr,page->pra_page_link.prev,
        // page->pra_page_link.next);
    }

    return page;                                     // 返回页面指针
}

static void check_alloc_page(void)
{
    /*
     * 功能: 调用 pmm_manager->check() 运行管理器自带的单元测试，以验证空闲链表状态。
     */
    pmm_manager->check();                 // 运行管理器内置测试
    cprintf("check_alloc_page() succeeded!\n"); // 输出成功信息
}

static void check_pgdir(void)
{
    /*
     * 功能: 针对 boot_pgdir 执行一系列插入/删除断言，用于验证 get_pte/page_insert 等
     *       基础操作的正确性以及页表引用计数管理。
     * 步骤示例: 分配页面、建立映射、检查权限位，再逐步删去并确认引用计数归零。
     */
    // assert(npage <= KMEMSIZE / PGSIZE);
    // 在 RISC-V 中内存从 2GB 开始
    // 所以 npage 总是大于 KMEMSIZE / PGSIZE
    size_t nr_free_store;                           // 保存初始空闲页数

    nr_free_store = nr_free_pages();                // 记录当前空闲页

    assert(npage <= KERNTOP / PGSIZE);              // 确认页数未超上限
    assert(boot_pgdir_va != NULL && (uint32_t)PGOFF(boot_pgdir_va) == 0); // 引导目录存在且对齐
    assert(get_page(boot_pgdir_va, 0x0, NULL) == NULL); // 最低地址无映射

    struct Page *p1, *p2;                           // 测试页指针
    p1 = alloc_page();                              // 分配第一页
    assert(page_insert(boot_pgdir_va, p1, 0x0, 0) == 0); // 映射到 0 地址

    pte_t *ptep;                                    // 临时 PTE 指针
    assert((ptep = get_pte(boot_pgdir_va, 0x0, 0)) != NULL); // 获取 PTE
    assert(pte2page(*ptep) == p1);                  // 确认指向 p1
    assert(page_ref(p1) == 1);                      // 引用计数应为 1

    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir_va[0])); // 访问一级目录
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;       // 访问下一级并偏移一项
    assert(get_pte(boot_pgdir_va, PGSIZE, 0) == ptep);  // 再次确认地址

    p2 = alloc_page();                              // 分配第二页
    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0); // 映射到 4KB
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL); // 获取该 PTE
    assert(*ptep & PTE_U);                          // 验证用户位
    assert(*ptep & PTE_W);                          // 验证写位
    assert(boot_pgdir_va[0] & PTE_U);               // 一级目录也应设置用户位
    assert(page_ref(p2) == 1);                      // 引用计数为 1

    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0); // 用 p1 覆盖原映射
    assert(page_ref(p1) == 2);                      // p1 现在被两个地址引用
    assert(page_ref(p2) == 0);                      // p2 已被释放
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL); // 再次获取 PTE
    assert(pte2page(*ptep) == p1);                  // 应指向 p1
    assert((*ptep & PTE_U) == 0);                   // 用户位被清除

    page_remove(boot_pgdir_va, 0x0);                // 移除 0 地址映射
    assert(page_ref(p1) == 1);                      // p1 引用减为 1
    assert(page_ref(p2) == 0);                      // p2 仍为 0

    page_remove(boot_pgdir_va, PGSIZE);             // 移除第二个映射
    assert(page_ref(p1) == 0);                      // p1 被完全释放
    assert(page_ref(p2) == 0);                      // p2 保持 0

    assert(page_ref(pde2page(boot_pgdir_va[0])) == 1); // 页表页引用应为 1

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0])); // 取得目录与页表页
    free_page(pde2page(pd0[0]));                    // 释放第二级页表页
    free_page(pde2page(pd1[0]));                    // 释放一级目录页
    boot_pgdir_va[0] = 0;                           // 清除目录项
    flush_tlb();                                     // 刷新 TLB

    assert(nr_free_store == nr_free_pages());        // 空闲页应恢复原值

    cprintf("check_pgdir() succeeded!\n");        // 打印成功信息
}

static void check_boot_pgdir(void)
{
    /*
     * 功能: 验证内核静态映射与复制填充逻辑，确保内核虚拟地址空间的恒等映射正确。
     * 测试内容: 检查 KERNBASE 以上的恒等映射、用户态双映射字符串一致性，以及清理后
     *          空闲页数恢复到初始值。
     */
    size_t nr_free_store;                               // 记录初始空闲页
    pte_t *ptep;                                        // 临时 PTE 指针
    int i;                                              // 循环变量

    nr_free_store = nr_free_pages();                    // 保存当前空闲页数

    for (i = ROUNDDOWN(KERNBASE, PGSIZE); i < npage * PGSIZE; i += PGSIZE)
    {
        assert((ptep = get_pte(boot_pgdir_va, (uintptr_t)KADDR(i), 0)) != NULL); // 获取各内核恒等映射的 PTE
        assert(PTE_ADDR(*ptep) == i);                       // 确认物理地址匹配
    }

    assert(boot_pgdir_va[0] == 0);                         // 用户空间首项应为空

    struct Page *p;                                        // 测试页
    p = alloc_page();                                      // 分配一页
    assert(page_insert(boot_pgdir_va, p, 0x100, PTE_W | PTE_R) == 0); // 映射到 0x100
    assert(page_ref(p) == 1);                              // 引用为 1
    assert(page_insert(boot_pgdir_va, p, 0x100 + PGSIZE, PTE_W | PTE_R) == 0); // 再映射到下一页
    assert(page_ref(p) == 2);                              // 引用增加到 2

    const char *str = "ucore: Hello world!!";             // 用固定字符串验证双映射可见性
    strcpy((void *)0x100, str);                            // 将字符串写入低地址映射的一份
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0); // 另一份映射读取应得到完全相同内容

    *(char *)(page2kva(p) + 0x100) = '\0';                // 直接改写底层物理页相应偏移为终止符
    assert(strlen((const char *)0x100) == 0);              // 再访问虚拟地址即可看到长度变 0，证明同物理页共享

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0])); // 获取目录与页表页
    free_page(p);                                          // 释放测试页
    free_page(pde2page(pd0[0]));                           // 释放页表页
    free_page(pde2page(pd1[0]));                           // 释放目录页
    boot_pgdir_va[0] = 0;                                  // 清空目录项
    flush_tlb();                                           // 刷新 TLB

    assert(nr_free_store == nr_free_pages());              // 空闲页数应复原

    cprintf("check_boot_pgdir() succeeded!\n");          // 打印成功信息
}

// perm2str - 使用字符串 'u,r,w,-' 来表示权限
static const char *perm2str(int perm)
{
    /*
     * 功能: 将 PTE 中的用户/可写权限位转换为 'u','r','w' 字符串，方便调试输出。
     */
    static char str[4];                        // 本地静态缓存
    str[0] = (perm & PTE_U) ? 'u' : '-';       // 用户位
    str[1] = 'r';                              // 读权限恒为 r
    str[2] = (perm & PTE_W) ? 'w' : '-';       // 写权限
    str[3] = '\0';                            // 字符串结尾
    return str;                                // 返回描述串
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
    /*
     * 功能: 在页表/页目录内扫描连续的有效项段，返回段权限并告知起止下标。
     * 用途: 通常用于打印页表或统计地址空间布局。
     */
    if (start >= right)                    // 起点不在范围内
    {
        return 0;                          // 无有效项直接返回
    }
    while (start < right && !(table[start] & PTE_V)) // 跳过无效项
    {
        start++;
    }
    if (start < right)                     // 找到第一个有效项
    {
        if (left_store != NULL)           // 如需记录左端
        {
            *left_store = start;
        }
        int perm = (table[start++] & PTE_USER); // 保存其权限位
        while (start < right && (table[start] & PTE_USER) == perm) // 扩展同权限区间
        {
            start++;
        }
        if (right_store != NULL)          // 如需记录右端
        {
            *right_store = start;
        }
        return perm;                      // 返回该段权限编码
    }
    return 0;                              // 没有找到连续段
}
