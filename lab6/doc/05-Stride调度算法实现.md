# Lab6 进程调度 - Stride 调度算法实现

> **讲义版本**: 1.0  
> **覆盖标签**: [S46] - [S65]  
> **先修要求**: RR 调度算法、优先队列概念、整数溢出处理

---

## 📚 本章学习目标

完成本章学习后，你将能够：
- 理解 Stride Scheduling 调度算法的原理和数学基础
- 掌握斜堆（Skew Heap）数据结构的使用
- 正确处理 stride 值的整数溢出问题
- 实现 Stride 调度算法的五个核心函数

---

## 1. Stride 调度算法原理 [S46]

### 1.1 知识卡片：Stride Scheduling

| 字段 | 内容 |
|:---|:---|
| **名称** | Stride Scheduling（步进调度） |
| **要解决的问题** | 按优先级比例分配 CPU 时间，同时保持确定性 |
| **先修要求** | RR 调度、优先级概念、基础数学 |
| **论文出处** | Waldspurger & Weihl, 1995 |

### 1.2 直觉理解

想象一场**赛跑比赛**：
- 每个选手（进程）有不同的**步幅**（stride）
- 步幅大的选手跑得慢，步幅小的跑得快
- 每次选择**当前位置最靠后**的选手先跑
- 选手跑完一步后，位置增加一个步幅

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     Stride 调度直觉示意                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  进程      优先级    步幅(pass)     位置(stride)                            │
│  ────      ─────    ──────────     ────────────                            │
│  P1        6        100/6≈16       0 → 16 → 32 → 48 → ...                  │
│  P2        3        100/3≈33       0 → 33 → 66 → 99 → ...                  │
│  P3        1        100/1=100      0 → 100 → 200 → ...                     │
│                                                                             │
│  调度顺序（选择 stride 最小的）：                                           │
│  ─────────────────────────────                                             │
│  T0: P1(0), P2(0), P3(0) → 选 P1（或任一）                                 │
│  T1: P1(16), P2(0), P3(0) → 选 P2                                          │
│  T2: P1(16), P2(33), P3(0) → 选 P3                                         │
│  T3: P1(16), P2(33), P3(100) → 选 P1                                       │
│  T4: P1(32), P2(33), P3(100) → 选 P1                                       │
│  T5: P1(48), P2(33), P3(100) → 选 P2                                       │
│  ...                                                                        │
│                                                                             │
│  结果：P1 被调度的次数 ≈ P2 的 2 倍 ≈ P3 的 6 倍                            │
│       与优先级比例 6:3:1 相符！                                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```
[Fig·S46-1]: Stride 调度直觉示意图

### 1.3 形式化描述 [S47]

**核心概念**：

| 概念 | 符号 | 定义 |
|:---|:---|:---|
| 优先级 | priority | 进程的重要程度，数值越大越重要 |
| 步幅 | pass | `BIG_STRIDE / priority`，优先级越高步幅越小 |
| 步进值 | stride | 进程的"当前位置"，每次调度后增加 pass |
| 大步幅常数 | BIG_STRIDE | 一个足够大的常数，用于计算 pass |

**算法步骤**：

1. 为每个进程维护一个 `stride` 值，初始为 0
2. 计算每个进程的 `pass = BIG_STRIDE / priority`
3. 每次调度时，选择 `stride` **最小**的进程
4. 被选中的进程执行一个时间片后，`stride += pass`
5. 重复步骤 3-4

**关键公式**：

$$\text{pass}_i = \frac{\text{BIG\_STRIDE}}{\text{priority}_i}$$

$$\text{stride}_i^{\text{new}} = \text{stride}_i^{\text{old}} + \text{pass}_i$$

[Fig·S47-1]: Stride 算法公式

### 1.4 为什么 Stride 能保证比例分配？[S48]

**直觉证明**：

假设有两个进程 P1、P2，优先级分别为 $p_1$ 和 $p_2$。

- P1 的步幅：$\text{pass}_1 = \frac{B}{p_1}$
- P2 的步幅：$\text{pass}_2 = \frac{B}{p_2}$

经过足够多的调度后，两个进程的 stride 会趋于接近。设 P1 被调度 $n_1$ 次，P2 被调度 $n_2$ 次：

$$n_1 \times \text{pass}_1 \approx n_2 \times \text{pass}_2$$

$$n_1 \times \frac{B}{p_1} \approx n_2 \times \frac{B}{p_2}$$

$$\frac{n_1}{n_2} \approx \frac{p_1}{p_2}$$

**结论**：调度次数之比约等于优先级之比！

### 1.5 Stride vs RR 对比 [S49]

| 对比项 | RR | Stride |
|:---|:---|:---|
| 优先级支持 | ❌ 无 | ✅ 有 |
| 公平性 | 绝对公平 | 按优先级比例公平 |
| 确定性 | ✅ 确定 | ✅ 确定 |
| 实现复杂度 | 简单 | 中等 |
| 选择下一进程 | O(1) 链表头 | O(n) 遍历 / O(log n) 堆 |
| 适用场景 | 交互式系统 | 需要区分优先级的系统 |

[Fig·S49-1]: Stride 与 RR 对比表

---

## 2. 整数溢出问题 [S50]

### 2.1 问题描述

在实际实现中，`stride` 是不断增加的。使用 32 位无符号整数表示时，最终会**溢出**。

**示例**：

| 时刻 | P1.stride (实际) | P2.stride (实际) | 正确比较 | 实际比较 |
|:---:|:---:|:---:|:---:|:---:|
| T1 | 65534 | 65535 | P1 < P2 ✓ | P1 < P2 ✓ |
| T2 (P1+100) | 98 (溢出!) | 65535 | P1 > P2 ✓ | 98 < 65535 ❌ |

溢出后，直接比较 `P1.stride < P2.stride` 会得到**错误**结果！

### 2.2 解决方案：有符号差值比较 [S51]

**核心洞察**：

可以证明，在任意时刻：
$$\text{STRIDE\_MAX} - \text{STRIDE\_MIN} \leq \text{PASS\_MAX} \leq \text{BIG\_STRIDE}$$

只要 `BIG_STRIDE` 选择合适（不超过有符号整数的一半），就可以通过**计算差值并解释为有符号数**来正确比较。

**比较方法**：

```c
// 错误的比较方式
if (a->stride < b->stride)  // 溢出后可能出错

