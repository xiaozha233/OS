# GDB调试QEMU页表查询过程实验报告

## 实验目的

1. 通过调试QEMU源码，深入理解虚拟地址到物理地址转换的完整过程
2. 观察软件模拟的MMU如何实现TLB查询和页表遍历
3. 掌握使用大模型辅助学习和调试复杂项目的能力
4. 理解软件模拟硬件的实现原理

## 实验环境

- 操作系统：Ubuntu 22.04 LTS x86-64
- QEMU版本：4.1.1
- GDB版本：GNU gdb (Ubuntu 12.1-0ubuntu1~22.04.2) 12.1
- RISC-V GCC版本：riscv64-unknown-elf-gcc

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

### 5.2 终端2：查找QEMU进程PID

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

**终端2输出：**
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

在终端2执行 `continue` 让QEMU继续运行：

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
进入GDB后，解决时序同步问题：
```
(gdb) set remotetimeout unlimited
```

### 5.6 终端3：在ucore中设置断点并执行

在终端3的riscv64-gdb中执行：

```gdb
(gdb) break kern_init
Breakpoint 1 at 0xffffffffc02000d8: file kern/init/init.c, line 30.
(gdb) c
Continuing.

Breakpoint 1, kern_init () at kern/init/init.c:30
30          memset(edata, 0, end - edata);
```


**说明：** 此时ucore开始执行，并在 `kern_init` 函数处停下。

### 5.7 终端3：查看当前指令和地址
在终端3的riscv64-gdb中执行：
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
可以看到这里的一条访存指令 `sd ra,8(sp)`，接下来我们将单步执行到这条指令。
### 5.8 终端3：单步执行并观察访存指令

```gdb
(gdb) stepi
(gdb) info registers pc
```

重复执行上面指令直到PC指向访存指令 `sd ra,8(sp)`：
```
pc             0xffffffffc02000ee       0xffffffffc02000ee <kern_init+22>
```

在终端2中，ctrl+C中断QEMU的执行，打上断点，然后继续执行：

```gdb
(gdb) break get_physical_address
Breakpoint 1 at 0x5a0d288ac460: file /root/build/qemu-4.1.1/target/riscv/cpu_helper.c, line 158.
(gdb) continue
```
此时，在终端3执行 `stepi` 访存指令，QEMU会触发页表遍历，命中断点。我们可以在终端2看到：

