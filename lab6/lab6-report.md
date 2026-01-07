# Lab6 实验报告：进程调度

## 小组信息

**小组成员及分工：**

| 学号 | 姓名 | 分工 |
|------|------|------|
| 2314076 | 查许琴 | 练习0、练习1 |
| 2314035 | 陈翔 | 练习2 |
| 2313255 | 刘璇 | 扩展练习Challenge 1 |

---

## 练习0：填写已有实验

本实验依赖实验2/3/4/5，需要将之前实验的代码填入本实验相应部分，并针对Lab6的调度需求进行必要的更新。

### 0.1 进程控制块初始化的更新

在`kern/process/proc.c`的`alloc_proc`函数中，需要初始化Lab6新增的调度相关字段：

```c
// LAB6:YOUR CODE (update LAB5 steps)
/*
 * below fields(add in LAB6) in proc_struct need to be initialized
 *       struct run_queue *rq;                       // run queue contains Process
 *       list_entry_t run_link;                      // the entry linked in run queue
 *       int time_slice;                             // time slice for occupying the CPU
 *       skew_heap_entry_t lab6_run_pool;            // entry in the run pool (lab6 stride)
 *       uint32_t lab6_stride;                       // stride value (lab6 stride)
 *       uint32_t lab6_priority;                     // priority value (lab6 stride)
 */
proc->rq = NULL;                        // 初始化运行队列
list_init(&(proc->run_link));           // 初始化运行队列链接
proc->time_slice = 0;                   // 初始化时间片
skew_heap_init(&(proc->lab6_run_pool)); // 初始化斜堆节点
proc->lab6_stride = 0;                  // 初始化步长
proc->lab6_priority = 0;                // 初始化优先级
```

### 0.2 时钟中断处理的更新

在`kern/trap/trap.c`中，时钟中断处理需要调用调度器的`sched_class_proc_tick`函数：

```c
case IRQ_S_TIMER:
    clock_set_next_event();
    ticks++;
    if (ticks % TICK_NUM == 0) {
        print_ticks();
    }
    // lab6: YOUR CODE (update LAB3 steps)
    // 在时钟中断时调用调度器的 sched_class_proc_tick 函数
    sched_class_proc_tick(current);
    break;
```

这个改动使得每次时钟中断都会调用调度器来处理当前进程的时间片，是实现时间片轮转调度的关键。

---

## 练习1：理解调度器框架的实现

### 1.1 调度类结构体 sched_class 的分析

`sched_class`结构体定义在`kern/schedule/sched.h`中，是调度器框架的核心抽象：

```c
struct sched_class {
    const char *name;                                              // 调度类名称
    void (*init)(struct run_queue *rq);                            // 初始化运行队列
    void (*enqueue)(struct run_queue *rq, struct proc_struct *proc); // 入队操作
    void (*dequeue)(struct run_queue *rq, struct proc_struct *proc); // 出队操作
    struct proc_struct *(*pick_next)(struct run_queue *rq);        // 选择下一进程
    void (*proc_tick)(struct run_queue *rq, struct proc_struct *proc); // 时钟tick处理
};
```

**各函数指针的作用和调用时机：**

| 函数指针 | 作用 | 调用时机 |
|----------|------|----------|
| `init` | 初始化运行队列的数据结构 | 内核初始化时，`sched_init()`中调用 |
| `enqueue` | 将进程加入运行队列 | 进程变为就绪状态时（如被唤醒、时间片用完） |
| `dequeue` | 将进程从运行队列移除 | 进程被选中执行时 |
| `pick_next` | 从运行队列中选择下一个要执行的进程 | 调度器`schedule()`函数中 |
| `proc_tick` | 处理时钟中断，更新进程时间片状态 | 每次时钟中断时 |

**为什么需要使用函数指针？**

使用函数指针实现调度器接口有以下优点：

1. **多态性**：通过函数指针，可以在运行时动态绑定不同的调度算法实现，实现类似面向对象的多态特性
2. **解耦合**：调度器框架与具体算法分离，框架代码不需要关心算法细节
3. **可扩展性**：添加新调度算法只需实现这些接口，无需修改框架代码
4. **易于切换**：只需修改`sched_class`指针指向即可切换调度算法

