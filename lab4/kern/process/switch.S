#include <riscv.h>

.text
# void switch_to(struct proc_struct* from, struct proc_struct* to)
# 上下文切换函数：将 CPU 从进程 from 切换到进程 to。
# 参数约定：a0 指向 from 的 proc_struct，a1 指向 to 的 proc_struct。
# 该函数保存 from 的必须保留寄存器到其 proc_struct 中，然后从 to 的 proc_struct 恢复这些寄存器，
# 最后通过 ret（使用恢复后的 ra）跳转到 to 的继续执行点。
# 进程结构体中为每个寄存器分配 REGBYTES 字节（通常为 8 字节），偏移按寄存器索引排列。
.globl switch_to
switch_to:
    # 保存 from 进程的寄存器到其 proc_struct（a0）
    # 保存的寄存器包括：ra（返回地址）、sp（栈指针）以及 RISC-V 的 callee-saved 寄存器 s0-s11
    # 这些寄存器在上下文切换期间必须被保存，以便之后能完整恢复进程执行状态。
    STORE ra, 0*REGBYTES(a0)
    STORE sp, 1*REGBYTES(a0)
    STORE s0, 2*REGBYTES(a0)
    STORE s1, 3*REGBYTES(a0)
    STORE s2, 4*REGBYTES(a0)
    STORE s3, 5*REGBYTES(a0)
    STORE s4, 6*REGBYTES(a0)
    STORE s5, 7*REGBYTES(a0)
    STORE s6, 8*REGBYTES(a0)
    STORE s7, 9*REGBYTES(a0)
    STORE s8, 10*REGBYTES(a0)
    STORE s9, 11*REGBYTES(a0)
    STORE s10, 12*REGBYTES(a0)
    STORE s11, 13*REGBYTES(a0)

    # 从 to 进程的 proc_struct（a1）恢复寄存器
    # 恢复相同的一组寄存器：ra/sp/s0-s11。恢复后 ret 会使用恢复的 ra 跳回到 to 的执行位置。
    LOAD ra, 0*REGBYTES(a1)
    LOAD sp, 1*REGBYTES(a1)
    LOAD s0, 2*REGBYTES(a1)
    LOAD s1, 3*REGBYTES(a1)
    LOAD s2, 4*REGBYTES(a1)
    LOAD s3, 5*REGBYTES(a1)
    LOAD s4, 6*REGBYTES(a1)
    LOAD s5, 7*REGBYTES(a1)
    LOAD s6, 8*REGBYTES(a1)
    LOAD s7, 9*REGBYTES(a1)
    LOAD s8, 10*REGBYTES(a1)
    LOAD s9, 11*REGBYTES(a1)
    LOAD s10, 12*REGBYTES(a1)
    LOAD s11, 13*REGBYTES(a1)

    # 完成切换并返回到恢复后的 ra（to 进程的返回/继续地址）
    ret