// 正确的比较方式
int32_t c = a->stride - b->stride;  // 无符号减法后转为有符号
if (c < 0) {
    // a 的 stride 更小
}
```

**原理**：

```
假设使用 16 位无符号整数：

P1.stride = 98  (理论值 65634)
P2.stride = 65535

差值 (无符号): 98 - 65535 = 98 - 65535 + 65536 = 99 (发生下溢)
解释为有符号 int16_t: 99

由于 99 > 0，说明 P1 > P2 ✓（正确！）
```
[Fig·S51-1]: 溢出处理原理

### 2.3 BIG_STRIDE 的选择 [S52]

**问题**：BIG_STRIDE 应该取多大？

**分析**：

- 使用 32 位无符号整数表示 stride
- 有符号 32 位整数范围：$[-2^{31}, 2^{31}-1]$
- 要使差值在有符号范围内：`STRIDE_MAX - STRIDE_MIN <= 2^31 - 1`
- 由于 `STRIDE_MAX - STRIDE_MIN <= BIG_STRIDE`
- 所以 `BIG_STRIDE <= 2^31 - 1 = 0x7FFFFFFF`

**ucore 实现**：

```c
#define BIG_STRIDE 0x7FFFFFFF  // 2^31 - 1
```

### 2.4 比较函数实现 [S53]

在 `default_sched_stride.c` 中：

```c
static int
proc_stride_comp_f(void *a, void *b)
{
    struct proc_struct *p = le2proc(a, lab6_run_pool);
    struct proc_struct *q = le2proc(b, lab6_run_pool);
    
    // 使用有符号差值比较
    int32_t c = p->lab6_stride - q->lab6_stride;
    
    if (c > 0)
        return 1;   // p > q
    else if (c == 0)
        return 0;   // p == q
    else
        return -1;  // p < q
}
```

---

## 3. 斜堆数据结构 [S54]

### 3.1 知识卡片：斜堆

| 字段 | 内容 |
|:---|:---|
| **名称** | 斜堆（Skew Heap） |
| **要解决的问题** | 高效实现优先队列，支持快速的插入和取最小值 |
| **时间复杂度** | 均摊 O(log n) 的插入、删除、合并 |
| **特点** | 自平衡、实现简单、不需要额外的平衡因子 |

### 3.2 为什么使用斜堆？[S55]

**对比链表实现**：

| 操作 | 链表 | 斜堆 |
|:---|:---|:---|
| 插入 | O(1) | O(log n) |
| 删除指定节点 | O(1) | O(log n) |
| 找最小值 | O(n) | O(1) |
| **选择下一个进程** | **O(n)** | **O(1)** |

对于 Stride 调度，`pick_next` 需要找到 stride 最小的进程：
- 链表：每次需要遍历整个队列
- 斜堆：堆顶就是最小元素

**结论**：当进程数量较多时，斜堆效率更高。

### 3.3 斜堆结构定义

```c
struct skew_heap_entry {
    struct skew_heap_entry *parent, *left, *right;
};
typedef struct skew_heap_entry skew_heap_entry_t;
```

### 3.4 斜堆核心操作 [S56]

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          斜堆核心操作                                        │
├─────────────────┬───────────────────────────────────────────────────────────┤
│     函数         │                      功能说明                             │
├─────────────────┼───────────────────────────────────────────────────────────┤
│                 │                                                           │
│ skew_heap_init  │  初始化堆节点，parent/left/right 都置为 NULL              │
│ (a)             │                                                           │
│                 │                                                           │
├─────────────────┼───────────────────────────────────────────────────────────┤
│                 │                                                           │
│ skew_heap_insert│  将节点 b 插入到以 a 为根的堆中                           │
│ (a, b, comp)    │  返回新堆的根节点                                         │
│                 │                                                           │
│                 │  内部调用 merge 实现                                      │
│                 │                                                           │
├─────────────────┼───────────────────────────────────────────────────────────┤
│                 │                                                           │
│ skew_heap_remove│  从以 a 为根的堆中删除节点 b                              │
│ (a, b, comp)    │  返回新堆的根节点                                         │
│                 │                                                           │
│                 │  将 b 的左右子树合并，替代 b 的位置                        │
│                 │                                                           │
├─────────────────┼───────────────────────────────────────────────────────────┤
│                 │                                                           │
│ skew_heap_merge │  合并两个堆                                               │
│ (a, b, comp)    │  返回合并后的根节点                                       │
│                 │                                                           │
│                 │  关键操作：合并后交换左右子树（保持平衡）                  │
│                 │                                                           │
└─────────────────┴───────────────────────────────────────────────────────────┘
```
[Fig·S56-1]: 斜堆核心操作表

