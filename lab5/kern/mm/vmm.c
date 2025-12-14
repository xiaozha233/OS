#include <vmm.h>          // 引入虚拟内存管理接口定义
#include <sync.h>          // 引入同步与自旋锁工具
#include <string.h>        // 引入内存字符串操作函数
#include <assert.h>        // 引入断言宏以捕获逻辑错误
#include <stdio.h>         // 引入格式化输出函数
#include <error.h>         // 引入通用错误码定义
#include <pmm.h>           // 引入物理内存管理相关声明
#include <riscv.h>         // 引入 RISC-V 架构相关宏
#include <kmalloc.h>       // 引入内核堆分配接口

/*
  vmm design include two parts: mm_struct (mm) & vma_struct (vma)
  mm is the memory manager for the set of continuous virtual memory
  area which have the same PDT. vma is a continuous virtual memory area.
  There a linear link list for vma & a redblack link list for vma in mm.
---------------
  mm related functions:
   golbal functions
     struct mm_struct * mm_create(void)
     void mm_destroy(struct mm_struct *mm)
     int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr)
--------------
  vma related functions:
   global functions
     struct vma_struct * vma_create (uintptr_t vm_start, uintptr_t vm_end,...)
     void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma)
     struct vma_struct * find_vma(struct mm_struct *mm, uintptr_t addr)
   local functions
     inline void check_vma_overlap(struct vma_struct *prev, struct vma_struct *next)
---------------
   check correctness functions
     void check_vmm(void);
     void check_vma_struct(void);
     void check_pgfault(void);
*/

static void check_vmm(void);            // 前向声明：顶层虚拟内存管理自检函数
static void check_vma_struct(void);     // 前向声明：VMA 结构完整性检查函数

// mm_create -  alloc a mm_struct & initialize it.
/*
 * 功能: 为一个新的地址空间 (mm_struct) 分配内存并初始化其链表、锁和引用计数。
 * 细节:
 * 1. kmalloc 分配 mm_struct，失败返回 NULL。
 * 2. 初始化 mmap_list 双向链表头和缓存指针 mmap_cache。
 * 3. 清空页目录/映射计数，并设置同步原语（私有锁、引用计数）。
 * 返回: 初始化完毕的 mm_struct，用于进程或内核线程的虚拟内存管理。
 */
struct mm_struct *                 // 返回指向 mm_struct 的指针
mm_create(void)                    // 创建并初始化新的地址空间描述符
{
    struct mm_struct *mm = kmalloc(sizeof(struct mm_struct)); // 申请 mm_struct 内存

    if (mm != NULL)               // 确认内存申请成功
    {
        list_init(&(mm->mmap_list)); // 初始化 VMA 链表头节点
        mm->mmap_cache = NULL;       // 清空最近访问的缓存指针
        mm->pgdir = NULL;            // 尚未绑定页目录
        mm->map_count = 0;           // 当前 VMA 数量归零

        mm->sm_priv = NULL;          // 交换管理私有数据置空

        set_mm_count(mm, 0);         // 设置引用计数为 0
        lock_init(&(mm->mm_lock));   // 初始化地址空间自旋锁
    }
    return mm;                       // 返回创建结果（可能为 NULL）
}

// vma_create - alloc a vma_struct & initialize it. (addr range: vm_start~vm_end)
/*
 * 功能: 创建并初始化单个虚拟内存区域 (VMA)，记录起止地址与权限标志。
 * 说明: 仅执行结构体分配与字段赋值，不与 mm_struct 建立关联。
 */
struct vma_struct *                              // 返回 VMA 指针
vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags)
{                                                 // 根据起止地址与标志创建 VMA
    struct vma_struct *vma = kmalloc(sizeof(struct vma_struct)); // 为 VMA 申请内存

    if (vma != NULL)                             // 若成功分配
    {
        vma->vm_start = vm_start;                // 记录区域起始虚拟地址
        vma->vm_end = vm_end;                    // 记录区域结束虚拟地址
        vma->vm_flags = vm_flags;                // 保存权限与属性标志
    }
    return vma;                                  // 返回创建的 VMA（可能为 NULL）
}

