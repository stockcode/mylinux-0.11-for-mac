/*
 *  linux/kernel/sched.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 *
 * 'sched.c'是主要的内核文件。其中包括有关调度的基本函数(sleep_on、wakeup、schedule 等)以及
 * 一些简单的系统调用函数(比如 getpid()，仅从当前任务中获取一个字段)。
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>
#include<linux/mm.h>

#include <signal.h>

// 取信号 nr 在信号位图中对应位的二进制数值。信号编号 1-32。比如信号 5 的位图数值 = 1<<(5-1)
// = 16 = 00010000b。
#define _S(nr) (1<<((nr)-1))

// 除了 SIGKILL 和 SIGSTOP 信号以外其他都是可阻塞的(...10111111111011111111b)。
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

// 显示任务号 nr 的进程号、进程状态和内核堆栈空闲字节数(大约)。
void show_task(int nr,struct task_struct * p)
{
    int i,j = 4096-sizeof(struct task_struct);

    printk("%d: pid=%d, state=%d, ",nr,p->pid,p->state);
    i=0;
    while (i<j && !((char *)(p+1))[i])      // 检测指定任务数据结构以后等于 0 的字节数。
        i++;
    printk("%d (of %d) chars free in kernel stack\n\r",i,j);
}

// 显示所有任务的任务号、进程号、进程状态和内核堆栈空闲字节数(大约)。
void show_stat(void)
{
    int i;

    // NR_TASKS 是系统能容纳的最大进程(任务)数量(64个)，定义在 include/kernel/sched.h 第 4 行。
    for (i=0;i<NR_TASKS;i++)
        if (task[i])
            show_task(i,task[i]);
}


// PC 机 8253 定时芯片的输入时钟频率约为 1.193180MHz。Linux 内核希望定时器发出中断的频率是
// 100Hz，也即每 10ms 发出一次时钟中断。因此这里的 LATCH 是设置 8253 芯片的初值，参见 407 行。
#define LATCH (11931800/HZ)

extern void mem_use(void);          // [??]没有任何地方定义和引用该函数。

extern int timer_interrupt(void);   // 时钟中断处理程序(kernel/system_call.s,176)。
extern int system_call(void);       // 系统调用中断处理程序(kernel/system_call.s,80)。

//task_struct与内核栈的共用体
// 每个任务(进程)在内核态运行时都有自己的内核态栈。这里定义了任务的内核态栈结构。
union task_union {              // 定义任务联合（任务结构成员和 stack字符数组成员）。
    struct task_struct task;    // 因为一个任务的数据结构与其内核态堆栈放在同一内存页
    //PAGE_SIZE是4KB
    char stack[PAGE_SIZE];      // 中，所以从栈段寄存器 ss可以获得其数据段选择符。
};

//进程0的 task_struct
static union task_union init_task = {INIT_TASK,};   // 定义初始任务的数据(sched.h 中)。

// 从开机开始算起的滴答数时间值(10ms/滴答)。定义在 include/linux/sched.h 第 139 行。
// 前面的限定符 volatile，英文解释是易变、不稳定的意思。这里是要求 gcc 不要对该变量进行优化
// 处理，也不要挪动位置，因为也许别的程序会来修改它的值。
long volatile jiffies=0;
long startup_time=0;                                // 开机时间。从 1970:0:0:0 开始计时的秒数。
struct task_struct *current = &(init_task.task);    // 当前任务指针(初始化为初始任务)。
struct task_struct *last_task_used_math = NULL;     // 使用过协处理器任务的指针。

//初始化进程槽 task[NR_TASKS]的第一项为进程0，即 task[0]为进程0占用
struct task_struct * task[NR_TASKS] = {&(init_task.task), };        // 定义任务指针数组。

// 定义用户栈，4K。在内核初始化操作完成以后将被用作任务 0 的用户态栈。
long user_stack [ PAGE_SIZE>>2 ] ;

// 该结构用于设置栈 ss:esp(数据段选择符，指针)，见 head.s，第 23 行。指针指在最后一项。
struct {
    long * a;
    short b;
} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };

/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 * 将当前协处理器内容保存到老协处理器状态数组中，并将当前任务的协处理器
 * 内容加载进协处理器。
 */