### 1.2 运行队列结构体 run_queue 的分析

**run_queue 结构体定义：**

```c
struct run_queue {
    list_entry_t run_list;              // 运行队列链表
    unsigned int proc_num;              // 进程数量
    int max_time_slice;                 // 最大时间片
    // For LAB6 ONLY
    skew_heap_entry_t *lab6_run_pool;   // LAB6专用：基于斜堆的运行池
};
```

**Lab5与Lab6的差异：**

| 特性 | Lab5 | Lab6 |
|------|------|------|
| 数据结构 | 简单链表（FIFO） | 链表 + 斜堆（优先队列） |
| 调度算法支持 | 仅FIFO | RR、Stride等多种算法 |
| 时间片管理 | 无 | 有`max_time_slice`控制 |
| 优先级支持 | 无 | 通过斜堆实现优先级调度 |

**为什么需要支持两种数据结构？**

1. **链表（run_list）**：适用于RR等简单调度算法，按FIFO顺序管理进程，O(1)的入队和出队操作
2. **斜堆（lab6_run_pool）**：适用于Stride等需要优先级的调度算法，可以O(log n)地找到stride最小的进程

不同调度算法有不同的需求，链表适合公平调度，斜堆适合基于优先级的调度。提供两种数据结构使框架更加灵活。

### 1.3 调度器框架函数分析

#### sched_init() 函数

```c
void sched_init(void) {
    list_init(&timer_list);              // 初始化定时器列表
    sched_class = &default_sched_class;  // 设置默认调度类
    rq = &__rq;                          // 初始化运行队列指针
    rq->max_time_slice = MAX_TIME_SLICE; // 设置最大时间片
    sched_class->init(rq);               // 初始化调度类
    cprintf("sched class: %s\n", sched_class->name);
}
```

该函数完成调度器的初始化：绑定默认调度类，初始化运行队列，并调用具体调度算法的初始化函数。

#### wakeup_proc() 函数

```c
void wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE);
    bool intr_flag;
    local_intr_save(intr_flag);  // 关中断（临界区保护）
    {
        if (proc->state != PROC_RUNNABLE) {
            proc->state = PROC_RUNNABLE;     // 设置为就绪状态
            proc->wait_state = 0;
            if (proc != current) {
                sched_class_enqueue(proc);   // 加入运行队列
            }
        }
    }
    local_intr_restore(intr_flag);
}
```

该函数将进程唤醒并加入调度队列。注意通过关中断保证原子性操作。

#### schedule() 函数

```c
void schedule(void) {
    bool intr_flag;
    struct proc_struct *next;
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;           // 清除调度标志
        if (current->state == PROC_RUNNABLE) {
            sched_class_enqueue(current);    // 当前进程重新入队
        }
        if ((next = sched_class_pick_next()) != NULL) {
            sched_class_dequeue(next);       // 选中进程出队
        }
        if (next == NULL) {
            next = idleproc;                 // 无进程可调度时运行idle
        }
        next->runs++;
        if (next != current) {
            proc_run(next);                  // 切换到新进程
        }
    }
    local_intr_restore(intr_flag);
}
```

该函数是调度的核心：将当前进程入队，选择下一个进程，执行进程切换。

### 1.4 调度类的初始化流程

```
kern_init()
    |
    +--> pmm_init()          // 物理内存管理初始化
    |
    +--> vmm_init()          // 虚拟内存管理初始化
    |
    +--> sched_init()        // 调度器初始化
    |        |
    |        +--> sched_class = &default_sched_class  // 绑定默认调度类
    |        |
    |        +--> sched_class->init(rq)               // 调用RR_init或stride_init
    |
    +--> proc_init()         // 进程表初始化
    |
    +--> clock_init()        // 时钟中断初始化
    |
    +--> intr_enable()       // 使能中断
    |
    +--> cpu_idle()          // 运行idle进程
```

`default_sched_class`是一个全局的`sched_class`结构体，在`default_sched.c`中定义：

```c
struct sched_class default_sched_class = {
    .name = "RR_scheduler",
    .init = RR_init,
    .enqueue = RR_enqueue,
    .dequeue = RR_dequeue,
    .pick_next = RR_pick_next,
    .proc_tick = RR_proc_tick,
};
```

### 1.5 进程调度流程图

