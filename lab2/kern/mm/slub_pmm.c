#include <slub_pmm.h>
#include <pmm.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Slub 分配器实现
 * 
 * 架构：
 * - 底层：Buddy System 提供页级别分配
 * - 上层：Slub 提供对象级别分配
 * 
 * 核心思想：
 * 1. 为常用大小的对象创建专用的 kmem_cache
 * 2. 每个 cache 管理多个 slab (由一个或多个页组成)
 * 3. 每个 slab 被分割成固定大小的 objects
 */

// 定义宏：从 list_entry 获取包含它的 slab_t 结构
#define le2slab(le, member) \
    to_struct((le), slab_t, member)

// Slab 分配器管理的缓存池
#define MAX_CACHES 32
static kmem_cache_t cache_pool[MAX_CACHES];
static int cache_count = 0;


// 全局 cache 链表 (管理所有创建的 kmem_cache)
static list_entry_t cache_list;

// 通用大小的 cache (用于 kmalloc)
#define NUM_SIZE_CACHES 8
static kmem_cache_t *size_caches[NUM_SIZE_CACHES];
static size_t size_cache_sizes[NUM_SIZE_CACHES] = {
    16, 32, 64, 128, 256, 512, 1024, 2048
};

// ========== 辅助函数 ==========

// 计算一个 Page 中可以容纳多少个对象
static size_t calculate_objects_per_slab(size_t obj_size) {
    // 简化：假设每个 slab 只占用 1 个 Page (4KB)
    size_t slab_size = PGSIZE;
    
    // 预留一些空间给 slab_t 元数据
    size_t usable_size = slab_size - sizeof(slab_t);
    
    return usable_size / obj_size;
}

// 从对象地址获取其所属的 slab
static slab_t* obj_to_slab(void *obj) {
    // 找到对象所在的 Page
    struct Page *page = pa2page(PADDR(obj));
    
    // Page 结构的 property 字段存储指向 slab_t 的指针
    return (slab_t*)(uintptr_t)page->property;
}

// ========== Slab 管理函数 ==========

// 创建一个新的 slab
static slab_t* slab_create(kmem_cache_t *cache) {
    // 1. 向buddy系统请求一个页面
    struct Page *page = pmm_manager->alloc_pages(1);
    if (!page) {
        return NULL;
    }
    // 2. 获取page的虚拟地址
    void *page_addr = KADDR(page2pa(page));
    // 3. 在page开头放置slab_t结构
    slab_t *slab = (slab_t *)page_addr;
    // 4. 初始化slab结构
    slab->page = page;
    slab->inuse = 0;
    slab->objects = cache->num_objs_per_slab;
    slab->free_list = NULL;
    //5. 计算第一个object的地址
    void *first_obj = (void *)((uintptr_t)page_addr + sizeof(slab_t));
    // 6. 构建空闲object链表
    void *current_obj = first_obj;
    for (uint32_t i = 0; i < slab->objects; i++) {
        void *next_obj = (i == slab->objects - 1) ? NULL :
                            (void *)((uintptr_t)current_obj + cache->size);
        // 将当前object的下一个指针指向下一个object
        *(void **)current_obj = next_obj;

        current_obj = next_obj;
    }
    // 7. 设置slab的free_list指针指向第一个object
    slab->free_list = first_obj;
    // 8. 将slab的地址存储在page的property字段中
    page->property = (uintptr_t)slab;
    // 9. 返回新创建的slab

    return slab;
}

// 销毁一个 slab
static void slab_destroy(kmem_cache_t *cache, slab_t *slab) {
    // TODO: 第二步实现
}

// 从 slab 中分配一个对象
static void* slab_alloc_obj(slab_t *slab) {
    // 1. 检查是否有可用对象
    if (slab->free_list == NULL) {
        return NULL; // slab 已满
    }
    // 2. 取出链表头的对象
    void *obj = slab->free_list;
    // 3. 更新 free_list 指针
    slab->free_list = *(void **)obj;
    // 4. 更新 inuse 计数
    slab->inuse++;
    // 5. 返回分配的对象
    return obj;
}

// 释放对象回 slab
static void slab_free_obj(slab_t *slab, void *obj) {
    // 1. 让obj指向当前的free_list头
    *(void **)obj = slab->free_list;
    // 2. 更新slab的free_list指针
    slab->free_list = obj;
    // 3. 更新inuse计数
    slab->inuse--;
}

// ========== Kmem Cache 管理函数 ==========

kmem_cache_t* kmem_cache_create(const char *name, size_t size, size_t align) {
    // 1. 检查size参数是否有效
    if (size < SLUB_MIN_SIZE || size > SLUB_MAX_SIZE) {
        cprintf("Invalid kmem_cache size: %zu\n", size);
        return NULL;
    }
    // 2. 检查是否还有可用的cache槽位
    if (cache_count >= MAX_CACHES) {
        cprintf("Maximum number of kmem_caches reached\n");
        return NULL;
    }
    // 3. 从cache_pool中分配一个kmem_cache_t
    kmem_cache_t *cache = &cache_pool[cache_count++];
    // 4. 初始化kmem_cache_t结构
    cache->name = name;
    // 对齐size
    size_t aligned_size = (size + align - 1) & ~(align - 1);
    cache->size = aligned_size;
    cache->align = align;
    cache->num_slabs = 0;
    cache->num_objs_per_slab = calculate_objects_per_slab(cache->size);
    if (cache->num_objs_per_slab == 0) {
        cprintf("Object size too large for slab: %zu\n", cache->size);
        return NULL;
    }
    // 5. 初始化三个slab链表头
    list_init(&cache->slabs_full);
    list_init(&cache->slabs_partial);
    list_init(&cache->slabs_empty);
    // 6. 将新cache加入全局cache链表
    list_add(&cache_list, &cache->cache_link);

    cprintf("Created cache '%s': obj_size=%d, objs_per_slab=%d\n",
            name, cache->size, cache->num_objs_per_slab);

    // 7. 返回新创建的cache
    return cache;
}