```
Thread 1 "qemu-system-ris" hit Breakpoint 1, get_physical_address (env=0x5a0d2afc7720, physical=physical@entry=0x7ffcd6c14e60, prot=prot@entry=0x7ffcd6c14e5c, addr=addr@entry=18446744072637906944, access_type=access_type@entry=0, mmu_idx=1) at /root/build/qemu-4.1.1/target/riscv/cpu_helper.c:158
```
表明get_physical_address断点被命中，根据输入`bt`得到函数调用栈分析，此时应该正在执行riscv_cpu_get_phys_page_debug函数。
接下来，在终端2输入`p/x addr`查看传入的虚拟地址：
```
(gdb) p/x addr
$3 = 0xffffffffc0204ff8
```
再次多次输入continue直到终端3中的si指令执行完毕，返回到ucore的kern_init函数。
可以在终端3中查看栈地址：
```
(gdb) info registers sp
sp             0xffffffffc0204ff0       0xffffffffc0204ff0
```
可以发现，栈地址 `0xffffffffc0204ff0` 刚好与前面的addr = ''`0xffffffffc0204ff8` 相差了 `0x4ff0 - 0x4ff8 = -0x8`，符合栈的生长方向，数值8正是访存指令 `sd ra,8(sp)` 中的偏移量。
### 5.9 终端2：观察QEMU中的地址翻译过程
我们在终端2执行`info args`查看待执行的get_physical_address的参数：
```
(gdb) info args
env = 0x5a0d2afc7720
physical = 0x7ffcd6c14e60
prot = 0x7ffcd6c14e5c
addr = 18446744072637906944
access_type = 0
mmu_idx = 1
```
可通过`list`命令查看当前函数的源码：
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
```
接下来我们可以单步进入get_physical_address函数，观察地址翻译的具体过程：
```
(gdb) step
165         if (mode == PRV_M && access_type != MMU_INST_FETCH) {
```
此时我们就进入了get_physical_address函数，可以继续单步执行，观察页表遍历的每一步。
我们先查看从satp寄存器获取页表基址：
```
(gdb) print /x env->satp
$2 = 0x8000000000080205
```
接下来我们继续单步执行：
```
(gdb) step
171         if (mode == PRV_M || !riscv_feature(env, RISCV_FEATURE_MMU)) {
(gdb) step
riscv_feature (env=0x5cd4bd7c22a0, feature=0) at /root/build/qemu-4.1.1/target/riscv/cpu.h:243
243         return env->features & (1ULL << feature);
(gdb) step
244     }
(gdb) step
get_physical_address (env=0x5cd4bd7c22a0, physical=0x7fff08ceb208, prot=0x7fff08ceb200, addr=18446744072637906944, access_type=0, mmu_idx=1) at /root/build/qemu-4.1.1/target/riscv/cpu_helper.c:177
177         *prot = 0;
(gdb) step
181         int mxr = get_field(env->mstatus, MSTATUS_MXR);
(gdb) step
183         if (env->priv_ver >= PRIV_VERSION_1_10_0) {
(gdb) step
184             base = get_field(env->satp, SATP_PPN) << PGSHIFT;
(gdb) step
185             sum = get_field(env->mstatus, MSTATUS_SUM);
(gdb) step
186             vm = get_field(env->satp, SATP_MODE);
(gdb) step
187             switch (vm) {
(gdb) step
191               levels = 3; ptidxbits = 9; ptesize = 8; break;
(gdb) step
223         CPUState *cs = env_cpu(env);
```
查看base的值：
```
(gdb) print /x base
$3 = <optimized out>
```
`base` 变量显示为 `<optimized out>`，这是因为QEMU编译时使用了优化选项（-O2或-O3），导致局部变量被优化掉

为此，我们重新编译了QEMU，添加了 `-O0` 选项，重新运行调试，观察到：
```
(gdb) print /x base
$3 = 0x80205000
```
这符合预期，因为 `satp = 0x8000000000080205`，PPN部分是 `0x80205`，因此 `base = 0x80205000`。
接下来我们观察分页模式：
```
(gdb) print vm
$4 = 8
(gdb) print levels
$5 = 3
(gdb) print ptidxbits
$6 = 9
```
`vm = 8` 对应 SV39 分页模式，`levels = 3`，`ptidxbits = 9`，符合RISC-V的SV39规范。

### 5.10 完整复现：从环境启动到页表遍历

首先执行5.9之前的步骤直到触发get_physical_address断点，然后继续使用next单步执行，观察页表遍历的每一步。
**步骤 1：单步追踪第一级页表 (L2) 并发现大页**

*   **操作终端**：终端2
*   **命令**：
    ```gdb
    (gdb) next
    ```
    执行，直到第242行：
    ```
    242             target_ulong pte_addr = base + idx * ptesize;
(gdb) p/x idx
$12 = 0x1ff
    ```
    继续执行直到第254行：
    ```
    254             target_ulong ppn = pte >> PTE_PPN_SHIFT;
(gdb) p/x pte
    ```
*   **输出与分析**：
    *   `idx = 0x1ff` (VPN[2])
    *   `pte_addr = 0x80205ff8` (根页表基址 + 偏移)
    *   `pte = 0x200000cf`
        *   低 8 位是 `0xcf` -> 二进制 `1100 1111`。
        *   对应标志位：**V=1, R=1, W=1, X=1**, U=0, G=0, A=1, D=1。

    **关键发现：**
    在页表遍历过程中，如果遇到 **R, W, X 中任意一位为 1** 的 PTE，硬件（或 QEMU）就会认为这是一个 **叶子节点（Leaf Entry）**，即物理页面的映射，而不是指向下一级页表的指针。
    
    此处 `pte` 的 R/W/X 均为 1，说明这是一个 **1GB 的大页（Gigapage）**。QEMU 检测到叶子节点后，不再进入下一轮循环，而是直接计算物理地址。

**步骤 2：验证物理地址计算**

*   **操作终端**：终端2
*   **命令**：
    ```gdb
    (gdb) next
    ... (跳出循环，进入物理地址计算逻辑)
    (gdb) p/x *physical
    ```
*   **预期输出**：`0x80204ff8`
*   **结论**：
    *   由于是大页映射，物理地址计算公式为：`Physical Address = (PPN << 12) | (Virtual Address & Offset_Mask)`
    *   对于 1GB 大页，Offset 是虚拟地址的低 30 位。
    *   `VA = 0xffffffffc0204ff8`
    *   `Offset = 0x00204ff8`
    *   `PPN = 0x80000` (来自 PTE `0x200000cf` >> 10)
    *   `Base PA = 0x80000 << 12 = 0x80000000`
    *   `Calculated PA = 0x80000000 + 0x00204ff8 = 0x80204ff8`
    *   验证成功！这证明了 ucore 在启动阶段使用了 1GB 大页映射。

---

#### 第五阶段：验证TLB缓存（终端2 & 3）

验证TLB是否生效：第二次访问同一地址不应再触发页表遍历。

**步骤 3：恢复QEMU执行**

*   **操作终端**：终端2
*   **命令**：
    ```gdb
    (gdb) continue
    ```
*   **现象**：QEMU继续运行。

**步骤 4：再次触发访存**

*   **操作终端**：终端3
*   **命令**：
    ```gdb
    (gdb) x/gx $sp+8
    ```
*   **现象**：终端3直接显示内存内容 `0xffffffffc0204ff8: ...`。

**步骤 5：确认未命中断点**

*   **操作终端**：终端2
*   **现象**：终端2 **保持运行状态，没有命中断点**。
*   **结论**：第二次访问直接命中了TLB缓存，跳过了 `get_physical_address` 的执行，验证了TLB机制的有效性。


## 六、从调试记录中还原的完整调用路径与关键分支

本节将 5.10~5.15 的观测结果整理为“可写进报告的结论”，并明确哪些来自调试现场、哪些来自源码对照。

### 6.1 关键调用路径（两条：正常访存路径 vs GDB调试读路径）

#### 6.1.1 正常访存路径（Guest执行load/store/取指时）

> 这条路径对应“CPU执行访存指令 → QEMU TCG 执行 helper → 查TLB → miss则填充”。


1. Guest执行一条访存（load/store/取指）。
2. QEMU TCG 进入相应的访存帮助函数（读/写/取指）。
3. 主TLB查询：比较 `page` 与 `CPUTLBEntry` 中对应的 tag（如 `addr_read/addr_write/addr_code`）。
4. 主TLB miss → `victim_tlb_hit()`（查询 victim TLB，命中则交换回主TLB）。
5. victim TLB miss → `tlb_fill()`。
6. `tlb_fill()` 调用架构回调：`riscv_cpu_tlb_fill()`。
7. `riscv_cpu_tlb_fill()` 调用：`get_physical_address()` 进行页表遍历。
8. 遍历成功后：`tlb_set_page()` 将映射写入TLB（缓存）。
9. 回到访存路径，使用PA完成本次内存访问。

#### 6.1.2 GDB调试读路径（终端3用 x/gx 读内存触发）

> 本实验 5.11 主要采用这条路径作为“可控触发器”。

1. 终端3（riscv64-gdb）对 Guest 虚拟地址执行内存读取（例如 `x/gx $sp+8`）。
2. QEMU的GDB stub 处理读请求，需要把 Guest VA 翻译为PA。
3. QEMU进入 `riscv_cpu_get_phys_page_debug()`（用于调试读写的页翻译入口）。
4. 进一步调用 `get_physical_address()`，完成页表遍历，返回PA。
5. QEMU用得到的PA从Guest物理内存读取数据，再通过GDB协议返回给终端3。

### 6.2 关键分支语句（源码对照 + 调试意义）

#### 6.2.1 `get_physical_address` 的关键分支

1. **是否需要翻译（M模式/无MMU快速路径）**

```c
if (mode == PRV_M || !riscv_feature(env, RISCV_FEATURE_MMU)) {
        *physical = addr;
        *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return TRANSLATE_SUCCESS;
}
```

意义：

- 若处于 M 模式或未启用MMU，则虚拟地址按物理地址处理，直接返回；
- 这是“未开启虚拟地址空间时”的关键对比点。

2. **PTE有效性与是否为叶子**

```c
if (!(pte & PTE_V)) {
        return TRANSLATE_FAIL;
} else if (!(pte & (PTE_R | PTE_W | PTE_X))) {
        base = ppn << PGSHIFT;   // 非叶子：继续下一级
} else {
        // 叶子：继续权限检查与PA计算
}
```

意义：

- `V=0` 直接失败；
- `R=W=X=0` 表示“页目录项/中间项”，继续走下一层；
- **关键点**：若 `R/W/X` 中任意一位为 1，则表示这是**叶子节点**（Leaf Entry）。
    - 如果这是在 L2 或 L1 层遇到叶子节点，说明这是**大页（Superpage/Gigapage）**。
    - 本次实验中，我们在 L2 层（第一轮循环）就遇到了 `RWX=1` 的 PTE，因此 QEMU 识别为 1GB 大页，直接终止遍历并计算物理地址。

3. **权限检查（读/写/执行/U位等）**

意义：

- 对应硬件MMU的权限判断逻辑；
- 翻译失败会导致 QEMU 抛出页异常（在 `riscv_cpu_tlb_fill` 中调用 `raise_mmu_exception`）。

#### 6.2.2 `victim_tlb_hit` 的关键分支

```c
if (cmp == page) {
        // 命中：交换victim与主TLB表项
        return true;
}
return false;
```

意义：

- 主TLB miss 后，QEMU会尝试在 victim TLB 中找“刚被替换出去”的映射；
- 命中则通过交换把映射放回主TLB，提高局部性。

## 八、实验要求完成情况

### 8.1 关键调用路径

根据调试（终端2断点 + 终端3触发）与源码对照（第六节），关键调用路径应区分为两条：

#### 8.1.1 路径A：正常访存（Guest执行load/store/取指）

这条路径用于满足 `guide` “先查TLB，miss后查页表”的要求。

```
Guest访存/取指
→ QEMU TCG 访存路径
→ 主TLB查找（tag比较，命中则直接使用缓存结果）
→ miss → victim_tlb_hit()        // 二级victim TLB
→ miss → tlb_fill()
→ riscv_cpu_tlb_fill()
→ get_physical_address()         // 页表遍历
→ tlb_set_page()                 // 回填TLB
→ 完成访存
```

#### 8.1.2 路径B：调试读（终端3读取内存触发，作为稳定触发器）

本实验为了保证可复现性，优先使用终端3的 `x/gx <VA>` 来触发一次地址翻译。

```
终端3: x/gx <Guest VA>
→ QEMU GDB stub 处理读请求
→ riscv_cpu_get_phys_page_debug()
→ get_physical_address()         // 页表遍历
→ 读取Guest物理内存
→ 返回给终端3
```

**终端侧证据（如何记录）：**

当终端2命中 `get_physical_address` 断点后执行：

```gdb
(gdb) bt
```

将看到调用栈顶部包含 `get_physical_address`，并向上追溯到 `riscv_cpu_get_phys_page_debug` 或 `riscv_cpu_tlb_fill`（分别对应路径B/路径A）。

**关键分支语句分析：**

（以下代码片段来自 QEMU 4.1.1 源码，配合终端2单步/观察即可验证。）

```c
// 1) 是否需要地址翻译（M模式/未启用MMU快速路径）
if (mode == PRV_M || !riscv_feature(env, RISCV_FEATURE_MMU)) {
    *physical = addr;
    *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
    return TRANSLATE_SUCCESS;
}

// 2) 遍历中PTE的三类分支：无效 / 非叶子 / 叶子
if (!(pte & PTE_V)) {
    return TRANSLATE_FAIL;               // 无效PTE：页故障
} else if (!(pte & (PTE_R | PTE_W | PTE_X))) {
    base = ppn << PGSHIFT;               // 非叶子：继续下一级
} else {
    // 叶子：进入权限检查、A/D位更新、PA计算
}

// 3) victim TLB 命中：交换表项回主TLB
if (cmp == page) {
    return true;
}
```

**这些分支对应的调试意义：**

1. 通过在终端2观察 `mmu_idx`/`env->satp` 与单步执行，可以确认当前属于“M模式快速路径”还是“SV39页表遍历路径”。
2. 通过观察 `pte` 的 R/W/X/V 位，可判断当前级别是“中间页表项”还是“叶子页表项”。
3. 通过对同一地址进行两次访问（见 5.15），可以看到第一次会进入“TLB miss → fill → walk”，第二次通常不会再进入 fill/walk，证明TLB缓存效应。

### 8.2 单步调试页表翻译的详细流程

以访问虚拟地址 `0xffffffffc0204ff8` 为例（本次实验记录）。

**步骤1：进入get_physical_address**

1. 终端3（riscv64-gdb）停在 `kern_init`，并触发一次对 `sp+8` 的读取：

    ```gdb
    (gdb) info registers sp
    (gdb) x/gx $sp+8
    ```

2. 终端2（x86-64 gdb）命中 `get_physical_address` 后记录参数：

    ```gdb
    (gdb) info args
    (gdb) p/x addr
    (gdb) p mmu_idx
    (gdb) p/x env->satp
    ```

本次实验记录到：

- `addr = 0xffffffffc0204ff8`
- `mmu_idx = 1`（S模式视角）
- `env->satp = 0x8000000000080205`（SV39）

**步骤2：获取页表基址**

终端2单步执行到 satp 解析处，可得到：

- `PPN = 0x80205`
- `base = PPN << 12 = 0x80205000`

终端1同时有内核自打印作为交叉验证：

```
satp physical address: 0x0000000080205000
```

**步骤3：第一级页表查询与大页发现**

SV39 三级页表，第一步查 L2 表（使用 `VPN[2]`）：

- `VPN[2] = 0x1ff`
- `idx = 0x1ff`
- `pte_addr(L2, PA) = 0x80205000 + 0x1ff * 8 = 0x80205ff8`

终端2在循环内观察 `pte`：

- `pte = 0x200000cf`
- **关键发现**：`pte` 的低 8 位为 `0xcf` (1100 1111)，即 `V=1, R=1, W=1, X=1`。
- 根据 RISC-V 规范，`R/W/X` 任意一位为 1 即表示**叶子节点**。
- 由于这是在 L2 层（第一级）遇到的叶子节点，说明这是一个 **1GB 的大页（Gigapage）**。
- QEMU 识别到叶子节点后，**不再进行后续的 L1、L0 查询**，而是直接跳出循环计算物理地址。

**步骤4：计算最终物理地址**

最终物理地址计算如下：

1. **按QEMU页表遍历逻辑（终端2观察/打印 `*physical`）**

终端2在函数返回前执行：

```gdb
(gdb) finish
(gdb) p/x *physical
(gdb) p *prot
```

2. **按大页映射逻辑验证**

- 1GB 大页的页内偏移为虚拟地址的低 30 位：`0x00204ff8`。
- 物理基址来自 PTE 的 PPN：`0x80000 << 12 = 0x80000000`。
- `PA = 0x80000000 + 0x00204ff8 = 0x80204ff8`。

3. **按内核直映偏移交叉验证**

`kern/mm/memlayout.h` 定义：

- `PHYSICAL_MEMORY_OFFSET = 0xffffffff40000000`

因此：

- `PA = VA - PHYSICAL_MEMORY_OFFSET = 0x80204ff8`

三种方式得到一致结果，证明翻译正确，且验证了 ucore 启动阶段使用了 1GB 大页映射。

### 8.3 TLB查找代码分析

**QEMU中TLB查找的核心代码位置：**

```
文件：/root/build/qemu-4.1.1/accel/tcg/cputlb.c
函数：victim_tlb_hit（victim TLB 查询与交换逻辑）
```

**TLB查找流程：**

```
1) 计算页对齐地址：
    page = address & TARGET_PAGE_MASK

2) 计算主TLB索引index（具体实现为QEMU内部哈希/取位）：
    index = f(page)

3) 主TLB查找：
    - 选择addr_read/addr_write/addr_code之一作为tag
    - 比较tag是否等于page
    - 相等则命中，直接使用缓存结果