// 当任务被调度交换过以后，该函数用以保存原任务的协处理器状态(上下文)并恢复新调度进来的
// 当前任务的协处理器执行状态。
void math_state_restore()
{
    if (last_task_used_math == current)     // 如果任务没变则返回(上一个任务就是当前任务)。
        return;                             // 这里所指的"上一个任务"是刚被交换出去的任务。
    __asm__("fwait");                       // 在发送协处理器命令之前要先发 WAIT 指令。
    if (last_task_used_math) {              // 如果上个任务使用了协处理器，则保存其状态。
        __asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
    }
    last_task_used_math=current;            // 现在，last_task_used_math 指向当前任务，
                                            // 以备当前任务被交换出去时使用。
    if (current->used_math) {               // 如果当前任务用过协处理器，则恢复其状态。
        __asm__("frstor %0"::"m" (current->tss.i387));
    } else {                                // 否则的话说明是第一次使用，
        __asm__("fninit"::);                // 于是就向协处理器发初始化命令，
        current->used_math=1;               // 并设置使用了协处理器标志。
    }
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 *
 * 'schedule()'是调度函数。这是个很好的代码!没有任何理由对它进行修改，因为它可以在所有的
 * 环境下工作(比如能够对 IO-边界处理很好的响应等)。只有一件事值得留意，那就是这里的信号
 * 处理代码。
 * 注意!!任务 0 是个闲置('idle')任务，只有当没有其他任务可以运行时才调用它。它不能被杀
 * 死，也不能睡眠。任务 0 中的状态信息'state'是从来不用的。
 */
void schedule(void)
{
    int i,next,c;
    struct task_struct ** p;            // 任务结构指针的指针。

/* check alarm, wake up any interruptible tasks that have got a signal */
/* 检测 alarm(进程的报警定时值)，唤醒任何已得到信号的可中断任务 */

// 从任务数组中最后一个任务开始检测 alarm。
    for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
        if (*p) {
            // 如果设置过任务的定时值 alarm，并且已经过期(alarm<jiffies),则在信号位图中置 SIGALRM 信号，
            // 即向任务发送 SIGALARM 信号。然后清 alarm。该信号的默认操作是终止进程。
            // jiffies 是系统从开机开始算起的滴答数(10ms/滴答)。定义在 sched.h 第 139 行。
            if ((*p)->alarm && (*p)->alarm < jiffies) {
                (*p)->signal |= (1<<(SIGALRM-1));
                (*p)->alarm = 0;
            }
            // 如果信号位图中除被阻塞的信号外还有其他信号，并且任务处于可中断状态，则置任务为就绪状态。
            // 其中'~(_BLOCKABLE & (*p)->blocked)'用于忽略被阻塞的信号，但 SIGKILL 和 SIGSTOP 不能被阻塞。
            if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
                (*p)->state==TASK_INTERRUPTIBLE)
                (*p)->state=TASK_RUNNING;           //置为就绪(可执行)状态。
        }

/* this is the scheduler proper: */
/* 这里是调度程序的主要部分 */

    while (1) {
        c = -1;
        next = 0;
        i = NR_TASKS;
        p = &task[NR_TASKS];

        // 这段代码也是从任务数组的最后一个任务开始循环处理，并跳过不含任务的数组槽。比较每个就绪
        // 状态任务的 counter(任务运行时间的递减滴答计数)值，哪一个值大，运行时间还不长，next 就
        // 指向哪个的任务号。
        while (--i) {
            if (!*--p)
                continue;
            if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
                c = (*p)->counter, next = i;
        }
        // 如果比较得出有 counter 值不等于 0 的结果，则退出 169 行开始的循环，执行任务切换(197 行)。
        if (c) break;

        // 否则就根据每个任务的优先权值，更新每一个任务的 counter 值，然后回到 170 行重新比较。
        // counter 值的计算方式为 counter = counter /2 + priority。这里计算过程不考虑进程的状态。
        for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
            if (*p)
                (*p)->counter = ((*p)->counter >> 1) +
                                (*p)->priority;
    }
    // 切换到任务号为 next 的任务运行。在 171 行 next 被初始化为 0。因此若系统中没有任何其他任务
    // 可运行时，则 next 始终为 0。因此调度函数会在系统空闲时去执行任务 0。此时任务 0 仅执行
    // pause()系统调用，并又会调用本函数。
    switch_to(next);        // 切换到任务号为 next 的任务，并运行之。
}