```
                    +-----------------------+
                    |    用户进程执行       |
                    +-----------+-----------+
                                |
                    +-----------v-----------+
                    |     时钟中断触发      |
                    +-----------+-----------+
                                |
                    +-----------v-----------+
                    |  trap() 进入内核态    |
                    +-----------+-----------+
                                |
                    +-----------v-----------+
                    | interrupt_handler()   |
                    | 处理 IRQ_S_TIMER      |
                    +-----------+-----------+
                                |
                    +-----------v-----------+
                    | sched_class_proc_tick |
                    | (调用 RR_proc_tick)   |
                    +-----------+-----------+
                                |
                    +-----------v-----------+
                    | 时间片减1             |
                    | 时间片=0则设置        |
                    | need_resched=1        |
                    +-----------+-----------+
                                |
                    +-----------v-----------+
                    |  trap() 检查          |
                    |  need_resched标志     |
                    +-----------+-----------+
                                |
              need_resched=1    |   need_resched=0
                    +-----------+-----------+
                    |                       |
          +---------v---------+    +--------v--------+
          |    schedule()     |    | 返回用户态继续  |
          +-------------------+    +-----------------+
                    |
          +---------v---------+
          | sched_class_      |
          | enqueue(current)  |<-- 当前进程入队
          +---------+---------+
                    |
          +---------v---------+
          | sched_class_      |
          | pick_next()       |<-- 选择下一进程
          +---------+---------+
                    |
          +---------v---------+
          | sched_class_      |
          | dequeue(next)     |<-- 下一进程出队
          +---------+---------+
                    |
          +---------v---------+
          |   proc_run(next)  |<-- 切换到新进程
          +---------+---------+
                    |
          +---------v---------+
          |  新进程执行       |
          +-------------------+
```

**need_resched标志位的作用：**

`need_resched`是进程控制块中的一个布尔标志，用于标记当前进程是否需要被调度。它的作用包括：

1. **延迟调度决策**：在时钟中断处理中设置该标志，但不立即调度
2. **统一调度检查点**：在`trap()`函数返回用户态前检查该标志
3. **避免嵌套调度**：通过在`schedule()`入口清除该标志，防止重复调度
4. **主动调度支持**：内核代码可以直接设置该标志请求调度

### 1.6 调度算法的切换机制

**切换到新调度算法需要的修改：**

1. **实现调度类接口**：在新文件（如`default_sched_stride.c`）中实现`sched_class`的所有接口函数

2. **定义调度类实例**：
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

3. **修改sched_init()中的绑定**：
```c
// 原来：
sched_class = &default_sched_class;
// 改为：
sched_class = &stride_sched_class;
```

**为什么当前设计使得切换算法变得容易？**

1. **接口统一**：所有调度算法都实现相同的`sched_class`接口
2. **实现分离**：每个算法独立实现，互不影响
3. **单点切换**：只需在`sched_init()`中修改一行代码即可切换
4. **框架透明**：`sched.c`中的框架代码通过函数指针调用，无需修改

这种设计体现了**策略与机制分离**的原则，调度框架是机制，具体算法是策略。

---

## 练习2：实现 Round Robin 调度算法

### 2.1 Lab5与Lab6的函数差异分析

以`kern/schedule/sched.c`中的`schedule()`函数为例，分析Lab5和Lab6的差异：

**Lab5中的schedule()（简化版）：**
```c
void schedule(void) {
    // 简单的FIFO调度，直接从链表取下一个进程
    list_entry_t *le = list_next(&proc_list);
    // ... 遍历找到第一个RUNNABLE的进程
}
```

**Lab6中的schedule()：**
```c
void schedule(void) {
    current->need_resched = 0;
    if (current->state == PROC_RUNNABLE) {
        sched_class_enqueue(current);     // 通过调度类接口入队
    }
    if ((next = sched_class_pick_next()) != NULL) {
        sched_class_dequeue(next);        // 通过调度类接口出队
    }
    // ...
}
```

**改动原因：**
1. **支持多种调度算法**：Lab5只有FIFO，Lab6需要支持RR、Stride等
2. **时间片管理**：Lab6需要处理时间片耗尽后的重新入队
3. **框架化设计**：通过调度类接口解耦，便于扩展

