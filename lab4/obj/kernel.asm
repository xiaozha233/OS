
bin/kernel:     file format elf64-littleriscv


Disassembly of section .text:

ffffffffc0200000 <kern_entry>:
    .globl kern_entry
kern_entry:
    # a0: hartid
    # a1: dtb physical address
    # save hartid and dtb address
    la t0, boot_hartid
ffffffffc0200000:	00009297          	auipc	t0,0x9
ffffffffc0200004:	00028293          	mv	t0,t0
    sd a0, 0(t0)
ffffffffc0200008:	00a2b023          	sd	a0,0(t0) # ffffffffc0209000 <boot_hartid>
    la t0, boot_dtb
ffffffffc020000c:	00009297          	auipc	t0,0x9
ffffffffc0200010:	ffc28293          	addi	t0,t0,-4 # ffffffffc0209008 <boot_dtb>
    sd a1, 0(t0)
ffffffffc0200014:	00b2b023          	sd	a1,0(t0)
    
    # t0 := 三级页表的虚拟地址
    lui     t0, %hi(boot_page_table_sv39)
ffffffffc0200018:	c02082b7          	lui	t0,0xc0208
    # t1 := 0xffffffff40000000 即虚实映射偏移量
    li      t1, 0xffffffffc0000000 - 0x80000000
ffffffffc020001c:	ffd0031b          	addiw	t1,zero,-3
ffffffffc0200020:	037a                	slli	t1,t1,0x1e
    # t0 减去虚实映射偏移量 0xffffffff40000000，变为三级页表的物理地址
    sub     t0, t0, t1
ffffffffc0200022:	406282b3          	sub	t0,t0,t1
    # t0 >>= 12，变为三级页表的物理页号
    srli    t0, t0, 12
ffffffffc0200026:	00c2d293          	srli	t0,t0,0xc

    # t1 := 8 << 60，设置 satp 的 MODE 字段为 Sv39
    li      t1, 8 << 60
ffffffffc020002a:	fff0031b          	addiw	t1,zero,-1
ffffffffc020002e:	137e                	slli	t1,t1,0x3f
    # 将刚才计算出的预设三级页表物理页号附加到 satp 中
    or      t0, t0, t1
ffffffffc0200030:	0062e2b3          	or	t0,t0,t1
    # 将算出的 t0(即新的MODE|页表基址物理页号) 覆盖到 satp 中
    csrw    satp, t0
ffffffffc0200034:	18029073          	csrw	satp,t0
    # 使用 sfence.vma 指令刷新 TLB
    sfence.vma
ffffffffc0200038:	12000073          	sfence.vma
    # 从此，我们给内核搭建出了一个完美的虚拟内存空间！
    #nop # 可能映射的位置有些bug。。插入一个nop
    
    # 我们在虚拟内存空间中：随意将 sp 设置为虚拟地址！
    lui sp, %hi(bootstacktop)
ffffffffc020003c:	c0208137          	lui	sp,0xc0208

    # 我们在虚拟内存空间中：随意跳转到虚拟地址！
    # 跳转到 kern_init
    lui t0, %hi(kern_init)
ffffffffc0200040:	c02002b7          	lui	t0,0xc0200
    addi t0, t0, %lo(kern_init)
ffffffffc0200044:	04a28293          	addi	t0,t0,74 # ffffffffc020004a <kern_init>
    jr t0
ffffffffc0200048:	8282                	jr	t0

ffffffffc020004a <kern_init>:
void grade_backtrace(void);

int kern_init(void)
{
    extern char edata[], end[];
    memset(edata, 0, end - edata);
ffffffffc020004a:	00009517          	auipc	a0,0x9
ffffffffc020004e:	fe650513          	addi	a0,a0,-26 # ffffffffc0209030 <buf>
ffffffffc0200052:	0000d617          	auipc	a2,0xd
ffffffffc0200056:	49260613          	addi	a2,a2,1170 # ffffffffc020d4e4 <end>
{
ffffffffc020005a:	1141                	addi	sp,sp,-16
    memset(edata, 0, end - edata);
ffffffffc020005c:	8e09                	sub	a2,a2,a0
ffffffffc020005e:	4581                	li	a1,0
{
ffffffffc0200060:	e406                	sd	ra,8(sp)
    memset(edata, 0, end - edata);
ffffffffc0200062:	5ff030ef          	jal	ra,ffffffffc0203e60 <memset>
    dtb_init();
ffffffffc0200066:	514000ef          	jal	ra,ffffffffc020057a <dtb_init>
    cons_init(); // init the console
ffffffffc020006a:	49e000ef          	jal	ra,ffffffffc0200508 <cons_init>

    const char *message = "(THU.CST) os is loading ...";
    cprintf("%s\n\n", message);
ffffffffc020006e:	00004597          	auipc	a1,0x4
ffffffffc0200072:	e4258593          	addi	a1,a1,-446 # ffffffffc0203eb0 <etext+0x2>
ffffffffc0200076:	00004517          	auipc	a0,0x4
ffffffffc020007a:	e5a50513          	addi	a0,a0,-422 # ffffffffc0203ed0 <etext+0x22>
ffffffffc020007e:	116000ef          	jal	ra,ffffffffc0200194 <cprintf>

    print_kerninfo();
ffffffffc0200082:	15a000ef          	jal	ra,ffffffffc02001dc <print_kerninfo>

    // grade_backtrace();

    pmm_init(); // init physical memory management
ffffffffc0200086:	0ac020ef          	jal	ra,ffffffffc0202132 <pmm_init>

    pic_init(); // init interrupt controller
ffffffffc020008a:	0ad000ef          	jal	ra,ffffffffc0200936 <pic_init>
    idt_init(); // init interrupt descriptor table
ffffffffc020008e:	0ab000ef          	jal	ra,ffffffffc0200938 <idt_init>

    vmm_init();  // init virtual memory management
ffffffffc0200092:	615020ef          	jal	ra,ffffffffc0202ea6 <vmm_init>
    proc_init(); // init process table
ffffffffc0200096:	58a030ef          	jal	ra,ffffffffc0203620 <proc_init>

    clock_init();  // init clock interrupt
ffffffffc020009a:	41c000ef          	jal	ra,ffffffffc02004b6 <clock_init>
    intr_enable(); // enable irq interrupt
ffffffffc020009e:	08d000ef          	jal	ra,ffffffffc020092a <intr_enable>

    cpu_idle(); // run idle process
ffffffffc02000a2:	7cc030ef          	jal	ra,ffffffffc020386e <cpu_idle>

ffffffffc02000a6 <readline>:
 * The readline() function returns the text of the line read. If some errors
 * are happened, NULL is returned. The return value is a global variable,
 * thus it should be copied before it is used.
 * */
char *
readline(const char *prompt) {
ffffffffc02000a6:	715d                	addi	sp,sp,-80
ffffffffc02000a8:	e486                	sd	ra,72(sp)
ffffffffc02000aa:	e0a6                	sd	s1,64(sp)
ffffffffc02000ac:	fc4a                	sd	s2,56(sp)
ffffffffc02000ae:	f84e                	sd	s3,48(sp)
ffffffffc02000b0:	f452                	sd	s4,40(sp)
ffffffffc02000b2:	f056                	sd	s5,32(sp)
ffffffffc02000b4:	ec5a                	sd	s6,24(sp)
ffffffffc02000b6:	e85e                	sd	s7,16(sp)
    if (prompt != NULL) {
ffffffffc02000b8:	c901                	beqz	a0,ffffffffc02000c8 <readline+0x22>
ffffffffc02000ba:	85aa                	mv	a1,a0
        cprintf("%s", prompt);
ffffffffc02000bc:	00004517          	auipc	a0,0x4
ffffffffc02000c0:	e1c50513          	addi	a0,a0,-484 # ffffffffc0203ed8 <etext+0x2a>
ffffffffc02000c4:	0d0000ef          	jal	ra,ffffffffc0200194 <cprintf>
readline(const char *prompt) {
ffffffffc02000c8:	4481                	li	s1,0
    while (1) {
        c = getchar();
        if (c < 0) {
            return NULL;
        }
        else if (c >= ' ' && i < BUFSIZE - 1) {
ffffffffc02000ca:	497d                	li	s2,31
            cputchar(c);
            buf[i ++] = c;
        }
        else if (c == '\b' && i > 0) {
ffffffffc02000cc:	49a1                	li	s3,8
            cputchar(c);
            i --;
        }
        else if (c == '\n' || c == '\r') {
ffffffffc02000ce:	4aa9                	li	s5,10
ffffffffc02000d0:	4b35                	li	s6,13
            buf[i ++] = c;
ffffffffc02000d2:	00009b97          	auipc	s7,0x9
ffffffffc02000d6:	f5eb8b93          	addi	s7,s7,-162 # ffffffffc0209030 <buf>
        else if (c >= ' ' && i < BUFSIZE - 1) {
ffffffffc02000da:	3fe00a13          	li	s4,1022
        c = getchar();
ffffffffc02000de:	0ee000ef          	jal	ra,ffffffffc02001cc <getchar>
        if (c < 0) {
ffffffffc02000e2:	00054a63          	bltz	a0,ffffffffc02000f6 <readline+0x50>
        else if (c >= ' ' && i < BUFSIZE - 1) {
ffffffffc02000e6:	00a95a63          	bge	s2,a0,ffffffffc02000fa <readline+0x54>
ffffffffc02000ea:	029a5263          	bge	s4,s1,ffffffffc020010e <readline+0x68>
        c = getchar();
ffffffffc02000ee:	0de000ef          	jal	ra,ffffffffc02001cc <getchar>
        if (c < 0) {
ffffffffc02000f2:	fe055ae3          	bgez	a0,ffffffffc02000e6 <readline+0x40>
            return NULL;
ffffffffc02000f6:	4501                	li	a0,0
ffffffffc02000f8:	a091                	j	ffffffffc020013c <readline+0x96>
        else if (c == '\b' && i > 0) {
ffffffffc02000fa:	03351463          	bne	a0,s3,ffffffffc0200122 <readline+0x7c>
ffffffffc02000fe:	e8a9                	bnez	s1,ffffffffc0200150 <readline+0xaa>
        c = getchar();
ffffffffc0200100:	0cc000ef          	jal	ra,ffffffffc02001cc <getchar>
        if (c < 0) {
ffffffffc0200104:	fe0549e3          	bltz	a0,ffffffffc02000f6 <readline+0x50>
        else if (c >= ' ' && i < BUFSIZE - 1) {
ffffffffc0200108:	fea959e3          	bge	s2,a0,ffffffffc02000fa <readline+0x54>
ffffffffc020010c:	4481                	li	s1,0
            cputchar(c);
ffffffffc020010e:	e42a                	sd	a0,8(sp)
ffffffffc0200110:	0ba000ef          	jal	ra,ffffffffc02001ca <cputchar>
            buf[i ++] = c;
ffffffffc0200114:	6522                	ld	a0,8(sp)
ffffffffc0200116:	009b87b3          	add	a5,s7,s1
ffffffffc020011a:	2485                	addiw	s1,s1,1
ffffffffc020011c:	00a78023          	sb	a0,0(a5)
ffffffffc0200120:	bf7d                	j	ffffffffc02000de <readline+0x38>
        else if (c == '\n' || c == '\r') {
ffffffffc0200122:	01550463          	beq	a0,s5,ffffffffc020012a <readline+0x84>
ffffffffc0200126:	fb651ce3          	bne	a0,s6,ffffffffc02000de <readline+0x38>
            cputchar(c);
ffffffffc020012a:	0a0000ef          	jal	ra,ffffffffc02001ca <cputchar>
            buf[i] = '\0';
ffffffffc020012e:	00009517          	auipc	a0,0x9
ffffffffc0200132:	f0250513          	addi	a0,a0,-254 # ffffffffc0209030 <buf>
ffffffffc0200136:	94aa                	add	s1,s1,a0
ffffffffc0200138:	00048023          	sb	zero,0(s1)
            return buf;
        }
    }
}
ffffffffc020013c:	60a6                	ld	ra,72(sp)
ffffffffc020013e:	6486                	ld	s1,64(sp)
ffffffffc0200140:	7962                	ld	s2,56(sp)
ffffffffc0200142:	79c2                	ld	s3,48(sp)
ffffffffc0200144:	7a22                	ld	s4,40(sp)
ffffffffc0200146:	7a82                	ld	s5,32(sp)
ffffffffc0200148:	6b62                	ld	s6,24(sp)
ffffffffc020014a:	6bc2                	ld	s7,16(sp)
ffffffffc020014c:	6161                	addi	sp,sp,80
ffffffffc020014e:	8082                	ret
            cputchar(c);
ffffffffc0200150:	4521                	li	a0,8
ffffffffc0200152:	078000ef          	jal	ra,ffffffffc02001ca <cputchar>
            i --;
ffffffffc0200156:	34fd                	addiw	s1,s1,-1
ffffffffc0200158:	b759                	j	ffffffffc02000de <readline+0x38>

ffffffffc020015a <cputch>:
 * cputch - writes a single character @c to stdout, and it will
 * increace the value of counter pointed by @cnt.
 * */
static void
cputch(int c, int *cnt)
{
ffffffffc020015a:	1141                	addi	sp,sp,-16
ffffffffc020015c:	e022                	sd	s0,0(sp)
ffffffffc020015e:	e406                	sd	ra,8(sp)
ffffffffc0200160:	842e                	mv	s0,a1
    cons_putc(c);
ffffffffc0200162:	3a8000ef          	jal	ra,ffffffffc020050a <cons_putc>
    (*cnt)++;
ffffffffc0200166:	401c                	lw	a5,0(s0)
}
ffffffffc0200168:	60a2                	ld	ra,8(sp)
    (*cnt)++;
ffffffffc020016a:	2785                	addiw	a5,a5,1
ffffffffc020016c:	c01c                	sw	a5,0(s0)
}
ffffffffc020016e:	6402                	ld	s0,0(sp)
ffffffffc0200170:	0141                	addi	sp,sp,16
ffffffffc0200172:	8082                	ret

ffffffffc0200174 <vcprintf>:
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want cprintf() instead.
 * */
int vcprintf(const char *fmt, va_list ap)
{
ffffffffc0200174:	1101                	addi	sp,sp,-32
ffffffffc0200176:	862a                	mv	a2,a0
ffffffffc0200178:	86ae                	mv	a3,a1
    int cnt = 0;
    vprintfmt((void *)cputch, &cnt, fmt, ap);
ffffffffc020017a:	00000517          	auipc	a0,0x0
ffffffffc020017e:	fe050513          	addi	a0,a0,-32 # ffffffffc020015a <cputch>
ffffffffc0200182:	006c                	addi	a1,sp,12
{
ffffffffc0200184:	ec06                	sd	ra,24(sp)
    int cnt = 0;
ffffffffc0200186:	c602                	sw	zero,12(sp)
    vprintfmt((void *)cputch, &cnt, fmt, ap);
ffffffffc0200188:	0b5030ef          	jal	ra,ffffffffc0203a3c <vprintfmt>
    return cnt;
}
ffffffffc020018c:	60e2                	ld	ra,24(sp)
ffffffffc020018e:	4532                	lw	a0,12(sp)
ffffffffc0200190:	6105                	addi	sp,sp,32
ffffffffc0200192:	8082                	ret

ffffffffc0200194 <cprintf>:
 *
 * The return value is the number of characters which would be
 * written to stdout.
 * */
int cprintf(const char *fmt, ...)
{
ffffffffc0200194:	711d                	addi	sp,sp,-96
    va_list ap;
    int cnt;
    va_start(ap, fmt);
ffffffffc0200196:	02810313          	addi	t1,sp,40 # ffffffffc0208028 <boot_page_table_sv39+0x28>
{
ffffffffc020019a:	8e2a                	mv	t3,a0
ffffffffc020019c:	f42e                	sd	a1,40(sp)
ffffffffc020019e:	f832                	sd	a2,48(sp)
ffffffffc02001a0:	fc36                	sd	a3,56(sp)
    vprintfmt((void *)cputch, &cnt, fmt, ap);
ffffffffc02001a2:	00000517          	auipc	a0,0x0
ffffffffc02001a6:	fb850513          	addi	a0,a0,-72 # ffffffffc020015a <cputch>
ffffffffc02001aa:	004c                	addi	a1,sp,4
ffffffffc02001ac:	869a                	mv	a3,t1
ffffffffc02001ae:	8672                	mv	a2,t3
{
ffffffffc02001b0:	ec06                	sd	ra,24(sp)
ffffffffc02001b2:	e0ba                	sd	a4,64(sp)
ffffffffc02001b4:	e4be                	sd	a5,72(sp)
ffffffffc02001b6:	e8c2                	sd	a6,80(sp)
ffffffffc02001b8:	ecc6                	sd	a7,88(sp)
    va_start(ap, fmt);
ffffffffc02001ba:	e41a                	sd	t1,8(sp)
    int cnt = 0;
ffffffffc02001bc:	c202                	sw	zero,4(sp)
    vprintfmt((void *)cputch, &cnt, fmt, ap);
ffffffffc02001be:	07f030ef          	jal	ra,ffffffffc0203a3c <vprintfmt>
    cnt = vcprintf(fmt, ap);
    va_end(ap);
    return cnt;
}
ffffffffc02001c2:	60e2                	ld	ra,24(sp)
ffffffffc02001c4:	4512                	lw	a0,4(sp)
ffffffffc02001c6:	6125                	addi	sp,sp,96
ffffffffc02001c8:	8082                	ret

ffffffffc02001ca <cputchar>:

/* cputchar - writes a single character to stdout */
void cputchar(int c)
{
    cons_putc(c);
ffffffffc02001ca:	a681                	j	ffffffffc020050a <cons_putc>

ffffffffc02001cc <getchar>:
}

/* getchar - reads a single non-zero character from stdin */
int getchar(void)
{
ffffffffc02001cc:	1141                	addi	sp,sp,-16
ffffffffc02001ce:	e406                	sd	ra,8(sp)
    int c;
    while ((c = cons_getc()) == 0)
ffffffffc02001d0:	36e000ef          	jal	ra,ffffffffc020053e <cons_getc>
ffffffffc02001d4:	dd75                	beqz	a0,ffffffffc02001d0 <getchar+0x4>
        /* do nothing */;
    return c;
}
ffffffffc02001d6:	60a2                	ld	ra,8(sp)
ffffffffc02001d8:	0141                	addi	sp,sp,16
ffffffffc02001da:	8082                	ret

ffffffffc02001dc <print_kerninfo>:
 * print_kerninfo - print the information about kernel, including the location
 * of kernel entry, the start addresses of data and text segements, the start
 * address of free memory and how many memory that kernel has used.
 * */
void print_kerninfo(void)
{
ffffffffc02001dc:	1141                	addi	sp,sp,-16
    extern char etext[], edata[], end[], kern_init[];
    cprintf("Special kernel symbols:\n");
ffffffffc02001de:	00004517          	auipc	a0,0x4
ffffffffc02001e2:	d0250513          	addi	a0,a0,-766 # ffffffffc0203ee0 <etext+0x32>
{
ffffffffc02001e6:	e406                	sd	ra,8(sp)
    cprintf("Special kernel symbols:\n");
ffffffffc02001e8:	fadff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  entry  0x%08x (virtual)\n", kern_init);
ffffffffc02001ec:	00000597          	auipc	a1,0x0
ffffffffc02001f0:	e5e58593          	addi	a1,a1,-418 # ffffffffc020004a <kern_init>
ffffffffc02001f4:	00004517          	auipc	a0,0x4
ffffffffc02001f8:	d0c50513          	addi	a0,a0,-756 # ffffffffc0203f00 <etext+0x52>
ffffffffc02001fc:	f99ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  etext  0x%08x (virtual)\n", etext);
ffffffffc0200200:	00004597          	auipc	a1,0x4
ffffffffc0200204:	cae58593          	addi	a1,a1,-850 # ffffffffc0203eae <etext>
ffffffffc0200208:	00004517          	auipc	a0,0x4
ffffffffc020020c:	d1850513          	addi	a0,a0,-744 # ffffffffc0203f20 <etext+0x72>
ffffffffc0200210:	f85ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  edata  0x%08x (virtual)\n", edata);
ffffffffc0200214:	00009597          	auipc	a1,0x9
ffffffffc0200218:	e1c58593          	addi	a1,a1,-484 # ffffffffc0209030 <buf>
ffffffffc020021c:	00004517          	auipc	a0,0x4
ffffffffc0200220:	d2450513          	addi	a0,a0,-732 # ffffffffc0203f40 <etext+0x92>
ffffffffc0200224:	f71ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  end    0x%08x (virtual)\n", end);
ffffffffc0200228:	0000d597          	auipc	a1,0xd
ffffffffc020022c:	2bc58593          	addi	a1,a1,700 # ffffffffc020d4e4 <end>
ffffffffc0200230:	00004517          	auipc	a0,0x4
ffffffffc0200234:	d3050513          	addi	a0,a0,-720 # ffffffffc0203f60 <etext+0xb2>
ffffffffc0200238:	f5dff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("Kernel executable memory footprint: %dKB\n",
            (end - kern_init + 1023) / 1024);
ffffffffc020023c:	0000d597          	auipc	a1,0xd
ffffffffc0200240:	6a758593          	addi	a1,a1,1703 # ffffffffc020d8e3 <end+0x3ff>
ffffffffc0200244:	00000797          	auipc	a5,0x0
ffffffffc0200248:	e0678793          	addi	a5,a5,-506 # ffffffffc020004a <kern_init>
ffffffffc020024c:	40f587b3          	sub	a5,a1,a5
    cprintf("Kernel executable memory footprint: %dKB\n",
ffffffffc0200250:	43f7d593          	srai	a1,a5,0x3f
}
ffffffffc0200254:	60a2                	ld	ra,8(sp)
    cprintf("Kernel executable memory footprint: %dKB\n",
ffffffffc0200256:	3ff5f593          	andi	a1,a1,1023
ffffffffc020025a:	95be                	add	a1,a1,a5
ffffffffc020025c:	85a9                	srai	a1,a1,0xa
ffffffffc020025e:	00004517          	auipc	a0,0x4
ffffffffc0200262:	d2250513          	addi	a0,a0,-734 # ffffffffc0203f80 <etext+0xd2>
}
ffffffffc0200266:	0141                	addi	sp,sp,16
    cprintf("Kernel executable memory footprint: %dKB\n",
ffffffffc0200268:	b735                	j	ffffffffc0200194 <cprintf>

ffffffffc020026a <print_stackframe>:
 * jumping
 * to the kernel entry, the value of ebp has been set to zero, that's the
 * boundary.
 * */
void print_stackframe(void)
{
ffffffffc020026a:	1141                	addi	sp,sp,-16
    panic("Not Implemented!");
ffffffffc020026c:	00004617          	auipc	a2,0x4
ffffffffc0200270:	d4460613          	addi	a2,a2,-700 # ffffffffc0203fb0 <etext+0x102>
ffffffffc0200274:	04900593          	li	a1,73
ffffffffc0200278:	00004517          	auipc	a0,0x4
ffffffffc020027c:	d5050513          	addi	a0,a0,-688 # ffffffffc0203fc8 <etext+0x11a>
{
ffffffffc0200280:	e406                	sd	ra,8(sp)
    panic("Not Implemented!");
ffffffffc0200282:	1d8000ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0200286 <mon_help>:
    }
}

/* mon_help - print the information about mon_* functions */
int
mon_help(int argc, char **argv, struct trapframe *tf) {
ffffffffc0200286:	1141                	addi	sp,sp,-16
    int i;
    for (i = 0; i < NCOMMANDS; i ++) {
        cprintf("%s - %s\n", commands[i].name, commands[i].desc);
ffffffffc0200288:	00004617          	auipc	a2,0x4
ffffffffc020028c:	d5860613          	addi	a2,a2,-680 # ffffffffc0203fe0 <etext+0x132>
ffffffffc0200290:	00004597          	auipc	a1,0x4
ffffffffc0200294:	d7058593          	addi	a1,a1,-656 # ffffffffc0204000 <etext+0x152>
ffffffffc0200298:	00004517          	auipc	a0,0x4
ffffffffc020029c:	d7050513          	addi	a0,a0,-656 # ffffffffc0204008 <etext+0x15a>
mon_help(int argc, char **argv, struct trapframe *tf) {
ffffffffc02002a0:	e406                	sd	ra,8(sp)
        cprintf("%s - %s\n", commands[i].name, commands[i].desc);
ffffffffc02002a2:	ef3ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
ffffffffc02002a6:	00004617          	auipc	a2,0x4
ffffffffc02002aa:	d7260613          	addi	a2,a2,-654 # ffffffffc0204018 <etext+0x16a>
ffffffffc02002ae:	00004597          	auipc	a1,0x4
ffffffffc02002b2:	d9258593          	addi	a1,a1,-622 # ffffffffc0204040 <etext+0x192>
ffffffffc02002b6:	00004517          	auipc	a0,0x4
ffffffffc02002ba:	d5250513          	addi	a0,a0,-686 # ffffffffc0204008 <etext+0x15a>
ffffffffc02002be:	ed7ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
ffffffffc02002c2:	00004617          	auipc	a2,0x4
ffffffffc02002c6:	d8e60613          	addi	a2,a2,-626 # ffffffffc0204050 <etext+0x1a2>
ffffffffc02002ca:	00004597          	auipc	a1,0x4
ffffffffc02002ce:	da658593          	addi	a1,a1,-602 # ffffffffc0204070 <etext+0x1c2>
ffffffffc02002d2:	00004517          	auipc	a0,0x4
ffffffffc02002d6:	d3650513          	addi	a0,a0,-714 # ffffffffc0204008 <etext+0x15a>
ffffffffc02002da:	ebbff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    }
    return 0;
}
ffffffffc02002de:	60a2                	ld	ra,8(sp)
ffffffffc02002e0:	4501                	li	a0,0
ffffffffc02002e2:	0141                	addi	sp,sp,16
ffffffffc02002e4:	8082                	ret

ffffffffc02002e6 <mon_kerninfo>:
/* *
 * mon_kerninfo - call print_kerninfo in kern/debug/kdebug.c to
 * print the memory occupancy in kernel.
 * */
int
mon_kerninfo(int argc, char **argv, struct trapframe *tf) {
ffffffffc02002e6:	1141                	addi	sp,sp,-16
ffffffffc02002e8:	e406                	sd	ra,8(sp)
    print_kerninfo();
ffffffffc02002ea:	ef3ff0ef          	jal	ra,ffffffffc02001dc <print_kerninfo>
    return 0;
}
ffffffffc02002ee:	60a2                	ld	ra,8(sp)
ffffffffc02002f0:	4501                	li	a0,0
ffffffffc02002f2:	0141                	addi	sp,sp,16
ffffffffc02002f4:	8082                	ret

ffffffffc02002f6 <mon_backtrace>:
/* *
 * mon_backtrace - call print_stackframe in kern/debug/kdebug.c to
 * print a backtrace of the stack.
 * */
int
mon_backtrace(int argc, char **argv, struct trapframe *tf) {
ffffffffc02002f6:	1141                	addi	sp,sp,-16
ffffffffc02002f8:	e406                	sd	ra,8(sp)
    print_stackframe();
ffffffffc02002fa:	f71ff0ef          	jal	ra,ffffffffc020026a <print_stackframe>
    return 0;
}
ffffffffc02002fe:	60a2                	ld	ra,8(sp)
ffffffffc0200300:	4501                	li	a0,0
ffffffffc0200302:	0141                	addi	sp,sp,16
ffffffffc0200304:	8082                	ret

ffffffffc0200306 <kmonitor>:
kmonitor(struct trapframe *tf) {
ffffffffc0200306:	7115                	addi	sp,sp,-224
ffffffffc0200308:	ed5e                	sd	s7,152(sp)
ffffffffc020030a:	8baa                	mv	s7,a0
    cprintf("Welcome to the kernel debug monitor!!\n");
ffffffffc020030c:	00004517          	auipc	a0,0x4
ffffffffc0200310:	d7450513          	addi	a0,a0,-652 # ffffffffc0204080 <etext+0x1d2>
kmonitor(struct trapframe *tf) {
ffffffffc0200314:	ed86                	sd	ra,216(sp)
ffffffffc0200316:	e9a2                	sd	s0,208(sp)
ffffffffc0200318:	e5a6                	sd	s1,200(sp)
ffffffffc020031a:	e1ca                	sd	s2,192(sp)
ffffffffc020031c:	fd4e                	sd	s3,184(sp)
ffffffffc020031e:	f952                	sd	s4,176(sp)
ffffffffc0200320:	f556                	sd	s5,168(sp)
ffffffffc0200322:	f15a                	sd	s6,160(sp)
ffffffffc0200324:	e962                	sd	s8,144(sp)
ffffffffc0200326:	e566                	sd	s9,136(sp)
ffffffffc0200328:	e16a                	sd	s10,128(sp)
    cprintf("Welcome to the kernel debug monitor!!\n");
ffffffffc020032a:	e6bff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("Type 'help' for a list of commands.\n");
ffffffffc020032e:	00004517          	auipc	a0,0x4
ffffffffc0200332:	d7a50513          	addi	a0,a0,-646 # ffffffffc02040a8 <etext+0x1fa>
ffffffffc0200336:	e5fff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    if (tf != NULL) {
ffffffffc020033a:	000b8563          	beqz	s7,ffffffffc0200344 <kmonitor+0x3e>
        print_trapframe(tf);
ffffffffc020033e:	855e                	mv	a0,s7
ffffffffc0200340:	7e0000ef          	jal	ra,ffffffffc0200b20 <print_trapframe>
#endif
}

static inline void sbi_shutdown(void)
{
	SBI_CALL_0(SBI_SHUTDOWN);
ffffffffc0200344:	4501                	li	a0,0
ffffffffc0200346:	4581                	li	a1,0
ffffffffc0200348:	4601                	li	a2,0
ffffffffc020034a:	48a1                	li	a7,8
ffffffffc020034c:	00000073          	ecall
ffffffffc0200350:	00004c17          	auipc	s8,0x4
ffffffffc0200354:	dc8c0c13          	addi	s8,s8,-568 # ffffffffc0204118 <commands>
        if ((buf = readline("K> ")) != NULL) {
ffffffffc0200358:	00004917          	auipc	s2,0x4
ffffffffc020035c:	d7890913          	addi	s2,s2,-648 # ffffffffc02040d0 <etext+0x222>
        while (*buf != '\0' && strchr(WHITESPACE, *buf) != NULL) {
ffffffffc0200360:	00004497          	auipc	s1,0x4
ffffffffc0200364:	d7848493          	addi	s1,s1,-648 # ffffffffc02040d8 <etext+0x22a>
        if (argc == MAXARGS - 1) {
ffffffffc0200368:	49bd                	li	s3,15
            cprintf("Too many arguments (max %d).\n", MAXARGS);
ffffffffc020036a:	00004b17          	auipc	s6,0x4
ffffffffc020036e:	d76b0b13          	addi	s6,s6,-650 # ffffffffc02040e0 <etext+0x232>
        argv[argc ++] = buf;
ffffffffc0200372:	00004a17          	auipc	s4,0x4
ffffffffc0200376:	c8ea0a13          	addi	s4,s4,-882 # ffffffffc0204000 <etext+0x152>
    for (i = 0; i < NCOMMANDS; i ++) {
ffffffffc020037a:	4a8d                	li	s5,3
        if ((buf = readline("K> ")) != NULL) {
ffffffffc020037c:	854a                	mv	a0,s2
ffffffffc020037e:	d29ff0ef          	jal	ra,ffffffffc02000a6 <readline>
ffffffffc0200382:	842a                	mv	s0,a0
ffffffffc0200384:	dd65                	beqz	a0,ffffffffc020037c <kmonitor+0x76>
        while (*buf != '\0' && strchr(WHITESPACE, *buf) != NULL) {
ffffffffc0200386:	00054583          	lbu	a1,0(a0)
    int argc = 0;
ffffffffc020038a:	4c81                	li	s9,0
        while (*buf != '\0' && strchr(WHITESPACE, *buf) != NULL) {
ffffffffc020038c:	e1bd                	bnez	a1,ffffffffc02003f2 <kmonitor+0xec>
    if (argc == 0) {
ffffffffc020038e:	fe0c87e3          	beqz	s9,ffffffffc020037c <kmonitor+0x76>
        if (strcmp(commands[i].name, argv[0]) == 0) {
ffffffffc0200392:	6582                	ld	a1,0(sp)
ffffffffc0200394:	00004d17          	auipc	s10,0x4
ffffffffc0200398:	d84d0d13          	addi	s10,s10,-636 # ffffffffc0204118 <commands>
        argv[argc ++] = buf;
ffffffffc020039c:	8552                	mv	a0,s4
    for (i = 0; i < NCOMMANDS; i ++) {
ffffffffc020039e:	4401                	li	s0,0
ffffffffc02003a0:	0d61                	addi	s10,s10,24
        if (strcmp(commands[i].name, argv[0]) == 0) {
ffffffffc02003a2:	265030ef          	jal	ra,ffffffffc0203e06 <strcmp>
ffffffffc02003a6:	c919                	beqz	a0,ffffffffc02003bc <kmonitor+0xb6>
    for (i = 0; i < NCOMMANDS; i ++) {
ffffffffc02003a8:	2405                	addiw	s0,s0,1
ffffffffc02003aa:	0b540063          	beq	s0,s5,ffffffffc020044a <kmonitor+0x144>
        if (strcmp(commands[i].name, argv[0]) == 0) {
ffffffffc02003ae:	000d3503          	ld	a0,0(s10)
ffffffffc02003b2:	6582                	ld	a1,0(sp)
    for (i = 0; i < NCOMMANDS; i ++) {
ffffffffc02003b4:	0d61                	addi	s10,s10,24
        if (strcmp(commands[i].name, argv[0]) == 0) {
ffffffffc02003b6:	251030ef          	jal	ra,ffffffffc0203e06 <strcmp>
ffffffffc02003ba:	f57d                	bnez	a0,ffffffffc02003a8 <kmonitor+0xa2>
            return commands[i].func(argc - 1, argv + 1, tf);
ffffffffc02003bc:	00141793          	slli	a5,s0,0x1
ffffffffc02003c0:	97a2                	add	a5,a5,s0
ffffffffc02003c2:	078e                	slli	a5,a5,0x3
ffffffffc02003c4:	97e2                	add	a5,a5,s8
ffffffffc02003c6:	6b9c                	ld	a5,16(a5)
ffffffffc02003c8:	865e                	mv	a2,s7
ffffffffc02003ca:	002c                	addi	a1,sp,8
ffffffffc02003cc:	fffc851b          	addiw	a0,s9,-1
ffffffffc02003d0:	9782                	jalr	a5
            if (runcmd(buf, tf) < 0) {
ffffffffc02003d2:	fa0555e3          	bgez	a0,ffffffffc020037c <kmonitor+0x76>
}
ffffffffc02003d6:	60ee                	ld	ra,216(sp)
ffffffffc02003d8:	644e                	ld	s0,208(sp)
ffffffffc02003da:	64ae                	ld	s1,200(sp)
ffffffffc02003dc:	690e                	ld	s2,192(sp)
ffffffffc02003de:	79ea                	ld	s3,184(sp)
ffffffffc02003e0:	7a4a                	ld	s4,176(sp)
ffffffffc02003e2:	7aaa                	ld	s5,168(sp)
ffffffffc02003e4:	7b0a                	ld	s6,160(sp)
ffffffffc02003e6:	6bea                	ld	s7,152(sp)
ffffffffc02003e8:	6c4a                	ld	s8,144(sp)
ffffffffc02003ea:	6caa                	ld	s9,136(sp)
ffffffffc02003ec:	6d0a                	ld	s10,128(sp)
ffffffffc02003ee:	612d                	addi	sp,sp,224
ffffffffc02003f0:	8082                	ret
        while (*buf != '\0' && strchr(WHITESPACE, *buf) != NULL) {
ffffffffc02003f2:	8526                	mv	a0,s1
ffffffffc02003f4:	257030ef          	jal	ra,ffffffffc0203e4a <strchr>
ffffffffc02003f8:	c901                	beqz	a0,ffffffffc0200408 <kmonitor+0x102>
ffffffffc02003fa:	00144583          	lbu	a1,1(s0)
            *buf ++ = '\0';
ffffffffc02003fe:	00040023          	sb	zero,0(s0)
ffffffffc0200402:	0405                	addi	s0,s0,1
        while (*buf != '\0' && strchr(WHITESPACE, *buf) != NULL) {
ffffffffc0200404:	d5c9                	beqz	a1,ffffffffc020038e <kmonitor+0x88>
ffffffffc0200406:	b7f5                	j	ffffffffc02003f2 <kmonitor+0xec>
        if (*buf == '\0') {
ffffffffc0200408:	00044783          	lbu	a5,0(s0)
ffffffffc020040c:	d3c9                	beqz	a5,ffffffffc020038e <kmonitor+0x88>
        if (argc == MAXARGS - 1) {
ffffffffc020040e:	033c8963          	beq	s9,s3,ffffffffc0200440 <kmonitor+0x13a>
        argv[argc ++] = buf;
ffffffffc0200412:	003c9793          	slli	a5,s9,0x3
ffffffffc0200416:	0118                	addi	a4,sp,128
ffffffffc0200418:	97ba                	add	a5,a5,a4
ffffffffc020041a:	f887b023          	sd	s0,-128(a5)
        while (*buf != '\0' && strchr(WHITESPACE, *buf) == NULL) {
ffffffffc020041e:	00044583          	lbu	a1,0(s0)
        argv[argc ++] = buf;
ffffffffc0200422:	2c85                	addiw	s9,s9,1
        while (*buf != '\0' && strchr(WHITESPACE, *buf) == NULL) {
ffffffffc0200424:	e591                	bnez	a1,ffffffffc0200430 <kmonitor+0x12a>
ffffffffc0200426:	b7b5                	j	ffffffffc0200392 <kmonitor+0x8c>
ffffffffc0200428:	00144583          	lbu	a1,1(s0)
            buf ++;
ffffffffc020042c:	0405                	addi	s0,s0,1
        while (*buf != '\0' && strchr(WHITESPACE, *buf) == NULL) {
ffffffffc020042e:	d1a5                	beqz	a1,ffffffffc020038e <kmonitor+0x88>
ffffffffc0200430:	8526                	mv	a0,s1
ffffffffc0200432:	219030ef          	jal	ra,ffffffffc0203e4a <strchr>
ffffffffc0200436:	d96d                	beqz	a0,ffffffffc0200428 <kmonitor+0x122>
        while (*buf != '\0' && strchr(WHITESPACE, *buf) != NULL) {
ffffffffc0200438:	00044583          	lbu	a1,0(s0)
ffffffffc020043c:	d9a9                	beqz	a1,ffffffffc020038e <kmonitor+0x88>
ffffffffc020043e:	bf55                	j	ffffffffc02003f2 <kmonitor+0xec>
            cprintf("Too many arguments (max %d).\n", MAXARGS);
ffffffffc0200440:	45c1                	li	a1,16
ffffffffc0200442:	855a                	mv	a0,s6
ffffffffc0200444:	d51ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
ffffffffc0200448:	b7e9                	j	ffffffffc0200412 <kmonitor+0x10c>
    cprintf("Unknown command '%s'\n", argv[0]);
ffffffffc020044a:	6582                	ld	a1,0(sp)
ffffffffc020044c:	00004517          	auipc	a0,0x4
ffffffffc0200450:	cb450513          	addi	a0,a0,-844 # ffffffffc0204100 <etext+0x252>
ffffffffc0200454:	d41ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    return 0;
ffffffffc0200458:	b715                	j	ffffffffc020037c <kmonitor+0x76>

ffffffffc020045a <__panic>:
 * __panic - __panic is called on unresolvable fatal errors. it prints
 * "panic: 'message'", and then enters the kernel monitor.
 * */
void
__panic(const char *file, int line, const char *fmt, ...) {
    if (is_panic) {
ffffffffc020045a:	0000d317          	auipc	t1,0xd
ffffffffc020045e:	00e30313          	addi	t1,t1,14 # ffffffffc020d468 <is_panic>
ffffffffc0200462:	00032e03          	lw	t3,0(t1)
__panic(const char *file, int line, const char *fmt, ...) {
ffffffffc0200466:	715d                	addi	sp,sp,-80
ffffffffc0200468:	ec06                	sd	ra,24(sp)
ffffffffc020046a:	e822                	sd	s0,16(sp)
ffffffffc020046c:	f436                	sd	a3,40(sp)
ffffffffc020046e:	f83a                	sd	a4,48(sp)
ffffffffc0200470:	fc3e                	sd	a5,56(sp)
ffffffffc0200472:	e0c2                	sd	a6,64(sp)
ffffffffc0200474:	e4c6                	sd	a7,72(sp)
    if (is_panic) {
ffffffffc0200476:	020e1a63          	bnez	t3,ffffffffc02004aa <__panic+0x50>
        goto panic_dead;
    }
    is_panic = 1;
ffffffffc020047a:	4785                	li	a5,1
ffffffffc020047c:	00f32023          	sw	a5,0(t1)

    // print the 'message'
    va_list ap;
    va_start(ap, fmt);
ffffffffc0200480:	8432                	mv	s0,a2
ffffffffc0200482:	103c                	addi	a5,sp,40
    cprintf("kernel panic at %s:%d:\n    ", file, line);
ffffffffc0200484:	862e                	mv	a2,a1
ffffffffc0200486:	85aa                	mv	a1,a0
ffffffffc0200488:	00004517          	auipc	a0,0x4
ffffffffc020048c:	cd850513          	addi	a0,a0,-808 # ffffffffc0204160 <commands+0x48>
    va_start(ap, fmt);
ffffffffc0200490:	e43e                	sd	a5,8(sp)
    cprintf("kernel panic at %s:%d:\n    ", file, line);
ffffffffc0200492:	d03ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    vcprintf(fmt, ap);
ffffffffc0200496:	65a2                	ld	a1,8(sp)
ffffffffc0200498:	8522                	mv	a0,s0
ffffffffc020049a:	cdbff0ef          	jal	ra,ffffffffc0200174 <vcprintf>
    cprintf("\n");
ffffffffc020049e:	00005517          	auipc	a0,0x5
ffffffffc02004a2:	d7250513          	addi	a0,a0,-654 # ffffffffc0205210 <default_pmm_manager+0x530>
ffffffffc02004a6:	cefff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    va_end(ap);

panic_dead:
    intr_disable();
ffffffffc02004aa:	486000ef          	jal	ra,ffffffffc0200930 <intr_disable>
    while (1) {
        kmonitor(NULL);
ffffffffc02004ae:	4501                	li	a0,0
ffffffffc02004b0:	e57ff0ef          	jal	ra,ffffffffc0200306 <kmonitor>
    while (1) {
ffffffffc02004b4:	bfed                	j	ffffffffc02004ae <__panic+0x54>

ffffffffc02004b6 <clock_init>:
 * and then enable IRQ_TIMER.
 * */
void clock_init(void) {
    // divided by 500 when using Spike(2MHz)
    // divided by 100 when using QEMU(10MHz)
    timebase = 1e7 / 100;
ffffffffc02004b6:	67e1                	lui	a5,0x18
ffffffffc02004b8:	6a078793          	addi	a5,a5,1696 # 186a0 <kern_entry-0xffffffffc01e7960>
ffffffffc02004bc:	0000d717          	auipc	a4,0xd
ffffffffc02004c0:	faf73e23          	sd	a5,-68(a4) # ffffffffc020d478 <timebase>
    __asm__ __volatile__("rdtime %0" : "=r"(n));
ffffffffc02004c4:	c0102573          	rdtime	a0
	SBI_CALL_1(SBI_SET_TIMER, stime_value);
ffffffffc02004c8:	4581                	li	a1,0
    ticks = 0;

    cprintf("++ setup timer interrupts\n");
}

void clock_set_next_event(void) { sbi_set_timer(get_cycles() + timebase); }
ffffffffc02004ca:	953e                	add	a0,a0,a5
ffffffffc02004cc:	4601                	li	a2,0
ffffffffc02004ce:	4881                	li	a7,0
ffffffffc02004d0:	00000073          	ecall
    set_csr(sie, MIP_STIP);
ffffffffc02004d4:	02000793          	li	a5,32
ffffffffc02004d8:	1047a7f3          	csrrs	a5,sie,a5
    cprintf("++ setup timer interrupts\n");
ffffffffc02004dc:	00004517          	auipc	a0,0x4
ffffffffc02004e0:	ca450513          	addi	a0,a0,-860 # ffffffffc0204180 <commands+0x68>
    ticks = 0;
ffffffffc02004e4:	0000d797          	auipc	a5,0xd
ffffffffc02004e8:	f807b623          	sd	zero,-116(a5) # ffffffffc020d470 <ticks>
    cprintf("++ setup timer interrupts\n");
ffffffffc02004ec:	b165                	j	ffffffffc0200194 <cprintf>

ffffffffc02004ee <clock_set_next_event>:
    __asm__ __volatile__("rdtime %0" : "=r"(n));
ffffffffc02004ee:	c0102573          	rdtime	a0
void clock_set_next_event(void) { sbi_set_timer(get_cycles() + timebase); }
ffffffffc02004f2:	0000d797          	auipc	a5,0xd
ffffffffc02004f6:	f867b783          	ld	a5,-122(a5) # ffffffffc020d478 <timebase>
ffffffffc02004fa:	953e                	add	a0,a0,a5
ffffffffc02004fc:	4581                	li	a1,0
ffffffffc02004fe:	4601                	li	a2,0
ffffffffc0200500:	4881                	li	a7,0
ffffffffc0200502:	00000073          	ecall
ffffffffc0200506:	8082                	ret

ffffffffc0200508 <cons_init>:

/* serial_intr - try to feed input characters from serial port */
void serial_intr(void) {}

/* cons_init - initializes the console devices */
void cons_init(void) {}
ffffffffc0200508:	8082                	ret

ffffffffc020050a <cons_putc>:
#include <defs.h>
#include <intr.h>
#include <riscv.h>

static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc020050a:	100027f3          	csrr	a5,sstatus
ffffffffc020050e:	8b89                	andi	a5,a5,2
	SBI_CALL_1(SBI_CONSOLE_PUTCHAR, ch);
ffffffffc0200510:	0ff57513          	zext.b	a0,a0
ffffffffc0200514:	e799                	bnez	a5,ffffffffc0200522 <cons_putc+0x18>
ffffffffc0200516:	4581                	li	a1,0
ffffffffc0200518:	4601                	li	a2,0
ffffffffc020051a:	4885                	li	a7,1
ffffffffc020051c:	00000073          	ecall
    }
    return 0;
}

static inline void __intr_restore(bool flag) {
    if (flag) {
ffffffffc0200520:	8082                	ret

/* cons_putc - print a single character @c to console devices */
void cons_putc(int c) {
ffffffffc0200522:	1101                	addi	sp,sp,-32
ffffffffc0200524:	ec06                	sd	ra,24(sp)
ffffffffc0200526:	e42a                	sd	a0,8(sp)
        intr_disable();
ffffffffc0200528:	408000ef          	jal	ra,ffffffffc0200930 <intr_disable>
ffffffffc020052c:	6522                	ld	a0,8(sp)
ffffffffc020052e:	4581                	li	a1,0
ffffffffc0200530:	4601                	li	a2,0
ffffffffc0200532:	4885                	li	a7,1
ffffffffc0200534:	00000073          	ecall
    local_intr_save(intr_flag);
    {
        sbi_console_putchar((unsigned char)c);
    }
    local_intr_restore(intr_flag);
}
ffffffffc0200538:	60e2                	ld	ra,24(sp)
ffffffffc020053a:	6105                	addi	sp,sp,32
        intr_enable();
ffffffffc020053c:	a6fd                	j	ffffffffc020092a <intr_enable>

ffffffffc020053e <cons_getc>:
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc020053e:	100027f3          	csrr	a5,sstatus
ffffffffc0200542:	8b89                	andi	a5,a5,2
ffffffffc0200544:	eb89                	bnez	a5,ffffffffc0200556 <cons_getc+0x18>
	return SBI_CALL_0(SBI_CONSOLE_GETCHAR);
ffffffffc0200546:	4501                	li	a0,0
ffffffffc0200548:	4581                	li	a1,0
ffffffffc020054a:	4601                	li	a2,0
ffffffffc020054c:	4889                	li	a7,2
ffffffffc020054e:	00000073          	ecall
ffffffffc0200552:	2501                	sext.w	a0,a0
    {
        c = sbi_console_getchar();
    }
    local_intr_restore(intr_flag);
    return c;
}
ffffffffc0200554:	8082                	ret
int cons_getc(void) {
ffffffffc0200556:	1101                	addi	sp,sp,-32
ffffffffc0200558:	ec06                	sd	ra,24(sp)
        intr_disable();
ffffffffc020055a:	3d6000ef          	jal	ra,ffffffffc0200930 <intr_disable>
ffffffffc020055e:	4501                	li	a0,0
ffffffffc0200560:	4581                	li	a1,0
ffffffffc0200562:	4601                	li	a2,0
ffffffffc0200564:	4889                	li	a7,2
ffffffffc0200566:	00000073          	ecall
ffffffffc020056a:	2501                	sext.w	a0,a0
ffffffffc020056c:	e42a                	sd	a0,8(sp)
        intr_enable();
ffffffffc020056e:	3bc000ef          	jal	ra,ffffffffc020092a <intr_enable>
}
ffffffffc0200572:	60e2                	ld	ra,24(sp)
ffffffffc0200574:	6522                	ld	a0,8(sp)
ffffffffc0200576:	6105                	addi	sp,sp,32
ffffffffc0200578:	8082                	ret

ffffffffc020057a <dtb_init>:

// 保存解析出的系统物理内存信息
static uint64_t memory_base = 0;
static uint64_t memory_size = 0;

void dtb_init(void) {
ffffffffc020057a:	7119                	addi	sp,sp,-128
    cprintf("DTB Init\n");
ffffffffc020057c:	00004517          	auipc	a0,0x4
ffffffffc0200580:	c2450513          	addi	a0,a0,-988 # ffffffffc02041a0 <commands+0x88>
void dtb_init(void) {
ffffffffc0200584:	fc86                	sd	ra,120(sp)
ffffffffc0200586:	f8a2                	sd	s0,112(sp)
ffffffffc0200588:	e8d2                	sd	s4,80(sp)
ffffffffc020058a:	f4a6                	sd	s1,104(sp)
ffffffffc020058c:	f0ca                	sd	s2,96(sp)
ffffffffc020058e:	ecce                	sd	s3,88(sp)
ffffffffc0200590:	e4d6                	sd	s5,72(sp)
ffffffffc0200592:	e0da                	sd	s6,64(sp)
ffffffffc0200594:	fc5e                	sd	s7,56(sp)
ffffffffc0200596:	f862                	sd	s8,48(sp)
ffffffffc0200598:	f466                	sd	s9,40(sp)
ffffffffc020059a:	f06a                	sd	s10,32(sp)
ffffffffc020059c:	ec6e                	sd	s11,24(sp)
    cprintf("DTB Init\n");
ffffffffc020059e:	bf7ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("HartID: %ld\n", boot_hartid);
ffffffffc02005a2:	00009597          	auipc	a1,0x9
ffffffffc02005a6:	a5e5b583          	ld	a1,-1442(a1) # ffffffffc0209000 <boot_hartid>
ffffffffc02005aa:	00004517          	auipc	a0,0x4
ffffffffc02005ae:	c0650513          	addi	a0,a0,-1018 # ffffffffc02041b0 <commands+0x98>
ffffffffc02005b2:	be3ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("DTB Address: 0x%lx\n", boot_dtb);
ffffffffc02005b6:	00009417          	auipc	s0,0x9
ffffffffc02005ba:	a5240413          	addi	s0,s0,-1454 # ffffffffc0209008 <boot_dtb>
ffffffffc02005be:	600c                	ld	a1,0(s0)
ffffffffc02005c0:	00004517          	auipc	a0,0x4
ffffffffc02005c4:	c0050513          	addi	a0,a0,-1024 # ffffffffc02041c0 <commands+0xa8>
ffffffffc02005c8:	bcdff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    
    if (boot_dtb == 0) {
ffffffffc02005cc:	00043a03          	ld	s4,0(s0)
        cprintf("Error: DTB address is null\n");
ffffffffc02005d0:	00004517          	auipc	a0,0x4
ffffffffc02005d4:	c0850513          	addi	a0,a0,-1016 # ffffffffc02041d8 <commands+0xc0>
    if (boot_dtb == 0) {
ffffffffc02005d8:	120a0463          	beqz	s4,ffffffffc0200700 <dtb_init+0x186>
        return;
    }
    
    // 转换为虚拟地址
    uintptr_t dtb_vaddr = boot_dtb + PHYSICAL_MEMORY_OFFSET;
ffffffffc02005dc:	57f5                	li	a5,-3
ffffffffc02005de:	07fa                	slli	a5,a5,0x1e
ffffffffc02005e0:	00fa0733          	add	a4,s4,a5
    const struct fdt_header *header = (const struct fdt_header *)dtb_vaddr;
    
    // 验证DTB
    uint32_t magic = fdt32_to_cpu(header->magic);
ffffffffc02005e4:	431c                	lw	a5,0(a4)
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc02005e6:	00ff0637          	lui	a2,0xff0
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc02005ea:	6b41                	lui	s6,0x10
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc02005ec:	0087d59b          	srliw	a1,a5,0x8
ffffffffc02005f0:	0187969b          	slliw	a3,a5,0x18
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc02005f4:	0187d51b          	srliw	a0,a5,0x18
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc02005f8:	0105959b          	slliw	a1,a1,0x10
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc02005fc:	0107d79b          	srliw	a5,a5,0x10
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc0200600:	8df1                	and	a1,a1,a2
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc0200602:	8ec9                	or	a3,a3,a0
ffffffffc0200604:	0087979b          	slliw	a5,a5,0x8
ffffffffc0200608:	1b7d                	addi	s6,s6,-1
ffffffffc020060a:	0167f7b3          	and	a5,a5,s6
ffffffffc020060e:	8dd5                	or	a1,a1,a3
ffffffffc0200610:	8ddd                	or	a1,a1,a5
    if (magic != 0xd00dfeed) {
ffffffffc0200612:	d00e07b7          	lui	a5,0xd00e0
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc0200616:	2581                	sext.w	a1,a1
    if (magic != 0xd00dfeed) {
ffffffffc0200618:	eed78793          	addi	a5,a5,-275 # ffffffffd00dfeed <end+0xfed2a09>
ffffffffc020061c:	10f59163          	bne	a1,a5,ffffffffc020071e <dtb_init+0x1a4>
        return;
    }
    
    // 提取内存信息
    uint64_t mem_base, mem_size;
    if (extract_memory_info(dtb_vaddr, header, &mem_base, &mem_size) == 0) {
ffffffffc0200620:	471c                	lw	a5,8(a4)
ffffffffc0200622:	4754                	lw	a3,12(a4)
    int in_memory_node = 0;
ffffffffc0200624:	4c81                	li	s9,0
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc0200626:	0087d59b          	srliw	a1,a5,0x8
ffffffffc020062a:	0086d51b          	srliw	a0,a3,0x8
ffffffffc020062e:	0186941b          	slliw	s0,a3,0x18
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc0200632:	0186d89b          	srliw	a7,a3,0x18
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc0200636:	01879a1b          	slliw	s4,a5,0x18
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc020063a:	0187d81b          	srliw	a6,a5,0x18
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc020063e:	0105151b          	slliw	a0,a0,0x10
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc0200642:	0106d69b          	srliw	a3,a3,0x10
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc0200646:	0105959b          	slliw	a1,a1,0x10
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc020064a:	0107d79b          	srliw	a5,a5,0x10
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc020064e:	8d71                	and	a0,a0,a2
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc0200650:	01146433          	or	s0,s0,a7
ffffffffc0200654:	0086969b          	slliw	a3,a3,0x8
ffffffffc0200658:	010a6a33          	or	s4,s4,a6
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc020065c:	8e6d                	and	a2,a2,a1
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc020065e:	0087979b          	slliw	a5,a5,0x8
ffffffffc0200662:	8c49                	or	s0,s0,a0
ffffffffc0200664:	0166f6b3          	and	a3,a3,s6
ffffffffc0200668:	00ca6a33          	or	s4,s4,a2
ffffffffc020066c:	0167f7b3          	and	a5,a5,s6
ffffffffc0200670:	8c55                	or	s0,s0,a3
ffffffffc0200672:	00fa6a33          	or	s4,s4,a5
    const char *strings_base = (const char *)(dtb_vaddr + strings_offset);
ffffffffc0200676:	1402                	slli	s0,s0,0x20
    const uint32_t *struct_ptr = (const uint32_t *)(dtb_vaddr + struct_offset);
ffffffffc0200678:	1a02                	slli	s4,s4,0x20
    const char *strings_base = (const char *)(dtb_vaddr + strings_offset);
ffffffffc020067a:	9001                	srli	s0,s0,0x20
    const uint32_t *struct_ptr = (const uint32_t *)(dtb_vaddr + struct_offset);
ffffffffc020067c:	020a5a13          	srli	s4,s4,0x20
    const char *strings_base = (const char *)(dtb_vaddr + strings_offset);
ffffffffc0200680:	943a                	add	s0,s0,a4
    const uint32_t *struct_ptr = (const uint32_t *)(dtb_vaddr + struct_offset);
ffffffffc0200682:	9a3a                	add	s4,s4,a4
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc0200684:	00ff0c37          	lui	s8,0xff0
        switch (token) {
ffffffffc0200688:	4b8d                	li	s7,3
                if (in_memory_node && strcmp(prop_name, "reg") == 0 && prop_len >= 16) {
ffffffffc020068a:	00004917          	auipc	s2,0x4
ffffffffc020068e:	b9e90913          	addi	s2,s2,-1122 # ffffffffc0204228 <commands+0x110>
ffffffffc0200692:	49bd                	li	s3,15
        switch (token) {
ffffffffc0200694:	4d91                	li	s11,4
ffffffffc0200696:	4d05                	li	s10,1
                if (strncmp(name, "memory", 6) == 0) {
ffffffffc0200698:	00004497          	auipc	s1,0x4
ffffffffc020069c:	b8848493          	addi	s1,s1,-1144 # ffffffffc0204220 <commands+0x108>
        uint32_t token = fdt32_to_cpu(*struct_ptr++);
ffffffffc02006a0:	000a2703          	lw	a4,0(s4)
ffffffffc02006a4:	004a0a93          	addi	s5,s4,4
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc02006a8:	0087569b          	srliw	a3,a4,0x8
ffffffffc02006ac:	0187179b          	slliw	a5,a4,0x18
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc02006b0:	0187561b          	srliw	a2,a4,0x18
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc02006b4:	0106969b          	slliw	a3,a3,0x10
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc02006b8:	0107571b          	srliw	a4,a4,0x10
ffffffffc02006bc:	8fd1                	or	a5,a5,a2
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc02006be:	0186f6b3          	and	a3,a3,s8
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc02006c2:	0087171b          	slliw	a4,a4,0x8
ffffffffc02006c6:	8fd5                	or	a5,a5,a3
ffffffffc02006c8:	00eb7733          	and	a4,s6,a4
ffffffffc02006cc:	8fd9                	or	a5,a5,a4
ffffffffc02006ce:	2781                	sext.w	a5,a5
        switch (token) {
ffffffffc02006d0:	09778c63          	beq	a5,s7,ffffffffc0200768 <dtb_init+0x1ee>
ffffffffc02006d4:	00fbea63          	bltu	s7,a5,ffffffffc02006e8 <dtb_init+0x16e>
ffffffffc02006d8:	07a78663          	beq	a5,s10,ffffffffc0200744 <dtb_init+0x1ca>
ffffffffc02006dc:	4709                	li	a4,2
ffffffffc02006de:	00e79763          	bne	a5,a4,ffffffffc02006ec <dtb_init+0x172>
ffffffffc02006e2:	4c81                	li	s9,0
ffffffffc02006e4:	8a56                	mv	s4,s5
ffffffffc02006e6:	bf6d                	j	ffffffffc02006a0 <dtb_init+0x126>
ffffffffc02006e8:	ffb78ee3          	beq	a5,s11,ffffffffc02006e4 <dtb_init+0x16a>
        cprintf("  End:  0x%016lx\n", mem_base + mem_size - 1);
        // 保存到全局变量，供 PMM 查询
        memory_base = mem_base;
        memory_size = mem_size;
    } else {
        cprintf("Warning: Could not extract memory info from DTB\n");
ffffffffc02006ec:	00004517          	auipc	a0,0x4
ffffffffc02006f0:	bb450513          	addi	a0,a0,-1100 # ffffffffc02042a0 <commands+0x188>
ffffffffc02006f4:	aa1ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    }
    cprintf("DTB init completed\n");
ffffffffc02006f8:	00004517          	auipc	a0,0x4
ffffffffc02006fc:	be050513          	addi	a0,a0,-1056 # ffffffffc02042d8 <commands+0x1c0>
}
ffffffffc0200700:	7446                	ld	s0,112(sp)
ffffffffc0200702:	70e6                	ld	ra,120(sp)
ffffffffc0200704:	74a6                	ld	s1,104(sp)
ffffffffc0200706:	7906                	ld	s2,96(sp)
ffffffffc0200708:	69e6                	ld	s3,88(sp)
ffffffffc020070a:	6a46                	ld	s4,80(sp)
ffffffffc020070c:	6aa6                	ld	s5,72(sp)
ffffffffc020070e:	6b06                	ld	s6,64(sp)
ffffffffc0200710:	7be2                	ld	s7,56(sp)
ffffffffc0200712:	7c42                	ld	s8,48(sp)
ffffffffc0200714:	7ca2                	ld	s9,40(sp)
ffffffffc0200716:	7d02                	ld	s10,32(sp)
ffffffffc0200718:	6de2                	ld	s11,24(sp)
ffffffffc020071a:	6109                	addi	sp,sp,128
    cprintf("DTB init completed\n");
ffffffffc020071c:	bca5                	j	ffffffffc0200194 <cprintf>
}
ffffffffc020071e:	7446                	ld	s0,112(sp)
ffffffffc0200720:	70e6                	ld	ra,120(sp)
ffffffffc0200722:	74a6                	ld	s1,104(sp)
ffffffffc0200724:	7906                	ld	s2,96(sp)
ffffffffc0200726:	69e6                	ld	s3,88(sp)
ffffffffc0200728:	6a46                	ld	s4,80(sp)
ffffffffc020072a:	6aa6                	ld	s5,72(sp)
ffffffffc020072c:	6b06                	ld	s6,64(sp)
ffffffffc020072e:	7be2                	ld	s7,56(sp)
ffffffffc0200730:	7c42                	ld	s8,48(sp)
ffffffffc0200732:	7ca2                	ld	s9,40(sp)
ffffffffc0200734:	7d02                	ld	s10,32(sp)
ffffffffc0200736:	6de2                	ld	s11,24(sp)
        cprintf("Error: Invalid DTB magic number: 0x%x\n", magic);
ffffffffc0200738:	00004517          	auipc	a0,0x4
ffffffffc020073c:	ac050513          	addi	a0,a0,-1344 # ffffffffc02041f8 <commands+0xe0>
}
ffffffffc0200740:	6109                	addi	sp,sp,128
        cprintf("Error: Invalid DTB magic number: 0x%x\n", magic);
ffffffffc0200742:	bc89                	j	ffffffffc0200194 <cprintf>
                int name_len = strlen(name);
ffffffffc0200744:	8556                	mv	a0,s5
ffffffffc0200746:	678030ef          	jal	ra,ffffffffc0203dbe <strlen>
ffffffffc020074a:	8a2a                	mv	s4,a0
                if (strncmp(name, "memory", 6) == 0) {
ffffffffc020074c:	4619                	li	a2,6
ffffffffc020074e:	85a6                	mv	a1,s1
ffffffffc0200750:	8556                	mv	a0,s5
                int name_len = strlen(name);
ffffffffc0200752:	2a01                	sext.w	s4,s4
                if (strncmp(name, "memory", 6) == 0) {
ffffffffc0200754:	6d0030ef          	jal	ra,ffffffffc0203e24 <strncmp>
ffffffffc0200758:	e111                	bnez	a0,ffffffffc020075c <dtb_init+0x1e2>
                    in_memory_node = 1;
ffffffffc020075a:	4c85                	li	s9,1
                struct_ptr = (const uint32_t *)(((uintptr_t)struct_ptr + name_len + 4) & ~3);
ffffffffc020075c:	0a91                	addi	s5,s5,4
ffffffffc020075e:	9ad2                	add	s5,s5,s4
ffffffffc0200760:	ffcafa93          	andi	s5,s5,-4
        switch (token) {
ffffffffc0200764:	8a56                	mv	s4,s5
ffffffffc0200766:	bf2d                	j	ffffffffc02006a0 <dtb_init+0x126>
                uint32_t prop_len = fdt32_to_cpu(*struct_ptr++);
ffffffffc0200768:	004a2783          	lw	a5,4(s4)
                uint32_t prop_nameoff = fdt32_to_cpu(*struct_ptr++);
ffffffffc020076c:	00ca0693          	addi	a3,s4,12
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc0200770:	0087d71b          	srliw	a4,a5,0x8
ffffffffc0200774:	01879a9b          	slliw	s5,a5,0x18
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc0200778:	0187d61b          	srliw	a2,a5,0x18
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc020077c:	0107171b          	slliw	a4,a4,0x10
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc0200780:	0107d79b          	srliw	a5,a5,0x10
ffffffffc0200784:	00caeab3          	or	s5,s5,a2
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc0200788:	01877733          	and	a4,a4,s8
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc020078c:	0087979b          	slliw	a5,a5,0x8
ffffffffc0200790:	00eaeab3          	or	s5,s5,a4
ffffffffc0200794:	00fb77b3          	and	a5,s6,a5
ffffffffc0200798:	00faeab3          	or	s5,s5,a5
ffffffffc020079c:	2a81                	sext.w	s5,s5
                if (in_memory_node && strcmp(prop_name, "reg") == 0 && prop_len >= 16) {
ffffffffc020079e:	000c9c63          	bnez	s9,ffffffffc02007b6 <dtb_init+0x23c>
                struct_ptr = (const uint32_t *)(((uintptr_t)struct_ptr + prop_len + 3) & ~3);
ffffffffc02007a2:	1a82                	slli	s5,s5,0x20
ffffffffc02007a4:	00368793          	addi	a5,a3,3
ffffffffc02007a8:	020ada93          	srli	s5,s5,0x20
ffffffffc02007ac:	9abe                	add	s5,s5,a5
ffffffffc02007ae:	ffcafa93          	andi	s5,s5,-4
        switch (token) {
ffffffffc02007b2:	8a56                	mv	s4,s5
ffffffffc02007b4:	b5f5                	j	ffffffffc02006a0 <dtb_init+0x126>
                uint32_t prop_nameoff = fdt32_to_cpu(*struct_ptr++);
ffffffffc02007b6:	008a2783          	lw	a5,8(s4)
                if (in_memory_node && strcmp(prop_name, "reg") == 0 && prop_len >= 16) {
ffffffffc02007ba:	85ca                	mv	a1,s2
ffffffffc02007bc:	e436                	sd	a3,8(sp)
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc02007be:	0087d51b          	srliw	a0,a5,0x8
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc02007c2:	0187d61b          	srliw	a2,a5,0x18
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc02007c6:	0187971b          	slliw	a4,a5,0x18
ffffffffc02007ca:	0105151b          	slliw	a0,a0,0x10
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc02007ce:	0107d79b          	srliw	a5,a5,0x10
ffffffffc02007d2:	8f51                	or	a4,a4,a2
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc02007d4:	01857533          	and	a0,a0,s8
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc02007d8:	0087979b          	slliw	a5,a5,0x8
ffffffffc02007dc:	8d59                	or	a0,a0,a4
ffffffffc02007de:	00fb77b3          	and	a5,s6,a5
ffffffffc02007e2:	8d5d                	or	a0,a0,a5
                const char *prop_name = strings_base + prop_nameoff;
ffffffffc02007e4:	1502                	slli	a0,a0,0x20
ffffffffc02007e6:	9101                	srli	a0,a0,0x20
                if (in_memory_node && strcmp(prop_name, "reg") == 0 && prop_len >= 16) {
ffffffffc02007e8:	9522                	add	a0,a0,s0
ffffffffc02007ea:	61c030ef          	jal	ra,ffffffffc0203e06 <strcmp>
ffffffffc02007ee:	66a2                	ld	a3,8(sp)
ffffffffc02007f0:	f94d                	bnez	a0,ffffffffc02007a2 <dtb_init+0x228>
ffffffffc02007f2:	fb59f8e3          	bgeu	s3,s5,ffffffffc02007a2 <dtb_init+0x228>
                    *mem_base = fdt64_to_cpu(reg_data[0]);
ffffffffc02007f6:	00ca3783          	ld	a5,12(s4)
                    *mem_size = fdt64_to_cpu(reg_data[1]);
ffffffffc02007fa:	014a3703          	ld	a4,20(s4)
        cprintf("Physical Memory from DTB:\n");
ffffffffc02007fe:	00004517          	auipc	a0,0x4
ffffffffc0200802:	a3250513          	addi	a0,a0,-1486 # ffffffffc0204230 <commands+0x118>
           fdt32_to_cpu(x >> 32);
ffffffffc0200806:	4207d613          	srai	a2,a5,0x20
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc020080a:	0087d31b          	srliw	t1,a5,0x8
           fdt32_to_cpu(x >> 32);
ffffffffc020080e:	42075593          	srai	a1,a4,0x20
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc0200812:	0187de1b          	srliw	t3,a5,0x18
ffffffffc0200816:	0186581b          	srliw	a6,a2,0x18
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc020081a:	0187941b          	slliw	s0,a5,0x18
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc020081e:	0107d89b          	srliw	a7,a5,0x10
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc0200822:	0187d693          	srli	a3,a5,0x18
ffffffffc0200826:	01861f1b          	slliw	t5,a2,0x18
ffffffffc020082a:	0087579b          	srliw	a5,a4,0x8
ffffffffc020082e:	0103131b          	slliw	t1,t1,0x10
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc0200832:	0106561b          	srliw	a2,a2,0x10
ffffffffc0200836:	010f6f33          	or	t5,t5,a6
ffffffffc020083a:	0187529b          	srliw	t0,a4,0x18
ffffffffc020083e:	0185df9b          	srliw	t6,a1,0x18
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc0200842:	01837333          	and	t1,t1,s8
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc0200846:	01c46433          	or	s0,s0,t3
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc020084a:	0186f6b3          	and	a3,a3,s8
ffffffffc020084e:	01859e1b          	slliw	t3,a1,0x18
ffffffffc0200852:	01871e9b          	slliw	t4,a4,0x18
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc0200856:	0107581b          	srliw	a6,a4,0x10
ffffffffc020085a:	0086161b          	slliw	a2,a2,0x8
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc020085e:	8361                	srli	a4,a4,0x18
ffffffffc0200860:	0107979b          	slliw	a5,a5,0x10
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc0200864:	0105d59b          	srliw	a1,a1,0x10
ffffffffc0200868:	01e6e6b3          	or	a3,a3,t5
ffffffffc020086c:	00cb7633          	and	a2,s6,a2
ffffffffc0200870:	0088181b          	slliw	a6,a6,0x8
ffffffffc0200874:	0085959b          	slliw	a1,a1,0x8
ffffffffc0200878:	00646433          	or	s0,s0,t1
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc020087c:	0187f7b3          	and	a5,a5,s8
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc0200880:	01fe6333          	or	t1,t3,t6
    return ((x & 0xff) << 24) | (((x >> 8) & 0xff) << 16) | 
ffffffffc0200884:	01877c33          	and	s8,a4,s8
           (((x >> 16) & 0xff) << 8) | ((x >> 24) & 0xff);
ffffffffc0200888:	0088989b          	slliw	a7,a7,0x8
ffffffffc020088c:	011b78b3          	and	a7,s6,a7
ffffffffc0200890:	005eeeb3          	or	t4,t4,t0
ffffffffc0200894:	00c6e733          	or	a4,a3,a2
ffffffffc0200898:	006c6c33          	or	s8,s8,t1
ffffffffc020089c:	010b76b3          	and	a3,s6,a6
ffffffffc02008a0:	00bb7b33          	and	s6,s6,a1
ffffffffc02008a4:	01d7e7b3          	or	a5,a5,t4
ffffffffc02008a8:	016c6b33          	or	s6,s8,s6
ffffffffc02008ac:	01146433          	or	s0,s0,a7
ffffffffc02008b0:	8fd5                	or	a5,a5,a3
           fdt32_to_cpu(x >> 32);
ffffffffc02008b2:	1702                	slli	a4,a4,0x20
ffffffffc02008b4:	1b02                	slli	s6,s6,0x20
    return ((uint64_t)fdt32_to_cpu(x & 0xffffffff) << 32) | 
ffffffffc02008b6:	1782                	slli	a5,a5,0x20
           fdt32_to_cpu(x >> 32);
ffffffffc02008b8:	9301                	srli	a4,a4,0x20
    return ((uint64_t)fdt32_to_cpu(x & 0xffffffff) << 32) | 
ffffffffc02008ba:	1402                	slli	s0,s0,0x20
           fdt32_to_cpu(x >> 32);
ffffffffc02008bc:	020b5b13          	srli	s6,s6,0x20
    return ((uint64_t)fdt32_to_cpu(x & 0xffffffff) << 32) | 
ffffffffc02008c0:	0167eb33          	or	s6,a5,s6
ffffffffc02008c4:	8c59                	or	s0,s0,a4
        cprintf("Physical Memory from DTB:\n");
ffffffffc02008c6:	8cfff0ef          	jal	ra,ffffffffc0200194 <cprintf>
        cprintf("  Base: 0x%016lx\n", mem_base);
ffffffffc02008ca:	85a2                	mv	a1,s0
ffffffffc02008cc:	00004517          	auipc	a0,0x4
ffffffffc02008d0:	98450513          	addi	a0,a0,-1660 # ffffffffc0204250 <commands+0x138>
ffffffffc02008d4:	8c1ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
        cprintf("  Size: 0x%016lx (%ld MB)\n", mem_size, mem_size / (1024 * 1024));
ffffffffc02008d8:	014b5613          	srli	a2,s6,0x14
ffffffffc02008dc:	85da                	mv	a1,s6
ffffffffc02008de:	00004517          	auipc	a0,0x4
ffffffffc02008e2:	98a50513          	addi	a0,a0,-1654 # ffffffffc0204268 <commands+0x150>
ffffffffc02008e6:	8afff0ef          	jal	ra,ffffffffc0200194 <cprintf>
        cprintf("  End:  0x%016lx\n", mem_base + mem_size - 1);
ffffffffc02008ea:	008b05b3          	add	a1,s6,s0
ffffffffc02008ee:	15fd                	addi	a1,a1,-1
ffffffffc02008f0:	00004517          	auipc	a0,0x4
ffffffffc02008f4:	99850513          	addi	a0,a0,-1640 # ffffffffc0204288 <commands+0x170>
ffffffffc02008f8:	89dff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("DTB init completed\n");
ffffffffc02008fc:	00004517          	auipc	a0,0x4
ffffffffc0200900:	9dc50513          	addi	a0,a0,-1572 # ffffffffc02042d8 <commands+0x1c0>
        memory_base = mem_base;
ffffffffc0200904:	0000d797          	auipc	a5,0xd
ffffffffc0200908:	b687be23          	sd	s0,-1156(a5) # ffffffffc020d480 <memory_base>
        memory_size = mem_size;
ffffffffc020090c:	0000d797          	auipc	a5,0xd
ffffffffc0200910:	b767be23          	sd	s6,-1156(a5) # ffffffffc020d488 <memory_size>
    cprintf("DTB init completed\n");
ffffffffc0200914:	b3f5                	j	ffffffffc0200700 <dtb_init+0x186>

ffffffffc0200916 <get_memory_base>:

uint64_t get_memory_base(void) {
    return memory_base;
}
ffffffffc0200916:	0000d517          	auipc	a0,0xd
ffffffffc020091a:	b6a53503          	ld	a0,-1174(a0) # ffffffffc020d480 <memory_base>
ffffffffc020091e:	8082                	ret

ffffffffc0200920 <get_memory_size>:

uint64_t get_memory_size(void) {
    return memory_size;
ffffffffc0200920:	0000d517          	auipc	a0,0xd
ffffffffc0200924:	b6853503          	ld	a0,-1176(a0) # ffffffffc020d488 <memory_size>
ffffffffc0200928:	8082                	ret

ffffffffc020092a <intr_enable>:
#include <intr.h>
#include <riscv.h>

/* intr_enable - enable irq interrupt */
void intr_enable(void) { set_csr(sstatus, SSTATUS_SIE); }
ffffffffc020092a:	100167f3          	csrrsi	a5,sstatus,2
ffffffffc020092e:	8082                	ret

ffffffffc0200930 <intr_disable>:

/* intr_disable - disable irq interrupt */
void intr_disable(void) { clear_csr(sstatus, SSTATUS_SIE); }
ffffffffc0200930:	100177f3          	csrrci	a5,sstatus,2
ffffffffc0200934:	8082                	ret

ffffffffc0200936 <pic_init>:
#include <picirq.h>

void pic_enable(unsigned int irq) {}

/* pic_init - initialize the 8259A interrupt controllers */
void pic_init(void) {}
ffffffffc0200936:	8082                	ret

ffffffffc0200938 <idt_init>:
void idt_init(void)
{
    extern void __alltraps(void);
    /* Set sscratch register to 0, indicating to exception vector that we are
     * presently executing in the kernel */
    write_csr(sscratch, 0);
ffffffffc0200938:	14005073          	csrwi	sscratch,0
    /* Set the exception vector address */
    write_csr(stvec, &__alltraps);
ffffffffc020093c:	00000797          	auipc	a5,0x0
ffffffffc0200940:	3bc78793          	addi	a5,a5,956 # ffffffffc0200cf8 <__alltraps>
ffffffffc0200944:	10579073          	csrw	stvec,a5
    /* Allow kernel to access user memory */
    set_csr(sstatus, SSTATUS_SUM);
ffffffffc0200948:	000407b7          	lui	a5,0x40
ffffffffc020094c:	1007a7f3          	csrrs	a5,sstatus,a5
}
ffffffffc0200950:	8082                	ret

ffffffffc0200952 <print_regs>:
    cprintf("  cause    0x%08x\n", tf->cause);
}

void print_regs(struct pushregs *gpr)
{
    cprintf("  zero     0x%08x\n", gpr->zero);
ffffffffc0200952:	610c                	ld	a1,0(a0)
{
ffffffffc0200954:	1141                	addi	sp,sp,-16
ffffffffc0200956:	e022                	sd	s0,0(sp)
ffffffffc0200958:	842a                	mv	s0,a0
    cprintf("  zero     0x%08x\n", gpr->zero);
ffffffffc020095a:	00004517          	auipc	a0,0x4
ffffffffc020095e:	99650513          	addi	a0,a0,-1642 # ffffffffc02042f0 <commands+0x1d8>
{
ffffffffc0200962:	e406                	sd	ra,8(sp)
    cprintf("  zero     0x%08x\n", gpr->zero);
ffffffffc0200964:	831ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  ra       0x%08x\n", gpr->ra);
ffffffffc0200968:	640c                	ld	a1,8(s0)
ffffffffc020096a:	00004517          	auipc	a0,0x4
ffffffffc020096e:	99e50513          	addi	a0,a0,-1634 # ffffffffc0204308 <commands+0x1f0>
ffffffffc0200972:	823ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  sp       0x%08x\n", gpr->sp);
ffffffffc0200976:	680c                	ld	a1,16(s0)
ffffffffc0200978:	00004517          	auipc	a0,0x4
ffffffffc020097c:	9a850513          	addi	a0,a0,-1624 # ffffffffc0204320 <commands+0x208>
ffffffffc0200980:	815ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  gp       0x%08x\n", gpr->gp);
ffffffffc0200984:	6c0c                	ld	a1,24(s0)
ffffffffc0200986:	00004517          	auipc	a0,0x4
ffffffffc020098a:	9b250513          	addi	a0,a0,-1614 # ffffffffc0204338 <commands+0x220>
ffffffffc020098e:	807ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  tp       0x%08x\n", gpr->tp);
ffffffffc0200992:	700c                	ld	a1,32(s0)
ffffffffc0200994:	00004517          	auipc	a0,0x4
ffffffffc0200998:	9bc50513          	addi	a0,a0,-1604 # ffffffffc0204350 <commands+0x238>
ffffffffc020099c:	ff8ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  t0       0x%08x\n", gpr->t0);
ffffffffc02009a0:	740c                	ld	a1,40(s0)
ffffffffc02009a2:	00004517          	auipc	a0,0x4
ffffffffc02009a6:	9c650513          	addi	a0,a0,-1594 # ffffffffc0204368 <commands+0x250>
ffffffffc02009aa:	feaff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  t1       0x%08x\n", gpr->t1);
ffffffffc02009ae:	780c                	ld	a1,48(s0)
ffffffffc02009b0:	00004517          	auipc	a0,0x4
ffffffffc02009b4:	9d050513          	addi	a0,a0,-1584 # ffffffffc0204380 <commands+0x268>
ffffffffc02009b8:	fdcff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  t2       0x%08x\n", gpr->t2);
ffffffffc02009bc:	7c0c                	ld	a1,56(s0)
ffffffffc02009be:	00004517          	auipc	a0,0x4
ffffffffc02009c2:	9da50513          	addi	a0,a0,-1574 # ffffffffc0204398 <commands+0x280>
ffffffffc02009c6:	fceff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  s0       0x%08x\n", gpr->s0);
ffffffffc02009ca:	602c                	ld	a1,64(s0)
ffffffffc02009cc:	00004517          	auipc	a0,0x4
ffffffffc02009d0:	9e450513          	addi	a0,a0,-1564 # ffffffffc02043b0 <commands+0x298>
ffffffffc02009d4:	fc0ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  s1       0x%08x\n", gpr->s1);
ffffffffc02009d8:	642c                	ld	a1,72(s0)
ffffffffc02009da:	00004517          	auipc	a0,0x4
ffffffffc02009de:	9ee50513          	addi	a0,a0,-1554 # ffffffffc02043c8 <commands+0x2b0>
ffffffffc02009e2:	fb2ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  a0       0x%08x\n", gpr->a0);
ffffffffc02009e6:	682c                	ld	a1,80(s0)
ffffffffc02009e8:	00004517          	auipc	a0,0x4
ffffffffc02009ec:	9f850513          	addi	a0,a0,-1544 # ffffffffc02043e0 <commands+0x2c8>
ffffffffc02009f0:	fa4ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  a1       0x%08x\n", gpr->a1);
ffffffffc02009f4:	6c2c                	ld	a1,88(s0)
ffffffffc02009f6:	00004517          	auipc	a0,0x4
ffffffffc02009fa:	a0250513          	addi	a0,a0,-1534 # ffffffffc02043f8 <commands+0x2e0>
ffffffffc02009fe:	f96ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  a2       0x%08x\n", gpr->a2);
ffffffffc0200a02:	702c                	ld	a1,96(s0)
ffffffffc0200a04:	00004517          	auipc	a0,0x4
ffffffffc0200a08:	a0c50513          	addi	a0,a0,-1524 # ffffffffc0204410 <commands+0x2f8>
ffffffffc0200a0c:	f88ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  a3       0x%08x\n", gpr->a3);
ffffffffc0200a10:	742c                	ld	a1,104(s0)
ffffffffc0200a12:	00004517          	auipc	a0,0x4
ffffffffc0200a16:	a1650513          	addi	a0,a0,-1514 # ffffffffc0204428 <commands+0x310>
ffffffffc0200a1a:	f7aff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  a4       0x%08x\n", gpr->a4);
ffffffffc0200a1e:	782c                	ld	a1,112(s0)
ffffffffc0200a20:	00004517          	auipc	a0,0x4
ffffffffc0200a24:	a2050513          	addi	a0,a0,-1504 # ffffffffc0204440 <commands+0x328>
ffffffffc0200a28:	f6cff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  a5       0x%08x\n", gpr->a5);
ffffffffc0200a2c:	7c2c                	ld	a1,120(s0)
ffffffffc0200a2e:	00004517          	auipc	a0,0x4
ffffffffc0200a32:	a2a50513          	addi	a0,a0,-1494 # ffffffffc0204458 <commands+0x340>
ffffffffc0200a36:	f5eff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  a6       0x%08x\n", gpr->a6);
ffffffffc0200a3a:	604c                	ld	a1,128(s0)
ffffffffc0200a3c:	00004517          	auipc	a0,0x4
ffffffffc0200a40:	a3450513          	addi	a0,a0,-1484 # ffffffffc0204470 <commands+0x358>
ffffffffc0200a44:	f50ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  a7       0x%08x\n", gpr->a7);
ffffffffc0200a48:	644c                	ld	a1,136(s0)
ffffffffc0200a4a:	00004517          	auipc	a0,0x4
ffffffffc0200a4e:	a3e50513          	addi	a0,a0,-1474 # ffffffffc0204488 <commands+0x370>
ffffffffc0200a52:	f42ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  s2       0x%08x\n", gpr->s2);
ffffffffc0200a56:	684c                	ld	a1,144(s0)
ffffffffc0200a58:	00004517          	auipc	a0,0x4
ffffffffc0200a5c:	a4850513          	addi	a0,a0,-1464 # ffffffffc02044a0 <commands+0x388>
ffffffffc0200a60:	f34ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  s3       0x%08x\n", gpr->s3);
ffffffffc0200a64:	6c4c                	ld	a1,152(s0)
ffffffffc0200a66:	00004517          	auipc	a0,0x4
ffffffffc0200a6a:	a5250513          	addi	a0,a0,-1454 # ffffffffc02044b8 <commands+0x3a0>
ffffffffc0200a6e:	f26ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  s4       0x%08x\n", gpr->s4);
ffffffffc0200a72:	704c                	ld	a1,160(s0)
ffffffffc0200a74:	00004517          	auipc	a0,0x4
ffffffffc0200a78:	a5c50513          	addi	a0,a0,-1444 # ffffffffc02044d0 <commands+0x3b8>
ffffffffc0200a7c:	f18ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  s5       0x%08x\n", gpr->s5);
ffffffffc0200a80:	744c                	ld	a1,168(s0)
ffffffffc0200a82:	00004517          	auipc	a0,0x4
ffffffffc0200a86:	a6650513          	addi	a0,a0,-1434 # ffffffffc02044e8 <commands+0x3d0>
ffffffffc0200a8a:	f0aff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  s6       0x%08x\n", gpr->s6);
ffffffffc0200a8e:	784c                	ld	a1,176(s0)
ffffffffc0200a90:	00004517          	auipc	a0,0x4
ffffffffc0200a94:	a7050513          	addi	a0,a0,-1424 # ffffffffc0204500 <commands+0x3e8>
ffffffffc0200a98:	efcff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  s7       0x%08x\n", gpr->s7);
ffffffffc0200a9c:	7c4c                	ld	a1,184(s0)
ffffffffc0200a9e:	00004517          	auipc	a0,0x4
ffffffffc0200aa2:	a7a50513          	addi	a0,a0,-1414 # ffffffffc0204518 <commands+0x400>
ffffffffc0200aa6:	eeeff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  s8       0x%08x\n", gpr->s8);
ffffffffc0200aaa:	606c                	ld	a1,192(s0)
ffffffffc0200aac:	00004517          	auipc	a0,0x4
ffffffffc0200ab0:	a8450513          	addi	a0,a0,-1404 # ffffffffc0204530 <commands+0x418>
ffffffffc0200ab4:	ee0ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  s9       0x%08x\n", gpr->s9);
ffffffffc0200ab8:	646c                	ld	a1,200(s0)
ffffffffc0200aba:	00004517          	auipc	a0,0x4
ffffffffc0200abe:	a8e50513          	addi	a0,a0,-1394 # ffffffffc0204548 <commands+0x430>
ffffffffc0200ac2:	ed2ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  s10      0x%08x\n", gpr->s10);
ffffffffc0200ac6:	686c                	ld	a1,208(s0)
ffffffffc0200ac8:	00004517          	auipc	a0,0x4
ffffffffc0200acc:	a9850513          	addi	a0,a0,-1384 # ffffffffc0204560 <commands+0x448>
ffffffffc0200ad0:	ec4ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  s11      0x%08x\n", gpr->s11);
ffffffffc0200ad4:	6c6c                	ld	a1,216(s0)
ffffffffc0200ad6:	00004517          	auipc	a0,0x4
ffffffffc0200ada:	aa250513          	addi	a0,a0,-1374 # ffffffffc0204578 <commands+0x460>
ffffffffc0200ade:	eb6ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  t3       0x%08x\n", gpr->t3);
ffffffffc0200ae2:	706c                	ld	a1,224(s0)
ffffffffc0200ae4:	00004517          	auipc	a0,0x4
ffffffffc0200ae8:	aac50513          	addi	a0,a0,-1364 # ffffffffc0204590 <commands+0x478>
ffffffffc0200aec:	ea8ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  t4       0x%08x\n", gpr->t4);
ffffffffc0200af0:	746c                	ld	a1,232(s0)
ffffffffc0200af2:	00004517          	auipc	a0,0x4
ffffffffc0200af6:	ab650513          	addi	a0,a0,-1354 # ffffffffc02045a8 <commands+0x490>
ffffffffc0200afa:	e9aff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  t5       0x%08x\n", gpr->t5);
ffffffffc0200afe:	786c                	ld	a1,240(s0)
ffffffffc0200b00:	00004517          	auipc	a0,0x4
ffffffffc0200b04:	ac050513          	addi	a0,a0,-1344 # ffffffffc02045c0 <commands+0x4a8>
ffffffffc0200b08:	e8cff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  t6       0x%08x\n", gpr->t6);
ffffffffc0200b0c:	7c6c                	ld	a1,248(s0)
}
ffffffffc0200b0e:	6402                	ld	s0,0(sp)
ffffffffc0200b10:	60a2                	ld	ra,8(sp)
    cprintf("  t6       0x%08x\n", gpr->t6);
ffffffffc0200b12:	00004517          	auipc	a0,0x4
ffffffffc0200b16:	ac650513          	addi	a0,a0,-1338 # ffffffffc02045d8 <commands+0x4c0>
}
ffffffffc0200b1a:	0141                	addi	sp,sp,16
    cprintf("  t6       0x%08x\n", gpr->t6);
ffffffffc0200b1c:	e78ff06f          	j	ffffffffc0200194 <cprintf>

ffffffffc0200b20 <print_trapframe>:
{
ffffffffc0200b20:	1141                	addi	sp,sp,-16
ffffffffc0200b22:	e022                	sd	s0,0(sp)
    cprintf("trapframe at %p\n", tf);
ffffffffc0200b24:	85aa                	mv	a1,a0
{
ffffffffc0200b26:	842a                	mv	s0,a0
    cprintf("trapframe at %p\n", tf);
ffffffffc0200b28:	00004517          	auipc	a0,0x4
ffffffffc0200b2c:	ac850513          	addi	a0,a0,-1336 # ffffffffc02045f0 <commands+0x4d8>
{
ffffffffc0200b30:	e406                	sd	ra,8(sp)
    cprintf("trapframe at %p\n", tf);
ffffffffc0200b32:	e62ff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    print_regs(&tf->gpr);
ffffffffc0200b36:	8522                	mv	a0,s0
ffffffffc0200b38:	e1bff0ef          	jal	ra,ffffffffc0200952 <print_regs>
    cprintf("  status   0x%08x\n", tf->status);
ffffffffc0200b3c:	10043583          	ld	a1,256(s0)
ffffffffc0200b40:	00004517          	auipc	a0,0x4
ffffffffc0200b44:	ac850513          	addi	a0,a0,-1336 # ffffffffc0204608 <commands+0x4f0>
ffffffffc0200b48:	e4cff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  epc      0x%08x\n", tf->epc);
ffffffffc0200b4c:	10843583          	ld	a1,264(s0)
ffffffffc0200b50:	00004517          	auipc	a0,0x4
ffffffffc0200b54:	ad050513          	addi	a0,a0,-1328 # ffffffffc0204620 <commands+0x508>
ffffffffc0200b58:	e3cff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  badvaddr 0x%08x\n", tf->badvaddr);
ffffffffc0200b5c:	11043583          	ld	a1,272(s0)
ffffffffc0200b60:	00004517          	auipc	a0,0x4
ffffffffc0200b64:	ad850513          	addi	a0,a0,-1320 # ffffffffc0204638 <commands+0x520>
ffffffffc0200b68:	e2cff0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("  cause    0x%08x\n", tf->cause);
ffffffffc0200b6c:	11843583          	ld	a1,280(s0)
}
ffffffffc0200b70:	6402                	ld	s0,0(sp)
ffffffffc0200b72:	60a2                	ld	ra,8(sp)
    cprintf("  cause    0x%08x\n", tf->cause);
ffffffffc0200b74:	00004517          	auipc	a0,0x4
ffffffffc0200b78:	adc50513          	addi	a0,a0,-1316 # ffffffffc0204650 <commands+0x538>
}
ffffffffc0200b7c:	0141                	addi	sp,sp,16
    cprintf("  cause    0x%08x\n", tf->cause);
ffffffffc0200b7e:	e16ff06f          	j	ffffffffc0200194 <cprintf>

ffffffffc0200b82 <interrupt_handler>:

extern struct mm_struct *check_mm_struct;

void interrupt_handler(struct trapframe *tf)
{
    intptr_t cause = (tf->cause << 1) >> 1;
ffffffffc0200b82:	11853783          	ld	a5,280(a0)
ffffffffc0200b86:	472d                	li	a4,11
ffffffffc0200b88:	0786                	slli	a5,a5,0x1
ffffffffc0200b8a:	8385                	srli	a5,a5,0x1
ffffffffc0200b8c:	06f76c63          	bltu	a4,a5,ffffffffc0200c04 <interrupt_handler+0x82>
ffffffffc0200b90:	00004717          	auipc	a4,0x4
ffffffffc0200b94:	b8870713          	addi	a4,a4,-1144 # ffffffffc0204718 <commands+0x600>
ffffffffc0200b98:	078a                	slli	a5,a5,0x2
ffffffffc0200b9a:	97ba                	add	a5,a5,a4
ffffffffc0200b9c:	439c                	lw	a5,0(a5)
ffffffffc0200b9e:	97ba                	add	a5,a5,a4
ffffffffc0200ba0:	8782                	jr	a5
        break;
    case IRQ_H_SOFT:
        cprintf("Hypervisor software interrupt\n");
        break;
    case IRQ_M_SOFT:
        cprintf("Machine software interrupt\n");
ffffffffc0200ba2:	00004517          	auipc	a0,0x4
ffffffffc0200ba6:	b2650513          	addi	a0,a0,-1242 # ffffffffc02046c8 <commands+0x5b0>
ffffffffc0200baa:	deaff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Hypervisor software interrupt\n");
ffffffffc0200bae:	00004517          	auipc	a0,0x4
ffffffffc0200bb2:	afa50513          	addi	a0,a0,-1286 # ffffffffc02046a8 <commands+0x590>
ffffffffc0200bb6:	ddeff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("User software interrupt\n");
ffffffffc0200bba:	00004517          	auipc	a0,0x4
ffffffffc0200bbe:	aae50513          	addi	a0,a0,-1362 # ffffffffc0204668 <commands+0x550>
ffffffffc0200bc2:	dd2ff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Supervisor software interrupt\n");
ffffffffc0200bc6:	00004517          	auipc	a0,0x4
ffffffffc0200bca:	ac250513          	addi	a0,a0,-1342 # ffffffffc0204688 <commands+0x570>
ffffffffc0200bce:	dc6ff06f          	j	ffffffffc0200194 <cprintf>
{
ffffffffc0200bd2:	1141                	addi	sp,sp,-16
ffffffffc0200bd4:	e406                	sd	ra,8(sp)
        // In fact, Call sbi_set_timer will clear STIP, or you can clear it
        // directly.
        // clear_csr(sip, SIP_STIP);

        /*LAB3 请补充你在lab3中的代码 */ 
        clock_set_next_event();
ffffffffc0200bd6:	919ff0ef          	jal	ra,ffffffffc02004ee <clock_set_next_event>
        if (++ticks % TICK_NUM == 0) {
ffffffffc0200bda:	0000d697          	auipc	a3,0xd
ffffffffc0200bde:	89668693          	addi	a3,a3,-1898 # ffffffffc020d470 <ticks>
ffffffffc0200be2:	629c                	ld	a5,0(a3)
ffffffffc0200be4:	06400713          	li	a4,100
ffffffffc0200be8:	0785                	addi	a5,a5,1
ffffffffc0200bea:	02e7f733          	remu	a4,a5,a4
ffffffffc0200bee:	e29c                	sd	a5,0(a3)
ffffffffc0200bf0:	cb19                	beqz	a4,ffffffffc0200c06 <interrupt_handler+0x84>
        break;
    default:
        print_trapframe(tf);
        break;
    }
}
ffffffffc0200bf2:	60a2                	ld	ra,8(sp)
ffffffffc0200bf4:	0141                	addi	sp,sp,16
ffffffffc0200bf6:	8082                	ret
        cprintf("Supervisor external interrupt\n");
ffffffffc0200bf8:	00004517          	auipc	a0,0x4
ffffffffc0200bfc:	b0050513          	addi	a0,a0,-1280 # ffffffffc02046f8 <commands+0x5e0>
ffffffffc0200c00:	d94ff06f          	j	ffffffffc0200194 <cprintf>
        print_trapframe(tf);
ffffffffc0200c04:	bf31                	j	ffffffffc0200b20 <print_trapframe>
}
ffffffffc0200c06:	60a2                	ld	ra,8(sp)
    cprintf("%d ticks\n", TICK_NUM);
ffffffffc0200c08:	06400593          	li	a1,100
ffffffffc0200c0c:	00004517          	auipc	a0,0x4
ffffffffc0200c10:	adc50513          	addi	a0,a0,-1316 # ffffffffc02046e8 <commands+0x5d0>
}
ffffffffc0200c14:	0141                	addi	sp,sp,16
    cprintf("%d ticks\n", TICK_NUM);
ffffffffc0200c16:	d7eff06f          	j	ffffffffc0200194 <cprintf>

ffffffffc0200c1a <exception_handler>:

void exception_handler(struct trapframe *tf)
{
    int ret;
    switch (tf->cause)
ffffffffc0200c1a:	11853783          	ld	a5,280(a0)
ffffffffc0200c1e:	473d                	li	a4,15
ffffffffc0200c20:	0cf76563          	bltu	a4,a5,ffffffffc0200cea <exception_handler+0xd0>
ffffffffc0200c24:	00004717          	auipc	a4,0x4
ffffffffc0200c28:	cbc70713          	addi	a4,a4,-836 # ffffffffc02048e0 <commands+0x7c8>
ffffffffc0200c2c:	078a                	slli	a5,a5,0x2
ffffffffc0200c2e:	97ba                	add	a5,a5,a4
ffffffffc0200c30:	439c                	lw	a5,0(a5)
ffffffffc0200c32:	97ba                	add	a5,a5,a4
ffffffffc0200c34:	8782                	jr	a5
        break;
    case CAUSE_LOAD_PAGE_FAULT:
        cprintf("Load page fault\n");
        break;
    case CAUSE_STORE_PAGE_FAULT:
        cprintf("Store/AMO page fault\n");
ffffffffc0200c36:	00004517          	auipc	a0,0x4
ffffffffc0200c3a:	c9250513          	addi	a0,a0,-878 # ffffffffc02048c8 <commands+0x7b0>
ffffffffc0200c3e:	d56ff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Instruction address misaligned\n");
ffffffffc0200c42:	00004517          	auipc	a0,0x4
ffffffffc0200c46:	b0650513          	addi	a0,a0,-1274 # ffffffffc0204748 <commands+0x630>
ffffffffc0200c4a:	d4aff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Instruction access fault\n");
ffffffffc0200c4e:	00004517          	auipc	a0,0x4
ffffffffc0200c52:	b1a50513          	addi	a0,a0,-1254 # ffffffffc0204768 <commands+0x650>
ffffffffc0200c56:	d3eff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Illegal instruction\n");
ffffffffc0200c5a:	00004517          	auipc	a0,0x4
ffffffffc0200c5e:	b2e50513          	addi	a0,a0,-1234 # ffffffffc0204788 <commands+0x670>
ffffffffc0200c62:	d32ff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Breakpoint\n");
ffffffffc0200c66:	00004517          	auipc	a0,0x4
ffffffffc0200c6a:	b3a50513          	addi	a0,a0,-1222 # ffffffffc02047a0 <commands+0x688>
ffffffffc0200c6e:	d26ff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Load address misaligned\n");
ffffffffc0200c72:	00004517          	auipc	a0,0x4
ffffffffc0200c76:	b3e50513          	addi	a0,a0,-1218 # ffffffffc02047b0 <commands+0x698>
ffffffffc0200c7a:	d1aff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Load access fault\n");
ffffffffc0200c7e:	00004517          	auipc	a0,0x4
ffffffffc0200c82:	b5250513          	addi	a0,a0,-1198 # ffffffffc02047d0 <commands+0x6b8>
ffffffffc0200c86:	d0eff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("AMO address misaligned\n");
ffffffffc0200c8a:	00004517          	auipc	a0,0x4
ffffffffc0200c8e:	b5e50513          	addi	a0,a0,-1186 # ffffffffc02047e8 <commands+0x6d0>
ffffffffc0200c92:	d02ff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Store/AMO access fault\n");
ffffffffc0200c96:	00004517          	auipc	a0,0x4
ffffffffc0200c9a:	b6a50513          	addi	a0,a0,-1174 # ffffffffc0204800 <commands+0x6e8>
ffffffffc0200c9e:	cf6ff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Environment call from U-mode\n");
ffffffffc0200ca2:	00004517          	auipc	a0,0x4
ffffffffc0200ca6:	b7650513          	addi	a0,a0,-1162 # ffffffffc0204818 <commands+0x700>
ffffffffc0200caa:	ceaff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Environment call from S-mode\n");
ffffffffc0200cae:	00004517          	auipc	a0,0x4
ffffffffc0200cb2:	b8a50513          	addi	a0,a0,-1142 # ffffffffc0204838 <commands+0x720>
ffffffffc0200cb6:	cdeff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Environment call from H-mode\n");
ffffffffc0200cba:	00004517          	auipc	a0,0x4
ffffffffc0200cbe:	b9e50513          	addi	a0,a0,-1122 # ffffffffc0204858 <commands+0x740>
ffffffffc0200cc2:	cd2ff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Environment call from M-mode\n");
ffffffffc0200cc6:	00004517          	auipc	a0,0x4
ffffffffc0200cca:	bb250513          	addi	a0,a0,-1102 # ffffffffc0204878 <commands+0x760>
ffffffffc0200cce:	cc6ff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Instruction page fault\n");
ffffffffc0200cd2:	00004517          	auipc	a0,0x4
ffffffffc0200cd6:	bc650513          	addi	a0,a0,-1082 # ffffffffc0204898 <commands+0x780>
ffffffffc0200cda:	cbaff06f          	j	ffffffffc0200194 <cprintf>
        cprintf("Load page fault\n");
ffffffffc0200cde:	00004517          	auipc	a0,0x4
ffffffffc0200ce2:	bd250513          	addi	a0,a0,-1070 # ffffffffc02048b0 <commands+0x798>
ffffffffc0200ce6:	caeff06f          	j	ffffffffc0200194 <cprintf>
        break;
    default:
        print_trapframe(tf);
ffffffffc0200cea:	bd1d                	j	ffffffffc0200b20 <print_trapframe>

ffffffffc0200cec <trap>:
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void trap(struct trapframe *tf)
{
    // dispatch based on what type of trap occurred
    if ((intptr_t)tf->cause < 0)
ffffffffc0200cec:	11853783          	ld	a5,280(a0)
ffffffffc0200cf0:	0007c363          	bltz	a5,ffffffffc0200cf6 <trap+0xa>
        interrupt_handler(tf);
    }
    else
    {
        // exceptions
        exception_handler(tf);
ffffffffc0200cf4:	b71d                	j	ffffffffc0200c1a <exception_handler>
        interrupt_handler(tf);
ffffffffc0200cf6:	b571                	j	ffffffffc0200b82 <interrupt_handler>

ffffffffc0200cf8 <__alltraps>:
    LOAD  x2,2*REGBYTES(sp)
    .endm

    .globl __alltraps
__alltraps:
    SAVE_ALL
ffffffffc0200cf8:	14011073          	csrw	sscratch,sp
ffffffffc0200cfc:	712d                	addi	sp,sp,-288
ffffffffc0200cfe:	e406                	sd	ra,8(sp)
ffffffffc0200d00:	ec0e                	sd	gp,24(sp)
ffffffffc0200d02:	f012                	sd	tp,32(sp)
ffffffffc0200d04:	f416                	sd	t0,40(sp)
ffffffffc0200d06:	f81a                	sd	t1,48(sp)
ffffffffc0200d08:	fc1e                	sd	t2,56(sp)
ffffffffc0200d0a:	e0a2                	sd	s0,64(sp)
ffffffffc0200d0c:	e4a6                	sd	s1,72(sp)
ffffffffc0200d0e:	e8aa                	sd	a0,80(sp)
ffffffffc0200d10:	ecae                	sd	a1,88(sp)
ffffffffc0200d12:	f0b2                	sd	a2,96(sp)
ffffffffc0200d14:	f4b6                	sd	a3,104(sp)
ffffffffc0200d16:	f8ba                	sd	a4,112(sp)
ffffffffc0200d18:	fcbe                	sd	a5,120(sp)
ffffffffc0200d1a:	e142                	sd	a6,128(sp)
ffffffffc0200d1c:	e546                	sd	a7,136(sp)
ffffffffc0200d1e:	e94a                	sd	s2,144(sp)
ffffffffc0200d20:	ed4e                	sd	s3,152(sp)
ffffffffc0200d22:	f152                	sd	s4,160(sp)
ffffffffc0200d24:	f556                	sd	s5,168(sp)
ffffffffc0200d26:	f95a                	sd	s6,176(sp)
ffffffffc0200d28:	fd5e                	sd	s7,184(sp)
ffffffffc0200d2a:	e1e2                	sd	s8,192(sp)
ffffffffc0200d2c:	e5e6                	sd	s9,200(sp)
ffffffffc0200d2e:	e9ea                	sd	s10,208(sp)
ffffffffc0200d30:	edee                	sd	s11,216(sp)
ffffffffc0200d32:	f1f2                	sd	t3,224(sp)
ffffffffc0200d34:	f5f6                	sd	t4,232(sp)
ffffffffc0200d36:	f9fa                	sd	t5,240(sp)
ffffffffc0200d38:	fdfe                	sd	t6,248(sp)
ffffffffc0200d3a:	14002473          	csrr	s0,sscratch
ffffffffc0200d3e:	100024f3          	csrr	s1,sstatus
ffffffffc0200d42:	14102973          	csrr	s2,sepc
ffffffffc0200d46:	143029f3          	csrr	s3,stval
ffffffffc0200d4a:	14202a73          	csrr	s4,scause
ffffffffc0200d4e:	e822                	sd	s0,16(sp)
ffffffffc0200d50:	e226                	sd	s1,256(sp)
ffffffffc0200d52:	e64a                	sd	s2,264(sp)
ffffffffc0200d54:	ea4e                	sd	s3,272(sp)
ffffffffc0200d56:	ee52                	sd	s4,280(sp)

    move  a0, sp
ffffffffc0200d58:	850a                	mv	a0,sp
    jal trap
ffffffffc0200d5a:	f93ff0ef          	jal	ra,ffffffffc0200cec <trap>

ffffffffc0200d5e <__trapret>:
    # sp should be the same as before "jal trap"

    .globl __trapret
__trapret:
    RESTORE_ALL
ffffffffc0200d5e:	6492                	ld	s1,256(sp)
ffffffffc0200d60:	6932                	ld	s2,264(sp)
ffffffffc0200d62:	10049073          	csrw	sstatus,s1
ffffffffc0200d66:	14191073          	csrw	sepc,s2
ffffffffc0200d6a:	60a2                	ld	ra,8(sp)
ffffffffc0200d6c:	61e2                	ld	gp,24(sp)
ffffffffc0200d6e:	7202                	ld	tp,32(sp)
ffffffffc0200d70:	72a2                	ld	t0,40(sp)
ffffffffc0200d72:	7342                	ld	t1,48(sp)
ffffffffc0200d74:	73e2                	ld	t2,56(sp)
ffffffffc0200d76:	6406                	ld	s0,64(sp)
ffffffffc0200d78:	64a6                	ld	s1,72(sp)
ffffffffc0200d7a:	6546                	ld	a0,80(sp)
ffffffffc0200d7c:	65e6                	ld	a1,88(sp)
ffffffffc0200d7e:	7606                	ld	a2,96(sp)
ffffffffc0200d80:	76a6                	ld	a3,104(sp)
ffffffffc0200d82:	7746                	ld	a4,112(sp)
ffffffffc0200d84:	77e6                	ld	a5,120(sp)
ffffffffc0200d86:	680a                	ld	a6,128(sp)
ffffffffc0200d88:	68aa                	ld	a7,136(sp)
ffffffffc0200d8a:	694a                	ld	s2,144(sp)
ffffffffc0200d8c:	69ea                	ld	s3,152(sp)
ffffffffc0200d8e:	7a0a                	ld	s4,160(sp)
ffffffffc0200d90:	7aaa                	ld	s5,168(sp)
ffffffffc0200d92:	7b4a                	ld	s6,176(sp)
ffffffffc0200d94:	7bea                	ld	s7,184(sp)
ffffffffc0200d96:	6c0e                	ld	s8,192(sp)
ffffffffc0200d98:	6cae                	ld	s9,200(sp)
ffffffffc0200d9a:	6d4e                	ld	s10,208(sp)
ffffffffc0200d9c:	6dee                	ld	s11,216(sp)
ffffffffc0200d9e:	7e0e                	ld	t3,224(sp)
ffffffffc0200da0:	7eae                	ld	t4,232(sp)
ffffffffc0200da2:	7f4e                	ld	t5,240(sp)
ffffffffc0200da4:	7fee                	ld	t6,248(sp)
ffffffffc0200da6:	6142                	ld	sp,16(sp)
    # go back from supervisor call
    sret
ffffffffc0200da8:	10200073          	sret

ffffffffc0200dac <forkrets>:
 
    .globl forkrets
forkrets:
    # set stack to this new process's trapframe
    move sp, a0
ffffffffc0200dac:	812a                	mv	sp,a0
    j __trapret
ffffffffc0200dae:	bf45                	j	ffffffffc0200d5e <__trapret>
	...

ffffffffc0200db2 <default_init>:
 * list_init - initialize a new entry
 * @elm:        new entry to be initialized
 * */
static inline void
list_init(list_entry_t *elm) {
    elm->prev = elm->next = elm;
ffffffffc0200db2:	00008797          	auipc	a5,0x8
ffffffffc0200db6:	67e78793          	addi	a5,a5,1662 # ffffffffc0209430 <free_area>
ffffffffc0200dba:	e79c                	sd	a5,8(a5)
ffffffffc0200dbc:	e39c                	sd	a5,0(a5)
#define nr_free (free_area.nr_free)

static void
default_init(void) {
    list_init(&free_list);
    nr_free = 0;
ffffffffc0200dbe:	0007a823          	sw	zero,16(a5)
}
ffffffffc0200dc2:	8082                	ret

ffffffffc0200dc4 <default_nr_free_pages>:
}

static size_t
default_nr_free_pages(void) {
    return nr_free;
}
ffffffffc0200dc4:	00008517          	auipc	a0,0x8
ffffffffc0200dc8:	67c56503          	lwu	a0,1660(a0) # ffffffffc0209440 <free_area+0x10>
ffffffffc0200dcc:	8082                	ret

ffffffffc0200dce <default_check>:
}

// LAB2: below code is used to check the first fit allocation algorithm 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
ffffffffc0200dce:	715d                	addi	sp,sp,-80
ffffffffc0200dd0:	e0a2                	sd	s0,64(sp)
 * list_next - get the next entry
 * @listelm:    the list head
 **/
static inline list_entry_t *
list_next(list_entry_t *listelm) {
    return listelm->next;
ffffffffc0200dd2:	00008417          	auipc	s0,0x8
ffffffffc0200dd6:	65e40413          	addi	s0,s0,1630 # ffffffffc0209430 <free_area>
ffffffffc0200dda:	641c                	ld	a5,8(s0)
ffffffffc0200ddc:	e486                	sd	ra,72(sp)
ffffffffc0200dde:	fc26                	sd	s1,56(sp)
ffffffffc0200de0:	f84a                	sd	s2,48(sp)
ffffffffc0200de2:	f44e                	sd	s3,40(sp)
ffffffffc0200de4:	f052                	sd	s4,32(sp)
ffffffffc0200de6:	ec56                	sd	s5,24(sp)
ffffffffc0200de8:	e85a                	sd	s6,16(sp)
ffffffffc0200dea:	e45e                	sd	s7,8(sp)
ffffffffc0200dec:	e062                	sd	s8,0(sp)
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
ffffffffc0200dee:	2a878d63          	beq	a5,s0,ffffffffc02010a8 <default_check+0x2da>
    int count = 0, total = 0;
ffffffffc0200df2:	4481                	li	s1,0
ffffffffc0200df4:	4901                	li	s2,0
 * test_bit - Determine whether a bit is set
 * @nr:     the bit to test
 * @addr:   the address to count from
 * */
static inline bool test_bit(int nr, volatile void *addr) {
    return (((*(volatile unsigned long *)addr) >> nr) & 1);
ffffffffc0200df6:	ff07b703          	ld	a4,-16(a5)
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
ffffffffc0200dfa:	8b09                	andi	a4,a4,2
ffffffffc0200dfc:	2a070a63          	beqz	a4,ffffffffc02010b0 <default_check+0x2e2>
        count ++, total += p->property;
ffffffffc0200e00:	ff87a703          	lw	a4,-8(a5)
ffffffffc0200e04:	679c                	ld	a5,8(a5)
ffffffffc0200e06:	2905                	addiw	s2,s2,1
ffffffffc0200e08:	9cb9                	addw	s1,s1,a4
    while ((le = list_next(le)) != &free_list) {
ffffffffc0200e0a:	fe8796e3          	bne	a5,s0,ffffffffc0200df6 <default_check+0x28>
    }
    assert(total == nr_free_pages());
ffffffffc0200e0e:	89a6                	mv	s3,s1
ffffffffc0200e10:	6db000ef          	jal	ra,ffffffffc0201cea <nr_free_pages>
ffffffffc0200e14:	6f351e63          	bne	a0,s3,ffffffffc0201510 <default_check+0x742>
    assert((p0 = alloc_page()) != NULL);
ffffffffc0200e18:	4505                	li	a0,1
ffffffffc0200e1a:	653000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200e1e:	8aaa                	mv	s5,a0
ffffffffc0200e20:	42050863          	beqz	a0,ffffffffc0201250 <default_check+0x482>
    assert((p1 = alloc_page()) != NULL);
ffffffffc0200e24:	4505                	li	a0,1
ffffffffc0200e26:	647000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200e2a:	89aa                	mv	s3,a0
ffffffffc0200e2c:	70050263          	beqz	a0,ffffffffc0201530 <default_check+0x762>
    assert((p2 = alloc_page()) != NULL);
ffffffffc0200e30:	4505                	li	a0,1
ffffffffc0200e32:	63b000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200e36:	8a2a                	mv	s4,a0
ffffffffc0200e38:	48050c63          	beqz	a0,ffffffffc02012d0 <default_check+0x502>
    assert(p0 != p1 && p0 != p2 && p1 != p2);
ffffffffc0200e3c:	293a8a63          	beq	s5,s3,ffffffffc02010d0 <default_check+0x302>
ffffffffc0200e40:	28aa8863          	beq	s5,a0,ffffffffc02010d0 <default_check+0x302>
ffffffffc0200e44:	28a98663          	beq	s3,a0,ffffffffc02010d0 <default_check+0x302>
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);
ffffffffc0200e48:	000aa783          	lw	a5,0(s5)
ffffffffc0200e4c:	2a079263          	bnez	a5,ffffffffc02010f0 <default_check+0x322>
ffffffffc0200e50:	0009a783          	lw	a5,0(s3)
ffffffffc0200e54:	28079e63          	bnez	a5,ffffffffc02010f0 <default_check+0x322>
ffffffffc0200e58:	411c                	lw	a5,0(a0)
ffffffffc0200e5a:	28079b63          	bnez	a5,ffffffffc02010f0 <default_check+0x322>
extern uint_t va_pa_offset;

static inline ppn_t
page2ppn(struct Page *page)
{
    return page - pages + nbase;
ffffffffc0200e5e:	0000c797          	auipc	a5,0xc
ffffffffc0200e62:	6527b783          	ld	a5,1618(a5) # ffffffffc020d4b0 <pages>
ffffffffc0200e66:	40fa8733          	sub	a4,s5,a5
ffffffffc0200e6a:	00005617          	auipc	a2,0x5
ffffffffc0200e6e:	b8e63603          	ld	a2,-1138(a2) # ffffffffc02059f8 <nbase>
ffffffffc0200e72:	8719                	srai	a4,a4,0x6
ffffffffc0200e74:	9732                	add	a4,a4,a2
    assert(page2pa(p0) < npage * PGSIZE);
ffffffffc0200e76:	0000c697          	auipc	a3,0xc
ffffffffc0200e7a:	6326b683          	ld	a3,1586(a3) # ffffffffc020d4a8 <npage>
ffffffffc0200e7e:	06b2                	slli	a3,a3,0xc
}

static inline uintptr_t
page2pa(struct Page *page)
{
    return page2ppn(page) << PGSHIFT;
ffffffffc0200e80:	0732                	slli	a4,a4,0xc
ffffffffc0200e82:	28d77763          	bgeu	a4,a3,ffffffffc0201110 <default_check+0x342>
    return page - pages + nbase;
ffffffffc0200e86:	40f98733          	sub	a4,s3,a5
ffffffffc0200e8a:	8719                	srai	a4,a4,0x6
ffffffffc0200e8c:	9732                	add	a4,a4,a2
    return page2ppn(page) << PGSHIFT;
ffffffffc0200e8e:	0732                	slli	a4,a4,0xc
    assert(page2pa(p1) < npage * PGSIZE);
ffffffffc0200e90:	4cd77063          	bgeu	a4,a3,ffffffffc0201350 <default_check+0x582>
    return page - pages + nbase;
ffffffffc0200e94:	40f507b3          	sub	a5,a0,a5
ffffffffc0200e98:	8799                	srai	a5,a5,0x6
ffffffffc0200e9a:	97b2                	add	a5,a5,a2
    return page2ppn(page) << PGSHIFT;
ffffffffc0200e9c:	07b2                	slli	a5,a5,0xc
    assert(page2pa(p2) < npage * PGSIZE);
ffffffffc0200e9e:	30d7f963          	bgeu	a5,a3,ffffffffc02011b0 <default_check+0x3e2>
    assert(alloc_page() == NULL);
ffffffffc0200ea2:	4505                	li	a0,1
    list_entry_t free_list_store = free_list;
ffffffffc0200ea4:	00043c03          	ld	s8,0(s0)
ffffffffc0200ea8:	00843b83          	ld	s7,8(s0)
    unsigned int nr_free_store = nr_free;
ffffffffc0200eac:	01042b03          	lw	s6,16(s0)
    elm->prev = elm->next = elm;
ffffffffc0200eb0:	e400                	sd	s0,8(s0)
ffffffffc0200eb2:	e000                	sd	s0,0(s0)
    nr_free = 0;
ffffffffc0200eb4:	00008797          	auipc	a5,0x8
ffffffffc0200eb8:	5807a623          	sw	zero,1420(a5) # ffffffffc0209440 <free_area+0x10>
    assert(alloc_page() == NULL);
ffffffffc0200ebc:	5b1000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200ec0:	2c051863          	bnez	a0,ffffffffc0201190 <default_check+0x3c2>
    free_page(p0);
ffffffffc0200ec4:	4585                	li	a1,1
ffffffffc0200ec6:	8556                	mv	a0,s5
ffffffffc0200ec8:	5e3000ef          	jal	ra,ffffffffc0201caa <free_pages>
    free_page(p1);
ffffffffc0200ecc:	4585                	li	a1,1
ffffffffc0200ece:	854e                	mv	a0,s3
ffffffffc0200ed0:	5db000ef          	jal	ra,ffffffffc0201caa <free_pages>
    free_page(p2);
ffffffffc0200ed4:	4585                	li	a1,1
ffffffffc0200ed6:	8552                	mv	a0,s4
ffffffffc0200ed8:	5d3000ef          	jal	ra,ffffffffc0201caa <free_pages>
    assert(nr_free == 3);
ffffffffc0200edc:	4818                	lw	a4,16(s0)
ffffffffc0200ede:	478d                	li	a5,3
ffffffffc0200ee0:	28f71863          	bne	a4,a5,ffffffffc0201170 <default_check+0x3a2>
    assert((p0 = alloc_page()) != NULL);
ffffffffc0200ee4:	4505                	li	a0,1
ffffffffc0200ee6:	587000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200eea:	89aa                	mv	s3,a0
ffffffffc0200eec:	26050263          	beqz	a0,ffffffffc0201150 <default_check+0x382>
    assert((p1 = alloc_page()) != NULL);
ffffffffc0200ef0:	4505                	li	a0,1
ffffffffc0200ef2:	57b000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200ef6:	8aaa                	mv	s5,a0
ffffffffc0200ef8:	3a050c63          	beqz	a0,ffffffffc02012b0 <default_check+0x4e2>
    assert((p2 = alloc_page()) != NULL);
ffffffffc0200efc:	4505                	li	a0,1
ffffffffc0200efe:	56f000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200f02:	8a2a                	mv	s4,a0
ffffffffc0200f04:	38050663          	beqz	a0,ffffffffc0201290 <default_check+0x4c2>
    assert(alloc_page() == NULL);
ffffffffc0200f08:	4505                	li	a0,1
ffffffffc0200f0a:	563000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200f0e:	36051163          	bnez	a0,ffffffffc0201270 <default_check+0x4a2>
    free_page(p0);
ffffffffc0200f12:	4585                	li	a1,1
ffffffffc0200f14:	854e                	mv	a0,s3
ffffffffc0200f16:	595000ef          	jal	ra,ffffffffc0201caa <free_pages>
    assert(!list_empty(&free_list));
ffffffffc0200f1a:	641c                	ld	a5,8(s0)
ffffffffc0200f1c:	20878a63          	beq	a5,s0,ffffffffc0201130 <default_check+0x362>
    assert((p = alloc_page()) == p0);
ffffffffc0200f20:	4505                	li	a0,1
ffffffffc0200f22:	54b000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200f26:	30a99563          	bne	s3,a0,ffffffffc0201230 <default_check+0x462>
    assert(alloc_page() == NULL);
ffffffffc0200f2a:	4505                	li	a0,1
ffffffffc0200f2c:	541000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200f30:	2e051063          	bnez	a0,ffffffffc0201210 <default_check+0x442>
    assert(nr_free == 0);
ffffffffc0200f34:	481c                	lw	a5,16(s0)
ffffffffc0200f36:	2a079d63          	bnez	a5,ffffffffc02011f0 <default_check+0x422>
    free_page(p);
ffffffffc0200f3a:	854e                	mv	a0,s3
ffffffffc0200f3c:	4585                	li	a1,1
    free_list = free_list_store;
ffffffffc0200f3e:	01843023          	sd	s8,0(s0)
ffffffffc0200f42:	01743423          	sd	s7,8(s0)
    nr_free = nr_free_store;
ffffffffc0200f46:	01642823          	sw	s6,16(s0)
    free_page(p);
ffffffffc0200f4a:	561000ef          	jal	ra,ffffffffc0201caa <free_pages>
    free_page(p1);
ffffffffc0200f4e:	4585                	li	a1,1
ffffffffc0200f50:	8556                	mv	a0,s5
ffffffffc0200f52:	559000ef          	jal	ra,ffffffffc0201caa <free_pages>
    free_page(p2);
ffffffffc0200f56:	4585                	li	a1,1
ffffffffc0200f58:	8552                	mv	a0,s4
ffffffffc0200f5a:	551000ef          	jal	ra,ffffffffc0201caa <free_pages>

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
ffffffffc0200f5e:	4515                	li	a0,5
ffffffffc0200f60:	50d000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200f64:	89aa                	mv	s3,a0
    assert(p0 != NULL);
ffffffffc0200f66:	26050563          	beqz	a0,ffffffffc02011d0 <default_check+0x402>
ffffffffc0200f6a:	651c                	ld	a5,8(a0)
ffffffffc0200f6c:	8385                	srli	a5,a5,0x1
    assert(!PageProperty(p0));
ffffffffc0200f6e:	8b85                	andi	a5,a5,1
ffffffffc0200f70:	54079063          	bnez	a5,ffffffffc02014b0 <default_check+0x6e2>

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);
ffffffffc0200f74:	4505                	li	a0,1
    list_entry_t free_list_store = free_list;
ffffffffc0200f76:	00043b03          	ld	s6,0(s0)
ffffffffc0200f7a:	00843a83          	ld	s5,8(s0)
ffffffffc0200f7e:	e000                	sd	s0,0(s0)
ffffffffc0200f80:	e400                	sd	s0,8(s0)
    assert(alloc_page() == NULL);
ffffffffc0200f82:	4eb000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200f86:	50051563          	bnez	a0,ffffffffc0201490 <default_check+0x6c2>

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
ffffffffc0200f8a:	08098a13          	addi	s4,s3,128
ffffffffc0200f8e:	8552                	mv	a0,s4
ffffffffc0200f90:	458d                	li	a1,3
    unsigned int nr_free_store = nr_free;
ffffffffc0200f92:	01042b83          	lw	s7,16(s0)
    nr_free = 0;
ffffffffc0200f96:	00008797          	auipc	a5,0x8
ffffffffc0200f9a:	4a07a523          	sw	zero,1194(a5) # ffffffffc0209440 <free_area+0x10>
    free_pages(p0 + 2, 3);
ffffffffc0200f9e:	50d000ef          	jal	ra,ffffffffc0201caa <free_pages>
    assert(alloc_pages(4) == NULL);
ffffffffc0200fa2:	4511                	li	a0,4
ffffffffc0200fa4:	4c9000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200fa8:	4c051463          	bnez	a0,ffffffffc0201470 <default_check+0x6a2>
ffffffffc0200fac:	0889b783          	ld	a5,136(s3)
ffffffffc0200fb0:	8385                	srli	a5,a5,0x1
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
ffffffffc0200fb2:	8b85                	andi	a5,a5,1
ffffffffc0200fb4:	48078e63          	beqz	a5,ffffffffc0201450 <default_check+0x682>
ffffffffc0200fb8:	0909a703          	lw	a4,144(s3)
ffffffffc0200fbc:	478d                	li	a5,3
ffffffffc0200fbe:	48f71963          	bne	a4,a5,ffffffffc0201450 <default_check+0x682>
    assert((p1 = alloc_pages(3)) != NULL);
ffffffffc0200fc2:	450d                	li	a0,3
ffffffffc0200fc4:	4a9000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200fc8:	8c2a                	mv	s8,a0
ffffffffc0200fca:	46050363          	beqz	a0,ffffffffc0201430 <default_check+0x662>
    assert(alloc_page() == NULL);
ffffffffc0200fce:	4505                	li	a0,1
ffffffffc0200fd0:	49d000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0200fd4:	42051e63          	bnez	a0,ffffffffc0201410 <default_check+0x642>
    assert(p0 + 2 == p1);
ffffffffc0200fd8:	418a1c63          	bne	s4,s8,ffffffffc02013f0 <default_check+0x622>

    p2 = p0 + 1;
    free_page(p0);
ffffffffc0200fdc:	4585                	li	a1,1
ffffffffc0200fde:	854e                	mv	a0,s3
ffffffffc0200fe0:	4cb000ef          	jal	ra,ffffffffc0201caa <free_pages>
    free_pages(p1, 3);
ffffffffc0200fe4:	458d                	li	a1,3
ffffffffc0200fe6:	8552                	mv	a0,s4
ffffffffc0200fe8:	4c3000ef          	jal	ra,ffffffffc0201caa <free_pages>
ffffffffc0200fec:	0089b783          	ld	a5,8(s3)
    p2 = p0 + 1;
ffffffffc0200ff0:	04098c13          	addi	s8,s3,64
ffffffffc0200ff4:	8385                	srli	a5,a5,0x1
    assert(PageProperty(p0) && p0->property == 1);
ffffffffc0200ff6:	8b85                	andi	a5,a5,1
ffffffffc0200ff8:	3c078c63          	beqz	a5,ffffffffc02013d0 <default_check+0x602>
ffffffffc0200ffc:	0109a703          	lw	a4,16(s3)
ffffffffc0201000:	4785                	li	a5,1
ffffffffc0201002:	3cf71763          	bne	a4,a5,ffffffffc02013d0 <default_check+0x602>
ffffffffc0201006:	008a3783          	ld	a5,8(s4)
ffffffffc020100a:	8385                	srli	a5,a5,0x1
    assert(PageProperty(p1) && p1->property == 3);
ffffffffc020100c:	8b85                	andi	a5,a5,1
ffffffffc020100e:	3a078163          	beqz	a5,ffffffffc02013b0 <default_check+0x5e2>
ffffffffc0201012:	010a2703          	lw	a4,16(s4)
ffffffffc0201016:	478d                	li	a5,3
ffffffffc0201018:	38f71c63          	bne	a4,a5,ffffffffc02013b0 <default_check+0x5e2>

    assert((p0 = alloc_page()) == p2 - 1);
ffffffffc020101c:	4505                	li	a0,1
ffffffffc020101e:	44f000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0201022:	36a99763          	bne	s3,a0,ffffffffc0201390 <default_check+0x5c2>
    free_page(p0);
ffffffffc0201026:	4585                	li	a1,1
ffffffffc0201028:	483000ef          	jal	ra,ffffffffc0201caa <free_pages>
    assert((p0 = alloc_pages(2)) == p2 + 1);
ffffffffc020102c:	4509                	li	a0,2
ffffffffc020102e:	43f000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0201032:	32aa1f63          	bne	s4,a0,ffffffffc0201370 <default_check+0x5a2>

    free_pages(p0, 2);
ffffffffc0201036:	4589                	li	a1,2
ffffffffc0201038:	473000ef          	jal	ra,ffffffffc0201caa <free_pages>
    free_page(p2);
ffffffffc020103c:	4585                	li	a1,1
ffffffffc020103e:	8562                	mv	a0,s8
ffffffffc0201040:	46b000ef          	jal	ra,ffffffffc0201caa <free_pages>

    assert((p0 = alloc_pages(5)) != NULL);
ffffffffc0201044:	4515                	li	a0,5
ffffffffc0201046:	427000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc020104a:	89aa                	mv	s3,a0
ffffffffc020104c:	48050263          	beqz	a0,ffffffffc02014d0 <default_check+0x702>
    assert(alloc_page() == NULL);
ffffffffc0201050:	4505                	li	a0,1
ffffffffc0201052:	41b000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
ffffffffc0201056:	2c051d63          	bnez	a0,ffffffffc0201330 <default_check+0x562>

    assert(nr_free == 0);
ffffffffc020105a:	481c                	lw	a5,16(s0)
ffffffffc020105c:	2a079a63          	bnez	a5,ffffffffc0201310 <default_check+0x542>
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);
ffffffffc0201060:	4595                	li	a1,5
ffffffffc0201062:	854e                	mv	a0,s3
    nr_free = nr_free_store;
ffffffffc0201064:	01742823          	sw	s7,16(s0)
    free_list = free_list_store;
ffffffffc0201068:	01643023          	sd	s6,0(s0)
ffffffffc020106c:	01543423          	sd	s5,8(s0)
    free_pages(p0, 5);
ffffffffc0201070:	43b000ef          	jal	ra,ffffffffc0201caa <free_pages>
    return listelm->next;
ffffffffc0201074:	641c                	ld	a5,8(s0)

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
ffffffffc0201076:	00878963          	beq	a5,s0,ffffffffc0201088 <default_check+0x2ba>
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
ffffffffc020107a:	ff87a703          	lw	a4,-8(a5)
ffffffffc020107e:	679c                	ld	a5,8(a5)
ffffffffc0201080:	397d                	addiw	s2,s2,-1
ffffffffc0201082:	9c99                	subw	s1,s1,a4
    while ((le = list_next(le)) != &free_list) {
ffffffffc0201084:	fe879be3          	bne	a5,s0,ffffffffc020107a <default_check+0x2ac>
    }
    assert(count == 0);
ffffffffc0201088:	26091463          	bnez	s2,ffffffffc02012f0 <default_check+0x522>
    assert(total == 0);
ffffffffc020108c:	46049263          	bnez	s1,ffffffffc02014f0 <default_check+0x722>
}
ffffffffc0201090:	60a6                	ld	ra,72(sp)
ffffffffc0201092:	6406                	ld	s0,64(sp)
ffffffffc0201094:	74e2                	ld	s1,56(sp)
ffffffffc0201096:	7942                	ld	s2,48(sp)
ffffffffc0201098:	79a2                	ld	s3,40(sp)
ffffffffc020109a:	7a02                	ld	s4,32(sp)
ffffffffc020109c:	6ae2                	ld	s5,24(sp)
ffffffffc020109e:	6b42                	ld	s6,16(sp)
ffffffffc02010a0:	6ba2                	ld	s7,8(sp)
ffffffffc02010a2:	6c02                	ld	s8,0(sp)
ffffffffc02010a4:	6161                	addi	sp,sp,80
ffffffffc02010a6:	8082                	ret
    while ((le = list_next(le)) != &free_list) {
ffffffffc02010a8:	4981                	li	s3,0
    int count = 0, total = 0;
ffffffffc02010aa:	4481                	li	s1,0
ffffffffc02010ac:	4901                	li	s2,0
ffffffffc02010ae:	b38d                	j	ffffffffc0200e10 <default_check+0x42>
        assert(PageProperty(p));
ffffffffc02010b0:	00004697          	auipc	a3,0x4
ffffffffc02010b4:	87068693          	addi	a3,a3,-1936 # ffffffffc0204920 <commands+0x808>
ffffffffc02010b8:	00004617          	auipc	a2,0x4
ffffffffc02010bc:	87860613          	addi	a2,a2,-1928 # ffffffffc0204930 <commands+0x818>
ffffffffc02010c0:	0f000593          	li	a1,240
ffffffffc02010c4:	00004517          	auipc	a0,0x4
ffffffffc02010c8:	88450513          	addi	a0,a0,-1916 # ffffffffc0204948 <commands+0x830>
ffffffffc02010cc:	b8eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(p0 != p1 && p0 != p2 && p1 != p2);
ffffffffc02010d0:	00004697          	auipc	a3,0x4
ffffffffc02010d4:	91068693          	addi	a3,a3,-1776 # ffffffffc02049e0 <commands+0x8c8>
ffffffffc02010d8:	00004617          	auipc	a2,0x4
ffffffffc02010dc:	85860613          	addi	a2,a2,-1960 # ffffffffc0204930 <commands+0x818>
ffffffffc02010e0:	0bd00593          	li	a1,189
ffffffffc02010e4:	00004517          	auipc	a0,0x4
ffffffffc02010e8:	86450513          	addi	a0,a0,-1948 # ffffffffc0204948 <commands+0x830>
ffffffffc02010ec:	b6eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);
ffffffffc02010f0:	00004697          	auipc	a3,0x4
ffffffffc02010f4:	91868693          	addi	a3,a3,-1768 # ffffffffc0204a08 <commands+0x8f0>
ffffffffc02010f8:	00004617          	auipc	a2,0x4
ffffffffc02010fc:	83860613          	addi	a2,a2,-1992 # ffffffffc0204930 <commands+0x818>
ffffffffc0201100:	0be00593          	li	a1,190
ffffffffc0201104:	00004517          	auipc	a0,0x4
ffffffffc0201108:	84450513          	addi	a0,a0,-1980 # ffffffffc0204948 <commands+0x830>
ffffffffc020110c:	b4eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page2pa(p0) < npage * PGSIZE);
ffffffffc0201110:	00004697          	auipc	a3,0x4
ffffffffc0201114:	93868693          	addi	a3,a3,-1736 # ffffffffc0204a48 <commands+0x930>
ffffffffc0201118:	00004617          	auipc	a2,0x4
ffffffffc020111c:	81860613          	addi	a2,a2,-2024 # ffffffffc0204930 <commands+0x818>
ffffffffc0201120:	0c000593          	li	a1,192
ffffffffc0201124:	00004517          	auipc	a0,0x4
ffffffffc0201128:	82450513          	addi	a0,a0,-2012 # ffffffffc0204948 <commands+0x830>
ffffffffc020112c:	b2eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(!list_empty(&free_list));
ffffffffc0201130:	00004697          	auipc	a3,0x4
ffffffffc0201134:	9a068693          	addi	a3,a3,-1632 # ffffffffc0204ad0 <commands+0x9b8>
ffffffffc0201138:	00003617          	auipc	a2,0x3
ffffffffc020113c:	7f860613          	addi	a2,a2,2040 # ffffffffc0204930 <commands+0x818>
ffffffffc0201140:	0d900593          	li	a1,217
ffffffffc0201144:	00004517          	auipc	a0,0x4
ffffffffc0201148:	80450513          	addi	a0,a0,-2044 # ffffffffc0204948 <commands+0x830>
ffffffffc020114c:	b0eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((p0 = alloc_page()) != NULL);
ffffffffc0201150:	00004697          	auipc	a3,0x4
ffffffffc0201154:	83068693          	addi	a3,a3,-2000 # ffffffffc0204980 <commands+0x868>
ffffffffc0201158:	00003617          	auipc	a2,0x3
ffffffffc020115c:	7d860613          	addi	a2,a2,2008 # ffffffffc0204930 <commands+0x818>
ffffffffc0201160:	0d200593          	li	a1,210
ffffffffc0201164:	00003517          	auipc	a0,0x3
ffffffffc0201168:	7e450513          	addi	a0,a0,2020 # ffffffffc0204948 <commands+0x830>
ffffffffc020116c:	aeeff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(nr_free == 3);
ffffffffc0201170:	00004697          	auipc	a3,0x4
ffffffffc0201174:	95068693          	addi	a3,a3,-1712 # ffffffffc0204ac0 <commands+0x9a8>
ffffffffc0201178:	00003617          	auipc	a2,0x3
ffffffffc020117c:	7b860613          	addi	a2,a2,1976 # ffffffffc0204930 <commands+0x818>
ffffffffc0201180:	0d000593          	li	a1,208
ffffffffc0201184:	00003517          	auipc	a0,0x3
ffffffffc0201188:	7c450513          	addi	a0,a0,1988 # ffffffffc0204948 <commands+0x830>
ffffffffc020118c:	aceff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(alloc_page() == NULL);
ffffffffc0201190:	00004697          	auipc	a3,0x4
ffffffffc0201194:	91868693          	addi	a3,a3,-1768 # ffffffffc0204aa8 <commands+0x990>
ffffffffc0201198:	00003617          	auipc	a2,0x3
ffffffffc020119c:	79860613          	addi	a2,a2,1944 # ffffffffc0204930 <commands+0x818>
ffffffffc02011a0:	0cb00593          	li	a1,203
ffffffffc02011a4:	00003517          	auipc	a0,0x3
ffffffffc02011a8:	7a450513          	addi	a0,a0,1956 # ffffffffc0204948 <commands+0x830>
ffffffffc02011ac:	aaeff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page2pa(p2) < npage * PGSIZE);
ffffffffc02011b0:	00004697          	auipc	a3,0x4
ffffffffc02011b4:	8d868693          	addi	a3,a3,-1832 # ffffffffc0204a88 <commands+0x970>
ffffffffc02011b8:	00003617          	auipc	a2,0x3
ffffffffc02011bc:	77860613          	addi	a2,a2,1912 # ffffffffc0204930 <commands+0x818>
ffffffffc02011c0:	0c200593          	li	a1,194
ffffffffc02011c4:	00003517          	auipc	a0,0x3
ffffffffc02011c8:	78450513          	addi	a0,a0,1924 # ffffffffc0204948 <commands+0x830>
ffffffffc02011cc:	a8eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(p0 != NULL);
ffffffffc02011d0:	00004697          	auipc	a3,0x4
ffffffffc02011d4:	94868693          	addi	a3,a3,-1720 # ffffffffc0204b18 <commands+0xa00>
ffffffffc02011d8:	00003617          	auipc	a2,0x3
ffffffffc02011dc:	75860613          	addi	a2,a2,1880 # ffffffffc0204930 <commands+0x818>
ffffffffc02011e0:	0f800593          	li	a1,248
ffffffffc02011e4:	00003517          	auipc	a0,0x3
ffffffffc02011e8:	76450513          	addi	a0,a0,1892 # ffffffffc0204948 <commands+0x830>
ffffffffc02011ec:	a6eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(nr_free == 0);
ffffffffc02011f0:	00004697          	auipc	a3,0x4
ffffffffc02011f4:	91868693          	addi	a3,a3,-1768 # ffffffffc0204b08 <commands+0x9f0>
ffffffffc02011f8:	00003617          	auipc	a2,0x3
ffffffffc02011fc:	73860613          	addi	a2,a2,1848 # ffffffffc0204930 <commands+0x818>
ffffffffc0201200:	0df00593          	li	a1,223
ffffffffc0201204:	00003517          	auipc	a0,0x3
ffffffffc0201208:	74450513          	addi	a0,a0,1860 # ffffffffc0204948 <commands+0x830>
ffffffffc020120c:	a4eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(alloc_page() == NULL);
ffffffffc0201210:	00004697          	auipc	a3,0x4
ffffffffc0201214:	89868693          	addi	a3,a3,-1896 # ffffffffc0204aa8 <commands+0x990>
ffffffffc0201218:	00003617          	auipc	a2,0x3
ffffffffc020121c:	71860613          	addi	a2,a2,1816 # ffffffffc0204930 <commands+0x818>
ffffffffc0201220:	0dd00593          	li	a1,221
ffffffffc0201224:	00003517          	auipc	a0,0x3
ffffffffc0201228:	72450513          	addi	a0,a0,1828 # ffffffffc0204948 <commands+0x830>
ffffffffc020122c:	a2eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((p = alloc_page()) == p0);
ffffffffc0201230:	00004697          	auipc	a3,0x4
ffffffffc0201234:	8b868693          	addi	a3,a3,-1864 # ffffffffc0204ae8 <commands+0x9d0>
ffffffffc0201238:	00003617          	auipc	a2,0x3
ffffffffc020123c:	6f860613          	addi	a2,a2,1784 # ffffffffc0204930 <commands+0x818>
ffffffffc0201240:	0dc00593          	li	a1,220
ffffffffc0201244:	00003517          	auipc	a0,0x3
ffffffffc0201248:	70450513          	addi	a0,a0,1796 # ffffffffc0204948 <commands+0x830>
ffffffffc020124c:	a0eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((p0 = alloc_page()) != NULL);
ffffffffc0201250:	00003697          	auipc	a3,0x3
ffffffffc0201254:	73068693          	addi	a3,a3,1840 # ffffffffc0204980 <commands+0x868>
ffffffffc0201258:	00003617          	auipc	a2,0x3
ffffffffc020125c:	6d860613          	addi	a2,a2,1752 # ffffffffc0204930 <commands+0x818>
ffffffffc0201260:	0b900593          	li	a1,185
ffffffffc0201264:	00003517          	auipc	a0,0x3
ffffffffc0201268:	6e450513          	addi	a0,a0,1764 # ffffffffc0204948 <commands+0x830>
ffffffffc020126c:	9eeff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(alloc_page() == NULL);
ffffffffc0201270:	00004697          	auipc	a3,0x4
ffffffffc0201274:	83868693          	addi	a3,a3,-1992 # ffffffffc0204aa8 <commands+0x990>
ffffffffc0201278:	00003617          	auipc	a2,0x3
ffffffffc020127c:	6b860613          	addi	a2,a2,1720 # ffffffffc0204930 <commands+0x818>
ffffffffc0201280:	0d600593          	li	a1,214
ffffffffc0201284:	00003517          	auipc	a0,0x3
ffffffffc0201288:	6c450513          	addi	a0,a0,1732 # ffffffffc0204948 <commands+0x830>
ffffffffc020128c:	9ceff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((p2 = alloc_page()) != NULL);
ffffffffc0201290:	00003697          	auipc	a3,0x3
ffffffffc0201294:	73068693          	addi	a3,a3,1840 # ffffffffc02049c0 <commands+0x8a8>
ffffffffc0201298:	00003617          	auipc	a2,0x3
ffffffffc020129c:	69860613          	addi	a2,a2,1688 # ffffffffc0204930 <commands+0x818>
ffffffffc02012a0:	0d400593          	li	a1,212
ffffffffc02012a4:	00003517          	auipc	a0,0x3
ffffffffc02012a8:	6a450513          	addi	a0,a0,1700 # ffffffffc0204948 <commands+0x830>
ffffffffc02012ac:	9aeff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((p1 = alloc_page()) != NULL);
ffffffffc02012b0:	00003697          	auipc	a3,0x3
ffffffffc02012b4:	6f068693          	addi	a3,a3,1776 # ffffffffc02049a0 <commands+0x888>
ffffffffc02012b8:	00003617          	auipc	a2,0x3
ffffffffc02012bc:	67860613          	addi	a2,a2,1656 # ffffffffc0204930 <commands+0x818>
ffffffffc02012c0:	0d300593          	li	a1,211
ffffffffc02012c4:	00003517          	auipc	a0,0x3
ffffffffc02012c8:	68450513          	addi	a0,a0,1668 # ffffffffc0204948 <commands+0x830>
ffffffffc02012cc:	98eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((p2 = alloc_page()) != NULL);
ffffffffc02012d0:	00003697          	auipc	a3,0x3
ffffffffc02012d4:	6f068693          	addi	a3,a3,1776 # ffffffffc02049c0 <commands+0x8a8>
ffffffffc02012d8:	00003617          	auipc	a2,0x3
ffffffffc02012dc:	65860613          	addi	a2,a2,1624 # ffffffffc0204930 <commands+0x818>
ffffffffc02012e0:	0bb00593          	li	a1,187
ffffffffc02012e4:	00003517          	auipc	a0,0x3
ffffffffc02012e8:	66450513          	addi	a0,a0,1636 # ffffffffc0204948 <commands+0x830>
ffffffffc02012ec:	96eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(count == 0);
ffffffffc02012f0:	00004697          	auipc	a3,0x4
ffffffffc02012f4:	97868693          	addi	a3,a3,-1672 # ffffffffc0204c68 <commands+0xb50>
ffffffffc02012f8:	00003617          	auipc	a2,0x3
ffffffffc02012fc:	63860613          	addi	a2,a2,1592 # ffffffffc0204930 <commands+0x818>
ffffffffc0201300:	12500593          	li	a1,293
ffffffffc0201304:	00003517          	auipc	a0,0x3
ffffffffc0201308:	64450513          	addi	a0,a0,1604 # ffffffffc0204948 <commands+0x830>
ffffffffc020130c:	94eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(nr_free == 0);
ffffffffc0201310:	00003697          	auipc	a3,0x3
ffffffffc0201314:	7f868693          	addi	a3,a3,2040 # ffffffffc0204b08 <commands+0x9f0>
ffffffffc0201318:	00003617          	auipc	a2,0x3
ffffffffc020131c:	61860613          	addi	a2,a2,1560 # ffffffffc0204930 <commands+0x818>
ffffffffc0201320:	11a00593          	li	a1,282
ffffffffc0201324:	00003517          	auipc	a0,0x3
ffffffffc0201328:	62450513          	addi	a0,a0,1572 # ffffffffc0204948 <commands+0x830>
ffffffffc020132c:	92eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(alloc_page() == NULL);
ffffffffc0201330:	00003697          	auipc	a3,0x3
ffffffffc0201334:	77868693          	addi	a3,a3,1912 # ffffffffc0204aa8 <commands+0x990>
ffffffffc0201338:	00003617          	auipc	a2,0x3
ffffffffc020133c:	5f860613          	addi	a2,a2,1528 # ffffffffc0204930 <commands+0x818>
ffffffffc0201340:	11800593          	li	a1,280
ffffffffc0201344:	00003517          	auipc	a0,0x3
ffffffffc0201348:	60450513          	addi	a0,a0,1540 # ffffffffc0204948 <commands+0x830>
ffffffffc020134c:	90eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page2pa(p1) < npage * PGSIZE);
ffffffffc0201350:	00003697          	auipc	a3,0x3
ffffffffc0201354:	71868693          	addi	a3,a3,1816 # ffffffffc0204a68 <commands+0x950>
ffffffffc0201358:	00003617          	auipc	a2,0x3
ffffffffc020135c:	5d860613          	addi	a2,a2,1496 # ffffffffc0204930 <commands+0x818>
ffffffffc0201360:	0c100593          	li	a1,193
ffffffffc0201364:	00003517          	auipc	a0,0x3
ffffffffc0201368:	5e450513          	addi	a0,a0,1508 # ffffffffc0204948 <commands+0x830>
ffffffffc020136c:	8eeff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((p0 = alloc_pages(2)) == p2 + 1);
ffffffffc0201370:	00004697          	auipc	a3,0x4
ffffffffc0201374:	8b868693          	addi	a3,a3,-1864 # ffffffffc0204c28 <commands+0xb10>
ffffffffc0201378:	00003617          	auipc	a2,0x3
ffffffffc020137c:	5b860613          	addi	a2,a2,1464 # ffffffffc0204930 <commands+0x818>
ffffffffc0201380:	11200593          	li	a1,274
ffffffffc0201384:	00003517          	auipc	a0,0x3
ffffffffc0201388:	5c450513          	addi	a0,a0,1476 # ffffffffc0204948 <commands+0x830>
ffffffffc020138c:	8ceff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((p0 = alloc_page()) == p2 - 1);
ffffffffc0201390:	00004697          	auipc	a3,0x4
ffffffffc0201394:	87868693          	addi	a3,a3,-1928 # ffffffffc0204c08 <commands+0xaf0>
ffffffffc0201398:	00003617          	auipc	a2,0x3
ffffffffc020139c:	59860613          	addi	a2,a2,1432 # ffffffffc0204930 <commands+0x818>
ffffffffc02013a0:	11000593          	li	a1,272
ffffffffc02013a4:	00003517          	auipc	a0,0x3
ffffffffc02013a8:	5a450513          	addi	a0,a0,1444 # ffffffffc0204948 <commands+0x830>
ffffffffc02013ac:	8aeff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(PageProperty(p1) && p1->property == 3);
ffffffffc02013b0:	00004697          	auipc	a3,0x4
ffffffffc02013b4:	83068693          	addi	a3,a3,-2000 # ffffffffc0204be0 <commands+0xac8>
ffffffffc02013b8:	00003617          	auipc	a2,0x3
ffffffffc02013bc:	57860613          	addi	a2,a2,1400 # ffffffffc0204930 <commands+0x818>
ffffffffc02013c0:	10e00593          	li	a1,270
ffffffffc02013c4:	00003517          	auipc	a0,0x3
ffffffffc02013c8:	58450513          	addi	a0,a0,1412 # ffffffffc0204948 <commands+0x830>
ffffffffc02013cc:	88eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(PageProperty(p0) && p0->property == 1);
ffffffffc02013d0:	00003697          	auipc	a3,0x3
ffffffffc02013d4:	7e868693          	addi	a3,a3,2024 # ffffffffc0204bb8 <commands+0xaa0>
ffffffffc02013d8:	00003617          	auipc	a2,0x3
ffffffffc02013dc:	55860613          	addi	a2,a2,1368 # ffffffffc0204930 <commands+0x818>
ffffffffc02013e0:	10d00593          	li	a1,269
ffffffffc02013e4:	00003517          	auipc	a0,0x3
ffffffffc02013e8:	56450513          	addi	a0,a0,1380 # ffffffffc0204948 <commands+0x830>
ffffffffc02013ec:	86eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(p0 + 2 == p1);
ffffffffc02013f0:	00003697          	auipc	a3,0x3
ffffffffc02013f4:	7b868693          	addi	a3,a3,1976 # ffffffffc0204ba8 <commands+0xa90>
ffffffffc02013f8:	00003617          	auipc	a2,0x3
ffffffffc02013fc:	53860613          	addi	a2,a2,1336 # ffffffffc0204930 <commands+0x818>
ffffffffc0201400:	10800593          	li	a1,264
ffffffffc0201404:	00003517          	auipc	a0,0x3
ffffffffc0201408:	54450513          	addi	a0,a0,1348 # ffffffffc0204948 <commands+0x830>
ffffffffc020140c:	84eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(alloc_page() == NULL);
ffffffffc0201410:	00003697          	auipc	a3,0x3
ffffffffc0201414:	69868693          	addi	a3,a3,1688 # ffffffffc0204aa8 <commands+0x990>
ffffffffc0201418:	00003617          	auipc	a2,0x3
ffffffffc020141c:	51860613          	addi	a2,a2,1304 # ffffffffc0204930 <commands+0x818>
ffffffffc0201420:	10700593          	li	a1,263
ffffffffc0201424:	00003517          	auipc	a0,0x3
ffffffffc0201428:	52450513          	addi	a0,a0,1316 # ffffffffc0204948 <commands+0x830>
ffffffffc020142c:	82eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((p1 = alloc_pages(3)) != NULL);
ffffffffc0201430:	00003697          	auipc	a3,0x3
ffffffffc0201434:	75868693          	addi	a3,a3,1880 # ffffffffc0204b88 <commands+0xa70>
ffffffffc0201438:	00003617          	auipc	a2,0x3
ffffffffc020143c:	4f860613          	addi	a2,a2,1272 # ffffffffc0204930 <commands+0x818>
ffffffffc0201440:	10600593          	li	a1,262
ffffffffc0201444:	00003517          	auipc	a0,0x3
ffffffffc0201448:	50450513          	addi	a0,a0,1284 # ffffffffc0204948 <commands+0x830>
ffffffffc020144c:	80eff0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
ffffffffc0201450:	00003697          	auipc	a3,0x3
ffffffffc0201454:	70868693          	addi	a3,a3,1800 # ffffffffc0204b58 <commands+0xa40>
ffffffffc0201458:	00003617          	auipc	a2,0x3
ffffffffc020145c:	4d860613          	addi	a2,a2,1240 # ffffffffc0204930 <commands+0x818>
ffffffffc0201460:	10500593          	li	a1,261
ffffffffc0201464:	00003517          	auipc	a0,0x3
ffffffffc0201468:	4e450513          	addi	a0,a0,1252 # ffffffffc0204948 <commands+0x830>
ffffffffc020146c:	feffe0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(alloc_pages(4) == NULL);
ffffffffc0201470:	00003697          	auipc	a3,0x3
ffffffffc0201474:	6d068693          	addi	a3,a3,1744 # ffffffffc0204b40 <commands+0xa28>
ffffffffc0201478:	00003617          	auipc	a2,0x3
ffffffffc020147c:	4b860613          	addi	a2,a2,1208 # ffffffffc0204930 <commands+0x818>
ffffffffc0201480:	10400593          	li	a1,260
ffffffffc0201484:	00003517          	auipc	a0,0x3
ffffffffc0201488:	4c450513          	addi	a0,a0,1220 # ffffffffc0204948 <commands+0x830>
ffffffffc020148c:	fcffe0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(alloc_page() == NULL);
ffffffffc0201490:	00003697          	auipc	a3,0x3
ffffffffc0201494:	61868693          	addi	a3,a3,1560 # ffffffffc0204aa8 <commands+0x990>
ffffffffc0201498:	00003617          	auipc	a2,0x3
ffffffffc020149c:	49860613          	addi	a2,a2,1176 # ffffffffc0204930 <commands+0x818>
ffffffffc02014a0:	0fe00593          	li	a1,254
ffffffffc02014a4:	00003517          	auipc	a0,0x3
ffffffffc02014a8:	4a450513          	addi	a0,a0,1188 # ffffffffc0204948 <commands+0x830>
ffffffffc02014ac:	faffe0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(!PageProperty(p0));
ffffffffc02014b0:	00003697          	auipc	a3,0x3
ffffffffc02014b4:	67868693          	addi	a3,a3,1656 # ffffffffc0204b28 <commands+0xa10>
ffffffffc02014b8:	00003617          	auipc	a2,0x3
ffffffffc02014bc:	47860613          	addi	a2,a2,1144 # ffffffffc0204930 <commands+0x818>
ffffffffc02014c0:	0f900593          	li	a1,249
ffffffffc02014c4:	00003517          	auipc	a0,0x3
ffffffffc02014c8:	48450513          	addi	a0,a0,1156 # ffffffffc0204948 <commands+0x830>
ffffffffc02014cc:	f8ffe0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((p0 = alloc_pages(5)) != NULL);
ffffffffc02014d0:	00003697          	auipc	a3,0x3
ffffffffc02014d4:	77868693          	addi	a3,a3,1912 # ffffffffc0204c48 <commands+0xb30>
ffffffffc02014d8:	00003617          	auipc	a2,0x3
ffffffffc02014dc:	45860613          	addi	a2,a2,1112 # ffffffffc0204930 <commands+0x818>
ffffffffc02014e0:	11700593          	li	a1,279
ffffffffc02014e4:	00003517          	auipc	a0,0x3
ffffffffc02014e8:	46450513          	addi	a0,a0,1124 # ffffffffc0204948 <commands+0x830>
ffffffffc02014ec:	f6ffe0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(total == 0);
ffffffffc02014f0:	00003697          	auipc	a3,0x3
ffffffffc02014f4:	78868693          	addi	a3,a3,1928 # ffffffffc0204c78 <commands+0xb60>
ffffffffc02014f8:	00003617          	auipc	a2,0x3
ffffffffc02014fc:	43860613          	addi	a2,a2,1080 # ffffffffc0204930 <commands+0x818>
ffffffffc0201500:	12600593          	li	a1,294
ffffffffc0201504:	00003517          	auipc	a0,0x3
ffffffffc0201508:	44450513          	addi	a0,a0,1092 # ffffffffc0204948 <commands+0x830>
ffffffffc020150c:	f4ffe0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(total == nr_free_pages());
ffffffffc0201510:	00003697          	auipc	a3,0x3
ffffffffc0201514:	45068693          	addi	a3,a3,1104 # ffffffffc0204960 <commands+0x848>
ffffffffc0201518:	00003617          	auipc	a2,0x3
ffffffffc020151c:	41860613          	addi	a2,a2,1048 # ffffffffc0204930 <commands+0x818>
ffffffffc0201520:	0f300593          	li	a1,243
ffffffffc0201524:	00003517          	auipc	a0,0x3
ffffffffc0201528:	42450513          	addi	a0,a0,1060 # ffffffffc0204948 <commands+0x830>
ffffffffc020152c:	f2ffe0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((p1 = alloc_page()) != NULL);
ffffffffc0201530:	00003697          	auipc	a3,0x3
ffffffffc0201534:	47068693          	addi	a3,a3,1136 # ffffffffc02049a0 <commands+0x888>
ffffffffc0201538:	00003617          	auipc	a2,0x3
ffffffffc020153c:	3f860613          	addi	a2,a2,1016 # ffffffffc0204930 <commands+0x818>
ffffffffc0201540:	0ba00593          	li	a1,186
ffffffffc0201544:	00003517          	auipc	a0,0x3
ffffffffc0201548:	40450513          	addi	a0,a0,1028 # ffffffffc0204948 <commands+0x830>
ffffffffc020154c:	f0ffe0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0201550 <default_free_pages>:
default_free_pages(struct Page *base, size_t n) {
ffffffffc0201550:	1141                	addi	sp,sp,-16
ffffffffc0201552:	e406                	sd	ra,8(sp)
    assert(n > 0);
ffffffffc0201554:	14058463          	beqz	a1,ffffffffc020169c <default_free_pages+0x14c>
    for (; p != base + n; p ++) {
ffffffffc0201558:	00659693          	slli	a3,a1,0x6
ffffffffc020155c:	96aa                	add	a3,a3,a0
ffffffffc020155e:	87aa                	mv	a5,a0
ffffffffc0201560:	02d50263          	beq	a0,a3,ffffffffc0201584 <default_free_pages+0x34>
ffffffffc0201564:	6798                	ld	a4,8(a5)
        assert(!PageReserved(p) && !PageProperty(p));
ffffffffc0201566:	8b05                	andi	a4,a4,1
ffffffffc0201568:	10071a63          	bnez	a4,ffffffffc020167c <default_free_pages+0x12c>
ffffffffc020156c:	6798                	ld	a4,8(a5)
ffffffffc020156e:	8b09                	andi	a4,a4,2
ffffffffc0201570:	10071663          	bnez	a4,ffffffffc020167c <default_free_pages+0x12c>
        p->flags = 0;
ffffffffc0201574:	0007b423          	sd	zero,8(a5)
}

static inline void
set_page_ref(struct Page *page, int val)
{
    page->ref = val;
ffffffffc0201578:	0007a023          	sw	zero,0(a5)
    for (; p != base + n; p ++) {
ffffffffc020157c:	04078793          	addi	a5,a5,64
ffffffffc0201580:	fed792e3          	bne	a5,a3,ffffffffc0201564 <default_free_pages+0x14>
    base->property = n;
ffffffffc0201584:	2581                	sext.w	a1,a1
ffffffffc0201586:	c90c                	sw	a1,16(a0)
    SetPageProperty(base);
ffffffffc0201588:	00850893          	addi	a7,a0,8
    __op_bit(or, __NOP, nr, ((volatile unsigned long *)addr));
ffffffffc020158c:	4789                	li	a5,2
ffffffffc020158e:	40f8b02f          	amoor.d	zero,a5,(a7)
    nr_free += n;
ffffffffc0201592:	00008697          	auipc	a3,0x8
ffffffffc0201596:	e9e68693          	addi	a3,a3,-354 # ffffffffc0209430 <free_area>
ffffffffc020159a:	4a98                	lw	a4,16(a3)
    return list->next == list;
ffffffffc020159c:	669c                	ld	a5,8(a3)
        list_add(&free_list, &(base->page_link));
ffffffffc020159e:	01850613          	addi	a2,a0,24
    nr_free += n;
ffffffffc02015a2:	9db9                	addw	a1,a1,a4
ffffffffc02015a4:	ca8c                	sw	a1,16(a3)
    if (list_empty(&free_list)) {
ffffffffc02015a6:	0ad78463          	beq	a5,a3,ffffffffc020164e <default_free_pages+0xfe>
            struct Page* page = le2page(le, page_link);
ffffffffc02015aa:	fe878713          	addi	a4,a5,-24
ffffffffc02015ae:	0006b803          	ld	a6,0(a3)
    if (list_empty(&free_list)) {
ffffffffc02015b2:	4581                	li	a1,0
            if (base < page) {
ffffffffc02015b4:	00e56a63          	bltu	a0,a4,ffffffffc02015c8 <default_free_pages+0x78>
    return listelm->next;
ffffffffc02015b8:	6798                	ld	a4,8(a5)
            } else if (list_next(le) == &free_list) {
ffffffffc02015ba:	04d70c63          	beq	a4,a3,ffffffffc0201612 <default_free_pages+0xc2>
    for (; p != base + n; p ++) {
ffffffffc02015be:	87ba                	mv	a5,a4
            struct Page* page = le2page(le, page_link);
ffffffffc02015c0:	fe878713          	addi	a4,a5,-24
            if (base < page) {
ffffffffc02015c4:	fee57ae3          	bgeu	a0,a4,ffffffffc02015b8 <default_free_pages+0x68>
ffffffffc02015c8:	c199                	beqz	a1,ffffffffc02015ce <default_free_pages+0x7e>
ffffffffc02015ca:	0106b023          	sd	a6,0(a3)
    __list_add(elm, listelm->prev, listelm);
ffffffffc02015ce:	6398                	ld	a4,0(a5)
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 * */
static inline void
__list_add(list_entry_t *elm, list_entry_t *prev, list_entry_t *next) {
    prev->next = next->prev = elm;
ffffffffc02015d0:	e390                	sd	a2,0(a5)
ffffffffc02015d2:	e710                	sd	a2,8(a4)
    elm->next = next;
ffffffffc02015d4:	f11c                	sd	a5,32(a0)
    elm->prev = prev;
ffffffffc02015d6:	ed18                	sd	a4,24(a0)
    if (le != &free_list) {
ffffffffc02015d8:	00d70d63          	beq	a4,a3,ffffffffc02015f2 <default_free_pages+0xa2>
        if (p + p->property == base) {
ffffffffc02015dc:	ff872583          	lw	a1,-8(a4)
        p = le2page(le, page_link);
ffffffffc02015e0:	fe870613          	addi	a2,a4,-24
        if (p + p->property == base) {
ffffffffc02015e4:	02059813          	slli	a6,a1,0x20
ffffffffc02015e8:	01a85793          	srli	a5,a6,0x1a
ffffffffc02015ec:	97b2                	add	a5,a5,a2
ffffffffc02015ee:	02f50c63          	beq	a0,a5,ffffffffc0201626 <default_free_pages+0xd6>
    return listelm->next;
ffffffffc02015f2:	711c                	ld	a5,32(a0)
    if (le != &free_list) {
ffffffffc02015f4:	00d78c63          	beq	a5,a3,ffffffffc020160c <default_free_pages+0xbc>
        if (base + base->property == p) {
ffffffffc02015f8:	4910                	lw	a2,16(a0)
        p = le2page(le, page_link);
ffffffffc02015fa:	fe878693          	addi	a3,a5,-24
        if (base + base->property == p) {
ffffffffc02015fe:	02061593          	slli	a1,a2,0x20
ffffffffc0201602:	01a5d713          	srli	a4,a1,0x1a
ffffffffc0201606:	972a                	add	a4,a4,a0
ffffffffc0201608:	04e68a63          	beq	a3,a4,ffffffffc020165c <default_free_pages+0x10c>
}
ffffffffc020160c:	60a2                	ld	ra,8(sp)
ffffffffc020160e:	0141                	addi	sp,sp,16
ffffffffc0201610:	8082                	ret
    prev->next = next->prev = elm;
ffffffffc0201612:	e790                	sd	a2,8(a5)
    elm->next = next;
ffffffffc0201614:	f114                	sd	a3,32(a0)
    return listelm->next;
ffffffffc0201616:	6798                	ld	a4,8(a5)
    elm->prev = prev;
ffffffffc0201618:	ed1c                	sd	a5,24(a0)
        while ((le = list_next(le)) != &free_list) {
ffffffffc020161a:	02d70763          	beq	a4,a3,ffffffffc0201648 <default_free_pages+0xf8>
    prev->next = next->prev = elm;
ffffffffc020161e:	8832                	mv	a6,a2
ffffffffc0201620:	4585                	li	a1,1
    for (; p != base + n; p ++) {
ffffffffc0201622:	87ba                	mv	a5,a4
ffffffffc0201624:	bf71                	j	ffffffffc02015c0 <default_free_pages+0x70>
            p->property += base->property;
ffffffffc0201626:	491c                	lw	a5,16(a0)
ffffffffc0201628:	9dbd                	addw	a1,a1,a5
ffffffffc020162a:	feb72c23          	sw	a1,-8(a4)
    __op_bit(and, __NOT, nr, ((volatile unsigned long *)addr));
ffffffffc020162e:	57f5                	li	a5,-3
ffffffffc0201630:	60f8b02f          	amoand.d	zero,a5,(a7)
    __list_del(listelm->prev, listelm->next);
ffffffffc0201634:	01853803          	ld	a6,24(a0)
ffffffffc0201638:	710c                	ld	a1,32(a0)
            base = p;
ffffffffc020163a:	8532                	mv	a0,a2
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 * */
static inline void
__list_del(list_entry_t *prev, list_entry_t *next) {
    prev->next = next;
ffffffffc020163c:	00b83423          	sd	a1,8(a6)
    return listelm->next;
ffffffffc0201640:	671c                	ld	a5,8(a4)
    next->prev = prev;
ffffffffc0201642:	0105b023          	sd	a6,0(a1)
ffffffffc0201646:	b77d                	j	ffffffffc02015f4 <default_free_pages+0xa4>
ffffffffc0201648:	e290                	sd	a2,0(a3)
        while ((le = list_next(le)) != &free_list) {
ffffffffc020164a:	873e                	mv	a4,a5
ffffffffc020164c:	bf41                	j	ffffffffc02015dc <default_free_pages+0x8c>
}
ffffffffc020164e:	60a2                	ld	ra,8(sp)
    prev->next = next->prev = elm;
ffffffffc0201650:	e390                	sd	a2,0(a5)
ffffffffc0201652:	e790                	sd	a2,8(a5)
    elm->next = next;
ffffffffc0201654:	f11c                	sd	a5,32(a0)
    elm->prev = prev;
ffffffffc0201656:	ed1c                	sd	a5,24(a0)
ffffffffc0201658:	0141                	addi	sp,sp,16
ffffffffc020165a:	8082                	ret
            base->property += p->property;
ffffffffc020165c:	ff87a703          	lw	a4,-8(a5)
ffffffffc0201660:	ff078693          	addi	a3,a5,-16
ffffffffc0201664:	9e39                	addw	a2,a2,a4
ffffffffc0201666:	c910                	sw	a2,16(a0)
ffffffffc0201668:	5775                	li	a4,-3
ffffffffc020166a:	60e6b02f          	amoand.d	zero,a4,(a3)
    __list_del(listelm->prev, listelm->next);
ffffffffc020166e:	6398                	ld	a4,0(a5)
ffffffffc0201670:	679c                	ld	a5,8(a5)
}
ffffffffc0201672:	60a2                	ld	ra,8(sp)
    prev->next = next;
ffffffffc0201674:	e71c                	sd	a5,8(a4)
    next->prev = prev;
ffffffffc0201676:	e398                	sd	a4,0(a5)
ffffffffc0201678:	0141                	addi	sp,sp,16
ffffffffc020167a:	8082                	ret
        assert(!PageReserved(p) && !PageProperty(p));
ffffffffc020167c:	00003697          	auipc	a3,0x3
ffffffffc0201680:	61468693          	addi	a3,a3,1556 # ffffffffc0204c90 <commands+0xb78>
ffffffffc0201684:	00003617          	auipc	a2,0x3
ffffffffc0201688:	2ac60613          	addi	a2,a2,684 # ffffffffc0204930 <commands+0x818>
ffffffffc020168c:	08300593          	li	a1,131
ffffffffc0201690:	00003517          	auipc	a0,0x3
ffffffffc0201694:	2b850513          	addi	a0,a0,696 # ffffffffc0204948 <commands+0x830>
ffffffffc0201698:	dc3fe0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(n > 0);
ffffffffc020169c:	00003697          	auipc	a3,0x3
ffffffffc02016a0:	5ec68693          	addi	a3,a3,1516 # ffffffffc0204c88 <commands+0xb70>
ffffffffc02016a4:	00003617          	auipc	a2,0x3
ffffffffc02016a8:	28c60613          	addi	a2,a2,652 # ffffffffc0204930 <commands+0x818>
ffffffffc02016ac:	08000593          	li	a1,128
ffffffffc02016b0:	00003517          	auipc	a0,0x3
ffffffffc02016b4:	29850513          	addi	a0,a0,664 # ffffffffc0204948 <commands+0x830>
ffffffffc02016b8:	da3fe0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc02016bc <default_alloc_pages>:
    assert(n > 0);
ffffffffc02016bc:	c941                	beqz	a0,ffffffffc020174c <default_alloc_pages+0x90>
    if (n > nr_free) {
ffffffffc02016be:	00008597          	auipc	a1,0x8
ffffffffc02016c2:	d7258593          	addi	a1,a1,-654 # ffffffffc0209430 <free_area>
ffffffffc02016c6:	0105a803          	lw	a6,16(a1)
ffffffffc02016ca:	872a                	mv	a4,a0
ffffffffc02016cc:	02081793          	slli	a5,a6,0x20
ffffffffc02016d0:	9381                	srli	a5,a5,0x20
ffffffffc02016d2:	00a7ee63          	bltu	a5,a0,ffffffffc02016ee <default_alloc_pages+0x32>
    list_entry_t *le = &free_list;
ffffffffc02016d6:	87ae                	mv	a5,a1
ffffffffc02016d8:	a801                	j	ffffffffc02016e8 <default_alloc_pages+0x2c>
        if (p->property >= n) {
ffffffffc02016da:	ff87a683          	lw	a3,-8(a5)
ffffffffc02016de:	02069613          	slli	a2,a3,0x20
ffffffffc02016e2:	9201                	srli	a2,a2,0x20
ffffffffc02016e4:	00e67763          	bgeu	a2,a4,ffffffffc02016f2 <default_alloc_pages+0x36>
    return listelm->next;
ffffffffc02016e8:	679c                	ld	a5,8(a5)
    while ((le = list_next(le)) != &free_list) {
ffffffffc02016ea:	feb798e3          	bne	a5,a1,ffffffffc02016da <default_alloc_pages+0x1e>
        return NULL;
ffffffffc02016ee:	4501                	li	a0,0
}
ffffffffc02016f0:	8082                	ret
    return listelm->prev;
ffffffffc02016f2:	0007b883          	ld	a7,0(a5)
    __list_del(listelm->prev, listelm->next);
ffffffffc02016f6:	0087b303          	ld	t1,8(a5)
        struct Page *p = le2page(le, page_link);
ffffffffc02016fa:	fe878513          	addi	a0,a5,-24
            p->property = page->property - n;
ffffffffc02016fe:	00070e1b          	sext.w	t3,a4
    prev->next = next;
ffffffffc0201702:	0068b423          	sd	t1,8(a7)
    next->prev = prev;
ffffffffc0201706:	01133023          	sd	a7,0(t1)
        if (page->property > n) {
ffffffffc020170a:	02c77863          	bgeu	a4,a2,ffffffffc020173a <default_alloc_pages+0x7e>
            struct Page *p = page + n;
ffffffffc020170e:	071a                	slli	a4,a4,0x6
ffffffffc0201710:	972a                	add	a4,a4,a0
            p->property = page->property - n;
ffffffffc0201712:	41c686bb          	subw	a3,a3,t3
ffffffffc0201716:	cb14                	sw	a3,16(a4)
    __op_bit(or, __NOP, nr, ((volatile unsigned long *)addr));
ffffffffc0201718:	00870613          	addi	a2,a4,8
ffffffffc020171c:	4689                	li	a3,2
ffffffffc020171e:	40d6302f          	amoor.d	zero,a3,(a2)
    __list_add(elm, listelm, listelm->next);
ffffffffc0201722:	0088b683          	ld	a3,8(a7)
            list_add(prev, &(p->page_link));
ffffffffc0201726:	01870613          	addi	a2,a4,24
        nr_free -= n;
ffffffffc020172a:	0105a803          	lw	a6,16(a1)
    prev->next = next->prev = elm;
ffffffffc020172e:	e290                	sd	a2,0(a3)
ffffffffc0201730:	00c8b423          	sd	a2,8(a7)
    elm->next = next;
ffffffffc0201734:	f314                	sd	a3,32(a4)
    elm->prev = prev;
ffffffffc0201736:	01173c23          	sd	a7,24(a4)
ffffffffc020173a:	41c8083b          	subw	a6,a6,t3
ffffffffc020173e:	0105a823          	sw	a6,16(a1)
    __op_bit(and, __NOT, nr, ((volatile unsigned long *)addr));
ffffffffc0201742:	5775                	li	a4,-3
ffffffffc0201744:	17c1                	addi	a5,a5,-16
ffffffffc0201746:	60e7b02f          	amoand.d	zero,a4,(a5)
}
ffffffffc020174a:	8082                	ret
default_alloc_pages(size_t n) {
ffffffffc020174c:	1141                	addi	sp,sp,-16
    assert(n > 0);
ffffffffc020174e:	00003697          	auipc	a3,0x3
ffffffffc0201752:	53a68693          	addi	a3,a3,1338 # ffffffffc0204c88 <commands+0xb70>
ffffffffc0201756:	00003617          	auipc	a2,0x3
ffffffffc020175a:	1da60613          	addi	a2,a2,474 # ffffffffc0204930 <commands+0x818>
ffffffffc020175e:	06200593          	li	a1,98
ffffffffc0201762:	00003517          	auipc	a0,0x3
ffffffffc0201766:	1e650513          	addi	a0,a0,486 # ffffffffc0204948 <commands+0x830>
default_alloc_pages(size_t n) {
ffffffffc020176a:	e406                	sd	ra,8(sp)
    assert(n > 0);
ffffffffc020176c:	ceffe0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0201770 <default_init_memmap>:
default_init_memmap(struct Page *base, size_t n) {
ffffffffc0201770:	1141                	addi	sp,sp,-16
ffffffffc0201772:	e406                	sd	ra,8(sp)
    assert(n > 0);
ffffffffc0201774:	c5f1                	beqz	a1,ffffffffc0201840 <default_init_memmap+0xd0>
    for (; p != base + n; p ++) {
ffffffffc0201776:	00659693          	slli	a3,a1,0x6
ffffffffc020177a:	96aa                	add	a3,a3,a0
ffffffffc020177c:	87aa                	mv	a5,a0
ffffffffc020177e:	00d50f63          	beq	a0,a3,ffffffffc020179c <default_init_memmap+0x2c>
    return (((*(volatile unsigned long *)addr) >> nr) & 1);
ffffffffc0201782:	6798                	ld	a4,8(a5)
        assert(PageReserved(p));
ffffffffc0201784:	8b05                	andi	a4,a4,1
ffffffffc0201786:	cf49                	beqz	a4,ffffffffc0201820 <default_init_memmap+0xb0>
        p->flags = p->property = 0;
ffffffffc0201788:	0007a823          	sw	zero,16(a5)
ffffffffc020178c:	0007b423          	sd	zero,8(a5)
ffffffffc0201790:	0007a023          	sw	zero,0(a5)
    for (; p != base + n; p ++) {
ffffffffc0201794:	04078793          	addi	a5,a5,64
ffffffffc0201798:	fed795e3          	bne	a5,a3,ffffffffc0201782 <default_init_memmap+0x12>
    base->property = n;
ffffffffc020179c:	2581                	sext.w	a1,a1
ffffffffc020179e:	c90c                	sw	a1,16(a0)
    __op_bit(or, __NOP, nr, ((volatile unsigned long *)addr));
ffffffffc02017a0:	4789                	li	a5,2
ffffffffc02017a2:	00850713          	addi	a4,a0,8
ffffffffc02017a6:	40f7302f          	amoor.d	zero,a5,(a4)
    nr_free += n;
ffffffffc02017aa:	00008697          	auipc	a3,0x8
ffffffffc02017ae:	c8668693          	addi	a3,a3,-890 # ffffffffc0209430 <free_area>
ffffffffc02017b2:	4a98                	lw	a4,16(a3)
    return list->next == list;
ffffffffc02017b4:	669c                	ld	a5,8(a3)
        list_add(&free_list, &(base->page_link));
ffffffffc02017b6:	01850613          	addi	a2,a0,24
    nr_free += n;
ffffffffc02017ba:	9db9                	addw	a1,a1,a4
ffffffffc02017bc:	ca8c                	sw	a1,16(a3)
    if (list_empty(&free_list)) {
ffffffffc02017be:	04d78a63          	beq	a5,a3,ffffffffc0201812 <default_init_memmap+0xa2>
            struct Page* page = le2page(le, page_link);
ffffffffc02017c2:	fe878713          	addi	a4,a5,-24
ffffffffc02017c6:	0006b803          	ld	a6,0(a3)
    if (list_empty(&free_list)) {
ffffffffc02017ca:	4581                	li	a1,0
            if (base < page) {
ffffffffc02017cc:	00e56a63          	bltu	a0,a4,ffffffffc02017e0 <default_init_memmap+0x70>
    return listelm->next;
ffffffffc02017d0:	6798                	ld	a4,8(a5)
            } else if (list_next(le) == &free_list) {
ffffffffc02017d2:	02d70263          	beq	a4,a3,ffffffffc02017f6 <default_init_memmap+0x86>
    for (; p != base + n; p ++) {
ffffffffc02017d6:	87ba                	mv	a5,a4
            struct Page* page = le2page(le, page_link);
ffffffffc02017d8:	fe878713          	addi	a4,a5,-24
            if (base < page) {
ffffffffc02017dc:	fee57ae3          	bgeu	a0,a4,ffffffffc02017d0 <default_init_memmap+0x60>
ffffffffc02017e0:	c199                	beqz	a1,ffffffffc02017e6 <default_init_memmap+0x76>
ffffffffc02017e2:	0106b023          	sd	a6,0(a3)
    __list_add(elm, listelm->prev, listelm);
ffffffffc02017e6:	6398                	ld	a4,0(a5)
}
ffffffffc02017e8:	60a2                	ld	ra,8(sp)
    prev->next = next->prev = elm;
ffffffffc02017ea:	e390                	sd	a2,0(a5)
ffffffffc02017ec:	e710                	sd	a2,8(a4)
    elm->next = next;
ffffffffc02017ee:	f11c                	sd	a5,32(a0)
    elm->prev = prev;
ffffffffc02017f0:	ed18                	sd	a4,24(a0)
ffffffffc02017f2:	0141                	addi	sp,sp,16
ffffffffc02017f4:	8082                	ret
    prev->next = next->prev = elm;
ffffffffc02017f6:	e790                	sd	a2,8(a5)
    elm->next = next;
ffffffffc02017f8:	f114                	sd	a3,32(a0)
    return listelm->next;
ffffffffc02017fa:	6798                	ld	a4,8(a5)
    elm->prev = prev;
ffffffffc02017fc:	ed1c                	sd	a5,24(a0)
        while ((le = list_next(le)) != &free_list) {
ffffffffc02017fe:	00d70663          	beq	a4,a3,ffffffffc020180a <default_init_memmap+0x9a>
    prev->next = next->prev = elm;
ffffffffc0201802:	8832                	mv	a6,a2
ffffffffc0201804:	4585                	li	a1,1
    for (; p != base + n; p ++) {
ffffffffc0201806:	87ba                	mv	a5,a4
ffffffffc0201808:	bfc1                	j	ffffffffc02017d8 <default_init_memmap+0x68>
}
ffffffffc020180a:	60a2                	ld	ra,8(sp)
ffffffffc020180c:	e290                	sd	a2,0(a3)
ffffffffc020180e:	0141                	addi	sp,sp,16
ffffffffc0201810:	8082                	ret
ffffffffc0201812:	60a2                	ld	ra,8(sp)
ffffffffc0201814:	e390                	sd	a2,0(a5)
ffffffffc0201816:	e790                	sd	a2,8(a5)
    elm->next = next;
ffffffffc0201818:	f11c                	sd	a5,32(a0)
    elm->prev = prev;
ffffffffc020181a:	ed1c                	sd	a5,24(a0)
ffffffffc020181c:	0141                	addi	sp,sp,16
ffffffffc020181e:	8082                	ret
        assert(PageReserved(p));
ffffffffc0201820:	00003697          	auipc	a3,0x3
ffffffffc0201824:	49868693          	addi	a3,a3,1176 # ffffffffc0204cb8 <commands+0xba0>
ffffffffc0201828:	00003617          	auipc	a2,0x3
ffffffffc020182c:	10860613          	addi	a2,a2,264 # ffffffffc0204930 <commands+0x818>
ffffffffc0201830:	04900593          	li	a1,73
ffffffffc0201834:	00003517          	auipc	a0,0x3
ffffffffc0201838:	11450513          	addi	a0,a0,276 # ffffffffc0204948 <commands+0x830>
ffffffffc020183c:	c1ffe0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(n > 0);
ffffffffc0201840:	00003697          	auipc	a3,0x3
ffffffffc0201844:	44868693          	addi	a3,a3,1096 # ffffffffc0204c88 <commands+0xb70>
ffffffffc0201848:	00003617          	auipc	a2,0x3
ffffffffc020184c:	0e860613          	addi	a2,a2,232 # ffffffffc0204930 <commands+0x818>
ffffffffc0201850:	04600593          	li	a1,70
ffffffffc0201854:	00003517          	auipc	a0,0x3
ffffffffc0201858:	0f450513          	addi	a0,a0,244 # ffffffffc0204948 <commands+0x830>
ffffffffc020185c:	bfffe0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0201860 <slob_free>:
static void slob_free(void *block, int size)
{
	slob_t *cur, *b = (slob_t *)block;
	unsigned long flags;

	if (!block)
ffffffffc0201860:	c94d                	beqz	a0,ffffffffc0201912 <slob_free+0xb2>
{
ffffffffc0201862:	1141                	addi	sp,sp,-16
ffffffffc0201864:	e022                	sd	s0,0(sp)
ffffffffc0201866:	e406                	sd	ra,8(sp)
ffffffffc0201868:	842a                	mv	s0,a0
		return;

	if (size)
ffffffffc020186a:	e9c1                	bnez	a1,ffffffffc02018fa <slob_free+0x9a>
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc020186c:	100027f3          	csrr	a5,sstatus
ffffffffc0201870:	8b89                	andi	a5,a5,2
    return 0;
ffffffffc0201872:	4501                	li	a0,0
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc0201874:	ebd9                	bnez	a5,ffffffffc020190a <slob_free+0xaa>
		b->units = SLOB_UNITS(size);

	/* Find reinsertion point */
	spin_lock_irqsave(&slob_lock, flags);
	for (cur = slobfree; !(b > cur && b < cur->next); cur = cur->next)
ffffffffc0201876:	00007617          	auipc	a2,0x7
ffffffffc020187a:	7aa60613          	addi	a2,a2,1962 # ffffffffc0209020 <slobfree>
ffffffffc020187e:	621c                	ld	a5,0(a2)
		if (cur >= cur->next && (b > cur || b < cur->next))
ffffffffc0201880:	873e                	mv	a4,a5
	for (cur = slobfree; !(b > cur && b < cur->next); cur = cur->next)
ffffffffc0201882:	679c                	ld	a5,8(a5)
ffffffffc0201884:	02877a63          	bgeu	a4,s0,ffffffffc02018b8 <slob_free+0x58>
ffffffffc0201888:	00f46463          	bltu	s0,a5,ffffffffc0201890 <slob_free+0x30>
		if (cur >= cur->next && (b > cur || b < cur->next))
ffffffffc020188c:	fef76ae3          	bltu	a4,a5,ffffffffc0201880 <slob_free+0x20>
			break;

	if (b + b->units == cur->next)
ffffffffc0201890:	400c                	lw	a1,0(s0)
ffffffffc0201892:	00459693          	slli	a3,a1,0x4
ffffffffc0201896:	96a2                	add	a3,a3,s0
ffffffffc0201898:	02d78a63          	beq	a5,a3,ffffffffc02018cc <slob_free+0x6c>
		b->next = cur->next->next;
	}
	else
		b->next = cur->next;

	if (cur + cur->units == b)
ffffffffc020189c:	4314                	lw	a3,0(a4)
		b->next = cur->next;
ffffffffc020189e:	e41c                	sd	a5,8(s0)
	if (cur + cur->units == b)
ffffffffc02018a0:	00469793          	slli	a5,a3,0x4
ffffffffc02018a4:	97ba                	add	a5,a5,a4
ffffffffc02018a6:	02f40e63          	beq	s0,a5,ffffffffc02018e2 <slob_free+0x82>
	{
		cur->units += b->units;
		cur->next = b->next;
	}
	else
		cur->next = b;
ffffffffc02018aa:	e700                	sd	s0,8(a4)

	slobfree = cur;
ffffffffc02018ac:	e218                	sd	a4,0(a2)
    if (flag) {
ffffffffc02018ae:	e129                	bnez	a0,ffffffffc02018f0 <slob_free+0x90>

	spin_unlock_irqrestore(&slob_lock, flags);
}
ffffffffc02018b0:	60a2                	ld	ra,8(sp)
ffffffffc02018b2:	6402                	ld	s0,0(sp)
ffffffffc02018b4:	0141                	addi	sp,sp,16
ffffffffc02018b6:	8082                	ret
		if (cur >= cur->next && (b > cur || b < cur->next))
ffffffffc02018b8:	fcf764e3          	bltu	a4,a5,ffffffffc0201880 <slob_free+0x20>
ffffffffc02018bc:	fcf472e3          	bgeu	s0,a5,ffffffffc0201880 <slob_free+0x20>
	if (b + b->units == cur->next)
ffffffffc02018c0:	400c                	lw	a1,0(s0)
ffffffffc02018c2:	00459693          	slli	a3,a1,0x4
ffffffffc02018c6:	96a2                	add	a3,a3,s0
ffffffffc02018c8:	fcd79ae3          	bne	a5,a3,ffffffffc020189c <slob_free+0x3c>
		b->units += cur->next->units;
ffffffffc02018cc:	4394                	lw	a3,0(a5)
		b->next = cur->next->next;
ffffffffc02018ce:	679c                	ld	a5,8(a5)
		b->units += cur->next->units;
ffffffffc02018d0:	9db5                	addw	a1,a1,a3
ffffffffc02018d2:	c00c                	sw	a1,0(s0)
	if (cur + cur->units == b)
ffffffffc02018d4:	4314                	lw	a3,0(a4)
		b->next = cur->next->next;
ffffffffc02018d6:	e41c                	sd	a5,8(s0)
	if (cur + cur->units == b)
ffffffffc02018d8:	00469793          	slli	a5,a3,0x4
ffffffffc02018dc:	97ba                	add	a5,a5,a4
ffffffffc02018de:	fcf416e3          	bne	s0,a5,ffffffffc02018aa <slob_free+0x4a>
		cur->units += b->units;
ffffffffc02018e2:	401c                	lw	a5,0(s0)
		cur->next = b->next;
ffffffffc02018e4:	640c                	ld	a1,8(s0)
	slobfree = cur;
ffffffffc02018e6:	e218                	sd	a4,0(a2)
		cur->units += b->units;
ffffffffc02018e8:	9ebd                	addw	a3,a3,a5
ffffffffc02018ea:	c314                	sw	a3,0(a4)
		cur->next = b->next;
ffffffffc02018ec:	e70c                	sd	a1,8(a4)
ffffffffc02018ee:	d169                	beqz	a0,ffffffffc02018b0 <slob_free+0x50>
}
ffffffffc02018f0:	6402                	ld	s0,0(sp)
ffffffffc02018f2:	60a2                	ld	ra,8(sp)
ffffffffc02018f4:	0141                	addi	sp,sp,16
        intr_enable();
ffffffffc02018f6:	834ff06f          	j	ffffffffc020092a <intr_enable>
		b->units = SLOB_UNITS(size);
ffffffffc02018fa:	25bd                	addiw	a1,a1,15
ffffffffc02018fc:	8191                	srli	a1,a1,0x4
ffffffffc02018fe:	c10c                	sw	a1,0(a0)
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc0201900:	100027f3          	csrr	a5,sstatus
ffffffffc0201904:	8b89                	andi	a5,a5,2
    return 0;
ffffffffc0201906:	4501                	li	a0,0
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc0201908:	d7bd                	beqz	a5,ffffffffc0201876 <slob_free+0x16>
        intr_disable();
ffffffffc020190a:	826ff0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        return 1;
ffffffffc020190e:	4505                	li	a0,1
ffffffffc0201910:	b79d                	j	ffffffffc0201876 <slob_free+0x16>
ffffffffc0201912:	8082                	ret

ffffffffc0201914 <__slob_get_free_pages.constprop.0>:
	struct Page *page = alloc_pages(1 << order);
ffffffffc0201914:	4785                	li	a5,1
static void *__slob_get_free_pages(gfp_t gfp, int order)
ffffffffc0201916:	1141                	addi	sp,sp,-16
	struct Page *page = alloc_pages(1 << order);
ffffffffc0201918:	00a7953b          	sllw	a0,a5,a0
static void *__slob_get_free_pages(gfp_t gfp, int order)
ffffffffc020191c:	e406                	sd	ra,8(sp)
	struct Page *page = alloc_pages(1 << order);
ffffffffc020191e:	34e000ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
	if (!page)
ffffffffc0201922:	c91d                	beqz	a0,ffffffffc0201958 <__slob_get_free_pages.constprop.0+0x44>
    return page - pages + nbase;
ffffffffc0201924:	0000c697          	auipc	a3,0xc
ffffffffc0201928:	b8c6b683          	ld	a3,-1140(a3) # ffffffffc020d4b0 <pages>
ffffffffc020192c:	8d15                	sub	a0,a0,a3
ffffffffc020192e:	8519                	srai	a0,a0,0x6
ffffffffc0201930:	00004697          	auipc	a3,0x4
ffffffffc0201934:	0c86b683          	ld	a3,200(a3) # ffffffffc02059f8 <nbase>
ffffffffc0201938:	9536                	add	a0,a0,a3
    return KADDR(page2pa(page));
ffffffffc020193a:	00c51793          	slli	a5,a0,0xc
ffffffffc020193e:	83b1                	srli	a5,a5,0xc
ffffffffc0201940:	0000c717          	auipc	a4,0xc
ffffffffc0201944:	b6873703          	ld	a4,-1176(a4) # ffffffffc020d4a8 <npage>
    return page2ppn(page) << PGSHIFT;
ffffffffc0201948:	0532                	slli	a0,a0,0xc
    return KADDR(page2pa(page));
ffffffffc020194a:	00e7fa63          	bgeu	a5,a4,ffffffffc020195e <__slob_get_free_pages.constprop.0+0x4a>
ffffffffc020194e:	0000c697          	auipc	a3,0xc
ffffffffc0201952:	b726b683          	ld	a3,-1166(a3) # ffffffffc020d4c0 <va_pa_offset>
ffffffffc0201956:	9536                	add	a0,a0,a3
}
ffffffffc0201958:	60a2                	ld	ra,8(sp)
ffffffffc020195a:	0141                	addi	sp,sp,16
ffffffffc020195c:	8082                	ret
ffffffffc020195e:	86aa                	mv	a3,a0
ffffffffc0201960:	00003617          	auipc	a2,0x3
ffffffffc0201964:	3b860613          	addi	a2,a2,952 # ffffffffc0204d18 <default_pmm_manager+0x38>
ffffffffc0201968:	07100593          	li	a1,113
ffffffffc020196c:	00003517          	auipc	a0,0x3
ffffffffc0201970:	3d450513          	addi	a0,a0,980 # ffffffffc0204d40 <default_pmm_manager+0x60>
ffffffffc0201974:	ae7fe0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0201978 <slob_alloc.constprop.0>:
static void *slob_alloc(size_t size, gfp_t gfp, int align)
ffffffffc0201978:	1101                	addi	sp,sp,-32
ffffffffc020197a:	ec06                	sd	ra,24(sp)
ffffffffc020197c:	e822                	sd	s0,16(sp)
ffffffffc020197e:	e426                	sd	s1,8(sp)
ffffffffc0201980:	e04a                	sd	s2,0(sp)
	assert((size + SLOB_UNIT) < PAGE_SIZE);
ffffffffc0201982:	01050713          	addi	a4,a0,16
ffffffffc0201986:	6785                	lui	a5,0x1
ffffffffc0201988:	0cf77363          	bgeu	a4,a5,ffffffffc0201a4e <slob_alloc.constprop.0+0xd6>
	int delta = 0, units = SLOB_UNITS(size);
ffffffffc020198c:	00f50493          	addi	s1,a0,15
ffffffffc0201990:	8091                	srli	s1,s1,0x4
ffffffffc0201992:	2481                	sext.w	s1,s1
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc0201994:	10002673          	csrr	a2,sstatus
ffffffffc0201998:	8a09                	andi	a2,a2,2
ffffffffc020199a:	e25d                	bnez	a2,ffffffffc0201a40 <slob_alloc.constprop.0+0xc8>
	prev = slobfree;
ffffffffc020199c:	00007917          	auipc	s2,0x7
ffffffffc02019a0:	68490913          	addi	s2,s2,1668 # ffffffffc0209020 <slobfree>
ffffffffc02019a4:	00093683          	ld	a3,0(s2)
	for (cur = prev->next;; prev = cur, cur = cur->next)
ffffffffc02019a8:	669c                	ld	a5,8(a3)
		if (cur->units >= units + delta)
ffffffffc02019aa:	4398                	lw	a4,0(a5)
ffffffffc02019ac:	08975e63          	bge	a4,s1,ffffffffc0201a48 <slob_alloc.constprop.0+0xd0>
		if (cur == slobfree)
ffffffffc02019b0:	00d78b63          	beq	a5,a3,ffffffffc02019c6 <slob_alloc.constprop.0+0x4e>
	for (cur = prev->next;; prev = cur, cur = cur->next)
ffffffffc02019b4:	6780                	ld	s0,8(a5)
		if (cur->units >= units + delta)
ffffffffc02019b6:	4018                	lw	a4,0(s0)
ffffffffc02019b8:	02975a63          	bge	a4,s1,ffffffffc02019ec <slob_alloc.constprop.0+0x74>
		if (cur == slobfree)
ffffffffc02019bc:	00093683          	ld	a3,0(s2)
ffffffffc02019c0:	87a2                	mv	a5,s0
ffffffffc02019c2:	fed799e3          	bne	a5,a3,ffffffffc02019b4 <slob_alloc.constprop.0+0x3c>
    if (flag) {
ffffffffc02019c6:	ee31                	bnez	a2,ffffffffc0201a22 <slob_alloc.constprop.0+0xaa>
			cur = (slob_t *)__slob_get_free_page(gfp);
ffffffffc02019c8:	4501                	li	a0,0
ffffffffc02019ca:	f4bff0ef          	jal	ra,ffffffffc0201914 <__slob_get_free_pages.constprop.0>
ffffffffc02019ce:	842a                	mv	s0,a0
			if (!cur)
ffffffffc02019d0:	cd05                	beqz	a0,ffffffffc0201a08 <slob_alloc.constprop.0+0x90>
			slob_free(cur, PAGE_SIZE);
ffffffffc02019d2:	6585                	lui	a1,0x1
ffffffffc02019d4:	e8dff0ef          	jal	ra,ffffffffc0201860 <slob_free>
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc02019d8:	10002673          	csrr	a2,sstatus
ffffffffc02019dc:	8a09                	andi	a2,a2,2
ffffffffc02019de:	ee05                	bnez	a2,ffffffffc0201a16 <slob_alloc.constprop.0+0x9e>
			cur = slobfree;
ffffffffc02019e0:	00093783          	ld	a5,0(s2)
	for (cur = prev->next;; prev = cur, cur = cur->next)
ffffffffc02019e4:	6780                	ld	s0,8(a5)
		if (cur->units >= units + delta)
ffffffffc02019e6:	4018                	lw	a4,0(s0)
ffffffffc02019e8:	fc974ae3          	blt	a4,s1,ffffffffc02019bc <slob_alloc.constprop.0+0x44>
			if (cur->units == units)	/* exact fit? */
ffffffffc02019ec:	04e48763          	beq	s1,a4,ffffffffc0201a3a <slob_alloc.constprop.0+0xc2>
				prev->next = cur + units;
ffffffffc02019f0:	00449693          	slli	a3,s1,0x4
ffffffffc02019f4:	96a2                	add	a3,a3,s0
ffffffffc02019f6:	e794                	sd	a3,8(a5)
				prev->next->next = cur->next;
ffffffffc02019f8:	640c                	ld	a1,8(s0)
				prev->next->units = cur->units - units;
ffffffffc02019fa:	9f05                	subw	a4,a4,s1
ffffffffc02019fc:	c298                	sw	a4,0(a3)
				prev->next->next = cur->next;
ffffffffc02019fe:	e68c                	sd	a1,8(a3)
				cur->units = units;
ffffffffc0201a00:	c004                	sw	s1,0(s0)
			slobfree = prev;
ffffffffc0201a02:	00f93023          	sd	a5,0(s2)
    if (flag) {
ffffffffc0201a06:	e20d                	bnez	a2,ffffffffc0201a28 <slob_alloc.constprop.0+0xb0>
}
ffffffffc0201a08:	60e2                	ld	ra,24(sp)
ffffffffc0201a0a:	8522                	mv	a0,s0
ffffffffc0201a0c:	6442                	ld	s0,16(sp)
ffffffffc0201a0e:	64a2                	ld	s1,8(sp)
ffffffffc0201a10:	6902                	ld	s2,0(sp)
ffffffffc0201a12:	6105                	addi	sp,sp,32
ffffffffc0201a14:	8082                	ret
        intr_disable();
ffffffffc0201a16:	f1bfe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
			cur = slobfree;
ffffffffc0201a1a:	00093783          	ld	a5,0(s2)
        return 1;
ffffffffc0201a1e:	4605                	li	a2,1
ffffffffc0201a20:	b7d1                	j	ffffffffc02019e4 <slob_alloc.constprop.0+0x6c>
        intr_enable();
ffffffffc0201a22:	f09fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc0201a26:	b74d                	j	ffffffffc02019c8 <slob_alloc.constprop.0+0x50>
ffffffffc0201a28:	f03fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
}
ffffffffc0201a2c:	60e2                	ld	ra,24(sp)
ffffffffc0201a2e:	8522                	mv	a0,s0
ffffffffc0201a30:	6442                	ld	s0,16(sp)
ffffffffc0201a32:	64a2                	ld	s1,8(sp)
ffffffffc0201a34:	6902                	ld	s2,0(sp)
ffffffffc0201a36:	6105                	addi	sp,sp,32
ffffffffc0201a38:	8082                	ret
				prev->next = cur->next; /* unlink */
ffffffffc0201a3a:	6418                	ld	a4,8(s0)
ffffffffc0201a3c:	e798                	sd	a4,8(a5)
ffffffffc0201a3e:	b7d1                	j	ffffffffc0201a02 <slob_alloc.constprop.0+0x8a>
        intr_disable();
ffffffffc0201a40:	ef1fe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        return 1;
ffffffffc0201a44:	4605                	li	a2,1
ffffffffc0201a46:	bf99                	j	ffffffffc020199c <slob_alloc.constprop.0+0x24>
		if (cur->units >= units + delta)
ffffffffc0201a48:	843e                	mv	s0,a5
ffffffffc0201a4a:	87b6                	mv	a5,a3
ffffffffc0201a4c:	b745                	j	ffffffffc02019ec <slob_alloc.constprop.0+0x74>
	assert((size + SLOB_UNIT) < PAGE_SIZE);
ffffffffc0201a4e:	00003697          	auipc	a3,0x3
ffffffffc0201a52:	30268693          	addi	a3,a3,770 # ffffffffc0204d50 <default_pmm_manager+0x70>
ffffffffc0201a56:	00003617          	auipc	a2,0x3
ffffffffc0201a5a:	eda60613          	addi	a2,a2,-294 # ffffffffc0204930 <commands+0x818>
ffffffffc0201a5e:	06300593          	li	a1,99
ffffffffc0201a62:	00003517          	auipc	a0,0x3
ffffffffc0201a66:	30e50513          	addi	a0,a0,782 # ffffffffc0204d70 <default_pmm_manager+0x90>
ffffffffc0201a6a:	9f1fe0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0201a6e <kmalloc_init>:
	cprintf("use SLOB allocator\n");
}

inline void
kmalloc_init(void)
{
ffffffffc0201a6e:	1141                	addi	sp,sp,-16
	cprintf("use SLOB allocator\n");
ffffffffc0201a70:	00003517          	auipc	a0,0x3
ffffffffc0201a74:	31850513          	addi	a0,a0,792 # ffffffffc0204d88 <default_pmm_manager+0xa8>
{
ffffffffc0201a78:	e406                	sd	ra,8(sp)
	cprintf("use SLOB allocator\n");
ffffffffc0201a7a:	f1afe0ef          	jal	ra,ffffffffc0200194 <cprintf>
	slob_init();
	cprintf("kmalloc_init() succeeded!\n");
}
ffffffffc0201a7e:	60a2                	ld	ra,8(sp)
	cprintf("kmalloc_init() succeeded!\n");
ffffffffc0201a80:	00003517          	auipc	a0,0x3
ffffffffc0201a84:	32050513          	addi	a0,a0,800 # ffffffffc0204da0 <default_pmm_manager+0xc0>
}
ffffffffc0201a88:	0141                	addi	sp,sp,16
	cprintf("kmalloc_init() succeeded!\n");
ffffffffc0201a8a:	f0afe06f          	j	ffffffffc0200194 <cprintf>

ffffffffc0201a8e <kmalloc>:
	return 0;
}

void *
kmalloc(size_t size)
{
ffffffffc0201a8e:	1101                	addi	sp,sp,-32
ffffffffc0201a90:	e04a                	sd	s2,0(sp)
	if (size < PAGE_SIZE - SLOB_UNIT)
ffffffffc0201a92:	6905                	lui	s2,0x1
{
ffffffffc0201a94:	e822                	sd	s0,16(sp)
ffffffffc0201a96:	ec06                	sd	ra,24(sp)
ffffffffc0201a98:	e426                	sd	s1,8(sp)
	if (size < PAGE_SIZE - SLOB_UNIT)
ffffffffc0201a9a:	fef90793          	addi	a5,s2,-17 # fef <kern_entry-0xffffffffc01ff011>
{
ffffffffc0201a9e:	842a                	mv	s0,a0
	if (size < PAGE_SIZE - SLOB_UNIT)
ffffffffc0201aa0:	04a7f963          	bgeu	a5,a0,ffffffffc0201af2 <kmalloc+0x64>
	bb = slob_alloc(sizeof(bigblock_t), gfp, 0);
ffffffffc0201aa4:	4561                	li	a0,24
ffffffffc0201aa6:	ed3ff0ef          	jal	ra,ffffffffc0201978 <slob_alloc.constprop.0>
ffffffffc0201aaa:	84aa                	mv	s1,a0
	if (!bb)
ffffffffc0201aac:	c929                	beqz	a0,ffffffffc0201afe <kmalloc+0x70>
	bb->order = find_order(size);
ffffffffc0201aae:	0004079b          	sext.w	a5,s0
	int order = 0;
ffffffffc0201ab2:	4501                	li	a0,0
	for (; size > 4096; size >>= 1)
ffffffffc0201ab4:	00f95763          	bge	s2,a5,ffffffffc0201ac2 <kmalloc+0x34>
ffffffffc0201ab8:	6705                	lui	a4,0x1
ffffffffc0201aba:	8785                	srai	a5,a5,0x1
		order++;
ffffffffc0201abc:	2505                	addiw	a0,a0,1
	for (; size > 4096; size >>= 1)
ffffffffc0201abe:	fef74ee3          	blt	a4,a5,ffffffffc0201aba <kmalloc+0x2c>
	bb->order = find_order(size);
ffffffffc0201ac2:	c088                	sw	a0,0(s1)
	bb->pages = (void *)__slob_get_free_pages(gfp, bb->order);
ffffffffc0201ac4:	e51ff0ef          	jal	ra,ffffffffc0201914 <__slob_get_free_pages.constprop.0>
ffffffffc0201ac8:	e488                	sd	a0,8(s1)
ffffffffc0201aca:	842a                	mv	s0,a0
	if (bb->pages)
ffffffffc0201acc:	c525                	beqz	a0,ffffffffc0201b34 <kmalloc+0xa6>
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc0201ace:	100027f3          	csrr	a5,sstatus
ffffffffc0201ad2:	8b89                	andi	a5,a5,2
ffffffffc0201ad4:	ef8d                	bnez	a5,ffffffffc0201b0e <kmalloc+0x80>
		bb->next = bigblocks;
ffffffffc0201ad6:	0000c797          	auipc	a5,0xc
ffffffffc0201ada:	9ba78793          	addi	a5,a5,-1606 # ffffffffc020d490 <bigblocks>
ffffffffc0201ade:	6398                	ld	a4,0(a5)
		bigblocks = bb;
ffffffffc0201ae0:	e384                	sd	s1,0(a5)
		bb->next = bigblocks;
ffffffffc0201ae2:	e898                	sd	a4,16(s1)
	return __kmalloc(size, 0);
}
ffffffffc0201ae4:	60e2                	ld	ra,24(sp)
ffffffffc0201ae6:	8522                	mv	a0,s0
ffffffffc0201ae8:	6442                	ld	s0,16(sp)
ffffffffc0201aea:	64a2                	ld	s1,8(sp)
ffffffffc0201aec:	6902                	ld	s2,0(sp)
ffffffffc0201aee:	6105                	addi	sp,sp,32
ffffffffc0201af0:	8082                	ret
		m = slob_alloc(size + SLOB_UNIT, gfp, 0);
ffffffffc0201af2:	0541                	addi	a0,a0,16
ffffffffc0201af4:	e85ff0ef          	jal	ra,ffffffffc0201978 <slob_alloc.constprop.0>
		return m ? (void *)(m + 1) : 0;
ffffffffc0201af8:	01050413          	addi	s0,a0,16
ffffffffc0201afc:	f565                	bnez	a0,ffffffffc0201ae4 <kmalloc+0x56>
ffffffffc0201afe:	4401                	li	s0,0
}
ffffffffc0201b00:	60e2                	ld	ra,24(sp)
ffffffffc0201b02:	8522                	mv	a0,s0
ffffffffc0201b04:	6442                	ld	s0,16(sp)
ffffffffc0201b06:	64a2                	ld	s1,8(sp)
ffffffffc0201b08:	6902                	ld	s2,0(sp)
ffffffffc0201b0a:	6105                	addi	sp,sp,32
ffffffffc0201b0c:	8082                	ret
        intr_disable();
ffffffffc0201b0e:	e23fe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
		bb->next = bigblocks;
ffffffffc0201b12:	0000c797          	auipc	a5,0xc
ffffffffc0201b16:	97e78793          	addi	a5,a5,-1666 # ffffffffc020d490 <bigblocks>
ffffffffc0201b1a:	6398                	ld	a4,0(a5)
		bigblocks = bb;
ffffffffc0201b1c:	e384                	sd	s1,0(a5)
		bb->next = bigblocks;
ffffffffc0201b1e:	e898                	sd	a4,16(s1)
        intr_enable();
ffffffffc0201b20:	e0bfe0ef          	jal	ra,ffffffffc020092a <intr_enable>
		return bb->pages;
ffffffffc0201b24:	6480                	ld	s0,8(s1)
}
ffffffffc0201b26:	60e2                	ld	ra,24(sp)
ffffffffc0201b28:	64a2                	ld	s1,8(sp)
ffffffffc0201b2a:	8522                	mv	a0,s0
ffffffffc0201b2c:	6442                	ld	s0,16(sp)
ffffffffc0201b2e:	6902                	ld	s2,0(sp)
ffffffffc0201b30:	6105                	addi	sp,sp,32
ffffffffc0201b32:	8082                	ret
	slob_free(bb, sizeof(bigblock_t));
ffffffffc0201b34:	45e1                	li	a1,24
ffffffffc0201b36:	8526                	mv	a0,s1
ffffffffc0201b38:	d29ff0ef          	jal	ra,ffffffffc0201860 <slob_free>
	return __kmalloc(size, 0);
ffffffffc0201b3c:	b765                	j	ffffffffc0201ae4 <kmalloc+0x56>

ffffffffc0201b3e <kfree>:
void kfree(void *block)
{
	bigblock_t *bb, **last = &bigblocks;
	unsigned long flags;

	if (!block)
ffffffffc0201b3e:	c169                	beqz	a0,ffffffffc0201c00 <kfree+0xc2>
{
ffffffffc0201b40:	1101                	addi	sp,sp,-32
ffffffffc0201b42:	e822                	sd	s0,16(sp)
ffffffffc0201b44:	ec06                	sd	ra,24(sp)
ffffffffc0201b46:	e426                	sd	s1,8(sp)
		return;

	if (!((unsigned long)block & (PAGE_SIZE - 1)))
ffffffffc0201b48:	03451793          	slli	a5,a0,0x34
ffffffffc0201b4c:	842a                	mv	s0,a0
ffffffffc0201b4e:	e3d9                	bnez	a5,ffffffffc0201bd4 <kfree+0x96>
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc0201b50:	100027f3          	csrr	a5,sstatus
ffffffffc0201b54:	8b89                	andi	a5,a5,2
ffffffffc0201b56:	e7d9                	bnez	a5,ffffffffc0201be4 <kfree+0xa6>
	{
		/* might be on the big block list */
		spin_lock_irqsave(&block_lock, flags);
		for (bb = bigblocks; bb; last = &bb->next, bb = bb->next)
ffffffffc0201b58:	0000c797          	auipc	a5,0xc
ffffffffc0201b5c:	9387b783          	ld	a5,-1736(a5) # ffffffffc020d490 <bigblocks>
    return 0;
ffffffffc0201b60:	4601                	li	a2,0
ffffffffc0201b62:	cbad                	beqz	a5,ffffffffc0201bd4 <kfree+0x96>
	bigblock_t *bb, **last = &bigblocks;
ffffffffc0201b64:	0000c697          	auipc	a3,0xc
ffffffffc0201b68:	92c68693          	addi	a3,a3,-1748 # ffffffffc020d490 <bigblocks>
ffffffffc0201b6c:	a021                	j	ffffffffc0201b74 <kfree+0x36>
		for (bb = bigblocks; bb; last = &bb->next, bb = bb->next)
ffffffffc0201b6e:	01048693          	addi	a3,s1,16
ffffffffc0201b72:	c3a5                	beqz	a5,ffffffffc0201bd2 <kfree+0x94>
		{
			if (bb->pages == block)
ffffffffc0201b74:	6798                	ld	a4,8(a5)
ffffffffc0201b76:	84be                	mv	s1,a5
			{
				*last = bb->next;
ffffffffc0201b78:	6b9c                	ld	a5,16(a5)
			if (bb->pages == block)
ffffffffc0201b7a:	fe871ae3          	bne	a4,s0,ffffffffc0201b6e <kfree+0x30>
				*last = bb->next;
ffffffffc0201b7e:	e29c                	sd	a5,0(a3)
    if (flag) {
ffffffffc0201b80:	ee2d                	bnez	a2,ffffffffc0201bfa <kfree+0xbc>
    return pa2page(PADDR(kva));
ffffffffc0201b82:	c02007b7          	lui	a5,0xc0200
				spin_unlock_irqrestore(&block_lock, flags);
				__slob_free_pages((unsigned long)block, bb->order);
ffffffffc0201b86:	4098                	lw	a4,0(s1)
ffffffffc0201b88:	08f46963          	bltu	s0,a5,ffffffffc0201c1a <kfree+0xdc>
ffffffffc0201b8c:	0000c697          	auipc	a3,0xc
ffffffffc0201b90:	9346b683          	ld	a3,-1740(a3) # ffffffffc020d4c0 <va_pa_offset>
ffffffffc0201b94:	8c15                	sub	s0,s0,a3
    if (PPN(pa) >= npage)
ffffffffc0201b96:	8031                	srli	s0,s0,0xc
ffffffffc0201b98:	0000c797          	auipc	a5,0xc
ffffffffc0201b9c:	9107b783          	ld	a5,-1776(a5) # ffffffffc020d4a8 <npage>
ffffffffc0201ba0:	06f47163          	bgeu	s0,a5,ffffffffc0201c02 <kfree+0xc4>
    return &pages[PPN(pa) - nbase];
ffffffffc0201ba4:	00004517          	auipc	a0,0x4
ffffffffc0201ba8:	e5453503          	ld	a0,-428(a0) # ffffffffc02059f8 <nbase>
ffffffffc0201bac:	8c09                	sub	s0,s0,a0
ffffffffc0201bae:	041a                	slli	s0,s0,0x6
	free_pages(kva2page(kva), 1 << order);
ffffffffc0201bb0:	0000c517          	auipc	a0,0xc
ffffffffc0201bb4:	90053503          	ld	a0,-1792(a0) # ffffffffc020d4b0 <pages>
ffffffffc0201bb8:	4585                	li	a1,1
ffffffffc0201bba:	9522                	add	a0,a0,s0
ffffffffc0201bbc:	00e595bb          	sllw	a1,a1,a4
ffffffffc0201bc0:	0ea000ef          	jal	ra,ffffffffc0201caa <free_pages>
		spin_unlock_irqrestore(&block_lock, flags);
	}

	slob_free((slob_t *)block - 1, 0);
	return;
}
ffffffffc0201bc4:	6442                	ld	s0,16(sp)
ffffffffc0201bc6:	60e2                	ld	ra,24(sp)
				slob_free(bb, sizeof(bigblock_t));
ffffffffc0201bc8:	8526                	mv	a0,s1
}
ffffffffc0201bca:	64a2                	ld	s1,8(sp)
				slob_free(bb, sizeof(bigblock_t));
ffffffffc0201bcc:	45e1                	li	a1,24
}
ffffffffc0201bce:	6105                	addi	sp,sp,32
	slob_free((slob_t *)block - 1, 0);
ffffffffc0201bd0:	b941                	j	ffffffffc0201860 <slob_free>
ffffffffc0201bd2:	e20d                	bnez	a2,ffffffffc0201bf4 <kfree+0xb6>
ffffffffc0201bd4:	ff040513          	addi	a0,s0,-16
}
ffffffffc0201bd8:	6442                	ld	s0,16(sp)
ffffffffc0201bda:	60e2                	ld	ra,24(sp)
ffffffffc0201bdc:	64a2                	ld	s1,8(sp)
	slob_free((slob_t *)block - 1, 0);
ffffffffc0201bde:	4581                	li	a1,0
}
ffffffffc0201be0:	6105                	addi	sp,sp,32
	slob_free((slob_t *)block - 1, 0);
ffffffffc0201be2:	b9bd                	j	ffffffffc0201860 <slob_free>
        intr_disable();
ffffffffc0201be4:	d4dfe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
		for (bb = bigblocks; bb; last = &bb->next, bb = bb->next)
ffffffffc0201be8:	0000c797          	auipc	a5,0xc
ffffffffc0201bec:	8a87b783          	ld	a5,-1880(a5) # ffffffffc020d490 <bigblocks>
        return 1;
ffffffffc0201bf0:	4605                	li	a2,1
ffffffffc0201bf2:	fbad                	bnez	a5,ffffffffc0201b64 <kfree+0x26>
        intr_enable();
ffffffffc0201bf4:	d37fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc0201bf8:	bff1                	j	ffffffffc0201bd4 <kfree+0x96>
ffffffffc0201bfa:	d31fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc0201bfe:	b751                	j	ffffffffc0201b82 <kfree+0x44>
ffffffffc0201c00:	8082                	ret
        panic("pa2page called with invalid pa");
ffffffffc0201c02:	00003617          	auipc	a2,0x3
ffffffffc0201c06:	1e660613          	addi	a2,a2,486 # ffffffffc0204de8 <default_pmm_manager+0x108>
ffffffffc0201c0a:	06900593          	li	a1,105
ffffffffc0201c0e:	00003517          	auipc	a0,0x3
ffffffffc0201c12:	13250513          	addi	a0,a0,306 # ffffffffc0204d40 <default_pmm_manager+0x60>
ffffffffc0201c16:	845fe0ef          	jal	ra,ffffffffc020045a <__panic>
    return pa2page(PADDR(kva));
ffffffffc0201c1a:	86a2                	mv	a3,s0
ffffffffc0201c1c:	00003617          	auipc	a2,0x3
ffffffffc0201c20:	1a460613          	addi	a2,a2,420 # ffffffffc0204dc0 <default_pmm_manager+0xe0>
ffffffffc0201c24:	07700593          	li	a1,119
ffffffffc0201c28:	00003517          	auipc	a0,0x3
ffffffffc0201c2c:	11850513          	addi	a0,a0,280 # ffffffffc0204d40 <default_pmm_manager+0x60>
ffffffffc0201c30:	82bfe0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0201c34 <pa2page.part.0>:
pa2page(uintptr_t pa)
ffffffffc0201c34:	1141                	addi	sp,sp,-16
        panic("pa2page called with invalid pa");
ffffffffc0201c36:	00003617          	auipc	a2,0x3
ffffffffc0201c3a:	1b260613          	addi	a2,a2,434 # ffffffffc0204de8 <default_pmm_manager+0x108>
ffffffffc0201c3e:	06900593          	li	a1,105
ffffffffc0201c42:	00003517          	auipc	a0,0x3
ffffffffc0201c46:	0fe50513          	addi	a0,a0,254 # ffffffffc0204d40 <default_pmm_manager+0x60>
pa2page(uintptr_t pa)
ffffffffc0201c4a:	e406                	sd	ra,8(sp)
        panic("pa2page called with invalid pa");
ffffffffc0201c4c:	80ffe0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0201c50 <pte2page.part.0>:
pte2page(pte_t pte)
ffffffffc0201c50:	1141                	addi	sp,sp,-16
        panic("pte2page called with invalid pte");
ffffffffc0201c52:	00003617          	auipc	a2,0x3
ffffffffc0201c56:	1b660613          	addi	a2,a2,438 # ffffffffc0204e08 <default_pmm_manager+0x128>
ffffffffc0201c5a:	07f00593          	li	a1,127
ffffffffc0201c5e:	00003517          	auipc	a0,0x3
ffffffffc0201c62:	0e250513          	addi	a0,a0,226 # ffffffffc0204d40 <default_pmm_manager+0x60>
pte2page(pte_t pte)
ffffffffc0201c66:	e406                	sd	ra,8(sp)
        panic("pte2page called with invalid pte");
ffffffffc0201c68:	ff2fe0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0201c6c <alloc_pages>:
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc0201c6c:	100027f3          	csrr	a5,sstatus
ffffffffc0201c70:	8b89                	andi	a5,a5,2
ffffffffc0201c72:	e799                	bnez	a5,ffffffffc0201c80 <alloc_pages+0x14>
{
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
ffffffffc0201c74:	0000c797          	auipc	a5,0xc
ffffffffc0201c78:	8447b783          	ld	a5,-1980(a5) # ffffffffc020d4b8 <pmm_manager>
ffffffffc0201c7c:	6f9c                	ld	a5,24(a5)
ffffffffc0201c7e:	8782                	jr	a5
{
ffffffffc0201c80:	1141                	addi	sp,sp,-16
ffffffffc0201c82:	e406                	sd	ra,8(sp)
ffffffffc0201c84:	e022                	sd	s0,0(sp)
ffffffffc0201c86:	842a                	mv	s0,a0
        intr_disable();
ffffffffc0201c88:	ca9fe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        page = pmm_manager->alloc_pages(n);
ffffffffc0201c8c:	0000c797          	auipc	a5,0xc
ffffffffc0201c90:	82c7b783          	ld	a5,-2004(a5) # ffffffffc020d4b8 <pmm_manager>
ffffffffc0201c94:	6f9c                	ld	a5,24(a5)
ffffffffc0201c96:	8522                	mv	a0,s0
ffffffffc0201c98:	9782                	jalr	a5
ffffffffc0201c9a:	842a                	mv	s0,a0
        intr_enable();
ffffffffc0201c9c:	c8ffe0ef          	jal	ra,ffffffffc020092a <intr_enable>
    }
    local_intr_restore(intr_flag);
    return page;
}
ffffffffc0201ca0:	60a2                	ld	ra,8(sp)
ffffffffc0201ca2:	8522                	mv	a0,s0
ffffffffc0201ca4:	6402                	ld	s0,0(sp)
ffffffffc0201ca6:	0141                	addi	sp,sp,16
ffffffffc0201ca8:	8082                	ret

ffffffffc0201caa <free_pages>:
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc0201caa:	100027f3          	csrr	a5,sstatus
ffffffffc0201cae:	8b89                	andi	a5,a5,2
ffffffffc0201cb0:	e799                	bnez	a5,ffffffffc0201cbe <free_pages+0x14>
void free_pages(struct Page *base, size_t n)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
ffffffffc0201cb2:	0000c797          	auipc	a5,0xc
ffffffffc0201cb6:	8067b783          	ld	a5,-2042(a5) # ffffffffc020d4b8 <pmm_manager>
ffffffffc0201cba:	739c                	ld	a5,32(a5)
ffffffffc0201cbc:	8782                	jr	a5
{
ffffffffc0201cbe:	1101                	addi	sp,sp,-32
ffffffffc0201cc0:	ec06                	sd	ra,24(sp)
ffffffffc0201cc2:	e822                	sd	s0,16(sp)
ffffffffc0201cc4:	e426                	sd	s1,8(sp)
ffffffffc0201cc6:	842a                	mv	s0,a0
ffffffffc0201cc8:	84ae                	mv	s1,a1
        intr_disable();
ffffffffc0201cca:	c67fe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        pmm_manager->free_pages(base, n);
ffffffffc0201cce:	0000b797          	auipc	a5,0xb
ffffffffc0201cd2:	7ea7b783          	ld	a5,2026(a5) # ffffffffc020d4b8 <pmm_manager>
ffffffffc0201cd6:	739c                	ld	a5,32(a5)
ffffffffc0201cd8:	85a6                	mv	a1,s1
ffffffffc0201cda:	8522                	mv	a0,s0
ffffffffc0201cdc:	9782                	jalr	a5
    }
    local_intr_restore(intr_flag);
}
ffffffffc0201cde:	6442                	ld	s0,16(sp)
ffffffffc0201ce0:	60e2                	ld	ra,24(sp)
ffffffffc0201ce2:	64a2                	ld	s1,8(sp)
ffffffffc0201ce4:	6105                	addi	sp,sp,32
        intr_enable();
ffffffffc0201ce6:	c45fe06f          	j	ffffffffc020092a <intr_enable>

ffffffffc0201cea <nr_free_pages>:
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc0201cea:	100027f3          	csrr	a5,sstatus
ffffffffc0201cee:	8b89                	andi	a5,a5,2
ffffffffc0201cf0:	e799                	bnez	a5,ffffffffc0201cfe <nr_free_pages+0x14>
{
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
ffffffffc0201cf2:	0000b797          	auipc	a5,0xb
ffffffffc0201cf6:	7c67b783          	ld	a5,1990(a5) # ffffffffc020d4b8 <pmm_manager>
ffffffffc0201cfa:	779c                	ld	a5,40(a5)
ffffffffc0201cfc:	8782                	jr	a5
{
ffffffffc0201cfe:	1141                	addi	sp,sp,-16
ffffffffc0201d00:	e406                	sd	ra,8(sp)
ffffffffc0201d02:	e022                	sd	s0,0(sp)
        intr_disable();
ffffffffc0201d04:	c2dfe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        ret = pmm_manager->nr_free_pages();
ffffffffc0201d08:	0000b797          	auipc	a5,0xb
ffffffffc0201d0c:	7b07b783          	ld	a5,1968(a5) # ffffffffc020d4b8 <pmm_manager>
ffffffffc0201d10:	779c                	ld	a5,40(a5)
ffffffffc0201d12:	9782                	jalr	a5
ffffffffc0201d14:	842a                	mv	s0,a0
        intr_enable();
ffffffffc0201d16:	c15fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
    }
    local_intr_restore(intr_flag);
    return ret;
}
ffffffffc0201d1a:	60a2                	ld	ra,8(sp)
ffffffffc0201d1c:	8522                	mv	a0,s0
ffffffffc0201d1e:	6402                	ld	s0,0(sp)
ffffffffc0201d20:	0141                	addi	sp,sp,16
ffffffffc0201d22:	8082                	ret

ffffffffc0201d24 <get_pte>:
//  la:     the linear address need to map
//  create: a logical value to decide if alloc a page for PT
// return vaule: the kernel virtual address of this pte
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    pde_t *pdep1 = &pgdir[PDX1(la)];
ffffffffc0201d24:	01e5d793          	srli	a5,a1,0x1e
ffffffffc0201d28:	1ff7f793          	andi	a5,a5,511
{
ffffffffc0201d2c:	7139                	addi	sp,sp,-64
    pde_t *pdep1 = &pgdir[PDX1(la)];
ffffffffc0201d2e:	078e                	slli	a5,a5,0x3
{
ffffffffc0201d30:	f426                	sd	s1,40(sp)
    pde_t *pdep1 = &pgdir[PDX1(la)];
ffffffffc0201d32:	00f504b3          	add	s1,a0,a5
    if (!(*pdep1 & PTE_V))
ffffffffc0201d36:	6094                	ld	a3,0(s1)
{
ffffffffc0201d38:	f04a                	sd	s2,32(sp)
ffffffffc0201d3a:	ec4e                	sd	s3,24(sp)
ffffffffc0201d3c:	e852                	sd	s4,16(sp)
ffffffffc0201d3e:	fc06                	sd	ra,56(sp)
ffffffffc0201d40:	f822                	sd	s0,48(sp)
ffffffffc0201d42:	e456                	sd	s5,8(sp)
ffffffffc0201d44:	e05a                	sd	s6,0(sp)
    if (!(*pdep1 & PTE_V))
ffffffffc0201d46:	0016f793          	andi	a5,a3,1
{
ffffffffc0201d4a:	892e                	mv	s2,a1
ffffffffc0201d4c:	8a32                	mv	s4,a2
ffffffffc0201d4e:	0000b997          	auipc	s3,0xb
ffffffffc0201d52:	75a98993          	addi	s3,s3,1882 # ffffffffc020d4a8 <npage>
    if (!(*pdep1 & PTE_V))
ffffffffc0201d56:	efbd                	bnez	a5,ffffffffc0201dd4 <get_pte+0xb0>
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
ffffffffc0201d58:	14060c63          	beqz	a2,ffffffffc0201eb0 <get_pte+0x18c>
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc0201d5c:	100027f3          	csrr	a5,sstatus
ffffffffc0201d60:	8b89                	andi	a5,a5,2
ffffffffc0201d62:	14079963          	bnez	a5,ffffffffc0201eb4 <get_pte+0x190>
        page = pmm_manager->alloc_pages(n);
ffffffffc0201d66:	0000b797          	auipc	a5,0xb
ffffffffc0201d6a:	7527b783          	ld	a5,1874(a5) # ffffffffc020d4b8 <pmm_manager>
ffffffffc0201d6e:	6f9c                	ld	a5,24(a5)
ffffffffc0201d70:	4505                	li	a0,1
ffffffffc0201d72:	9782                	jalr	a5
ffffffffc0201d74:	842a                	mv	s0,a0
        if (!create || (page = alloc_page()) == NULL)
ffffffffc0201d76:	12040d63          	beqz	s0,ffffffffc0201eb0 <get_pte+0x18c>
    return page - pages + nbase;
ffffffffc0201d7a:	0000bb17          	auipc	s6,0xb
ffffffffc0201d7e:	736b0b13          	addi	s6,s6,1846 # ffffffffc020d4b0 <pages>
ffffffffc0201d82:	000b3503          	ld	a0,0(s6)
ffffffffc0201d86:	00080ab7          	lui	s5,0x80
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
ffffffffc0201d8a:	0000b997          	auipc	s3,0xb
ffffffffc0201d8e:	71e98993          	addi	s3,s3,1822 # ffffffffc020d4a8 <npage>
ffffffffc0201d92:	40a40533          	sub	a0,s0,a0
ffffffffc0201d96:	8519                	srai	a0,a0,0x6
ffffffffc0201d98:	9556                	add	a0,a0,s5
ffffffffc0201d9a:	0009b703          	ld	a4,0(s3)
ffffffffc0201d9e:	00c51793          	slli	a5,a0,0xc
    page->ref = val;
ffffffffc0201da2:	4685                	li	a3,1
ffffffffc0201da4:	c014                	sw	a3,0(s0)
ffffffffc0201da6:	83b1                	srli	a5,a5,0xc
    return page2ppn(page) << PGSHIFT;
ffffffffc0201da8:	0532                	slli	a0,a0,0xc
ffffffffc0201daa:	16e7f763          	bgeu	a5,a4,ffffffffc0201f18 <get_pte+0x1f4>
ffffffffc0201dae:	0000b797          	auipc	a5,0xb
ffffffffc0201db2:	7127b783          	ld	a5,1810(a5) # ffffffffc020d4c0 <va_pa_offset>
ffffffffc0201db6:	6605                	lui	a2,0x1
ffffffffc0201db8:	4581                	li	a1,0
ffffffffc0201dba:	953e                	add	a0,a0,a5
ffffffffc0201dbc:	0a4020ef          	jal	ra,ffffffffc0203e60 <memset>
    return page - pages + nbase;
ffffffffc0201dc0:	000b3683          	ld	a3,0(s6)
ffffffffc0201dc4:	40d406b3          	sub	a3,s0,a3
ffffffffc0201dc8:	8699                	srai	a3,a3,0x6
ffffffffc0201dca:	96d6                	add	a3,a3,s5
}

// construct PTE from a page and permission bits
static inline pte_t pte_create(uintptr_t ppn, int type)
{
    return (ppn << PTE_PPN_SHIFT) | PTE_V | type;
ffffffffc0201dcc:	06aa                	slli	a3,a3,0xa
ffffffffc0201dce:	0116e693          	ori	a3,a3,17
        *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
ffffffffc0201dd2:	e094                	sd	a3,0(s1)
    }
    pde_t *pdep0 = &((pte_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
ffffffffc0201dd4:	77fd                	lui	a5,0xfffff
ffffffffc0201dd6:	068a                	slli	a3,a3,0x2
ffffffffc0201dd8:	0009b703          	ld	a4,0(s3)
ffffffffc0201ddc:	8efd                	and	a3,a3,a5
ffffffffc0201dde:	00c6d793          	srli	a5,a3,0xc
ffffffffc0201de2:	10e7ff63          	bgeu	a5,a4,ffffffffc0201f00 <get_pte+0x1dc>
ffffffffc0201de6:	0000ba97          	auipc	s5,0xb
ffffffffc0201dea:	6daa8a93          	addi	s5,s5,1754 # ffffffffc020d4c0 <va_pa_offset>
ffffffffc0201dee:	000ab403          	ld	s0,0(s5)
ffffffffc0201df2:	01595793          	srli	a5,s2,0x15
ffffffffc0201df6:	1ff7f793          	andi	a5,a5,511
ffffffffc0201dfa:	96a2                	add	a3,a3,s0
ffffffffc0201dfc:	00379413          	slli	s0,a5,0x3
ffffffffc0201e00:	9436                	add	s0,s0,a3
    if (!(*pdep0 & PTE_V))
ffffffffc0201e02:	6014                	ld	a3,0(s0)
ffffffffc0201e04:	0016f793          	andi	a5,a3,1
ffffffffc0201e08:	ebad                	bnez	a5,ffffffffc0201e7a <get_pte+0x156>
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
ffffffffc0201e0a:	0a0a0363          	beqz	s4,ffffffffc0201eb0 <get_pte+0x18c>
ffffffffc0201e0e:	100027f3          	csrr	a5,sstatus
ffffffffc0201e12:	8b89                	andi	a5,a5,2
ffffffffc0201e14:	efcd                	bnez	a5,ffffffffc0201ece <get_pte+0x1aa>
        page = pmm_manager->alloc_pages(n);
ffffffffc0201e16:	0000b797          	auipc	a5,0xb
ffffffffc0201e1a:	6a27b783          	ld	a5,1698(a5) # ffffffffc020d4b8 <pmm_manager>
ffffffffc0201e1e:	6f9c                	ld	a5,24(a5)
ffffffffc0201e20:	4505                	li	a0,1
ffffffffc0201e22:	9782                	jalr	a5
ffffffffc0201e24:	84aa                	mv	s1,a0
        if (!create || (page = alloc_page()) == NULL)
ffffffffc0201e26:	c4c9                	beqz	s1,ffffffffc0201eb0 <get_pte+0x18c>
    return page - pages + nbase;
ffffffffc0201e28:	0000bb17          	auipc	s6,0xb
ffffffffc0201e2c:	688b0b13          	addi	s6,s6,1672 # ffffffffc020d4b0 <pages>
ffffffffc0201e30:	000b3503          	ld	a0,0(s6)
ffffffffc0201e34:	00080a37          	lui	s4,0x80
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PGSIZE);
ffffffffc0201e38:	0009b703          	ld	a4,0(s3)
ffffffffc0201e3c:	40a48533          	sub	a0,s1,a0
ffffffffc0201e40:	8519                	srai	a0,a0,0x6
ffffffffc0201e42:	9552                	add	a0,a0,s4
ffffffffc0201e44:	00c51793          	slli	a5,a0,0xc
    page->ref = val;
ffffffffc0201e48:	4685                	li	a3,1
ffffffffc0201e4a:	c094                	sw	a3,0(s1)
ffffffffc0201e4c:	83b1                	srli	a5,a5,0xc
    return page2ppn(page) << PGSHIFT;
ffffffffc0201e4e:	0532                	slli	a0,a0,0xc
ffffffffc0201e50:	0ee7f163          	bgeu	a5,a4,ffffffffc0201f32 <get_pte+0x20e>
ffffffffc0201e54:	000ab783          	ld	a5,0(s5)
ffffffffc0201e58:	6605                	lui	a2,0x1
ffffffffc0201e5a:	4581                	li	a1,0
ffffffffc0201e5c:	953e                	add	a0,a0,a5
ffffffffc0201e5e:	002020ef          	jal	ra,ffffffffc0203e60 <memset>
    return page - pages + nbase;
ffffffffc0201e62:	000b3683          	ld	a3,0(s6)
ffffffffc0201e66:	40d486b3          	sub	a3,s1,a3
ffffffffc0201e6a:	8699                	srai	a3,a3,0x6
ffffffffc0201e6c:	96d2                	add	a3,a3,s4
    return (ppn << PTE_PPN_SHIFT) | PTE_V | type;
ffffffffc0201e6e:	06aa                	slli	a3,a3,0xa
ffffffffc0201e70:	0116e693          	ori	a3,a3,17
        *pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
ffffffffc0201e74:	e014                	sd	a3,0(s0)
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
ffffffffc0201e76:	0009b703          	ld	a4,0(s3)
ffffffffc0201e7a:	068a                	slli	a3,a3,0x2
ffffffffc0201e7c:	757d                	lui	a0,0xfffff
ffffffffc0201e7e:	8ee9                	and	a3,a3,a0
ffffffffc0201e80:	00c6d793          	srli	a5,a3,0xc
ffffffffc0201e84:	06e7f263          	bgeu	a5,a4,ffffffffc0201ee8 <get_pte+0x1c4>
ffffffffc0201e88:	000ab503          	ld	a0,0(s5)
ffffffffc0201e8c:	00c95913          	srli	s2,s2,0xc
ffffffffc0201e90:	1ff97913          	andi	s2,s2,511
ffffffffc0201e94:	96aa                	add	a3,a3,a0
ffffffffc0201e96:	00391513          	slli	a0,s2,0x3
ffffffffc0201e9a:	9536                	add	a0,a0,a3
}
ffffffffc0201e9c:	70e2                	ld	ra,56(sp)
ffffffffc0201e9e:	7442                	ld	s0,48(sp)
ffffffffc0201ea0:	74a2                	ld	s1,40(sp)
ffffffffc0201ea2:	7902                	ld	s2,32(sp)
ffffffffc0201ea4:	69e2                	ld	s3,24(sp)
ffffffffc0201ea6:	6a42                	ld	s4,16(sp)
ffffffffc0201ea8:	6aa2                	ld	s5,8(sp)
ffffffffc0201eaa:	6b02                	ld	s6,0(sp)
ffffffffc0201eac:	6121                	addi	sp,sp,64
ffffffffc0201eae:	8082                	ret
            return NULL;
ffffffffc0201eb0:	4501                	li	a0,0
ffffffffc0201eb2:	b7ed                	j	ffffffffc0201e9c <get_pte+0x178>
        intr_disable();
ffffffffc0201eb4:	a7dfe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        page = pmm_manager->alloc_pages(n);
ffffffffc0201eb8:	0000b797          	auipc	a5,0xb
ffffffffc0201ebc:	6007b783          	ld	a5,1536(a5) # ffffffffc020d4b8 <pmm_manager>
ffffffffc0201ec0:	6f9c                	ld	a5,24(a5)
ffffffffc0201ec2:	4505                	li	a0,1
ffffffffc0201ec4:	9782                	jalr	a5
ffffffffc0201ec6:	842a                	mv	s0,a0
        intr_enable();
ffffffffc0201ec8:	a63fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc0201ecc:	b56d                	j	ffffffffc0201d76 <get_pte+0x52>
        intr_disable();
ffffffffc0201ece:	a63fe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
ffffffffc0201ed2:	0000b797          	auipc	a5,0xb
ffffffffc0201ed6:	5e67b783          	ld	a5,1510(a5) # ffffffffc020d4b8 <pmm_manager>
ffffffffc0201eda:	6f9c                	ld	a5,24(a5)
ffffffffc0201edc:	4505                	li	a0,1
ffffffffc0201ede:	9782                	jalr	a5
ffffffffc0201ee0:	84aa                	mv	s1,a0
        intr_enable();
ffffffffc0201ee2:	a49fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc0201ee6:	b781                	j	ffffffffc0201e26 <get_pte+0x102>
    return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
ffffffffc0201ee8:	00003617          	auipc	a2,0x3
ffffffffc0201eec:	e3060613          	addi	a2,a2,-464 # ffffffffc0204d18 <default_pmm_manager+0x38>
ffffffffc0201ef0:	0fb00593          	li	a1,251
ffffffffc0201ef4:	00003517          	auipc	a0,0x3
ffffffffc0201ef8:	f3c50513          	addi	a0,a0,-196 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0201efc:	d5efe0ef          	jal	ra,ffffffffc020045a <__panic>
    pde_t *pdep0 = &((pte_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
ffffffffc0201f00:	00003617          	auipc	a2,0x3
ffffffffc0201f04:	e1860613          	addi	a2,a2,-488 # ffffffffc0204d18 <default_pmm_manager+0x38>
ffffffffc0201f08:	0ee00593          	li	a1,238
ffffffffc0201f0c:	00003517          	auipc	a0,0x3
ffffffffc0201f10:	f2450513          	addi	a0,a0,-220 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0201f14:	d46fe0ef          	jal	ra,ffffffffc020045a <__panic>
        memset(KADDR(pa), 0, PGSIZE);
ffffffffc0201f18:	86aa                	mv	a3,a0
ffffffffc0201f1a:	00003617          	auipc	a2,0x3
ffffffffc0201f1e:	dfe60613          	addi	a2,a2,-514 # ffffffffc0204d18 <default_pmm_manager+0x38>
ffffffffc0201f22:	0eb00593          	li	a1,235
ffffffffc0201f26:	00003517          	auipc	a0,0x3
ffffffffc0201f2a:	f0a50513          	addi	a0,a0,-246 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0201f2e:	d2cfe0ef          	jal	ra,ffffffffc020045a <__panic>
        memset(KADDR(pa), 0, PGSIZE);
ffffffffc0201f32:	86aa                	mv	a3,a0
ffffffffc0201f34:	00003617          	auipc	a2,0x3
ffffffffc0201f38:	de460613          	addi	a2,a2,-540 # ffffffffc0204d18 <default_pmm_manager+0x38>
ffffffffc0201f3c:	0f800593          	li	a1,248
ffffffffc0201f40:	00003517          	auipc	a0,0x3
ffffffffc0201f44:	ef050513          	addi	a0,a0,-272 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0201f48:	d12fe0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0201f4c <get_page>:

// get_page - get related Page struct for linear address la using PDT pgdir
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
{
ffffffffc0201f4c:	1141                	addi	sp,sp,-16
ffffffffc0201f4e:	e022                	sd	s0,0(sp)
ffffffffc0201f50:	8432                	mv	s0,a2
    pte_t *ptep = get_pte(pgdir, la, 0);
ffffffffc0201f52:	4601                	li	a2,0
{
ffffffffc0201f54:	e406                	sd	ra,8(sp)
    pte_t *ptep = get_pte(pgdir, la, 0);
ffffffffc0201f56:	dcfff0ef          	jal	ra,ffffffffc0201d24 <get_pte>
    if (ptep_store != NULL)
ffffffffc0201f5a:	c011                	beqz	s0,ffffffffc0201f5e <get_page+0x12>
    {
        *ptep_store = ptep;
ffffffffc0201f5c:	e008                	sd	a0,0(s0)
    }
    if (ptep != NULL && *ptep & PTE_V)
ffffffffc0201f5e:	c511                	beqz	a0,ffffffffc0201f6a <get_page+0x1e>
ffffffffc0201f60:	611c                	ld	a5,0(a0)
    {
        return pte2page(*ptep);
    }
    return NULL;
ffffffffc0201f62:	4501                	li	a0,0
    if (ptep != NULL && *ptep & PTE_V)
ffffffffc0201f64:	0017f713          	andi	a4,a5,1
ffffffffc0201f68:	e709                	bnez	a4,ffffffffc0201f72 <get_page+0x26>
}
ffffffffc0201f6a:	60a2                	ld	ra,8(sp)
ffffffffc0201f6c:	6402                	ld	s0,0(sp)
ffffffffc0201f6e:	0141                	addi	sp,sp,16
ffffffffc0201f70:	8082                	ret
    return pa2page(PTE_ADDR(pte));
ffffffffc0201f72:	078a                	slli	a5,a5,0x2
ffffffffc0201f74:	83b1                	srli	a5,a5,0xc
    if (PPN(pa) >= npage)
ffffffffc0201f76:	0000b717          	auipc	a4,0xb
ffffffffc0201f7a:	53273703          	ld	a4,1330(a4) # ffffffffc020d4a8 <npage>
ffffffffc0201f7e:	00e7ff63          	bgeu	a5,a4,ffffffffc0201f9c <get_page+0x50>
ffffffffc0201f82:	60a2                	ld	ra,8(sp)
ffffffffc0201f84:	6402                	ld	s0,0(sp)
    return &pages[PPN(pa) - nbase];
ffffffffc0201f86:	fff80537          	lui	a0,0xfff80
ffffffffc0201f8a:	97aa                	add	a5,a5,a0
ffffffffc0201f8c:	079a                	slli	a5,a5,0x6
ffffffffc0201f8e:	0000b517          	auipc	a0,0xb
ffffffffc0201f92:	52253503          	ld	a0,1314(a0) # ffffffffc020d4b0 <pages>
ffffffffc0201f96:	953e                	add	a0,a0,a5
ffffffffc0201f98:	0141                	addi	sp,sp,16
ffffffffc0201f9a:	8082                	ret
ffffffffc0201f9c:	c99ff0ef          	jal	ra,ffffffffc0201c34 <pa2page.part.0>

ffffffffc0201fa0 <page_remove>:
}

// page_remove - free an Page which is related linear address la and has an
// validated pte
void page_remove(pde_t *pgdir, uintptr_t la)
{
ffffffffc0201fa0:	7179                	addi	sp,sp,-48
    pte_t *ptep = get_pte(pgdir, la, 0);
ffffffffc0201fa2:	4601                	li	a2,0
{
ffffffffc0201fa4:	ec26                	sd	s1,24(sp)
ffffffffc0201fa6:	f406                	sd	ra,40(sp)
ffffffffc0201fa8:	f022                	sd	s0,32(sp)
ffffffffc0201faa:	84ae                	mv	s1,a1
    pte_t *ptep = get_pte(pgdir, la, 0);
ffffffffc0201fac:	d79ff0ef          	jal	ra,ffffffffc0201d24 <get_pte>
    if (ptep != NULL)
ffffffffc0201fb0:	c511                	beqz	a0,ffffffffc0201fbc <page_remove+0x1c>
    if (*ptep & PTE_V)
ffffffffc0201fb2:	611c                	ld	a5,0(a0)
ffffffffc0201fb4:	842a                	mv	s0,a0
ffffffffc0201fb6:	0017f713          	andi	a4,a5,1
ffffffffc0201fba:	e711                	bnez	a4,ffffffffc0201fc6 <page_remove+0x26>
    {
        page_remove_pte(pgdir, la, ptep);
    }
}
ffffffffc0201fbc:	70a2                	ld	ra,40(sp)
ffffffffc0201fbe:	7402                	ld	s0,32(sp)
ffffffffc0201fc0:	64e2                	ld	s1,24(sp)
ffffffffc0201fc2:	6145                	addi	sp,sp,48
ffffffffc0201fc4:	8082                	ret
    return pa2page(PTE_ADDR(pte));
ffffffffc0201fc6:	078a                	slli	a5,a5,0x2
ffffffffc0201fc8:	83b1                	srli	a5,a5,0xc
    if (PPN(pa) >= npage)
ffffffffc0201fca:	0000b717          	auipc	a4,0xb
ffffffffc0201fce:	4de73703          	ld	a4,1246(a4) # ffffffffc020d4a8 <npage>
ffffffffc0201fd2:	06e7f363          	bgeu	a5,a4,ffffffffc0202038 <page_remove+0x98>
    return &pages[PPN(pa) - nbase];
ffffffffc0201fd6:	fff80537          	lui	a0,0xfff80
ffffffffc0201fda:	97aa                	add	a5,a5,a0
ffffffffc0201fdc:	079a                	slli	a5,a5,0x6
ffffffffc0201fde:	0000b517          	auipc	a0,0xb
ffffffffc0201fe2:	4d253503          	ld	a0,1234(a0) # ffffffffc020d4b0 <pages>
ffffffffc0201fe6:	953e                	add	a0,a0,a5
    page->ref -= 1;
ffffffffc0201fe8:	411c                	lw	a5,0(a0)
ffffffffc0201fea:	fff7871b          	addiw	a4,a5,-1
ffffffffc0201fee:	c118                	sw	a4,0(a0)
        if (page_ref(page) ==
ffffffffc0201ff0:	cb11                	beqz	a4,ffffffffc0202004 <page_remove+0x64>
        *ptep = 0;                 //(5) clear second page table entry
ffffffffc0201ff2:	00043023          	sd	zero,0(s0)
// edited are the ones currently in use by the processor.
void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{
    // flush_tlb();
    // The flush_tlb flush the entire TLB, is there any better way?
    asm volatile("sfence.vma %0" : : "r"(la));
ffffffffc0201ff6:	12048073          	sfence.vma	s1
}
ffffffffc0201ffa:	70a2                	ld	ra,40(sp)
ffffffffc0201ffc:	7402                	ld	s0,32(sp)
ffffffffc0201ffe:	64e2                	ld	s1,24(sp)
ffffffffc0202000:	6145                	addi	sp,sp,48
ffffffffc0202002:	8082                	ret
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc0202004:	100027f3          	csrr	a5,sstatus
ffffffffc0202008:	8b89                	andi	a5,a5,2
ffffffffc020200a:	eb89                	bnez	a5,ffffffffc020201c <page_remove+0x7c>
        pmm_manager->free_pages(base, n);
ffffffffc020200c:	0000b797          	auipc	a5,0xb
ffffffffc0202010:	4ac7b783          	ld	a5,1196(a5) # ffffffffc020d4b8 <pmm_manager>
ffffffffc0202014:	739c                	ld	a5,32(a5)
ffffffffc0202016:	4585                	li	a1,1
ffffffffc0202018:	9782                	jalr	a5
    if (flag) {
ffffffffc020201a:	bfe1                	j	ffffffffc0201ff2 <page_remove+0x52>
        intr_disable();
ffffffffc020201c:	e42a                	sd	a0,8(sp)
ffffffffc020201e:	913fe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
ffffffffc0202022:	0000b797          	auipc	a5,0xb
ffffffffc0202026:	4967b783          	ld	a5,1174(a5) # ffffffffc020d4b8 <pmm_manager>
ffffffffc020202a:	739c                	ld	a5,32(a5)
ffffffffc020202c:	6522                	ld	a0,8(sp)
ffffffffc020202e:	4585                	li	a1,1
ffffffffc0202030:	9782                	jalr	a5
        intr_enable();
ffffffffc0202032:	8f9fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc0202036:	bf75                	j	ffffffffc0201ff2 <page_remove+0x52>
ffffffffc0202038:	bfdff0ef          	jal	ra,ffffffffc0201c34 <pa2page.part.0>

ffffffffc020203c <page_insert>:
{
ffffffffc020203c:	7139                	addi	sp,sp,-64
ffffffffc020203e:	e852                	sd	s4,16(sp)
ffffffffc0202040:	8a32                	mv	s4,a2
ffffffffc0202042:	f822                	sd	s0,48(sp)
    pte_t *ptep = get_pte(pgdir, la, 1);
ffffffffc0202044:	4605                	li	a2,1
{
ffffffffc0202046:	842e                	mv	s0,a1
    pte_t *ptep = get_pte(pgdir, la, 1);
ffffffffc0202048:	85d2                	mv	a1,s4
{
ffffffffc020204a:	f426                	sd	s1,40(sp)
ffffffffc020204c:	fc06                	sd	ra,56(sp)
ffffffffc020204e:	f04a                	sd	s2,32(sp)
ffffffffc0202050:	ec4e                	sd	s3,24(sp)
ffffffffc0202052:	e456                	sd	s5,8(sp)
ffffffffc0202054:	84b6                	mv	s1,a3
    pte_t *ptep = get_pte(pgdir, la, 1);
ffffffffc0202056:	ccfff0ef          	jal	ra,ffffffffc0201d24 <get_pte>
    if (ptep == NULL)
ffffffffc020205a:	c961                	beqz	a0,ffffffffc020212a <page_insert+0xee>
    page->ref += 1;
ffffffffc020205c:	4014                	lw	a3,0(s0)
    if (*ptep & PTE_V)
ffffffffc020205e:	611c                	ld	a5,0(a0)
ffffffffc0202060:	89aa                	mv	s3,a0
ffffffffc0202062:	0016871b          	addiw	a4,a3,1
ffffffffc0202066:	c018                	sw	a4,0(s0)
ffffffffc0202068:	0017f713          	andi	a4,a5,1
ffffffffc020206c:	ef05                	bnez	a4,ffffffffc02020a4 <page_insert+0x68>
    return page - pages + nbase;
ffffffffc020206e:	0000b717          	auipc	a4,0xb
ffffffffc0202072:	44273703          	ld	a4,1090(a4) # ffffffffc020d4b0 <pages>
ffffffffc0202076:	8c19                	sub	s0,s0,a4
ffffffffc0202078:	000807b7          	lui	a5,0x80
ffffffffc020207c:	8419                	srai	s0,s0,0x6
ffffffffc020207e:	943e                	add	s0,s0,a5
    return (ppn << PTE_PPN_SHIFT) | PTE_V | type;
ffffffffc0202080:	042a                	slli	s0,s0,0xa
ffffffffc0202082:	8cc1                	or	s1,s1,s0
ffffffffc0202084:	0014e493          	ori	s1,s1,1
    *ptep = pte_create(page2ppn(page), PTE_V | perm);
ffffffffc0202088:	0099b023          	sd	s1,0(s3)
    asm volatile("sfence.vma %0" : : "r"(la));
ffffffffc020208c:	120a0073          	sfence.vma	s4
    return 0;
ffffffffc0202090:	4501                	li	a0,0
}
ffffffffc0202092:	70e2                	ld	ra,56(sp)
ffffffffc0202094:	7442                	ld	s0,48(sp)
ffffffffc0202096:	74a2                	ld	s1,40(sp)
ffffffffc0202098:	7902                	ld	s2,32(sp)
ffffffffc020209a:	69e2                	ld	s3,24(sp)
ffffffffc020209c:	6a42                	ld	s4,16(sp)
ffffffffc020209e:	6aa2                	ld	s5,8(sp)
ffffffffc02020a0:	6121                	addi	sp,sp,64
ffffffffc02020a2:	8082                	ret
    return pa2page(PTE_ADDR(pte));
ffffffffc02020a4:	078a                	slli	a5,a5,0x2
ffffffffc02020a6:	83b1                	srli	a5,a5,0xc
    if (PPN(pa) >= npage)
ffffffffc02020a8:	0000b717          	auipc	a4,0xb
ffffffffc02020ac:	40073703          	ld	a4,1024(a4) # ffffffffc020d4a8 <npage>
ffffffffc02020b0:	06e7ff63          	bgeu	a5,a4,ffffffffc020212e <page_insert+0xf2>
    return &pages[PPN(pa) - nbase];
ffffffffc02020b4:	0000ba97          	auipc	s5,0xb
ffffffffc02020b8:	3fca8a93          	addi	s5,s5,1020 # ffffffffc020d4b0 <pages>
ffffffffc02020bc:	000ab703          	ld	a4,0(s5)
ffffffffc02020c0:	fff80937          	lui	s2,0xfff80
ffffffffc02020c4:	993e                	add	s2,s2,a5
ffffffffc02020c6:	091a                	slli	s2,s2,0x6
ffffffffc02020c8:	993a                	add	s2,s2,a4
        if (p == page)
ffffffffc02020ca:	01240c63          	beq	s0,s2,ffffffffc02020e2 <page_insert+0xa6>
    page->ref -= 1;
ffffffffc02020ce:	00092783          	lw	a5,0(s2) # fffffffffff80000 <end+0x3fd72b1c>
ffffffffc02020d2:	fff7869b          	addiw	a3,a5,-1
ffffffffc02020d6:	00d92023          	sw	a3,0(s2)
        if (page_ref(page) ==
ffffffffc02020da:	c691                	beqz	a3,ffffffffc02020e6 <page_insert+0xaa>
    asm volatile("sfence.vma %0" : : "r"(la));
ffffffffc02020dc:	120a0073          	sfence.vma	s4
}
ffffffffc02020e0:	bf59                	j	ffffffffc0202076 <page_insert+0x3a>
ffffffffc02020e2:	c014                	sw	a3,0(s0)
    return page->ref;
ffffffffc02020e4:	bf49                	j	ffffffffc0202076 <page_insert+0x3a>
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc02020e6:	100027f3          	csrr	a5,sstatus
ffffffffc02020ea:	8b89                	andi	a5,a5,2
ffffffffc02020ec:	ef91                	bnez	a5,ffffffffc0202108 <page_insert+0xcc>
        pmm_manager->free_pages(base, n);
ffffffffc02020ee:	0000b797          	auipc	a5,0xb
ffffffffc02020f2:	3ca7b783          	ld	a5,970(a5) # ffffffffc020d4b8 <pmm_manager>
ffffffffc02020f6:	739c                	ld	a5,32(a5)
ffffffffc02020f8:	4585                	li	a1,1
ffffffffc02020fa:	854a                	mv	a0,s2
ffffffffc02020fc:	9782                	jalr	a5
    return page - pages + nbase;
ffffffffc02020fe:	000ab703          	ld	a4,0(s5)
    asm volatile("sfence.vma %0" : : "r"(la));
ffffffffc0202102:	120a0073          	sfence.vma	s4
ffffffffc0202106:	bf85                	j	ffffffffc0202076 <page_insert+0x3a>
        intr_disable();
ffffffffc0202108:	829fe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        pmm_manager->free_pages(base, n);
ffffffffc020210c:	0000b797          	auipc	a5,0xb
ffffffffc0202110:	3ac7b783          	ld	a5,940(a5) # ffffffffc020d4b8 <pmm_manager>
ffffffffc0202114:	739c                	ld	a5,32(a5)
ffffffffc0202116:	4585                	li	a1,1
ffffffffc0202118:	854a                	mv	a0,s2
ffffffffc020211a:	9782                	jalr	a5
        intr_enable();
ffffffffc020211c:	80ffe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc0202120:	000ab703          	ld	a4,0(s5)
    asm volatile("sfence.vma %0" : : "r"(la));
ffffffffc0202124:	120a0073          	sfence.vma	s4
ffffffffc0202128:	b7b9                	j	ffffffffc0202076 <page_insert+0x3a>
        return -E_NO_MEM;
ffffffffc020212a:	5571                	li	a0,-4
ffffffffc020212c:	b79d                	j	ffffffffc0202092 <page_insert+0x56>
ffffffffc020212e:	b07ff0ef          	jal	ra,ffffffffc0201c34 <pa2page.part.0>

ffffffffc0202132 <pmm_init>:
    pmm_manager = &default_pmm_manager;
ffffffffc0202132:	00003797          	auipc	a5,0x3
ffffffffc0202136:	bae78793          	addi	a5,a5,-1106 # ffffffffc0204ce0 <default_pmm_manager>
    cprintf("memory management: %s\n", pmm_manager->name);
ffffffffc020213a:	638c                	ld	a1,0(a5)
{
ffffffffc020213c:	7159                	addi	sp,sp,-112
ffffffffc020213e:	f85a                	sd	s6,48(sp)
    cprintf("memory management: %s\n", pmm_manager->name);
ffffffffc0202140:	00003517          	auipc	a0,0x3
ffffffffc0202144:	d0050513          	addi	a0,a0,-768 # ffffffffc0204e40 <default_pmm_manager+0x160>
    pmm_manager = &default_pmm_manager;
ffffffffc0202148:	0000bb17          	auipc	s6,0xb
ffffffffc020214c:	370b0b13          	addi	s6,s6,880 # ffffffffc020d4b8 <pmm_manager>
{
ffffffffc0202150:	f486                	sd	ra,104(sp)
ffffffffc0202152:	e8ca                	sd	s2,80(sp)
ffffffffc0202154:	e4ce                	sd	s3,72(sp)
ffffffffc0202156:	f0a2                	sd	s0,96(sp)
ffffffffc0202158:	eca6                	sd	s1,88(sp)
ffffffffc020215a:	e0d2                	sd	s4,64(sp)
ffffffffc020215c:	fc56                	sd	s5,56(sp)
ffffffffc020215e:	f45e                	sd	s7,40(sp)
ffffffffc0202160:	f062                	sd	s8,32(sp)
ffffffffc0202162:	ec66                	sd	s9,24(sp)
    pmm_manager = &default_pmm_manager;
ffffffffc0202164:	00fb3023          	sd	a5,0(s6)
    cprintf("memory management: %s\n", pmm_manager->name);
ffffffffc0202168:	82cfe0ef          	jal	ra,ffffffffc0200194 <cprintf>
    pmm_manager->init();
ffffffffc020216c:	000b3783          	ld	a5,0(s6)
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
ffffffffc0202170:	0000b997          	auipc	s3,0xb
ffffffffc0202174:	35098993          	addi	s3,s3,848 # ffffffffc020d4c0 <va_pa_offset>
    pmm_manager->init();
ffffffffc0202178:	679c                	ld	a5,8(a5)
ffffffffc020217a:	9782                	jalr	a5
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
ffffffffc020217c:	57f5                	li	a5,-3
ffffffffc020217e:	07fa                	slli	a5,a5,0x1e
ffffffffc0202180:	00f9b023          	sd	a5,0(s3)
    uint64_t mem_begin = get_memory_base();
ffffffffc0202184:	f92fe0ef          	jal	ra,ffffffffc0200916 <get_memory_base>
ffffffffc0202188:	892a                	mv	s2,a0
    uint64_t mem_size  = get_memory_size();
ffffffffc020218a:	f96fe0ef          	jal	ra,ffffffffc0200920 <get_memory_size>
    if (mem_size == 0) {
ffffffffc020218e:	200505e3          	beqz	a0,ffffffffc0202b98 <pmm_init+0xa66>
    uint64_t mem_end   = mem_begin + mem_size;
ffffffffc0202192:	84aa                	mv	s1,a0
    cprintf("physcial memory map:\n");
ffffffffc0202194:	00003517          	auipc	a0,0x3
ffffffffc0202198:	ce450513          	addi	a0,a0,-796 # ffffffffc0204e78 <default_pmm_manager+0x198>
ffffffffc020219c:	ff9fd0ef          	jal	ra,ffffffffc0200194 <cprintf>
    uint64_t mem_end   = mem_begin + mem_size;
ffffffffc02021a0:	00990433          	add	s0,s2,s1
    cprintf("  memory: 0x%08lx, [0x%08lx, 0x%08lx].\n", mem_size, mem_begin,
ffffffffc02021a4:	fff40693          	addi	a3,s0,-1
ffffffffc02021a8:	864a                	mv	a2,s2
ffffffffc02021aa:	85a6                	mv	a1,s1
ffffffffc02021ac:	00003517          	auipc	a0,0x3
ffffffffc02021b0:	ce450513          	addi	a0,a0,-796 # ffffffffc0204e90 <default_pmm_manager+0x1b0>
ffffffffc02021b4:	fe1fd0ef          	jal	ra,ffffffffc0200194 <cprintf>
    npage = maxpa / PGSIZE;
ffffffffc02021b8:	c8000737          	lui	a4,0xc8000
ffffffffc02021bc:	87a2                	mv	a5,s0
ffffffffc02021be:	54876163          	bltu	a4,s0,ffffffffc0202700 <pmm_init+0x5ce>
ffffffffc02021c2:	757d                	lui	a0,0xfffff
ffffffffc02021c4:	0000c617          	auipc	a2,0xc
ffffffffc02021c8:	31f60613          	addi	a2,a2,799 # ffffffffc020e4e3 <end+0xfff>
ffffffffc02021cc:	8e69                	and	a2,a2,a0
ffffffffc02021ce:	0000b497          	auipc	s1,0xb
ffffffffc02021d2:	2da48493          	addi	s1,s1,730 # ffffffffc020d4a8 <npage>
ffffffffc02021d6:	00c7d513          	srli	a0,a5,0xc
    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE);
ffffffffc02021da:	0000bb97          	auipc	s7,0xb
ffffffffc02021de:	2d6b8b93          	addi	s7,s7,726 # ffffffffc020d4b0 <pages>
    npage = maxpa / PGSIZE;
ffffffffc02021e2:	e088                	sd	a0,0(s1)
    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE);
ffffffffc02021e4:	00cbb023          	sd	a2,0(s7)
    for (size_t i = 0; i < npage - nbase; i++)
ffffffffc02021e8:	000807b7          	lui	a5,0x80
    pages = (struct Page *)ROUNDUP((void *)end, PGSIZE);
ffffffffc02021ec:	86b2                	mv	a3,a2
    for (size_t i = 0; i < npage - nbase; i++)
ffffffffc02021ee:	02f50863          	beq	a0,a5,ffffffffc020221e <pmm_init+0xec>
ffffffffc02021f2:	4781                	li	a5,0
ffffffffc02021f4:	4585                	li	a1,1
ffffffffc02021f6:	fff806b7          	lui	a3,0xfff80
        SetPageReserved(pages + i);
ffffffffc02021fa:	00679513          	slli	a0,a5,0x6
ffffffffc02021fe:	9532                	add	a0,a0,a2
ffffffffc0202200:	00850713          	addi	a4,a0,8 # fffffffffffff008 <end+0x3fdf1b24>
ffffffffc0202204:	40b7302f          	amoor.d	zero,a1,(a4)
    for (size_t i = 0; i < npage - nbase; i++)
ffffffffc0202208:	6088                	ld	a0,0(s1)
ffffffffc020220a:	0785                	addi	a5,a5,1
        SetPageReserved(pages + i);
ffffffffc020220c:	000bb603          	ld	a2,0(s7)
    for (size_t i = 0; i < npage - nbase; i++)
ffffffffc0202210:	00d50733          	add	a4,a0,a3
ffffffffc0202214:	fee7e3e3          	bltu	a5,a4,ffffffffc02021fa <pmm_init+0xc8>
    uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * (npage - nbase));
ffffffffc0202218:	071a                	slli	a4,a4,0x6
ffffffffc020221a:	00e606b3          	add	a3,a2,a4
ffffffffc020221e:	c02007b7          	lui	a5,0xc0200
ffffffffc0202222:	2ef6ece3          	bltu	a3,a5,ffffffffc0202d1a <pmm_init+0xbe8>
ffffffffc0202226:	0009b583          	ld	a1,0(s3)
    mem_end = ROUNDDOWN(mem_end, PGSIZE);
ffffffffc020222a:	77fd                	lui	a5,0xfffff
ffffffffc020222c:	8c7d                	and	s0,s0,a5
    uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * (npage - nbase));
ffffffffc020222e:	8e8d                	sub	a3,a3,a1
    if (freemem < mem_end)
ffffffffc0202230:	5086eb63          	bltu	a3,s0,ffffffffc0202746 <pmm_init+0x614>
    cprintf("vapaofset is %llu\n", va_pa_offset);
ffffffffc0202234:	00003517          	auipc	a0,0x3
ffffffffc0202238:	c8450513          	addi	a0,a0,-892 # ffffffffc0204eb8 <default_pmm_manager+0x1d8>
ffffffffc020223c:	f59fd0ef          	jal	ra,ffffffffc0200194 <cprintf>
}

static void check_alloc_page(void)
{
    pmm_manager->check();
ffffffffc0202240:	000b3783          	ld	a5,0(s6)
    boot_pgdir_va = (pte_t *)boot_page_table_sv39;
ffffffffc0202244:	0000b917          	auipc	s2,0xb
ffffffffc0202248:	25c90913          	addi	s2,s2,604 # ffffffffc020d4a0 <boot_pgdir_va>
    pmm_manager->check();
ffffffffc020224c:	7b9c                	ld	a5,48(a5)
ffffffffc020224e:	9782                	jalr	a5
    cprintf("check_alloc_page() succeeded!\n");
ffffffffc0202250:	00003517          	auipc	a0,0x3
ffffffffc0202254:	c8050513          	addi	a0,a0,-896 # ffffffffc0204ed0 <default_pmm_manager+0x1f0>
ffffffffc0202258:	f3dfd0ef          	jal	ra,ffffffffc0200194 <cprintf>
    boot_pgdir_va = (pte_t *)boot_page_table_sv39;
ffffffffc020225c:	00006697          	auipc	a3,0x6
ffffffffc0202260:	da468693          	addi	a3,a3,-604 # ffffffffc0208000 <boot_page_table_sv39>
ffffffffc0202264:	00d93023          	sd	a3,0(s2)
    boot_pgdir_pa = PADDR(boot_pgdir_va);
ffffffffc0202268:	c02007b7          	lui	a5,0xc0200
ffffffffc020226c:	28f6ebe3          	bltu	a3,a5,ffffffffc0202d02 <pmm_init+0xbd0>
ffffffffc0202270:	0009b783          	ld	a5,0(s3)
ffffffffc0202274:	8e9d                	sub	a3,a3,a5
ffffffffc0202276:	0000b797          	auipc	a5,0xb
ffffffffc020227a:	22d7b123          	sd	a3,546(a5) # ffffffffc020d498 <boot_pgdir_pa>
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc020227e:	100027f3          	csrr	a5,sstatus
ffffffffc0202282:	8b89                	andi	a5,a5,2
ffffffffc0202284:	4a079763          	bnez	a5,ffffffffc0202732 <pmm_init+0x600>
        ret = pmm_manager->nr_free_pages();
ffffffffc0202288:	000b3783          	ld	a5,0(s6)
ffffffffc020228c:	779c                	ld	a5,40(a5)
ffffffffc020228e:	9782                	jalr	a5
ffffffffc0202290:	842a                	mv	s0,a0
    // so npage is always larger than KMEMSIZE / PGSIZE
    size_t nr_free_store;

    nr_free_store = nr_free_pages();

    assert(npage <= KERNTOP / PGSIZE);
ffffffffc0202292:	6098                	ld	a4,0(s1)
ffffffffc0202294:	c80007b7          	lui	a5,0xc8000
ffffffffc0202298:	83b1                	srli	a5,a5,0xc
ffffffffc020229a:	66e7e363          	bltu	a5,a4,ffffffffc0202900 <pmm_init+0x7ce>
    assert(boot_pgdir_va != NULL && (uint32_t)PGOFF(boot_pgdir_va) == 0);
ffffffffc020229e:	00093503          	ld	a0,0(s2)
ffffffffc02022a2:	62050f63          	beqz	a0,ffffffffc02028e0 <pmm_init+0x7ae>
ffffffffc02022a6:	03451793          	slli	a5,a0,0x34
ffffffffc02022aa:	62079b63          	bnez	a5,ffffffffc02028e0 <pmm_init+0x7ae>
    assert(get_page(boot_pgdir_va, 0x0, NULL) == NULL);
ffffffffc02022ae:	4601                	li	a2,0
ffffffffc02022b0:	4581                	li	a1,0
ffffffffc02022b2:	c9bff0ef          	jal	ra,ffffffffc0201f4c <get_page>
ffffffffc02022b6:	60051563          	bnez	a0,ffffffffc02028c0 <pmm_init+0x78e>
ffffffffc02022ba:	100027f3          	csrr	a5,sstatus
ffffffffc02022be:	8b89                	andi	a5,a5,2
ffffffffc02022c0:	44079e63          	bnez	a5,ffffffffc020271c <pmm_init+0x5ea>
        page = pmm_manager->alloc_pages(n);
ffffffffc02022c4:	000b3783          	ld	a5,0(s6)
ffffffffc02022c8:	4505                	li	a0,1
ffffffffc02022ca:	6f9c                	ld	a5,24(a5)
ffffffffc02022cc:	9782                	jalr	a5
ffffffffc02022ce:	8a2a                	mv	s4,a0

    struct Page *p1, *p2;
    p1 = alloc_page();
    assert(page_insert(boot_pgdir_va, p1, 0x0, 0) == 0);
ffffffffc02022d0:	00093503          	ld	a0,0(s2)
ffffffffc02022d4:	4681                	li	a3,0
ffffffffc02022d6:	4601                	li	a2,0
ffffffffc02022d8:	85d2                	mv	a1,s4
ffffffffc02022da:	d63ff0ef          	jal	ra,ffffffffc020203c <page_insert>
ffffffffc02022de:	26051ae3          	bnez	a0,ffffffffc0202d52 <pmm_init+0xc20>

    pte_t *ptep;
    assert((ptep = get_pte(boot_pgdir_va, 0x0, 0)) != NULL);
ffffffffc02022e2:	00093503          	ld	a0,0(s2)
ffffffffc02022e6:	4601                	li	a2,0
ffffffffc02022e8:	4581                	li	a1,0
ffffffffc02022ea:	a3bff0ef          	jal	ra,ffffffffc0201d24 <get_pte>
ffffffffc02022ee:	240502e3          	beqz	a0,ffffffffc0202d32 <pmm_init+0xc00>
    assert(pte2page(*ptep) == p1);
ffffffffc02022f2:	611c                	ld	a5,0(a0)
    if (!(pte & PTE_V))
ffffffffc02022f4:	0017f713          	andi	a4,a5,1
ffffffffc02022f8:	5a070263          	beqz	a4,ffffffffc020289c <pmm_init+0x76a>
    if (PPN(pa) >= npage)
ffffffffc02022fc:	6098                	ld	a4,0(s1)
    return pa2page(PTE_ADDR(pte));
ffffffffc02022fe:	078a                	slli	a5,a5,0x2
ffffffffc0202300:	83b1                	srli	a5,a5,0xc
    if (PPN(pa) >= npage)
ffffffffc0202302:	58e7fb63          	bgeu	a5,a4,ffffffffc0202898 <pmm_init+0x766>
    return &pages[PPN(pa) - nbase];
ffffffffc0202306:	000bb683          	ld	a3,0(s7)
ffffffffc020230a:	fff80637          	lui	a2,0xfff80
ffffffffc020230e:	97b2                	add	a5,a5,a2
ffffffffc0202310:	079a                	slli	a5,a5,0x6
ffffffffc0202312:	97b6                	add	a5,a5,a3
ffffffffc0202314:	14fa17e3          	bne	s4,a5,ffffffffc0202c62 <pmm_init+0xb30>
    assert(page_ref(p1) == 1);
ffffffffc0202318:	000a2683          	lw	a3,0(s4) # 80000 <kern_entry-0xffffffffc0180000>
ffffffffc020231c:	4785                	li	a5,1
ffffffffc020231e:	12f692e3          	bne	a3,a5,ffffffffc0202c42 <pmm_init+0xb10>

    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir_va[0]));
ffffffffc0202322:	00093503          	ld	a0,0(s2)
ffffffffc0202326:	77fd                	lui	a5,0xfffff
ffffffffc0202328:	6114                	ld	a3,0(a0)
ffffffffc020232a:	068a                	slli	a3,a3,0x2
ffffffffc020232c:	8efd                	and	a3,a3,a5
ffffffffc020232e:	00c6d613          	srli	a2,a3,0xc
ffffffffc0202332:	0ee67ce3          	bgeu	a2,a4,ffffffffc0202c2a <pmm_init+0xaf8>
ffffffffc0202336:	0009bc03          	ld	s8,0(s3)
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
ffffffffc020233a:	96e2                	add	a3,a3,s8
ffffffffc020233c:	0006ba83          	ld	s5,0(a3)
ffffffffc0202340:	0a8a                	slli	s5,s5,0x2
ffffffffc0202342:	00fafab3          	and	s5,s5,a5
ffffffffc0202346:	00cad793          	srli	a5,s5,0xc
ffffffffc020234a:	0ce7f3e3          	bgeu	a5,a4,ffffffffc0202c10 <pmm_init+0xade>
    assert(get_pte(boot_pgdir_va, PGSIZE, 0) == ptep);
ffffffffc020234e:	4601                	li	a2,0
ffffffffc0202350:	6585                	lui	a1,0x1
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
ffffffffc0202352:	9ae2                	add	s5,s5,s8
    assert(get_pte(boot_pgdir_va, PGSIZE, 0) == ptep);
ffffffffc0202354:	9d1ff0ef          	jal	ra,ffffffffc0201d24 <get_pte>
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
ffffffffc0202358:	0aa1                	addi	s5,s5,8
    assert(get_pte(boot_pgdir_va, PGSIZE, 0) == ptep);
ffffffffc020235a:	55551363          	bne	a0,s5,ffffffffc02028a0 <pmm_init+0x76e>
ffffffffc020235e:	100027f3          	csrr	a5,sstatus
ffffffffc0202362:	8b89                	andi	a5,a5,2
ffffffffc0202364:	3a079163          	bnez	a5,ffffffffc0202706 <pmm_init+0x5d4>
        page = pmm_manager->alloc_pages(n);
ffffffffc0202368:	000b3783          	ld	a5,0(s6)
ffffffffc020236c:	4505                	li	a0,1
ffffffffc020236e:	6f9c                	ld	a5,24(a5)
ffffffffc0202370:	9782                	jalr	a5
ffffffffc0202372:	8c2a                	mv	s8,a0

    p2 = alloc_page();
    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
ffffffffc0202374:	00093503          	ld	a0,0(s2)
ffffffffc0202378:	46d1                	li	a3,20
ffffffffc020237a:	6605                	lui	a2,0x1
ffffffffc020237c:	85e2                	mv	a1,s8
ffffffffc020237e:	cbfff0ef          	jal	ra,ffffffffc020203c <page_insert>
ffffffffc0202382:	060517e3          	bnez	a0,ffffffffc0202bf0 <pmm_init+0xabe>
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
ffffffffc0202386:	00093503          	ld	a0,0(s2)
ffffffffc020238a:	4601                	li	a2,0
ffffffffc020238c:	6585                	lui	a1,0x1
ffffffffc020238e:	997ff0ef          	jal	ra,ffffffffc0201d24 <get_pte>
ffffffffc0202392:	02050fe3          	beqz	a0,ffffffffc0202bd0 <pmm_init+0xa9e>
    assert(*ptep & PTE_U);
ffffffffc0202396:	611c                	ld	a5,0(a0)
ffffffffc0202398:	0107f713          	andi	a4,a5,16
ffffffffc020239c:	7c070e63          	beqz	a4,ffffffffc0202b78 <pmm_init+0xa46>
    assert(*ptep & PTE_W);
ffffffffc02023a0:	8b91                	andi	a5,a5,4
ffffffffc02023a2:	7a078b63          	beqz	a5,ffffffffc0202b58 <pmm_init+0xa26>
    assert(boot_pgdir_va[0] & PTE_U);
ffffffffc02023a6:	00093503          	ld	a0,0(s2)
ffffffffc02023aa:	611c                	ld	a5,0(a0)
ffffffffc02023ac:	8bc1                	andi	a5,a5,16
ffffffffc02023ae:	78078563          	beqz	a5,ffffffffc0202b38 <pmm_init+0xa06>
    assert(page_ref(p2) == 1);
ffffffffc02023b2:	000c2703          	lw	a4,0(s8) # ff0000 <kern_entry-0xffffffffbf210000>
ffffffffc02023b6:	4785                	li	a5,1
ffffffffc02023b8:	76f71063          	bne	a4,a5,ffffffffc0202b18 <pmm_init+0x9e6>

    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
ffffffffc02023bc:	4681                	li	a3,0
ffffffffc02023be:	6605                	lui	a2,0x1
ffffffffc02023c0:	85d2                	mv	a1,s4
ffffffffc02023c2:	c7bff0ef          	jal	ra,ffffffffc020203c <page_insert>
ffffffffc02023c6:	72051963          	bnez	a0,ffffffffc0202af8 <pmm_init+0x9c6>
    assert(page_ref(p1) == 2);
ffffffffc02023ca:	000a2703          	lw	a4,0(s4)
ffffffffc02023ce:	4789                	li	a5,2
ffffffffc02023d0:	70f71463          	bne	a4,a5,ffffffffc0202ad8 <pmm_init+0x9a6>
    assert(page_ref(p2) == 0);
ffffffffc02023d4:	000c2783          	lw	a5,0(s8)
ffffffffc02023d8:	6e079063          	bnez	a5,ffffffffc0202ab8 <pmm_init+0x986>
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
ffffffffc02023dc:	00093503          	ld	a0,0(s2)
ffffffffc02023e0:	4601                	li	a2,0
ffffffffc02023e2:	6585                	lui	a1,0x1
ffffffffc02023e4:	941ff0ef          	jal	ra,ffffffffc0201d24 <get_pte>
ffffffffc02023e8:	6a050863          	beqz	a0,ffffffffc0202a98 <pmm_init+0x966>
    assert(pte2page(*ptep) == p1);
ffffffffc02023ec:	6118                	ld	a4,0(a0)
    if (!(pte & PTE_V))
ffffffffc02023ee:	00177793          	andi	a5,a4,1
ffffffffc02023f2:	4a078563          	beqz	a5,ffffffffc020289c <pmm_init+0x76a>
    if (PPN(pa) >= npage)
ffffffffc02023f6:	6094                	ld	a3,0(s1)
    return pa2page(PTE_ADDR(pte));
ffffffffc02023f8:	00271793          	slli	a5,a4,0x2
ffffffffc02023fc:	83b1                	srli	a5,a5,0xc
    if (PPN(pa) >= npage)
ffffffffc02023fe:	48d7fd63          	bgeu	a5,a3,ffffffffc0202898 <pmm_init+0x766>
    return &pages[PPN(pa) - nbase];
ffffffffc0202402:	000bb683          	ld	a3,0(s7)
ffffffffc0202406:	fff80ab7          	lui	s5,0xfff80
ffffffffc020240a:	97d6                	add	a5,a5,s5
ffffffffc020240c:	079a                	slli	a5,a5,0x6
ffffffffc020240e:	97b6                	add	a5,a5,a3
ffffffffc0202410:	66fa1463          	bne	s4,a5,ffffffffc0202a78 <pmm_init+0x946>
    assert((*ptep & PTE_U) == 0);
ffffffffc0202414:	8b41                	andi	a4,a4,16
ffffffffc0202416:	64071163          	bnez	a4,ffffffffc0202a58 <pmm_init+0x926>

    page_remove(boot_pgdir_va, 0x0);
ffffffffc020241a:	00093503          	ld	a0,0(s2)
ffffffffc020241e:	4581                	li	a1,0
ffffffffc0202420:	b81ff0ef          	jal	ra,ffffffffc0201fa0 <page_remove>
    assert(page_ref(p1) == 1);
ffffffffc0202424:	000a2c83          	lw	s9,0(s4)
ffffffffc0202428:	4785                	li	a5,1
ffffffffc020242a:	60fc9763          	bne	s9,a5,ffffffffc0202a38 <pmm_init+0x906>
    assert(page_ref(p2) == 0);
ffffffffc020242e:	000c2783          	lw	a5,0(s8)
ffffffffc0202432:	5e079363          	bnez	a5,ffffffffc0202a18 <pmm_init+0x8e6>

    page_remove(boot_pgdir_va, PGSIZE);
ffffffffc0202436:	00093503          	ld	a0,0(s2)
ffffffffc020243a:	6585                	lui	a1,0x1
ffffffffc020243c:	b65ff0ef          	jal	ra,ffffffffc0201fa0 <page_remove>
    assert(page_ref(p1) == 0);
ffffffffc0202440:	000a2783          	lw	a5,0(s4)
ffffffffc0202444:	52079a63          	bnez	a5,ffffffffc0202978 <pmm_init+0x846>
    assert(page_ref(p2) == 0);
ffffffffc0202448:	000c2783          	lw	a5,0(s8)
ffffffffc020244c:	50079663          	bnez	a5,ffffffffc0202958 <pmm_init+0x826>

    assert(page_ref(pde2page(boot_pgdir_va[0])) == 1);
ffffffffc0202450:	00093a03          	ld	s4,0(s2)
    if (PPN(pa) >= npage)
ffffffffc0202454:	608c                	ld	a1,0(s1)
    return pa2page(PDE_ADDR(pde));
ffffffffc0202456:	000a3683          	ld	a3,0(s4)
ffffffffc020245a:	068a                	slli	a3,a3,0x2
ffffffffc020245c:	82b1                	srli	a3,a3,0xc
    if (PPN(pa) >= npage)
ffffffffc020245e:	42b6fd63          	bgeu	a3,a1,ffffffffc0202898 <pmm_init+0x766>
    return &pages[PPN(pa) - nbase];
ffffffffc0202462:	000bb503          	ld	a0,0(s7)
ffffffffc0202466:	96d6                	add	a3,a3,s5
ffffffffc0202468:	069a                	slli	a3,a3,0x6
    return page->ref;
ffffffffc020246a:	00d507b3          	add	a5,a0,a3
ffffffffc020246e:	439c                	lw	a5,0(a5)
ffffffffc0202470:	4d979463          	bne	a5,s9,ffffffffc0202938 <pmm_init+0x806>
    return page - pages + nbase;
ffffffffc0202474:	8699                	srai	a3,a3,0x6
ffffffffc0202476:	00080637          	lui	a2,0x80
ffffffffc020247a:	96b2                	add	a3,a3,a2
    return KADDR(page2pa(page));
ffffffffc020247c:	00c69713          	slli	a4,a3,0xc
ffffffffc0202480:	8331                	srli	a4,a4,0xc
    return page2ppn(page) << PGSHIFT;
ffffffffc0202482:	06b2                	slli	a3,a3,0xc
    return KADDR(page2pa(page));
ffffffffc0202484:	48b77e63          	bgeu	a4,a1,ffffffffc0202920 <pmm_init+0x7ee>

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
    free_page(pde2page(pd0[0]));
ffffffffc0202488:	0009b703          	ld	a4,0(s3)
ffffffffc020248c:	96ba                	add	a3,a3,a4
    return pa2page(PDE_ADDR(pde));
ffffffffc020248e:	629c                	ld	a5,0(a3)
ffffffffc0202490:	078a                	slli	a5,a5,0x2
ffffffffc0202492:	83b1                	srli	a5,a5,0xc
    if (PPN(pa) >= npage)
ffffffffc0202494:	40b7f263          	bgeu	a5,a1,ffffffffc0202898 <pmm_init+0x766>
    return &pages[PPN(pa) - nbase];
ffffffffc0202498:	8f91                	sub	a5,a5,a2
ffffffffc020249a:	079a                	slli	a5,a5,0x6
ffffffffc020249c:	953e                	add	a0,a0,a5
ffffffffc020249e:	100027f3          	csrr	a5,sstatus
ffffffffc02024a2:	8b89                	andi	a5,a5,2
ffffffffc02024a4:	30079963          	bnez	a5,ffffffffc02027b6 <pmm_init+0x684>
        pmm_manager->free_pages(base, n);
ffffffffc02024a8:	000b3783          	ld	a5,0(s6)
ffffffffc02024ac:	4585                	li	a1,1
ffffffffc02024ae:	739c                	ld	a5,32(a5)
ffffffffc02024b0:	9782                	jalr	a5
    return pa2page(PDE_ADDR(pde));
ffffffffc02024b2:	000a3783          	ld	a5,0(s4)
    if (PPN(pa) >= npage)
ffffffffc02024b6:	6098                	ld	a4,0(s1)
    return pa2page(PDE_ADDR(pde));
ffffffffc02024b8:	078a                	slli	a5,a5,0x2
ffffffffc02024ba:	83b1                	srli	a5,a5,0xc
    if (PPN(pa) >= npage)
ffffffffc02024bc:	3ce7fe63          	bgeu	a5,a4,ffffffffc0202898 <pmm_init+0x766>
    return &pages[PPN(pa) - nbase];
ffffffffc02024c0:	000bb503          	ld	a0,0(s7)
ffffffffc02024c4:	fff80737          	lui	a4,0xfff80
ffffffffc02024c8:	97ba                	add	a5,a5,a4
ffffffffc02024ca:	079a                	slli	a5,a5,0x6
ffffffffc02024cc:	953e                	add	a0,a0,a5
ffffffffc02024ce:	100027f3          	csrr	a5,sstatus
ffffffffc02024d2:	8b89                	andi	a5,a5,2
ffffffffc02024d4:	2c079563          	bnez	a5,ffffffffc020279e <pmm_init+0x66c>
ffffffffc02024d8:	000b3783          	ld	a5,0(s6)
ffffffffc02024dc:	4585                	li	a1,1
ffffffffc02024de:	739c                	ld	a5,32(a5)
ffffffffc02024e0:	9782                	jalr	a5
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
ffffffffc02024e2:	00093783          	ld	a5,0(s2)
ffffffffc02024e6:	0007b023          	sd	zero,0(a5) # fffffffffffff000 <end+0x3fdf1b1c>
    asm volatile("sfence.vma");
ffffffffc02024ea:	12000073          	sfence.vma
ffffffffc02024ee:	100027f3          	csrr	a5,sstatus
ffffffffc02024f2:	8b89                	andi	a5,a5,2
ffffffffc02024f4:	28079b63          	bnez	a5,ffffffffc020278a <pmm_init+0x658>
        ret = pmm_manager->nr_free_pages();
ffffffffc02024f8:	000b3783          	ld	a5,0(s6)
ffffffffc02024fc:	779c                	ld	a5,40(a5)
ffffffffc02024fe:	9782                	jalr	a5
ffffffffc0202500:	8a2a                	mv	s4,a0
    flush_tlb();

    assert(nr_free_store == nr_free_pages());
ffffffffc0202502:	4b441b63          	bne	s0,s4,ffffffffc02029b8 <pmm_init+0x886>

    cprintf("check_pgdir() succeeded!\n");
ffffffffc0202506:	00003517          	auipc	a0,0x3
ffffffffc020250a:	cf250513          	addi	a0,a0,-782 # ffffffffc02051f8 <default_pmm_manager+0x518>
ffffffffc020250e:	c87fd0ef          	jal	ra,ffffffffc0200194 <cprintf>
ffffffffc0202512:	100027f3          	csrr	a5,sstatus
ffffffffc0202516:	8b89                	andi	a5,a5,2
ffffffffc0202518:	24079f63          	bnez	a5,ffffffffc0202776 <pmm_init+0x644>
        ret = pmm_manager->nr_free_pages();
ffffffffc020251c:	000b3783          	ld	a5,0(s6)
ffffffffc0202520:	779c                	ld	a5,40(a5)
ffffffffc0202522:	9782                	jalr	a5
ffffffffc0202524:	8c2a                	mv	s8,a0
    pte_t *ptep;
    int i;

    nr_free_store = nr_free_pages();

    for (i = ROUNDDOWN(KERNBASE, PGSIZE); i < npage * PGSIZE; i += PGSIZE)
ffffffffc0202526:	6098                	ld	a4,0(s1)
ffffffffc0202528:	c0200437          	lui	s0,0xc0200
    {
        assert((ptep = get_pte(boot_pgdir_va, (uintptr_t)KADDR(i), 0)) != NULL);
        assert(PTE_ADDR(*ptep) == i);
ffffffffc020252c:	7afd                	lui	s5,0xfffff
    for (i = ROUNDDOWN(KERNBASE, PGSIZE); i < npage * PGSIZE; i += PGSIZE)
ffffffffc020252e:	00c71793          	slli	a5,a4,0xc
ffffffffc0202532:	6a05                	lui	s4,0x1
ffffffffc0202534:	02f47c63          	bgeu	s0,a5,ffffffffc020256c <pmm_init+0x43a>
        assert((ptep = get_pte(boot_pgdir_va, (uintptr_t)KADDR(i), 0)) != NULL);
ffffffffc0202538:	00c45793          	srli	a5,s0,0xc
ffffffffc020253c:	00093503          	ld	a0,0(s2)
ffffffffc0202540:	2ee7ff63          	bgeu	a5,a4,ffffffffc020283e <pmm_init+0x70c>
ffffffffc0202544:	0009b583          	ld	a1,0(s3)
ffffffffc0202548:	4601                	li	a2,0
ffffffffc020254a:	95a2                	add	a1,a1,s0
ffffffffc020254c:	fd8ff0ef          	jal	ra,ffffffffc0201d24 <get_pte>
ffffffffc0202550:	32050463          	beqz	a0,ffffffffc0202878 <pmm_init+0x746>
        assert(PTE_ADDR(*ptep) == i);
ffffffffc0202554:	611c                	ld	a5,0(a0)
ffffffffc0202556:	078a                	slli	a5,a5,0x2
ffffffffc0202558:	0157f7b3          	and	a5,a5,s5
ffffffffc020255c:	2e879e63          	bne	a5,s0,ffffffffc0202858 <pmm_init+0x726>
    for (i = ROUNDDOWN(KERNBASE, PGSIZE); i < npage * PGSIZE; i += PGSIZE)
ffffffffc0202560:	6098                	ld	a4,0(s1)
ffffffffc0202562:	9452                	add	s0,s0,s4
ffffffffc0202564:	00c71793          	slli	a5,a4,0xc
ffffffffc0202568:	fcf468e3          	bltu	s0,a5,ffffffffc0202538 <pmm_init+0x406>
    }

    assert(boot_pgdir_va[0] == 0);
ffffffffc020256c:	00093783          	ld	a5,0(s2)
ffffffffc0202570:	639c                	ld	a5,0(a5)
ffffffffc0202572:	42079363          	bnez	a5,ffffffffc0202998 <pmm_init+0x866>
ffffffffc0202576:	100027f3          	csrr	a5,sstatus
ffffffffc020257a:	8b89                	andi	a5,a5,2
ffffffffc020257c:	24079963          	bnez	a5,ffffffffc02027ce <pmm_init+0x69c>
        page = pmm_manager->alloc_pages(n);
ffffffffc0202580:	000b3783          	ld	a5,0(s6)
ffffffffc0202584:	4505                	li	a0,1
ffffffffc0202586:	6f9c                	ld	a5,24(a5)
ffffffffc0202588:	9782                	jalr	a5
ffffffffc020258a:	8a2a                	mv	s4,a0

    struct Page *p;
    p = alloc_page();
    assert(page_insert(boot_pgdir_va, p, 0x100, PTE_W | PTE_R) == 0);
ffffffffc020258c:	00093503          	ld	a0,0(s2)
ffffffffc0202590:	4699                	li	a3,6
ffffffffc0202592:	10000613          	li	a2,256
ffffffffc0202596:	85d2                	mv	a1,s4
ffffffffc0202598:	aa5ff0ef          	jal	ra,ffffffffc020203c <page_insert>
ffffffffc020259c:	44051e63          	bnez	a0,ffffffffc02029f8 <pmm_init+0x8c6>
    assert(page_ref(p) == 1);
ffffffffc02025a0:	000a2703          	lw	a4,0(s4) # 1000 <kern_entry-0xffffffffc01ff000>
ffffffffc02025a4:	4785                	li	a5,1
ffffffffc02025a6:	42f71963          	bne	a4,a5,ffffffffc02029d8 <pmm_init+0x8a6>
    assert(page_insert(boot_pgdir_va, p, 0x100 + PGSIZE, PTE_W | PTE_R) == 0);
ffffffffc02025aa:	00093503          	ld	a0,0(s2)
ffffffffc02025ae:	6405                	lui	s0,0x1
ffffffffc02025b0:	4699                	li	a3,6
ffffffffc02025b2:	10040613          	addi	a2,s0,256 # 1100 <kern_entry-0xffffffffc01fef00>
ffffffffc02025b6:	85d2                	mv	a1,s4
ffffffffc02025b8:	a85ff0ef          	jal	ra,ffffffffc020203c <page_insert>
ffffffffc02025bc:	72051363          	bnez	a0,ffffffffc0202ce2 <pmm_init+0xbb0>
    assert(page_ref(p) == 2);
ffffffffc02025c0:	000a2703          	lw	a4,0(s4)
ffffffffc02025c4:	4789                	li	a5,2
ffffffffc02025c6:	6ef71e63          	bne	a4,a5,ffffffffc0202cc2 <pmm_init+0xb90>

    const char *str = "ucore: Hello world!!";
    strcpy((void *)0x100, str);
ffffffffc02025ca:	00003597          	auipc	a1,0x3
ffffffffc02025ce:	d7658593          	addi	a1,a1,-650 # ffffffffc0205340 <default_pmm_manager+0x660>
ffffffffc02025d2:	10000513          	li	a0,256
ffffffffc02025d6:	01f010ef          	jal	ra,ffffffffc0203df4 <strcpy>
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);
ffffffffc02025da:	10040593          	addi	a1,s0,256
ffffffffc02025de:	10000513          	li	a0,256
ffffffffc02025e2:	025010ef          	jal	ra,ffffffffc0203e06 <strcmp>
ffffffffc02025e6:	6a051e63          	bnez	a0,ffffffffc0202ca2 <pmm_init+0xb70>
    return page - pages + nbase;
ffffffffc02025ea:	000bb683          	ld	a3,0(s7)
ffffffffc02025ee:	00080737          	lui	a4,0x80
    return KADDR(page2pa(page));
ffffffffc02025f2:	547d                	li	s0,-1
    return page - pages + nbase;
ffffffffc02025f4:	40da06b3          	sub	a3,s4,a3
ffffffffc02025f8:	8699                	srai	a3,a3,0x6
    return KADDR(page2pa(page));
ffffffffc02025fa:	609c                	ld	a5,0(s1)
    return page - pages + nbase;
ffffffffc02025fc:	96ba                	add	a3,a3,a4
    return KADDR(page2pa(page));
ffffffffc02025fe:	8031                	srli	s0,s0,0xc
ffffffffc0202600:	0086f733          	and	a4,a3,s0
    return page2ppn(page) << PGSHIFT;
ffffffffc0202604:	06b2                	slli	a3,a3,0xc
    return KADDR(page2pa(page));
ffffffffc0202606:	30f77d63          	bgeu	a4,a5,ffffffffc0202920 <pmm_init+0x7ee>

    *(char *)(page2kva(p) + 0x100) = '\0';
ffffffffc020260a:	0009b783          	ld	a5,0(s3)
    assert(strlen((const char *)0x100) == 0);
ffffffffc020260e:	10000513          	li	a0,256
    *(char *)(page2kva(p) + 0x100) = '\0';
ffffffffc0202612:	96be                	add	a3,a3,a5
ffffffffc0202614:	10068023          	sb	zero,256(a3)
    assert(strlen((const char *)0x100) == 0);
ffffffffc0202618:	7a6010ef          	jal	ra,ffffffffc0203dbe <strlen>
ffffffffc020261c:	66051363          	bnez	a0,ffffffffc0202c82 <pmm_init+0xb50>

    pde_t *pd1 = boot_pgdir_va, *pd0 = page2kva(pde2page(boot_pgdir_va[0]));
ffffffffc0202620:	00093a83          	ld	s5,0(s2)
    if (PPN(pa) >= npage)
ffffffffc0202624:	609c                	ld	a5,0(s1)
    return pa2page(PDE_ADDR(pde));
ffffffffc0202626:	000ab683          	ld	a3,0(s5) # fffffffffffff000 <end+0x3fdf1b1c>
ffffffffc020262a:	068a                	slli	a3,a3,0x2
ffffffffc020262c:	82b1                	srli	a3,a3,0xc
    if (PPN(pa) >= npage)
ffffffffc020262e:	26f6f563          	bgeu	a3,a5,ffffffffc0202898 <pmm_init+0x766>
    return KADDR(page2pa(page));
ffffffffc0202632:	8c75                	and	s0,s0,a3
    return page2ppn(page) << PGSHIFT;
ffffffffc0202634:	06b2                	slli	a3,a3,0xc
    return KADDR(page2pa(page));
ffffffffc0202636:	2ef47563          	bgeu	s0,a5,ffffffffc0202920 <pmm_init+0x7ee>
ffffffffc020263a:	0009b403          	ld	s0,0(s3)
ffffffffc020263e:	9436                	add	s0,s0,a3
ffffffffc0202640:	100027f3          	csrr	a5,sstatus
ffffffffc0202644:	8b89                	andi	a5,a5,2
ffffffffc0202646:	1e079163          	bnez	a5,ffffffffc0202828 <pmm_init+0x6f6>
        pmm_manager->free_pages(base, n);
ffffffffc020264a:	000b3783          	ld	a5,0(s6)
ffffffffc020264e:	4585                	li	a1,1
ffffffffc0202650:	8552                	mv	a0,s4
ffffffffc0202652:	739c                	ld	a5,32(a5)
ffffffffc0202654:	9782                	jalr	a5
    return pa2page(PDE_ADDR(pde));
ffffffffc0202656:	601c                	ld	a5,0(s0)
    if (PPN(pa) >= npage)
ffffffffc0202658:	6098                	ld	a4,0(s1)
    return pa2page(PDE_ADDR(pde));
ffffffffc020265a:	078a                	slli	a5,a5,0x2
ffffffffc020265c:	83b1                	srli	a5,a5,0xc
    if (PPN(pa) >= npage)
ffffffffc020265e:	22e7fd63          	bgeu	a5,a4,ffffffffc0202898 <pmm_init+0x766>
    return &pages[PPN(pa) - nbase];
ffffffffc0202662:	000bb503          	ld	a0,0(s7)
ffffffffc0202666:	fff80737          	lui	a4,0xfff80
ffffffffc020266a:	97ba                	add	a5,a5,a4
ffffffffc020266c:	079a                	slli	a5,a5,0x6
ffffffffc020266e:	953e                	add	a0,a0,a5
ffffffffc0202670:	100027f3          	csrr	a5,sstatus
ffffffffc0202674:	8b89                	andi	a5,a5,2
ffffffffc0202676:	18079d63          	bnez	a5,ffffffffc0202810 <pmm_init+0x6de>
ffffffffc020267a:	000b3783          	ld	a5,0(s6)
ffffffffc020267e:	4585                	li	a1,1
ffffffffc0202680:	739c                	ld	a5,32(a5)
ffffffffc0202682:	9782                	jalr	a5
    return pa2page(PDE_ADDR(pde));
ffffffffc0202684:	000ab783          	ld	a5,0(s5)
    if (PPN(pa) >= npage)
ffffffffc0202688:	6098                	ld	a4,0(s1)
    return pa2page(PDE_ADDR(pde));
ffffffffc020268a:	078a                	slli	a5,a5,0x2
ffffffffc020268c:	83b1                	srli	a5,a5,0xc
    if (PPN(pa) >= npage)
ffffffffc020268e:	20e7f563          	bgeu	a5,a4,ffffffffc0202898 <pmm_init+0x766>
    return &pages[PPN(pa) - nbase];
ffffffffc0202692:	000bb503          	ld	a0,0(s7)
ffffffffc0202696:	fff80737          	lui	a4,0xfff80
ffffffffc020269a:	97ba                	add	a5,a5,a4
ffffffffc020269c:	079a                	slli	a5,a5,0x6
ffffffffc020269e:	953e                	add	a0,a0,a5
ffffffffc02026a0:	100027f3          	csrr	a5,sstatus
ffffffffc02026a4:	8b89                	andi	a5,a5,2
ffffffffc02026a6:	14079963          	bnez	a5,ffffffffc02027f8 <pmm_init+0x6c6>
ffffffffc02026aa:	000b3783          	ld	a5,0(s6)
ffffffffc02026ae:	4585                	li	a1,1
ffffffffc02026b0:	739c                	ld	a5,32(a5)
ffffffffc02026b2:	9782                	jalr	a5
    free_page(p);
    free_page(pde2page(pd0[0]));
    free_page(pde2page(pd1[0]));
    boot_pgdir_va[0] = 0;
ffffffffc02026b4:	00093783          	ld	a5,0(s2)
ffffffffc02026b8:	0007b023          	sd	zero,0(a5)
    asm volatile("sfence.vma");
ffffffffc02026bc:	12000073          	sfence.vma
ffffffffc02026c0:	100027f3          	csrr	a5,sstatus
ffffffffc02026c4:	8b89                	andi	a5,a5,2
ffffffffc02026c6:	10079f63          	bnez	a5,ffffffffc02027e4 <pmm_init+0x6b2>
        ret = pmm_manager->nr_free_pages();
ffffffffc02026ca:	000b3783          	ld	a5,0(s6)
ffffffffc02026ce:	779c                	ld	a5,40(a5)
ffffffffc02026d0:	9782                	jalr	a5
ffffffffc02026d2:	842a                	mv	s0,a0
    flush_tlb();

    assert(nr_free_store == nr_free_pages());
ffffffffc02026d4:	4c8c1e63          	bne	s8,s0,ffffffffc0202bb0 <pmm_init+0xa7e>

    cprintf("check_boot_pgdir() succeeded!\n");
ffffffffc02026d8:	00003517          	auipc	a0,0x3
ffffffffc02026dc:	ce050513          	addi	a0,a0,-800 # ffffffffc02053b8 <default_pmm_manager+0x6d8>
ffffffffc02026e0:	ab5fd0ef          	jal	ra,ffffffffc0200194 <cprintf>
}
ffffffffc02026e4:	7406                	ld	s0,96(sp)
ffffffffc02026e6:	70a6                	ld	ra,104(sp)
ffffffffc02026e8:	64e6                	ld	s1,88(sp)
ffffffffc02026ea:	6946                	ld	s2,80(sp)
ffffffffc02026ec:	69a6                	ld	s3,72(sp)
ffffffffc02026ee:	6a06                	ld	s4,64(sp)
ffffffffc02026f0:	7ae2                	ld	s5,56(sp)
ffffffffc02026f2:	7b42                	ld	s6,48(sp)
ffffffffc02026f4:	7ba2                	ld	s7,40(sp)
ffffffffc02026f6:	7c02                	ld	s8,32(sp)
ffffffffc02026f8:	6ce2                	ld	s9,24(sp)
ffffffffc02026fa:	6165                	addi	sp,sp,112
    kmalloc_init();
ffffffffc02026fc:	b72ff06f          	j	ffffffffc0201a6e <kmalloc_init>
    npage = maxpa / PGSIZE;
ffffffffc0202700:	c80007b7          	lui	a5,0xc8000
ffffffffc0202704:	bc7d                	j	ffffffffc02021c2 <pmm_init+0x90>
        intr_disable();
ffffffffc0202706:	a2afe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        page = pmm_manager->alloc_pages(n);
ffffffffc020270a:	000b3783          	ld	a5,0(s6)
ffffffffc020270e:	4505                	li	a0,1
ffffffffc0202710:	6f9c                	ld	a5,24(a5)
ffffffffc0202712:	9782                	jalr	a5
ffffffffc0202714:	8c2a                	mv	s8,a0
        intr_enable();
ffffffffc0202716:	a14fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc020271a:	b9a9                	j	ffffffffc0202374 <pmm_init+0x242>
        intr_disable();
ffffffffc020271c:	a14fe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
ffffffffc0202720:	000b3783          	ld	a5,0(s6)
ffffffffc0202724:	4505                	li	a0,1
ffffffffc0202726:	6f9c                	ld	a5,24(a5)
ffffffffc0202728:	9782                	jalr	a5
ffffffffc020272a:	8a2a                	mv	s4,a0
        intr_enable();
ffffffffc020272c:	9fefe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc0202730:	b645                	j	ffffffffc02022d0 <pmm_init+0x19e>
        intr_disable();
ffffffffc0202732:	9fefe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        ret = pmm_manager->nr_free_pages();
ffffffffc0202736:	000b3783          	ld	a5,0(s6)
ffffffffc020273a:	779c                	ld	a5,40(a5)
ffffffffc020273c:	9782                	jalr	a5
ffffffffc020273e:	842a                	mv	s0,a0
        intr_enable();
ffffffffc0202740:	9eafe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc0202744:	b6b9                	j	ffffffffc0202292 <pmm_init+0x160>
    mem_begin = ROUNDUP(freemem, PGSIZE);
ffffffffc0202746:	6705                	lui	a4,0x1
ffffffffc0202748:	177d                	addi	a4,a4,-1
ffffffffc020274a:	96ba                	add	a3,a3,a4
ffffffffc020274c:	8ff5                	and	a5,a5,a3
    if (PPN(pa) >= npage)
ffffffffc020274e:	00c7d713          	srli	a4,a5,0xc
ffffffffc0202752:	14a77363          	bgeu	a4,a0,ffffffffc0202898 <pmm_init+0x766>
    pmm_manager->init_memmap(base, n);
ffffffffc0202756:	000b3683          	ld	a3,0(s6)
    return &pages[PPN(pa) - nbase];
ffffffffc020275a:	fff80537          	lui	a0,0xfff80
ffffffffc020275e:	972a                	add	a4,a4,a0
ffffffffc0202760:	6a94                	ld	a3,16(a3)
        init_memmap(pa2page(mem_begin), (mem_end - mem_begin) / PGSIZE);
ffffffffc0202762:	8c1d                	sub	s0,s0,a5
ffffffffc0202764:	00671513          	slli	a0,a4,0x6
    pmm_manager->init_memmap(base, n);
ffffffffc0202768:	00c45593          	srli	a1,s0,0xc
ffffffffc020276c:	9532                	add	a0,a0,a2
ffffffffc020276e:	9682                	jalr	a3
    cprintf("vapaofset is %llu\n", va_pa_offset);
ffffffffc0202770:	0009b583          	ld	a1,0(s3)
}
ffffffffc0202774:	b4c1                	j	ffffffffc0202234 <pmm_init+0x102>
        intr_disable();
ffffffffc0202776:	9bafe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        ret = pmm_manager->nr_free_pages();
ffffffffc020277a:	000b3783          	ld	a5,0(s6)
ffffffffc020277e:	779c                	ld	a5,40(a5)
ffffffffc0202780:	9782                	jalr	a5
ffffffffc0202782:	8c2a                	mv	s8,a0
        intr_enable();
ffffffffc0202784:	9a6fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc0202788:	bb79                	j	ffffffffc0202526 <pmm_init+0x3f4>
        intr_disable();
ffffffffc020278a:	9a6fe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
ffffffffc020278e:	000b3783          	ld	a5,0(s6)
ffffffffc0202792:	779c                	ld	a5,40(a5)
ffffffffc0202794:	9782                	jalr	a5
ffffffffc0202796:	8a2a                	mv	s4,a0
        intr_enable();
ffffffffc0202798:	992fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc020279c:	b39d                	j	ffffffffc0202502 <pmm_init+0x3d0>
ffffffffc020279e:	e42a                	sd	a0,8(sp)
        intr_disable();
ffffffffc02027a0:	990fe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        pmm_manager->free_pages(base, n);
ffffffffc02027a4:	000b3783          	ld	a5,0(s6)
ffffffffc02027a8:	6522                	ld	a0,8(sp)
ffffffffc02027aa:	4585                	li	a1,1
ffffffffc02027ac:	739c                	ld	a5,32(a5)
ffffffffc02027ae:	9782                	jalr	a5
        intr_enable();
ffffffffc02027b0:	97afe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc02027b4:	b33d                	j	ffffffffc02024e2 <pmm_init+0x3b0>
ffffffffc02027b6:	e42a                	sd	a0,8(sp)
        intr_disable();
ffffffffc02027b8:	978fe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
ffffffffc02027bc:	000b3783          	ld	a5,0(s6)
ffffffffc02027c0:	6522                	ld	a0,8(sp)
ffffffffc02027c2:	4585                	li	a1,1
ffffffffc02027c4:	739c                	ld	a5,32(a5)
ffffffffc02027c6:	9782                	jalr	a5
        intr_enable();
ffffffffc02027c8:	962fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc02027cc:	b1dd                	j	ffffffffc02024b2 <pmm_init+0x380>
        intr_disable();
ffffffffc02027ce:	962fe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        page = pmm_manager->alloc_pages(n);
ffffffffc02027d2:	000b3783          	ld	a5,0(s6)
ffffffffc02027d6:	4505                	li	a0,1
ffffffffc02027d8:	6f9c                	ld	a5,24(a5)
ffffffffc02027da:	9782                	jalr	a5
ffffffffc02027dc:	8a2a                	mv	s4,a0
        intr_enable();
ffffffffc02027de:	94cfe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc02027e2:	b36d                	j	ffffffffc020258c <pmm_init+0x45a>
        intr_disable();
ffffffffc02027e4:	94cfe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        ret = pmm_manager->nr_free_pages();
ffffffffc02027e8:	000b3783          	ld	a5,0(s6)
ffffffffc02027ec:	779c                	ld	a5,40(a5)
ffffffffc02027ee:	9782                	jalr	a5
ffffffffc02027f0:	842a                	mv	s0,a0
        intr_enable();
ffffffffc02027f2:	938fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc02027f6:	bdf9                	j	ffffffffc02026d4 <pmm_init+0x5a2>
ffffffffc02027f8:	e42a                	sd	a0,8(sp)
        intr_disable();
ffffffffc02027fa:	936fe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        pmm_manager->free_pages(base, n);
ffffffffc02027fe:	000b3783          	ld	a5,0(s6)
ffffffffc0202802:	6522                	ld	a0,8(sp)
ffffffffc0202804:	4585                	li	a1,1
ffffffffc0202806:	739c                	ld	a5,32(a5)
ffffffffc0202808:	9782                	jalr	a5
        intr_enable();
ffffffffc020280a:	920fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc020280e:	b55d                	j	ffffffffc02026b4 <pmm_init+0x582>
ffffffffc0202810:	e42a                	sd	a0,8(sp)
        intr_disable();
ffffffffc0202812:	91efe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
ffffffffc0202816:	000b3783          	ld	a5,0(s6)
ffffffffc020281a:	6522                	ld	a0,8(sp)
ffffffffc020281c:	4585                	li	a1,1
ffffffffc020281e:	739c                	ld	a5,32(a5)
ffffffffc0202820:	9782                	jalr	a5
        intr_enable();
ffffffffc0202822:	908fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc0202826:	bdb9                	j	ffffffffc0202684 <pmm_init+0x552>
        intr_disable();
ffffffffc0202828:	908fe0ef          	jal	ra,ffffffffc0200930 <intr_disable>
ffffffffc020282c:	000b3783          	ld	a5,0(s6)
ffffffffc0202830:	4585                	li	a1,1
ffffffffc0202832:	8552                	mv	a0,s4
ffffffffc0202834:	739c                	ld	a5,32(a5)
ffffffffc0202836:	9782                	jalr	a5
        intr_enable();
ffffffffc0202838:	8f2fe0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc020283c:	bd29                	j	ffffffffc0202656 <pmm_init+0x524>
        assert((ptep = get_pte(boot_pgdir_va, (uintptr_t)KADDR(i), 0)) != NULL);
ffffffffc020283e:	86a2                	mv	a3,s0
ffffffffc0202840:	00002617          	auipc	a2,0x2
ffffffffc0202844:	4d860613          	addi	a2,a2,1240 # ffffffffc0204d18 <default_pmm_manager+0x38>
ffffffffc0202848:	1a400593          	li	a1,420
ffffffffc020284c:	00002517          	auipc	a0,0x2
ffffffffc0202850:	5e450513          	addi	a0,a0,1508 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202854:	c07fd0ef          	jal	ra,ffffffffc020045a <__panic>
        assert(PTE_ADDR(*ptep) == i);
ffffffffc0202858:	00003697          	auipc	a3,0x3
ffffffffc020285c:	a0068693          	addi	a3,a3,-1536 # ffffffffc0205258 <default_pmm_manager+0x578>
ffffffffc0202860:	00002617          	auipc	a2,0x2
ffffffffc0202864:	0d060613          	addi	a2,a2,208 # ffffffffc0204930 <commands+0x818>
ffffffffc0202868:	1a500593          	li	a1,421
ffffffffc020286c:	00002517          	auipc	a0,0x2
ffffffffc0202870:	5c450513          	addi	a0,a0,1476 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202874:	be7fd0ef          	jal	ra,ffffffffc020045a <__panic>
        assert((ptep = get_pte(boot_pgdir_va, (uintptr_t)KADDR(i), 0)) != NULL);
ffffffffc0202878:	00003697          	auipc	a3,0x3
ffffffffc020287c:	9a068693          	addi	a3,a3,-1632 # ffffffffc0205218 <default_pmm_manager+0x538>
ffffffffc0202880:	00002617          	auipc	a2,0x2
ffffffffc0202884:	0b060613          	addi	a2,a2,176 # ffffffffc0204930 <commands+0x818>
ffffffffc0202888:	1a400593          	li	a1,420
ffffffffc020288c:	00002517          	auipc	a0,0x2
ffffffffc0202890:	5a450513          	addi	a0,a0,1444 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202894:	bc7fd0ef          	jal	ra,ffffffffc020045a <__panic>
ffffffffc0202898:	b9cff0ef          	jal	ra,ffffffffc0201c34 <pa2page.part.0>
ffffffffc020289c:	bb4ff0ef          	jal	ra,ffffffffc0201c50 <pte2page.part.0>
    assert(get_pte(boot_pgdir_va, PGSIZE, 0) == ptep);
ffffffffc02028a0:	00002697          	auipc	a3,0x2
ffffffffc02028a4:	77068693          	addi	a3,a3,1904 # ffffffffc0205010 <default_pmm_manager+0x330>
ffffffffc02028a8:	00002617          	auipc	a2,0x2
ffffffffc02028ac:	08860613          	addi	a2,a2,136 # ffffffffc0204930 <commands+0x818>
ffffffffc02028b0:	17400593          	li	a1,372
ffffffffc02028b4:	00002517          	auipc	a0,0x2
ffffffffc02028b8:	57c50513          	addi	a0,a0,1404 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc02028bc:	b9ffd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(get_page(boot_pgdir_va, 0x0, NULL) == NULL);
ffffffffc02028c0:	00002697          	auipc	a3,0x2
ffffffffc02028c4:	69068693          	addi	a3,a3,1680 # ffffffffc0204f50 <default_pmm_manager+0x270>
ffffffffc02028c8:	00002617          	auipc	a2,0x2
ffffffffc02028cc:	06860613          	addi	a2,a2,104 # ffffffffc0204930 <commands+0x818>
ffffffffc02028d0:	16700593          	li	a1,359
ffffffffc02028d4:	00002517          	auipc	a0,0x2
ffffffffc02028d8:	55c50513          	addi	a0,a0,1372 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc02028dc:	b7ffd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(boot_pgdir_va != NULL && (uint32_t)PGOFF(boot_pgdir_va) == 0);
ffffffffc02028e0:	00002697          	auipc	a3,0x2
ffffffffc02028e4:	63068693          	addi	a3,a3,1584 # ffffffffc0204f10 <default_pmm_manager+0x230>
ffffffffc02028e8:	00002617          	auipc	a2,0x2
ffffffffc02028ec:	04860613          	addi	a2,a2,72 # ffffffffc0204930 <commands+0x818>
ffffffffc02028f0:	16600593          	li	a1,358
ffffffffc02028f4:	00002517          	auipc	a0,0x2
ffffffffc02028f8:	53c50513          	addi	a0,a0,1340 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc02028fc:	b5ffd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(npage <= KERNTOP / PGSIZE);
ffffffffc0202900:	00002697          	auipc	a3,0x2
ffffffffc0202904:	5f068693          	addi	a3,a3,1520 # ffffffffc0204ef0 <default_pmm_manager+0x210>
ffffffffc0202908:	00002617          	auipc	a2,0x2
ffffffffc020290c:	02860613          	addi	a2,a2,40 # ffffffffc0204930 <commands+0x818>
ffffffffc0202910:	16500593          	li	a1,357
ffffffffc0202914:	00002517          	auipc	a0,0x2
ffffffffc0202918:	51c50513          	addi	a0,a0,1308 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc020291c:	b3ffd0ef          	jal	ra,ffffffffc020045a <__panic>
    return KADDR(page2pa(page));
ffffffffc0202920:	00002617          	auipc	a2,0x2
ffffffffc0202924:	3f860613          	addi	a2,a2,1016 # ffffffffc0204d18 <default_pmm_manager+0x38>
ffffffffc0202928:	07100593          	li	a1,113
ffffffffc020292c:	00002517          	auipc	a0,0x2
ffffffffc0202930:	41450513          	addi	a0,a0,1044 # ffffffffc0204d40 <default_pmm_manager+0x60>
ffffffffc0202934:	b27fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_ref(pde2page(boot_pgdir_va[0])) == 1);
ffffffffc0202938:	00003697          	auipc	a3,0x3
ffffffffc020293c:	86868693          	addi	a3,a3,-1944 # ffffffffc02051a0 <default_pmm_manager+0x4c0>
ffffffffc0202940:	00002617          	auipc	a2,0x2
ffffffffc0202944:	ff060613          	addi	a2,a2,-16 # ffffffffc0204930 <commands+0x818>
ffffffffc0202948:	18d00593          	li	a1,397
ffffffffc020294c:	00002517          	auipc	a0,0x2
ffffffffc0202950:	4e450513          	addi	a0,a0,1252 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202954:	b07fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_ref(p2) == 0);
ffffffffc0202958:	00003697          	auipc	a3,0x3
ffffffffc020295c:	80068693          	addi	a3,a3,-2048 # ffffffffc0205158 <default_pmm_manager+0x478>
ffffffffc0202960:	00002617          	auipc	a2,0x2
ffffffffc0202964:	fd060613          	addi	a2,a2,-48 # ffffffffc0204930 <commands+0x818>
ffffffffc0202968:	18b00593          	li	a1,395
ffffffffc020296c:	00002517          	auipc	a0,0x2
ffffffffc0202970:	4c450513          	addi	a0,a0,1220 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202974:	ae7fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_ref(p1) == 0);
ffffffffc0202978:	00003697          	auipc	a3,0x3
ffffffffc020297c:	81068693          	addi	a3,a3,-2032 # ffffffffc0205188 <default_pmm_manager+0x4a8>
ffffffffc0202980:	00002617          	auipc	a2,0x2
ffffffffc0202984:	fb060613          	addi	a2,a2,-80 # ffffffffc0204930 <commands+0x818>
ffffffffc0202988:	18a00593          	li	a1,394
ffffffffc020298c:	00002517          	auipc	a0,0x2
ffffffffc0202990:	4a450513          	addi	a0,a0,1188 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202994:	ac7fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(boot_pgdir_va[0] == 0);
ffffffffc0202998:	00003697          	auipc	a3,0x3
ffffffffc020299c:	8d868693          	addi	a3,a3,-1832 # ffffffffc0205270 <default_pmm_manager+0x590>
ffffffffc02029a0:	00002617          	auipc	a2,0x2
ffffffffc02029a4:	f9060613          	addi	a2,a2,-112 # ffffffffc0204930 <commands+0x818>
ffffffffc02029a8:	1a800593          	li	a1,424
ffffffffc02029ac:	00002517          	auipc	a0,0x2
ffffffffc02029b0:	48450513          	addi	a0,a0,1156 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc02029b4:	aa7fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(nr_free_store == nr_free_pages());
ffffffffc02029b8:	00003697          	auipc	a3,0x3
ffffffffc02029bc:	81868693          	addi	a3,a3,-2024 # ffffffffc02051d0 <default_pmm_manager+0x4f0>
ffffffffc02029c0:	00002617          	auipc	a2,0x2
ffffffffc02029c4:	f7060613          	addi	a2,a2,-144 # ffffffffc0204930 <commands+0x818>
ffffffffc02029c8:	19500593          	li	a1,405
ffffffffc02029cc:	00002517          	auipc	a0,0x2
ffffffffc02029d0:	46450513          	addi	a0,a0,1124 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc02029d4:	a87fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_ref(p) == 1);
ffffffffc02029d8:	00003697          	auipc	a3,0x3
ffffffffc02029dc:	8f068693          	addi	a3,a3,-1808 # ffffffffc02052c8 <default_pmm_manager+0x5e8>
ffffffffc02029e0:	00002617          	auipc	a2,0x2
ffffffffc02029e4:	f5060613          	addi	a2,a2,-176 # ffffffffc0204930 <commands+0x818>
ffffffffc02029e8:	1ad00593          	li	a1,429
ffffffffc02029ec:	00002517          	auipc	a0,0x2
ffffffffc02029f0:	44450513          	addi	a0,a0,1092 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc02029f4:	a67fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_insert(boot_pgdir_va, p, 0x100, PTE_W | PTE_R) == 0);
ffffffffc02029f8:	00003697          	auipc	a3,0x3
ffffffffc02029fc:	89068693          	addi	a3,a3,-1904 # ffffffffc0205288 <default_pmm_manager+0x5a8>
ffffffffc0202a00:	00002617          	auipc	a2,0x2
ffffffffc0202a04:	f3060613          	addi	a2,a2,-208 # ffffffffc0204930 <commands+0x818>
ffffffffc0202a08:	1ac00593          	li	a1,428
ffffffffc0202a0c:	00002517          	auipc	a0,0x2
ffffffffc0202a10:	42450513          	addi	a0,a0,1060 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202a14:	a47fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_ref(p2) == 0);
ffffffffc0202a18:	00002697          	auipc	a3,0x2
ffffffffc0202a1c:	74068693          	addi	a3,a3,1856 # ffffffffc0205158 <default_pmm_manager+0x478>
ffffffffc0202a20:	00002617          	auipc	a2,0x2
ffffffffc0202a24:	f1060613          	addi	a2,a2,-240 # ffffffffc0204930 <commands+0x818>
ffffffffc0202a28:	18700593          	li	a1,391
ffffffffc0202a2c:	00002517          	auipc	a0,0x2
ffffffffc0202a30:	40450513          	addi	a0,a0,1028 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202a34:	a27fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_ref(p1) == 1);
ffffffffc0202a38:	00002697          	auipc	a3,0x2
ffffffffc0202a3c:	5c068693          	addi	a3,a3,1472 # ffffffffc0204ff8 <default_pmm_manager+0x318>
ffffffffc0202a40:	00002617          	auipc	a2,0x2
ffffffffc0202a44:	ef060613          	addi	a2,a2,-272 # ffffffffc0204930 <commands+0x818>
ffffffffc0202a48:	18600593          	li	a1,390
ffffffffc0202a4c:	00002517          	auipc	a0,0x2
ffffffffc0202a50:	3e450513          	addi	a0,a0,996 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202a54:	a07fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((*ptep & PTE_U) == 0);
ffffffffc0202a58:	00002697          	auipc	a3,0x2
ffffffffc0202a5c:	71868693          	addi	a3,a3,1816 # ffffffffc0205170 <default_pmm_manager+0x490>
ffffffffc0202a60:	00002617          	auipc	a2,0x2
ffffffffc0202a64:	ed060613          	addi	a2,a2,-304 # ffffffffc0204930 <commands+0x818>
ffffffffc0202a68:	18300593          	li	a1,387
ffffffffc0202a6c:	00002517          	auipc	a0,0x2
ffffffffc0202a70:	3c450513          	addi	a0,a0,964 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202a74:	9e7fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(pte2page(*ptep) == p1);
ffffffffc0202a78:	00002697          	auipc	a3,0x2
ffffffffc0202a7c:	56868693          	addi	a3,a3,1384 # ffffffffc0204fe0 <default_pmm_manager+0x300>
ffffffffc0202a80:	00002617          	auipc	a2,0x2
ffffffffc0202a84:	eb060613          	addi	a2,a2,-336 # ffffffffc0204930 <commands+0x818>
ffffffffc0202a88:	18200593          	li	a1,386
ffffffffc0202a8c:	00002517          	auipc	a0,0x2
ffffffffc0202a90:	3a450513          	addi	a0,a0,932 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202a94:	9c7fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
ffffffffc0202a98:	00002697          	auipc	a3,0x2
ffffffffc0202a9c:	5e868693          	addi	a3,a3,1512 # ffffffffc0205080 <default_pmm_manager+0x3a0>
ffffffffc0202aa0:	00002617          	auipc	a2,0x2
ffffffffc0202aa4:	e9060613          	addi	a2,a2,-368 # ffffffffc0204930 <commands+0x818>
ffffffffc0202aa8:	18100593          	li	a1,385
ffffffffc0202aac:	00002517          	auipc	a0,0x2
ffffffffc0202ab0:	38450513          	addi	a0,a0,900 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202ab4:	9a7fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_ref(p2) == 0);
ffffffffc0202ab8:	00002697          	auipc	a3,0x2
ffffffffc0202abc:	6a068693          	addi	a3,a3,1696 # ffffffffc0205158 <default_pmm_manager+0x478>
ffffffffc0202ac0:	00002617          	auipc	a2,0x2
ffffffffc0202ac4:	e7060613          	addi	a2,a2,-400 # ffffffffc0204930 <commands+0x818>
ffffffffc0202ac8:	18000593          	li	a1,384
ffffffffc0202acc:	00002517          	auipc	a0,0x2
ffffffffc0202ad0:	36450513          	addi	a0,a0,868 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202ad4:	987fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_ref(p1) == 2);
ffffffffc0202ad8:	00002697          	auipc	a3,0x2
ffffffffc0202adc:	66868693          	addi	a3,a3,1640 # ffffffffc0205140 <default_pmm_manager+0x460>
ffffffffc0202ae0:	00002617          	auipc	a2,0x2
ffffffffc0202ae4:	e5060613          	addi	a2,a2,-432 # ffffffffc0204930 <commands+0x818>
ffffffffc0202ae8:	17f00593          	li	a1,383
ffffffffc0202aec:	00002517          	auipc	a0,0x2
ffffffffc0202af0:	34450513          	addi	a0,a0,836 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202af4:	967fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_insert(boot_pgdir_va, p1, PGSIZE, 0) == 0);
ffffffffc0202af8:	00002697          	auipc	a3,0x2
ffffffffc0202afc:	61868693          	addi	a3,a3,1560 # ffffffffc0205110 <default_pmm_manager+0x430>
ffffffffc0202b00:	00002617          	auipc	a2,0x2
ffffffffc0202b04:	e3060613          	addi	a2,a2,-464 # ffffffffc0204930 <commands+0x818>
ffffffffc0202b08:	17e00593          	li	a1,382
ffffffffc0202b0c:	00002517          	auipc	a0,0x2
ffffffffc0202b10:	32450513          	addi	a0,a0,804 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202b14:	947fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_ref(p2) == 1);
ffffffffc0202b18:	00002697          	auipc	a3,0x2
ffffffffc0202b1c:	5e068693          	addi	a3,a3,1504 # ffffffffc02050f8 <default_pmm_manager+0x418>
ffffffffc0202b20:	00002617          	auipc	a2,0x2
ffffffffc0202b24:	e1060613          	addi	a2,a2,-496 # ffffffffc0204930 <commands+0x818>
ffffffffc0202b28:	17c00593          	li	a1,380
ffffffffc0202b2c:	00002517          	auipc	a0,0x2
ffffffffc0202b30:	30450513          	addi	a0,a0,772 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202b34:	927fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(boot_pgdir_va[0] & PTE_U);
ffffffffc0202b38:	00002697          	auipc	a3,0x2
ffffffffc0202b3c:	5a068693          	addi	a3,a3,1440 # ffffffffc02050d8 <default_pmm_manager+0x3f8>
ffffffffc0202b40:	00002617          	auipc	a2,0x2
ffffffffc0202b44:	df060613          	addi	a2,a2,-528 # ffffffffc0204930 <commands+0x818>
ffffffffc0202b48:	17b00593          	li	a1,379
ffffffffc0202b4c:	00002517          	auipc	a0,0x2
ffffffffc0202b50:	2e450513          	addi	a0,a0,740 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202b54:	907fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(*ptep & PTE_W);
ffffffffc0202b58:	00002697          	auipc	a3,0x2
ffffffffc0202b5c:	57068693          	addi	a3,a3,1392 # ffffffffc02050c8 <default_pmm_manager+0x3e8>
ffffffffc0202b60:	00002617          	auipc	a2,0x2
ffffffffc0202b64:	dd060613          	addi	a2,a2,-560 # ffffffffc0204930 <commands+0x818>
ffffffffc0202b68:	17a00593          	li	a1,378
ffffffffc0202b6c:	00002517          	auipc	a0,0x2
ffffffffc0202b70:	2c450513          	addi	a0,a0,708 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202b74:	8e7fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(*ptep & PTE_U);
ffffffffc0202b78:	00002697          	auipc	a3,0x2
ffffffffc0202b7c:	54068693          	addi	a3,a3,1344 # ffffffffc02050b8 <default_pmm_manager+0x3d8>
ffffffffc0202b80:	00002617          	auipc	a2,0x2
ffffffffc0202b84:	db060613          	addi	a2,a2,-592 # ffffffffc0204930 <commands+0x818>
ffffffffc0202b88:	17900593          	li	a1,377
ffffffffc0202b8c:	00002517          	auipc	a0,0x2
ffffffffc0202b90:	2a450513          	addi	a0,a0,676 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202b94:	8c7fd0ef          	jal	ra,ffffffffc020045a <__panic>
        panic("DTB memory info not available");
ffffffffc0202b98:	00002617          	auipc	a2,0x2
ffffffffc0202b9c:	2c060613          	addi	a2,a2,704 # ffffffffc0204e58 <default_pmm_manager+0x178>
ffffffffc0202ba0:	06400593          	li	a1,100
ffffffffc0202ba4:	00002517          	auipc	a0,0x2
ffffffffc0202ba8:	28c50513          	addi	a0,a0,652 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202bac:	8affd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(nr_free_store == nr_free_pages());
ffffffffc0202bb0:	00002697          	auipc	a3,0x2
ffffffffc0202bb4:	62068693          	addi	a3,a3,1568 # ffffffffc02051d0 <default_pmm_manager+0x4f0>
ffffffffc0202bb8:	00002617          	auipc	a2,0x2
ffffffffc0202bbc:	d7860613          	addi	a2,a2,-648 # ffffffffc0204930 <commands+0x818>
ffffffffc0202bc0:	1bf00593          	li	a1,447
ffffffffc0202bc4:	00002517          	auipc	a0,0x2
ffffffffc0202bc8:	26c50513          	addi	a0,a0,620 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202bcc:	88ffd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((ptep = get_pte(boot_pgdir_va, PGSIZE, 0)) != NULL);
ffffffffc0202bd0:	00002697          	auipc	a3,0x2
ffffffffc0202bd4:	4b068693          	addi	a3,a3,1200 # ffffffffc0205080 <default_pmm_manager+0x3a0>
ffffffffc0202bd8:	00002617          	auipc	a2,0x2
ffffffffc0202bdc:	d5860613          	addi	a2,a2,-680 # ffffffffc0204930 <commands+0x818>
ffffffffc0202be0:	17800593          	li	a1,376
ffffffffc0202be4:	00002517          	auipc	a0,0x2
ffffffffc0202be8:	24c50513          	addi	a0,a0,588 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202bec:	86ffd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_insert(boot_pgdir_va, p2, PGSIZE, PTE_U | PTE_W) == 0);
ffffffffc0202bf0:	00002697          	auipc	a3,0x2
ffffffffc0202bf4:	45068693          	addi	a3,a3,1104 # ffffffffc0205040 <default_pmm_manager+0x360>
ffffffffc0202bf8:	00002617          	auipc	a2,0x2
ffffffffc0202bfc:	d3860613          	addi	a2,a2,-712 # ffffffffc0204930 <commands+0x818>
ffffffffc0202c00:	17700593          	li	a1,375
ffffffffc0202c04:	00002517          	auipc	a0,0x2
ffffffffc0202c08:	22c50513          	addi	a0,a0,556 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202c0c:	84ffd0ef          	jal	ra,ffffffffc020045a <__panic>
    ptep = (pte_t *)KADDR(PDE_ADDR(ptep[0])) + 1;
ffffffffc0202c10:	86d6                	mv	a3,s5
ffffffffc0202c12:	00002617          	auipc	a2,0x2
ffffffffc0202c16:	10660613          	addi	a2,a2,262 # ffffffffc0204d18 <default_pmm_manager+0x38>
ffffffffc0202c1a:	17300593          	li	a1,371
ffffffffc0202c1e:	00002517          	auipc	a0,0x2
ffffffffc0202c22:	21250513          	addi	a0,a0,530 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202c26:	835fd0ef          	jal	ra,ffffffffc020045a <__panic>
    ptep = (pte_t *)KADDR(PDE_ADDR(boot_pgdir_va[0]));
ffffffffc0202c2a:	00002617          	auipc	a2,0x2
ffffffffc0202c2e:	0ee60613          	addi	a2,a2,238 # ffffffffc0204d18 <default_pmm_manager+0x38>
ffffffffc0202c32:	17200593          	li	a1,370
ffffffffc0202c36:	00002517          	auipc	a0,0x2
ffffffffc0202c3a:	1fa50513          	addi	a0,a0,506 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202c3e:	81dfd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_ref(p1) == 1);
ffffffffc0202c42:	00002697          	auipc	a3,0x2
ffffffffc0202c46:	3b668693          	addi	a3,a3,950 # ffffffffc0204ff8 <default_pmm_manager+0x318>
ffffffffc0202c4a:	00002617          	auipc	a2,0x2
ffffffffc0202c4e:	ce660613          	addi	a2,a2,-794 # ffffffffc0204930 <commands+0x818>
ffffffffc0202c52:	17000593          	li	a1,368
ffffffffc0202c56:	00002517          	auipc	a0,0x2
ffffffffc0202c5a:	1da50513          	addi	a0,a0,474 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202c5e:	ffcfd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(pte2page(*ptep) == p1);
ffffffffc0202c62:	00002697          	auipc	a3,0x2
ffffffffc0202c66:	37e68693          	addi	a3,a3,894 # ffffffffc0204fe0 <default_pmm_manager+0x300>
ffffffffc0202c6a:	00002617          	auipc	a2,0x2
ffffffffc0202c6e:	cc660613          	addi	a2,a2,-826 # ffffffffc0204930 <commands+0x818>
ffffffffc0202c72:	16f00593          	li	a1,367
ffffffffc0202c76:	00002517          	auipc	a0,0x2
ffffffffc0202c7a:	1ba50513          	addi	a0,a0,442 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202c7e:	fdcfd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(strlen((const char *)0x100) == 0);
ffffffffc0202c82:	00002697          	auipc	a3,0x2
ffffffffc0202c86:	70e68693          	addi	a3,a3,1806 # ffffffffc0205390 <default_pmm_manager+0x6b0>
ffffffffc0202c8a:	00002617          	auipc	a2,0x2
ffffffffc0202c8e:	ca660613          	addi	a2,a2,-858 # ffffffffc0204930 <commands+0x818>
ffffffffc0202c92:	1b600593          	li	a1,438
ffffffffc0202c96:	00002517          	auipc	a0,0x2
ffffffffc0202c9a:	19a50513          	addi	a0,a0,410 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202c9e:	fbcfd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(strcmp((void *)0x100, (void *)(0x100 + PGSIZE)) == 0);
ffffffffc0202ca2:	00002697          	auipc	a3,0x2
ffffffffc0202ca6:	6b668693          	addi	a3,a3,1718 # ffffffffc0205358 <default_pmm_manager+0x678>
ffffffffc0202caa:	00002617          	auipc	a2,0x2
ffffffffc0202cae:	c8660613          	addi	a2,a2,-890 # ffffffffc0204930 <commands+0x818>
ffffffffc0202cb2:	1b300593          	li	a1,435
ffffffffc0202cb6:	00002517          	auipc	a0,0x2
ffffffffc0202cba:	17a50513          	addi	a0,a0,378 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202cbe:	f9cfd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_ref(p) == 2);
ffffffffc0202cc2:	00002697          	auipc	a3,0x2
ffffffffc0202cc6:	66668693          	addi	a3,a3,1638 # ffffffffc0205328 <default_pmm_manager+0x648>
ffffffffc0202cca:	00002617          	auipc	a2,0x2
ffffffffc0202cce:	c6660613          	addi	a2,a2,-922 # ffffffffc0204930 <commands+0x818>
ffffffffc0202cd2:	1af00593          	li	a1,431
ffffffffc0202cd6:	00002517          	auipc	a0,0x2
ffffffffc0202cda:	15a50513          	addi	a0,a0,346 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202cde:	f7cfd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_insert(boot_pgdir_va, p, 0x100 + PGSIZE, PTE_W | PTE_R) == 0);
ffffffffc0202ce2:	00002697          	auipc	a3,0x2
ffffffffc0202ce6:	5fe68693          	addi	a3,a3,1534 # ffffffffc02052e0 <default_pmm_manager+0x600>
ffffffffc0202cea:	00002617          	auipc	a2,0x2
ffffffffc0202cee:	c4660613          	addi	a2,a2,-954 # ffffffffc0204930 <commands+0x818>
ffffffffc0202cf2:	1ae00593          	li	a1,430
ffffffffc0202cf6:	00002517          	auipc	a0,0x2
ffffffffc0202cfa:	13a50513          	addi	a0,a0,314 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202cfe:	f5cfd0ef          	jal	ra,ffffffffc020045a <__panic>
    boot_pgdir_pa = PADDR(boot_pgdir_va);
ffffffffc0202d02:	00002617          	auipc	a2,0x2
ffffffffc0202d06:	0be60613          	addi	a2,a2,190 # ffffffffc0204dc0 <default_pmm_manager+0xe0>
ffffffffc0202d0a:	0cb00593          	li	a1,203
ffffffffc0202d0e:	00002517          	auipc	a0,0x2
ffffffffc0202d12:	12250513          	addi	a0,a0,290 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202d16:	f44fd0ef          	jal	ra,ffffffffc020045a <__panic>
    uintptr_t freemem = PADDR((uintptr_t)pages + sizeof(struct Page) * (npage - nbase));
ffffffffc0202d1a:	00002617          	auipc	a2,0x2
ffffffffc0202d1e:	0a660613          	addi	a2,a2,166 # ffffffffc0204dc0 <default_pmm_manager+0xe0>
ffffffffc0202d22:	08000593          	li	a1,128
ffffffffc0202d26:	00002517          	auipc	a0,0x2
ffffffffc0202d2a:	10a50513          	addi	a0,a0,266 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202d2e:	f2cfd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert((ptep = get_pte(boot_pgdir_va, 0x0, 0)) != NULL);
ffffffffc0202d32:	00002697          	auipc	a3,0x2
ffffffffc0202d36:	27e68693          	addi	a3,a3,638 # ffffffffc0204fb0 <default_pmm_manager+0x2d0>
ffffffffc0202d3a:	00002617          	auipc	a2,0x2
ffffffffc0202d3e:	bf660613          	addi	a2,a2,-1034 # ffffffffc0204930 <commands+0x818>
ffffffffc0202d42:	16e00593          	li	a1,366
ffffffffc0202d46:	00002517          	auipc	a0,0x2
ffffffffc0202d4a:	0ea50513          	addi	a0,a0,234 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202d4e:	f0cfd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(page_insert(boot_pgdir_va, p1, 0x0, 0) == 0);
ffffffffc0202d52:	00002697          	auipc	a3,0x2
ffffffffc0202d56:	22e68693          	addi	a3,a3,558 # ffffffffc0204f80 <default_pmm_manager+0x2a0>
ffffffffc0202d5a:	00002617          	auipc	a2,0x2
ffffffffc0202d5e:	bd660613          	addi	a2,a2,-1066 # ffffffffc0204930 <commands+0x818>
ffffffffc0202d62:	16b00593          	li	a1,363
ffffffffc0202d66:	00002517          	auipc	a0,0x2
ffffffffc0202d6a:	0ca50513          	addi	a0,a0,202 # ffffffffc0204e30 <default_pmm_manager+0x150>
ffffffffc0202d6e:	eecfd0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0202d72 <check_vma_overlap.part.0>:
    return vma;
}

// check_vma_overlap - check if vma1 overlaps vma2 ?
static inline void
check_vma_overlap(struct vma_struct *prev, struct vma_struct *next)
ffffffffc0202d72:	1141                	addi	sp,sp,-16
{
    assert(prev->vm_start < prev->vm_end);
    assert(prev->vm_end <= next->vm_start);
    assert(next->vm_start < next->vm_end);
ffffffffc0202d74:	00002697          	auipc	a3,0x2
ffffffffc0202d78:	66468693          	addi	a3,a3,1636 # ffffffffc02053d8 <default_pmm_manager+0x6f8>
ffffffffc0202d7c:	00002617          	auipc	a2,0x2
ffffffffc0202d80:	bb460613          	addi	a2,a2,-1100 # ffffffffc0204930 <commands+0x818>
ffffffffc0202d84:	08800593          	li	a1,136
ffffffffc0202d88:	00002517          	auipc	a0,0x2
ffffffffc0202d8c:	67050513          	addi	a0,a0,1648 # ffffffffc02053f8 <default_pmm_manager+0x718>
check_vma_overlap(struct vma_struct *prev, struct vma_struct *next)
ffffffffc0202d90:	e406                	sd	ra,8(sp)
    assert(next->vm_start < next->vm_end);
ffffffffc0202d92:	ec8fd0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0202d96 <find_vma>:
{
ffffffffc0202d96:	86aa                	mv	a3,a0
    if (mm != NULL)
ffffffffc0202d98:	c505                	beqz	a0,ffffffffc0202dc0 <find_vma+0x2a>
        vma = mm->mmap_cache;
ffffffffc0202d9a:	6908                	ld	a0,16(a0)
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr))
ffffffffc0202d9c:	c501                	beqz	a0,ffffffffc0202da4 <find_vma+0xe>
ffffffffc0202d9e:	651c                	ld	a5,8(a0)
ffffffffc0202da0:	02f5f263          	bgeu	a1,a5,ffffffffc0202dc4 <find_vma+0x2e>
    return listelm->next;
ffffffffc0202da4:	669c                	ld	a5,8(a3)
            while ((le = list_next(le)) != list)
ffffffffc0202da6:	00f68d63          	beq	a3,a5,ffffffffc0202dc0 <find_vma+0x2a>
                if (vma->vm_start <= addr && addr < vma->vm_end)
ffffffffc0202daa:	fe87b703          	ld	a4,-24(a5) # ffffffffc7ffffe8 <end+0x7df2b04>
ffffffffc0202dae:	00e5e663          	bltu	a1,a4,ffffffffc0202dba <find_vma+0x24>
ffffffffc0202db2:	ff07b703          	ld	a4,-16(a5)
ffffffffc0202db6:	00e5ec63          	bltu	a1,a4,ffffffffc0202dce <find_vma+0x38>
ffffffffc0202dba:	679c                	ld	a5,8(a5)
            while ((le = list_next(le)) != list)
ffffffffc0202dbc:	fef697e3          	bne	a3,a5,ffffffffc0202daa <find_vma+0x14>
    struct vma_struct *vma = NULL;
ffffffffc0202dc0:	4501                	li	a0,0
}
ffffffffc0202dc2:	8082                	ret
        if (!(vma != NULL && vma->vm_start <= addr && vma->vm_end > addr))
ffffffffc0202dc4:	691c                	ld	a5,16(a0)
ffffffffc0202dc6:	fcf5ffe3          	bgeu	a1,a5,ffffffffc0202da4 <find_vma+0xe>
            mm->mmap_cache = vma;
ffffffffc0202dca:	ea88                	sd	a0,16(a3)
ffffffffc0202dcc:	8082                	ret
                vma = le2vma(le, list_link);
ffffffffc0202dce:	fe078513          	addi	a0,a5,-32
            mm->mmap_cache = vma;
ffffffffc0202dd2:	ea88                	sd	a0,16(a3)
ffffffffc0202dd4:	8082                	ret

ffffffffc0202dd6 <insert_vma_struct>:
}

// insert_vma_struct -insert vma in mm's list link
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma)
{
    assert(vma->vm_start < vma->vm_end);
ffffffffc0202dd6:	6590                	ld	a2,8(a1)
ffffffffc0202dd8:	0105b803          	ld	a6,16(a1)
{
ffffffffc0202ddc:	1141                	addi	sp,sp,-16
ffffffffc0202dde:	e406                	sd	ra,8(sp)
ffffffffc0202de0:	87aa                	mv	a5,a0
    assert(vma->vm_start < vma->vm_end);
ffffffffc0202de2:	01066763          	bltu	a2,a6,ffffffffc0202df0 <insert_vma_struct+0x1a>
ffffffffc0202de6:	a085                	j	ffffffffc0202e46 <insert_vma_struct+0x70>

    list_entry_t *le = list;
    while ((le = list_next(le)) != list)
    {
        struct vma_struct *mmap_prev = le2vma(le, list_link);
        if (mmap_prev->vm_start > vma->vm_start)
ffffffffc0202de8:	fe87b703          	ld	a4,-24(a5)
ffffffffc0202dec:	04e66863          	bltu	a2,a4,ffffffffc0202e3c <insert_vma_struct+0x66>
ffffffffc0202df0:	86be                	mv	a3,a5
ffffffffc0202df2:	679c                	ld	a5,8(a5)
    while ((le = list_next(le)) != list)
ffffffffc0202df4:	fef51ae3          	bne	a0,a5,ffffffffc0202de8 <insert_vma_struct+0x12>
    }

    le_next = list_next(le_prev);

    /* check overlap */
    if (le_prev != list)
ffffffffc0202df8:	02a68463          	beq	a3,a0,ffffffffc0202e20 <insert_vma_struct+0x4a>
    {
        check_vma_overlap(le2vma(le_prev, list_link), vma);
ffffffffc0202dfc:	ff06b703          	ld	a4,-16(a3)
    assert(prev->vm_start < prev->vm_end);
ffffffffc0202e00:	fe86b883          	ld	a7,-24(a3)
ffffffffc0202e04:	08e8f163          	bgeu	a7,a4,ffffffffc0202e86 <insert_vma_struct+0xb0>
    assert(prev->vm_end <= next->vm_start);
ffffffffc0202e08:	04e66f63          	bltu	a2,a4,ffffffffc0202e66 <insert_vma_struct+0x90>
    }
    if (le_next != list)
ffffffffc0202e0c:	00f50a63          	beq	a0,a5,ffffffffc0202e20 <insert_vma_struct+0x4a>
        if (mmap_prev->vm_start > vma->vm_start)
ffffffffc0202e10:	fe87b703          	ld	a4,-24(a5)
    assert(prev->vm_end <= next->vm_start);
ffffffffc0202e14:	05076963          	bltu	a4,a6,ffffffffc0202e66 <insert_vma_struct+0x90>
    assert(next->vm_start < next->vm_end);
ffffffffc0202e18:	ff07b603          	ld	a2,-16(a5)
ffffffffc0202e1c:	02c77363          	bgeu	a4,a2,ffffffffc0202e42 <insert_vma_struct+0x6c>
    }

    vma->vm_mm = mm;
    list_add_after(le_prev, &(vma->list_link));

    mm->map_count++;
ffffffffc0202e20:	5118                	lw	a4,32(a0)
    vma->vm_mm = mm;
ffffffffc0202e22:	e188                	sd	a0,0(a1)
    list_add_after(le_prev, &(vma->list_link));
ffffffffc0202e24:	02058613          	addi	a2,a1,32
    prev->next = next->prev = elm;
ffffffffc0202e28:	e390                	sd	a2,0(a5)
ffffffffc0202e2a:	e690                	sd	a2,8(a3)
}
ffffffffc0202e2c:	60a2                	ld	ra,8(sp)
    elm->next = next;
ffffffffc0202e2e:	f59c                	sd	a5,40(a1)
    elm->prev = prev;
ffffffffc0202e30:	f194                	sd	a3,32(a1)
    mm->map_count++;
ffffffffc0202e32:	0017079b          	addiw	a5,a4,1
ffffffffc0202e36:	d11c                	sw	a5,32(a0)
}
ffffffffc0202e38:	0141                	addi	sp,sp,16
ffffffffc0202e3a:	8082                	ret
    if (le_prev != list)
ffffffffc0202e3c:	fca690e3          	bne	a3,a0,ffffffffc0202dfc <insert_vma_struct+0x26>
ffffffffc0202e40:	bfd1                	j	ffffffffc0202e14 <insert_vma_struct+0x3e>
ffffffffc0202e42:	f31ff0ef          	jal	ra,ffffffffc0202d72 <check_vma_overlap.part.0>
    assert(vma->vm_start < vma->vm_end);
ffffffffc0202e46:	00002697          	auipc	a3,0x2
ffffffffc0202e4a:	5c268693          	addi	a3,a3,1474 # ffffffffc0205408 <default_pmm_manager+0x728>
ffffffffc0202e4e:	00002617          	auipc	a2,0x2
ffffffffc0202e52:	ae260613          	addi	a2,a2,-1310 # ffffffffc0204930 <commands+0x818>
ffffffffc0202e56:	08e00593          	li	a1,142
ffffffffc0202e5a:	00002517          	auipc	a0,0x2
ffffffffc0202e5e:	59e50513          	addi	a0,a0,1438 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc0202e62:	df8fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(prev->vm_end <= next->vm_start);
ffffffffc0202e66:	00002697          	auipc	a3,0x2
ffffffffc0202e6a:	5e268693          	addi	a3,a3,1506 # ffffffffc0205448 <default_pmm_manager+0x768>
ffffffffc0202e6e:	00002617          	auipc	a2,0x2
ffffffffc0202e72:	ac260613          	addi	a2,a2,-1342 # ffffffffc0204930 <commands+0x818>
ffffffffc0202e76:	08700593          	li	a1,135
ffffffffc0202e7a:	00002517          	auipc	a0,0x2
ffffffffc0202e7e:	57e50513          	addi	a0,a0,1406 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc0202e82:	dd8fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(prev->vm_start < prev->vm_end);
ffffffffc0202e86:	00002697          	auipc	a3,0x2
ffffffffc0202e8a:	5a268693          	addi	a3,a3,1442 # ffffffffc0205428 <default_pmm_manager+0x748>
ffffffffc0202e8e:	00002617          	auipc	a2,0x2
ffffffffc0202e92:	aa260613          	addi	a2,a2,-1374 # ffffffffc0204930 <commands+0x818>
ffffffffc0202e96:	08600593          	li	a1,134
ffffffffc0202e9a:	00002517          	auipc	a0,0x2
ffffffffc0202e9e:	55e50513          	addi	a0,a0,1374 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc0202ea2:	db8fd0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0202ea6 <vmm_init>:
}

// vmm_init - initialize virtual memory management
//          - now just call check_vmm to check correctness of vmm
void vmm_init(void)
{
ffffffffc0202ea6:	7139                	addi	sp,sp,-64
    struct mm_struct *mm = kmalloc(sizeof(struct mm_struct));
ffffffffc0202ea8:	03000513          	li	a0,48
{
ffffffffc0202eac:	fc06                	sd	ra,56(sp)
ffffffffc0202eae:	f822                	sd	s0,48(sp)
ffffffffc0202eb0:	f426                	sd	s1,40(sp)
ffffffffc0202eb2:	f04a                	sd	s2,32(sp)
ffffffffc0202eb4:	ec4e                	sd	s3,24(sp)
ffffffffc0202eb6:	e852                	sd	s4,16(sp)
ffffffffc0202eb8:	e456                	sd	s5,8(sp)
    struct mm_struct *mm = kmalloc(sizeof(struct mm_struct));
ffffffffc0202eba:	bd5fe0ef          	jal	ra,ffffffffc0201a8e <kmalloc>
    if (mm != NULL)
ffffffffc0202ebe:	2e050f63          	beqz	a0,ffffffffc02031bc <vmm_init+0x316>
ffffffffc0202ec2:	84aa                	mv	s1,a0
    elm->prev = elm->next = elm;
ffffffffc0202ec4:	e508                	sd	a0,8(a0)
ffffffffc0202ec6:	e108                	sd	a0,0(a0)
        mm->mmap_cache = NULL;
ffffffffc0202ec8:	00053823          	sd	zero,16(a0)
        mm->pgdir = NULL;
ffffffffc0202ecc:	00053c23          	sd	zero,24(a0)
        mm->map_count = 0;
ffffffffc0202ed0:	02052023          	sw	zero,32(a0)
        mm->sm_priv = NULL;
ffffffffc0202ed4:	02053423          	sd	zero,40(a0)
ffffffffc0202ed8:	03200413          	li	s0,50
ffffffffc0202edc:	a811                	j	ffffffffc0202ef0 <vmm_init+0x4a>
        vma->vm_start = vm_start;
ffffffffc0202ede:	e500                	sd	s0,8(a0)
        vma->vm_end = vm_end;
ffffffffc0202ee0:	e91c                	sd	a5,16(a0)
        vma->vm_flags = vm_flags;
ffffffffc0202ee2:	00052c23          	sw	zero,24(a0)
    assert(mm != NULL);

    int step1 = 10, step2 = step1 * 10;

    int i;
    for (i = step1; i >= 1; i--)
ffffffffc0202ee6:	146d                	addi	s0,s0,-5
    {
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
ffffffffc0202ee8:	8526                	mv	a0,s1
ffffffffc0202eea:	eedff0ef          	jal	ra,ffffffffc0202dd6 <insert_vma_struct>
    for (i = step1; i >= 1; i--)
ffffffffc0202eee:	c80d                	beqz	s0,ffffffffc0202f20 <vmm_init+0x7a>
    struct vma_struct *vma = kmalloc(sizeof(struct vma_struct));
ffffffffc0202ef0:	03000513          	li	a0,48
ffffffffc0202ef4:	b9bfe0ef          	jal	ra,ffffffffc0201a8e <kmalloc>
ffffffffc0202ef8:	85aa                	mv	a1,a0
ffffffffc0202efa:	00240793          	addi	a5,s0,2
    if (vma != NULL)
ffffffffc0202efe:	f165                	bnez	a0,ffffffffc0202ede <vmm_init+0x38>
        assert(vma != NULL);
ffffffffc0202f00:	00002697          	auipc	a3,0x2
ffffffffc0202f04:	6e068693          	addi	a3,a3,1760 # ffffffffc02055e0 <default_pmm_manager+0x900>
ffffffffc0202f08:	00002617          	auipc	a2,0x2
ffffffffc0202f0c:	a2860613          	addi	a2,a2,-1496 # ffffffffc0204930 <commands+0x818>
ffffffffc0202f10:	0da00593          	li	a1,218
ffffffffc0202f14:	00002517          	auipc	a0,0x2
ffffffffc0202f18:	4e450513          	addi	a0,a0,1252 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc0202f1c:	d3efd0ef          	jal	ra,ffffffffc020045a <__panic>
ffffffffc0202f20:	03700413          	li	s0,55
    }

    for (i = step1 + 1; i <= step2; i++)
ffffffffc0202f24:	1f900913          	li	s2,505
ffffffffc0202f28:	a819                	j	ffffffffc0202f3e <vmm_init+0x98>
        vma->vm_start = vm_start;
ffffffffc0202f2a:	e500                	sd	s0,8(a0)
        vma->vm_end = vm_end;
ffffffffc0202f2c:	e91c                	sd	a5,16(a0)
        vma->vm_flags = vm_flags;
ffffffffc0202f2e:	00052c23          	sw	zero,24(a0)
    for (i = step1 + 1; i <= step2; i++)
ffffffffc0202f32:	0415                	addi	s0,s0,5
    {
        struct vma_struct *vma = vma_create(i * 5, i * 5 + 2, 0);
        assert(vma != NULL);
        insert_vma_struct(mm, vma);
ffffffffc0202f34:	8526                	mv	a0,s1
ffffffffc0202f36:	ea1ff0ef          	jal	ra,ffffffffc0202dd6 <insert_vma_struct>
    for (i = step1 + 1; i <= step2; i++)
ffffffffc0202f3a:	03240a63          	beq	s0,s2,ffffffffc0202f6e <vmm_init+0xc8>
    struct vma_struct *vma = kmalloc(sizeof(struct vma_struct));
ffffffffc0202f3e:	03000513          	li	a0,48
ffffffffc0202f42:	b4dfe0ef          	jal	ra,ffffffffc0201a8e <kmalloc>
ffffffffc0202f46:	85aa                	mv	a1,a0
ffffffffc0202f48:	00240793          	addi	a5,s0,2
    if (vma != NULL)
ffffffffc0202f4c:	fd79                	bnez	a0,ffffffffc0202f2a <vmm_init+0x84>
        assert(vma != NULL);
ffffffffc0202f4e:	00002697          	auipc	a3,0x2
ffffffffc0202f52:	69268693          	addi	a3,a3,1682 # ffffffffc02055e0 <default_pmm_manager+0x900>
ffffffffc0202f56:	00002617          	auipc	a2,0x2
ffffffffc0202f5a:	9da60613          	addi	a2,a2,-1574 # ffffffffc0204930 <commands+0x818>
ffffffffc0202f5e:	0e100593          	li	a1,225
ffffffffc0202f62:	00002517          	auipc	a0,0x2
ffffffffc0202f66:	49650513          	addi	a0,a0,1174 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc0202f6a:	cf0fd0ef          	jal	ra,ffffffffc020045a <__panic>
    return listelm->next;
ffffffffc0202f6e:	649c                	ld	a5,8(s1)
ffffffffc0202f70:	471d                	li	a4,7
    }

    list_entry_t *le = list_next(&(mm->mmap_list));

    for (i = 1; i <= step2; i++)
ffffffffc0202f72:	1fb00593          	li	a1,507
    {
        assert(le != &(mm->mmap_list));
ffffffffc0202f76:	18f48363          	beq	s1,a5,ffffffffc02030fc <vmm_init+0x256>
        struct vma_struct *mmap = le2vma(le, list_link);
        assert(mmap->vm_start == i * 5 && mmap->vm_end == i * 5 + 2);
ffffffffc0202f7a:	fe87b603          	ld	a2,-24(a5)
ffffffffc0202f7e:	ffe70693          	addi	a3,a4,-2 # ffe <kern_entry-0xffffffffc01ff002>
ffffffffc0202f82:	10d61d63          	bne	a2,a3,ffffffffc020309c <vmm_init+0x1f6>
ffffffffc0202f86:	ff07b683          	ld	a3,-16(a5)
ffffffffc0202f8a:	10e69963          	bne	a3,a4,ffffffffc020309c <vmm_init+0x1f6>
    for (i = 1; i <= step2; i++)
ffffffffc0202f8e:	0715                	addi	a4,a4,5
ffffffffc0202f90:	679c                	ld	a5,8(a5)
ffffffffc0202f92:	feb712e3          	bne	a4,a1,ffffffffc0202f76 <vmm_init+0xd0>
ffffffffc0202f96:	4a1d                	li	s4,7
ffffffffc0202f98:	4415                	li	s0,5
        le = list_next(le);
    }

    for (i = 5; i <= 5 * step2; i += 5)
ffffffffc0202f9a:	1f900a93          	li	s5,505
    {
        struct vma_struct *vma1 = find_vma(mm, i);
ffffffffc0202f9e:	85a2                	mv	a1,s0
ffffffffc0202fa0:	8526                	mv	a0,s1
ffffffffc0202fa2:	df5ff0ef          	jal	ra,ffffffffc0202d96 <find_vma>
ffffffffc0202fa6:	892a                	mv	s2,a0
        assert(vma1 != NULL);
ffffffffc0202fa8:	18050a63          	beqz	a0,ffffffffc020313c <vmm_init+0x296>
        struct vma_struct *vma2 = find_vma(mm, i + 1);
ffffffffc0202fac:	00140593          	addi	a1,s0,1
ffffffffc0202fb0:	8526                	mv	a0,s1
ffffffffc0202fb2:	de5ff0ef          	jal	ra,ffffffffc0202d96 <find_vma>
ffffffffc0202fb6:	89aa                	mv	s3,a0
        assert(vma2 != NULL);
ffffffffc0202fb8:	16050263          	beqz	a0,ffffffffc020311c <vmm_init+0x276>
        struct vma_struct *vma3 = find_vma(mm, i + 2);
ffffffffc0202fbc:	85d2                	mv	a1,s4
ffffffffc0202fbe:	8526                	mv	a0,s1
ffffffffc0202fc0:	dd7ff0ef          	jal	ra,ffffffffc0202d96 <find_vma>
        assert(vma3 == NULL);
ffffffffc0202fc4:	18051c63          	bnez	a0,ffffffffc020315c <vmm_init+0x2b6>
        struct vma_struct *vma4 = find_vma(mm, i + 3);
ffffffffc0202fc8:	00340593          	addi	a1,s0,3
ffffffffc0202fcc:	8526                	mv	a0,s1
ffffffffc0202fce:	dc9ff0ef          	jal	ra,ffffffffc0202d96 <find_vma>
        assert(vma4 == NULL);
ffffffffc0202fd2:	1c051563          	bnez	a0,ffffffffc020319c <vmm_init+0x2f6>
        struct vma_struct *vma5 = find_vma(mm, i + 4);
ffffffffc0202fd6:	00440593          	addi	a1,s0,4
ffffffffc0202fda:	8526                	mv	a0,s1
ffffffffc0202fdc:	dbbff0ef          	jal	ra,ffffffffc0202d96 <find_vma>
        assert(vma5 == NULL);
ffffffffc0202fe0:	18051e63          	bnez	a0,ffffffffc020317c <vmm_init+0x2d6>

        assert(vma1->vm_start == i && vma1->vm_end == i + 2);
ffffffffc0202fe4:	00893783          	ld	a5,8(s2)
ffffffffc0202fe8:	0c879a63          	bne	a5,s0,ffffffffc02030bc <vmm_init+0x216>
ffffffffc0202fec:	01093783          	ld	a5,16(s2)
ffffffffc0202ff0:	0d479663          	bne	a5,s4,ffffffffc02030bc <vmm_init+0x216>
        assert(vma2->vm_start == i && vma2->vm_end == i + 2);
ffffffffc0202ff4:	0089b783          	ld	a5,8(s3)
ffffffffc0202ff8:	0e879263          	bne	a5,s0,ffffffffc02030dc <vmm_init+0x236>
ffffffffc0202ffc:	0109b783          	ld	a5,16(s3)
ffffffffc0203000:	0d479e63          	bne	a5,s4,ffffffffc02030dc <vmm_init+0x236>
    for (i = 5; i <= 5 * step2; i += 5)
ffffffffc0203004:	0415                	addi	s0,s0,5
ffffffffc0203006:	0a15                	addi	s4,s4,5
ffffffffc0203008:	f9541be3          	bne	s0,s5,ffffffffc0202f9e <vmm_init+0xf8>
ffffffffc020300c:	4411                	li	s0,4
    }

    for (i = 4; i >= 0; i--)
ffffffffc020300e:	597d                	li	s2,-1
    {
        struct vma_struct *vma_below_5 = find_vma(mm, i);
ffffffffc0203010:	85a2                	mv	a1,s0
ffffffffc0203012:	8526                	mv	a0,s1
ffffffffc0203014:	d83ff0ef          	jal	ra,ffffffffc0202d96 <find_vma>
ffffffffc0203018:	0004059b          	sext.w	a1,s0
        if (vma_below_5 != NULL)
ffffffffc020301c:	c90d                	beqz	a0,ffffffffc020304e <vmm_init+0x1a8>
        {
            cprintf("vma_below_5: i %x, start %x, end %x\n", i, vma_below_5->vm_start, vma_below_5->vm_end);
ffffffffc020301e:	6914                	ld	a3,16(a0)
ffffffffc0203020:	6510                	ld	a2,8(a0)
ffffffffc0203022:	00002517          	auipc	a0,0x2
ffffffffc0203026:	54650513          	addi	a0,a0,1350 # ffffffffc0205568 <default_pmm_manager+0x888>
ffffffffc020302a:	96afd0ef          	jal	ra,ffffffffc0200194 <cprintf>
        }
        assert(vma_below_5 == NULL);
ffffffffc020302e:	00002697          	auipc	a3,0x2
ffffffffc0203032:	56268693          	addi	a3,a3,1378 # ffffffffc0205590 <default_pmm_manager+0x8b0>
ffffffffc0203036:	00002617          	auipc	a2,0x2
ffffffffc020303a:	8fa60613          	addi	a2,a2,-1798 # ffffffffc0204930 <commands+0x818>
ffffffffc020303e:	10700593          	li	a1,263
ffffffffc0203042:	00002517          	auipc	a0,0x2
ffffffffc0203046:	3b650513          	addi	a0,a0,950 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc020304a:	c10fd0ef          	jal	ra,ffffffffc020045a <__panic>
    for (i = 4; i >= 0; i--)
ffffffffc020304e:	147d                	addi	s0,s0,-1
ffffffffc0203050:	fd2410e3          	bne	s0,s2,ffffffffc0203010 <vmm_init+0x16a>
ffffffffc0203054:	6488                	ld	a0,8(s1)
    while ((le = list_next(list)) != list)
ffffffffc0203056:	00a48c63          	beq	s1,a0,ffffffffc020306e <vmm_init+0x1c8>
    __list_del(listelm->prev, listelm->next);
ffffffffc020305a:	6118                	ld	a4,0(a0)
ffffffffc020305c:	651c                	ld	a5,8(a0)
        kfree(le2vma(le, list_link)); // kfree vma
ffffffffc020305e:	1501                	addi	a0,a0,-32
    prev->next = next;
ffffffffc0203060:	e71c                	sd	a5,8(a4)
    next->prev = prev;
ffffffffc0203062:	e398                	sd	a4,0(a5)
ffffffffc0203064:	adbfe0ef          	jal	ra,ffffffffc0201b3e <kfree>
    return listelm->next;
ffffffffc0203068:	6488                	ld	a0,8(s1)
    while ((le = list_next(list)) != list)
ffffffffc020306a:	fea498e3          	bne	s1,a0,ffffffffc020305a <vmm_init+0x1b4>
    kfree(mm); // kfree mm
ffffffffc020306e:	8526                	mv	a0,s1
ffffffffc0203070:	acffe0ef          	jal	ra,ffffffffc0201b3e <kfree>
    }

    mm_destroy(mm);

    cprintf("check_vma_struct() succeeded!\n");
ffffffffc0203074:	00002517          	auipc	a0,0x2
ffffffffc0203078:	53450513          	addi	a0,a0,1332 # ffffffffc02055a8 <default_pmm_manager+0x8c8>
ffffffffc020307c:	918fd0ef          	jal	ra,ffffffffc0200194 <cprintf>
}
ffffffffc0203080:	7442                	ld	s0,48(sp)
ffffffffc0203082:	70e2                	ld	ra,56(sp)
ffffffffc0203084:	74a2                	ld	s1,40(sp)
ffffffffc0203086:	7902                	ld	s2,32(sp)
ffffffffc0203088:	69e2                	ld	s3,24(sp)
ffffffffc020308a:	6a42                	ld	s4,16(sp)
ffffffffc020308c:	6aa2                	ld	s5,8(sp)
    cprintf("check_vmm() succeeded.\n");
ffffffffc020308e:	00002517          	auipc	a0,0x2
ffffffffc0203092:	53a50513          	addi	a0,a0,1338 # ffffffffc02055c8 <default_pmm_manager+0x8e8>
}
ffffffffc0203096:	6121                	addi	sp,sp,64
    cprintf("check_vmm() succeeded.\n");
ffffffffc0203098:	8fcfd06f          	j	ffffffffc0200194 <cprintf>
        assert(mmap->vm_start == i * 5 && mmap->vm_end == i * 5 + 2);
ffffffffc020309c:	00002697          	auipc	a3,0x2
ffffffffc02030a0:	3e468693          	addi	a3,a3,996 # ffffffffc0205480 <default_pmm_manager+0x7a0>
ffffffffc02030a4:	00002617          	auipc	a2,0x2
ffffffffc02030a8:	88c60613          	addi	a2,a2,-1908 # ffffffffc0204930 <commands+0x818>
ffffffffc02030ac:	0eb00593          	li	a1,235
ffffffffc02030b0:	00002517          	auipc	a0,0x2
ffffffffc02030b4:	34850513          	addi	a0,a0,840 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc02030b8:	ba2fd0ef          	jal	ra,ffffffffc020045a <__panic>
        assert(vma1->vm_start == i && vma1->vm_end == i + 2);
ffffffffc02030bc:	00002697          	auipc	a3,0x2
ffffffffc02030c0:	44c68693          	addi	a3,a3,1100 # ffffffffc0205508 <default_pmm_manager+0x828>
ffffffffc02030c4:	00002617          	auipc	a2,0x2
ffffffffc02030c8:	86c60613          	addi	a2,a2,-1940 # ffffffffc0204930 <commands+0x818>
ffffffffc02030cc:	0fc00593          	li	a1,252
ffffffffc02030d0:	00002517          	auipc	a0,0x2
ffffffffc02030d4:	32850513          	addi	a0,a0,808 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc02030d8:	b82fd0ef          	jal	ra,ffffffffc020045a <__panic>
        assert(vma2->vm_start == i && vma2->vm_end == i + 2);
ffffffffc02030dc:	00002697          	auipc	a3,0x2
ffffffffc02030e0:	45c68693          	addi	a3,a3,1116 # ffffffffc0205538 <default_pmm_manager+0x858>
ffffffffc02030e4:	00002617          	auipc	a2,0x2
ffffffffc02030e8:	84c60613          	addi	a2,a2,-1972 # ffffffffc0204930 <commands+0x818>
ffffffffc02030ec:	0fd00593          	li	a1,253
ffffffffc02030f0:	00002517          	auipc	a0,0x2
ffffffffc02030f4:	30850513          	addi	a0,a0,776 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc02030f8:	b62fd0ef          	jal	ra,ffffffffc020045a <__panic>
        assert(le != &(mm->mmap_list));
ffffffffc02030fc:	00002697          	auipc	a3,0x2
ffffffffc0203100:	36c68693          	addi	a3,a3,876 # ffffffffc0205468 <default_pmm_manager+0x788>
ffffffffc0203104:	00002617          	auipc	a2,0x2
ffffffffc0203108:	82c60613          	addi	a2,a2,-2004 # ffffffffc0204930 <commands+0x818>
ffffffffc020310c:	0e900593          	li	a1,233
ffffffffc0203110:	00002517          	auipc	a0,0x2
ffffffffc0203114:	2e850513          	addi	a0,a0,744 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc0203118:	b42fd0ef          	jal	ra,ffffffffc020045a <__panic>
        assert(vma2 != NULL);
ffffffffc020311c:	00002697          	auipc	a3,0x2
ffffffffc0203120:	3ac68693          	addi	a3,a3,940 # ffffffffc02054c8 <default_pmm_manager+0x7e8>
ffffffffc0203124:	00002617          	auipc	a2,0x2
ffffffffc0203128:	80c60613          	addi	a2,a2,-2036 # ffffffffc0204930 <commands+0x818>
ffffffffc020312c:	0f400593          	li	a1,244
ffffffffc0203130:	00002517          	auipc	a0,0x2
ffffffffc0203134:	2c850513          	addi	a0,a0,712 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc0203138:	b22fd0ef          	jal	ra,ffffffffc020045a <__panic>
        assert(vma1 != NULL);
ffffffffc020313c:	00002697          	auipc	a3,0x2
ffffffffc0203140:	37c68693          	addi	a3,a3,892 # ffffffffc02054b8 <default_pmm_manager+0x7d8>
ffffffffc0203144:	00001617          	auipc	a2,0x1
ffffffffc0203148:	7ec60613          	addi	a2,a2,2028 # ffffffffc0204930 <commands+0x818>
ffffffffc020314c:	0f200593          	li	a1,242
ffffffffc0203150:	00002517          	auipc	a0,0x2
ffffffffc0203154:	2a850513          	addi	a0,a0,680 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc0203158:	b02fd0ef          	jal	ra,ffffffffc020045a <__panic>
        assert(vma3 == NULL);
ffffffffc020315c:	00002697          	auipc	a3,0x2
ffffffffc0203160:	37c68693          	addi	a3,a3,892 # ffffffffc02054d8 <default_pmm_manager+0x7f8>
ffffffffc0203164:	00001617          	auipc	a2,0x1
ffffffffc0203168:	7cc60613          	addi	a2,a2,1996 # ffffffffc0204930 <commands+0x818>
ffffffffc020316c:	0f600593          	li	a1,246
ffffffffc0203170:	00002517          	auipc	a0,0x2
ffffffffc0203174:	28850513          	addi	a0,a0,648 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc0203178:	ae2fd0ef          	jal	ra,ffffffffc020045a <__panic>
        assert(vma5 == NULL);
ffffffffc020317c:	00002697          	auipc	a3,0x2
ffffffffc0203180:	37c68693          	addi	a3,a3,892 # ffffffffc02054f8 <default_pmm_manager+0x818>
ffffffffc0203184:	00001617          	auipc	a2,0x1
ffffffffc0203188:	7ac60613          	addi	a2,a2,1964 # ffffffffc0204930 <commands+0x818>
ffffffffc020318c:	0fa00593          	li	a1,250
ffffffffc0203190:	00002517          	auipc	a0,0x2
ffffffffc0203194:	26850513          	addi	a0,a0,616 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc0203198:	ac2fd0ef          	jal	ra,ffffffffc020045a <__panic>
        assert(vma4 == NULL);
ffffffffc020319c:	00002697          	auipc	a3,0x2
ffffffffc02031a0:	34c68693          	addi	a3,a3,844 # ffffffffc02054e8 <default_pmm_manager+0x808>
ffffffffc02031a4:	00001617          	auipc	a2,0x1
ffffffffc02031a8:	78c60613          	addi	a2,a2,1932 # ffffffffc0204930 <commands+0x818>
ffffffffc02031ac:	0f800593          	li	a1,248
ffffffffc02031b0:	00002517          	auipc	a0,0x2
ffffffffc02031b4:	24850513          	addi	a0,a0,584 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc02031b8:	aa2fd0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(mm != NULL);
ffffffffc02031bc:	00002697          	auipc	a3,0x2
ffffffffc02031c0:	43468693          	addi	a3,a3,1076 # ffffffffc02055f0 <default_pmm_manager+0x910>
ffffffffc02031c4:	00001617          	auipc	a2,0x1
ffffffffc02031c8:	76c60613          	addi	a2,a2,1900 # ffffffffc0204930 <commands+0x818>
ffffffffc02031cc:	0d200593          	li	a1,210
ffffffffc02031d0:	00002517          	auipc	a0,0x2
ffffffffc02031d4:	22850513          	addi	a0,a0,552 # ffffffffc02053f8 <default_pmm_manager+0x718>
ffffffffc02031d8:	a82fd0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc02031dc <kernel_thread_entry>:
.text
.globl kernel_thread_entry
kernel_thread_entry:        # void kernel_thread(void)
	move a0, s1
ffffffffc02031dc:	8526                	mv	a0,s1
	jalr s0
ffffffffc02031de:	9402                	jalr	s0

	jal do_exit
ffffffffc02031e0:	424000ef          	jal	ra,ffffffffc0203604 <do_exit>

ffffffffc02031e4 <alloc_proc>:
void switch_to(struct context *from, struct context *to);

// alloc_proc - alloc a proc_struct and init all fields of proc_struct
static struct proc_struct *
alloc_proc(void)
{
ffffffffc02031e4:	1141                	addi	sp,sp,-16
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
ffffffffc02031e6:	0e800513          	li	a0,232
{
ffffffffc02031ea:	e022                	sd	s0,0(sp)
ffffffffc02031ec:	e406                	sd	ra,8(sp)
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
ffffffffc02031ee:	8a1fe0ef          	jal	ra,ffffffffc0201a8e <kmalloc>
ffffffffc02031f2:	842a                	mv	s0,a0
    if (proc != NULL)
ffffffffc02031f4:	c521                	beqz	a0,ffffffffc020323c <alloc_proc+0x58>
         *       struct trapframe *tf;                       // Trap frame for current interrupt
         *       uintptr_t pgdir;                            // the base addr of Page Directroy Table(PDT)
         *       uint32_t flags;                             // Process flag
         *       char name[PROC_NAME_LEN + 1];               // Process name
         */
        proc->state = PROC_UNINIT;  // 设置进程为未初始化状态
ffffffffc02031f6:	57fd                	li	a5,-1
ffffffffc02031f8:	1782                	slli	a5,a5,0x20
ffffffffc02031fa:	e11c                	sd	a5,0(a0)
        proc->runs = 0;              // 初始化运行时间
        proc->kstack = 0;            // 内核栈地址
        proc->need_resched = 0;      // 不需要调度
        proc->parent = NULL;         // 父进程为空
        proc->mm = NULL;             // 虚拟内存管理为空
        memset(&(proc->context), 0, sizeof(struct context));  // 初始化上下文
ffffffffc02031fc:	07000613          	li	a2,112
ffffffffc0203200:	4581                	li	a1,0
        proc->runs = 0;              // 初始化运行时间
ffffffffc0203202:	00052423          	sw	zero,8(a0)
        proc->kstack = 0;            // 内核栈地址
ffffffffc0203206:	00053823          	sd	zero,16(a0)
        proc->need_resched = 0;      // 不需要调度
ffffffffc020320a:	00052c23          	sw	zero,24(a0)
        proc->parent = NULL;         // 父进程为空
ffffffffc020320e:	02053023          	sd	zero,32(a0)
        proc->mm = NULL;             // 虚拟内存管理为空
ffffffffc0203212:	02053423          	sd	zero,40(a0)
        memset(&(proc->context), 0, sizeof(struct context));  // 初始化上下文
ffffffffc0203216:	03050513          	addi	a0,a0,48
ffffffffc020321a:	447000ef          	jal	ra,ffffffffc0203e60 <memset>
        proc->tf = NULL;             // 中断帧指针为空
        proc->pgdir = boot_pgdir_pa; // 使用内核页目录表的物理地址
ffffffffc020321e:	0000a797          	auipc	a5,0xa
ffffffffc0203222:	27a7b783          	ld	a5,634(a5) # ffffffffc020d498 <boot_pgdir_pa>
        proc->tf = NULL;             // 中断帧指针为空
ffffffffc0203226:	0a043023          	sd	zero,160(s0)
        proc->pgdir = boot_pgdir_pa; // 使用内核页目录表的物理地址
ffffffffc020322a:	f45c                	sd	a5,168(s0)
        proc->flags = 0;             // 标志位为0
ffffffffc020322c:	0a042823          	sw	zero,176(s0)
        memset(proc->name, 0, PROC_NAME_LEN + 1);  // 清空进程名
ffffffffc0203230:	4641                	li	a2,16
ffffffffc0203232:	4581                	li	a1,0
ffffffffc0203234:	0b440513          	addi	a0,s0,180
ffffffffc0203238:	429000ef          	jal	ra,ffffffffc0203e60 <memset>
    }
    return proc;
}
ffffffffc020323c:	60a2                	ld	ra,8(sp)
ffffffffc020323e:	8522                	mv	a0,s0
ffffffffc0203240:	6402                	ld	s0,0(sp)
ffffffffc0203242:	0141                	addi	sp,sp,16
ffffffffc0203244:	8082                	ret

ffffffffc0203246 <forkret>:
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void
forkret(void)
{
    forkrets(current->tf);
ffffffffc0203246:	0000a797          	auipc	a5,0xa
ffffffffc020324a:	2827b783          	ld	a5,642(a5) # ffffffffc020d4c8 <current>
ffffffffc020324e:	73c8                	ld	a0,160(a5)
ffffffffc0203250:	b5dfd06f          	j	ffffffffc0200dac <forkrets>

ffffffffc0203254 <init_main>:
}

// init_main - the second kernel thread used to create user_main kernel threads
static int
init_main(void *arg)
{
ffffffffc0203254:	7179                	addi	sp,sp,-48
ffffffffc0203256:	ec26                	sd	s1,24(sp)
    memset(name, 0, sizeof(name));
ffffffffc0203258:	0000a497          	auipc	s1,0xa
ffffffffc020325c:	1f048493          	addi	s1,s1,496 # ffffffffc020d448 <name.2>
{
ffffffffc0203260:	f022                	sd	s0,32(sp)
ffffffffc0203262:	e84a                	sd	s2,16(sp)
ffffffffc0203264:	842a                	mv	s0,a0
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
ffffffffc0203266:	0000a917          	auipc	s2,0xa
ffffffffc020326a:	26293903          	ld	s2,610(s2) # ffffffffc020d4c8 <current>
    memset(name, 0, sizeof(name));
ffffffffc020326e:	4641                	li	a2,16
ffffffffc0203270:	4581                	li	a1,0
ffffffffc0203272:	8526                	mv	a0,s1
{
ffffffffc0203274:	f406                	sd	ra,40(sp)
ffffffffc0203276:	e44e                	sd	s3,8(sp)
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
ffffffffc0203278:	00492983          	lw	s3,4(s2)
    memset(name, 0, sizeof(name));
ffffffffc020327c:	3e5000ef          	jal	ra,ffffffffc0203e60 <memset>
    return memcpy(name, proc->name, PROC_NAME_LEN);
ffffffffc0203280:	0b490593          	addi	a1,s2,180
ffffffffc0203284:	463d                	li	a2,15
ffffffffc0203286:	8526                	mv	a0,s1
ffffffffc0203288:	3eb000ef          	jal	ra,ffffffffc0203e72 <memcpy>
ffffffffc020328c:	862a                	mv	a2,a0
    cprintf("this initproc, pid = %d, name = \"%s\"\n", current->pid, get_proc_name(current));
ffffffffc020328e:	85ce                	mv	a1,s3
ffffffffc0203290:	00002517          	auipc	a0,0x2
ffffffffc0203294:	37050513          	addi	a0,a0,880 # ffffffffc0205600 <default_pmm_manager+0x920>
ffffffffc0203298:	efdfc0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("To U: \"%s\".\n", (const char *)arg);
ffffffffc020329c:	85a2                	mv	a1,s0
ffffffffc020329e:	00002517          	auipc	a0,0x2
ffffffffc02032a2:	38a50513          	addi	a0,a0,906 # ffffffffc0205628 <default_pmm_manager+0x948>
ffffffffc02032a6:	eeffc0ef          	jal	ra,ffffffffc0200194 <cprintf>
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");
ffffffffc02032aa:	00002517          	auipc	a0,0x2
ffffffffc02032ae:	38e50513          	addi	a0,a0,910 # ffffffffc0205638 <default_pmm_manager+0x958>
ffffffffc02032b2:	ee3fc0ef          	jal	ra,ffffffffc0200194 <cprintf>
    return 0;
}
ffffffffc02032b6:	70a2                	ld	ra,40(sp)
ffffffffc02032b8:	7402                	ld	s0,32(sp)
ffffffffc02032ba:	64e2                	ld	s1,24(sp)
ffffffffc02032bc:	6942                	ld	s2,16(sp)
ffffffffc02032be:	69a2                	ld	s3,8(sp)
ffffffffc02032c0:	4501                	li	a0,0
ffffffffc02032c2:	6145                	addi	sp,sp,48
ffffffffc02032c4:	8082                	ret

ffffffffc02032c6 <proc_run>:
{
ffffffffc02032c6:	7179                	addi	sp,sp,-48
ffffffffc02032c8:	ec4a                	sd	s2,24(sp)
    if (proc != current)
ffffffffc02032ca:	0000a917          	auipc	s2,0xa
ffffffffc02032ce:	1fe90913          	addi	s2,s2,510 # ffffffffc020d4c8 <current>
{
ffffffffc02032d2:	f026                	sd	s1,32(sp)
    if (proc != current)
ffffffffc02032d4:	00093483          	ld	s1,0(s2)
{
ffffffffc02032d8:	f406                	sd	ra,40(sp)
ffffffffc02032da:	e84e                	sd	s3,16(sp)
    if (proc != current)
ffffffffc02032dc:	02a48963          	beq	s1,a0,ffffffffc020330e <proc_run+0x48>
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc02032e0:	100027f3          	csrr	a5,sstatus
ffffffffc02032e4:	8b89                	andi	a5,a5,2
    return 0;
ffffffffc02032e6:	4981                	li	s3,0
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc02032e8:	e3a1                	bnez	a5,ffffffffc0203328 <proc_run+0x62>
            lsatp(next->pgdir);
ffffffffc02032ea:	755c                	ld	a5,168(a0)
#define barrier() __asm__ __volatile__("fence" ::: "memory")

static inline void
lsatp(unsigned int pgdir)
{
  write_csr(satp, SATP32_MODE | (pgdir >> RISCV_PGSHIFT));
ffffffffc02032ec:	80000737          	lui	a4,0x80000
            current = proc;
ffffffffc02032f0:	00a93023          	sd	a0,0(s2)
ffffffffc02032f4:	00c7d79b          	srliw	a5,a5,0xc
ffffffffc02032f8:	8fd9                	or	a5,a5,a4
ffffffffc02032fa:	18079073          	csrw	satp,a5
            switch_to(&(prev->context), &(next->context));
ffffffffc02032fe:	03050593          	addi	a1,a0,48
ffffffffc0203302:	03048513          	addi	a0,s1,48
ffffffffc0203306:	584000ef          	jal	ra,ffffffffc020388a <switch_to>
    if (flag) {
ffffffffc020330a:	00099863          	bnez	s3,ffffffffc020331a <proc_run+0x54>
}
ffffffffc020330e:	70a2                	ld	ra,40(sp)
ffffffffc0203310:	7482                	ld	s1,32(sp)
ffffffffc0203312:	6962                	ld	s2,24(sp)
ffffffffc0203314:	69c2                	ld	s3,16(sp)
ffffffffc0203316:	6145                	addi	sp,sp,48
ffffffffc0203318:	8082                	ret
ffffffffc020331a:	70a2                	ld	ra,40(sp)
ffffffffc020331c:	7482                	ld	s1,32(sp)
ffffffffc020331e:	6962                	ld	s2,24(sp)
ffffffffc0203320:	69c2                	ld	s3,16(sp)
ffffffffc0203322:	6145                	addi	sp,sp,48
        intr_enable();
ffffffffc0203324:	e06fd06f          	j	ffffffffc020092a <intr_enable>
ffffffffc0203328:	e42a                	sd	a0,8(sp)
        intr_disable();
ffffffffc020332a:	e06fd0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        return 1;
ffffffffc020332e:	6522                	ld	a0,8(sp)
ffffffffc0203330:	4985                	li	s3,1
ffffffffc0203332:	bf65                	j	ffffffffc02032ea <proc_run+0x24>

ffffffffc0203334 <do_fork>:
{
ffffffffc0203334:	7179                	addi	sp,sp,-48
ffffffffc0203336:	ec26                	sd	s1,24(sp)
    if (nr_process >= MAX_PROCESS)
ffffffffc0203338:	0000a497          	auipc	s1,0xa
ffffffffc020333c:	1a848493          	addi	s1,s1,424 # ffffffffc020d4e0 <nr_process>
ffffffffc0203340:	4098                	lw	a4,0(s1)
{
ffffffffc0203342:	f406                	sd	ra,40(sp)
ffffffffc0203344:	f022                	sd	s0,32(sp)
ffffffffc0203346:	e84a                	sd	s2,16(sp)
ffffffffc0203348:	e44e                	sd	s3,8(sp)
ffffffffc020334a:	e052                	sd	s4,0(sp)
    if (nr_process >= MAX_PROCESS)
ffffffffc020334c:	6785                	lui	a5,0x1
ffffffffc020334e:	22f75063          	bge	a4,a5,ffffffffc020356e <do_fork+0x23a>
ffffffffc0203352:	892e                	mv	s2,a1
ffffffffc0203354:	8432                	mv	s0,a2
    if ((proc = alloc_proc()) == NULL) {
ffffffffc0203356:	e8fff0ef          	jal	ra,ffffffffc02031e4 <alloc_proc>
ffffffffc020335a:	89aa                	mv	s3,a0
ffffffffc020335c:	20050e63          	beqz	a0,ffffffffc0203578 <do_fork+0x244>
    proc->parent = current;
ffffffffc0203360:	0000aa17          	auipc	s4,0xa
ffffffffc0203364:	168a0a13          	addi	s4,s4,360 # ffffffffc020d4c8 <current>
ffffffffc0203368:	000a3783          	ld	a5,0(s4)
    struct Page *page = alloc_pages(KSTACKPAGE);
ffffffffc020336c:	4509                	li	a0,2
    proc->parent = current;
ffffffffc020336e:	02f9b023          	sd	a5,32(s3)
    struct Page *page = alloc_pages(KSTACKPAGE);
ffffffffc0203372:	8fbfe0ef          	jal	ra,ffffffffc0201c6c <alloc_pages>
    if (page != NULL)
ffffffffc0203376:	1e050763          	beqz	a0,ffffffffc0203564 <do_fork+0x230>
    return page - pages + nbase;
ffffffffc020337a:	0000a697          	auipc	a3,0xa
ffffffffc020337e:	1366b683          	ld	a3,310(a3) # ffffffffc020d4b0 <pages>
ffffffffc0203382:	40d506b3          	sub	a3,a0,a3
ffffffffc0203386:	8699                	srai	a3,a3,0x6
ffffffffc0203388:	00002517          	auipc	a0,0x2
ffffffffc020338c:	67053503          	ld	a0,1648(a0) # ffffffffc02059f8 <nbase>
ffffffffc0203390:	96aa                	add	a3,a3,a0
    return KADDR(page2pa(page));
ffffffffc0203392:	00c69793          	slli	a5,a3,0xc
ffffffffc0203396:	83b1                	srli	a5,a5,0xc
ffffffffc0203398:	0000a717          	auipc	a4,0xa
ffffffffc020339c:	11073703          	ld	a4,272(a4) # ffffffffc020d4a8 <npage>
    return page2ppn(page) << PGSHIFT;
ffffffffc02033a0:	06b2                	slli	a3,a3,0xc
    return KADDR(page2pa(page));
ffffffffc02033a2:	1ee7fd63          	bgeu	a5,a4,ffffffffc020359c <do_fork+0x268>
    assert(current->mm == NULL);
ffffffffc02033a6:	000a3783          	ld	a5,0(s4)
ffffffffc02033aa:	0000a717          	auipc	a4,0xa
ffffffffc02033ae:	11673703          	ld	a4,278(a4) # ffffffffc020d4c0 <va_pa_offset>
ffffffffc02033b2:	96ba                	add	a3,a3,a4
ffffffffc02033b4:	779c                	ld	a5,40(a5)
        proc->kstack = (uintptr_t)page2kva(page);
ffffffffc02033b6:	00d9b823          	sd	a3,16(s3)
    assert(current->mm == NULL);
ffffffffc02033ba:	1c079163          	bnez	a5,ffffffffc020357c <do_fork+0x248>
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
ffffffffc02033be:	6789                	lui	a5,0x2
ffffffffc02033c0:	ee078793          	addi	a5,a5,-288 # 1ee0 <kern_entry-0xffffffffc01fe120>
ffffffffc02033c4:	96be                	add	a3,a3,a5
    *(proc->tf) = *tf;
ffffffffc02033c6:	8622                	mv	a2,s0
    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE - sizeof(struct trapframe));
ffffffffc02033c8:	0ad9b023          	sd	a3,160(s3)
    *(proc->tf) = *tf;
ffffffffc02033cc:	87b6                	mv	a5,a3
ffffffffc02033ce:	12040893          	addi	a7,s0,288
ffffffffc02033d2:	00063803          	ld	a6,0(a2)
ffffffffc02033d6:	6608                	ld	a0,8(a2)
ffffffffc02033d8:	6a0c                	ld	a1,16(a2)
ffffffffc02033da:	6e18                	ld	a4,24(a2)
ffffffffc02033dc:	0107b023          	sd	a6,0(a5)
ffffffffc02033e0:	e788                	sd	a0,8(a5)
ffffffffc02033e2:	eb8c                	sd	a1,16(a5)
ffffffffc02033e4:	ef98                	sd	a4,24(a5)
ffffffffc02033e6:	02060613          	addi	a2,a2,32
ffffffffc02033ea:	02078793          	addi	a5,a5,32
ffffffffc02033ee:	ff1612e3          	bne	a2,a7,ffffffffc02033d2 <do_fork+0x9e>
    proc->tf->gpr.a0 = 0;
ffffffffc02033f2:	0406b823          	sd	zero,80(a3)
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;
ffffffffc02033f6:	12090563          	beqz	s2,ffffffffc0203520 <do_fork+0x1ec>
ffffffffc02033fa:	0126b823          	sd	s2,16(a3)
    proc->context.ra = (uintptr_t)forkret;
ffffffffc02033fe:	00000797          	auipc	a5,0x0
ffffffffc0203402:	e4878793          	addi	a5,a5,-440 # ffffffffc0203246 <forkret>
ffffffffc0203406:	02f9b823          	sd	a5,48(s3)
    proc->context.sp = (uintptr_t)(proc->tf);
ffffffffc020340a:	02d9bc23          	sd	a3,56(s3)
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc020340e:	100027f3          	csrr	a5,sstatus
ffffffffc0203412:	8b89                	andi	a5,a5,2
    return 0;
ffffffffc0203414:	4901                	li	s2,0
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc0203416:	12079663          	bnez	a5,ffffffffc0203542 <do_fork+0x20e>
    if (++last_pid >= MAX_PID)
ffffffffc020341a:	00006817          	auipc	a6,0x6
ffffffffc020341e:	c0e80813          	addi	a6,a6,-1010 # ffffffffc0209028 <last_pid.1>
ffffffffc0203422:	00082783          	lw	a5,0(a6)
ffffffffc0203426:	6709                	lui	a4,0x2
ffffffffc0203428:	0017851b          	addiw	a0,a5,1
ffffffffc020342c:	00a82023          	sw	a0,0(a6)
ffffffffc0203430:	08e55163          	bge	a0,a4,ffffffffc02034b2 <do_fork+0x17e>
    if (last_pid >= next_safe)
ffffffffc0203434:	00006317          	auipc	t1,0x6
ffffffffc0203438:	bf830313          	addi	t1,t1,-1032 # ffffffffc020902c <next_safe.0>
ffffffffc020343c:	00032783          	lw	a5,0(t1)
ffffffffc0203440:	0000a417          	auipc	s0,0xa
ffffffffc0203444:	01840413          	addi	s0,s0,24 # ffffffffc020d458 <proc_list>
ffffffffc0203448:	06f55d63          	bge	a0,a5,ffffffffc02034c2 <do_fork+0x18e>
        proc->pid = get_pid();
ffffffffc020344c:	00a9a223          	sw	a0,4(s3)
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
ffffffffc0203450:	45a9                	li	a1,10
ffffffffc0203452:	2501                	sext.w	a0,a0
ffffffffc0203454:	566000ef          	jal	ra,ffffffffc02039ba <hash32>
ffffffffc0203458:	02051793          	slli	a5,a0,0x20
ffffffffc020345c:	01c7d513          	srli	a0,a5,0x1c
ffffffffc0203460:	00006797          	auipc	a5,0x6
ffffffffc0203464:	fe878793          	addi	a5,a5,-24 # ffffffffc0209448 <hash_list>
ffffffffc0203468:	953e                	add	a0,a0,a5
    __list_add(elm, listelm, listelm->next);
ffffffffc020346a:	6510                	ld	a2,8(a0)
ffffffffc020346c:	0d898793          	addi	a5,s3,216
ffffffffc0203470:	6414                	ld	a3,8(s0)
        nr_process++;
ffffffffc0203472:	4098                	lw	a4,0(s1)
    prev->next = next->prev = elm;
ffffffffc0203474:	e21c                	sd	a5,0(a2)
ffffffffc0203476:	e51c                	sd	a5,8(a0)
    elm->next = next;
ffffffffc0203478:	0ec9b023          	sd	a2,224(s3)
        list_add(&proc_list, &(proc->list_link));
ffffffffc020347c:	0c898793          	addi	a5,s3,200
    elm->prev = prev;
ffffffffc0203480:	0ca9bc23          	sd	a0,216(s3)
    prev->next = next->prev = elm;
ffffffffc0203484:	e29c                	sd	a5,0(a3)
        nr_process++;
ffffffffc0203486:	2705                	addiw	a4,a4,1
ffffffffc0203488:	e41c                	sd	a5,8(s0)
    elm->next = next;
ffffffffc020348a:	0cd9b823          	sd	a3,208(s3)
    elm->prev = prev;
ffffffffc020348e:	0c89b423          	sd	s0,200(s3)
ffffffffc0203492:	c098                	sw	a4,0(s1)
    if (flag) {
ffffffffc0203494:	0a091b63          	bnez	s2,ffffffffc020354a <do_fork+0x216>
    wakeup_proc(proc);
ffffffffc0203498:	854e                	mv	a0,s3
ffffffffc020349a:	45a000ef          	jal	ra,ffffffffc02038f4 <wakeup_proc>
    ret = proc->pid;
ffffffffc020349e:	0049a503          	lw	a0,4(s3)
}
ffffffffc02034a2:	70a2                	ld	ra,40(sp)
ffffffffc02034a4:	7402                	ld	s0,32(sp)
ffffffffc02034a6:	64e2                	ld	s1,24(sp)
ffffffffc02034a8:	6942                	ld	s2,16(sp)
ffffffffc02034aa:	69a2                	ld	s3,8(sp)
ffffffffc02034ac:	6a02                	ld	s4,0(sp)
ffffffffc02034ae:	6145                	addi	sp,sp,48
ffffffffc02034b0:	8082                	ret
        last_pid = 1;
ffffffffc02034b2:	4785                	li	a5,1
ffffffffc02034b4:	00f82023          	sw	a5,0(a6)
        goto inside;
ffffffffc02034b8:	4505                	li	a0,1
ffffffffc02034ba:	00006317          	auipc	t1,0x6
ffffffffc02034be:	b7230313          	addi	t1,t1,-1166 # ffffffffc020902c <next_safe.0>
    return listelm->next;
ffffffffc02034c2:	0000a417          	auipc	s0,0xa
ffffffffc02034c6:	f9640413          	addi	s0,s0,-106 # ffffffffc020d458 <proc_list>
ffffffffc02034ca:	00843e03          	ld	t3,8(s0)
        next_safe = MAX_PID;
ffffffffc02034ce:	6789                	lui	a5,0x2
ffffffffc02034d0:	00f32023          	sw	a5,0(t1)
ffffffffc02034d4:	86aa                	mv	a3,a0
ffffffffc02034d6:	4581                	li	a1,0
        while ((le = list_next(le)) != list)
ffffffffc02034d8:	6e89                	lui	t4,0x2
ffffffffc02034da:	088e0063          	beq	t3,s0,ffffffffc020355a <do_fork+0x226>
ffffffffc02034de:	88ae                	mv	a7,a1
ffffffffc02034e0:	87f2                	mv	a5,t3
ffffffffc02034e2:	6609                	lui	a2,0x2
ffffffffc02034e4:	a811                	j	ffffffffc02034f8 <do_fork+0x1c4>
            else if (proc->pid > last_pid && next_safe > proc->pid)
ffffffffc02034e6:	00e6d663          	bge	a3,a4,ffffffffc02034f2 <do_fork+0x1be>
ffffffffc02034ea:	00c75463          	bge	a4,a2,ffffffffc02034f2 <do_fork+0x1be>
ffffffffc02034ee:	863a                	mv	a2,a4
ffffffffc02034f0:	4885                	li	a7,1
ffffffffc02034f2:	679c                	ld	a5,8(a5)
        while ((le = list_next(le)) != list)
ffffffffc02034f4:	00878d63          	beq	a5,s0,ffffffffc020350e <do_fork+0x1da>
            if (proc->pid == last_pid)
ffffffffc02034f8:	f3c7a703          	lw	a4,-196(a5) # 1f3c <kern_entry-0xffffffffc01fe0c4>
ffffffffc02034fc:	fed715e3          	bne	a4,a3,ffffffffc02034e6 <do_fork+0x1b2>
                if (++last_pid >= next_safe)
ffffffffc0203500:	2685                	addiw	a3,a3,1
ffffffffc0203502:	04c6d763          	bge	a3,a2,ffffffffc0203550 <do_fork+0x21c>
ffffffffc0203506:	679c                	ld	a5,8(a5)
ffffffffc0203508:	4585                	li	a1,1
        while ((le = list_next(le)) != list)
ffffffffc020350a:	fe8797e3          	bne	a5,s0,ffffffffc02034f8 <do_fork+0x1c4>
ffffffffc020350e:	c581                	beqz	a1,ffffffffc0203516 <do_fork+0x1e2>
ffffffffc0203510:	00d82023          	sw	a3,0(a6)
ffffffffc0203514:	8536                	mv	a0,a3
ffffffffc0203516:	f2088be3          	beqz	a7,ffffffffc020344c <do_fork+0x118>
ffffffffc020351a:	00c32023          	sw	a2,0(t1)
ffffffffc020351e:	b73d                	j	ffffffffc020344c <do_fork+0x118>
    proc->tf->gpr.sp = (esp == 0) ? (uintptr_t)proc->tf : esp;
ffffffffc0203520:	8936                	mv	s2,a3
ffffffffc0203522:	0126b823          	sd	s2,16(a3)
    proc->context.ra = (uintptr_t)forkret;
ffffffffc0203526:	00000797          	auipc	a5,0x0
ffffffffc020352a:	d2078793          	addi	a5,a5,-736 # ffffffffc0203246 <forkret>
ffffffffc020352e:	02f9b823          	sd	a5,48(s3)
    proc->context.sp = (uintptr_t)(proc->tf);
ffffffffc0203532:	02d9bc23          	sd	a3,56(s3)
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc0203536:	100027f3          	csrr	a5,sstatus
ffffffffc020353a:	8b89                	andi	a5,a5,2
    return 0;
ffffffffc020353c:	4901                	li	s2,0
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc020353e:	ec078ee3          	beqz	a5,ffffffffc020341a <do_fork+0xe6>
        intr_disable();
ffffffffc0203542:	beefd0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        return 1;
ffffffffc0203546:	4905                	li	s2,1
ffffffffc0203548:	bdc9                	j	ffffffffc020341a <do_fork+0xe6>
        intr_enable();
ffffffffc020354a:	be0fd0ef          	jal	ra,ffffffffc020092a <intr_enable>
ffffffffc020354e:	b7a9                	j	ffffffffc0203498 <do_fork+0x164>
                    if (last_pid >= MAX_PID)
ffffffffc0203550:	01d6c363          	blt	a3,t4,ffffffffc0203556 <do_fork+0x222>
                        last_pid = 1;
ffffffffc0203554:	4685                	li	a3,1
                    goto repeat;
ffffffffc0203556:	4585                	li	a1,1
ffffffffc0203558:	b749                	j	ffffffffc02034da <do_fork+0x1a6>
ffffffffc020355a:	cd81                	beqz	a1,ffffffffc0203572 <do_fork+0x23e>
ffffffffc020355c:	00d82023          	sw	a3,0(a6)
    return last_pid;
ffffffffc0203560:	8536                	mv	a0,a3
ffffffffc0203562:	b5ed                	j	ffffffffc020344c <do_fork+0x118>
    kfree(proc);
ffffffffc0203564:	854e                	mv	a0,s3
ffffffffc0203566:	dd8fe0ef          	jal	ra,ffffffffc0201b3e <kfree>
    ret = -E_NO_MEM;
ffffffffc020356a:	5571                	li	a0,-4
    goto fork_out;
ffffffffc020356c:	bf1d                	j	ffffffffc02034a2 <do_fork+0x16e>
    int ret = -E_NO_FREE_PROC;
ffffffffc020356e:	556d                	li	a0,-5
ffffffffc0203570:	bf0d                	j	ffffffffc02034a2 <do_fork+0x16e>
    return last_pid;
ffffffffc0203572:	00082503          	lw	a0,0(a6)
ffffffffc0203576:	bdd9                	j	ffffffffc020344c <do_fork+0x118>
    ret = -E_NO_MEM;
ffffffffc0203578:	5571                	li	a0,-4
    return ret;
ffffffffc020357a:	b725                	j	ffffffffc02034a2 <do_fork+0x16e>
    assert(current->mm == NULL);
ffffffffc020357c:	00002697          	auipc	a3,0x2
ffffffffc0203580:	0dc68693          	addi	a3,a3,220 # ffffffffc0205658 <default_pmm_manager+0x978>
ffffffffc0203584:	00001617          	auipc	a2,0x1
ffffffffc0203588:	3ac60613          	addi	a2,a2,940 # ffffffffc0204930 <commands+0x818>
ffffffffc020358c:	12500593          	li	a1,293
ffffffffc0203590:	00002517          	auipc	a0,0x2
ffffffffc0203594:	0e050513          	addi	a0,a0,224 # ffffffffc0205670 <default_pmm_manager+0x990>
ffffffffc0203598:	ec3fc0ef          	jal	ra,ffffffffc020045a <__panic>
ffffffffc020359c:	00001617          	auipc	a2,0x1
ffffffffc02035a0:	77c60613          	addi	a2,a2,1916 # ffffffffc0204d18 <default_pmm_manager+0x38>
ffffffffc02035a4:	07100593          	li	a1,113
ffffffffc02035a8:	00001517          	auipc	a0,0x1
ffffffffc02035ac:	79850513          	addi	a0,a0,1944 # ffffffffc0204d40 <default_pmm_manager+0x60>
ffffffffc02035b0:	eabfc0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc02035b4 <kernel_thread>:
{
ffffffffc02035b4:	7129                	addi	sp,sp,-320
ffffffffc02035b6:	fa22                	sd	s0,304(sp)
ffffffffc02035b8:	f626                	sd	s1,296(sp)
ffffffffc02035ba:	f24a                	sd	s2,288(sp)
ffffffffc02035bc:	84ae                	mv	s1,a1
ffffffffc02035be:	892a                	mv	s2,a0
ffffffffc02035c0:	8432                	mv	s0,a2
    memset(&tf, 0, sizeof(struct trapframe));
ffffffffc02035c2:	4581                	li	a1,0
ffffffffc02035c4:	12000613          	li	a2,288
ffffffffc02035c8:	850a                	mv	a0,sp
{
ffffffffc02035ca:	fe06                	sd	ra,312(sp)
    memset(&tf, 0, sizeof(struct trapframe));
ffffffffc02035cc:	095000ef          	jal	ra,ffffffffc0203e60 <memset>
    tf.gpr.s0 = (uintptr_t)fn;
ffffffffc02035d0:	e0ca                	sd	s2,64(sp)
    tf.gpr.s1 = (uintptr_t)arg;
ffffffffc02035d2:	e4a6                	sd	s1,72(sp)
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE;
ffffffffc02035d4:	100027f3          	csrr	a5,sstatus
ffffffffc02035d8:	edd7f793          	andi	a5,a5,-291
ffffffffc02035dc:	1207e793          	ori	a5,a5,288
ffffffffc02035e0:	e23e                	sd	a5,256(sp)
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
ffffffffc02035e2:	860a                	mv	a2,sp
ffffffffc02035e4:	10046513          	ori	a0,s0,256
    tf.epc = (uintptr_t)kernel_thread_entry;
ffffffffc02035e8:	00000797          	auipc	a5,0x0
ffffffffc02035ec:	bf478793          	addi	a5,a5,-1036 # ffffffffc02031dc <kernel_thread_entry>
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
ffffffffc02035f0:	4581                	li	a1,0
    tf.epc = (uintptr_t)kernel_thread_entry;
ffffffffc02035f2:	e63e                	sd	a5,264(sp)
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
ffffffffc02035f4:	d41ff0ef          	jal	ra,ffffffffc0203334 <do_fork>
}
ffffffffc02035f8:	70f2                	ld	ra,312(sp)
ffffffffc02035fa:	7452                	ld	s0,304(sp)
ffffffffc02035fc:	74b2                	ld	s1,296(sp)
ffffffffc02035fe:	7912                	ld	s2,288(sp)
ffffffffc0203600:	6131                	addi	sp,sp,320
ffffffffc0203602:	8082                	ret

ffffffffc0203604 <do_exit>:
{
ffffffffc0203604:	1141                	addi	sp,sp,-16
    panic("process exit!!.\n");
ffffffffc0203606:	00002617          	auipc	a2,0x2
ffffffffc020360a:	08260613          	addi	a2,a2,130 # ffffffffc0205688 <default_pmm_manager+0x9a8>
ffffffffc020360e:	19800593          	li	a1,408
ffffffffc0203612:	00002517          	auipc	a0,0x2
ffffffffc0203616:	05e50513          	addi	a0,a0,94 # ffffffffc0205670 <default_pmm_manager+0x990>
{
ffffffffc020361a:	e406                	sd	ra,8(sp)
    panic("process exit!!.\n");
ffffffffc020361c:	e3ffc0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0203620 <proc_init>:

// proc_init - set up the first kernel thread idleproc "idle" by itself and
//           - create the second kernel thread init_main
void proc_init(void)
{
ffffffffc0203620:	7179                	addi	sp,sp,-48
ffffffffc0203622:	ec26                	sd	s1,24(sp)
    elm->prev = elm->next = elm;
ffffffffc0203624:	0000a797          	auipc	a5,0xa
ffffffffc0203628:	e3478793          	addi	a5,a5,-460 # ffffffffc020d458 <proc_list>
ffffffffc020362c:	f406                	sd	ra,40(sp)
ffffffffc020362e:	f022                	sd	s0,32(sp)
ffffffffc0203630:	e84a                	sd	s2,16(sp)
ffffffffc0203632:	e44e                	sd	s3,8(sp)
ffffffffc0203634:	00006497          	auipc	s1,0x6
ffffffffc0203638:	e1448493          	addi	s1,s1,-492 # ffffffffc0209448 <hash_list>
ffffffffc020363c:	e79c                	sd	a5,8(a5)
ffffffffc020363e:	e39c                	sd	a5,0(a5)
    int i;

    list_init(&proc_list);
    for (i = 0; i < HASH_LIST_SIZE; i++)
ffffffffc0203640:	0000a717          	auipc	a4,0xa
ffffffffc0203644:	e0870713          	addi	a4,a4,-504 # ffffffffc020d448 <name.2>
ffffffffc0203648:	87a6                	mv	a5,s1
ffffffffc020364a:	e79c                	sd	a5,8(a5)
ffffffffc020364c:	e39c                	sd	a5,0(a5)
ffffffffc020364e:	07c1                	addi	a5,a5,16
ffffffffc0203650:	fef71de3          	bne	a4,a5,ffffffffc020364a <proc_init+0x2a>
    {
        list_init(hash_list + i);
    }

    if ((idleproc = alloc_proc()) == NULL)
ffffffffc0203654:	b91ff0ef          	jal	ra,ffffffffc02031e4 <alloc_proc>
ffffffffc0203658:	0000a917          	auipc	s2,0xa
ffffffffc020365c:	e7890913          	addi	s2,s2,-392 # ffffffffc020d4d0 <idleproc>
ffffffffc0203660:	00a93023          	sd	a0,0(s2)
ffffffffc0203664:	18050d63          	beqz	a0,ffffffffc02037fe <proc_init+0x1de>
    {
        panic("cannot alloc idleproc.\n");
    }

    // check the proc structure
    int *context_mem = (int *)kmalloc(sizeof(struct context));
ffffffffc0203668:	07000513          	li	a0,112
ffffffffc020366c:	c22fe0ef          	jal	ra,ffffffffc0201a8e <kmalloc>
    memset(context_mem, 0, sizeof(struct context));
ffffffffc0203670:	07000613          	li	a2,112
ffffffffc0203674:	4581                	li	a1,0
    int *context_mem = (int *)kmalloc(sizeof(struct context));
ffffffffc0203676:	842a                	mv	s0,a0
    memset(context_mem, 0, sizeof(struct context));
ffffffffc0203678:	7e8000ef          	jal	ra,ffffffffc0203e60 <memset>
    int context_init_flag = memcmp(&(idleproc->context), context_mem, sizeof(struct context));
ffffffffc020367c:	00093503          	ld	a0,0(s2)
ffffffffc0203680:	85a2                	mv	a1,s0
ffffffffc0203682:	07000613          	li	a2,112
ffffffffc0203686:	03050513          	addi	a0,a0,48
ffffffffc020368a:	001000ef          	jal	ra,ffffffffc0203e8a <memcmp>
ffffffffc020368e:	89aa                	mv	s3,a0

    int *proc_name_mem = (int *)kmalloc(PROC_NAME_LEN);
ffffffffc0203690:	453d                	li	a0,15
ffffffffc0203692:	bfcfe0ef          	jal	ra,ffffffffc0201a8e <kmalloc>
    memset(proc_name_mem, 0, PROC_NAME_LEN);
ffffffffc0203696:	463d                	li	a2,15
ffffffffc0203698:	4581                	li	a1,0
    int *proc_name_mem = (int *)kmalloc(PROC_NAME_LEN);
ffffffffc020369a:	842a                	mv	s0,a0
    memset(proc_name_mem, 0, PROC_NAME_LEN);
ffffffffc020369c:	7c4000ef          	jal	ra,ffffffffc0203e60 <memset>
    int proc_name_flag = memcmp(&(idleproc->name), proc_name_mem, PROC_NAME_LEN);
ffffffffc02036a0:	00093503          	ld	a0,0(s2)
ffffffffc02036a4:	463d                	li	a2,15
ffffffffc02036a6:	85a2                	mv	a1,s0
ffffffffc02036a8:	0b450513          	addi	a0,a0,180
ffffffffc02036ac:	7de000ef          	jal	ra,ffffffffc0203e8a <memcmp>

    if (idleproc->pgdir == boot_pgdir_pa && idleproc->tf == NULL && !context_init_flag && idleproc->state == PROC_UNINIT && idleproc->pid == -1 && idleproc->runs == 0 && idleproc->kstack == 0 && idleproc->need_resched == 0 && idleproc->parent == NULL && idleproc->mm == NULL && idleproc->flags == 0 && !proc_name_flag)
ffffffffc02036b0:	00093783          	ld	a5,0(s2)
ffffffffc02036b4:	0000a717          	auipc	a4,0xa
ffffffffc02036b8:	de473703          	ld	a4,-540(a4) # ffffffffc020d498 <boot_pgdir_pa>
ffffffffc02036bc:	77d4                	ld	a3,168(a5)
ffffffffc02036be:	0ee68463          	beq	a3,a4,ffffffffc02037a6 <proc_init+0x186>
    {
        cprintf("alloc_proc() correct!\n");
    }

    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
ffffffffc02036c2:	4709                	li	a4,2
ffffffffc02036c4:	e398                	sd	a4,0(a5)
    idleproc->kstack = (uintptr_t)bootstack;
ffffffffc02036c6:	00003717          	auipc	a4,0x3
ffffffffc02036ca:	93a70713          	addi	a4,a4,-1734 # ffffffffc0206000 <bootstack>
    memset(proc->name, 0, sizeof(proc->name));
ffffffffc02036ce:	0b478413          	addi	s0,a5,180
    idleproc->kstack = (uintptr_t)bootstack;
ffffffffc02036d2:	eb98                	sd	a4,16(a5)
    idleproc->need_resched = 1;
ffffffffc02036d4:	4705                	li	a4,1
ffffffffc02036d6:	cf98                	sw	a4,24(a5)
    memset(proc->name, 0, sizeof(proc->name));
ffffffffc02036d8:	4641                	li	a2,16
ffffffffc02036da:	4581                	li	a1,0
ffffffffc02036dc:	8522                	mv	a0,s0
ffffffffc02036de:	782000ef          	jal	ra,ffffffffc0203e60 <memset>
    return memcpy(proc->name, name, PROC_NAME_LEN);
ffffffffc02036e2:	463d                	li	a2,15
ffffffffc02036e4:	00002597          	auipc	a1,0x2
ffffffffc02036e8:	fec58593          	addi	a1,a1,-20 # ffffffffc02056d0 <default_pmm_manager+0x9f0>
ffffffffc02036ec:	8522                	mv	a0,s0
ffffffffc02036ee:	784000ef          	jal	ra,ffffffffc0203e72 <memcpy>
    set_proc_name(idleproc, "idle");
    nr_process++;
ffffffffc02036f2:	0000a717          	auipc	a4,0xa
ffffffffc02036f6:	dee70713          	addi	a4,a4,-530 # ffffffffc020d4e0 <nr_process>
ffffffffc02036fa:	431c                	lw	a5,0(a4)

    current = idleproc;
ffffffffc02036fc:	00093683          	ld	a3,0(s2)

    int pid = kernel_thread(init_main, "Hello world!!", 0);
ffffffffc0203700:	4601                	li	a2,0
    nr_process++;
ffffffffc0203702:	2785                	addiw	a5,a5,1
    int pid = kernel_thread(init_main, "Hello world!!", 0);
ffffffffc0203704:	00002597          	auipc	a1,0x2
ffffffffc0203708:	fd458593          	addi	a1,a1,-44 # ffffffffc02056d8 <default_pmm_manager+0x9f8>
ffffffffc020370c:	00000517          	auipc	a0,0x0
ffffffffc0203710:	b4850513          	addi	a0,a0,-1208 # ffffffffc0203254 <init_main>
    nr_process++;
ffffffffc0203714:	c31c                	sw	a5,0(a4)
    current = idleproc;
ffffffffc0203716:	0000a797          	auipc	a5,0xa
ffffffffc020371a:	dad7b923          	sd	a3,-590(a5) # ffffffffc020d4c8 <current>
    int pid = kernel_thread(init_main, "Hello world!!", 0);
ffffffffc020371e:	e97ff0ef          	jal	ra,ffffffffc02035b4 <kernel_thread>
ffffffffc0203722:	842a                	mv	s0,a0
    if (pid <= 0)
ffffffffc0203724:	0ea05963          	blez	a0,ffffffffc0203816 <proc_init+0x1f6>
    if (0 < pid && pid < MAX_PID)
ffffffffc0203728:	6789                	lui	a5,0x2
ffffffffc020372a:	fff5071b          	addiw	a4,a0,-1
ffffffffc020372e:	17f9                	addi	a5,a5,-2
ffffffffc0203730:	2501                	sext.w	a0,a0
ffffffffc0203732:	02e7e363          	bltu	a5,a4,ffffffffc0203758 <proc_init+0x138>
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
ffffffffc0203736:	45a9                	li	a1,10
ffffffffc0203738:	282000ef          	jal	ra,ffffffffc02039ba <hash32>
ffffffffc020373c:	02051793          	slli	a5,a0,0x20
ffffffffc0203740:	01c7d693          	srli	a3,a5,0x1c
ffffffffc0203744:	96a6                	add	a3,a3,s1
ffffffffc0203746:	87b6                	mv	a5,a3
        while ((le = list_next(le)) != list)
ffffffffc0203748:	a029                	j	ffffffffc0203752 <proc_init+0x132>
            if (proc->pid == pid)
ffffffffc020374a:	f2c7a703          	lw	a4,-212(a5) # 1f2c <kern_entry-0xffffffffc01fe0d4>
ffffffffc020374e:	0a870563          	beq	a4,s0,ffffffffc02037f8 <proc_init+0x1d8>
    return listelm->next;
ffffffffc0203752:	679c                	ld	a5,8(a5)
        while ((le = list_next(le)) != list)
ffffffffc0203754:	fef69be3          	bne	a3,a5,ffffffffc020374a <proc_init+0x12a>
    return NULL;
ffffffffc0203758:	4781                	li	a5,0
    memset(proc->name, 0, sizeof(proc->name));
ffffffffc020375a:	0b478493          	addi	s1,a5,180
ffffffffc020375e:	4641                	li	a2,16
ffffffffc0203760:	4581                	li	a1,0
    {
        panic("create init_main failed.\n");
    }

    initproc = find_proc(pid);
ffffffffc0203762:	0000a417          	auipc	s0,0xa
ffffffffc0203766:	d7640413          	addi	s0,s0,-650 # ffffffffc020d4d8 <initproc>
    memset(proc->name, 0, sizeof(proc->name));
ffffffffc020376a:	8526                	mv	a0,s1
    initproc = find_proc(pid);
ffffffffc020376c:	e01c                	sd	a5,0(s0)
    memset(proc->name, 0, sizeof(proc->name));
ffffffffc020376e:	6f2000ef          	jal	ra,ffffffffc0203e60 <memset>
    return memcpy(proc->name, name, PROC_NAME_LEN);
ffffffffc0203772:	463d                	li	a2,15
ffffffffc0203774:	00002597          	auipc	a1,0x2
ffffffffc0203778:	f9458593          	addi	a1,a1,-108 # ffffffffc0205708 <default_pmm_manager+0xa28>
ffffffffc020377c:	8526                	mv	a0,s1
ffffffffc020377e:	6f4000ef          	jal	ra,ffffffffc0203e72 <memcpy>
    set_proc_name(initproc, "init");

    assert(idleproc != NULL && idleproc->pid == 0);
ffffffffc0203782:	00093783          	ld	a5,0(s2)
ffffffffc0203786:	c7e1                	beqz	a5,ffffffffc020384e <proc_init+0x22e>
ffffffffc0203788:	43dc                	lw	a5,4(a5)
ffffffffc020378a:	e3f1                	bnez	a5,ffffffffc020384e <proc_init+0x22e>
    assert(initproc != NULL && initproc->pid == 1);
ffffffffc020378c:	601c                	ld	a5,0(s0)
ffffffffc020378e:	c3c5                	beqz	a5,ffffffffc020382e <proc_init+0x20e>
ffffffffc0203790:	43d8                	lw	a4,4(a5)
ffffffffc0203792:	4785                	li	a5,1
ffffffffc0203794:	08f71d63          	bne	a4,a5,ffffffffc020382e <proc_init+0x20e>
}
ffffffffc0203798:	70a2                	ld	ra,40(sp)
ffffffffc020379a:	7402                	ld	s0,32(sp)
ffffffffc020379c:	64e2                	ld	s1,24(sp)
ffffffffc020379e:	6942                	ld	s2,16(sp)
ffffffffc02037a0:	69a2                	ld	s3,8(sp)
ffffffffc02037a2:	6145                	addi	sp,sp,48
ffffffffc02037a4:	8082                	ret
    if (idleproc->pgdir == boot_pgdir_pa && idleproc->tf == NULL && !context_init_flag && idleproc->state == PROC_UNINIT && idleproc->pid == -1 && idleproc->runs == 0 && idleproc->kstack == 0 && idleproc->need_resched == 0 && idleproc->parent == NULL && idleproc->mm == NULL && idleproc->flags == 0 && !proc_name_flag)
ffffffffc02037a6:	73d8                	ld	a4,160(a5)
ffffffffc02037a8:	ff09                	bnez	a4,ffffffffc02036c2 <proc_init+0xa2>
ffffffffc02037aa:	f0099ce3          	bnez	s3,ffffffffc02036c2 <proc_init+0xa2>
ffffffffc02037ae:	6394                	ld	a3,0(a5)
ffffffffc02037b0:	577d                	li	a4,-1
ffffffffc02037b2:	1702                	slli	a4,a4,0x20
ffffffffc02037b4:	f0e697e3          	bne	a3,a4,ffffffffc02036c2 <proc_init+0xa2>
ffffffffc02037b8:	4798                	lw	a4,8(a5)
ffffffffc02037ba:	f00714e3          	bnez	a4,ffffffffc02036c2 <proc_init+0xa2>
ffffffffc02037be:	6b98                	ld	a4,16(a5)
ffffffffc02037c0:	f00711e3          	bnez	a4,ffffffffc02036c2 <proc_init+0xa2>
ffffffffc02037c4:	4f98                	lw	a4,24(a5)
ffffffffc02037c6:	2701                	sext.w	a4,a4
ffffffffc02037c8:	ee071de3          	bnez	a4,ffffffffc02036c2 <proc_init+0xa2>
ffffffffc02037cc:	7398                	ld	a4,32(a5)
ffffffffc02037ce:	ee071ae3          	bnez	a4,ffffffffc02036c2 <proc_init+0xa2>
ffffffffc02037d2:	7798                	ld	a4,40(a5)
ffffffffc02037d4:	ee0717e3          	bnez	a4,ffffffffc02036c2 <proc_init+0xa2>
ffffffffc02037d8:	0b07a703          	lw	a4,176(a5)
ffffffffc02037dc:	8d59                	or	a0,a0,a4
ffffffffc02037de:	0005071b          	sext.w	a4,a0
ffffffffc02037e2:	ee0710e3          	bnez	a4,ffffffffc02036c2 <proc_init+0xa2>
        cprintf("alloc_proc() correct!\n");
ffffffffc02037e6:	00002517          	auipc	a0,0x2
ffffffffc02037ea:	ed250513          	addi	a0,a0,-302 # ffffffffc02056b8 <default_pmm_manager+0x9d8>
ffffffffc02037ee:	9a7fc0ef          	jal	ra,ffffffffc0200194 <cprintf>
    idleproc->pid = 0;
ffffffffc02037f2:	00093783          	ld	a5,0(s2)
ffffffffc02037f6:	b5f1                	j	ffffffffc02036c2 <proc_init+0xa2>
            struct proc_struct *proc = le2proc(le, hash_link);
ffffffffc02037f8:	f2878793          	addi	a5,a5,-216
ffffffffc02037fc:	bfb9                	j	ffffffffc020375a <proc_init+0x13a>
        panic("cannot alloc idleproc.\n");
ffffffffc02037fe:	00002617          	auipc	a2,0x2
ffffffffc0203802:	ea260613          	addi	a2,a2,-350 # ffffffffc02056a0 <default_pmm_manager+0x9c0>
ffffffffc0203806:	1b300593          	li	a1,435
ffffffffc020380a:	00002517          	auipc	a0,0x2
ffffffffc020380e:	e6650513          	addi	a0,a0,-410 # ffffffffc0205670 <default_pmm_manager+0x990>
ffffffffc0203812:	c49fc0ef          	jal	ra,ffffffffc020045a <__panic>
        panic("create init_main failed.\n");
ffffffffc0203816:	00002617          	auipc	a2,0x2
ffffffffc020381a:	ed260613          	addi	a2,a2,-302 # ffffffffc02056e8 <default_pmm_manager+0xa08>
ffffffffc020381e:	1d000593          	li	a1,464
ffffffffc0203822:	00002517          	auipc	a0,0x2
ffffffffc0203826:	e4e50513          	addi	a0,a0,-434 # ffffffffc0205670 <default_pmm_manager+0x990>
ffffffffc020382a:	c31fc0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(initproc != NULL && initproc->pid == 1);
ffffffffc020382e:	00002697          	auipc	a3,0x2
ffffffffc0203832:	f0a68693          	addi	a3,a3,-246 # ffffffffc0205738 <default_pmm_manager+0xa58>
ffffffffc0203836:	00001617          	auipc	a2,0x1
ffffffffc020383a:	0fa60613          	addi	a2,a2,250 # ffffffffc0204930 <commands+0x818>
ffffffffc020383e:	1d700593          	li	a1,471
ffffffffc0203842:	00002517          	auipc	a0,0x2
ffffffffc0203846:	e2e50513          	addi	a0,a0,-466 # ffffffffc0205670 <default_pmm_manager+0x990>
ffffffffc020384a:	c11fc0ef          	jal	ra,ffffffffc020045a <__panic>
    assert(idleproc != NULL && idleproc->pid == 0);
ffffffffc020384e:	00002697          	auipc	a3,0x2
ffffffffc0203852:	ec268693          	addi	a3,a3,-318 # ffffffffc0205710 <default_pmm_manager+0xa30>
ffffffffc0203856:	00001617          	auipc	a2,0x1
ffffffffc020385a:	0da60613          	addi	a2,a2,218 # ffffffffc0204930 <commands+0x818>
ffffffffc020385e:	1d600593          	li	a1,470
ffffffffc0203862:	00002517          	auipc	a0,0x2
ffffffffc0203866:	e0e50513          	addi	a0,a0,-498 # ffffffffc0205670 <default_pmm_manager+0x990>
ffffffffc020386a:	bf1fc0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc020386e <cpu_idle>:

// cpu_idle - at the end of kern_init, the first kernel thread idleproc will do below works
void cpu_idle(void)
{
ffffffffc020386e:	1141                	addi	sp,sp,-16
ffffffffc0203870:	e022                	sd	s0,0(sp)
ffffffffc0203872:	e406                	sd	ra,8(sp)
ffffffffc0203874:	0000a417          	auipc	s0,0xa
ffffffffc0203878:	c5440413          	addi	s0,s0,-940 # ffffffffc020d4c8 <current>
    while (1)
    {
        if (current->need_resched)
ffffffffc020387c:	6018                	ld	a4,0(s0)
ffffffffc020387e:	4f1c                	lw	a5,24(a4)
ffffffffc0203880:	2781                	sext.w	a5,a5
ffffffffc0203882:	dff5                	beqz	a5,ffffffffc020387e <cpu_idle+0x10>
        {
            schedule();
ffffffffc0203884:	0a2000ef          	jal	ra,ffffffffc0203926 <schedule>
ffffffffc0203888:	bfd5                	j	ffffffffc020387c <cpu_idle+0xe>

ffffffffc020388a <switch_to>:
.text
# void switch_to(struct proc_struct* from, struct proc_struct* to)
.globl switch_to
switch_to:
    # save from's registers
    STORE ra, 0*REGBYTES(a0)
ffffffffc020388a:	00153023          	sd	ra,0(a0)
    STORE sp, 1*REGBYTES(a0)
ffffffffc020388e:	00253423          	sd	sp,8(a0)
    STORE s0, 2*REGBYTES(a0)
ffffffffc0203892:	e900                	sd	s0,16(a0)
    STORE s1, 3*REGBYTES(a0)
ffffffffc0203894:	ed04                	sd	s1,24(a0)
    STORE s2, 4*REGBYTES(a0)
ffffffffc0203896:	03253023          	sd	s2,32(a0)
    STORE s3, 5*REGBYTES(a0)
ffffffffc020389a:	03353423          	sd	s3,40(a0)
    STORE s4, 6*REGBYTES(a0)
ffffffffc020389e:	03453823          	sd	s4,48(a0)
    STORE s5, 7*REGBYTES(a0)
ffffffffc02038a2:	03553c23          	sd	s5,56(a0)
    STORE s6, 8*REGBYTES(a0)
ffffffffc02038a6:	05653023          	sd	s6,64(a0)
    STORE s7, 9*REGBYTES(a0)
ffffffffc02038aa:	05753423          	sd	s7,72(a0)
    STORE s8, 10*REGBYTES(a0)
ffffffffc02038ae:	05853823          	sd	s8,80(a0)
    STORE s9, 11*REGBYTES(a0)
ffffffffc02038b2:	05953c23          	sd	s9,88(a0)
    STORE s10, 12*REGBYTES(a0)
ffffffffc02038b6:	07a53023          	sd	s10,96(a0)
    STORE s11, 13*REGBYTES(a0)
ffffffffc02038ba:	07b53423          	sd	s11,104(a0)

    # restore to's registers
    LOAD ra, 0*REGBYTES(a1)
ffffffffc02038be:	0005b083          	ld	ra,0(a1)
    LOAD sp, 1*REGBYTES(a1)
ffffffffc02038c2:	0085b103          	ld	sp,8(a1)
    LOAD s0, 2*REGBYTES(a1)
ffffffffc02038c6:	6980                	ld	s0,16(a1)
    LOAD s1, 3*REGBYTES(a1)
ffffffffc02038c8:	6d84                	ld	s1,24(a1)
    LOAD s2, 4*REGBYTES(a1)
ffffffffc02038ca:	0205b903          	ld	s2,32(a1)
    LOAD s3, 5*REGBYTES(a1)
ffffffffc02038ce:	0285b983          	ld	s3,40(a1)
    LOAD s4, 6*REGBYTES(a1)
ffffffffc02038d2:	0305ba03          	ld	s4,48(a1)
    LOAD s5, 7*REGBYTES(a1)
ffffffffc02038d6:	0385ba83          	ld	s5,56(a1)
    LOAD s6, 8*REGBYTES(a1)
ffffffffc02038da:	0405bb03          	ld	s6,64(a1)
    LOAD s7, 9*REGBYTES(a1)
ffffffffc02038de:	0485bb83          	ld	s7,72(a1)
    LOAD s8, 10*REGBYTES(a1)
ffffffffc02038e2:	0505bc03          	ld	s8,80(a1)
    LOAD s9, 11*REGBYTES(a1)
ffffffffc02038e6:	0585bc83          	ld	s9,88(a1)
    LOAD s10, 12*REGBYTES(a1)
ffffffffc02038ea:	0605bd03          	ld	s10,96(a1)
    LOAD s11, 13*REGBYTES(a1)
ffffffffc02038ee:	0685bd83          	ld	s11,104(a1)

    ret
ffffffffc02038f2:	8082                	ret

ffffffffc02038f4 <wakeup_proc>:
#include <sched.h>
#include <assert.h>

void
wakeup_proc(struct proc_struct *proc) {
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
ffffffffc02038f4:	411c                	lw	a5,0(a0)
ffffffffc02038f6:	4705                	li	a4,1
ffffffffc02038f8:	37f9                	addiw	a5,a5,-2
ffffffffc02038fa:	00f77563          	bgeu	a4,a5,ffffffffc0203904 <wakeup_proc+0x10>
    proc->state = PROC_RUNNABLE;
ffffffffc02038fe:	4789                	li	a5,2
ffffffffc0203900:	c11c                	sw	a5,0(a0)
ffffffffc0203902:	8082                	ret
wakeup_proc(struct proc_struct *proc) {
ffffffffc0203904:	1141                	addi	sp,sp,-16
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
ffffffffc0203906:	00002697          	auipc	a3,0x2
ffffffffc020390a:	e5a68693          	addi	a3,a3,-422 # ffffffffc0205760 <default_pmm_manager+0xa80>
ffffffffc020390e:	00001617          	auipc	a2,0x1
ffffffffc0203912:	02260613          	addi	a2,a2,34 # ffffffffc0204930 <commands+0x818>
ffffffffc0203916:	45a5                	li	a1,9
ffffffffc0203918:	00002517          	auipc	a0,0x2
ffffffffc020391c:	e8850513          	addi	a0,a0,-376 # ffffffffc02057a0 <default_pmm_manager+0xac0>
wakeup_proc(struct proc_struct *proc) {
ffffffffc0203920:	e406                	sd	ra,8(sp)
    assert(proc->state != PROC_ZOMBIE && proc->state != PROC_RUNNABLE);
ffffffffc0203922:	b39fc0ef          	jal	ra,ffffffffc020045a <__panic>

ffffffffc0203926 <schedule>:
}

void
schedule(void) {
ffffffffc0203926:	1141                	addi	sp,sp,-16
ffffffffc0203928:	e406                	sd	ra,8(sp)
ffffffffc020392a:	e022                	sd	s0,0(sp)
    if (read_csr(sstatus) & SSTATUS_SIE) {
ffffffffc020392c:	100027f3          	csrr	a5,sstatus
ffffffffc0203930:	8b89                	andi	a5,a5,2
ffffffffc0203932:	4401                	li	s0,0
ffffffffc0203934:	efbd                	bnez	a5,ffffffffc02039b2 <schedule+0x8c>
    bool intr_flag;
    list_entry_t *le, *last;
    struct proc_struct *next = NULL;
    local_intr_save(intr_flag);
    {
        current->need_resched = 0;
ffffffffc0203936:	0000a897          	auipc	a7,0xa
ffffffffc020393a:	b928b883          	ld	a7,-1134(a7) # ffffffffc020d4c8 <current>
ffffffffc020393e:	0008ac23          	sw	zero,24(a7)
        last = (current == idleproc) ? &proc_list : &(current->list_link);
ffffffffc0203942:	0000a517          	auipc	a0,0xa
ffffffffc0203946:	b8e53503          	ld	a0,-1138(a0) # ffffffffc020d4d0 <idleproc>
ffffffffc020394a:	04a88e63          	beq	a7,a0,ffffffffc02039a6 <schedule+0x80>
ffffffffc020394e:	0c888693          	addi	a3,a7,200
ffffffffc0203952:	0000a617          	auipc	a2,0xa
ffffffffc0203956:	b0660613          	addi	a2,a2,-1274 # ffffffffc020d458 <proc_list>
        le = last;
ffffffffc020395a:	87b6                	mv	a5,a3
    struct proc_struct *next = NULL;
ffffffffc020395c:	4581                	li	a1,0
        do {
            if ((le = list_next(le)) != &proc_list) {
                next = le2proc(le, list_link);
                if (next->state == PROC_RUNNABLE) {
ffffffffc020395e:	4809                	li	a6,2
ffffffffc0203960:	679c                	ld	a5,8(a5)
            if ((le = list_next(le)) != &proc_list) {
ffffffffc0203962:	00c78863          	beq	a5,a2,ffffffffc0203972 <schedule+0x4c>
                if (next->state == PROC_RUNNABLE) {
ffffffffc0203966:	f387a703          	lw	a4,-200(a5)
                next = le2proc(le, list_link);
ffffffffc020396a:	f3878593          	addi	a1,a5,-200
                if (next->state == PROC_RUNNABLE) {
ffffffffc020396e:	03070163          	beq	a4,a6,ffffffffc0203990 <schedule+0x6a>
                    break;
                }
            }
        } while (le != last);
ffffffffc0203972:	fef697e3          	bne	a3,a5,ffffffffc0203960 <schedule+0x3a>
        if (next == NULL || next->state != PROC_RUNNABLE) {
ffffffffc0203976:	ed89                	bnez	a1,ffffffffc0203990 <schedule+0x6a>
            next = idleproc;
        }
        next->runs ++;
ffffffffc0203978:	451c                	lw	a5,8(a0)
ffffffffc020397a:	2785                	addiw	a5,a5,1
ffffffffc020397c:	c51c                	sw	a5,8(a0)
        if (next != current) {
ffffffffc020397e:	00a88463          	beq	a7,a0,ffffffffc0203986 <schedule+0x60>
            proc_run(next);
ffffffffc0203982:	945ff0ef          	jal	ra,ffffffffc02032c6 <proc_run>
    if (flag) {
ffffffffc0203986:	e819                	bnez	s0,ffffffffc020399c <schedule+0x76>
        }
    }
    local_intr_restore(intr_flag);
}
ffffffffc0203988:	60a2                	ld	ra,8(sp)
ffffffffc020398a:	6402                	ld	s0,0(sp)
ffffffffc020398c:	0141                	addi	sp,sp,16
ffffffffc020398e:	8082                	ret
        if (next == NULL || next->state != PROC_RUNNABLE) {
ffffffffc0203990:	4198                	lw	a4,0(a1)
ffffffffc0203992:	4789                	li	a5,2
ffffffffc0203994:	fef712e3          	bne	a4,a5,ffffffffc0203978 <schedule+0x52>
ffffffffc0203998:	852e                	mv	a0,a1
ffffffffc020399a:	bff9                	j	ffffffffc0203978 <schedule+0x52>
}
ffffffffc020399c:	6402                	ld	s0,0(sp)
ffffffffc020399e:	60a2                	ld	ra,8(sp)
ffffffffc02039a0:	0141                	addi	sp,sp,16
        intr_enable();
ffffffffc02039a2:	f89fc06f          	j	ffffffffc020092a <intr_enable>
        last = (current == idleproc) ? &proc_list : &(current->list_link);
ffffffffc02039a6:	0000a617          	auipc	a2,0xa
ffffffffc02039aa:	ab260613          	addi	a2,a2,-1358 # ffffffffc020d458 <proc_list>
ffffffffc02039ae:	86b2                	mv	a3,a2
ffffffffc02039b0:	b76d                	j	ffffffffc020395a <schedule+0x34>
        intr_disable();
ffffffffc02039b2:	f7ffc0ef          	jal	ra,ffffffffc0200930 <intr_disable>
        return 1;
ffffffffc02039b6:	4405                	li	s0,1
ffffffffc02039b8:	bfbd                	j	ffffffffc0203936 <schedule+0x10>

ffffffffc02039ba <hash32>:
 *
 * High bits are more random, so we use them.
 * */
uint32_t
hash32(uint32_t val, unsigned int bits) {
    uint32_t hash = val * GOLDEN_RATIO_PRIME_32;
ffffffffc02039ba:	9e3707b7          	lui	a5,0x9e370
ffffffffc02039be:	2785                	addiw	a5,a5,1
ffffffffc02039c0:	02a7853b          	mulw	a0,a5,a0
    return (hash >> (32 - bits));
ffffffffc02039c4:	02000793          	li	a5,32
ffffffffc02039c8:	9f8d                	subw	a5,a5,a1
}
ffffffffc02039ca:	00f5553b          	srlw	a0,a0,a5
ffffffffc02039ce:	8082                	ret

ffffffffc02039d0 <printnum>:
 * */
static void
printnum(void (*putch)(int, void*), void *putdat,
        unsigned long long num, unsigned base, int width, int padc) {
    unsigned long long result = num;
    unsigned mod = do_div(result, base);
ffffffffc02039d0:	02069813          	slli	a6,a3,0x20
        unsigned long long num, unsigned base, int width, int padc) {
ffffffffc02039d4:	7179                	addi	sp,sp,-48
    unsigned mod = do_div(result, base);
ffffffffc02039d6:	02085813          	srli	a6,a6,0x20
        unsigned long long num, unsigned base, int width, int padc) {
ffffffffc02039da:	e052                	sd	s4,0(sp)
    unsigned mod = do_div(result, base);
ffffffffc02039dc:	03067a33          	remu	s4,a2,a6
        unsigned long long num, unsigned base, int width, int padc) {
ffffffffc02039e0:	f022                	sd	s0,32(sp)
ffffffffc02039e2:	ec26                	sd	s1,24(sp)
ffffffffc02039e4:	e84a                	sd	s2,16(sp)
ffffffffc02039e6:	f406                	sd	ra,40(sp)
ffffffffc02039e8:	e44e                	sd	s3,8(sp)
ffffffffc02039ea:	84aa                	mv	s1,a0
ffffffffc02039ec:	892e                	mv	s2,a1
    // first recursively print all preceding (more significant) digits
    if (num >= base) {
        printnum(putch, putdat, result, base, width - 1, padc);
    } else {
        // print any needed pad characters before first digit
        while (-- width > 0)
ffffffffc02039ee:	fff7041b          	addiw	s0,a4,-1
    unsigned mod = do_div(result, base);
ffffffffc02039f2:	2a01                	sext.w	s4,s4
    if (num >= base) {
ffffffffc02039f4:	03067e63          	bgeu	a2,a6,ffffffffc0203a30 <printnum+0x60>
ffffffffc02039f8:	89be                	mv	s3,a5
        while (-- width > 0)
ffffffffc02039fa:	00805763          	blez	s0,ffffffffc0203a08 <printnum+0x38>
ffffffffc02039fe:	347d                	addiw	s0,s0,-1
            putch(padc, putdat);
ffffffffc0203a00:	85ca                	mv	a1,s2
ffffffffc0203a02:	854e                	mv	a0,s3
ffffffffc0203a04:	9482                	jalr	s1
        while (-- width > 0)
ffffffffc0203a06:	fc65                	bnez	s0,ffffffffc02039fe <printnum+0x2e>
    }
    // then print this (the least significant) digit
    putch("0123456789abcdef"[mod], putdat);
ffffffffc0203a08:	1a02                	slli	s4,s4,0x20
ffffffffc0203a0a:	00002797          	auipc	a5,0x2
ffffffffc0203a0e:	dae78793          	addi	a5,a5,-594 # ffffffffc02057b8 <default_pmm_manager+0xad8>
ffffffffc0203a12:	020a5a13          	srli	s4,s4,0x20
ffffffffc0203a16:	9a3e                	add	s4,s4,a5
}
ffffffffc0203a18:	7402                	ld	s0,32(sp)
    putch("0123456789abcdef"[mod], putdat);
ffffffffc0203a1a:	000a4503          	lbu	a0,0(s4)
}
ffffffffc0203a1e:	70a2                	ld	ra,40(sp)
ffffffffc0203a20:	69a2                	ld	s3,8(sp)
ffffffffc0203a22:	6a02                	ld	s4,0(sp)
    putch("0123456789abcdef"[mod], putdat);
ffffffffc0203a24:	85ca                	mv	a1,s2
ffffffffc0203a26:	87a6                	mv	a5,s1
}
ffffffffc0203a28:	6942                	ld	s2,16(sp)
ffffffffc0203a2a:	64e2                	ld	s1,24(sp)
ffffffffc0203a2c:	6145                	addi	sp,sp,48
    putch("0123456789abcdef"[mod], putdat);
ffffffffc0203a2e:	8782                	jr	a5
        printnum(putch, putdat, result, base, width - 1, padc);
ffffffffc0203a30:	03065633          	divu	a2,a2,a6
ffffffffc0203a34:	8722                	mv	a4,s0
ffffffffc0203a36:	f9bff0ef          	jal	ra,ffffffffc02039d0 <printnum>
ffffffffc0203a3a:	b7f9                	j	ffffffffc0203a08 <printnum+0x38>

ffffffffc0203a3c <vprintfmt>:
 *
 * Call this function if you are already dealing with a va_list.
 * Or you probably want printfmt() instead.
 * */
void
vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list ap) {
ffffffffc0203a3c:	7119                	addi	sp,sp,-128
ffffffffc0203a3e:	f4a6                	sd	s1,104(sp)
ffffffffc0203a40:	f0ca                	sd	s2,96(sp)
ffffffffc0203a42:	ecce                	sd	s3,88(sp)
ffffffffc0203a44:	e8d2                	sd	s4,80(sp)
ffffffffc0203a46:	e4d6                	sd	s5,72(sp)
ffffffffc0203a48:	e0da                	sd	s6,64(sp)
ffffffffc0203a4a:	fc5e                	sd	s7,56(sp)
ffffffffc0203a4c:	f06a                	sd	s10,32(sp)
ffffffffc0203a4e:	fc86                	sd	ra,120(sp)
ffffffffc0203a50:	f8a2                	sd	s0,112(sp)
ffffffffc0203a52:	f862                	sd	s8,48(sp)
ffffffffc0203a54:	f466                	sd	s9,40(sp)
ffffffffc0203a56:	ec6e                	sd	s11,24(sp)
ffffffffc0203a58:	892a                	mv	s2,a0
ffffffffc0203a5a:	84ae                	mv	s1,a1
ffffffffc0203a5c:	8d32                	mv	s10,a2
ffffffffc0203a5e:	8a36                	mv	s4,a3
    register int ch, err;
    unsigned long long num;
    int base, width, precision, lflag, altflag;

    while (1) {
        while ((ch = *(unsigned char *)fmt ++) != '%') {
ffffffffc0203a60:	02500993          	li	s3,37
            putch(ch, putdat);
        }

        // Process a %-escape sequence
        char padc = ' ';
        width = precision = -1;
ffffffffc0203a64:	5b7d                	li	s6,-1
ffffffffc0203a66:	00002a97          	auipc	s5,0x2
ffffffffc0203a6a:	d7ea8a93          	addi	s5,s5,-642 # ffffffffc02057e4 <default_pmm_manager+0xb04>
        case 'e':
            err = va_arg(ap, int);
            if (err < 0) {
                err = -err;
            }
            if (err > MAXERROR || (p = error_string[err]) == NULL) {
ffffffffc0203a6e:	00002b97          	auipc	s7,0x2
ffffffffc0203a72:	f52b8b93          	addi	s7,s7,-174 # ffffffffc02059c0 <error_string>
        while ((ch = *(unsigned char *)fmt ++) != '%') {
ffffffffc0203a76:	000d4503          	lbu	a0,0(s10)
ffffffffc0203a7a:	001d0413          	addi	s0,s10,1
ffffffffc0203a7e:	01350a63          	beq	a0,s3,ffffffffc0203a92 <vprintfmt+0x56>
            if (ch == '\0') {
ffffffffc0203a82:	c121                	beqz	a0,ffffffffc0203ac2 <vprintfmt+0x86>
            putch(ch, putdat);
ffffffffc0203a84:	85a6                	mv	a1,s1
        while ((ch = *(unsigned char *)fmt ++) != '%') {
ffffffffc0203a86:	0405                	addi	s0,s0,1
            putch(ch, putdat);
ffffffffc0203a88:	9902                	jalr	s2
        while ((ch = *(unsigned char *)fmt ++) != '%') {
ffffffffc0203a8a:	fff44503          	lbu	a0,-1(s0)
ffffffffc0203a8e:	ff351ae3          	bne	a0,s3,ffffffffc0203a82 <vprintfmt+0x46>
        switch (ch = *(unsigned char *)fmt ++) {
ffffffffc0203a92:	00044603          	lbu	a2,0(s0)
        char padc = ' ';
ffffffffc0203a96:	02000793          	li	a5,32
        lflag = altflag = 0;
ffffffffc0203a9a:	4c81                	li	s9,0
ffffffffc0203a9c:	4881                	li	a7,0
        width = precision = -1;
ffffffffc0203a9e:	5c7d                	li	s8,-1
ffffffffc0203aa0:	5dfd                	li	s11,-1
ffffffffc0203aa2:	05500513          	li	a0,85
                if (ch < '0' || ch > '9') {
ffffffffc0203aa6:	4825                	li	a6,9
        switch (ch = *(unsigned char *)fmt ++) {
ffffffffc0203aa8:	fdd6059b          	addiw	a1,a2,-35
ffffffffc0203aac:	0ff5f593          	zext.b	a1,a1
ffffffffc0203ab0:	00140d13          	addi	s10,s0,1
ffffffffc0203ab4:	04b56263          	bltu	a0,a1,ffffffffc0203af8 <vprintfmt+0xbc>
ffffffffc0203ab8:	058a                	slli	a1,a1,0x2
ffffffffc0203aba:	95d6                	add	a1,a1,s5
ffffffffc0203abc:	4194                	lw	a3,0(a1)
ffffffffc0203abe:	96d6                	add	a3,a3,s5
ffffffffc0203ac0:	8682                	jr	a3
            for (fmt --; fmt[-1] != '%'; fmt --)
                /* do nothing */;
            break;
        }
    }
}
ffffffffc0203ac2:	70e6                	ld	ra,120(sp)
ffffffffc0203ac4:	7446                	ld	s0,112(sp)
ffffffffc0203ac6:	74a6                	ld	s1,104(sp)
ffffffffc0203ac8:	7906                	ld	s2,96(sp)
ffffffffc0203aca:	69e6                	ld	s3,88(sp)
ffffffffc0203acc:	6a46                	ld	s4,80(sp)
ffffffffc0203ace:	6aa6                	ld	s5,72(sp)
ffffffffc0203ad0:	6b06                	ld	s6,64(sp)
ffffffffc0203ad2:	7be2                	ld	s7,56(sp)
ffffffffc0203ad4:	7c42                	ld	s8,48(sp)
ffffffffc0203ad6:	7ca2                	ld	s9,40(sp)
ffffffffc0203ad8:	7d02                	ld	s10,32(sp)
ffffffffc0203ada:	6de2                	ld	s11,24(sp)
ffffffffc0203adc:	6109                	addi	sp,sp,128
ffffffffc0203ade:	8082                	ret
            padc = '0';
ffffffffc0203ae0:	87b2                	mv	a5,a2
            goto reswitch;
ffffffffc0203ae2:	00144603          	lbu	a2,1(s0)
        switch (ch = *(unsigned char *)fmt ++) {
ffffffffc0203ae6:	846a                	mv	s0,s10
ffffffffc0203ae8:	00140d13          	addi	s10,s0,1
ffffffffc0203aec:	fdd6059b          	addiw	a1,a2,-35
ffffffffc0203af0:	0ff5f593          	zext.b	a1,a1
ffffffffc0203af4:	fcb572e3          	bgeu	a0,a1,ffffffffc0203ab8 <vprintfmt+0x7c>
            putch('%', putdat);
ffffffffc0203af8:	85a6                	mv	a1,s1
ffffffffc0203afa:	02500513          	li	a0,37
ffffffffc0203afe:	9902                	jalr	s2
            for (fmt --; fmt[-1] != '%'; fmt --)
ffffffffc0203b00:	fff44783          	lbu	a5,-1(s0)
ffffffffc0203b04:	8d22                	mv	s10,s0
ffffffffc0203b06:	f73788e3          	beq	a5,s3,ffffffffc0203a76 <vprintfmt+0x3a>
ffffffffc0203b0a:	ffed4783          	lbu	a5,-2(s10)
ffffffffc0203b0e:	1d7d                	addi	s10,s10,-1
ffffffffc0203b10:	ff379de3          	bne	a5,s3,ffffffffc0203b0a <vprintfmt+0xce>
ffffffffc0203b14:	b78d                	j	ffffffffc0203a76 <vprintfmt+0x3a>
                precision = precision * 10 + ch - '0';
ffffffffc0203b16:	fd060c1b          	addiw	s8,a2,-48
                ch = *fmt;
ffffffffc0203b1a:	00144603          	lbu	a2,1(s0)
        switch (ch = *(unsigned char *)fmt ++) {
ffffffffc0203b1e:	846a                	mv	s0,s10
                if (ch < '0' || ch > '9') {
ffffffffc0203b20:	fd06069b          	addiw	a3,a2,-48
                ch = *fmt;
ffffffffc0203b24:	0006059b          	sext.w	a1,a2
                if (ch < '0' || ch > '9') {
ffffffffc0203b28:	02d86463          	bltu	a6,a3,ffffffffc0203b50 <vprintfmt+0x114>
                ch = *fmt;
ffffffffc0203b2c:	00144603          	lbu	a2,1(s0)
                precision = precision * 10 + ch - '0';
ffffffffc0203b30:	002c169b          	slliw	a3,s8,0x2
ffffffffc0203b34:	0186873b          	addw	a4,a3,s8
ffffffffc0203b38:	0017171b          	slliw	a4,a4,0x1
ffffffffc0203b3c:	9f2d                	addw	a4,a4,a1
                if (ch < '0' || ch > '9') {
ffffffffc0203b3e:	fd06069b          	addiw	a3,a2,-48
            for (precision = 0; ; ++ fmt) {
ffffffffc0203b42:	0405                	addi	s0,s0,1
                precision = precision * 10 + ch - '0';
ffffffffc0203b44:	fd070c1b          	addiw	s8,a4,-48
                ch = *fmt;
ffffffffc0203b48:	0006059b          	sext.w	a1,a2
                if (ch < '0' || ch > '9') {
ffffffffc0203b4c:	fed870e3          	bgeu	a6,a3,ffffffffc0203b2c <vprintfmt+0xf0>
            if (width < 0)
ffffffffc0203b50:	f40ddce3          	bgez	s11,ffffffffc0203aa8 <vprintfmt+0x6c>
                width = precision, precision = -1;
ffffffffc0203b54:	8de2                	mv	s11,s8
ffffffffc0203b56:	5c7d                	li	s8,-1
ffffffffc0203b58:	bf81                	j	ffffffffc0203aa8 <vprintfmt+0x6c>
            if (width < 0)
ffffffffc0203b5a:	fffdc693          	not	a3,s11
ffffffffc0203b5e:	96fd                	srai	a3,a3,0x3f
ffffffffc0203b60:	00ddfdb3          	and	s11,s11,a3
        switch (ch = *(unsigned char *)fmt ++) {
ffffffffc0203b64:	00144603          	lbu	a2,1(s0)
ffffffffc0203b68:	2d81                	sext.w	s11,s11
ffffffffc0203b6a:	846a                	mv	s0,s10
            goto reswitch;
ffffffffc0203b6c:	bf35                	j	ffffffffc0203aa8 <vprintfmt+0x6c>
            precision = va_arg(ap, int);
ffffffffc0203b6e:	000a2c03          	lw	s8,0(s4)
        switch (ch = *(unsigned char *)fmt ++) {
ffffffffc0203b72:	00144603          	lbu	a2,1(s0)
            precision = va_arg(ap, int);
ffffffffc0203b76:	0a21                	addi	s4,s4,8
        switch (ch = *(unsigned char *)fmt ++) {
ffffffffc0203b78:	846a                	mv	s0,s10
            goto process_precision;
ffffffffc0203b7a:	bfd9                	j	ffffffffc0203b50 <vprintfmt+0x114>
    if (lflag >= 2) {
ffffffffc0203b7c:	4705                	li	a4,1
            precision = va_arg(ap, int);
ffffffffc0203b7e:	008a0593          	addi	a1,s4,8
    if (lflag >= 2) {
ffffffffc0203b82:	01174463          	blt	a4,a7,ffffffffc0203b8a <vprintfmt+0x14e>
    else if (lflag) {
ffffffffc0203b86:	1a088e63          	beqz	a7,ffffffffc0203d42 <vprintfmt+0x306>
        return va_arg(*ap, unsigned long);
ffffffffc0203b8a:	000a3603          	ld	a2,0(s4)
ffffffffc0203b8e:	46c1                	li	a3,16
ffffffffc0203b90:	8a2e                	mv	s4,a1
            printnum(putch, putdat, num, base, width, padc);
ffffffffc0203b92:	2781                	sext.w	a5,a5
ffffffffc0203b94:	876e                	mv	a4,s11
ffffffffc0203b96:	85a6                	mv	a1,s1
ffffffffc0203b98:	854a                	mv	a0,s2
ffffffffc0203b9a:	e37ff0ef          	jal	ra,ffffffffc02039d0 <printnum>
            break;
ffffffffc0203b9e:	bde1                	j	ffffffffc0203a76 <vprintfmt+0x3a>
            putch(va_arg(ap, int), putdat);
ffffffffc0203ba0:	000a2503          	lw	a0,0(s4)
ffffffffc0203ba4:	85a6                	mv	a1,s1
ffffffffc0203ba6:	0a21                	addi	s4,s4,8
ffffffffc0203ba8:	9902                	jalr	s2
            break;
ffffffffc0203baa:	b5f1                	j	ffffffffc0203a76 <vprintfmt+0x3a>
    if (lflag >= 2) {
ffffffffc0203bac:	4705                	li	a4,1
            precision = va_arg(ap, int);
ffffffffc0203bae:	008a0593          	addi	a1,s4,8
    if (lflag >= 2) {
ffffffffc0203bb2:	01174463          	blt	a4,a7,ffffffffc0203bba <vprintfmt+0x17e>
    else if (lflag) {
ffffffffc0203bb6:	18088163          	beqz	a7,ffffffffc0203d38 <vprintfmt+0x2fc>
        return va_arg(*ap, unsigned long);
ffffffffc0203bba:	000a3603          	ld	a2,0(s4)
ffffffffc0203bbe:	46a9                	li	a3,10
ffffffffc0203bc0:	8a2e                	mv	s4,a1
ffffffffc0203bc2:	bfc1                	j	ffffffffc0203b92 <vprintfmt+0x156>
        switch (ch = *(unsigned char *)fmt ++) {
ffffffffc0203bc4:	00144603          	lbu	a2,1(s0)
            altflag = 1;
ffffffffc0203bc8:	4c85                	li	s9,1
        switch (ch = *(unsigned char *)fmt ++) {
ffffffffc0203bca:	846a                	mv	s0,s10
            goto reswitch;
ffffffffc0203bcc:	bdf1                	j	ffffffffc0203aa8 <vprintfmt+0x6c>
            putch(ch, putdat);
ffffffffc0203bce:	85a6                	mv	a1,s1
ffffffffc0203bd0:	02500513          	li	a0,37
ffffffffc0203bd4:	9902                	jalr	s2
            break;
ffffffffc0203bd6:	b545                	j	ffffffffc0203a76 <vprintfmt+0x3a>
        switch (ch = *(unsigned char *)fmt ++) {
ffffffffc0203bd8:	00144603          	lbu	a2,1(s0)
            lflag ++;
ffffffffc0203bdc:	2885                	addiw	a7,a7,1
        switch (ch = *(unsigned char *)fmt ++) {
ffffffffc0203bde:	846a                	mv	s0,s10
            goto reswitch;
ffffffffc0203be0:	b5e1                	j	ffffffffc0203aa8 <vprintfmt+0x6c>
    if (lflag >= 2) {
ffffffffc0203be2:	4705                	li	a4,1
            precision = va_arg(ap, int);
ffffffffc0203be4:	008a0593          	addi	a1,s4,8
    if (lflag >= 2) {
ffffffffc0203be8:	01174463          	blt	a4,a7,ffffffffc0203bf0 <vprintfmt+0x1b4>
    else if (lflag) {
ffffffffc0203bec:	14088163          	beqz	a7,ffffffffc0203d2e <vprintfmt+0x2f2>
        return va_arg(*ap, unsigned long);
ffffffffc0203bf0:	000a3603          	ld	a2,0(s4)
ffffffffc0203bf4:	46a1                	li	a3,8
ffffffffc0203bf6:	8a2e                	mv	s4,a1
ffffffffc0203bf8:	bf69                	j	ffffffffc0203b92 <vprintfmt+0x156>
            putch('0', putdat);
ffffffffc0203bfa:	03000513          	li	a0,48
ffffffffc0203bfe:	85a6                	mv	a1,s1
ffffffffc0203c00:	e03e                	sd	a5,0(sp)
ffffffffc0203c02:	9902                	jalr	s2
            putch('x', putdat);
ffffffffc0203c04:	85a6                	mv	a1,s1
ffffffffc0203c06:	07800513          	li	a0,120
ffffffffc0203c0a:	9902                	jalr	s2
            num = (unsigned long long)(uintptr_t)va_arg(ap, void *);
ffffffffc0203c0c:	0a21                	addi	s4,s4,8
            goto number;
ffffffffc0203c0e:	6782                	ld	a5,0(sp)
ffffffffc0203c10:	46c1                	li	a3,16
            num = (unsigned long long)(uintptr_t)va_arg(ap, void *);
ffffffffc0203c12:	ff8a3603          	ld	a2,-8(s4)
            goto number;
ffffffffc0203c16:	bfb5                	j	ffffffffc0203b92 <vprintfmt+0x156>
            if ((p = va_arg(ap, char *)) == NULL) {
ffffffffc0203c18:	000a3403          	ld	s0,0(s4)
ffffffffc0203c1c:	008a0713          	addi	a4,s4,8
ffffffffc0203c20:	e03a                	sd	a4,0(sp)
ffffffffc0203c22:	14040263          	beqz	s0,ffffffffc0203d66 <vprintfmt+0x32a>
            if (width > 0 && padc != '-') {
ffffffffc0203c26:	0fb05763          	blez	s11,ffffffffc0203d14 <vprintfmt+0x2d8>
ffffffffc0203c2a:	02d00693          	li	a3,45
ffffffffc0203c2e:	0cd79163          	bne	a5,a3,ffffffffc0203cf0 <vprintfmt+0x2b4>
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
ffffffffc0203c32:	00044783          	lbu	a5,0(s0)
ffffffffc0203c36:	0007851b          	sext.w	a0,a5
ffffffffc0203c3a:	cf85                	beqz	a5,ffffffffc0203c72 <vprintfmt+0x236>
ffffffffc0203c3c:	00140a13          	addi	s4,s0,1
                if (altflag && (ch < ' ' || ch > '~')) {
ffffffffc0203c40:	05e00413          	li	s0,94
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
ffffffffc0203c44:	000c4563          	bltz	s8,ffffffffc0203c4e <vprintfmt+0x212>
ffffffffc0203c48:	3c7d                	addiw	s8,s8,-1
ffffffffc0203c4a:	036c0263          	beq	s8,s6,ffffffffc0203c6e <vprintfmt+0x232>
                    putch('?', putdat);
ffffffffc0203c4e:	85a6                	mv	a1,s1
                if (altflag && (ch < ' ' || ch > '~')) {
ffffffffc0203c50:	0e0c8e63          	beqz	s9,ffffffffc0203d4c <vprintfmt+0x310>
ffffffffc0203c54:	3781                	addiw	a5,a5,-32
ffffffffc0203c56:	0ef47b63          	bgeu	s0,a5,ffffffffc0203d4c <vprintfmt+0x310>
                    putch('?', putdat);
ffffffffc0203c5a:	03f00513          	li	a0,63
ffffffffc0203c5e:	9902                	jalr	s2
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
ffffffffc0203c60:	000a4783          	lbu	a5,0(s4)
ffffffffc0203c64:	3dfd                	addiw	s11,s11,-1
ffffffffc0203c66:	0a05                	addi	s4,s4,1
ffffffffc0203c68:	0007851b          	sext.w	a0,a5
ffffffffc0203c6c:	ffe1                	bnez	a5,ffffffffc0203c44 <vprintfmt+0x208>
            for (; width > 0; width --) {
ffffffffc0203c6e:	01b05963          	blez	s11,ffffffffc0203c80 <vprintfmt+0x244>
ffffffffc0203c72:	3dfd                	addiw	s11,s11,-1
                putch(' ', putdat);
ffffffffc0203c74:	85a6                	mv	a1,s1
ffffffffc0203c76:	02000513          	li	a0,32
ffffffffc0203c7a:	9902                	jalr	s2
            for (; width > 0; width --) {
ffffffffc0203c7c:	fe0d9be3          	bnez	s11,ffffffffc0203c72 <vprintfmt+0x236>
            if ((p = va_arg(ap, char *)) == NULL) {
ffffffffc0203c80:	6a02                	ld	s4,0(sp)
ffffffffc0203c82:	bbd5                	j	ffffffffc0203a76 <vprintfmt+0x3a>
    if (lflag >= 2) {
ffffffffc0203c84:	4705                	li	a4,1
            precision = va_arg(ap, int);
ffffffffc0203c86:	008a0c93          	addi	s9,s4,8
    if (lflag >= 2) {
ffffffffc0203c8a:	01174463          	blt	a4,a7,ffffffffc0203c92 <vprintfmt+0x256>
    else if (lflag) {
ffffffffc0203c8e:	08088d63          	beqz	a7,ffffffffc0203d28 <vprintfmt+0x2ec>
        return va_arg(*ap, long);
ffffffffc0203c92:	000a3403          	ld	s0,0(s4)
            if ((long long)num < 0) {
ffffffffc0203c96:	0a044d63          	bltz	s0,ffffffffc0203d50 <vprintfmt+0x314>
            num = getint(&ap, lflag);
ffffffffc0203c9a:	8622                	mv	a2,s0
ffffffffc0203c9c:	8a66                	mv	s4,s9
ffffffffc0203c9e:	46a9                	li	a3,10
ffffffffc0203ca0:	bdcd                	j	ffffffffc0203b92 <vprintfmt+0x156>
            err = va_arg(ap, int);
ffffffffc0203ca2:	000a2783          	lw	a5,0(s4)
            if (err > MAXERROR || (p = error_string[err]) == NULL) {
ffffffffc0203ca6:	4719                	li	a4,6
            err = va_arg(ap, int);
ffffffffc0203ca8:	0a21                	addi	s4,s4,8
            if (err < 0) {
ffffffffc0203caa:	41f7d69b          	sraiw	a3,a5,0x1f
ffffffffc0203cae:	8fb5                	xor	a5,a5,a3
ffffffffc0203cb0:	40d786bb          	subw	a3,a5,a3
            if (err > MAXERROR || (p = error_string[err]) == NULL) {
ffffffffc0203cb4:	02d74163          	blt	a4,a3,ffffffffc0203cd6 <vprintfmt+0x29a>
ffffffffc0203cb8:	00369793          	slli	a5,a3,0x3
ffffffffc0203cbc:	97de                	add	a5,a5,s7
ffffffffc0203cbe:	639c                	ld	a5,0(a5)
ffffffffc0203cc0:	cb99                	beqz	a5,ffffffffc0203cd6 <vprintfmt+0x29a>
                printfmt(putch, putdat, "%s", p);
ffffffffc0203cc2:	86be                	mv	a3,a5
ffffffffc0203cc4:	00000617          	auipc	a2,0x0
ffffffffc0203cc8:	21460613          	addi	a2,a2,532 # ffffffffc0203ed8 <etext+0x2a>
ffffffffc0203ccc:	85a6                	mv	a1,s1
ffffffffc0203cce:	854a                	mv	a0,s2
ffffffffc0203cd0:	0ce000ef          	jal	ra,ffffffffc0203d9e <printfmt>
ffffffffc0203cd4:	b34d                	j	ffffffffc0203a76 <vprintfmt+0x3a>
                printfmt(putch, putdat, "error %d", err);
ffffffffc0203cd6:	00002617          	auipc	a2,0x2
ffffffffc0203cda:	b0260613          	addi	a2,a2,-1278 # ffffffffc02057d8 <default_pmm_manager+0xaf8>
ffffffffc0203cde:	85a6                	mv	a1,s1
ffffffffc0203ce0:	854a                	mv	a0,s2
ffffffffc0203ce2:	0bc000ef          	jal	ra,ffffffffc0203d9e <printfmt>
ffffffffc0203ce6:	bb41                	j	ffffffffc0203a76 <vprintfmt+0x3a>
                p = "(null)";
ffffffffc0203ce8:	00002417          	auipc	s0,0x2
ffffffffc0203cec:	ae840413          	addi	s0,s0,-1304 # ffffffffc02057d0 <default_pmm_manager+0xaf0>
                for (width -= strnlen(p, precision); width > 0; width --) {
ffffffffc0203cf0:	85e2                	mv	a1,s8
ffffffffc0203cf2:	8522                	mv	a0,s0
ffffffffc0203cf4:	e43e                	sd	a5,8(sp)
ffffffffc0203cf6:	0e2000ef          	jal	ra,ffffffffc0203dd8 <strnlen>
ffffffffc0203cfa:	40ad8dbb          	subw	s11,s11,a0
ffffffffc0203cfe:	01b05b63          	blez	s11,ffffffffc0203d14 <vprintfmt+0x2d8>
                    putch(padc, putdat);
ffffffffc0203d02:	67a2                	ld	a5,8(sp)
ffffffffc0203d04:	00078a1b          	sext.w	s4,a5
                for (width -= strnlen(p, precision); width > 0; width --) {
ffffffffc0203d08:	3dfd                	addiw	s11,s11,-1
                    putch(padc, putdat);
ffffffffc0203d0a:	85a6                	mv	a1,s1
ffffffffc0203d0c:	8552                	mv	a0,s4
ffffffffc0203d0e:	9902                	jalr	s2
                for (width -= strnlen(p, precision); width > 0; width --) {
ffffffffc0203d10:	fe0d9ce3          	bnez	s11,ffffffffc0203d08 <vprintfmt+0x2cc>
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
ffffffffc0203d14:	00044783          	lbu	a5,0(s0)
ffffffffc0203d18:	00140a13          	addi	s4,s0,1
ffffffffc0203d1c:	0007851b          	sext.w	a0,a5
ffffffffc0203d20:	d3a5                	beqz	a5,ffffffffc0203c80 <vprintfmt+0x244>
                if (altflag && (ch < ' ' || ch > '~')) {
ffffffffc0203d22:	05e00413          	li	s0,94
ffffffffc0203d26:	bf39                	j	ffffffffc0203c44 <vprintfmt+0x208>
        return va_arg(*ap, int);
ffffffffc0203d28:	000a2403          	lw	s0,0(s4)
ffffffffc0203d2c:	b7ad                	j	ffffffffc0203c96 <vprintfmt+0x25a>
        return va_arg(*ap, unsigned int);
ffffffffc0203d2e:	000a6603          	lwu	a2,0(s4)
ffffffffc0203d32:	46a1                	li	a3,8
ffffffffc0203d34:	8a2e                	mv	s4,a1
ffffffffc0203d36:	bdb1                	j	ffffffffc0203b92 <vprintfmt+0x156>
ffffffffc0203d38:	000a6603          	lwu	a2,0(s4)
ffffffffc0203d3c:	46a9                	li	a3,10
ffffffffc0203d3e:	8a2e                	mv	s4,a1
ffffffffc0203d40:	bd89                	j	ffffffffc0203b92 <vprintfmt+0x156>
ffffffffc0203d42:	000a6603          	lwu	a2,0(s4)
ffffffffc0203d46:	46c1                	li	a3,16
ffffffffc0203d48:	8a2e                	mv	s4,a1
ffffffffc0203d4a:	b5a1                	j	ffffffffc0203b92 <vprintfmt+0x156>
                    putch(ch, putdat);
ffffffffc0203d4c:	9902                	jalr	s2
ffffffffc0203d4e:	bf09                	j	ffffffffc0203c60 <vprintfmt+0x224>
                putch('-', putdat);
ffffffffc0203d50:	85a6                	mv	a1,s1
ffffffffc0203d52:	02d00513          	li	a0,45
ffffffffc0203d56:	e03e                	sd	a5,0(sp)
ffffffffc0203d58:	9902                	jalr	s2
                num = -(long long)num;
ffffffffc0203d5a:	6782                	ld	a5,0(sp)
ffffffffc0203d5c:	8a66                	mv	s4,s9
ffffffffc0203d5e:	40800633          	neg	a2,s0
ffffffffc0203d62:	46a9                	li	a3,10
ffffffffc0203d64:	b53d                	j	ffffffffc0203b92 <vprintfmt+0x156>
            if (width > 0 && padc != '-') {
ffffffffc0203d66:	03b05163          	blez	s11,ffffffffc0203d88 <vprintfmt+0x34c>
ffffffffc0203d6a:	02d00693          	li	a3,45
ffffffffc0203d6e:	f6d79de3          	bne	a5,a3,ffffffffc0203ce8 <vprintfmt+0x2ac>
                p = "(null)";
ffffffffc0203d72:	00002417          	auipc	s0,0x2
ffffffffc0203d76:	a5e40413          	addi	s0,s0,-1442 # ffffffffc02057d0 <default_pmm_manager+0xaf0>
            for (; (ch = *p ++) != '\0' && (precision < 0 || -- precision >= 0); width --) {
ffffffffc0203d7a:	02800793          	li	a5,40
ffffffffc0203d7e:	02800513          	li	a0,40
ffffffffc0203d82:	00140a13          	addi	s4,s0,1
ffffffffc0203d86:	bd6d                	j	ffffffffc0203c40 <vprintfmt+0x204>
ffffffffc0203d88:	00002a17          	auipc	s4,0x2
ffffffffc0203d8c:	a49a0a13          	addi	s4,s4,-1463 # ffffffffc02057d1 <default_pmm_manager+0xaf1>
ffffffffc0203d90:	02800513          	li	a0,40
ffffffffc0203d94:	02800793          	li	a5,40
                if (altflag && (ch < ' ' || ch > '~')) {
ffffffffc0203d98:	05e00413          	li	s0,94
ffffffffc0203d9c:	b565                	j	ffffffffc0203c44 <vprintfmt+0x208>

ffffffffc0203d9e <printfmt>:
printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...) {
ffffffffc0203d9e:	715d                	addi	sp,sp,-80
    va_start(ap, fmt);
ffffffffc0203da0:	02810313          	addi	t1,sp,40
printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...) {
ffffffffc0203da4:	f436                	sd	a3,40(sp)
    vprintfmt(putch, putdat, fmt, ap);
ffffffffc0203da6:	869a                	mv	a3,t1
printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...) {
ffffffffc0203da8:	ec06                	sd	ra,24(sp)
ffffffffc0203daa:	f83a                	sd	a4,48(sp)
ffffffffc0203dac:	fc3e                	sd	a5,56(sp)
ffffffffc0203dae:	e0c2                	sd	a6,64(sp)
ffffffffc0203db0:	e4c6                	sd	a7,72(sp)
    va_start(ap, fmt);
ffffffffc0203db2:	e41a                	sd	t1,8(sp)
    vprintfmt(putch, putdat, fmt, ap);
ffffffffc0203db4:	c89ff0ef          	jal	ra,ffffffffc0203a3c <vprintfmt>
}
ffffffffc0203db8:	60e2                	ld	ra,24(sp)
ffffffffc0203dba:	6161                	addi	sp,sp,80
ffffffffc0203dbc:	8082                	ret

ffffffffc0203dbe <strlen>:
 * The strlen() function returns the length of string @s.
 * */
size_t
strlen(const char *s) {
    size_t cnt = 0;
    while (*s ++ != '\0') {
ffffffffc0203dbe:	00054783          	lbu	a5,0(a0)
strlen(const char *s) {
ffffffffc0203dc2:	872a                	mv	a4,a0
    size_t cnt = 0;
ffffffffc0203dc4:	4501                	li	a0,0
    while (*s ++ != '\0') {
ffffffffc0203dc6:	cb81                	beqz	a5,ffffffffc0203dd6 <strlen+0x18>
        cnt ++;
ffffffffc0203dc8:	0505                	addi	a0,a0,1
    while (*s ++ != '\0') {
ffffffffc0203dca:	00a707b3          	add	a5,a4,a0
ffffffffc0203dce:	0007c783          	lbu	a5,0(a5)
ffffffffc0203dd2:	fbfd                	bnez	a5,ffffffffc0203dc8 <strlen+0xa>
ffffffffc0203dd4:	8082                	ret
    }
    return cnt;
}
ffffffffc0203dd6:	8082                	ret

ffffffffc0203dd8 <strnlen>:
 * @len if there is no '\0' character among the first @len characters
 * pointed by @s.
 * */
size_t
strnlen(const char *s, size_t len) {
    size_t cnt = 0;
ffffffffc0203dd8:	4781                	li	a5,0
    while (cnt < len && *s ++ != '\0') {
ffffffffc0203dda:	e589                	bnez	a1,ffffffffc0203de4 <strnlen+0xc>
ffffffffc0203ddc:	a811                	j	ffffffffc0203df0 <strnlen+0x18>
        cnt ++;
ffffffffc0203dde:	0785                	addi	a5,a5,1
    while (cnt < len && *s ++ != '\0') {
ffffffffc0203de0:	00f58863          	beq	a1,a5,ffffffffc0203df0 <strnlen+0x18>
ffffffffc0203de4:	00f50733          	add	a4,a0,a5
ffffffffc0203de8:	00074703          	lbu	a4,0(a4)
ffffffffc0203dec:	fb6d                	bnez	a4,ffffffffc0203dde <strnlen+0x6>
ffffffffc0203dee:	85be                	mv	a1,a5
    }
    return cnt;
}
ffffffffc0203df0:	852e                	mv	a0,a1
ffffffffc0203df2:	8082                	ret

ffffffffc0203df4 <strcpy>:
char *
strcpy(char *dst, const char *src) {
#ifdef __HAVE_ARCH_STRCPY
    return __strcpy(dst, src);
#else
    char *p = dst;
ffffffffc0203df4:	87aa                	mv	a5,a0
    while ((*p ++ = *src ++) != '\0')
ffffffffc0203df6:	0005c703          	lbu	a4,0(a1)
ffffffffc0203dfa:	0785                	addi	a5,a5,1
ffffffffc0203dfc:	0585                	addi	a1,a1,1
ffffffffc0203dfe:	fee78fa3          	sb	a4,-1(a5)
ffffffffc0203e02:	fb75                	bnez	a4,ffffffffc0203df6 <strcpy+0x2>
        /* nothing */;
    return dst;
#endif /* __HAVE_ARCH_STRCPY */
}
ffffffffc0203e04:	8082                	ret

ffffffffc0203e06 <strcmp>:
int
strcmp(const char *s1, const char *s2) {
#ifdef __HAVE_ARCH_STRCMP
    return __strcmp(s1, s2);
#else
    while (*s1 != '\0' && *s1 == *s2) {
ffffffffc0203e06:	00054783          	lbu	a5,0(a0)
        s1 ++, s2 ++;
    }
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
ffffffffc0203e0a:	0005c703          	lbu	a4,0(a1)
    while (*s1 != '\0' && *s1 == *s2) {
ffffffffc0203e0e:	cb89                	beqz	a5,ffffffffc0203e20 <strcmp+0x1a>
        s1 ++, s2 ++;
ffffffffc0203e10:	0505                	addi	a0,a0,1
ffffffffc0203e12:	0585                	addi	a1,a1,1
    while (*s1 != '\0' && *s1 == *s2) {
ffffffffc0203e14:	fee789e3          	beq	a5,a4,ffffffffc0203e06 <strcmp>
    return (int)((unsigned char)*s1 - (unsigned char)*s2);
ffffffffc0203e18:	0007851b          	sext.w	a0,a5
#endif /* __HAVE_ARCH_STRCMP */
}
ffffffffc0203e1c:	9d19                	subw	a0,a0,a4
ffffffffc0203e1e:	8082                	ret
ffffffffc0203e20:	4501                	li	a0,0
ffffffffc0203e22:	bfed                	j	ffffffffc0203e1c <strcmp+0x16>

ffffffffc0203e24 <strncmp>:
 * the characters differ, until a terminating null-character is reached, or
 * until @n characters match in both strings, whichever happens first.
 * */
int
strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 != '\0' && *s1 == *s2) {
ffffffffc0203e24:	c20d                	beqz	a2,ffffffffc0203e46 <strncmp+0x22>
ffffffffc0203e26:	962e                	add	a2,a2,a1
ffffffffc0203e28:	a031                	j	ffffffffc0203e34 <strncmp+0x10>
        n --, s1 ++, s2 ++;
ffffffffc0203e2a:	0505                	addi	a0,a0,1
    while (n > 0 && *s1 != '\0' && *s1 == *s2) {
ffffffffc0203e2c:	00e79a63          	bne	a5,a4,ffffffffc0203e40 <strncmp+0x1c>
ffffffffc0203e30:	00b60b63          	beq	a2,a1,ffffffffc0203e46 <strncmp+0x22>
ffffffffc0203e34:	00054783          	lbu	a5,0(a0)
        n --, s1 ++, s2 ++;
ffffffffc0203e38:	0585                	addi	a1,a1,1
    while (n > 0 && *s1 != '\0' && *s1 == *s2) {
ffffffffc0203e3a:	fff5c703          	lbu	a4,-1(a1)
ffffffffc0203e3e:	f7f5                	bnez	a5,ffffffffc0203e2a <strncmp+0x6>
    }
    return (n == 0) ? 0 : (int)((unsigned char)*s1 - (unsigned char)*s2);
ffffffffc0203e40:	40e7853b          	subw	a0,a5,a4
}
ffffffffc0203e44:	8082                	ret
    return (n == 0) ? 0 : (int)((unsigned char)*s1 - (unsigned char)*s2);
ffffffffc0203e46:	4501                	li	a0,0
ffffffffc0203e48:	8082                	ret

ffffffffc0203e4a <strchr>:
 * The strchr() function returns a pointer to the first occurrence of
 * character in @s. If the value is not found, the function returns 'NULL'.
 * */
char *
strchr(const char *s, char c) {
    while (*s != '\0') {
ffffffffc0203e4a:	00054783          	lbu	a5,0(a0)
ffffffffc0203e4e:	c799                	beqz	a5,ffffffffc0203e5c <strchr+0x12>
        if (*s == c) {
ffffffffc0203e50:	00f58763          	beq	a1,a5,ffffffffc0203e5e <strchr+0x14>
    while (*s != '\0') {
ffffffffc0203e54:	00154783          	lbu	a5,1(a0)
            return (char *)s;
        }
        s ++;
ffffffffc0203e58:	0505                	addi	a0,a0,1
    while (*s != '\0') {
ffffffffc0203e5a:	fbfd                	bnez	a5,ffffffffc0203e50 <strchr+0x6>
    }
    return NULL;
ffffffffc0203e5c:	4501                	li	a0,0
}
ffffffffc0203e5e:	8082                	ret

ffffffffc0203e60 <memset>:
memset(void *s, char c, size_t n) {
#ifdef __HAVE_ARCH_MEMSET
    return __memset(s, c, n);
#else
    char *p = s;
    while (n -- > 0) {
ffffffffc0203e60:	ca01                	beqz	a2,ffffffffc0203e70 <memset+0x10>
ffffffffc0203e62:	962a                	add	a2,a2,a0
    char *p = s;
ffffffffc0203e64:	87aa                	mv	a5,a0
        *p ++ = c;
ffffffffc0203e66:	0785                	addi	a5,a5,1
ffffffffc0203e68:	feb78fa3          	sb	a1,-1(a5)
    while (n -- > 0) {
ffffffffc0203e6c:	fec79de3          	bne	a5,a2,ffffffffc0203e66 <memset+0x6>
    }
    return s;
#endif /* __HAVE_ARCH_MEMSET */
}
ffffffffc0203e70:	8082                	ret

ffffffffc0203e72 <memcpy>:
#ifdef __HAVE_ARCH_MEMCPY
    return __memcpy(dst, src, n);
#else
    const char *s = src;
    char *d = dst;
    while (n -- > 0) {
ffffffffc0203e72:	ca19                	beqz	a2,ffffffffc0203e88 <memcpy+0x16>
ffffffffc0203e74:	962e                	add	a2,a2,a1
    char *d = dst;
ffffffffc0203e76:	87aa                	mv	a5,a0
        *d ++ = *s ++;
ffffffffc0203e78:	0005c703          	lbu	a4,0(a1)
ffffffffc0203e7c:	0585                	addi	a1,a1,1
ffffffffc0203e7e:	0785                	addi	a5,a5,1
ffffffffc0203e80:	fee78fa3          	sb	a4,-1(a5)
    while (n -- > 0) {
ffffffffc0203e84:	fec59ae3          	bne	a1,a2,ffffffffc0203e78 <memcpy+0x6>
    }
    return dst;
#endif /* __HAVE_ARCH_MEMCPY */
}
ffffffffc0203e88:	8082                	ret

ffffffffc0203e8a <memcmp>:
 * */
int
memcmp(const void *v1, const void *v2, size_t n) {
    const char *s1 = (const char *)v1;
    const char *s2 = (const char *)v2;
    while (n -- > 0) {
ffffffffc0203e8a:	c205                	beqz	a2,ffffffffc0203eaa <memcmp+0x20>
ffffffffc0203e8c:	962e                	add	a2,a2,a1
ffffffffc0203e8e:	a019                	j	ffffffffc0203e94 <memcmp+0xa>
ffffffffc0203e90:	00c58d63          	beq	a1,a2,ffffffffc0203eaa <memcmp+0x20>
        if (*s1 != *s2) {
ffffffffc0203e94:	00054783          	lbu	a5,0(a0)
ffffffffc0203e98:	0005c703          	lbu	a4,0(a1)
            return (int)((unsigned char)*s1 - (unsigned char)*s2);
        }
        s1 ++, s2 ++;
ffffffffc0203e9c:	0505                	addi	a0,a0,1
ffffffffc0203e9e:	0585                	addi	a1,a1,1
        if (*s1 != *s2) {
ffffffffc0203ea0:	fee788e3          	beq	a5,a4,ffffffffc0203e90 <memcmp+0x6>
            return (int)((unsigned char)*s1 - (unsigned char)*s2);
ffffffffc0203ea4:	40e7853b          	subw	a0,a5,a4
ffffffffc0203ea8:	8082                	ret
    }
    return 0;
ffffffffc0203eaa:	4501                	li	a0,0
}
ffffffffc0203eac:	8082                	ret
