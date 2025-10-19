#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

extern const struct pmm_manager buddy_pmm_manager;

// 导出这些函数供 SLUB 使用
void buddy_init_memmap(struct Page *base, size_t n);
struct Page* buddy_alloc_pages(size_t n);
void buddy_free_pages(struct Page *base, size_t n);
size_t buddy_nr_free_pages(void);
void buddy_init(void);
#endif /* !__KERN_MM_BUDDY_PMM_H__ */