//// pause()系统调用。转换当前任务的状态为可中断的等待状态，并重新调度。
// 该系统调用将导致进程进入睡眠状态，直到收到一个信号。该信号用于终止进程或者使进程调用
// 一个信号捕获函数。只有当捕获了一个信号，并且信号捕获处理函数返回，pause()才会返回。
// 此时 pause()返回值应该是-1，并且 errno 被置为 EINTR。这里还没有完全实现(直到 0.95 版)。
int sys_pause(void)
{
    current->state = TASK_INTERRUPTIBLE;
    schedule();
    return 0;
}

// 把当前任务置为不可中断的等待状态，并让睡眠队列头的指针指向当前任务。
// 只有明确地唤醒时才会返回。该函数提供了进程与中断处理程序之间的同步机制。
// 函数参数*p 是放置等待任务的队列头指针。
void sleep_on(struct task_struct **p)
{
    struct task_struct *tmp;

    // 若指针无效，则退出。(指针所指的对象可以是 NULL，但指针本身不会为 0)。
    if (!p)
        return;
    // 如果当前任务是任务 0，则死机(impossible!)。
    if (current == &(init_task.task))
        panic("task[0] trying to sleep");

    // 让 tmp 指向已经在等待队列上的任务(如果有的话)，例如 inode->i_wait。并且将睡眠队列头的
    // 等待指针指向当前任务。
    tmp = *p;
    *p = current;
    // 将当前任务置为不可中断的等待状态，并执行重新调度。
    current->state = TASK_UNINTERRUPTIBLE;
    schedule();

    // 只有当这个等待任务被唤醒时，调度程序才又返回到这里，则表示进程已被明确地唤醒。
    // 既然大家都在等待同样的资源，那么在资源可用时，就有必要唤醒所有等待该资源的进程。该函数
    // 嵌套调用，也会嵌套唤醒所有等待该资源的进程。
    if (tmp)        // 若在其前还存在等待的任务，则也将其置为就绪状态(唤醒)。
        tmp->state=0;
}

// 将当前任务置为可中断的等待状态，并放入*p 指定的等待队列中。
void interruptible_sleep_on(struct task_struct **p)
{
    struct task_struct *tmp;

    if (!p)
        return;
    if (current == &(init_task.task))
        panic("task[0] trying to sleep");
    tmp=*p;
    *p=current;
    repeat:	current->state = TASK_INTERRUPTIBLE;
    schedule();

    // 如果等待队列中还有等待任务，并且队列头指针所指向的任务不是当前任务时，则将该等待任务置为
    // 可运行的就绪状态，并重新执行调度程序。当指针*p 所指向的不是当前任务时，表示在当前任务被放
    // 入队列后，又有新的任务被插入等待队列中，因此，就应该同时也将所有其他的等待任务置为可运行
    // 状态。
    if (*p && *p != current) {
        (**p).state=0;
        goto repeat;
    }

    // 下面一句代码有误，应该是*p = tmp，让队列头指针指向其余等待任务，否则在当前任务之前插入
    // 等待队列的任务均被抹掉了。当然，同时也需删除 275 行上的语句。
//    *p=NULL;
    *p = tmp;
    if (tmp)
        tmp->state=0;
}

// 唤醒指定任务*p。
void wake_up(struct task_struct **p)
{
    if (p && *p) {
        (**p).state=0;  // 置为就绪（可执行）状态。
//        *p=NULL;
    }
}

/*
 * OK, here are some floppy things that shouldn't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.
 *
 * 好了，从这里开始是一些有关软盘的子程序，本不应该放在内核的主要部分中的。将它们放在这里
 * 是因为软驱需要定时处理，而放在这里是最方便的。
 */