4) 主TLB miss：
    - 进入 victim_tlb_hit(env, mmu_idx, index, elt_ofs, page)
    - 遍历victim TLB，若cmp==page则交换回主TLB并返回true

5) victim TLB miss：
    - 进入 tlb_fill → riscv_cpu_tlb_fill
    - 调用 get_physical_address 进行页表遍历
    - 成功后 tlb_set_page 回填TLB
```

**调试记录：**

设置断点：
```gdb
(gdb) break victim_tlb_hit
```

观察到的调用：

按 5.15 的“双次读取同一地址”即可得到清晰的对比证据：

1. 终端3第一次执行 `x/gx $sp+8`：
    - 终端2命中 `victim_tlb_hit`：说明主TLB miss，开始查victim。
    - 随后命中 `riscv_cpu_tlb_fill` 与 `get_physical_address`：说明victim也miss，需要页表遍历并回填。

2. 终端3第二次执行 `x/gx $sp+8`：
    - 终端2通常不再命中 `riscv_cpu_tlb_fill/get_physical_address`：说明主TLB已命中，无需再次遍历页表。

### 8.4 QEMU模拟TLB与真实TLB的区别

根据调试观察和代码分析：

**相同点：**

1. 都缓存“虚拟页号 → 物理页号/页帧号”的映射关系，降低页表遍历频率。
2. 都与权限检查相关：TLB条目携带（显式或隐式）R/W/X/U等权限信息。
3. 都具备“miss → 触发填充”的机制：硬件由硬件页表遍历器填充；QEMU由软件函数链路填充。

**不同点：**

1. **软件数据结构 vs 专用硬件**
    - QEMU：TLB是 `CPUTLBEntry` 数组与相关元数据，查找/更新/交换都在C代码中完成。
    - 真实硬件：TLB通常使用并行比较/缓存电路，命中延迟极低。

2. **QEMU特有的victim TLB交换机制**
    - QEMU在 `victim_tlb_hit()` 中实现“命中则与主TLB交换表项”。
    - 真实CPU未必存在完全同构的“victim TLB + swap”机制。

3. **未开启分页/处于M模式时的差异（关键对比点）**
    - 真实RISC-V：在M模式或未启用分页时，VA按物理地址处理，硬件TLB可能被旁路。
    - QEMU：即便 `get_physical_address` 走“快速路径”（直接 `*physical=addr`），为了加速模拟，QEMU仍可能维护软件TLB/IO TLB 等缓存结构。

4. **缓存内容的侧重点不同**
    - 硬件TLB缓存的是硬件需要的PFN/权限等信息。
    - QEMU的TLB条目还会包含“如何在宿主机高效访问”的信息（例如IO TLB、host相关偏移），这是模拟器的额外需求。

**未开启虚拟地址空间的访存调试记录：**

本报告给出一个可复现实验，用于对比“未开启分页/处于M模式”场景：

1. 终端1：`make debug`（QEMU暂停等待gdb）。
2. 终端2：attach到QEMU后设置条件断点并继续：

    ```gdb
    (gdb) break get_physical_address if mmu_idx == 3
    (gdb) continue
    ```

3. 终端2预期观察到：
    - 很快命中断点；
    - `info args` 显示 `mmu_idx = 3 (PRV_M)`，`addr` 往往处于 `0x80000000` 附近；
    - 单步可验证进入快速路径并直接返回：`*physical = addr`。

4. 从结果可以分析：
    - 在M模式/未启用MMU时，QEMU不会执行SV39三级页表遍历；
    - 与本实验主体（S模式、SV39、多级页表）形成明确对比。

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

**发现5：大页映射导致的“消失的循环”**

在调试 `get_physical_address` 时，我原本预期会看到教科书式的“三级跳”：L2 -> L1 -> L0。然而，在单步调试时，我发现代码在第一轮循环（L2）后就直接跳出了！

起初我以为是 QEMU 的 bug 或者是我的 GDB 操作失误。但在仔细检查 `pte` 的值 `0x200000cf` 后，我发现其低 8 位 `0xcf` 对应的标志位是 `VRWXAD` 全为 1。

查阅 RISC-V 手册后恍然大悟：**只要 R/W/X 任意一位为 1，这就是叶子节点！** 这意味着 ucore 在启动阶段并没有建立精细的 4KB 页表，而是直接用一个 **1GB 的大页（Gigapage）** 映射了整个内核空间。

这一发现让我深刻理解了软硬件协同工作的本质：硬件（QEMU）不会凭空产生多级遍历，它只是忠实地执行软件（ucore）设定的页表规则。

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

- 模型名称：GPT-5.2
- 使用方式：在 VS Code 编辑器中通过 GitHub Copilot Chat 辅助阅读源码、设计调试流程、整理“可复现步骤 + 预期输出 + 结论推导”。

### 9.2 问题1：如何调试QEMU源码

**提问内容：**
```
我需要调试一个正在运行我的操作系统内核源码的qemu源码，从而观察一下cpu在虚拟地址空间访问一个虚拟地址是如何查找tlb以及页表的，那么我应该怎么做，是不是需要两个gdb？
```

**大模型回答要点：**

1. 需要“两个gdb + 三个终端”的分层调试：
    - 终端1：启动QEMU（带 `-s -S`），让它暂停等待调试器。
    - 终端2：用宿主机 `gdb` attach 到 QEMU 进程，调试 QEMU 源码（x86-64）。
    - 终端3：用 `riscv64-unknown-elf-gdb` 连接到 QEMU 的 gdb stub，调试 Guest 内核（RISC-V）。
2. 两个gdb的职责必须严格区分：
    - 终端2的gdb看的是“模拟器如何实现MMU/TLB/页表遍历”；
    - 终端3的gdb看的是“Guest正在执行到哪条指令、虚拟地址是什么”。
3. 先让终端2 attach 并 `continue`，再让终端3连接 stub，避免连接时序问题。

**实际帮助：**

- 明确了“观察硬件机制（TLB/页表遍历）必须去看QEMU源码”的关键点，因此终端2必须调QEMU本体。
- 形成了后续第5节的三终端流程骨架，并在 5.10~5.15 扩展为可复现的“断点组 + 可控触发器”方案。

### 9.3 问题2：编译带调试信息的QEMU

**提问内容：**

“系统里已经 `make install` 过 QEMU 4.1.1，但没有调试信息；我该怎么重新编译一个可调试的QEMU？是否必须 `make install`？如何避免污染系统QEMU？”

**大模型回答要点：**

1. 在QEMU源码目录重新 `configure` 时开启调试选项，例如 `--enable-debug`，必要时关闭优化（如 `--disable-optimizations`），以避免 `<optimized out>`。
2. 编译完成后**不需要** `make install`：直接使用构建目录下的 `riscv64-softmmu/qemu-system-riscv64` 即可。
3. 在 ucore 的 `Makefile` 中通过 `QEMU := /path/to/debug/qemu-system-riscv64` 指向调试版QEMU，从而“系统QEMU”和“调试QEMU”并存。

**实际操作：**

1. 使用 `/root/build/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64` 作为调试版QEMU。
2. 修改本项目 `Makefile` 中 `QEMU := ...` 指向该调试版QEMU（见第2节）。
3. 在需要观察局部变量（如 `base/idx/pte_addr/pte`）时，进一步使用关闭优化的QEMU构建，以保证终端2单步时局部变量可见（见 5.14）。

### 9.4 问题3：三个终端的执行顺序

**提问内容：**
```
目前我有了三个终端，但是我没有理解，这三个终端分别是做什么用的，我应该怎么执行这个流程，先执行哪一个，这是在做什么，给我解释的详细一点
```

**大模型回答：**

1. 终端1负责“跑起来并卡住”：`make debug` 启动QEMU并 `-S` 暂停。
2. 终端2负责“让模拟器继续”：attach QEMU →（设置断点）→ `continue`。
3. 终端3负责“控制Guest”：连接 `target remote :1234` → 在Guest内核打断点/单步。
4. 若终端3先连，而终端2没 `continue`，会出现连接/时序问题；因此必须先终端2放行，再终端3连接并控制Guest。

**理解和收获：**

- 三终端不是“冗余”，而是把“模拟器层”和“被模拟系统层”拆开：
    - 终端3决定“Guest此刻要访问哪个虚拟地址”；
    - 终端2决定“QEMU用什么路径把VA翻译成PA”。
- 这也是后续 5.11 采用“终端3可控触发器（x/gx 读内存）”的理由：稳定生成同一个VA访问，从而让终端2可重复观察翻译过程。

### 9.5 问题4：查找QEMU中地址翻译相关代码

**提问内容：**

“在QEMU 4.1.1 源码里，RISC-V 的地址翻译（页表遍历）和TLB查找分别在哪里实现？我应该从哪些函数开始下断点？”

**大模型回答：**

1. 页表遍历核心：`target/riscv/cpu_helper.c:get_physical_address`。
2. TLB miss 回填入口：`target/riscv/cpu_helper.c:riscv_cpu_tlb_fill`。
3. TLB（含victim TLB）相关：`accel/tcg/cputlb.c:victim_tlb_hit` 以及同文件的TLB访问路径。
4. 回填TLB的函数：`tlb_set_page`。

**验证结果：**

- 已在 `/root/build/qemu-4.1.1/target/riscv/cpu_helper.c` 找到 `get_physical_address` 与 `riscv_cpu_tlb_fill`，并在终端2命中断点验证其确实参与本次翻译（见 5.8~5.15）。
- 已在 `/root/build/qemu-4.1.1/accel/tcg/cputlb.c` 找到 `victim_tlb_hit`，并将其作为“TLB查找观测点”（见 5.10、5.15、8.3）。

### 9.6 问题5：条件断点的使用

**遇到的问题：**

`get_physical_address`/TLB相关函数被频繁调用，gdb反复停下导致几乎无法推进；同时编译优化可能导致局部变量 `<optimized out>`，进一步增加排查难度。

**向大模型提问：**

“QEMU地址翻译相关函数命中太频繁，如何只在某个特定虚拟地址发生翻译时才停下来？条件断点怎么写？”

**大模型建议：**

1. 使用条件断点过滤：在函数入口按 `addr`（或 `address/page`）精确过滤。
2. 如果条件涉及结构体字段/宏常量，先 `info args`/`p` 确认参数与枚举值，再写条件。
3. 对“极频繁函数”优先断在更下游、触发更少的点（例如只断 `riscv_cpu_tlb_fill` 而不是所有访存helper）。

**应用效果：**

在终端2使用：

```gdb
(gdb) break get_physical_address if addr == 0xffffffffc0204ff8
```

之后断点只在目标地址发生翻译时命中，调试推进效率显著提升；并能稳定复现 5.12~5.15 的观察记录。

### 9.7 问题6：遇到SIGTTOU信号

**问题描述：**
```
在GDB附加QEMU时收到SIGTTOU信号，导致调试中断
```

**向大模型提问：**

“我在用宿主机gdb attach QEMU时，经常被 SIGTTOU/SIGPIPE 打断，导致没法持续运行。如何处理这些信号？”

**大模型解决方案：**

在终端2的gdb里显式设置这些信号为“不停、不打印”，例如：

```gdb
(gdb) handle SIGPIPE nostop noprint
(gdb) handle SIGTTOU nostop noprint
```

**实际解决：**

按上述方式处理后，终端2不再因为这些信号频繁停下，能够连续 `continue/step` 并完成对TLB与页表遍历的观察（见 5.10.1）。

### 9.8 其他问题记录

**问题N：**

**问题描述：**

在执行 `make debug` 时，偶发出现如下报错，导致QEMU无法启动：

```
-s: Failed to find an available port: Address already in use
```

**向大模型提问：**

“`-s` 默认占用 `:1234` 作为 gdb stub 端口。现在提示端口被占用，我应该如何定位是谁占用了该端口，并安全清理残留QEMU进程？”

**大模型建议（可复现操作）：**

1. 查看 1234 端口监听者：
    - `ss -ltnp | grep :1234` 或 `lsof -i :1234`
2. 交叉确认是否是残留的 QEMU：
    - `pgrep -af qemu-system-riscv64`
3. 结束残留进程后重试。

**应用效果：**

按上述方式定位并清理后，`make debug` 能稳定启动，终端3可正常 `target remote :1234`。

### 9.9 大模型使用总结

**有效的提问方式：**

1. 先给“明确目标 + 具体验证点”：例如“我需要在QEMU源码层看到一次VA翻译的调用链，并记录 satp/addr/mmu_idx/physical”。
2. 提供环境与限制：例如 QEMU版本、是否优化编译、不能改代码还是只能下断点、是否允许输出日志。
3. 把问题拆成可执行的小步：例如“先让终端3触发固定VA读；再让终端2只在该VA上断；最后记录 bt + info args”。
4. 附上关键输出片段（如 `env->satp`、断点命中函数名、错误信息），能显著减少来回试错。

**大模型的优势：**

1. 快速给出“该断哪里”的候选点：`victim_tlb_hit`、`riscv_cpu_tlb_fill`、`get_physical_address`。
2. 把复杂流程“改写成可复现脚本”：三终端职责、断点组、条件断点过滤、触发器选择（例如用GDB读栈地址触发翻译）。
3. 对调试噪声给出处理策略：SIGTTOU/SIGPIPE 的忽略方式、端口冲突的定位方式。

**局限性：**

1. QEMU源码行号/函数栈在不同版本或不同构建选项下会变化，必须以本机源码与实际断点命中为准。
2. 面对 `-O2` 导致的 `<optimized out>`，大模型只能给建议（观测输入输出、换构建参数或日志），不能替代真实调试观察。
3. 对性能优化路径（内联/宏展开/TCG细节）容易出现“看似合理但不一定命中”的建议，需要通过断点与 `bt` 反复验证。

**学习收获：**

1. 建立了跨层调试的方法：终端3负责“制造访问”，终端2负责“解释翻译与缓存”，终端1负责“提供内核侧校验”。
2. 把SV39从抽象公式落地到可核对的量：satp→root PPN→base PA→VPN分解→最终PA，并能用直映偏移二次验证。
3. 对QEMU“软件实现MMU/TLB”的组织方式有更具体的认识：TLB查询、victim机制、miss回填、页表遍历之间的关系。

## 十、实验总结

### 10.1 实验成果

通过本次实验，我成功地：

1. 搭建并跑通“三终端 + 双GDB”的调试体系：
    - 终端1启动QEMU并等待调试；
    - 终端2调试QEMU源码（观察TLB与页表遍历）；
    - 终端3调试Guest内核并触发指定VA的访存。
2. 以 `0xffffffffc0204ff8` 为例，完整演示一次 SV39 地址翻译，并记录关键观测值：
    - 终端2：`addr=0xffffffffc0204ff8`、`env->satp=0x8000000000080205`（MODE=8，SV39）；
    - 终端1：`satp physical address = 0x80205000`（交叉验证根页表基址）。
3. 用直映偏移 `0xffffffff40000000` 做交叉验证，得到一致的物理地址结果：
    - $PA = VA - 0xffffffff40000000 = 0x80204ff8$。
4. 定位并说明QEMU TLB查询与miss回填路径（含victim机制），并用“两次读取同一地址”的差异作为TLB hit/miss 的可复现证据。

### 10.2 技术收获

**对地址翻译的理解：**

- 从 `satp.MODE` 入手定位页表模式（本实验为SV39），掌握VPN[2:0] + offset 的位段划分。
- 理解页表遍历的关键判断：PTE的V/R/W/X 位组合决定“非叶子/叶子/非法”，以及权限与A/D位的处理。
- 学会使用“独立校验通道”（内核打印 + 直映偏移推导）来验证翻译结果，避免只相信单一观察点。

**对QEMU的认识：**

- QEMU把“硬件MMU/硬件TLB”的行为抽象为软件实现：TLB miss 时进入 `riscv_cpu_tlb_fill`，再调用 `get_physical_address` 做页表遍历。
- QEMU存在“victim TLB”等实现细节，用于降低替换开销；这与真实CPU的微结构不完全等价，但能在功能正确的前提下提升模拟性能。

**调试技能提升：**

- 熟练使用条件断点对高频函数降噪：只在目标 `addr` 发生翻译时停下。
- 学会用GDB“读内存”作为访存触发器，稳定触发一次地址翻译，从而稳定复现实验记录。
- 形成“先抓输入/输出再深入中间量”的调试策略，能更好应对优化编译带来的变量不可见问题。

### 10.3 遇到的困难和解决

**困难1：**

QEMU优化编译导致局部变量显示为 `<optimized out>`，使得逐级打印 `pte/idx/pte_addr` 的调试体验变差。

**解决过程：**

1. 优先记录“函数入口参数 + 返回结果”形成最小闭环：`addr`、`satp`、`mmu_idx`、`*physical`、`*prot`。
2. 若必须观察中间量，使用更低优化级别的QEMU构建或启用QEMU的MMU日志（例如 `-d mmu`）辅助。
3. 用内核自打印与直映偏移做交叉验证，确保在“中间量不可见”的情况下仍能验证最终结论。

**困难2：**

三终端联动的环境问题：端口 1234 偶尔被残留QEMU占用；以及 attach 后 SIGTTOU/SIGPIPE 等信号干扰调试。

解决方式：

- 用 `ss/lsof/pgrep` 定位并清理残留进程；
- 在终端2使用 `handle SIGTTOU/SIGPIPE nostop noprint` 忽略干扰信号。

### 10.4 改进建议

对于实验流程的建议：

1. 建议在指导书中明确推荐一个“稳定触发翻译”的动作（如终端3 `x/gx $sp+8`），避免同学卡在“断点命中太随机”。
2. 提供一份可选的 gdb 初始化片段：常用断点组、条件断点示例、信号忽略设置、以及常用打印命令模板。
3. 明确说明“优化编译会影响局部变量可见性”，并给出两条完成实验的可行路线：
    - 只观测输入/输出 + 交叉验证（适用于默认构建）；
    - 或提供可选的低优化QEMU构建用于深入观察逐级PTE。

### 10.5 个人感悟

这次实验最直观的收获是把“TLB/页表遍历”从概念变成了可观察、可复现的过程：当终端3触发一次固定VA的访问，终端2在QEMU源码断点处停下并显示 `satp/addr` 时，我能够把每一个抽象名词对应到具体的执行路径与数据。

同时也认识到模拟器与真实硬件存在差异：QEMU为了性能会引入额外的软件缓存结构（例如victim TLB），其行为与真实CPU微结构不完全一致。理解这种差异，才能正确解释调试现象，并避免“把模拟器实现细节当作硬件定律”。

## 附录

### 附录A：关键代码位置速查

```
QEMU源码关键文件：
- /root/build/qemu-4.1.1/target/riscv/cpu_helper.c
  - get_physical_address: 行155-353（页表遍历）
  - riscv_cpu_tlb_fill: 行435-495（TLB填充）

- /root/build/qemu-4.1.1/accel/tcg/cputlb.c
  - victim_tlb_hit: 行977-1011（软件TLB查询）
    - tlb_set_page: 可在终端2用 `info line tlb_set_page` 或在源码中搜索函数名定位（TLB设置）

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

**实验完成时间：** （请根据实际完成日期填写）

**实验者：** （请填写姓名）
