#include <defs.h>
#include <list.h>
#include <proc.h>
#include <assert.h>
#include <default_sched.h>
#include <stdio.h>

#define USE_SKEW_HEAP 1

/* You should define the BigStride constant here*/
/* LAB6 CHALLENGE 1: 2314076 */
/* BIG_STRIDE 应该取一个合适的值，使得在32位无符号整数下比较正确
 * 由于 priority >= 1，我们需要 BIG_STRIDE <= 2^31 - 1
 * 选择 0x7FFFFFFF 作为最大值
 */
/* BIG_STRIDE 应该取一个合适的值，使得在32位无符号整数下比较正确
 * 由于 priority >= 1，我们需要 BIG_STRIDE <= 2^31 - 1
 * 选择 0x7FFFFFFF 作为最大值
 */
#define BIG_STRIDE 0x7FFFFFFF /* 大步长常量 */

/* The compare function for two skew_heap_node_t's and the
 * corresponding procs*/
/* 两个skew_heap_node_t及其对应进程的比较函数 */
static int
proc_stride_comp_f(void *a, void *b)
{
     struct proc_struct *p = le2proc(a, lab6_run_pool); // 获取进程p
     struct proc_struct *q = le2proc(b, lab6_run_pool); // 获取进程q
     int32_t c = p->lab6_stride - q->lab6_stride; // 比较步长
     if (c > 0)
          return 1;
     else if (c == 0)
          return 0;
     else
          return -1;
}

/*
 * stride_init initializes the run-queue rq with correct assignment for
 * member variables, including:
 *
 *   - run_list: should be a empty list after initialization.
 *   - lab6_run_pool: NULL
 *   - proc_num: 0
 *   - max_time_slice: no need here, the variable would be assigned by the caller.
 *
 * hint: see libs/list.h for routines of the list structures.
 */
/*
 * stride_init 初始化运行队列 rq，为成员变量赋值，包括：
 *   - run_list: 初始化后应为空链表。
 *   - lab6_run_pool: NULL
 *   - proc_num: 0
 *   - max_time_slice: 此处无需设置，该变量由调用者赋值。
 *
 * 提示: 查看 libs/list.h 以了解链表结构的操作。
 */
static void
stride_init(struct run_queue *rq)
{
     /* LAB6 CHALLENGE 1: 2313255
      * (1) 初始化就绪进程列表: rq->run_list
      * (2) 初始化运行池: rq->lab6_run_pool
      * (3) 设置进程数: rq->proc_num 为 0
      */
     list_init(&(rq->run_list)); // 初始化链表
     rq->lab6_run_pool = NULL; // 初始化优先队列
     rq->proc_num = 0; // 初始化进程数
}


/*
 * stride_enqueue 将进程 ``proc'' 插入运行队列 ``rq''。
 * 该过程应验证/初始化 ``proc'' 的相关成员，然后将 ``lab6_run_pool'' 节点
 * 放入队列（因为这里使用优先队列）。该过程还应更新 ``rq'' 结构中的元数据。
 *
 * proc->time_slice 表示分配给进程的时间片，应设置为 rq->max_time_slice。
 *
 * 提示: 查看 libs/skew_heap.h 以了解优先队列结构的操作。
 */
static void
stride_enqueue(struct run_queue *rq, struct proc_struct *proc)
{

     /* LAB6 CHALLENGE 1: 2314035
      * (1) 正确地将 proc 插入 rq
      * 注意: 可以使用 skew_heap 或 list。重要函数：
      *         skew_heap_insert: 将条目插入 skew_heap
      *         list_add_before: 将条目插入链表末尾
      * (2) 重新计算 proc->time_slice
      * (3) 将 proc->rq 指针设置为 rq
      * (4) 增加 rq->proc_num
      */
#if USE_SKEW_HEAP
     // 使用斜堆实现优先队列
     rq->lab6_run_pool = skew_heap_insert(rq->lab6_run_pool, 
                                           &(proc->lab6_run_pool), 
                                           proc_stride_comp_f);
#else
     // 使用链表实现
     list_add_before(&(rq->run_list), &(proc->run_link));
#endif
     // 重置时间片
     if (proc->time_slice == 0 || proc->time_slice > rq->max_time_slice)
     {
          proc->time_slice = rq->max_time_slice; // 设置时间片
     }
     proc->rq = rq; // 设置所属运行队列
     rq->proc_num++; // 增加进程数
}

/*
 * stride_dequeue removes the process ``proc'' from the run-queue
 * ``rq'', the operation would be finished by the skew_heap_remove
 * operations. Remember to update the ``rq'' structure.
 *
 * hint: see libs/skew_heap.h for routines of the priority
 * queue structures.
 */
/*
 * stride_dequeue 从运行队列 ``rq'' 中移除进程 ``proc''，
 * 该操作将通过 skew_heap_remove 操作完成。记得更新 ``rq'' 结构。
 *
 * 提示: 查看 libs/skew_heap.h 以了解优先队列结构的操作。
 */
