# GDB调试QEMU页表查询过程实验报告

## 实验目的

1. 通过调试QEMU源码，深入理解虚拟地址到物理地址转换的完整过程
2. 观察软件模拟的MMU如何实现TLB查询和页表遍历
3. 掌握使用大模型辅助学习和调试复杂项目的能力
4. 理解软件模拟硬件的实现原理

## 实验环境

- 操作系统：Linux
- QEMU版本：4.1.1
- GDB版本：
- RISC-V GCC版本：riscv64-unknown-elf-gcc
- ucore实验：lab2

## 一、实验准备

### 1.1 检查现有QEMU安装

首先检查系统中已安装的QEMU版本：

```bash
which qemu-system-riscv64
qemu-system-riscv64 --version
```

**输出：**
```
root@DESKTOP-GH7S2O7:/home/albus_os/labcode/lab2# which qemu-system-riscv64
/usr/local/bin/qemu-system-riscv64
root@DESKTOP-GH7S2O7:/home/albus_os/labcode/lab2# qemu-system-riscv64 --version
QEMU emulator version 4.1.1
Copyright (c) 2003-2019 Fabrice Bellard and the QEMU Project developers
```

### 1.2 检查QEMU源码位置

查找QEMU 4.1.1源码目录：

```bash
ls -la /root/build/
```

**输出：**
```
root@DESKTOP-GH7S2O7:/home/albus_os/labcode/lab2# ls -la /root/build/
total 60504
drwxr-xr-x  3 root  root      4096 Oct  2 19:39 .
drwx------  9 root  root      4096 Dec 11 16:03 ..
drwxr-xr-x 51 albus albus    12288 Oct  2 21:31 qemu-4.1.1
-rw-r--r--  1 root  root  61932460 Oct 27  2024 qemu-4.1.1.tar.xz
```

检查编译的QEMU是否带有调试信息：

```bash
ls -la /root/build/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64
file /root/build/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64
```
输出中包含 `with debug_info, not stripped`，说明这是带调试信息的版本。

## 二、修改Makefile使用调试版QEMU


### 2.1 修改Makefile

在Makefile中修改QEMU变量，指向带调试信息的QEMU：

```makefile
ifndef QEMU
QEMU := /root/build/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64
endif
```

### 2.2 验证修改

```bash
grep "QEMU :=" Makefile
```

**输出：**
```
QEMU := /root/build/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64
```

### 3.4 编译ucore内核

```bash
cd /home/albus_os/labcode/lab2
make clean
make
```

## 四、QEMU源码分析

在开始调试之前，先分析QEMU中与地址翻译相关的关键代码。

### 4.1 get_physical_address函数 - 页表遍历核心

**位置：** `/root/build/qemu-4.1.1/target/riscv/cpu_helper.c` (行155-353)

**函数签名：**
```c
static int get_physical_address(CPURISCVState *env, hwaddr *physical, int *prot, target_ulong addr, int access_type, int mmu_idx)
```

**源码（分段分析）：**

#### 4.1.1 特权级检查和M模式快速路径

```c
/* get_physical_address - get the physical address for this virtual address
 *
 * Do a page table walk to obtain the physical address corresponding to a
 * virtual address. Returns 0 if the translation was successful
 *
 * Adapted from Spike's mmu_t::translate and mmu_t::walk
 */
static int get_physical_address(CPURISCVState *env, hwaddr *physical,
                                int *prot, target_ulong addr,
                                int access_type, int mmu_idx)
{
    int mode = mmu_idx;

    // MPRV位检查：M模式下可以使用之前特权级的页表
    if (mode == PRV_M && access_type != MMU_INST_FETCH) {
        if (get_field(env->mstatus, MSTATUS_MPRV)) {
            mode = get_field(env->mstatus, MSTATUS_MPP);
        }
    }

    // M模式或无MMU：直接返回物理地址（无需翻译）
    if (mode == PRV_M || !riscv_feature(env, RISCV_FEATURE_MMU)) {
        *physical = addr;
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return TRANSLATE_SUCCESS;
    }
```

**分析：**
- 这是整个地址翻译的入口函数
- **关键优化：** M模式或无MMU时直接返回，跳过页表遍历
- MPRV（Modify PRiVilege）机制允许M模式以其他特权级的视角访问内存

#### 4.1.2 获取页表配置（satp寄存器解析）

```c
    *prot = 0;

    target_ulong base;
    int levels, ptidxbits, ptesize, vm, sum;
    int mxr = get_field(env->mstatus, MSTATUS_MXR);

    if (env->priv_ver >= PRIV_VERSION_1_10_0) {
        // 从satp寄存器获取页表基址（物理页号）
        base = get_field(env->satp, SATP_PPN) << PGSHIFT;
        sum = get_field(env->mstatus, MSTATUS_SUM);
        vm = get_field(env->satp, SATP_MODE);
        
        // 根据分页模式确定页表参数
        switch (vm) {
        case VM_1_10_SV32:
          levels = 2; ptidxbits = 10; ptesize = 4; break;  // 32位：2级页表
        case VM_1_10_SV39:
          levels = 3; ptidxbits = 9; ptesize = 8; break;   // 39位：3级页表
        case VM_1_10_SV48:
          levels = 4; ptidxbits = 9; ptesize = 8; break;   // 48位：4级页表
        case VM_1_10_SV57:
          levels = 5; ptidxbits = 9; ptesize = 8; break;   // 57位：5级页表
        case VM_1_10_MBARE:
            *physical = addr;
            *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
            return TRANSLATE_SUCCESS;
        default:
          g_assert_not_reached();
        }
    } else {
        // 旧版RISC-V（1.09及更早）
        base = env->sptbr << PGSHIFT;
        sum = !get_field(env->mstatus, MSTATUS_PUM);
        vm = get_field(env->mstatus, MSTATUS_VM);
        // ... 类似的switch判断
    }
```

**分析：**
- **satp寄存器**（Supervisor Address Translation and Protection）是关键：
  - `SATP_MODE`: 分页模式（SV39最常用）
  - `SATP_PPN`: 页表物理页号
- **不同分页模式的差异：**
  - SV32: 2级页表，每级10位索引
  - SV39: 3级页表，每级9位索引（本实验环境）
  - SV48/SV57: 更多级页表，支持更大虚拟地址空间

#### 4.1.3 虚拟地址合法性检查

