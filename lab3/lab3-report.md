  
  

# 实验三 - 实验报告

  

| 姓名 | 学号 | 分工 |
| :--- | :--- | :--- |
| 陈翔 | 2314035 | 练习1编码与报告撰写、知识点总结 |
| 查许琴 | 2314076 | 扩展练习 Challenge1 & 2 回答与报告撰写 |
| 刘璇 | 2313255 | 扩展练习 Challenge3 编码与报告撰写、整体报告整理与校对 |

  
## 实验目的

本次实验的核心是理解和掌握操作系统的中断处理机制。操作系统作为计算机系统的核心管理者，必须能够响应各种突发事件，例如硬件设备请求（中断）或程序执行错误（异常）。当中断或异常发生时，CPU会暂停当前任务，跳转到操作系统预设的处理程序，待处理完成后再返回原任务继续执行。


通过本次实验，我们学习了：

- RISC-V 架构的中断与异常处理机制，包括相关的特权级和控制状态寄存器（CSRs）。

- 中断发生前后，如何通过汇编代码实现上下文环境（即寄存器状态）的保存与恢复。

- 如何编写中断处理程序，并成功处理了断点（breakpoint）和时钟（timer）这两种基本的中断类型，验证了中断系统的正确性。

  

## 实验内容

  

本次实验主要围绕中断处理机制的实现展开。我们首先学习了RISC-V架构的中断处理原理、相关寄存器（如 `stvec`, `sepc`, `scause` 等）及特权指令。接着，我们深入分析了上下文保存与恢复的实现细节，理解了如何通过一个`trapframe`结构体和一段精巧的汇编代码`trapentry.S`来完成这一关键过程。最后，通过完成三个练习，我们动手实现了时钟中断处理和两种基本的异常处理，从而验证了我们所构建的中断系统的正确性和有效性。

  

## 练习与挑战

  

### 练习1：完善中断处理 （编程）

  

#### 1. 实现过程

  

本次练习的目标是完善位于 `kern/trap/trap.c` 文件中的 `interrupt_handler` 函数，使其能够正确处理S模式下的时钟中断（`IRQ_S_TIMER`）。

  

具体实现步骤如下：

  

1.  **定位代码位置**：在 `interrupt_handler` 函数中找到 `case IRQ_S_TIMER:` 分支。

2.  **设置下一次时钟中断**：在处理当前中断时，必须立即安排下一次中断的触发时间。我们通过调用 `clock_set_next_event()` 函数来实现。该函数内部会通过SBI调用 `sbi_set_timer()`，将下一次中断时间设置为当前时间加上一个固定的时间间隔（`timebase`）。

3.  **更新时钟计数器**：定义一个全局静态变量 `ticks`（位于 `kern/driver/clock.c`），每次进入时钟中断处理程序时，将其值加一。

4.  **周期性打印信息**：判断 `ticks` 是否达到 `TICK_NUM`（定义为100）的倍数。如果是，则调用 `print_ticks()` 函数，在屏幕上打印 "100 ticks"。

5.  **实现关机功能**：为了在打印10行后自动关机，我们在 `print_ticks()` 函数内部（或 `interrupt_handler` 中）定义一个静态计数器 `num`。每当 `print_ticks()` 被调用时，`num` 加一。当 `num` 达到10时，调用 `sbi_shutdown()` 函数关闭系统。

  

#### 2. 定时器中断处理流程

  

一次完整的时钟中断处理流程如下：

  

1.  **中断触发**：当 `time` 寄存器的值达到 OpenSBI 中设置的下一次事件时间点时，硬件会触发一个S模式的时钟中断。

2.  **硬件响应**：CPU硬件自动执行以下操作：

    *   将当前PC值（即被打断指令的地址）保存到 `sepc` 寄存器。

    *   在 `scause` 寄存器中记录中断原因（S模式时钟中断）。

    *   将 `sstatus` 寄存器中的 `SIE` 位（中断使能）保存到 `SPIE` 位，然后清除 `SIE` 位，以屏蔽后续的中断。

    *   将特权级从U模式或S模式切换到S模式，并将之前的特权级记录在 `SPP` 位。

    *   将PC设置为 `stvec` 寄存器指向的地址，即我们的中断入口点 `__alltraps`。

3.  **上下文保存**：执行 `kern/trap/trapentry.S` 中的 `__alltraps` 代码。

    *   `SAVE_ALL` 宏被调用，将所有通用寄存器以及 `sepc`, `scause`, `sstatus` 等CSR的值压入当前内核栈中，形成一个 `trapframe` 结构体。

4.  **进入C处理函数**：汇编代码将当前栈顶指针（即 `trapframe` 的地址）存入 `a0` 寄存器，作为参数，然后通过 `jal trap` 指令调用C语言实现的 `trap` 函数。

5.  **中断分发**：`trap` 函数调用 `trap_dispatch`，根据 `tf->cause` 的最高位判断是中断还是异常。对于时钟中断，它会调用 `interrupt_handler`。

6.  **时钟中断处理**：`interrupt_handler` 根据 `tf->cause` 的具体值，进入 `IRQ_S_TIMER` 分支，执行我们在练习1中编写的逻辑：调用 `clock_set_next_event()`、增加 `ticks` 计数器、并根据条件打印信息或关机。

7.  **返回汇编代码**：`trap` 函数执行完毕后返回到 `trapentry.S`。

8.  **上下文恢复**：执行 `__trapret` 处的代码。

    *   `RESTORE_ALL` 宏被调用，从栈上的 `trapframe` 中恢复所有通用寄存器以及 `sepc` 和 `sstatus` 的值。

9.  **中断返回**：最后执行 `sret` 特权指令。硬件会自动将 `sepc` 的值恢复到PC，并将 `sstatus` 的 `SPIE` 位恢复到 `SIE` 位，从而恢复到中断前的特权级和中断使能状态，程序从被中断处继续执行。

  

#### 3. 代码实现

  

以下是 `kern/trap/trap.c` 中 `interrupt_handler` 函数的修改部分：

  

```c

// kern/trap/trap.c

  

// 全局的ticks计数器，虽然定义在clock.c，但在这里被使用

extern volatile size_t ticks;

  

void interrupt_handler(struct trapframe *tf) {

    intptr_t cause = (tf->cause << 1) >> 1;

    switch (cause) {

        // ... 其他 case ...

        case IRQ_S_TIMER:

            // 实验三 练习一 YOUR CODE 2314035
            
            clock_set_next_event(); // (1) 设置下一次时钟中断

            ticks++;                // (2) 计数器（ticks）加一

            if (ticks % TICK_NUM == 0) {

                print_ticks();      // (3) 调用打印函数

                // (4) 判断打印次数并关机

                static int print_count = 0;

                print_count++;

                if (print_count >= 10) {

                    sbi_shutdown();

                }

            }

            break;

        // ... 其他 case ...

    }

}

```

  

### 扩展练习 Challenge1：描述与理解中断流程

  
  
  

### 扩展练习 Challenge2：理解上下文切换机制

  
  
  

### 扩展练习 Challenge3：完善异常中断

  
  
  

## 知识点总结

  

#### 1. 本实验中重要的知识点

  
  
  

#### 2. 与OS原理的对应关系

  
  
  

#### 3. OS原理中重要但在实验中没有对应上的知识点

  
  
  

## 总结