**不做改动的问题：**
- 无法实现时间片轮转，进程会一直运行直到主动放弃CPU
- 无法支持优先级调度
- 新增调度算法需要大量修改框架代码

### 2.2 RR调度算法实现详解

#### RR_init 函数

```c
static void RR_init(struct run_queue *rq) {
    list_init(&(rq->run_list));  // 初始化链表头
    rq->proc_num = 0;            // 进程计数清零
}
```

**思路**：初始化运行队列为空链表，将进程数设为0。

#### RR_enqueue 函数

```c
static void RR_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    assert(list_empty(&(proc->run_link)));  // 确保进程不在任何队列中
    // 将进程插入到队列尾部
    list_add_before(&(rq->run_list), &(proc->run_link));
    // 重置时间片
    if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
        proc->time_slice = rq->max_time_slice;
    }
    proc->rq = rq;      // 设置所属运行队列
    rq->proc_num++;     // 增加进程计数
}
```

**思路与关键点：**
1. 使用`list_add_before`将进程插入链表尾部（因为`run_list`是循环链表，头节点之前就是尾部）
2. 时间片处理：如果时间片为0（刚用完）或超过最大值，重置为最大时间片
3. 更新进程的队列指针和队列计数

**边界情况处理：**
- 空队列：`list_add_before`在空队列时也能正确工作
- 时间片为0：自动重置为最大时间片

#### RR_dequeue 函数

```c
static void RR_dequeue(struct run_queue *rq, struct proc_struct *proc) {
    assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
    list_del_init(&(proc->run_link));  // 从链表删除并重初始化
    rq->proc_num--;                    // 减少进程计数
}
```

**思路**：将进程从队列中删除，并更新计数。

**边界情况处理：**
- 使用`list_del_init`确保删除后链表项被重新初始化，避免野指针

#### RR_pick_next 函数

```c
static struct proc_struct *RR_pick_next(struct run_queue *rq) {
    list_entry_t *le = list_next(&(rq->run_list));
    if (le != &(rq->run_list)) {       // 队列非空
        return le2proc(le, run_link);  // 返回队首进程
    }
    return NULL;                       // 队列为空返回NULL
}
```

**思路**：取链表头节点后的第一个元素（即队首进程）。

**边界情况处理：**
- 空队列：当`le == &(rq->run_list)`时说明队列为空，返回NULL

#### RR_proc_tick 函数

```c
static void RR_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    if (proc->time_slice > 0) {
        proc->time_slice--;            // 时间片减1
    }
    if (proc->time_slice == 0) {
        proc->need_resched = 1;        // 标记需要调度
    }
}
```

**思路**：每次时钟中断减少时间片，为0时设置重调度标志。

**为什么在这里设置need_resched而不是直接调用schedule()？**
1. **中断安全**：避免在中断处理中进行复杂的调度操作
2. **统一检查点**：让`trap()`函数统一处理调度决策
3. **避免嵌套**：防止在调度过程中被再次中断导致问题

### 2.3 make grade 输出结果

```
$ make grade
...
Check SWAP:                             (1.0s)
  -check pmm:                           OK
  -check page table:                    OK
  -check vmm:                           OK
  -check swap page fault:               OK
  -check ticks:                         OK
Check PRIORITY:                         (2.5s)
  -check priority:                      OK
Total Score: 45/45
```

### 2.4 QEMU中观察到的调度现象

运行`make qemu`后可以观察到：

1. **时间片轮转**：多个用户进程交替执行，每个进程运行一个时间片后被切换
2. **100 ticks输出**：每100个时钟中断输出一次，表明时钟中断正常工作
3. **进程切换**：可以看到不同进程的输出交替出现

### 2.5 Round Robin调度算法分析

**优点：**
1. **公平性**：每个进程获得相等的CPU时间
2. **简单**：实现简单，易于理解和维护
3. **响应性**：交互式进程能够及时响应
4. **无饥饿**：所有进程都能得到执行

**缺点：**
1. **无优先级**：不能区分紧急程度不同的任务
2. **上下文切换开销**：频繁切换带来额外开销
3. **时间片选择困难**：太短增加开销，太长降低响应性

