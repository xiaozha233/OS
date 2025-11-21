#include <list.h>        // 包含链表操作相关的头文件
#include <sync.h>        // 包含同步相关的头文件（如中断控制）
#include <proc.h>        // 包含进程结构体和相关操作
#include <sched.h>       // 包含调度器相关的头文件
#include <assert.h>      // 包含断言相关的头文件

// 唤醒指定的进程
void
wakeup_proc(struct proc_struct *proc) {
    // 断言：进程不能是僵尸态或可运行态
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
    // 设置进程状态为可运行
    proc->state = PROC_RUNNABLE;
}

// 进程调度函数
void
schedule(void) {
    bool intr_flag;              // 用于保存中断标志
    list_entry_t *le, *last;     // le用于遍历进程链表，last为遍历的起始位置
    struct proc_struct *next = NULL; // 指向下一个将要运行的进程

    // 关闭中断并保存当前中断状态
    local_intr_save(intr_flag);
    {
        // 当前进程不再需要调度
        current->need_resched = 0;

        // 如果当前进程是空闲进程，则从进程链表头开始遍历
        // 否则从当前进程在链表中的位置开始遍历
        last = (current == idleproc) ? &proc_list : &(current->list_link);

        le = last; // 初始化遍历指针

        // 循环查找下一个可运行的进程
        do {
            // 移动到下一个链表节点
            if ((le = list_next(le)) != &proc_list) {
                // 获取链表节点对应的进程结构体
                next = le2proc(le, list_link);
                // 如果该进程是可运行态，则跳出循环
                if (next->state == PROC_RUNNABLE) {
                    break;
                }
            }
        } while (le != last); // 如果回到起始位置则结束循环

        // 如果没有找到可运行的进程，则选择空闲进程
        if (next == NULL || next->state != PROC_RUNNABLE) {
            next = idleproc;
        }

        // 统计该进程被调度运行的次数
        next->runs ++;

        // 如果下一个进程不是当前进程，则切换到下一个进程
        if (next != current) {
            proc_run(next);
        }
    }
    // 恢复之前的中断状态
    local_intr_restore(intr_flag);
}
