#include<linux/mm.h>
#include<linux/sched.h>
#include<asm/system.h>
#include<asm/io.h>
#include<linux/head.h>
#include<klib.h>
#include<linux/sys.h>

#define HZ 100
#define LATCH (1193180/HZ)

extern void timer_interrupt();

extern void system_call();

unsigned long user_stack[PAGE_SIZE>>2];
unsigned long user_stack_top=(unsigned long)&user_stack[1024];

union task_union{
    struct task_struct task;
    char stack[PAGE_SIZE];
};
static union task_union init_task={INIT_TASK,};

struct task_struct* task[NR_TASKS] = {&(init_task.task),};
struct task_struct* current = &(init_task.task);

extern int count;
void do_timer() {
//    disp_str("\nIn timer interrupt");
//    count++;
//    if (count > 100) {
//        count = 0;
//        clear();
//    }
//    for (int i = 0; i < 10000; i++);
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