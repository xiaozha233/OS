#include <defs.h>
#include <unistd.h>
#include <stdarg.h>
#include <syscall.h>

#define MAX_ARGS            5
// 参数num为系统调用号（内核服务的身份证号）
// 后面的省略号表示可变参数列表
static inline int
syscall(int64_t num, ...) {
    va_list ap; // 用于遍历不定参数列表
    va_start(ap, num);// 初始化ap，指向第一个可变参数（num之后的第一个参数）
    uint64_t a[MAX_ARGS];// 用于存放参数
    int i, ret;// ret用于接收系统调用返回值并作为函数返回值
    // 读取可变参数写入数组a
    for (i = 0; i < MAX_ARGS; i ++) {
        a[i] = va_arg(ap, uint64_t);
    }
    va_end(ap);// 清理va_list，结束对可变参数的读取
    // 内联汇编：把参数放到寄存器并执行ecall
    // %1代表第一个输入操作数的内存值num，0是output
    // 在这份 ucore 实现里，系统调用号使用 a0 保存
    asm volatile (
        "ld a0, %1\n"
        "ld a1, %2\n"
        "ld a2, %3\n"
        "ld a3, %4\n"
        "ld a4, %5\n"
    	"ld a5, %6\n"
        "ecall\n"
        "sd a0, %0"   // ecall返回值存入ret
        : "=m" (ret) // 表示输出 operand，= 表示写入（output-only），m 表示该 operand 是内存地址
        : "m"(num), "m"(a[0]), "m"(a[1]), "m"(a[2]), "m"(a[3]), "m"(a[4]) // 输入 operand 列表
        :"memory"); // 告诉编译器：此 asm 可能读写内存，强制在 asm 之前后刷新/重新加载内存相关变量，防止编译器做错误的内存优化（很重要）
    return ret;
}

int
sys_exit(int64_t error_code) {
    return syscall(SYS_exit, error_code);
}

int
sys_fork(void) {
    return syscall(SYS_fork);
}

int
sys_wait(int64_t pid, int *store) {
    return syscall(SYS_wait, pid, store);
}

int
sys_yield(void) {
    return syscall(SYS_yield);
}

int
sys_kill(int64_t pid) {
    return syscall(SYS_kill, pid);
}

int
sys_getpid(void) {
    return syscall(SYS_getpid);
}

int
sys_putc(int64_t c) {
    return syscall(SYS_putc, c);
}

int
sys_pgdir(void) {
    return syscall(SYS_pgdir);
}

