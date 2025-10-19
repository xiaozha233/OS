# Buddy System 物理内存管理器设计说明

## 1. 目标与取舍
- **目标**：在 `lab2` 中实现一个可替换 First-Fit/Best-Fit 的 buddy 物理内存管理器，突出快速合并与良好外部碎片控制。
- **优势**：
  - O(1) 伙伴定位带来快速合并；
  - 等幂块结构让空闲链表维护简单；
  - 适合频繁分配/释放的内核场景。
- **代价**：
  - 强制 2^k 粒度，平均约 20% 内部碎片；
  - 最大块尺寸受 `MAX_ORDER` 限制。

## 2. 架构概览

```
物理页数组 (struct Page[]) ──▶ Buddy 管理器 ──▶ alloc_pages/free_pages
                            │
                      free_area[order]
```

1. `pmm_manager` 指针切换到 `buddy_pmm_manager`；
2. `free_area[0..MAX_ORDER]` 维护不同阶次的空闲块链表；
3. `property` 字段被复用为块阶 (`order`)，便于常数时间判断伙伴是否匹配。

## 3. 核心数据结构

| 结构/变量              | 作用                                                         |
| ---------------------- | ------------------------------------------------------------ |
| `free_area_t free_area[]` | 每阶一个双向循环链表，记录空闲块及其数量 `nr_free`             |
| `struct Page *buddy_base` | 初始化时记下的起始页，用于计算页索引                          |
| `size_t buddy_total_pages`| 总页数边界，防止伙伴索引越界                                 |
| `Page.property`         | 存储当前块的阶次；仅块首页有效                                 |

辅助宏/函数：
- `calculate_order(n)`: 求满足 $2^{order} \ge n$ 的最小 `order`；
- `get_page_order(page)`/`set_page_order(page, order)`：访问 `property`；
- `free_list(order)`、`nr_free(order)` 简化链表与计数操作。

## 4. 关键流程

### 4.1 初始化 `buddy_init_memmap`
1. 清理 `Page` 元数据，重置 `ref`、`flags`、`property`；
2. 以“最大能容纳的 2^k 块”策略拆分连续空闲区域；
3. 对每块首页调用 `set_page_order` 并插入对应阶链表，维护 `nr_free` 计数。

### 4.2 分配 `buddy_alloc_pages`
1. 计算目标阶 `order = ceil(log2(n))`；
2. 自下而上查找第一个非空链表 `current_order`；
3. 若 `current_order > order`，循环拆分：
  - 块一分为二，后一半加入 `free_list(current_order-1)`；
  - 继续拆分直至达到目标阶；
4. 返回最终块首页，并标记 `PageProperty` 为 0。

### 4.3 释放 `buddy_free_pages`
1. 计算释放块阶次并清理页元数据；
2. 进入 `while` 循环尝试合并：
  - 伙伴索引：`buddy_idx = page_idx ^ (1 << order)`；
  - 伙伴存在、空闲、阶相等→从链表摘除并更新 `page` 与 `order`；
3. 插入最终阶链表并设置 `PageProperty`，`nr_free(order)++`。

### 4.4 查询 `buddy_nr_free_pages`
遍历所有阶，累加 `nr_free(order) << order` 即可。

## 5. 测试矩阵（节选）

| 类别                 | 目的                                        | 覆盖点                                   |
| -------------------- | ------------------------------------------- | --------------------------------------- |
| 基本功能             | 单页、多页、2^k 分配与回收                  | 正确分配对齐、回收后可再次分配          |
| 分裂/合并            | 观察链表迁移、验证伙伴 XOR 公式            | `nr_free` 恢复、阶次匹配                |
| 边界                 | 最大块、超限、非法参数                     | 保护性检查                              |
| 压力                 | 多轮随机分配释放                           | 无内存泄漏                              |
| 指标统计             | 打印各阶块数量、计算内部碎片               | 设计权衡量化                            |

所有测试可在 `make qemu` 自检输出和 `grade.sh` 中观察，完整日志附于实验报告。

## 6. 性能与特性
- **复杂度**：分配/释放 ~ O(log N)；合并单次 O(1)。
- **内部碎片**：请求非 2^k 页时将向上取整，平均浪费约 18%~25%。
- **外部碎片**：伙伴合并保持链表紧凑，外部碎片率显著低于 First-Fit。
- **内存开销**：额外的 `free_area` 链表与 `property` 字段重用，可忽略不计。

## 7. 后续改进
- 懒惰合并（延迟 merge，批量处理）；
- 依据系统总 RAM 调整 `MAX_ORDER`，避免过深遍历；
- 与 SLUB 等小对象分配器协同，构建分层内存管理。

---

> 更多实现细节（函数原型、辅助宏、调试日志）可直接查阅 `kern/mm/buddy_pmm.c` 与提交历史。
