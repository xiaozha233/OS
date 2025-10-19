#ifndef __KERN_MM_SLUB_PMM_H__
#define __KERN_MM_SLUB_PMM_H__

#include <defs.h>
#include <list.h>
#include <memlayout.h>

// Slub 分配器的核心数据结构

// 单个对象的最小大小和最大大小
#define SLUB_MIN_SIZE 16
#define SLUB_MAX_SIZE 2048

// 对象缓存 (kmem_cache)
typedef struct kmem_cache {
    const char *name;            // 缓存名称，如 "task_struct"
    size_t size;                 // 每个对象的大小
    size_t align;                // 对齐要求
    
    list_entry_t slabs_full;     // 全满的 slab 链表
    list_entry_t slabs_partial;  // 部分使用的 slab 链表
    list_entry_t slabs_empty;    // 全空的 slab 链表
    
    uint32_t num_objs_per_slab;  // 每个 slab 包含的对象数
    uint32_t num_slabs;          // 该缓存中 slab 的总数
    
    list_entry_t cache_link;     // 用于链接到全局 cache 链表
} kmem_cache_t;

// Slab 状态
typedef struct slab {
    list_entry_t slab_link;      // 用于链接到 kmem_cache 的三个链表之一
    void *free_list;             // 指向 slab 内第一个空闲对象
    uint32_t inuse;              // 已分配的对象数量
    uint32_t objects;            // 该 slab 中对象的总数
    struct Page *page;           // 该 slab 占用的物理页
    kmem_cache_t *cache;         // 指向所属的 kmem_cache
} slab_t;



// Slub PMM 管理器接口
extern const struct pmm_manager slub_pmm_manager;

// 核心 API
kmem_cache_t* kmem_cache_create(const char *name, size_t size, size_t align);
void* kmem_cache_alloc(kmem_cache_t *cache);
void kmem_cache_free(kmem_cache_t *cache, void *obj);
void kmem_cache_destroy(kmem_cache_t *cache);

// 通用内存分配接口 (类似 Linux 的 kmalloc/kfree)
void* kmalloc(size_t size);
void kfree(void *obj);

#endif /* !__KERN_MM_SLUB_PMM_H__ */