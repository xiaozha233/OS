#ifndef __KERN_MM_VMM_H__                         // 头文件保护开头，避免重复包含
#define __KERN_MM_VMM_H__                         // 定义保护宏

#include <defs.h>                                 // 内核通用定义
#include <list.h>                                 // 双向链表工具
#include <memlayout.h>                            // 内存布局常量/宏
#include <sync.h>                                 // 互斥锁等同步原语

// pre define
struct mm_struct;                                 // 前向声明 mm_struct 供 vma_struct 使用

// the virtual continuous memory area(vma), [vm_start, vm_end),
// addr belong to a vma means  vma.vm_start<= addr <vma.vm_end
struct vma_struct
{
    struct mm_struct *vm_mm; // 所属的 mm_struct，表示共享同一页目录
    uintptr_t vm_start;      // 区间起始虚拟地址（含）
    uintptr_t vm_end;        // 区间结束虚拟地址（不含）
    uint32_t vm_flags;       // 访问权限与属性标志
    list_entry_t list_link;  // 链入 mm_struct 按起址有序的链表
};

#define le2vma(le, member) \                       // 将链表结点还原为 vma_struct 指针
    to_struct((le), struct vma_struct, member)

#define VM_READ 0x00000001                        // 可读权限标志
#define VM_WRITE 0x00000002                       // 可写权限标志
#define VM_EXEC 0x00000004                        // 可执行权限标志
#define VM_STACK 0x00000008                       // 标记栈区域

// the control struct for a set of vma using the same PDT
struct mm_struct
{
    list_entry_t mmap_list;        // 所有 VMA 构成的有序链表头
    struct vma_struct *mmap_cache; // 最近访问的 VMA 缓存，加速查找
    pde_t *pgdir;                  // 与该 mm 关联的页目录
    int map_count;                 // 当前 VMA 数目
    void *sm_priv;                 // 供换页管理器使用的私有数据
    int mm_count;                  // 共享该 mm 的进程/线程引用计数
    lock_t mm_lock;                // 在 dup/映射调整时的互斥锁
};

struct vma_struct *find_vma(struct mm_struct *mm, uintptr_t addr); // 根据地址查找覆盖它的 VMA
struct vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags); // 创建 VMA 描述
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma); // 将 VMA 插入链表保持有序

struct mm_struct *mm_create(void);                  // 分配并初始化 mm_struct
void mm_destroy(struct mm_struct *mm);              // 释放 mm 和其 VMA

void vmm_init(void);                                // 虚拟内存管理子系统初始化
int mm_map(struct mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags,
           struct vma_struct **vma_store);          // 建立地址区间映射
int mm_unmap(struct mm_struct *mm, uintptr_t addr, size_t len); // 解除映射
int dup_mmap(struct mm_struct *to, struct mm_struct *from);     // 复制一个 mm 的映射到另一个
void exit_mmap(struct mm_struct *mm);               // 退出时释放所有映射
uintptr_t get_unmapped_area(struct mm_struct *mm, size_t len); // 查找满足长度的空闲区
int mm_brk(struct mm_struct *mm, uintptr_t addr, size_t len);  // 扩展或收缩堆区

extern volatile unsigned int pgfault_num;           // 统计页故障次数
extern struct mm_struct *check_mm_struct;           // 自检用的 mm 指针

bool user_mem_check(struct mm_struct *mm, uintptr_t start, size_t len, bool write); // 检查用户区访问合法性
bool copy_from_user(struct mm_struct *mm, void *dst, const void *src, size_t len, bool writable); // 从用户态复制数据
bool copy_to_user(struct mm_struct *mm, void *dst, const void *src, size_t len); // 向用户态写数据

static inline int
mm_count(struct mm_struct *mm)
{
    return mm->mm_count;                            // 直接读取引用计数
}

static inline void
set_mm_count(struct mm_struct *mm, int val)
{
    mm->mm_count = val;                             // 设置引用计数
}

static inline int
mm_count_inc(struct mm_struct *mm)
{
    mm->mm_count += 1;                              // 递增引用计数
    return mm->mm_count;                            // 返回更新后的值
}

static inline int
mm_count_dec(struct mm_struct *mm)
{
    mm->mm_count -= 1;                              // 递减引用计数
    return mm->mm_count;                            // 返回新值
}

static inline void
lock_mm(struct mm_struct *mm)
{
    if (mm != NULL)                                 // mm 存在才加锁
    {
        lock(&(mm->mm_lock));                       // 获取互斥锁保护结构
    }
}

static inline void
unlock_mm(struct mm_struct *mm)
{
    if (mm != NULL)                                 // mm 有效才解锁
    {
        unlock(&(mm->mm_lock));                     // 释放互斥锁
    }
}

#endif /* !__KERN_MM_VMM_H__ */                     // 头文件保护结束
