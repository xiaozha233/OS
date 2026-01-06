#include <list.h>
#include <sync.h>
#include <proc.h>
#include <sched.h>
#include <stdio.h>
#include <assert.h>
#include <default_sched.h>

// the list of timer
// 定时器列表
static list_entry_t timer_list;

static struct sched_class *sched_class; // 调度类

static struct run_queue *rq; // 运行队列

static inline void
sched_class_enqueue(struct proc_struct *proc)
{
    if (proc != idleproc) // idle进程不进入调度队列
    {
        sched_class->enqueue(rq, proc); // 调用具体调度类的入队函数
    }
}

static inline void
sched_class_dequeue(struct proc_struct *proc)
{
    sched_class->dequeue(rq, proc); // 调用具体调度类的出队函数
}

static inline struct proc_struct *
sched_class_pick_next(void)
{
    return sched_class->pick_next(rq); // 调用具体调度类的选择下一个进程函数
}

void sched_class_proc_tick(struct proc_struct *proc)
{
    if (proc != idleproc) // 如果不是idle进程
    {
        sched_class->proc_tick(rq, proc); // 处理时间片
    }
    else
    {
        proc->need_resched = 1; // 如果是idle进程，总是请求重新调度
    }
}

static struct run_queue __rq; // 定义一个静态运行队列

void sched_init(void)
{
    list_init(&timer_list); // 初始化定时器列表

    sched_class = &default_sched_class; // 设置默认调度类

    rq = &__rq; // 初始化运行队列指针
    rq->max_time_slice = MAX_TIME_SLICE; // 设置最大时间片
    sched_class->init(rq); // 初始化调度类

    cprintf("sched class: %s\n", sched_class->name); // 打印调度类名称
}

void wakeup_proc(struct proc_struct *proc)
{
    assert(proc->state != PROC_ZOMBIE); // 确保进程未处于僵死状态
    bool intr_flag;
    local_intr_save(intr_flag); // 关中断
    {
        if (proc->state != PROC_RUNNABLE) // 如果进程不是RUNNABLE
        {
            proc->state = PROC_RUNNABLE; // 设置为RUNNABLE
            proc->wait_state = 0; // 清除等待状态
            if (proc != current) // 如果不是当前进程
            {
                sched_class_enqueue(proc); // 放入调度队列
            }
        }
        else
        {
            warn("wakeup runnable process.\n"); // 警告：唤醒已运行进程
        }
    }
    local_intr_restore(intr_flag); // 开中断
}

void schedule(void)
{
    bool intr_flag;
    struct proc_struct *next;
    local_intr_save(intr_flag); // 关中断
    {
        current->need_resched = 0; // 清除重新调度标志
        if (current->state == PROC_RUNNABLE) // 如果当前进程是RUNNABLE
        {
            sched_class_enqueue(current); // 放入调度队列
        }
        if ((next = sched_class_pick_next()) != NULL) // 选择下一个进程
        {
            sched_class_dequeue(next); // 从队列中取出
        }
        if (next == NULL) // 如果没有可选进程
        {
            next = idleproc; // 切换到idle进程
        }
        next->runs++; // 增加运行次数
        if (next != current) // 如果是不同的进程
        {
            proc_run(next); // 运行新进程
        }
    }
    local_intr_restore(intr_flag); // 开中断
}