// find_vma - find a vma  (vma->vm_start <= addr <= vma_vm_end)
/*
 * 功能: 在 mm 的 VMA 链表中查找覆盖给定地址的区域，并维护缓存以加速下次查询。
 * 策略:
 * 1. 先检查 mmap_cache，命中则直接返回。
 * 2. 否则遍历 mmap_list（按起始地址排序）定位第一个满足 start<=addr<end 的 VMA。
 * 3. 查找成功时更新缓存；失败返回 NULL。
 */
struct vma_struct *                                 // 返回命中的 VMA
find_vma(struct mm_struct *mm, uintptr_t addr)       // 在 mm 中查找覆盖 addr 的区域
{
    struct vma_struct *vma = NULL;                  // 默认未找到
    if (mm != NULL)                                 // 仅在有效 mm 时执行
    {
        vma = mm->mmap_cache;                       // 优先尝试缓存命中
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr)) // 若缓存不满足
        {
            bool found = 0;                         // 标记是否找到
            list_entry_t *list = &(mm->mmap_list), *le = list; // 取得链表头
            while ((le = list_next(le)) != list)    // 遍历所有 VMA
            {
                vma = le2vma(le, list_link);        // 将链表节点转换为 VMA
                if (vma->vm_start <= addr && addr < vma->vm_end) // 检查是否覆盖地址
                {
                    found = 1;                      // 标记找到
                    break;                          // 退出循环
                }
            }
            if (!found)                             // 若遍历后仍未找到
            {
                vma = NULL;                         // 置空返回值
            }
        }
        if (vma != NULL)                            // 如果成功找到 VMA
        {
            mm->mmap_cache = vma;                   // 更新缓存，加速后续查询
        }
    }
    return vma;                                     // 返回查找结果
}

// check_vma_overlap - check if vma1 overlaps vma2 ?
static inline void                         // 内联辅助函数
check_vma_overlap(struct vma_struct *prev, struct vma_struct *next) // 确认相邻 VMA 无重叠
{
    assert(prev->vm_start < prev->vm_end);      // 前一个 VMA 的区间必须有效
    assert(prev->vm_end <= next->vm_start);     // 确保前后 VMA 不重叠且按序排列
    assert(next->vm_start < next->vm_end);      // 后一个 VMA 的区间同样有效
}

// insert_vma_struct -insert vma in mm's list link
/*
 * 功能: 按起始地址有序地将 VMA 插入 mm->mmap_list，并维护不重叠约束及 map_count。
 * 过程:
 * 1. 遍历链表找到第一个起始地址大于新 VMA 的位置。
 * 2. 与前后节点调用 check_vma_overlap 确认无交叠。
 * 3. 将 VMA 链入双向链表，设置 vm_mm，增加 map_count。
 */
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma)
{
    assert(vma->vm_start < vma->vm_end);    // 新 VMA 必须具备有效区间
    list_entry_t *list = &(mm->mmap_list);  // 取得 VMA 链表头节点
    list_entry_t *le_prev = list, *le_next; // 初始化插入位置的前后指针

    list_entry_t *le = list;               // 从表头开始遍历
    while ((le = list_next(le)) != list)   // 逐个节点向后检查
    {
        struct vma_struct *mmap_prev = le2vma(le, list_link); // 取当前节点对应的 VMA
        if (mmap_prev->vm_start > vma->vm_start)              // 找到起始地址更大的节点
        {
            break;                                           // 停止遍历，准备插入
        }
        le_prev = le;                                        // 更新前驱指针
    }

    le_next = list_next(le_prev);        // 插入位置的后继节点

    /* check overlap */
    if (le_prev != list)                 // 若存在前驱 VMA
    {
        check_vma_overlap(le2vma(le_prev, list_link), vma); // 校验与前驱不重叠
    }
    if (le_next != list)                 // 若存在后继 VMA
    {
        check_vma_overlap(vma, le2vma(le_next, list_link)); // 校验与后继不重叠
    }

    vma->vm_mm = mm;                     // 反向记录所属 mm
    list_add_after(le_prev, &(vma->list_link)); // 将新 VMA 插入链表

    mm->map_count++;                     // 更新 VMA 计数
}

// mm_destroy - free mm and mm internal fields
/*
 * 功能: 释放 mm_struct 以及它所拥有的所有 VMA 结构，要求引用计数已归零。
 * 步骤: 逐个从 mmap_list 中摘下 VMA 并 kfree，最后释放 mm 本身。
 */
