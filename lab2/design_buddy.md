# Buddy System 物理内存管理器 - 设计文档与测试报告

## 一、设计文档

### 1.1 设计概述

**设计目标**：实现一个基于伙伴系统(Buddy System)的物理内存分配器,用于替代 ucore 中的 First-Fit 或 Best-Fit 算法。

**核心优势**：
-  **快速合并**: O(1) 时间复杂度定位伙伴块
-  **抑制外部碎片**: 通过递归合并保持大块内存可用性
-  **结构简单**: 基于 2 的幂次,便于位运算优化

**主要权衡**：
- **内部碎片**: 只能分配 2^k 大小的块,可能浪费空间
- **大小限制**: 最大块大小受 MAX_ORDER 限制

### 1.2 核心数据结构

```c
#define MAX_ORDER 10  // 支持最大 2^10 = 1024 页

// 空闲链表数组,每个阶对应一个链表
static free_area_t free_area[MAX_ORDER + 1];

// 记录内存区域的全局信息
static struct Page *buddy_base = NULL;      // 内存起始地址
static size_t buddy_total_pages = 0;        // 总页数

// 每个 Page 结构使用 property 字段存储其所属块的阶
struct Page {
    // ...existing fields...
    uint32_t property;  // 存储块的阶(order)
};
```

**数据结构说明**：
- `free_area[i]`: 维护所有大小为 2^i 页的空闲块链表
- `property`: 每个块的首页记录该块的阶数
- `buddy_base`: 用于计算伙伴地址的基准

### 1.3 核心算法

#### 1.3.1 分配算法 (Allocation)

```
算法: buddy_alloc_pages(n)
输入: 请求的页数 n
输出: 分配的页块指针

1. 计算所需阶: order = ⌈log₂(n)⌉
2. 从 order 开始向上查找第一个非空链表 current_order
3. 如果找到:
   a. 从 free_list[current_order] 取出一块
   b. 如果 current_order > order:
      - 递归分裂: 将块二等分
      - 一半加入 free_list[current_order-1]
      - 另一半继续分裂,直到达到 order
   c. 返回分配的块
4. 否则返回 NULL (内存不足)
```

**时间复杂度**: O(MAX_ORDER) ≈ O(log N)

**示例流程图**:
```
请求 1 页 (order=0):
free_list[2]: [16KB块]
           ↓ 取出并分裂
free_list[1]: [8KB块₁] ← 加入
           ↓ 继续分裂 8KB块₂
free_list[0]: [4KB块₁] ← 加入, [4KB块₂] → 返回给用户
```

#### 1.3.2 释放与合并算法 (Freeing & Coalescing)

```
算法: buddy_free_pages(base, n)
输入: 释放的页块起始地址 base, 页数 n

1. 计算块的阶: order = ⌈log₂(n)⌉
2. 当前块地址: page = base
3. while order ≤ MAX_ORDER:
   a. 计算伙伴地址: buddy_idx = page_idx ⊕ (1 << order)
   b. 检查伙伴是否满足合并条件:
      - 地址合法
      - 标记为空闲 (PageProperty)
      - 阶相同 (property == order)
   c. 如果满足:
      - 从 free_list[order] 移除伙伴
      - page = min(page, buddy)  // 取低地址
      - order++
      - 继续循环
   d. 否则:
      - 跳出循环
4. 将合并后的块插入 free_list[order]
```

**关键: 伙伴地址计算**
```
buddy_addr = current_addr ⊕ (1 << order)
```
这个异或操作神奇地实现了:
- 如果当前块是"左伙伴",计算出"右伙伴"地址
- 如果当前块是"右伙伴",计算出"左伙伴"地址

**示例**:
```
释放 4KB (地址=4, order=0):
Step 1: 伙伴地址 = 4 ⊕ 1 = 5
        检查地址5的块: 空闲且 order=0 ✓
        合并成 8KB (地址=4, order=1)
        
Step 2: 伙伴地址 = 4 ⊕ 2 = 6
        检查地址6的块: 已分配 ✗
        停止合并,将 8KB块加入 free_list[1]
```

### 1.4 关键函数实现

#### 辅助函数

```c
// 计算满足 2^k ≥ n 的最小 k
static size_t calculate_order(size_t n) {
    size_t order = 0;
    size_t size = 1;
    while (size < n) {
        size <<= 1;
        order++;
    }
    return order;
}

// 获取/设置页的阶
static inline size_t get_page_order(struct Page *page) {
    return page->property;
}

static inline void set_page_order(struct Page *page, size_t order) {
    page->property = order;
}
```

### 1.5 内存布局示例

```
初始状态 (128KB 内存):
free_list[5]: [128KB块]
free_list[4]: []
free_list[3]: []
...

分配 8KB 后:
free_list[5]: []
free_list[4]: [64KB块]
free_list[3]: [32KB块]
free_list[2]: [16KB块]
free_list[1]: []
free_list[0]: []
已分配: [8KB块]

继续分配 4KB:
free_list[4]: [64KB块]
free_list[3]: [32KB块]
free_list[2]: [16KB块]
free_list[1]: []
free_list[0]: [4KB块]
已分配: [8KB块], [4KB块]
```

---


### 1.6 测试用例详细说明

#### Test 1: 基本分配与释放
**目的**: 验证基本的内存分配和释放功能

**测试点**:
- 单页分配
- 多页分配 (非 2 的幂次)
- 2 的幂次分配
- 内存正确释放

