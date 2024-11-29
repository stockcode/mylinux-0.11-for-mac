#include<linux/mm.h>
#include<linux/sched.h>
#include<linux/kernel.h>
#include<asm/system.h>
#include<asm/io.h>
#include<linux/head.h>
#include<klib.h>
#include<linux/sys.h>

void show_task(int nr,struct task_struct * p)
{
    int i,j = 4096-sizeof(struct task_struct);

    printk("%d: pid=%d, state=%d, ",nr,p->pid,p->state);
    i=0;
    while (i<j && !((char *)(p+1))[i])
        i++;
    printk("%d (of %d) chars free in kernel stack\n\r",i,j);
}

void show_stat(void)
{
    int i;

    for (i=0;i<NR_TASKS;i++)
        if (task[i])
            show_task(i,task[i]);
}

extern void timer_interrupt();

extern void system_call();

unsigned long user_stack[PAGE_SIZE>>2];
unsigned long user_stack_top=(unsigned long)&user_stack[1024];

union task_union{
    struct task_struct task;
    char stack[PAGE_SIZE];
};
static union task_union init_task={INIT_TASK,};

long volatile jiffies=0;
long startup_time=0;
struct task_struct* task[NR_TASKS] = {&(init_task.task),};
struct task_struct* current = &(init_task.task);

extern int count;
void do_timer() {
    schedule();
}

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
    if (*p && *p != current) {
        (**p).state=0;
        goto repeat;
    }
    *p=NULL;
    if (tmp)
        tmp->state=0;
}

void wake_up(struct task_struct **p)
{
    if (p && *p) {
        (**p).state=0;
        *p=NULL;
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
 */
void schedule(void)
{
    int i,next,c;
    struct task_struct ** p;

/* this is the scheduler proper: */

    while (1) {
        c = -1;
        next = 0;
        i = NR_TASKS;
        p = &task[NR_TASKS];
        while (--i) {
            if (!*--p)
                continue;
            if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
                c = (*p)->counter, next = i;
        }
        if (c) break;
        for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
            if (*p)
                (*p)->counter = ((*p)->counter >> 1) +
                                (*p)->priority;
    }
    switch_to(next);
}


void sched_init()
{
    int i;
    struct desc_struct* p;

    set_tss_desc((char*)(gdt+FIRST_TSS_ENTRY), &init_task.task.tss);
    set_ldt_desc((char*)(gdt+FIRST_LDT_ENTRY), &init_task.task.ldt);

    p=gdt+2+FIRST_TSS_ENTRY;
    for(i=1;i<NR_TASKS;i++){
        task[i]=NULL;
        p->a=p->b=0;
        p++;
        p->a=p->b=0;
        p++;
    }


    __asm__("pushfl;andl $0xffffbfff,(%esp);popfl");
    lldt(0);
    ltr(0);

    outb_p(0x36,0x43);		/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */
	outb(LATCH >> 8 , 0x40);	/* MSB */
    set_intr_gate(0x20,&timer_interrupt);
    outb(inb_p(0x21)&~0x01,0x21);
    set_system_gate(0x80,&system_call);
}