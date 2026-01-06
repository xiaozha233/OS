#ifndef __KERN_SCHEDULE_SCHED_H__
#define __KERN_SCHEDULE_SCHED_H__

#include <defs.h>
#include <list.h>
#include <skew_heap.h>

#define MAX_TIME_SLICE 5 // 最大时间片

struct proc_struct;

struct run_queue;

// The introduction of scheduling classes is borrrowed from Linux, and makes the
// core scheduler quite extensible. These classes (the scheduler modules) encapsulate
// the scheduling policies.
// 调度类的引入借鉴了Linux，使核心调度器具有很好的扩展性。
// 这些类（调度器模块）封装了调度策略。
struct sched_class
{
    // the name of sched_class
    // 调度类的名称
    const char *name;
    // Init the run queue
    // 初始化运行队列
    void (*init)(struct run_queue *rq);
    // put the proc into runqueue, and this function must be called with rq_lock
    // 将进程放入运行队列，调用此函数时必须持有rq_lock
    void (*enqueue)(struct run_queue *rq, struct proc_struct *proc);
    // get the proc out runqueue, and this function must be called with rq_lock
    // 将进程移出运行队列，调用此函数时必须持有rq_lock
    void (*dequeue)(struct run_queue *rq, struct proc_struct *proc);
    // choose the next runnable task
    // 选择下一个可运行任务
    struct proc_struct *(*pick_next)(struct run_queue *rq);
    // dealer of the time-tick
    // 时钟中断处理者
    void (*proc_tick)(struct run_queue *rq, struct proc_struct *proc);
    /* for SMP support in the future
     *  load_balance
     *     void (*load_balance)(struct rq* rq);
     *  get some proc from this rq, used in load_balance,
     *  return value is the num of gotten proc
     *  int (*get_proc)(struct rq* rq, struct proc* procs_moved[]);
     */
};

struct run_queue
{
    list_entry_t run_list; // 运行队列链表
    unsigned int proc_num; // 进程数量
    int max_time_slice; // 最大时间片
    // For LAB6 ONLY
    skew_heap_entry_t *lab6_run_pool; // LAB6专用：基于斜堆的运行池
};

void sched_init(void); // 初始化调度器
void wakeup_proc(struct proc_struct *proc); // 唤醒进程
void schedule(void); // 进行调度
void sched_class_proc_tick(struct proc_struct *proc); // 调度类时钟处理
#endif /* !__KERN_SCHEDULE_SCHED_H__ */