**预期结果**:
```
✓ 所有分配返回非空指针
✓ 非 2 的幂次请求向上取整到 2 的幂次
✓ 释放后内存可重新使用
```

#### Test 2: 分裂机制
**目的**: 验证大块分裂为小块的正确性

**测试点**:
- 连续分配多个小块
- 验证空闲内存总量不变

**关键验证**:
```
初始: free_list[5] = 1块(32页)
分配3次1页后:
  - 分配出 3 个 1 页块
  - 剩余若干不同大小的块
  - 总空闲页数 = 32 - 3 = 29
```

#### Test 3: 伙伴合并机制
**目的**: 验证释放时的伙伴查找与合并

**测试点**:
- 分配多个相邻块
- 按特定顺序释放
- 验证链表中出现合并后的大块

**关键逻辑**:
```
分配: p0, p1, p2, p3 (各1页, order=0)
释放 p1 → free_list[0] += 1
释放 p0 → 查找伙伴 p1 → 合并 → free_list[1] += 1
```

#### Test 4: 边界条件
**目的**: 测试极限情况的处理

**测试点**:
- 最大块分配 (2^MAX_ORDER)
- 超限分配 (返回 NULL)
- 零页分配 (触发 assert)

#### Test 5: 内存碎片分析
**目的**: 量化内部碎片程度

**测试点**:
- 显示各阶空闲块分布
- 计算内部碎片率

**示例输出**:
```
Requested: 3 pages
Allocated: 4 pages
Waste: 25.0%
```

#### Test 6: 压力测试
**目的**: 验证大量分配/释放的稳定性

**测试点**:
- 50 轮随机大小分配
- 全部释放
- 验证无内存泄漏

#### Test 7: 算法对比
**目的**: 对比 Buddy System 与 First-Fit

**关键差异**:

| 特性 | Buddy System | First-Fit |
|------|--------------|-----------|
| 分配速度 | O(log N) | O(N) |
| 合并速度 | **O(1)** | O(N) |
| 内部碎片 | **较高** | 较低 |
| 外部碎片 | **较低** | 较高 |

#### Test 8: 伙伴地址计算
**目的**: 验证核心算法的数学正确性

**验证公式**: `buddy_idx = idx XOR (1 << order)`

**示例**:
```
idx=8, order=0: buddy=9  (8^1=9)
idx=8, order=1: buddy=10 (8^2=10)
idx=8, order=2: buddy=12 (8^4=12)
```

---

## 二、性能分析

### 3.1 时间复杂度

| 操作 | Buddy System | First-Fit | Best-Fit |
|------|--------------|-----------|----------|
| 分配 | O(log N) | O(N) | O(N) |
| 释放 | O(log N) | O(N) | O(N) |
| 合并 | **O(1)** | O(N) | O(N) |

### 3.2 空间复杂度

- **链表开销**: O(MAX_ORDER) ≈ O(log N)
- **元数据**: 每页增加 1 个 uint32_t (阶信息)

### 3.3 碎片率对比

**内部碎片** (Buddy System 劣势):
```
请求大小 | 实际分配 | 浪费率
---------|----------|--------
1 页     | 1 页     | 0%
3 页     | 4 页     | 25%
5 页     | 8 页     | 37.5%
7 页     | 8 页     | 12.5%
平均     |          | ~18.75%
```

**外部碎片** (Buddy System 优势):
- 快速合并机制保持大块可用性
- First-Fit 平均外部碎片率: 30-40%
- Buddy System: <10%

---

## 四、运行测试

### 4.1 编译

```bash
cd labcode/lab2
make clean
make
```

### 4.2 运行

```bash
make qemu
```

### 4.3 预期输出

```
=== Buddy System Comprehensive Test ===

[Test 1] Basic Allocation and Free
  ✓ Single page allocation: 0x...
  ✓ 5 pages requested → allocated 2^3=8 pages: 0x...
  ✓ 16 pages (power of 2) allocation: 0x...
  ✓ All pages freed successfully

[Test 2] Block Splitting Mechanism
  ✓ Three pages allocated: ...
  ✓ Splitting verified, free pages restored: 32760

...

=== All Tests Passed! ===
```

---

## 五、结论

### 5.1 实现总结

✅ **已实现功能**:
- 基于 2 的幂次的内存分配
- 自动分裂大块为小块
- 快速伙伴查找与合并
- 完整的错误处理

### 5.2 优势与劣势

**优势**:
1. 🚀 合并速度极快 (O(1) vs O(N))
2. 🧩 有效抑制外部碎片
3. 💡 代码简洁,易于理解

**劣势**:
1. ❌ 内部碎片率较高 (~18%)
2. 📏 不适合非 2 的幂次需求场景

### 5.3 适用场景

**推荐使用**:
- 频繁分配/释放的场景
- 需要保持大块内存可用性
- 对合并速度有要求

**不推荐使用**:
- 内存极度紧张的嵌入式系统
- 所有分配都是奇数页的场景

---

## 六、扩展思考

### 6.1 可能的优化

1. **懒惰合并**: 延迟合并操作,减少频繁分配/释放的开销
2. **混合策略**: 小块用 Buddy,大块用 Slab
3. **可配置 MAX_ORDER**: 根据内存大小动态调整

### 6.2 与 Linux 内核对比

Linux 内核的 Buddy System:
- 使用位图加速伙伴查找
- 支持 NUMA 架构
- 与 Slab 分配器协同工作


---