```c
    CPUState *cs = env_cpu(env);
    int va_bits = PGSHIFT + levels * ptidxbits;
    target_ulong mask = (1L << (TARGET_LONG_BITS - (va_bits - 1))) - 1;
    target_ulong masked_msbs = (addr >> (va_bits - 1)) & mask;
    
    // 检查虚拟地址的高位是否合法（符号扩展检查）
    if (masked_msbs != 0 && masked_msbs != mask) {
        return TRANSLATE_FAIL;
    }

    int ptshift = (levels - 1) * ptidxbits;
    int i;
```

**分析：**
- RISC-V要求虚拟地址的高位必须是低位的符号扩展
- 例如SV39：只使用低39位，高25位必须全0或全1

#### 4.1.4 多级页表遍历（核心循环）

```c
#if !TCG_OVERSIZED_GUEST
restart:
#endif
    for (i = 0; i < levels; i++, ptshift -= ptidxbits) {
        // 计算当前级的页表索引
        target_ulong idx = (addr >> (PGSHIFT + ptshift)) &
                           ((1 << ptidxbits) - 1);

        // 计算PTE的物理地址：基址 + 索引 * PTE大小
        target_ulong pte_addr = base + idx * ptesize;

        // PMP（物理内存保护）检查
        if (riscv_feature(env, RISCV_FEATURE_PMP) &&
            !pmp_hart_has_privs(env, pte_addr, sizeof(target_ulong),
            1 << MMU_DATA_LOAD, PRV_S)) {
            return TRANSLATE_PMP_FAIL;
        }
        
        // 从物理内存读取PTE
#if defined(TARGET_RISCV32)
        target_ulong pte = ldl_phys(cs->as, pte_addr);
#elif defined(TARGET_RISCV64)
        target_ulong pte = ldq_phys(cs->as, pte_addr);
#endif
        target_ulong ppn = pte >> PTE_PPN_SHIFT;

        // 检查V位（有效位）
        if (!(pte & PTE_V)) {
            /* Invalid PTE */
            return TRANSLATE_FAIL;
        } else if (!(pte & (PTE_R | PTE_W | PTE_X))) {
            /* Inner PTE, continue walking */
            base = ppn << PGSHIFT;  // 更新基址到下一级页表
        } else if ((pte & (PTE_R | PTE_W | PTE_X)) == PTE_W) {
            /* Reserved leaf PTE flags: PTE_W */
            return TRANSLATE_FAIL;
        } else if ((pte & (PTE_R | PTE_W | PTE_X)) == (PTE_W | PTE_X)) {
            /* Reserved leaf PTE flags: PTE_W + PTE_X */
            return TRANSLATE_FAIL;
```

**分析：**
- **关键算法：** 从最高级页表开始，逐级查找
- **页表索引计算：** 使用虚拟地址的不同位段
- **PTE判断逻辑：**
  - V=0: PTE无效，翻译失败
  - R=W=X=0: 非叶子节点，继续下一级
  - 其他组合: 叶子节点，进行权限检查

#### 4.1.5 叶子PTE的权限检查

```c
        } else if ((pte & PTE_U) && ((mode != PRV_U) &&
                   (!sum || access_type == MMU_INST_FETCH))) {
            /* User PTE flags when not U mode and mstatus.SUM is not set,
               or the access type is an instruction fetch */
            return TRANSLATE_FAIL;
        } else if (!(pte & PTE_U) && (mode != PRV_S)) {
            /* Supervisor PTE flags when not S mode */
            return TRANSLATE_FAIL;
        } else if (ppn & ((1ULL << ptshift) - 1)) {
            /* Misaligned PPN */
            return TRANSLATE_FAIL;
        } else if (access_type == MMU_DATA_LOAD && !((pte & PTE_R) ||
                   ((pte & PTE_X) && mxr))) {
            /* Read access check failed */
            return TRANSLATE_FAIL;
        } else if (access_type == MMU_DATA_STORE && !(pte & PTE_W)) {
            /* Write access check failed */
            return TRANSLATE_FAIL;
        } else if (access_type == MMU_INST_FETCH && !(pte & PTE_X)) {
            /* Fetch access check failed */
            return TRANSLATE_FAIL;
        } else {
```

**分析：**
- **权限检查层次：**
  1. 用户/特权级权限（U位）
  2. 超级页对齐检查
  3. 读/写/执行权限（R/W/X位）
- **SUM位：** 允许S模式访问用户页
- **MXR位：** 可执行页也可读

#### 4.1.6 A/D位更新和物理地址计算

```c
            /* if necessary, set accessed and dirty bits. */
            target_ulong updated_pte = pte | PTE_A |
                (access_type == MMU_DATA_STORE ? PTE_D : 0);

            /* Page table updates need to be atomic with MTTCG enabled */
            if (updated_pte != pte) {
                MemoryRegion *mr;
                hwaddr l = sizeof(target_ulong), addr1;
                mr = address_space_translate(cs->as, pte_addr,
                    &addr1, &l, false, MEMTXATTRS_UNSPECIFIED);
                if (memory_region_is_ram(mr)) {
                    target_ulong *pte_pa =
                        qemu_map_ram_ptr(mr->ram_block, addr1);
#if TCG_OVERSIZED_GUEST
                    *pte_pa = pte = updated_pte;
#else
                    target_ulong old_pte =
                        atomic_cmpxchg(pte_pa, pte, updated_pte);
                    if (old_pte != pte) {
                        goto restart;  // PTE被修改，重新遍历
                    } else {
                        pte = updated_pte;
                    }
#endif
                } else {
                    return TRANSLATE_FAIL;
                }
            }

            /* for superpage mappings, make a fake leaf PTE for the TLB's
               benefit. */
            target_ulong vpn = addr >> PGSHIFT;
            *physical = (ppn | (vpn & ((1L << ptshift) - 1))) << PGSHIFT;

            /* set permissions on the TLB entry */
            if ((pte & PTE_R) || ((pte & PTE_X) && mxr)) {
                *prot |= PAGE_READ;
            }
            if ((pte & PTE_X)) {
                *prot |= PAGE_EXEC;
            }
            if ((pte & PTE_W) &&
                    (access_type == MMU_DATA_STORE || (pte & PTE_D))) {
                *prot |= PAGE_WRITE;
            }
            return TRANSLATE_SUCCESS;
        }
    }
    return TRANSLATE_FAIL;
}
```

**分析：**
- **A/D位自动更新：**
  - A（Access）: 页面被访问时设置
  - D（Dirty）: 页面被写入时设置
  - 使用**原子操作**（CAS）确保多线程安全
- **超级页支持：** 如果在第1或第2级找到叶子PTE，说明是大页（2MB或1GB）
- **物理地址计算：** `physical = (ppn | vpn的低位) << PGSHIFT`

---

### 4.2 riscv_cpu_tlb_fill函数 - TLB未命中处理