**时间片大小的优化：**
- **较小时间片**：适合交互式任务，响应快，但开销大
- **较大时间片**：适合CPU密集型任务，开销小，但响应慢
- 当前实现使用`MAX_TIME_SLICE = 5`，是一个折中选择

### 2.6 拓展思考

**实现优先级RR调度的修改：**

1. 使用多个运行队列，每个优先级一个队列
2. 修改`pick_next`先从高优先级队列选择
3. 修改`enqueue`根据进程优先级放入对应队列

```c
#define NR_PRIORITY 8
struct run_queue priority_queues[NR_PRIORITY];

static void Priority_RR_enqueue(struct run_queue *rq, struct proc_struct *proc) {
    int prio = proc->lab6_priority;
    list_add_before(&(priority_queues[prio].run_list), &(proc->run_link));
    // ...
}
```

**多核调度支持：**

**问：当前的调度实现是否支持多核调度？如果不支持，需要如何改进？**

答：目前的 ucore 调度实现**不支持**多核调度。当前的框架和算法均假定系统中仅存在单核心 CPU。若要支持多核调度，需进行以下改进：

1. **同步机制**：必须在调度器数据结构（如运行队列）中引入自旋锁（Spinlock），以保护多核并发访问下的数据一致性。
2. **每 CPU 运行队列**：为每个 CPU 核心维护独立的就绪队列，以减少全局锁的竞争，并提高缓存命中率。
3. **负载均衡**：实现任务迁移机制，在各核之间动态分配进程，防止出现“一核有难，多核围观”的负载不均现象。
4. **IPI（处理器间中断）**：用于在一个核唤醒另一个核上的高优先级进程时，通知对方执行抢占。
5. **亲和性管理**：支持进程与特定 CPU 核心的绑定，优化缓存（L1/L2）的使用。

---

## 扩展练习 Challenge 1：实现 Stride Scheduling 调度算法

### 3.1 Stride算法实现

#### BIG_STRIDE常量的选择

```c
#define BIG_STRIDE 0x7FFFFFFF  // 2^31 - 1
```

**选择原因**：
- stride使用32位无符号整数存储
- 比较时使用有符号差值：`(int32_t)(p->stride - q->stride)`
- 为保证比较正确，需要`STRIDE_MAX - STRIDE_MIN <= BIG_STRIDE`
- 由于`priority >= 1`，所以`pass <= BIG_STRIDE`
- 选择`0x7FFFFFFF`确保差值在`int32_t`范围内

#### stride_init 函数

```c
static void stride_init(struct run_queue *rq) {
    list_init(&(rq->run_list));    // 初始化链表
    rq->lab6_run_pool = NULL;      // 初始化优先队列为空
    rq->proc_num = 0;              // 初始化进程数
}
```

#### stride_enqueue 函数

```c
static void stride_enqueue(struct run_queue *rq, struct proc_struct *proc) {
#if USE_SKEW_HEAP
    // 使用斜堆实现优先队列
    rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool,
                                          &(proc->lab6_run_pool),
                                          proc_stride_comp_f);
#else
    // 使用链表实现
    list_add_before(&(rq->run_list), &(proc->run_link));
#endif
    if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice) {
        proc->time_slice = rq->max_time_slice;
    }
    proc->rq = rq;
    rq->proc_num++;
}
```

#### stride_dequeue 函数

```c
static void stride_dequeue(struct run_queue *rq, struct proc_struct *proc) {
#if USE_SKEW_HEAP
    rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool,
                                          &(proc->lab6_run_pool),
                                          proc_stride_comp_f);
#else
    list_del_init(&(proc->run_link));
#endif
    rq->proc_num--;
}
```

#### stride_pick_next 函数

```c
static struct proc_struct *stride_pick_next(struct run_queue *rq) {
#if USE_SKEW_HEAP
    if (rq->lab6_run_pool == NULL) return NULL;
    // 斜堆根节点就是stride最小的进程
    struct proc_struct *p = le2proc(rq->lab6_run_pool, lab6_run_pool);
#else
    // 链表需要遍历找最小值
    if (list_empty(&(rq->run_list))) return NULL;
    list_entry_t *le = list_next(&(rq->run_list));
    struct proc_struct *p = le2proc(le, run_link);
    while ((le = list_next(le)) != &(rq->run_list)) {
        struct proc_struct *proc = le2proc(le, run_link);
        if ((int32_t)(proc->lab6_stride - p->lab6_stride) < 0) {
            p = proc;
        }
    }
#endif
    // 更新stride值
    if (p->lab6_priority == 0) {
        p->lab6_stride += BIG_STRIDE;
    } else {
        p->lab6_stride += BIG_STRIDE / p->lab6_priority;
    }
    return p;
}
```