void mm_destroy(struct mm_struct *mm)
{
    assert(mm_count(mm) == 0);                   // 确保没有引用者

    list_entry_t *list = &(mm->mmap_list), *le;  // 获取 VMA 链表头
    while ((le = list_next(list)) != list)       // 遍历链表中所有节点
    {
        list_del(le);                           // 从链表摘除当前节点
        kfree(le2vma(le, list_link));           // 释放对应的 VMA 结构
    }
    kfree(mm);                                   // 释放 mm 结构自身
    mm = NULL;                                   // 清空局部指针以防误用
}

int mm_map(struct mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags, // 在 mm 中映射一段虚拟区间
           struct vma_struct **vma_store)                                      // 可选返回新建 VMA
{
    /*
     * 功能: 在用户地址空间中新增一段 [addr, addr+len) 的 VMA，自动页对齐。
     * 流程:
     * 1. 计算对齐后的 start/end，并确认完整落在用户空间。
     * 2. 确保与现有 VMA 不重叠（利用 find_vma 检查）。
     * 3. 创建 VMA、插入链表，并可选地返回指针给调用者。
     * 返回: 成功 0，非法参数 -E_INVAL，内存不足 -E_NO_MEM。
     */
    uintptr_t start = ROUNDDOWN(addr, PGSIZE), end = ROUNDUP(addr + len, PGSIZE); // 对齐起止地址
    if (!USER_ACCESS(start, end))            // 若区域不在用户空间
    {
        return -E_INVAL;                    // 返回非法参数
    }

    assert(mm != NULL);                     // mm 不可为 NULL

    int ret = -E_INVAL;                     // 默认错误码

    struct vma_struct *vma;                 // 临时 VMA 指针
    if ((vma = find_vma(mm, start)) != NULL && end > vma->vm_start) // 检测与已有 VMA 冲突
    {
        goto out;                           // 重叠则直接结束
    }
    ret = -E_NO_MEM;                        // 预设分配失败码

    if ((vma = vma_create(start, end, vm_flags)) == NULL) // 创建新 VMA
    {
        goto out;                           // 内存不足则退出
    }
    insert_vma_struct(mm, vma);             // 将 VMA 插入链表
    if (vma_store != NULL)                  // 如需返回 VMA
    {
        *vma_store = vma;                   // 写回给调用者
    }
    ret = 0;                                // 设置成功返回值

out:
    return ret;                             // 返回执行结果
}

int dup_mmap(struct mm_struct *to, struct mm_struct *from) // 复制 from 的映射到 to
{
    /*
     * 功能: 在 fork 等场景下复制父 mm 的 VMA 布局和物理页面内容到子 mm。
     * 步骤:
     * 1. 遍历 from->mmap_list，为每个 VMA 创建副本并插入 to。
     * 2. 调用 copy_range 逐页复制页表和物理页，实现独立地址空间。
     * 若任一步骤失败则返回 -E_NO_MEM，调用者需负责回滚。
     */
    assert(to != NULL && from != NULL);                 // 子父地址空间都必须有效
    list_entry_t *list = &(from->mmap_list), *le = list; // 从父 mm 的 VMA 链表尾部开始
    while ((le = list_prev(le)) != list)                 // 逆序遍历，保持插入顺序正确
    {
        struct vma_struct *vma, *nvma;                  // 原 VMA 与新 VMA 指针
        vma = le2vma(le, list_link);                    // 取得父 mm 的 VMA
        nvma = vma_create(vma->vm_start, vma->vm_end, vma->vm_flags); // 复制元信息
        if (nvma == NULL)                               // 分配失败
        {
            return -E_NO_MEM;                           // 直接返回
        }

        insert_vma_struct(to, nvma);                    // 将新 VMA 插入子 mm

        bool share = 0;                                 // 当前实现采用拷贝策略
        if (copy_range(to->pgdir, from->pgdir, vma->vm_start, vma->vm_end, share) != 0) // 复制页表与物理页
        {
            return -E_NO_MEM;                           // 遇到内存不足即失败
        }
    }
    return 0;                                           // 全部复制成功
}