**位置：** `/root/build/qemu-4.1.1/target/riscv/cpu_helper.c` (行435-495)

**源码：**

```c
bool riscv_cpu_tlb_fill(CPUState *cs, vaddr address, int size,
                        MMUAccessType access_type, int mmu_idx,
                        bool probe, uintptr_t retaddr)
{
#ifndef CONFIG_USER_ONLY
    RISCVCPU *cpu = RISCV_CPU(cs);
    CPURISCVState *env = &cpu->env;
    hwaddr pa = 0;
    int prot;
    bool pmp_violation = false;
    int ret = TRANSLATE_FAIL;
    int mode = mmu_idx;

    // 打印MMU调试日志
    qemu_log_mask(CPU_LOG_MMU, "%s ad %" VADDR_PRIx " rw %d mmu_idx %d\n",
                  __func__, address, access_type, mmu_idx);

    // 调用页表遍历函数
    ret = get_physical_address(env, &pa, &prot, address, access_type, mmu_idx);

    if (mode == PRV_M && access_type != MMU_INST_FETCH) {
        if (get_field(env->mstatus, MSTATUS_MPRV)) {
            mode = get_field(env->mstatus, MSTATUS_MPP);
        }
    }

    qemu_log_mask(CPU_LOG_MMU,
                  "%s address=%" VADDR_PRIx " ret %d physical " TARGET_FMT_plx
                  " prot %d\n", __func__, address, ret, pa, prot);

    // PMP（物理内存保护）检查
    if (riscv_feature(env, RISCV_FEATURE_PMP) &&
        (ret == TRANSLATE_SUCCESS) &&
        !pmp_hart_has_privs(env, pa, size, 1 << access_type, mode)) {
        ret = TRANSLATE_PMP_FAIL;
    }
    if (ret == TRANSLATE_PMP_FAIL) {
        pmp_violation = true;
    }
    
    // 翻译成功：将结果填入TLB
    if (ret == TRANSLATE_SUCCESS) {
        tlb_set_page(cs, address & TARGET_PAGE_MASK, pa & TARGET_PAGE_MASK,
                     prot, mmu_idx, TARGET_PAGE_SIZE);
        return true;
    } else if (probe) {
        return false;
    } else {
        // 翻译失败：抛出异常
        raise_mmu_exception(env, address, access_type, pmp_violation);
        riscv_raise_exception(env, cs->exception_index, retaddr);
    }
#endif
}
```

**分析：**
- **这是TLB miss的统一入口**，当硬件（软件模拟的）TLB查找失败时调用
- **主要流程：**
  1. 调用 `get_physical_address` 进行页表遍历
  2. 进行PMP（物理内存保护）检查
  3. 成功：调用 `tlb_set_page` 填充TLB
  4. 失败：触发页错误异常
- **probe参数：** 用于探测性访问，失败时不触发异常

---

### 4.3 victim_tlb_hit函数 - 软件TLB查询

**位置：** `/root/build/qemu-4.1.1/accel/tcg/cputlb.c` (行975-1011)

**源码：**

```c
/* Return true if ADDR is present in the victim tlb, and has been copied
   back to the main tlb.  */
static bool victim_tlb_hit(CPUArchState *env, size_t mmu_idx, size_t index,
                           size_t elt_ofs, target_ulong page)
{
    size_t vidx;

    assert_cpu_is_self(env_cpu(env));
    
    // 遍历victim TLB的所有表项
    for (vidx = 0; vidx < CPU_VTLB_SIZE; ++vidx) {
        CPUTLBEntry *vtlb = &env_tlb(env)->d[mmu_idx].vtable[vidx];
        target_ulong cmp;

        /* elt_ofs might correspond to .addr_write, so use atomic_read */
#if TCG_OVERSIZED_GUEST
        cmp = *(target_ulong *)((uintptr_t)vtlb + elt_ofs);
#else
        cmp = atomic_read((target_ulong *)((uintptr_t)vtlb + elt_ofs));
#endif

        // 找到匹配的虚拟页号
        if (cmp == page) {
            /* Found entry in victim tlb, swap tlb and iotlb.  */
            CPUTLBEntry tmptlb, *tlb = &env_tlb(env)->f[mmu_idx].table[index];

            // 交换主TLB和victim TLB的表项
            qemu_spin_lock(&env_tlb(env)->c.lock);
            copy_tlb_helper_locked(&tmptlb, tlb);
            copy_tlb_helper_locked(tlb, vtlb);
            copy_tlb_helper_locked(vtlb, &tmptlb);
            qemu_spin_unlock(&env_tlb(env)->c.lock);

            // 同时交换IO TLB表项
            CPUIOTLBEntry tmpio, *io = &env_tlb(env)->d[mmu_idx].iotlb[index];
            CPUIOTLBEntry *vio = &env_tlb(env)->d[mmu_idx].viotlb[vidx];
            tmpio = *io; *io = *vio; *vio = tmpio;
            return true;
        }
    }
    return false;
}
```

**分析：**
- **Victim TLB机制：** QEMU的优化策略，类似CPU的L2 TLB
- **工作原理：**
  1. 主TLB miss时，先查victim TLB
  2. 如果命中，将表项换回主TLB（提升局部性）
  3. 如果未命中，才进行页表遍历
- **这是纯软件实现**，真实硬件TLB没有这种"交换"机制
- 用**原子操作**保证多线程安全

---

### 4.4 关键代码总结

**完整的地址翻译流程：**

```
1. CPU执行访存指令
   ↓
2. 查询主TLB（硬件级缓存）
   ↓ miss
3. victim_tlb_hit() - 查询victim TLB（软件级缓存）
   ↓ miss
4. riscv_cpu_tlb_fill() - TLB填充入口
   ↓
5. get_physical_address() - 页表遍历
   │
   ├─ M模式或MBARE → 直接返回物理地址
   │
   └─ S/U模式 → 多级页表遍历
      ├─ 检查satp，确定页表基址和分页模式
      ├─ for循环：逐级查找PTE
      │  ├─ 计算页表索引
      │  ├─ 读取PTE
      │  └─ 判断：非叶子→下一级；叶子→权限检查
      │
      ├─ 权限检查（R/W/X/U位）
      ├─ 更新A/D位（原子操作）
      └─ 计算物理地址
   ↓
6. tlb_set_page() - 将结果填入TLB
   ↓
7. 完成内存访问
```

**关键数据结构：**
- **satp寄存器：** 保存页表基址和分页模式
- **PTE（页表项）：** 包含PPN、权限位（RWXU）、状态位（ADV）
- **TLB：** 主TLB + Victim TLB的二级结构