### 3.5 斜堆操作示意 [S57]

**插入操作**：

```
原始堆:              插入 P4 (stride=12):

      P1(5)                  P1(5)
     /    \                 /    \
  P2(10)  P3(15)    →    P4(12)  P2(10)
                            \       \
                           P3(15)   ∅

                        （交换左右子树）
```

**获取最小值**：

```
堆的根节点就是 stride 最小的进程！

      P1(5)  ← 直接返回 P1
     /    \
  P2(10)  P3(15)
```
[Fig·S57-1]: 斜堆操作示意

---

## 4. Stride 五个核心函数实现 [S58]

### 4.1 stride_init：初始化 [S59]

#### 函数原型
```c
static void stride_init(struct run_queue *rq);
```

#### 实现思路

```
        stride_init(rq)
               │
               ▼
      ┌─────────────────────┐
      │ 1. 初始化链表       │
      │    （兼容两种实现）  │
      └────────┬────────────┘
               │
               ▼
      ┌─────────────────────┐
      │ 2. 斜堆根置为 NULL  │
      └────────┬────────────┘
               │
               ▼
      ┌─────────────────────┐
      │ 3. proc_num = 0     │
      └────────┬────────────┘
               │
               ▼
            完成
```
[Fig·S59-1]: stride_init 流程图

#### 代码实现

```c
static void
stride_init(struct run_queue *rq)
{
    // 初始化链表（用于链表实现）
    list_init(&(rq->run_list));
    
    // 初始化斜堆根节点为空
    rq->lab6_run_pool = NULL;
    
    // 进程数量为 0
    rq->proc_num = 0;
}
```