void* kmem_cache_alloc(kmem_cache_t *cache) {
    // 1. 找一个可用的slab
    slab_t *slab = NULL;

    bool was_empty = 0;// 标记是否从空闲链表取出，用于后续处理


    if (!list_empty(&cache->slabs_partial)) {
        // 从部分使用的slab链表中取一个
        list_entry_t *le = list_next(&cache->slabs_partial);
        slab = le2slab(le, slab_link);
    } else if (!list_empty(&cache->slabs_empty)) {
        // 从空闲的slab链表中取一个
        list_entry_t *le = list_next(&cache->slabs_empty);
        slab = le2slab(le, slab_link);
        was_empty = 1;//先标记，后面再处理
    } else {
        // 没有可用的slab，创建一个新的
        slab = slab_create(cache);
        if (!slab) {
            return NULL; // 创建slab失败，内存不足
        }
        // 新创建的slab加入空闲链表
        list_add(&cache->slabs_empty, &slab->slab_link);
        was_empty = 1;//先标记，后面再处理
        cprintf("Created new slab for cache '%s'\n", cache->name);
    }
    // 2. 从slab中分配一个对象
    void *obj = slab_alloc_obj(slab);
    if (!obj) {
        panic("slab_alloc_obj failed unexpectedly");
    }
    // 3. 根据slab状态更新slab链表
    if (was_empty && slab->inuse > 0) {
        // slab之前是空的，现在变成部分使用
        list_del(&slab->slab_link);
        list_add(&cache->slabs_partial, &slab->slab_link);
    } else if (slab->inuse == slab->objects) {
        // slab现在满了
        list_del(&slab->slab_link);
        list_add(&cache->slabs_full, &slab->slab_link);
    }
    return obj;
}

void kmem_cache_free(kmem_cache_t *cache, void *obj) {
    // 1. 从对象地址获取所属slab
    slab_t *slab = obj_to_slab(obj);
    assert(slab != NULL);
    // 2. 记录释放前的slab状态，用于后续处理
    bool was_full = (slab->inuse == slab->objects);
    bool was_partial = (slab->inuse > 0 && slab->inuse < slab->objects);
    // 3. 调用底层函数释放对象
    slab_free_obj(slab, obj);
    // 4. 根据slab状态更新slab链表
    if (was_full && slab->inuse < slab->objects) {
        list_del(&slab->slab_link);
        list_add(&cache->slabs_partial, &slab->slab_link);  
    } else if (was_partial && slab->inuse == 0) {
        list_del(&slab->slab_link);
        list_add(&cache->slabs_empty, &slab->slab_link);  
    }
}

void kmem_cache_destroy(kmem_cache_t *cache) {
    // 1. 检查是否完全空闲
    if (!list_empty(&cache->slabs_full) ||
        !list_empty(&cache->slabs_partial)) {
        cprintf("Cannot destroy cache '%s': slabs still in use\n", cache->name);
        return;
        }
    // 2. 遍历空闲slab链表，销毁所有slab
    while (!list_empty(&cache->slabs_empty)) {
        list_entry_t *le = list_next(&cache->slabs_empty);
}

// ========== 通用分配接口 ==========

void* kmalloc(size_t size) {
    // TODO: 第七步实现
    return NULL;
}

void kfree(void *obj) {
    // TODO: 第七步实现
}

// ========== Slub PMM 初始化 ==========

static void slub_init(void) {
    cprintf("Slub allocator initializing...\n");
    list_init(&cache_list);
}

static void slub_init_memmap(struct Page *base, size_t n) {
    // Slub 不直接管理物理页，委托给底层的 Buddy System
    cprintf("Slub: delegating %d pages to buddy system\n", n);
}

static struct Page* slub_alloc_pages(size_t n) {
    // 不应该调用此函数，因为 Slub 是上层分配器
    panic("slub_alloc_pages should not be called directly!");
    return NULL;
}

static void slub_free_pages(struct Page *base, size_t n) {
    // 不应该调用此函数
    panic("slub_free_pages should not be called directly!");
}

static size_t slub_nr_free_pages(void) {
    // 返回底层 Buddy System 的空闲页数
    extern size_t buddy_nr_free_pages(void);
    return buddy_nr_free_pages();
}

static void slub_check(void) {
    cprintf("\n=== Slub Allocator Comprehensive Test ===\n");
    // TODO: 第八步实现测试
}

// Slub PMM Manager
const struct pmm_manager slub_pmm_manager = {
    .name = "slub_pmm_manager",
    .init = slub_init,
    .init_memmap = slub_init_memmap,
    .alloc_pages = slub_alloc_pages,
    .free_pages = slub_free_pages,
    .nr_free_pages = slub_nr_free_pages,
    .check = slub_check,
};