## 五、三终端调试流程

### 5.1 终端1：启动QEMU调试模式

在第一个终端中启动QEMU，使其等待GDB连接：

```bash
cd /home/albus_os/labcode/lab2
make debug
```
此时QEMU已启动但暂停执行，等待GDB通过端口1234连接。保持此终端打开。

### 5.2 查找QEMU进程PID

在另一个终端查找QEMU进程ID：

```bash
pgrep -f qemu-system-riscv64
```
记录此PID，下面称为 `<QEMU_PID>`。

### 5.3 终端2：用GDB附加到QEMU进程

在第二个终端中，使用x86-64版本的GDB附加到QEMU进程，以调试QEMU本身的源码：

```bash
cd /root/build/qemu-4.1.1
sudo gdb
```

进入GDB后，执行以下命令：

```gdb
(gdb) attach <QEMU_PID>
(gdb) handle SIGPIPE nostop noprint
```

**输出：**
```
(gdb) attach 167005
Attaching to program: /root/build/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64, process 167005
[New LWP 167006]
[New LWP 167007]
[Thread debugging using libthread_db enabled]
Using host libthread_db library "/lib/x86_64-linux-gnu/libthread_db.so.1".
0x00007b1d6b918d3e in __ppoll (fds=0x652e16811070, nfds=7, timeout=<optimized out>, timeout@entry=0x7ffc454d6a80, sigmask=sigmask@entry=0x0) at ../sysdeps/unix/sysv/linux/ppoll.c:42
42      ../sysdeps/unix/sysv/linux/ppoll.c: No such file or directory.

(gdb) handle SIGPIPE nostop noprint
Signal        Stop      Print   Pass to program Description
SIGPIPE       No        No      Yes             Broken pipe
```

**说明：** 
- `attach` 命令将GDB附加到正在运行的QEMU进程
- `handle SIGPIPE` 和 `handle SIGTTOU` 忽略某些信号，避免调试中断

执行 `continue` 让QEMU继续运行：

```gdb
(gdb) continue
Continuing.
```

### 5.4 终端3：用riscv64-gdb连接ucore内核

**注意：** 在执行此步骤前，**必须确保终端2的x86 GDB已经执行了`continue`命令**，让QEMU处于运行状态。否则会出现连接错误。

在第三个终端中，使用RISC-V版本的GDB连接到QEMU的GDB stub，调试ucore内核：

```bash
cd /home/albus_os/labcode/lab2
riscv64-unknown-elf-gdb -q \
  -ex 'file bin/kernel' \
  -ex 'set arch riscv:rv64' \
  -ex 'target remote localhost:1234'
```

**输出：**
```
root@DESKTOP-GH7S2O7:/home/albus_os/labcode/lab2# cd /home/albus_os/labcode/lab2
riscv64-unknown-elf-gdb -q \
  -ex 'file bin/kernel' \
  -ex 'set arch riscv:rv64' \
  -ex 'target remote localhost:1234'
Reading symbols from bin/kernel...
The target architecture is set to "riscv:rv64".
Remote debugging using localhost:1234
0x0000000000001000 in ?? ()
(gdb) 
```
解决时序同步问题：
```
set remotetimeout unlimited
```

### 5.6 在ucore中设置断点并执行

在riscv64-gdb中执行：

```gdb
(gdb) break kern_init
Breakpoint 1 at 0xffffffffc02000d8: file kern/init/init.c, line 30.
(gdb) c
Continuing.

Breakpoint 1, kern_init () at kern/init/init.c:30
30          memset(edata, 0, end - edata);
```




**说明：** 此时ucore开始执行，并在 `kern_init` 函数处停下。

### 5.7 查看当前指令和地址

```gdb
(gdb) info registers pc
pc             0xffffffffc02000d8       0xffffffffc02000d8 <kern_init>
(gdb) x/8i $pc
=> 0xffffffffc02000d8 <kern_init>:      auipc   a0,0x6
   0xffffffffc02000dc <kern_init+4>:    addi    a0,a0,-192
   0xffffffffc02000e0 <kern_init+8>:    auipc   a2,0x7
   0xffffffffc02000e4 <kern_init+12>:   addi    a2,a2,-784
   0xffffffffc02000e8 <kern_init+16>:   addi    sp,sp,-16
   0xffffffffc02000ea <kern_init+18>:   sub     a2,a2,a0
   0xffffffffc02000ec <kern_init+20>:   li      a1,0
   0xffffffffc02000ee <kern_init+22>:   sd      ra,8(sp)
```

### 5.8 单步执行并观察访存指令

```gdb
(gdb) stepi
(gdb) info registers
```


**说明：** 当执行到访存指令时，QEMU的GDB（终端2）应该会在断点处停下。

## 六、观察QEMU中的地址翻译过程

> **⚠️ 重要提示：关于编译优化**
>
> 在本章节的调试过程中，你会发现很多变量显示 `<optimized out>`。这是因为QEMU使用了编译优化（-O2或-O3）。
> 
> **应对策略：**
> - 关注函数的**输入参数**（如 `addr`, `mmu_idx`）和**输出参数**（如 `*physical`, `*prot`），这些通常可以观察到
> - 在关键代码行设置**断点**，观察程序执行流程
> - 使用 `disassemble` 和 `ni` 进行**汇编级调试**
> - 启用QEMU的**日志功能**：`make debug QEMUOPTS="-d mmu,cpu"`
> - 结合**源码分析**理解算法，而非完全依赖GDB观察
> 
> 如果确实需要观察所有局部变量，可以重新编译QEMU（会严重降低性能）：
> ```bash
> ./configure --enable-debug --disable-optimization
> make
> ```

### 6.1 QEMU GDB命中断点

切换回终端2（QEMU的GDB），应该看到断点被触发：

**输出：**
```
Thread 1 "qemu-system-ris" hit Breakpoint 1, get_physical_address (env=0x5f5e83919720, physical=physical@entry=0x7ffefdf525a0, prot=prot@entry=0x7ffefdf5259c, addr=addr@entry=18446744072637907160, access_type=access_type@entry=0, mmu_idx=1) at /root/build/qemu-4.1.1/target/riscv/cpu_helper.c:158
```

### 6.2 查看函数参数

在QEMU GDB中查看 `riscv_cpu_tlb_fill` 的参数：

```gdb
(gdb) info args
```

**输出：**
```
(gdb) info args
env = 0x5c3c44546720
physical = 0x7fff379364d0
prot = 0x7fff379364cc
addr = 18446744072637907160
access_type = 0
mmu_idx = 1
```