---

### 4.2 stride_enqueue：入队 [S60]

#### 函数原型
```c
static void stride_enqueue(struct run_queue *rq, struct proc_struct *proc);
```

#### 实现思路

```
        stride_enqueue(rq, proc)
               │
               ▼
      ┌───────────────────────────┐
      │ 1. 将 proc 插入斜堆       │
      │    skew_heap_insert       │
      └────────┬──────────────────┘
               │
               ▼
      ┌───────────────────────────┐
      │ 2. 重置时间片（如需要）    │
      └────────┬──────────────────┘
               │
               ▼
      ┌───────────────────────────┐
      │ 3. 设置 proc->rq = rq     │
      └────────┬──────────────────┘
               │
               ▼
      ┌───────────────────────────┐
      │ 4. proc_num++             │
      └────────┬──────────────────┘
               │
               ▼
            完成
```
[Fig·S60-1]: stride_enqueue 流程图

#### 代码实现（斜堆版本）

```c
#define USE_SKEW_HEAP 1

static void
stride_enqueue(struct run_queue *rq, struct proc_struct *proc)
{
#if USE_SKEW_HEAP
    // 使用斜堆实现
    // 将进程的 lab6_run_pool 节点插入堆中
    rq->lab6_run_pool = skew_heap_insert(
        rq->lab6_run_pool,           // 当前堆根
        &(proc->lab6_run_pool),      // 要插入的节点
        proc_stride_comp_f           // 比较函数
    );
#else
    // 使用链表实现
    list_add_before(&(rq->run_list), &(proc->run_link));
#endif

    // 重置时间片
    if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice)
    {
        proc->time_slice = rq->max_time_slice;
    }
    
    // 设置所属队列
    proc->rq = rq;
    
    // 增加进程数量
    rq->proc_num++;
}
```

---

### 4.3 stride_dequeue：出队 [S61]

#### 函数原型
```c
static void stride_dequeue(struct run_queue *rq, struct proc_struct *proc);
```

#### 代码实现

```c
static void
stride_dequeue(struct run_queue *rq, struct proc_struct *proc)
{
#if USE_SKEW_HEAP
    // 从斜堆中删除进程
    rq->lab6_run_pool = skew_heap_remove(
        rq->lab6_run_pool,           // 当前堆根
        &(proc->lab6_run_pool),      // 要删除的节点
        proc_stride_comp_f           // 比较函数
    );
#else
    // 从链表中删除
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    list_del_init(&(proc->run_link));
#endif

    // 减少进程数量
    rq->proc_num--;
}
```

---

### 4.4 stride_pick_next：选择下一个进程 [S62]

#### 函数原型
```c
static struct proc_struct *stride_pick_next(struct run_queue *rq);
```

#### 实现思路

```
        stride_pick_next(rq)
               │
               ▼
      ┌───────────────────────────┐
      │ 1. 堆为空？返回 NULL      │
      └────────┬──────────────────┘
               │
              否
               ▼
      ┌───────────────────────────┐
      │ 2. 获取堆顶进程           │
      │    （stride 最小的）      │
      └────────┬──────────────────┘
               │
               ▼
      ┌───────────────────────────┐
      │ 3. 更新 stride            │
      │    stride += pass         │
      └────────┬──────────────────┘
               │
               ▼
      ┌───────────────────────────┐
      │ 4. 返回进程指针           │
      └────────┬──────────────────┘
               │
               ▼
            完成
```
[Fig·S62-1]: stride_pick_next 流程图

#### 代码实现

