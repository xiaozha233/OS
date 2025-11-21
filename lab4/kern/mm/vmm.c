#include <vmm.h>
#include <sync.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <error.h>
#include <pmm.h>
#include <riscv.h>
#include <kmalloc.h>

/*
  虚拟内存管理(VMM)设计包含两个核心部分：mm_struct (mm) 和 vma_struct (vma)
  
  mm_struct (内存管理结构体):
    - 是一组连续虚拟内存区域的内存管理器
    - 这些虚拟内存区域共享同一个页目录表(PDT)
    - 管理整个进程的虚拟地址空间
  
  vma_struct (虚拟内存区域结构体):
    - 表示一块连续的虚拟内存区域
    - 每个vma描述一段具有相同属性的虚拟地址范围
    - 在mm中通过线性链表组织所有的vma
  
---------------
  mm相关函数:
   全局函数
     struct mm_struct * mm_create(void)
       - 创建并初始化一个新的内存管理结构体
     
     void mm_destroy(struct mm_struct *mm)
       - 销毁内存管理结构体并释放所有相关资源
     
     int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr)
       - 处理页面错误异常
--------------
  vma相关函数:
   全局函数
     struct vma_struct * vma_create (uintptr_t vm_start, uintptr_t vm_end,...)
       - 创建一个新的虚拟内存区域结构体
     
     void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma)
       - 将vma插入到mm的链表中，保持地址有序
     
     struct vma_struct * find_vma(struct mm_struct *mm, uintptr_t addr)
       - 查找包含指定地址的vma
   
   局部函数
     inline void check_vma_overlap(struct vma_struct *prev, struct vma_struct *next)
       - 检查两个vma是否存在地址重叠
---------------
   正确性检查函数
     void check_vmm(void);
       - 检查虚拟内存管理模块的整体正确性
     
     void check_vma_struct(void);
       - 检查vma结构体的创建、插入、查找功能
     
     void check_pgfault(void);
       - 检查页面错误处理功能
*/

// 调试辅助函数：打印vma和mm的详细信息
// print_vma - 打印虚拟内存区域(vma)的详细信息
// @name: 打印标识名称
// @vma: 要打印的虚拟内存区域结构体指针
void print_vma(char *name, struct vma_struct *vma)
{
    cprintf("-- %s print_vma --\n", name);
    cprintf("   mm_struct: %p\n", vma->vm_mm);
    cprintf("   vm_start,vm_end: %x,%x\n", vma->vm_start, vma->vm_end);
    cprintf("   vm_flags: %x\n", vma->vm_flags);
    cprintf("   list_entry_t: %p\n", &vma->list_link);
}

// print_mm - 打印内存管理结构体(mm)的详细信息
// @name: 打印标识名称
// @mm: 要打印的内存管理结构体指针
// 功能：遍历并打印mm中所有的vma信息
void print_mm(char *name, struct mm_struct *mm)
{
    cprintf("-- %s print_mm --\n", name);
    cprintf("   mmap_list: %p\n", &mm->mmap_list);
    cprintf("   map_count: %d\n", mm->map_count);
    list_entry_t *list = &mm->mmap_list;
    for (int i = 0; i < mm->map_count; i++)
    {
        list = list_next(list);
        print_vma(name, le2vma(list, list_link));
    }
}

static void check_vmm(void);
static void check_vma_struct(void);
static void check_pgfault(void);

// mm_create - 分配并初始化一个内存管理结构体
// @return: 成功返回新创建的mm_struct指针，失败返回NULL
// 功能说明：
//   1. 通过kmalloc分配mm_struct所需的内存空间
//   2. 初始化mmap_list链表头，用于管理所有的vma
//   3. 初始化mmap_cache为NULL，用于缓存最近访问的vma
//   4. 初始化pgdir为NULL，页目录表指针
//   5. 初始化map_count为0，记录vma的数量
//   6. 初始化sm_priv为NULL，用于交换管理器的私有数据
struct mm_struct *
mm_create(void)
{
    struct mm_struct *mm = kmalloc(sizeof(struct mm_struct));

    if (mm != NULL)
    {
        list_init(&(mm->mmap_list));
        mm->mmap_cache = NULL;
        mm->pgdir = NULL;
        mm->map_count = 0;
        mm->sm_priv = NULL;
    }
    return mm;
}

// vma_create - 分配并初始化一个虚拟内存区域结构体
// @vm_start: 虚拟内存区域的起始地址
// @vm_end: 虚拟内存区域的结束地址
// @vm_flags: 虚拟内存区域的标志位(如可读、可写、可执行等权限)
// @return: 成功返回新创建的vma_struct指针，失败返回NULL
// 功能说明：
//   创建一个地址范围为[vm_start, vm_end)的虚拟内存区域
//   注意：地址范围是左闭右开区间
struct vma_struct *
vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags)
{
    struct vma_struct *vma = kmalloc(sizeof(struct vma_struct));

    if (vma != NULL)
    {
        vma->vm_start = vm_start;
        vma->vm_end = vm_end;
        vma->vm_flags = vm_flags;
    }
    return vma;
}