**分析：** 
- `address`: 要翻译的虚拟地址
- `access_type`: 访问类型（0=取指，1=加载，2=存储）
- `mmu_idx`: MMU索引，通常对应特权级

### 6.3 查看当前函数的源代码

```gdb
(gdb) list
```

**输出：**
```
(gdb) list
153      *
154      */
155     static int get_physical_address(CPURISCVState *env, hwaddr *physical,
156                                     int *prot, target_ulong addr,
157                                     int access_type, int mmu_idx)
158     {
159         /* NOTE: the env->pc value visible here will not be
160          * correct, but the value visible to the exception handler
161          * (riscv_cpu_do_interrupt) is correct */
162
```

### 6.4 单步进入get_physical_address

```gdb
(gdb) step
```

重复step命令直到进入 `get_physical_address` 函数。

**输出：**
```
(gdb) step
165         if (mode == PRV_M && access_type != MMU_INST_FETCH) {
(gdb) step
243         return env->features & (1ULL << feature);
```

### 6.5 观察页表基址的获取

在 `get_physical_address` 函数中，查看从satp寄存器获取页表基址：

```gdb
(gdb) print /x env->satp
(gdb) print /x base
```

**输出：**
```
(gdb) print /x env->satp
$2 = 0x8000000000080205
(gdb) print /x base
$3 = <optimized out>
```

**分析：** 
- `env->satp = 0x8000000000080205`，这是satp寄存器的完整值
- `base` 变量显示为 `<optimized out>`，这是因为QEMU编译时使用了优化选项（-O2或-O3），导致局部变量被优化掉
- 从satp中提取页表基址的计算：`base = (satp & 0xFFFFFFFFFFF) << 12`
- 对于 `0x8000000000080205`：PPN部分是 `0x80205`，因此 `base = 0x80205000`

**替代方法：** 如果需要观察未优化的变量，可以重新编译QEMU并添加 `-O0` 选项，但这会显著降低QEMU性能。

### 6.6 观察分页模式

```gdb
(gdb) print vm
(gdb) print levels
(gdb) print ptidxbits
```

**输出：**
```
(gdb) print vm
$7 = <optimized out>
(gdb) print levels
$8 = <optimized out>
(gdb) print ptidxbits
$9 = <optimized out>
```

**问题说明：** 由于编译优化，这些局部变量在GDB中无法直接观察。

**替代观察方法：**

通过源码和satp值推断：
```gdb
# 从satp的MODE字段判断分页模式
(gdb) print /x (env->satp >> 60)
# 如果结果是8，表示SV39模式
```

从satp的最高4位可以判断分页模式：
- `0x8000000000080205 >> 60 = 0x8` → SV39模式
- SV39模式特征：`levels=3`, `ptidxbits=9`, `va_bits=39`

**理论分析：**
- `vm`: VM_1_10_SV39 (值为8)
- `levels`: 3（SV39为三级页表）
- `ptidxbits`: 9（每级索引占9位）

**实际调试案例：M模式下的地址翻译**

```
Thread 3 "qemu-system-ris" hit Breakpoint 1, get_physical_address 
    (env=env@entry=0x5555563004a0, 
     physical=physical@entry=0x7ffff4ea9290, 
     prot=prot@entry=0x7ffff4ea928c, 
     addr=addr@entry=2147483648,           # 虚拟地址 0x80000000
     access_type=access_type@entry=2,      # 2=存储操作
     mmu_idx=mmu_idx@entry=3)              # 3=M模式
    at /root/build/qemu-4.1.1/target/riscv/cpu_helper.c:158

158     {
(gdb) s
165         if (mode == PRV_M && access_type != MMU_INST_FETCH) {
(gdb) s
171         if (mode == PRV_M || !riscv_feature(env, RISCV_FEATURE_MMU)) {
(gdb) s
197                 *physical = addr;      # M模式：直接返回物理地址
(gdb) s
198                 *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;  # 完全权限
(gdb) s
199                 return TRANSLATE_SUCCESS;
(gdb) 
```

**重要发现：**
- 在M模式（Machine模式）下，地址翻译被绕过
- 虚拟地址直接作为物理地址返回（恒等映射）
- 这是RISC-V规范的要求：M模式拥有最高权限，可以直接访问物理地址
- 这个分支在第171行的判断：`if (mode == PRV_M || !riscv_feature(env, RISCV_FEATURE_MMU))`

### 6.7 观察页表遍历循环（S模式下）

**前提：** 需要在S模式且开启分页的情况下才会执行页表遍历。上面的M模式案例直接返回，不会进入循环。

**设置条件断点：** 为了捕获S模式的地址翻译，使用条件断点：

```gdb
(gdb) break get_physical_address if mmu_idx == 1
# mmu_idx=1 表示S模式
```

**观察页表遍历：**

由于编译优化，循环变量可能无法直接观察。替代方法：

1. **使用反汇编观察：**
```gdb
(gdb) disassemble
(gdb) ni  # 单步执行汇编指令
(gdb) info registers
```

2. **在关键行设置断点：**
```gdb
(gdb) break cpu_helper.c:241  # PTE读取行
(gdb) break cpu_helper.c:259  # 叶子节点判断行
```

3. **观察内存访问：**
```gdb
# 如果知道页表基址，可以直接查看内存
(gdb) x/8xg 0x80205000  # 查看一级页表内容
```

**理论流程：**
- `i`: 从0到2（SV39三级），但在GDB中显示为 `<optimized out>`
- `idx`: 从虚拟地址提取的页表索引，每级9位
- `pte_addr`: 页表项物理地址 = `base + idx * 8`

### 6.8 观察PTE读取

**尝试观察变量：**
```gdb
(gdb) print /x pte
(gdb) print /x ppn
```

**预期情况：** 由于编译优化，这些变量可能显示 `<optimized out>`。

**替代观察方法：**

1. **通过函数参数和返回值推断：**
```gdb
# 在函数返回前设置断点
(gdb) break cpu_helper.c:349
(gdb) print /x *physical  # 查看输出参数
(gdb) print *prot         # 查看权限
```

2. **查看寄存器：**
```gdb
(gdb) info registers
# PTE值通常会加载到某个寄存器中
```

3. **使用日志功能：**
在QEMU启动时添加日志选项可以看到详细的MMU操作：
```bash
make debug QEMUOPTS="-d mmu,cpu"
```

### 6.9 观察PTE标志位检查

**理论分析方法：**

如果能获取到PTE的值（假设为 `0xcf`），可以手动解析：