```c
static struct proc_struct *
stride_pick_next(struct run_queue *rq)
{
#if USE_SKEW_HEAP
    // 斜堆为空
    if (rq->lab6_run_pool == NULL)
    {
        return NULL;
    }
    
    // 堆顶就是 stride 最小的进程
    struct proc_struct *p = le2proc(rq->lab6_run_pool, lab6_run_pool);
    
#else
    // 链表为空
    if (list_empty(&(rq->run_list)))
    {
        return NULL;
    }
    
    // 遍历链表找最小 stride
    list_entry_t *le = list_next(&(rq->run_list));
    struct proc_struct *p = le2proc(le, run_link);
    
    while ((le = list_next(le)) != &(rq->run_list))
    {
        struct proc_struct *proc = le2proc(le, run_link);
        // 使用有符号差值比较
        if ((int32_t)(proc->lab6_stride - p->lab6_stride) < 0)
        {
            p = proc;
        }
    }
#endif

    // 【关键】更新 stride 值
    if (p->lab6_priority == 0)
    {
        // 优先级为 0 时，避免除零，设为最大步幅
        p->lab6_stride += BIG_STRIDE;
    }
    else
    {
        // stride += BIG_STRIDE / priority
        p->lab6_stride += BIG_STRIDE / p->lab6_priority;
    }
    
    return p;
}
```

#### 代码解读

1. **斜堆实现**：`rq->lab6_run_pool` 就是堆顶（stride 最小的进程）
2. **链表实现**：需要遍历整个链表找最小值，O(n) 复杂度
3. **更新 stride**：选中进程后立即更新，为下次调度做准备
4. **优先级为 0 的处理**：避免除零错误，使用最大步幅

---

### 4.5 stride_proc_tick：时钟中断 [S63]

#### 代码实现

```c
static void
stride_proc_tick(struct run_queue *rq, struct proc_struct *proc)
{
    // 与 RR 完全相同
    if (proc->time_slice > 0)
    {
        proc->time_slice--;
    }
    if (proc->time_slice == 0)
    {
        proc->need_resched = 1;
    }
}
```

**说明**：Stride 算法也使用时间片机制，每个时间片结束后触发调度。

---

## 5. 切换到 Stride 调度器 [S64]

### 5.1 调度类定义

```c
struct sched_class stride_sched_class = {
    .name = "stride_scheduler",
    .init = stride_init,
    .enqueue = stride_enqueue,
    .dequeue = stride_dequeue,
    .pick_next = stride_pick_next,
    .proc_tick = stride_proc_tick,
};
```

### 5.2 在 sched_init 中切换

修改 `kern/schedule/sched.c`：

```c
// 方法1：在 default_sched.h 中声明
extern struct sched_class stride_sched_class;

void sched_init(void)
{
    list_init(&timer_list);

    // 使用 Stride 调度器
    sched_class = &stride_sched_class;  // 修改这一行！
    
    // 或者保留 RR
    // sched_class = &default_sched_class;

    rq = &__rq;
    rq->max_time_slice = MAX_TIME_SLICE;
    sched_class->init(rq);

    cprintf("sched class: %s\n", sched_class->name);
}
```

---

## 6. 运行示例与验证 [S65]

### 6.1 priority 测试程序分析

`user/priority.c` 是测试 Stride 调度的程序：

```c
// 创建 5 个子进程
for (i = 0; i < TOTAL; i++) {
    if ((pids[i] = fork()) == 0) {
        lab6_setpriority(i + 1);  // 设置优先级 1, 2, 3, 4, 5
        while (1) {
            spin_delay();
            ++acc[i];
            // 统计执行次数
        }
    }
}
```

### 6.2 预期输出

```
sched class: stride_scheduler
kernel_execve: pid = 2, name = "priority".
Breakpoint
set priority to 6
main: fork ok,now need to wait pids.
set priority to 5
set priority to 4
set priority to 3
set priority to 2
set priority to 1
child pid 7, acc 944000, time 2010 
child pid 6, acc 788000, time 2010 
child pid 5, acc 620000, time 2010 
child pid 4, acc 460000, time 2020 
child pid 3, acc 316000, time 2020 
...
stride sched correct result: 1 1 2 2 3
```

### 6.3 结果分析

```
优先级   执行次数(acc)   比例
───────  ────────────   ────
6        944000         ≈ 6x
5        788000         ≈ 5x
4        620000         ≈ 4x
3        460000         ≈ 3x
2        316000         ≈ 2x
1        (基准)          1x
```

**结论**：执行次数与优先级成正比，验证了 Stride 算法的正确性！

---

## 7. 易错点与调试技巧

### 7.1 常见错误

