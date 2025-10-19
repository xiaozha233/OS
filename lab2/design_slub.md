# SLUB 分配器设计说明

## 1. 设计目标
- 在 `lab2` 的物理内存管理框架中提供对象级别的内存分配能力，降低频繁的小对象分配对 buddy 系统的压力。
- 保持内存分配延迟稳定，避免使用临时的 `kmalloc`/`kfree` 包装，每个缓存对象大小固定。
- 通过日志与断言易于调试，方便课程实验中定位问题。

## 2. 总体架构
```
Buddy System   <-- 提供页级分配 (alloc_pages/free_pages)
     │
     ▼
SLUB Allocator <-- 提供对象级分配 (kmalloc/kfree, kmem_cache_* 接口)
```
- 下层仍沿用课程提供的 buddy 系统，负责整页的分配与回收。
- 上层 SLUB 负责维护多个按对象大小划分的 `kmem_cache`，每个 cache 由若干个 slab 组成。
- slab 默认占用 1 个物理页，页首保存 `slab_t` 结构，其余空间切分为等长对象。

## 3. 核心数据结构
### 3.1 `kmem_cache_t`
- `name`/`size`/`align`：缓存名称、对象大小及对齐要求。
- `slabs_full`/`slabs_partial`/`slabs_empty`：记录该 cache 中 slab 的三种状态。
- `num_objs_per_slab`：单个 slab 可容纳对象数量。
- `num_slabs`：后续可用于统计，目前未强制维护。
- `cache_link`：挂入全局 `cache_list`，便于扩展自定义缓存。

### 3.2 `slab_t`
- `slab_link`：挂入 cache 的状态链表。
- `free_list`：单向链表，指向 slab 内首个空闲对象。
- `inuse`/`objects`：已使用对象数量与总对象数量。
- `page`：底层占用的 `Page` 描述符，用于回收时交还 buddy。
- `cache`：反向指针，释放时快速定位所属 cache。

### 3.3 辅助数组
- `cache_pool[MAX_CACHES]`：静态缓存池，存放所有创建的 cache 结构。
- `size_caches[NUM_SIZE_CACHES]` + `size_cache_sizes[]`：预制的一组常用缓存（16~2048 字节），供 `kmalloc` 快速选择。
- `size_cache_names[]`：每个通用缓存的名称，便于日志识别。
- `caches_ready`：标记 cache 是否初始化完成，防止初始化前调用。

## 4. 初始化流程
1. `slub_init` 清零缓存池、初始化全局链表并重置 `cache_count`。
2. 调用 `buddy_init()` 初始化下层页分配器。
3. 遍历 `size_cache_sizes`，通过 `kmem_cache_create` 创建通用缓存并填充 `size_caches`。
4. 创建成功后设置 `caches_ready = 1`，后续 `kmalloc` 才会对缓存进行检索。

## 5. 缓存创建 `kmem_cache_create`
- 检查对象大小、对齐合法性及缓存池容量。
- 对齐对象大小，计算 `num_objs_per_slab = floor((PGSIZE - sizeof(slab_t)) / size)`。
- 初始化三个 slab 链表并挂入全局 `cache_list`。
- 通过日志输出 cache 的基础信息。

## 6. slab 生命周期
### 6.1 创建 `slab_create`
1. 调用 `pmm_manager->alloc_pages(1)` 向 buddy 申请一个页。
2. 取得页首虚拟地址，覆盖为 `slab_t` 结构，初始化 `page/cache/inuse/objects` 等字段。
3. 使用对象大小切分剩余空间，构造单向空闲链表并赋值给 `free_list`。
4. 清零 `page->property` 避免沿用 buddy 中的块管理信息。

### 6.2 释放（待扩展）
- 当前 `slab_destroy` 仍为 TODO。课程要求可以保留占位，后续可实现引用计数归零就交还 buddy。

## 7. 分配路径 `kmem_cache_alloc`
1. 优先从 `slabs_partial` 取 slab；若无，则尝试 `slabs_empty`；都没有时新建 slab。
2. `slab_alloc_obj` 弹出 `free_list` 头部对象并增加 `inuse`。
3. 根据 slab 新状态迁移链表：空 → 部分使用 或 部分使用 → 满。
4. 返回对象指针。

## 8. 释放路径 `kmem_cache_free`
1. `obj_to_slab` 通过 `ROUNDDOWN(obj, PGSIZE)` 取得 slab 头地址，避免依赖 `Page.property`。
2. 调整 `was_full/was_partial` 状态，调用 `slab_free_obj` 将对象插回 `free_list`。
3. 根据 slab 的 `inuse` 迁移到 `slabs_partial` 或 `slabs_empty`。
4. 后续可根据 `slabs_empty` 中的冗余 slab 实现回收策略。

## 9. 通用接口 `kmalloc/kfree`
- `kmalloc`：
  1. 特殊情况 `size == 0` 直接返回 `NULL`。
  2. 调用 `find_cache` 在 `size_caches` 中查找第一个满足 `size <= cache_size` 的缓存。
  3. 若未找到，返回 `NULL` 并记录日志，后续可扩展落入 buddy 直接分配。
  4. 调用 `kmem_cache_alloc` 分配对象并打印调试信息。
- `kfree`：
  1. 校验空指针。
  2. 使用 `obj_to_slab` 定位 slab，再取出 `slab->cache`。
  3. 调用 `kmem_cache_free` 完成释放。

## 10. 与 Buddy 系统的交互
- `slab_alloc_pages`/`slab_free_pages`/`slub_nr_free_pages` 均简单转发到 buddy 对应接口。
- `slub_init_memmap` 把来自内核的可用物理页直接交由 buddy 管理，SLUB 不额外维护页信息。
- `slub_check` 运行期间触发 `buddy_alloc_pages` 日志，可帮助分析 slab 生成频率。

## 11. 调试与日志
- 关键路径通过 `cprintf` 输出 cache/slab 状态，方便在 QEMU 中追踪。
- `assert` 和 `panic` 用于捕捉异常，例如 `slab_alloc_obj` 返回 `NULL` 视为严重逻辑错误。
- 对象释放时若 `obj_to_slab` 返回 `NULL` 或 cache 不存在，会输出提示，便于定位非法指针。

## 12. 测试策略
- `slub_check`：
  - 测试 1：分别分配 32/64/128 字节，验证基本 `kmalloc/kfree` 流程。
  - 测试 2：多次分配 64 字节对象，验证在同一 slab 中的连续分配与回收。
- `make qemu`：观察日志 `=== All Slub tests passed! ===`，确认核心功能正常。
- `make grade`：待课程脚本更新后可扩展自动化比对；目前输出中 `memory management: slub_pmm_manager` 可佐证替换成功。

## 13. 后续可优化方向
- 实现 `slab_destroy` 与空 slab 回收策略，降低内存占用。
- 维护 `num_slabs`/`nr_partial` 等统计信息，提供诊断接口。
- 引入对象构造/析构回调，为更复杂的数据结构提供钩子。
- 根据分配模式调节 slab 粒度或支持多页 slab。

## 14. 参考
- Linux SLUB 分配器原理（简化版本）。
- buddy 系统与 `kmalloc` 框架。