static void
stride_dequeue(struct run_queue *rq, struct proc_struct *proc)
{

     /* LAB6 CHALLENGE 1: 2314035
      * (1) 从 rq 中正确移除 proc
      * 注意: 可以使用 skew_heap 或 list。重要函数：
      *         skew_heap_remove: 从 skew_heap 中移除条目
      *         list_del_init: 从链表中移除条目
      */
#if USE_SKEW_HEAP
     rq->lab6_run_pool = skew_heap_remove(rq->lab6_run_pool, 
                                           &(proc->lab6_run_pool), 
                                           proc_stride_comp_f); // 从斜堆移除
#else
     assert(!list_empty(&(proc->run_link)) && proc->rq == rq);
     list_del_init(&(proc->run_link));
#endif
     rq->proc_num--; // 减少进程数
}
/*
 * stride_pick_next pick the element from the ``run-queue'', with the
 * minimum value of stride, and returns the corresponding process
 * pointer. The process pointer would be calculated by macro le2proc,
 * see kern/process/proc.h for definition. Return NULL if
 * there is no process in the queue.
 *
 * When one proc structure is selected, remember to update the stride
 * property of the proc. (stride += BIG_STRIDE / priority)
 *
 * hint: see libs/skew_heap.h for routines of the priority
 * queue structures.
 */
/*
 * stride_pick_next 从 ``运行队列'' 中选取 stride 值最小的元素，
 * 并返回相应的进程指针。进程指针将通过宏 le2proc 计算，
 * 定义参见 kern/process/proc.h。如果队列中没有进程，返回 NULL。
 *
 * 当选定一个 proc 结构时，记得更新 proc 的 stride 属性。
 * (stride += BIG_STRIDE / priority)
 *
 * 提示: 查看 libs/skew_heap.h 以了解优先队列结构的操作。
 */
static struct proc_struct *
stride_pick_next(struct run_queue *rq)
{

     /* LAB6 CHALLENGE 1: 2314076
      * (1) 获取 stride 值最小的 proc_struct 指针 p
             (1.1) 如果使用 skew_heap，我们可以使用 le2proc 从 rq->lab6_run_pol 获取 p
             (1.2) 如果使用 list，我们必须搜索 list 以找到具有最小 stride 值的 p
      * (2) 更新 p 的 stride 值: p->lab6_stride
      * (3) 返回 p
      */
#if USE_SKEW_HEAP
     if (rq->lab6_run_pool == NULL)
     {
          return NULL;
     }
     // 斜堆的根节点就是stride最小的进程
     struct proc_struct *p = le2proc(rq->lab6_run_pool, lab6_run_pool);
#else
     // 使用链表需要遍历查找最小stride
     if (list_empty(&(rq->run_list)))
     {
          return NULL;
     }
     list_entry_t *le = list_next(&(rq->run_list));
     struct proc_struct *p = le2proc(le, run_link);
     while ((le = list_next(le)) != &(rq->run_list))
     {
          struct proc_struct *proc = le2proc(le, run_link);
          if ((int32_t)(proc->lab6_stride - p->lab6_stride) < 0)
          {
               p = proc;
          }
     }
#endif
     // 更新stride值: stride += BIG_STRIDE / priority
     // 注意：如果priority为0，设置为1避免除0错误
     if (p->lab6_priority == 0)
     {
          p->lab6_stride += BIG_STRIDE; // 防止除以0
     }
     else
     {
          p->lab6_stride += BIG_STRIDE / p->lab6_priority; // 更新步长
     }
     return p;
}

/*
 * stride_proc_tick works with the tick event of current process. You
 * should check whether the time slices for current process is
 * exhausted and update the proc struct ``proc''. proc->time_slice
 * denotes the time slices left for current
 * process. proc->need_resched is the flag variable for process
 * switching.
 */
/*
 * stride_proc_tick 处理当前进程的 tick 事件。
 * 你应该检查当前进程的时间片是否耗尽，并更新 proc 结构 ``proc''。
 * proc->time_slice 表示当前进程剩余的时间片。
 * proc->need_resched 是进程切换的标志变量。
 */
static void
stride_proc_tick(struct run_queue *rq, struct proc_struct *proc)
{
     /* LAB6 CHALLENGE 1: YOUR CODE */
     if (proc->time_slice > 0)
     {
          proc->time_slice--; // 减少时间片
     }
     if (proc->time_slice == 0)
     {
          proc->need_resched = 1; // 需要重新调度
     }
}

struct sched_class stride_sched_class = {
    .name = "stride_scheduler", // 调度器名称
    .init = stride_init, // 初始化函数
    .enqueue = stride_enqueue, // 入队函数
    .dequeue = stride_dequeue, // 出队函数
    .pick_next = stride_pick_next, // 选择下一个进程函数
    .proc_tick = stride_proc_tick, // tick处理函数
};