| 错误 | 原因 | 解决方案 |
|:---|:---|:---|
| 除零错误 | priority 为 0 | 在 pick_next 中检查 |
| 调度不均匀 | stride 比较方式错误 | 使用有符号差值比较 |
| 死循环 | 斜堆操作错误 | 检查比较函数返回值 |
| 进程丢失 | enqueue/dequeue 不匹配 | 添加断言检查 |

### 7.2 调试技巧

```c
// 在 pick_next 中添加调试输出
cprintf("pick_next: proc %d, stride %d, priority %d\n",
        p->pid, p->lab6_stride, p->lab6_priority);
```

---

## 8. 自测题

### 题目 1（判断题）
**题目**：在 Stride 调度中，优先级越高的进程，其 pass 值越大。

<details>
<summary>点击查看答案</summary>

**答案**：❌ 错误

**解释**：pass = BIG_STRIDE / priority。优先级越高，priority 越大，pass 越小。pass 小意味着 stride 增长慢，更容易被选中。

</details>

### 题目 2（单选题）
**题目**：在 32 位无符号整数下，为保证 stride 比较正确，BIG_STRIDE 的最大值是：

A. 2^32 - 1  
B. 2^31 - 1  
C. 2^31  
D. 2^16 - 1

<details>
<summary>点击查看答案</summary>

**答案**：B

**解释**：使用有符号差值比较时，差值必须在有符号整数范围内 $[-2^{31}, 2^{31}-1]$。由于 STRIDE_MAX - STRIDE_MIN ≤ BIG_STRIDE，所以 BIG_STRIDE ≤ 2^31 - 1 = 0x7FFFFFFF。

</details>

### 题目 3（开放题）
**题目**：假设有两个进程 P1 和 P2，优先级分别为 2 和 1，BIG_STRIDE = 100，初始 stride 都为 0。请写出前 6 次调度的顺序。

<details>
<summary>点击查看答案</summary>

**答案**：

| 次数 | 调度前 stride | 选中 | 调度后 stride |
|:---:|:---|:---:|:---|
| 1 | P1=0, P2=0 | P1 或 P2 (假设 P1) | P1=50, P2=0 |
| 2 | P1=50, P2=0 | P2 | P1=50, P2=100 |
| 3 | P1=50, P2=100 | P1 | P1=100, P2=100 |
| 4 | P1=100, P2=100 | P1 或 P2 (假设 P1) | P1=150, P2=100 |
| 5 | P1=150, P2=100 | P2 | P1=150, P2=200 |
| 6 | P1=150, P2=200 | P1 | P1=200, P2=200 |

**调度顺序**：P1 → P2 → P1 → P1 → P2 → P1

**统计**：P1 被调度 4 次，P2 被调度 2 次，比例 4:2 = 2:1，与优先级比例相符！

</details>

---

## 📝 本章小结

| 知识点 | 一句话带走 |
|:---|:---|
| Stride 原理 | 选 stride 最小的进程，stride 增量与优先级成反比 |
| pass 公式 | pass = BIG_STRIDE / priority |
| 溢出处理 | 使用有符号差值比较 |
| BIG_STRIDE | 取值 ≤ 0x7FFFFFFF |
| 斜堆 | 堆顶就是 stride 最小的进程，O(log n) 操作 |
| 切换调度器 | 只需在 sched_init 修改一行代码 |

---

**上一节**: [04-RR调度算法实现.md](04-RR调度算法实现.md)  
**下一节**: [06-动手实践指南.md](06-动手实践指南.md)

---

## 溯源与补丁

### 交叉引用表
| 材料要点 | 正文锚点 |
|:---|:---|
| S46 Stride 原理 | 第1节 |
| S47 形式化描述 | 第1.3节 |
| S48 比例分配证明 | 第1.4节 |
| S49 Stride vs RR | 第1.5节 |
| S50 溢出问题 | 第2.1节 |
| S51 有符号差值比较 | 第2.2节 |
| S52 BIG_STRIDE 选择 | 第2.3节 |
| S53 比较函数 | 第2.4节 |
| S54-S57 斜堆 | 第3节 |
| S58 五个核心函数 | 第4节 |
| S59-S63 函数实现 | 第4.1-4.5节 |
| S64 切换调度器 | 第5节 |
| S65 运行验证 | 第6节 |

### 缺漏扫描
✅ 本节无缺漏，所有材料要点已覆盖。