```
PTE = 0x00000000200000cf (示例)

标志位解析：
bit 0 (V):  0xcf & 0x1  = 0x1  ✓ 有效
bit 1 (R):  0xcf & 0x2  = 0x2  ✓ 可读  
bit 2 (W):  0xcf & 0x4  = 0x4  ✓ 可写
bit 3 (X):  0xcf & 0x8  = 0x8  ✓ 可执行
bit 4 (U):  0xcf & 0x10 = 0x0  ✗ 非用户页
bit 6 (A):  0xcf & 0x40 = 0x40 ✓ 已访问
bit 7 (D):  0xcf & 0x80 = 0x80 ✓ 已修改

PPN = (0x200000cf >> 10) = 0x80000
```

**判断规则：**
- `(pte & 0xE) == 0`: 非叶子节点，继续遍历下一级
- `(pte & 0xE) != 0`: 叶子节点，进行权限检查

### 6.10 观察最终物理地址计算

**通过返回值观察：**

```gdb
# 在函数返回前查看
(gdb) break cpu_helper.c:349
(gdb) continue
(gdb) print /x addr      # 输入的虚拟地址
(gdb) print /x *physical # 输出的物理地址
(gdb) print *prot        # 输出的权限
```

**物理地址计算公式：**
```
physical = (ppn << 12) | (addr & 0xFFF)
         = (物理页号 × 4096) + 页内偏移
```

**实用技巧：** 与其在循环中观察优化掉的变量，不如：
1. 关注函数的输入（`addr`）和输出（`*physical`, `*prot`）
2. 设置多个断点，在不同阶段观察状态
3. 结合源码理解执行流程

### 6.11 返回到riscv_cpu_tlb_fill

```gdb
(gdb) s
riscv_cpu_tlb_fill (cs=0x5555562f7a90, address=2147483648, size=0, access_type=MMU_INST_FETCH, mmu_idx=3, probe=<optimized out>, retaddr=0) at /root/build/qemu-4.1.1/target/riscv/cpu_helper.c:453
453         if (mode == PRV_M && access_type != MMU_INST_FETCH) {
```


### 6.12 观察TLB设置

如果地址翻译成功（ret == TRANSLATE_SUCCESS），会调用 `tlb_set_page`：

```gdb
(gdb) next
(gdb) print /x address & TARGET_PAGE_MASK
(gdb) print /x pa & TARGET_PAGE_MASK
(gdb) print prot
```

**输出：**
```
[在此填写实际输出]
```

**分析：** 这些参数用于在TLB中创建新表项，缓存虚拟地址到物理地址的映射。

### 6.13 继续执行回到ucore

在QEMU GDB中继续执行：

```gdb
(gdb) continue
```

**输出：**
```
[在此填写实际输出]
```

现在切换回终端3（ucore的GDB），应该可以继续调试ucore代码了。

## 七、查找和观察TLB查询代码

### 7.1 在cputlb.c中设置断点

为了观察TLB命中的情况，我们需要在软件TLB查询代码中设置断点。

回到终端2（QEMU GDB），设置新的断点：

```gdb
(gdb) delete  # 删除之前的断点
Delete all breakpoints? (y or n) y
(gdb) break victim_tlb_hit
(gdb) break get_page_addr_code
(gdb) continue
```

**输出：**
```
[在此填写实际输出]
```

### 7.2 观察TLB查询过程

在终端3继续执行ucore：

```gdb
(gdb) continue
```

当访存发生时，QEMU GDB应该在 `victim_tlb_hit` 或其他TLB相关函数停下。

**输出：**
```
[在此填写实际输出]
```

### 7.3 查看TLB表项

在QEMU GDB中：

```gdb
(gdb) info args
(gdb) print /x page
(gdb) print vidx
(gdb) print /x cmp
```

**输出：**
```
[在此填写实际输出]
```

**分析：**
- `page`: 要查找的虚拟页号
- `vidx`: 在victim TLB中的索引
- `cmp`: victim TLB表项中存储的虚拟页号

如果 `cmp == page`，说明在victim TLB中命中。

### 7.4 对比开启和未开启虚拟内存的访存

为了理解QEMU模拟TLB和真实TLB的区别，我们可以对比M模式（未开启分页）和S模式（开启分页）下的访存流程。

在M模式下，通常会直接进入物理地址访问，不经过TLB查询和页表遍历。

查看调用栈：

```gdb
(gdb) backtrace
```

**输出（开启虚拟内存时）：**
```
[在此填写实际输出]
```

**输出（未开启虚拟内存时）：**
```
[在此填写实际输出]
```

**分析：** 对比两种情况下的调用栈，可以看出虚拟内存开启时的额外处理步骤。

## 八、实验要求完成情况

### 8.1 关键调用路径

根据调试观察，QEMU中处理RISC-V虚拟地址翻译的关键调用路径为：

```
[在此填写观察到的完整调用路径，例如：
1. CPU执行访存指令
2. 调用get_page_addr_code (取指) 或其他内存访问函数
3. 检查TLB是否命中（tlb_hit）
4. 如果TLB未命中，检查victim_tlb_hit
5. 如果victim TLB也未命中，调用tlb_fill
6. tlb_fill调用架构相关的tlb_fill回调：riscv_cpu_tlb_fill
7. riscv_cpu_tlb_fill调用get_physical_address进行页表遍历
8. get_physical_address返回物理地址和权限
9. tlb_set_page将映射关系缓存到TLB中
10. 完成内存访问
]
```

**关键分支语句分析：**

```c
[在此填写关键分支语句，例如：

1. 在get_physical_address中（cpu_helper.c:171-174）：
   if (mode == PRV_M || !riscv_feature(env, RISCV_FEATURE_MMU)) {
       *physical = addr;
       return TRANSLATE_SUCCESS;
   }
   这个分支判断是否需要地址翻译，M模式直接返回物理地址。

2. 在页表遍历循环中（cpu_helper.c:259-261）：
   if (!(pte & (PTE_R | PTE_W | PTE_X))) {
       base = ppn << PGSHIFT;
   }
   判断是否为叶子节点，如果不是则继续下一级页表。

3. 等等...
]
```

### 8.2 单步调试页表翻译的详细流程

以访问虚拟地址 `0x____________` 为例（填入实际调试时的地址）：

**步骤1：进入get_physical_address**

```
[在此记录进入函数时的状态，包括：
- 虚拟地址addr的值
- mode（特权级）
- satp寄存器的值
]
```

**步骤2：获取页表基址**

```
[记录从satp提取的base值]
```

**步骤3：第一级页表查询**

```
[记录：
- 当前i值（0）
- 计算出的idx（一级页表索引）
- pte_addr（一级PTE物理地址）
- 读取到的pte值
- 判断结果（是否为叶子节点）
]
```