// find_vma - 查找包含指定地址的虚拟内存区域
// @mm: 内存管理结构体指针
// @addr: 要查找的虚拟地址
// @return: 找到返回对应的vma_struct指针，未找到返回NULL
// 查找条件：vma->vm_start <= addr < vma->vm_end
// 功能说明：
//   1. 首先检查mmap_cache缓存，如果缓存命中则直接返回
//   2. 缓存未命中则遍历mmap_list链表查找
//   3. 找到后更新mmap_cache以加速下次查找
//   4. 利用局部性原理，缓存可以提高查找效率
struct vma_struct *
find_vma(struct mm_struct *mm, uintptr_t addr)
{
    struct vma_struct *vma = NULL;
    if (mm != NULL)
    {
        vma = mm->mmap_cache;
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr))
        {
            bool found = 0;
            list_entry_t *list = &(mm->mmap_list), *le = list;
            while ((le = list_next(le)) != list)
            {
                vma = le2vma(le, list_link);
                if (vma->vm_start <= addr && addr < vma->vm_end)
                {
                    found = 1;
                    break;
                }
            }
            if (!found)
            {
                vma = NULL;
            }
        }
        if (vma != NULL)
        {
            mm->mmap_cache = vma;
        }
    }
    return vma;
}

// check_vma_overlap - 检查两个vma是否存在地址重叠
// @prev: 地址较低的vma
// @next: 地址较高的vma
// 功能说明：
//   1. 确保每个vma的起始地址小于结束地址(vm_start < vm_end)
//   2. 确保前一个vma的结束地址不超过后一个vma的起始地址(prev->vm_end <= next->vm_start)
//   3. 这样可以保证所有vma在地址空间上不重叠且有序排列
//   4. 如果检查失败会触发assert断言
static inline void
check_vma_overlap(struct vma_struct *prev, struct vma_struct *next)
{
    assert(prev->vm_start < prev->vm_end);
    assert(prev->vm_end <= next->vm_start);
    assert(next->vm_start < next->vm_end);
}

// insert_vma_struct - 将vma插入到mm的链表中
// @mm: 内存管理结构体指针
// @vma: 要插入的虚拟内存区域结构体指针
// 功能说明：
//   1. 按照地址从小到大的顺序将vma插入到mmap_list链表中
//   2. 遍历链表找到合适的插入位置(第一个起始地址大于当前vma的位置)
//   3. 插入前检查与前后vma是否存在地址重叠
//   4. 设置vma的vm_mm指针指向所属的mm
//   5. 将vma的list_link插入到链表中
//   6. 更新mm的map_count计数器
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma)
{
    assert(vma->vm_start < vma->vm_end);
    list_entry_t *list = &(mm->mmap_list);
    list_entry_t *le_prev = list, *le_next;

    // 遍历链表，找到第一个起始地址大于vma->vm_start的位置
    list_entry_t *le = list;
    while ((le = list_next(le)) != list)
    {
        struct vma_struct *mmap_prev = le2vma(le, list_link);
        if (mmap_prev->vm_start > vma->vm_start)
        {
            break;
        }
        le_prev = le;
    }

    le_next = list_next(le_prev);

    /* 检查地址重叠：确保新插入的vma不与前后的vma地址重叠 */
    if (le_prev != list)
    {
        check_vma_overlap(le2vma(le_prev, list_link), vma);
    }
    if (le_next != list)
    {
        check_vma_overlap(vma, le2vma(le_next, list_link));
    }

    vma->vm_mm = mm;
    list_add_after(le_prev, &(vma->list_link));

    mm->map_count++;
}

// mm_destroy - 销毁内存管理结构体并释放所有资源
// @mm: 要销毁的内存管理结构体指针
// 功能说明：
//   1. 遍历mmap_list链表，释放所有的vma结构体
//   2. 从链表中删除每个vma节点
//   3. 使用kfree释放每个vma占用的内存
//   4. 最后释放mm结构体本身占用的内存
//   5. 将mm指针设置为NULL，防止悬空指针
void mm_destroy(struct mm_struct *mm)
{

    list_entry_t *list = &(mm->mmap_list), *le;
    while ((le = list_next(list)) != list)
    {
        list_del(le);
        kfree(le2vma(le, list_link)); // 释放vma结构体
    }
    kfree(mm); // 释放mm结构体
    mm = NULL;
}

// vmm_init - 初始化虚拟内存管理模块
// 功能说明：
//   1. 初始化虚拟内存管理子系统
//   2. 调用check_vmm()进行正确性检查
//   3. 验证vma的创建、插入、查找等基本操作
//   4. 确保虚拟内存管理模块工作正常后才能继续系统启动
void vmm_init(void)
{
    check_vmm();
}

