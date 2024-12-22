//// 移动到用户模式运行。
// 该函数利用 iret 指令实现从内核模式移动到初始任务 0 中去执行。
//模仿中断硬件压栈，顺序是 ss、esp、eflags、cs、eip
#define move_to_user_mode() \
__asm__ ("movl %%esp, %%eax                 # 保存堆栈指针 esp 到 eax 寄存器中。 \n\t" \
        "pushl $0x17                        # SS进栈，0x17即二进制的00010111（3特权级、LDT、数据段） \n\t"   \
        "pushl %%eax                        # ESP 进栈\n\t"   \
        "pushfl                             # EFLAGS 进栈\n\t"        \
        "pushl $0xf                         # Task0 的 CS 进栈，0x0f 即00001111（3特权级、LDT、代码段）\n\t"    \
        "pushl $1f                          # 下面标号1的偏移地址 EIP 进栈\n\t"     \
        "iret                               # 执行中断返回指令，出栈恢复现场、反转特权级从0到3，跳转到下面标号1处 \n\t"          \
        "1:\tmovw $0x17, %%ax               # 开始执行任务0   \n\t"              \
        "movw %%ax, %%ds                    # 初始化段寄存器，指向本局部表的数据段，使ds、es、fs、gs 与 ss 一致 \n\t" \
        "movw %%ax, %%es\n\t" \
        "movw %%ax, %%fs\n\t" \
        "movw %%ax,%%gs" \
	    :::"ax")

#define sti() __asm__ ("sti"::)     // 开中断嵌入汇编宏函数。
#define cli() __asm__ ("cli"::)     // 关中断。
#define nop() __asm__ ("nop"::)     // 空操作。

#define iret() __asm__ ("iret"::)   // 中断返回。

/// 设置门描述符宏函数。
// 参数:gate_addr -描述符地址;type -描述符中类型域值;dpl -描述符特权层值;addr -偏移地址。
// %0 - (由 dpl,type 组合成的类型标志字);%1 - (描述符低 4 字节地址);
// %2 - (描述符高 4 字节地址);%3 - edx(程序偏移地址 addr);%4 - eax(高字中含有段选择符)。
#define _set_gate(gate_addr,type,dpl,addr) \
__asm__ ("movw %%dx,%%ax            # 将偏移地址低字与选择符组合成描述符低 4 字节(eax)。 \n\t" \
	"movw %0,%%dx                   # 将类型标志字与偏移高字组合成描述符高 4 字节(edx)。 \n\t" \
	"movl %%eax,%1                  # 分别设置门描述符的低 4 字节和高 4 字节。 \n\t" \
	"movl %%edx,%2" \
	: \
	: "i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
	"o" (*((char *) (gate_addr))), \
	"o" (*(4+(char *) (gate_addr))), \
	"d" ((char *) (addr)),"a" (0x00080000))

//// 设置中断门函数。
// 参数:n - 中断号;addr - 中断程序偏移地址。
// &idt[n]对应中断号在中断描述符表中的偏移值;中断描述符的类型是 14，特权级是 0。
#define set_intr_gate(n,addr) \
	_set_gate(&idt[n],14,0,addr)

//// 设置陷阱门函数。
// 参数:n - 中断号;addr - 中断程序偏移地址。
// &idt[n]对应中断号在中断描述符表中的偏移值;中断描述符的类型是 15，特权级是 0。
#define set_trap_gate(n,addr) \
	_set_gate(&idt[n],15,0,addr)

//// 设置系统陷阱门函数。
// 上面 set_trap_gate()设置的描述符的特权级为 0，而这里是 3。因此 set_system_gate()设置的
// 中断处理过程能够被所有程序执行。例如单步调试、溢出出错和边界超出出错处理。
// 参数:n - 中断号;addr - 中断程序偏移地址。
// &idt[n]对应中断号在中断描述符表中的偏移值;中断描述符的类型是 15，特权级是 3。
#define set_system_gate(n,addr) \
	_set_gate(&idt[n],15,3,addr)

//// 设置段描述符函数(内核中没有用到)。
// 参数:gate_addr -描述符地址;type -描述符中类型域值;dpl -描述符特权层值;
// base - 段的基地址; limit - 段限长。(参见段描述符的格式)
#define _set_seg_desc(gate_addr,type,dpl,base,limit) {\
	*(gate_addr) = ((base) & 0xff000000) |                  /* 描述符低 4 字节。 */ \
		(((base) & 0x00ff0000)>>16) | \
		((limit) & 0xf0000) | \
		((dpl)<<13) | \
		(0x00408000) | \
		((type)<<8); \
	*((gate_addr)+1) = (((base) & 0x0000ffff)<<16) |        /* 描述符高 4 字节 。 */ \
		((limit) & 0x0ffff); }

//// 在全局表中设置任务状态段/局部表描述符。状态段和局部表段的长度均被设置成 104 字节。
// 参数:n - 在全局表中描述符项 n 所对应的地址;addr - 状态段/局部表所在内存的基地址。
//      type - 描述符中的标志类型字节。
// %0 - eax(地址 addr);%1 - (描述符项 n 的地址);%2 - (描述符项 n 的地址偏移 2 处);
// %3 - (描述符项n的地址偏移4处);%4 - (描述符项n的地址偏移5处);
// %5 - (描述符项n的地址偏移6处);%6 - (描述符项n的地址偏移7处);
#define _set_tssldt_desc(n,addr,type) \
__asm__ ("movw $104,%1          #将104，即1101000存入描述符的第1、2字节\n\t" \
	"movw %%ax,%2               #将 tss或 ldt 基地址的低16位存入描述符的第3、4字节\n\t" \
	"rorl $16,%%eax             #循环右移16位，即高、低字节互换\n\t" \
	"movb %%al,%3               #将互换完的第1字节，即地址的第3字节存入第5字节\n\t" \
	"movb $" type ",%4          #将0x89(10001001)或0x82(10000010)存入第6字节\n\t" \
	"movb $0x00,%5              #将0x00存在第7字节\n\t" \
	"movb %%ah,%6               #将互换完的第2字节，即地址的第4字节存入第8字节\n\t" \
	"rorl $16,%%eax             #复原eax" \
	::"a" (addr), "m" (*(n)), "m" (*(n+2)), "m" (*(n+4)), \
	 "m" (*(n+5)), "m" (*(n+6)), "m" (*(n+7))                      \
    )
    //"m" (*(n))是 gdt 第 n 项描述符的地址开始的内存单元
    //"m" (*(n+2))是 gdt 第 n 项描述符的地址向上第3字节开始的内存单元
    //  其余以此类推

//// 在全局表中设置任务状态段描述符。
// n - 是该描述符的指针;addr - 是描述符项中段的基地址值。任务状态段描述符的类型是 0x89。
#define set_tss_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),"0x89")

//// 在全局表中设置局部表描述符。
// n - 是该描述符的指针;addr - 是描述符项中段的基地址值。局部表段描述符的类型是 0x82。
#define set_ldt_desc(n,addr) _set_tssldt_desc(((char *) (n)),((int)(addr)),"0x82")