void exit_mmap(struct mm_struct *mm) // 解除 mm 内所有映射并回收页表
{
    /*
     * 功能: 在进程退出时撤销所有用户态映射并释放多余页表。
     * 两阶段:
     * 1. 遍历 VMA，调用 unmap_range 清除 PTE 并回收物理页。
     * 2. 再次遍历，调用 exit_range 释放空的页表页，降低内核占用。
     */
    assert(mm != NULL && mm_count(mm) == 0);       // mm 必须存在且无人引用
    pde_t *pgdir = mm->pgdir;                      // 缓存页目录指针
    list_entry_t *list = &(mm->mmap_list), *le = list; // 获取 VMA 链表
    while ((le = list_next(le)) != list)           // 第一次遍历所有 VMA
    {
        struct vma_struct *vma = le2vma(le, list_link); // 取得当前 VMA
        unmap_range(pgdir, vma->vm_start, vma->vm_end);  // 解除其所有页映射
    }
    while ((le = list_next(le)) != list)           // 第二次遍历回收空页表
    {
        struct vma_struct *vma = le2vma(le, list_link); // 取得当前 VMA
        exit_range(pgdir, vma->vm_start, vma->vm_end);   // 释放页表页面
    }
}

bool copy_from_user(struct mm_struct *mm, void *dst, const void *src, size_t len, bool writable) // 从用户态读取数据
{
    /*
     * 功能: 将用户空间数据复制到内核缓冲区，复制前校验访问权限。
     * writable 参数指示用户源地址是否需要具备写权限（如调试场景）。
     */
    if (!user_mem_check(mm, (uintptr_t)src, len, writable)) // 验证源地址合法性与权限
    {
        return 0;                                          // 校验失败返回 0
    }
    memcpy(dst, src, len);                                 // 执行内存拷贝
    return 1;                                              // 表示成功
}

bool copy_to_user(struct mm_struct *mm, void *dst, const void *src, size_t len) // 将数据写入用户态
{
    /*
     * 功能: 将内核数据写回用户空间地址，复制前确保目标区域可写且合法。
     */
    if (!user_mem_check(mm, (uintptr_t)dst, len, 1)) // 目标区域必须可写
    {
        return 0;                                   // 不通过直接失败
    }
    memcpy(dst, src, len);                          // 写入用户内存
    return 1;                                       // 返回成功
}

// vmm_init - initialize virtual memory management
//          - now just call check_vmm to check correctness of vmm
void vmm_init(void) // 虚拟内存子系统初始化入口
{
    /*
     * 功能: 虚拟内存管理模块入口，目前主要运行自检例程 check_vmm。
     */
    check_vmm(); // 启动自检确保虚拟内存子系统可用
}

// check_vmm - check correctness of vmm
static void                      // 静态辅助函数
check_vmm(void)                  // 运行虚拟内存子系统自检
{
    /*
     * 功能: 顶层自检，依次运行各组件的验证函数（目前启用 check_vma_struct）。
     * 通过后打印成功信息，帮助确认内核早期初始化状态。
     */
    // size_t nr_free_pages_store = nr_free_pages();

    check_vma_struct();              // 检查 VMA 插入/查找逻辑
    // check_pgfault();

    cprintf("check_vmm() succeeded.\n"); // 输出测试通过信息
}