// check_vmm - 检查虚拟内存管理模块的正确性
// 功能说明：
//   1. 调用check_vma_struct()检查vma相关操作
//   2. 可选调用check_pgfault()检查页面错误处理(当前被注释)
//   3. 所有检查通过后打印成功信息
static void
check_vmm(void)
{
    check_vma_struct();
    // check_pgfault(); // 页面错误检查(待实现)

    cprintf("check_vmm() succeeded.\n");
}

// check_vma_struct - 检查vma结构体相关操作的正确性
// 功能说明：
//   这是一个综合测试函数，测试vma的创建、插入、查找功能
//   测试策略：
//     1. 创建100个vma，地址范围为[i*5, i*5+2)，i从1到100
//     2. 先逆序插入10个vma (i=10到1)
//     3. 再正序插入90个vma (i=11到100)
//     4. 验证插入后链表中vma按地址有序排列
//     5. 测试find_vma()在不同地址上的查找结果
//     6. 验证边界情况和空洞区域的查找
static void
check_vma_struct(void)
{
    // 创建一个新的内存管理结构体用于测试
    struct mm_struct *mm = mm_create();
    assert(mm != NULL);

    // step1=10: 第一批插入10个vma
    // step2=100: 总共插入100个vma
    int step1 = 10, step2 = step1 * 10;

    int i;
    // 第一阶段：逆序插入vma (i从10到1)
    // 目的：测试insert_vma_struct在逆序插入时能否正确排序
    // 每个vma的地址范围：[i*5, i*5+2)
    // 例如：i=10时地址为[50,52)，i=1时地址为[5,7)
    for (i = step1; i >= 1; i--)
    {
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
    }

    // 第二阶段：正序插入vma (i从11到100)
    // 目的：测试insert_vma_struct在正序插入时的性能和正确性
    // 每个vma的地址范围：[i*5, i*5+2)
    // 例如：i=11时地址为[55,57)，i=100时地址为[500,502)
    for (i = step1 + 1; i <= step2; i++)
    {
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
    }

    list_entry_t *le = list_next(&(mm->mmap_list));

    // 验证阶段1：检查链表中所有vma是否按地址从小到大正确排序
    // 遍历链表，验证每个vma的地址范围是否符合预期
    // 预期：第i个vma的地址范围应该是[i*5, i*5+2)
    for (i = 1; i <= step2; i++)
    {
        assert(le != &(mm->mmap_list));
        struct vma_struct *mmap = le2vma(le, list_link);
        assert(mmap->vm_start == i * 5 && mmap->vm_end == i * 5 + 2);
        le = list_next(le);
    }

    // 验证阶段2：测试find_vma函数的查找功能
    // 对每个vma区间[i*5, i*5+2)测试5个地址点
    // i: vma起始地址，应该能找到
    // i+1: vma内部地址，应该能找到
    // i+2: vma结束地址(不包含)，应该找不到
    // i+3: vma外部地址(空洞区域)，应该找不到  
    // i+4: vma外部地址(空洞区域)，应该找不到
    for (i = 5; i <= 5 * step2; i += 5)
    {
        struct vma_struct *vma1 = find_vma(mm, i);
        assert(vma1 != NULL);  // i在vma范围内
        struct vma_struct *vma2 = find_vma(mm, i + 1);
        assert(vma2 != NULL);  // i+1在vma范围内
        struct vma_struct *vma3 = find_vma(mm, i + 2);
        assert(vma3 == NULL);  // i+2不在vma范围内(左闭右开)
        struct vma_struct *vma4 = find_vma(mm, i + 3);
        assert(vma4 == NULL);  // i+3在空洞区域
        struct vma_struct *vma5 = find_vma(mm, i + 4);
        assert(vma5 == NULL);  // i+4在空洞区域

        // 验证找到的vma地址范围正确
        assert(vma1->vm_start == i && vma1->vm_end == i + 2);
        assert(vma2->vm_start == i && vma2->vm_end == i + 2);
    }

    // 验证阶段3：测试边界情况
    // 测试地址0-4，这些地址都在最小vma(起始地址为5)之前
    // 预期：这些地址都不应该找到任何vma
    // 这个测试确保find_vma不会错误地返回不包含目标地址的vma
    for (i = 4; i >= 0; i--)
    {
        struct vma_struct *vma_below_5 = find_vma(mm, i);
        if (vma_below_5 != NULL)
        {
            cprintf("vma_below_5: i %x, start %x, end %x\n", i, vma_below_5->vm_start, vma_below_5->vm_end);
        }
        assert(vma_below_5 == NULL);  // 地址小于5应该找不到vma
    }

    // 清理测试环境：销毁mm及其包含的所有vma
    mm_destroy(mm);

    cprintf("check_vma_struct() succeeded!\n");
}