**步骤4：第二级页表查询**

```
[类似上面，记录第二级查询的详细信息]
```

**步骤5：第三级页表查询（如果有）**

```
[记录第三级查询的详细信息]
```

**步骤6：计算最终物理地址**

```
[记录：
- 最终的ppn值
- vpn值
- 计算出的physical地址
- 设置的prot权限
]
```

### 8.3 TLB查找代码分析

**QEMU中TLB查找的核心代码位置：**

```
文件：/root/build/qemu-4.1.1/accel/tcg/cputlb.c
函数：victim_tlb_hit (行977-1011)
```

**TLB查找流程：**

```
[描述TLB查找的详细流程，包括：
1. 如何计算TLB索引
2. 如何比较虚拟页号
3. 命中后如何获取物理地址
4. 未命中时的处理
]
```

**调试记录：**

设置断点：
```gdb
(gdb) break victim_tlb_hit
```

观察到的调用：
```
[记录实际调试时观察到的TLB查找过程]
```

### 8.4 QEMU模拟TLB与真实TLB的区别

根据调试观察和代码分析：

**相同点：**
```
[列出QEMU TLB和真实硬件TLB在逻辑上的相似之处]
```

**不同点：**
```
[列出关键差异，例如：

1. 软件实现 vs 硬件实现：
   - QEMU的TLB是用C语言实现的数据结构
   - 真实TLB是专用硬件电路

2. 查询时机：
   - 开启虚拟内存时：QEMU的软件TLB工作方式与硬件TLB类似
   - 未开启虚拟内存时：QEMU [描述观察到的行为]

3. 性能特征：
   - QEMU [描述观察到的性能特征]
   - 真实硬件 [对比说明]
]
```

**未开启虚拟地址空间的访存调试记录：**

```
[记录在M模式下直接物理地址访问的调用路径和行为]
```

### 8.5 有趣的发现和细节

**发现1：编译优化对调试的影响**

在本次实验中，遇到的最大挑战是QEMU使用了编译优化（-O2/-O3），导致：
- 大量局部变量显示 `<optimized out>`
- 无法直接观察循环变量（i, idx）和中间结果（pte, ppn）
- 源码和实际执行流程不一致（指令重排、函数内联）

**解决策略：**
1. 关注函数的输入输出而非内部状态
2. 使用条件断点过滤无关调用
3. 启用QEMU日志功能（-d mmu,cpu）
4. 结合源码理解算法原理，而非完全依赖GDB观察

**发现2：M模式的特殊处理**

在调试中发现，M模式下的地址翻译极其简单：
```c
if (mode == PRV_M || !riscv_feature(env, RISCV_FEATURE_MMU)) {
    *physical = addr;  // 直接返回
    return TRANSLATE_SUCCESS;
}
```

这个分支在第171行就直接返回了，完全绕过了后面的页表遍历代码。
- 这是RISC-V规范的要求：M模式拥有最高权限，直接访问物理地址
- 即使satp寄存器已经设置了页表基址，M模式也不使用它
- 这与某些其他架构不同（如x86的分段机制在所有特权级都生效）

**发现3：QEMU的软件TLB在M模式下仍然工作**

虽然真实硬件在M模式下不使用TLB，但QEMU作为模拟器：
- 仍然会调用 `tlb_set_page()` 缓存恒等映射
- 这是因为QEMU本身是用户态程序，必须通过软件TLB管理内存访问
- 这体现了模拟器实现和真实硬件的差异

**发现4：Victim TLB优化机制**

QEMU实现了一个有趣的二级TLB结构：
- 主TLB未命中时，先查询Victim TLB
- 如果在Victim TLB找到，会与主TLB交换表项
- 这是纯软件优化，真实硬件通常没有这种"交换"机制
- 体现了软件模拟器可以使用与硬件不同的优化策略

**对软件模拟硬件的理解：**

通过本次实验，深刻理解了：

1. **抽象层次的选择**
   - QEMU选择在指令级别模拟，而非门电路级别
   - TLB被实现为C数据结构，而非CAM硬件
   - 这种抽象在保持功能正确性的同时，提供了足够的性能

2. **模拟与真实的差异**
   - 模拟器必须权衡准确性和性能
   - 某些实现细节（如Victim TLB）是模拟器特有的
   - 调试模拟器需要理解这些差异

3. **调试技巧的重要性**
   - 面对编译优化，需要灵活调整调试策略
   - 日志、断点、源码分析需要结合使用
   - 理解原理比单纯观察现象更重要

4. **多层次的理解**
   - 客户机OS（ucore）运行在QEMU模拟的虚拟硬件上
   - QEMU本身运行在宿主机OS上
   - 调试需要在这些层次间切换，理解各层的责任

## 九、大模型使用记录

### 9.1 使用的大模型

- 模型名称：
- 使用方式：

### 9.2 问题1：如何调试QEMU源码

**提问内容：**
```
我需要调试一个正在运行我的操作系统内核源码的qemu源码，从而观察一下cpu在虚拟地址空间访问一个虚拟地址是如何查找tlb以及页表的，那么我应该怎么做，是不是需要两个gdb？
```

**大模型回答要点：**
```
[记录大模型给出的关键建议]
```

**实际帮助：**
```
[说明这个回答如何帮助你理解了调试流程]
```

### 9.3 问题2：编译带调试信息的QEMU

**提问内容：**
```
[记录你的具体提问]
```

**大模型回答要点：**
```
[记录关键回答]
```

**实际操作：**
```
[说明你是如何根据建议实际操作的]
```

### 9.4 问题3：三个终端的执行顺序

**提问内容：**
```
目前我有了三个终端，但是我没有理解，这三个终端分别是做什么用的，我应该怎么执行这个流程，先执行哪一个，这是在做什么，给我解释的详细一点
```

**大模型回答：**
```
[记录大模型的详细解释]
```

**理解和收获：**
```
[说明通过这个解答你理解了什么]
```

### 9.5 问题4：查找QEMU中地址翻译相关代码

**提问内容：**
```
[记录提问]
```

**大模型回答：**
```
[记录回答]
```

**验证结果：**
```
[说明是否在源码中找到了对应的代码]
```

### 9.6 问题5：条件断点的使用

**遇到的问题：**
```
[描述在调试中遇到的问题，如某个函数被频繁调用]
```

**向大模型提问：**
```
[记录你的提问]
```

**大模型建议：**
```
[记录大模型关于条件断点的建议]
```

**应用效果：**
```
[说明使用条件断点后的效果]
```