#### stride_proc_tick 函数

```c
static void stride_proc_tick(struct run_queue *rq, struct proc_struct *proc) {
    if (proc->time_slice > 0) {
        proc->time_slice--;
    }
    if (proc->time_slice == 0) {
        proc->need_resched = 1;
    }
}
```

### 3.2 多级反馈队列调度算法设计

**概要设计：**

```
+-------------------+
|  Queue 0 (最高)   | <-- 新进程进入，时间片最短
+-------------------+
         |
         v (时间片用完降级)
+-------------------+
|  Queue 1          |
+-------------------+
         |
         v
+-------------------+
|  Queue 2          |
+-------------------+
         |
         v
+-------------------+
|  Queue N (最低)   | <-- 时间片最长，RR调度
+-------------------+
```

**详细设计：**

1. **数据结构**：
```c
#define MLFQ_LEVELS 4
struct run_queue mlfq[MLFQ_LEVELS];
int time_slices[MLFQ_LEVELS] = {2, 4, 8, 16};
```

2. **调度策略**：
   - 新进程进入最高优先级队列
   - 优先从高优先级队列调度
   - 时间片用完降级到下一队列
   - 最低队列使用RR调度
   - 定期提升所有进程优先级（防止饥饿）

3. **核心接口**：
```c
void MLFQ_enqueue(rq, proc) {
    int level = proc->priority_level;
    list_add_before(&mlfq[level].run_list, &proc->run_link);
}

proc_struct *MLFQ_pick_next(rq) {
    for (int i = 0; i < MLFQ_LEVELS; i++) {
        if (!list_empty(&mlfq[i].run_list)) {
            return le2proc(list_next(&mlfq[i].run_list), run_link);
        }
    }
    return NULL;
}
```

### 3.3 Stride算法正比性证明

**命题**：经过足够长时间后，每个进程分配的时间片数目与其优先级成正比。

**证明**：

设进程$P_i$的优先级为$p_i$，步长为$pass_i = \frac{BIG\_STRIDE}{p_i}$。

经过时间$T$后，进程$P_i$被调度了$n_i$次，则其stride增加了$n_i \times pass_i$。

由于Stride算法总是选择stride最小的进程，在稳态下，所有进程的stride应该大致相等：

$$stride_i \approx stride_j$$

即：
$$n_i \times pass_i \approx n_j \times pass_j$$

代入$pass = \frac{BIG\_STRIDE}{priority}$：

$$n_i \times \frac{BIG\_STRIDE}{p_i} \approx n_j \times \frac{BIG\_STRIDE}{p_j}$$

化简得：
$$\frac{n_i}{p_i} \approx \frac{n_j}{p_j}$$

即：
$$n_i : n_j \approx p_i : p_j$$

因此，进程获得的调度次数（时间片数目）与其优先级成正比。$\square$

---

## 实验中的重要知识点

### 与OS原理对应的知识点

| 实验知识点 | 原理知识点 | 关系说明 |
|------------|------------|----------|
| `sched_class`结构体 | 调度器框架设计 | 面向对象思想的C语言实现 |
| RR调度实现 | 时间片轮转算法 | 理论算法的具体实现 |
| `need_resched`标志 | 抢占式调度 | 实现被动调度的机制 |
| Stride算法 | 比例共享调度 | 公平调度的一种实现 |
| 斜堆优先队列 | 优先级队列数据结构 | 提高调度效率的实现 |

### 原理中重要但实验未涉及的知识点

1. **实时调度算法**：如EDF（最早截止时间优先）、RMS（速率单调调度）
2. **多核调度**：负载均衡、CPU亲和性
3. **公平共享调度**：CFS（完全公平调度器）
4. **能耗感知调度**：DVFS（动态电压频率调整）
5. **上下文切换的完整开销分析**：TLB刷新、Cache失效等