// 这里用于软驱定时处理的代码是 201–262 行。在阅读这段代码之前请先看一下块设备一章中有关
// 软盘驱动程序(floppy.c)后面的说明。或者到阅读软盘块设备驱动程序时在来看这段代码。
// 其中时间单位:1 个滴答 = 1/100 秒。
// 下面数组存放等待软驱马达启动到正常转速的进程指针。数组索引 0-3 分别对应软驱 A-D。
static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
// 下面数组分别存放各软驱马达启动所需要的滴答数。程序中默认启动时间为 50 个滴答(0.5 秒)。
static int  mon_timer[4]={0,0,0,0};
// 下面数组分别存放各软驱在马达停转之前需维持的时间。程序中设定为 10000 个滴答(100 秒)。
static int moff_timer[4]={0,0,0,0};

// 对应软驱控制器中当前数字输出寄存器。该寄存器每位的定义如下:
// 位 7-4:分别控制驱动器 D-A 马达的启动。1 - 启动;0 - 关闭。
// 位3 :1 - 允许DMA和中断请求;0 - 禁止DMA和中断请求。
// 位 2 :1 - 启动软盘控制器;0 - 复位软盘控制器。
// 位 1-0:00 - 11，用于选择控制的软驱 A-D。
unsigned char current_DOR = 0x0C;       // 这里设置初值为:允许 DMA 和中断请求、启动 FDC。

// 指定软驱启动到正常运转状态所需等待时间。
// nr -- 软驱号(0-3)，返回值为滴答数。
int ticks_to_floppy_on(unsigned int nr)
{
    extern unsigned char selected;
    unsigned char mask = 0x10 << nr;

    if (nr>3)
        panic("floppy_on: nr>3");
    moff_timer[nr]=10000;		/* 100 s = very big :-) */
    cli();				/* use floppy_off to turn it off */
    mask |= current_DOR;
    if (!selected) {
        mask &= 0xFC;
        mask |= nr;
    }
    if (mask != current_DOR) {
        outb(mask,FD_DOR);
        if ((mask ^ current_DOR) & 0xf0)
            mon_timer[nr] = HZ/2;
        else if (mon_timer[nr] < 2)
            mon_timer[nr] = 2;
        current_DOR = mask;
    }
    sti();
    return mon_timer[nr];
}

// 等待指定软驱马达启动所需的一段时间，然后返回。
// 设置指定软驱的马达启动到正常转速所需的延时，然后睡眠等待。在定时中断过程中会一直递减
// 判断这里设定的延时值。当延时到期，就会唤醒这里的等待进程。
void floppy_on(unsigned int nr)
{
    cli();
    while (ticks_to_floppy_on(nr))
        sleep_on(nr+wait_motor);
    sti();
}

// 置关闭相应软驱马达停转定时器(3 秒)。
// 若不使用该函数明确关闭指定的软驱马达，则在马达开启 100 秒之后也会被关闭。
void floppy_off(unsigned int nr)
{
    moff_timer[nr]=3*HZ;
}

// 软盘定时处理子程序。更新马达启动定时值和马达关闭停转计时值。该子程序会在时钟定时中断
// 过程中被调用，因此系统每经过一个滴答(10ms)就会被调用一次，随时更新马达开启或停转定时
// 器的值。如果某一个马达停转定时到，则将数字输出寄存器马达启动位复位。
void do_floppy_timer(void)
{
    int i;
    unsigned char mask = 0x10;

    for (i=0 ; i<4 ; i++,mask <<= 1) {
        if (!(mask & current_DOR))
            continue;
        if (mon_timer[i]) {
            if (!--mon_timer[i])
                wake_up(i+wait_motor);
        } else if (!moff_timer[i]) {
            current_DOR &= ~mask;
            outb(current_DOR,FD_DOR);
        } else
            moff_timer[i]--;
    }
}

#define TIME_REQUESTS 64        // 最多可有 64 个定时器链表。

// 定时器链表结构和定时器数组。
static struct timer_list {
    long jiffies;               // 定时滴答数。
    void (*fn)();               // 定时处理程序。
    struct timer_list * next;   // 下一个定时器。
} timer_list[TIME_REQUESTS], * next_timer = NULL;