### 9.7 问题6：遇到SIGTTOU信号

**问题描述：**
```
在GDB附加QEMU时收到SIGTTOU信号，导致调试中断
```

**向大模型提问：**
```
[你是如何描述这个问题的]
```

**大模型解决方案：**
```
[大模型给出的解决方法]
```

**实际解决：**
```
[记录最终如何解决的]
```

### 9.8 其他问题记录

**问题N：**
```
[记录其他向大模型提问的问题]
```

### 9.9 大模型使用总结

**有效的提问方式：**
```
[总结什么样的提问能得到更好的答案]
```

**大模型的优势：**
```
[总结大模型在本次实验中发挥的作用]
```

**局限性：**
```
[记录大模型回答不准确或需要人工验证的地方]
```

**学习收获：**
```
[通过使用大模型辅助学习，你获得了什么]
```

## 十、实验总结

### 10.1 实验成果

通过本次实验，我成功地：

1. [列出完成的目标]
2. [...]

### 10.2 技术收获

**对地址翻译的理解：**
```
[描述对虚拟地址翻译过程的深入理解]
```

**对QEMU的认识：**
```
[描述对模拟器实现原理的认识]
```

**调试技能提升：**
```
[描述掌握的新调试技巧]
```

### 10.3 遇到的困难和解决

**困难1：**
```
[描述遇到的第一个主要困难]
```

**解决过程：**
```
[如何解决的]
```

**困难2：**
```
[其他困难]
```

### 10.4 改进建议

对于实验流程的建议：
```
[提出你的建议]
```

### 10.5 个人感悟

```
[写下你对本次实验的感悟和思考]
```

## 附录

### 附录A：关键代码位置速查

```
QEMU源码关键文件：
- /root/build/qemu-4.1.1/target/riscv/cpu_helper.c
  - get_physical_address: 行155-353（页表遍历）
  - riscv_cpu_tlb_fill: 行435-495（TLB填充）

- /root/build/qemu-4.1.1/accel/tcg/cputlb.c
  - victim_tlb_hit: 行977-1011（软件TLB查询）
  - tlb_set_page: [行号]（TLB设置）

- /root/build/qemu-4.1.1/target/riscv/cpu.h
  - RISC-V CPU结构体定义

- /root/build/qemu-4.1.1/target/riscv/cpu_bits.h
  - PTE标志位定义
  - SATP寄存器字段定义
```

### 附录B：常用GDB命令

```
调试QEMU（x86 GDB）：
- attach <pid>: 附加到进程
- break <function>: 设置断点
- break <function> if <condition>: 条件断点
- continue: 继续执行
- step: 单步进入
- next: 单步跳过
- finish: 执行完当前函数
- info args: 查看函数参数
- print <var>: 打印变量
- print /x <var>: 十六进制打印
- backtrace: 查看调用栈
- info threads: 查看线程
- handle <signal> nostop noprint: 忽略信号

调试ucore（riscv64 GDB）：
- file <kernel>: 加载符号文件
- target remote <host:port>: 连接远程目标
- info registers: 查看寄存器
- x/i $pc: 查看当前指令
- stepi: 单步执行一条指令
- nexti: 单步跳过函数调用
```

### 附录C：RISC-V SV39分页模式

```
SV39分页模式特点：
- 虚拟地址宽度：39位
- 物理地址宽度：56位
- 页大小：4KB (0x1000)
- 页表级数：3级
- 每级页表索引：9位
- 页内偏移：12位

虚拟地址结构（39位）：
[38:30] - VPN[2] (9位) - 一级页表索引
[29:21] - VPN[1] (9位) - 二级页表索引
[20:12] - VPN[0] (9位) - 三级页表索引
[11:0]  - Offset (12位) - 页内偏移

PTE标志位：
- V (bit 0): 有效位
- R (bit 1): 可读
- W (bit 2): 可写
- X (bit 3): 可执行
- U (bit 4): 用户模式可访问
- G (bit 5): 全局映射
- A (bit 6): 访问位
- D (bit 7): 脏位
[53:10] - PPN (物理页号)
```

### 附录D：应对编译优化的调试技巧

**问题：** QEMU使用编译优化后，很多变量显示 `<optimized out>`

**解决方案：**

1. **关注函数边界而非内部状态**
   ```gdb
   # 在函数入口观察参数
   (gdb) break get_physical_address
   (gdb) info args
   
   # 在函数出口观察返回值
   (gdb) break cpu_helper.c:349  # return语句之前
   (gdb) print /x *physical
   (gdb) print *prot
   ```

2. **使用条件断点减少干扰**
   ```gdb
   # 只在S模式下断点
   (gdb) break get_physical_address if mmu_idx == 1
   
   # 只在特定地址范围断点
   (gdb) break get_physical_address if (addr & 0xFFFFFFFF00000000) == 0xFFFFFFFFC0000000
   ```

3. **汇编级调试**
   ```gdb
   (gdb) disassemble get_physical_address
   (gdb) break *0x... # 在特定指令地址设断点
   (gdb) ni           # 单步执行指令
   (gdb) info registers
   ```

4. **启用QEMU日志**
   ```bash
   # 查看MMU操作日志
   make debug QEMUOPTS="-d mmu -D qemu.log"
   
   # 查看更详细的日志
   make debug QEMUOPTS="-d mmu,cpu,int -D qemu.log"
   
   # 在另一个终端查看日志
   tail -f qemu.log
   ```

5. **手动计算验证**
   ```gdb
   # 如果satp = 0x8000000000080205
   (gdb) print /x (0x8000000000080205 & 0xFFFFFFFFFFF) << 12
   # 得到页表基址
   
   # 如果虚拟地址 = 0xFFFFFFFFC0200000
   (gdb) print /x (0xFFFFFFFFC0200000 >> 30) & 0x1FF  # VPN[2]
   (gdb) print /x (0xFFFFFFFFC0200000 >> 21) & 0x1FF  # VPN[1]
   (gdb) print /x (0xFFFFFFFFC0200000 >> 12) & 0x1FF  # VPN[0]
   ```

6. **重新编译QEMU（不推荐，性能很差）**
   ```bash
   cd /root/build/qemu-4.1.1
   ./configure --enable-debug --disable-optimizations
   make -j$(nproc)
   ```

**调试策略建议：**
- 对于页表遍历算法，**理解原理**比观察每一步更重要
- 使用**多个断点**在关键点采样，而非单步跟踪
- **日志文件**可以提供完整的执行历史
- **源码分析**与GDB调试相结合，互相验证

---

**实验完成时间：** [填写日期]

**实验者：** [填写姓名]
