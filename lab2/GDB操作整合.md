终端1：
```
make debug
```

终端2：
```
pgrep -f qemu-system-riscv64
sudo gdb
(gdb) attach <PID>
(gdb) handle SIGPIPE nostop noprint
```

保持运行：
```
(gdb) continue
```
终端3：
```
riscv64-unknown-elf-gdb -q \
  -ex 'file bin/kernel' \
  -ex 'set arch riscv:rv64' \
  -ex 'target remote localhost:1234'
```
处理时序问题：
```
(gdb) set remotetimeout unlimited
```

在终端3的GDB中设置断点并继续执行：
```
(gdb) break kern_init
(gdb) continue
```

查看当前位置指令，找到访存指令：
```
(gdb) x/8i $pc
```

单步执行直到访存指令的前一条：
```
(gdb) si
```

在终端2中，ctrl+c暂时中断，打上断点然后继续：
```
(gdb) break get_physical_address
(gdb) continue
```

在终端3中继续执行这条访存指令：
```
(gdb) si
```

在终端2查看当前地址：
```
(gdb) p/x addr
```
(这里可多次执行`continue`然后执行`p/x addr`观察地址变化)

回到终端3中，查看栈地址：
```
(gdb) info registers sp
```
可以看到刚好与上面的addr相差8字节。