// 添加定时器。输入参数为指定的定时值(滴答数)和相应的处理程序指针。
// 软盘驱动程序(floppy.c)利用该函数执行启动或关闭马达的延时操作。
// jiffies – 以 10 毫秒计的滴答数;*fn()- 定时时间到时执行的函数。
void add_timer(long jiffies, void (*fn)(void))
{
    struct timer_list * p;

    if (!fn)
        return;
    cli();
    if (jiffies <= 0)
        (fn)();
    else {
        for (p = timer_list ; p < timer_list + TIME_REQUESTS ; p++)
            if (!p->fn)
                break;
        if (p >= timer_list + TIME_REQUESTS)
            panic("No more time requests free");
        p->fn = fn;
        p->jiffies = jiffies;
        p->next = next_timer;
        next_timer = p;
        while (p->next && p->next->jiffies < p->jiffies) {
            p->jiffies -= p->next->jiffies;
            fn = p->fn;
            p->fn = p->next->fn;
            p->next->fn = fn;
            jiffies = p->jiffies;
            p->jiffies = p->next->jiffies;
            p->next->jiffies = jiffies;
            p = p->next;
        }
    }
    sti();
}

void do_timer(long cpl)
{
    extern int beepcount;
    extern void sysbeepstop(void);

    if (beepcount)
        if (!--beepcount)
            sysbeepstop();

    if (cpl)
        current->utime++;
    else
        current->stime++;

    if (next_timer) {
        next_timer->jiffies--;
        while (next_timer && next_timer->jiffies <= 0) {
            void (*fn)(void);

            fn = next_timer->fn;
            next_timer->fn = NULL;
            next_timer = next_timer->next;
            (fn)();
        }
    }
    if (current_DOR & 0xf0)
        do_floppy_timer();
    if ((--current->counter)>0) return;
    current->counter=0;
    if (!cpl) return;
    schedule();
}

int sys_alarm(long seconds)
{
    int old = current->alarm;

    if (old)
        old = (old - jiffies) / HZ;
    current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
    return (old);
}

int sys_getpid(void)
{
    return current->pid;
}

int sys_getppid(void)
{
    return current->father;
}

int sys_getuid(void)
{
    return current->uid;
}

int sys_geteuid(void)
{
    return current->euid;
}

int sys_getgid(void)
{
    return current->gid;
}

int sys_getegid(void)
{
    return current->egid;
}

int sys_nice(long increment)
{
    if (current->priority-increment>0)
        current->priority -= increment;
    return 0;
}

void sched_init()
{
    int i;
    struct desc_struct* p;

    set_tss_desc(gdt+FIRST_TSS_ENTRY, &init_task.task.tss);             //设置 TSS0
    set_ldt_desc(gdt+FIRST_LDT_ENTRY, &init_task.task.ldt);    //设置 LDT0

    p=gdt+2+FIRST_TSS_ENTRY;    //从GDT的第6项，即 TSS1 开始向上全部清零，并且将进程槽从1往后的项清空。0项为进程0所用
    for(i=1;i<NR_TASKS;i++){
        task[i]=NULL;
        p->a=p->b=0;
        p++;
        p->a=p->b=0;
        p++;
    }

/* Clear NT, so that we won't have troubles with that later on */
    __asm__("pushfl;andl $0xffffbfff,(%esp);popfl");
    lldt(0);    //重要！将 LDT0 挂接到 LDTR 寄存器
    ltr(0);     //重要！将 TSS0 挂接到 TR 寄存器

    outb_p(0x36,0x43);		/* binary, mode 3, LSB/MSB, ch 0 */     //设置定时器
	outb_p(LATCH & 0xff , 0x40);	/* LSB */   //每10毫秒一次时钟中断
	outb(LATCH >> 8 , 0x40);	/* MSB */
    set_intr_gate(0x20,&timer_interrupt);       //重要！设置时钟中断，进程调度的基础
    outb(inb_p(0x21)&~0x01,0x21);               //允许时钟中断
    set_system_gate(0x80,&system_call);         //重要！设置系统调用总入口
}