static void                          // 静态验证函数
check_vma_struct(void)               // 自检 VMA 链表正确性
{
    /*
     * 功能: 针对 VMA 链表操作进行全面自检，验证插入顺序、查找结果和边界情况。
     * 流程:
     * 1. 创建 mm，并插入两批按起始地址排序的 VMA，测试 insert_vma_struct。
     * 2. 遍历链表检查顺序正确。
     * 3. 调用 find_vma 测试命中/未命中场景，确保缓存逻辑有效。
     * 4. 最后销毁 mm，确保无内存泄露。
     */
    // size_t nr_free_pages_store = nr_free_pages();

    struct mm_struct *mm = mm_create();
    assert(mm != NULL);                      // 确保 mm 创建成功

    int step1 = 10, step2 = step1 * 10;      // 设定测试区间数量

    int i;                                   // 循环变量
    for (i = step1; i >= 1; i--)             // 首先插入一批逆序 VMA
    {
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0); // 创建 VMA
        assert(vma != NULL);                                     // 检查分配
        insert_vma_struct(mm, vma);                               // 插入链表
    }

    for (i = step1 + 1; i <= step2; i++)     // 再插入更多顺序 VMA
    {
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0); // 创建 VMA
        assert(vma != NULL);                                     // 确保成功
        insert_vma_struct(mm, vma);                               // 插入链表
    }

    list_entry_t *le = list_next(&(mm->mmap_list)); // 指向链表首个实际节点

    for (i = 1; i <= step2; i++)               // 检查链表顺序是否正确
    {
        assert(le != &(mm->mmap_list));        // 不应提前到达尾部
        struct vma_struct *mmap = le2vma(le, list_link); // 取得当前 VMA
        assert(mmap->vm_start == i * 5 && mmap->vm_end == i * 5 + 2); // 验证区间
        le = list_next(le);                   // 前进到下一个 VMA
    }

    for (i = 5; i <= 5 * step2; i += 5)       // 针对一系列地址测试 find_vma
    {
        struct vma_struct *vma1 = find_vma(mm, i);       // 命中区间开始
        assert(vma1 != NULL);                            // 应该存在
        struct vma_struct *vma2 = find_vma(mm, i + 1);   // 命中区间内点
        assert(vma2 != NULL);
        struct vma_struct *vma3 = find_vma(mm, i + 2);   // 超出区间
        assert(vma3 == NULL);                            // 应为 NULL
        struct vma_struct *vma4 = find_vma(mm, i + 3);   // 更远位置
        assert(vma4 == NULL);
        struct vma_struct *vma5 = find_vma(mm, i + 4);   // 更远位置
        assert(vma5 == NULL);

        assert(vma1->vm_start == i && vma1->vm_end == i + 2); // 验证 VMA 起止
        assert(vma2->vm_start == i && vma2->vm_end == i + 2);
    }

    for (i = 4; i >= 0; i--)                // 检查低地址不存在 VMA
    {
        struct vma_struct *vma_below_5 = find_vma(mm, i); // 查询地址小于 5 的情况
        if (vma_below_5 != NULL)       // 若意外命中
        {
            cprintf("vma_below_5: i %x, start %x, end %x\n", i, vma_below_5->vm_start, vma_below_5->vm_end); // 打印调试信息
        }
        assert(vma_below_5 == NULL);      // 预期应为 NULL
    }

    mm_destroy(mm);                        // 释放测试用 mm

    cprintf("check_vma_struct() succeeded!\n"); // 输出成功提示
}
bool user_mem_check(struct mm_struct *mm, uintptr_t addr, size_t len, bool write) // 核实用户区间合法性
{
    /*
     * 功能: 检查一段用户空间地址区间是否被合法映射且权限满足要求。
     * 要点:
     * 1. 对非 NULL mm，需确保区间位于 USER_ACCESS 范围。
     * 2. 逐个定位覆盖 start 的 VMA，并检查权限标志（VM_READ/VM_WRITE）。
     * 3. 当写目标包含栈区域时，限制栈增长不超过一页的保护区。
     * 4. 若 mm 为 NULL，则允许访问内核线性映射区 (KERN_ACCESS)。
     */
    if (mm != NULL)                               // 若传入具体 mm
    {
        if (!USER_ACCESS(addr, addr + len))       // 区间必须完全位于用户空间
        {
            return 0;                            // 越界直接失败
        }
        struct vma_struct *vma;                  // 用于迭代的 VMA 指针
        uintptr_t start = addr, end = addr + len; // 记录起止地址
        while (start < end)                      // 遍历覆盖整个区间
        {
            if ((vma = find_vma(mm, start)) == NULL || start < vma->vm_start) // 找不到或存在空洞
            {
                return 0;                        // 判定非法
            }
            if (!(vma->vm_flags & ((write) ? VM_WRITE : VM_READ))) // 权限不足
            {
                return 0;                        // 拒绝访问
            }
            if (write && (vma->vm_flags & VM_STACK)) // 写入栈区域需额外限制
            {
                if (start < vma->vm_start + PGSIZE)
                { // check stack start & size // 防止栈向下溢出超过保护页
                    return 0;                    // 不允许
                }
            }
            start = vma->vm_end;                 // 跳到下一个 VMA 区段
        }
        return 1;                                // 所有检查通过
    }
    return KERN_ACCESS(addr, addr + len);        // mm 为 NULL 时按内核地址区判断
}