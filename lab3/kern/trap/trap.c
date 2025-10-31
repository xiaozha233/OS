#include <assert.h>
#include <clock.h>
#include <console.h>
#include <defs.h>
#include <kdebug.h>
#include <memlayout.h>
#include <mmu.h>
#include <riscv.h>
#include <sbi.h>
#include <stdio.h>
#include <trap.h>

#define TICK_NUM 100

static void print_ticks() {
    cprintf("%d ticks\n", TICK_NUM);
#ifdef DEBUG_GRADE
    cprintf("End of Test.\n");
    panic("EOT: kernel seems ok.");
#endif
}

/* idt_init - 初始化IDT，使其指向 kern/trap/vectors.S 中的各个入口点
 */
void idt_init(void) {
    /* 实验三 你的代码：步骤二 */
    /* (1) 每个中断服务例程（ISR）的入口地址在哪里？
     *     所有ISR的入口地址都存储在 __vectors 中。uintptr_t __vectors[] 在哪里？
     *     __vectors[] 位于 kern/trap/vector.S 中，由 tools/vector.c 生成
     *     （在 lab3 中尝试 "make" 命令，你会在 kern/trap 目录下找到 vector.S）
     *     你可以使用 "extern uintptr_t __vectors[];" 来定义这个外部变量，稍后会用到。
     * (2) 现在你应该在中断描述符表（IDT）中设置ISR的条目。
     *     你能在这个文件中看到 idt[256] 吗？是的，它就是IDT！你可以使用 SETGATE 宏来设置IDT的每个项目。
     * (3) 设置好IDT的内容后，你需要使用 'lidt' 指令让CPU知道IDT的位置。
     *     你不知道这个指令的含义吗？Google一下！并查看 libs/x86.h 以了解更多信息。
     *     注意：lidt 的参数是 idt_pd。试着找到它！
     */

    extern void __alltraps(void);
    /* 将 sup0 scratch 寄存器设置为 0，向异常向量表明
       我们当前正在内核中执行 */
    write_csr(sscratch, 0);
    /* 设置异常向量地址 */
    write_csr(stvec, &__alltraps);
}

/* trap_in_kernel - 测试陷阱是否发生在内核中 */
bool trap_in_kernel(struct trapframe *tf) {
    return (tf->status & SSTATUS_SPP) != 0;
}

void print_trapframe(struct trapframe *tf) {
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->gpr);
    cprintf("  status   0x%08x\n", tf->status);
    cprintf("  epc      0x%08x\n", tf->epc);
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr);
    cprintf("  cause    0x%08x\n", tf->cause);
}

void print_regs(struct pushregs *gpr) {
    cprintf("  zero     0x%08x\n", gpr->zero);
    cprintf("  ra       0x%08x\n", gpr->ra);
    cprintf("  sp       0x%08x\n", gpr->sp);
    cprintf("  gp       0x%08x\n", gpr->gp);
    cprintf("  tp       0x%08x\n", gpr->tp);
    cprintf("  t0       0x%08x\n", gpr->t0);
    cprintf("  t1       0x%08x\n", gpr->t1);
    cprintf("  t2       0x%08x\n", gpr->t2);
    cprintf("  s0       0x%08x\n", gpr->s0);
    cprintf("  s1       0x%08x\n", gpr->s1);
    cprintf("  a0       0x%08x\n", gpr->a0);
    cprintf("  a1       0x%08x\n", gpr->a1);
    cprintf("  a2       0x%08x\n", gpr->a2);
    cprintf("  a3       0x%08x\n", gpr->a3);
    cprintf("  a4       0x%08x\n", gpr->a4);
    cprintf("  a5       0x%08x\n", gpr->a5);
    cprintf("  a6       0x%08x\n", gpr->a6);
    cprintf("  a7       0x%08x\n", gpr->a7);
    cprintf("  s2       0x%08x\n", gpr->s2);
    cprintf("  s3       0x%08x\n", gpr->s3);
    cprintf("  s4       0x%08x\n", gpr->s4);
    cprintf("  s5       0x%08x\n", gpr->s5);
    cprintf("  s6       0x%08x\n", gpr->s6);
    cprintf("  s7       0x%08x\n", gpr->s7);
    cprintf("  s8       0x%08x\n", gpr->s8);
    cprintf("  s9       0x%08x\n", gpr->s9);
    cprintf("  s10      0x%08x\n", gpr->s10);
    cprintf("  s11      0x%08x\n", gpr->s11);
    cprintf("  t3       0x%08x\n", gpr->t3);
    cprintf("  t4       0x%08x\n", gpr->t4);
    cprintf("  t5       0x%08x\n", gpr->t5);
    cprintf("  t6       0x%08x\n", gpr->t6);
}

void interrupt_handler(struct trapframe *tf) {
    intptr_t cause = (tf->cause << 1) >> 1;
    switch (cause) {
        case IRQ_U_SOFT:
            cprintf("User software interrupt\n");
            break;
        case IRQ_S_SOFT:
            cprintf("Supervisor software interrupt\n");
            break;
        case IRQ_H_SOFT:
            cprintf("Hypervisor software interrupt\n");
            break;
        case IRQ_M_SOFT:
            cprintf("Machine software interrupt\n");
            break;
        case IRQ_U_TIMER:
            cprintf("User Timer interrupt\n");
            break;
        case IRQ_S_TIMER:
            // "sip 寄存器中除了 SSIP 和 USIP 之外的所有位都是只读的。"
            // -- privileged spec1.9.1, 4.1.4, p59
            // 事实上，调用 sbi_set_timer 会清除 STIP，或者你可以直接清除它。
            // cprintf("Supervisor timer interrupt\n");
             /* 实验三 练习一   你的代码： */
            /*(1)设置下次时钟中断- clock_set_next_event()
             *(2)计数器（ticks）加一
             *(3)当计数器加到100的时候，我们会输出一个`100ticks`表示我们触发了100次时钟中断，同时打印次数（num）加一
            * (4)判断打印次数，当打印次数为10时，调用<sbi.h>中的关机函数关机
            */
            {
                extern volatile size_t ticks;
                clock_set_next_event(); // 设置下次时钟中断
                ticks++; // 计数器加一
                if (ticks % TICK_NUM == 0) { // 每100次时钟中断
                    print_ticks(); // 打印 "100 ticks"
                    static int num = 0; // 打印次数计数器
                    num++;
                    if (num == 10) { // 打印10次后关机
                        sbi_shutdown();
                    }
                }
            }
            break;
        case IRQ_H_TIMER:
            cprintf("Hypervisor software interrupt\n");
            break;
        case IRQ_M_TIMER:
            cprintf("Machine software interrupt\n");
            break;
        case IRQ_U_EXT:
            cprintf("User software interrupt\n");
            break;
        case IRQ_S_EXT:
            cprintf("Supervisor external interrupt\n");
            break;
        case IRQ_H_EXT:
            cprintf("Hypervisor software interrupt\n");
            break;
        case IRQ_M_EXT:
            cprintf("Machine software interrupt\n");
            break;
        default:
            print_trapframe(tf);
            break;
    }
}

void exception_handler(struct trapframe *tf) {
    switch (tf->cause) {
        case CAUSE_MISALIGNED_FETCH:
            break;
        case CAUSE_FAULT_FETCH:
            break;
        case CAUSE_ILLEGAL_INSTRUCTION:
             // 非法指令异常处理
             /* 实验三 挑战三   你的code： */
            /*(1)输出指令异常类型（ Illegal instruction）
             *(2)输出异常指令地址
             *(3)更新 tf->epc寄存器
            */
            cprintf("Exception type:Illegal instruction\n");
            cprintf("Illegal instruction caught at 0x%08x\n", tf->epc);
            // 跳过非法指令（假设为4字节指令）
            tf->epc += 4;
            
            break;
        case CAUSE_BREAKPOINT:
            //断点异常处理
            /* 实验三 挑战三   你的code： */
            /*(1)输出指令异常类型（ breakpoint）
             *(2)输出异常指令地址
             *(3)更新 tf->epc寄存器
            */
            cprintf("Exception type: breakpoint\n");
            cprintf("ebreak caught at 0x%08x\n", tf->epc);
            // ebreak 指令在 RV64C（压缩指令集）中是2字节，在标准指令集中是4字节
            // 检查指令长度：如果最低2位不是11，则是压缩指令（2字节）
            uint16_t instr = *(uint16_t*)(tf->epc);
            if ((instr & 0x3) != 0x3) {
                tf->epc += 2;  // 压缩指令
            } else {
                tf->epc += 4;  // 标准指令
            }

            break;
        case CAUSE_MISALIGNED_LOAD:
            break;
        case CAUSE_FAULT_LOAD:
            break;
        case CAUSE_MISALIGNED_STORE:
            break;
        case CAUSE_FAULT_STORE:
            break;
        case CAUSE_USER_ECALL:
            break;
        case CAUSE_SUPERVISOR_ECALL:
            break;
        case CAUSE_HYPERVISOR_ECALL:
            break;
        case CAUSE_MACHINE_ECALL:
            break;
        default:
            print_trapframe(tf);
            break;
    }
}

static inline void trap_dispatch(struct trapframe *tf) {
    if ((intptr_t)tf->cause < 0) {
        // 中断
        interrupt_handler(tf);
    } else {
        // 异常
        exception_handler(tf);
    }
}

/* *
 * trap - 处理或分发一个异常/中断。如果/当 trap() 返回时，
 * kern/trap/trapentry.S 中的代码会恢复保存在 trapframe 中的旧 CPU 状态，
 * 然后使用 iret 指令从异常中返回。
 * */
void trap(struct trapframe *tf) {
    // 根据发生的陷阱类型进行分发
    trap_dispatch(tf);
}
