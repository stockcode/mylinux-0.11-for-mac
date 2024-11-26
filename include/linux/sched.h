//
// Created by gengke on 2024/11/21.
//

#ifndef KERNEL_SCHED_H
#define KERNEL_SCHED_H
#include <linux/head.h>

#define NR_TASKS 64
#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)


#define _TSS(n) ((((unsigned long)n)<<4)+(FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long)n)<<4)+(FIRST_LDT_ENTRY<<3))
#define lldt(n) __asm__("lldt %%ax"::"a"(_LDT(n)))
#define ltr(n) __asm__("ltr %%ax"::"a"(_TSS(n)))

#define TASK_RUNNING 0
#define TASK_STOPPED 4

#ifndef NULL
#define NULL ((void *) 0)
#endif

struct tss_struct {
	long	back_link;	/* 16 high bits zero */
	long	esp0;
	long	ss0;		/* 16 high bits zero */
	long	esp1;
	long	ss1;		/* 16 high bits zero */
	long	esp2;
	long	ss2;		/* 16 high bits zero */
	long	cr3;
	long	eip;
	long	eflags;
	long	eax,ecx,edx,ebx;
	long	esp;
	long	ebp;
	long	esi;
	long	edi;
	long	es;		/* 16 high bits zero */
	long	cs;		/* 16 high bits zero */
	long	ss;		/* 16 high bits zero */
	long	ds;		/* 16 high bits zero */
	long	fs;		/* 16 high bits zero */
	long	gs;		/* 16 high bits zero */
	long	ldt;		/* 16 high bits zero */
	long	trace_bitmap;	/* bits: trace 0, bitmap 16-31 */
};

struct task_struct {
    long pid;
    long counter;
    long priority;
    long state;
    struct desc_struct ldt[3];
    struct tss_struct tss;
};

#define INIT_TASK \
{  \
    0,15,15,0, \
    /*ldt*/{   \
        {0,0},  \
        {0xfff,0x00c0fa00}, \
        {0xfff,0x00c0f200}, \
    },            \
    /*tss*/	{     \
    0,(long)&init_task+PAGE_SIZE,0x10,0,0,0,0,(long)&pg_dir,\
	 0,0,0,0,0,0,0,0, \
	 0,0,0,0,0,0,0,0, \
	 _LDT(0),0x80000000, \
	}, \
}

/*
 *	switch_to(n) should switch tasks to task nr n, first
 * checking that n isn't the current task, in which case it does nothing.
 * This also clears the TS-flag if the task we switched to has used
 * tha math co-processor latest.
 */
#define switch_to(n) {\
struct {long a,b;} __tmp; \
__asm__("cmpl %%ecx,current\n\t" \
	"je 1f\n\t" \
	"movw %%dx,%1\n\t" \
	"xchgl %%ecx,current\n\t" \
	"ljmp *%0\n\t" \
	"1:" \
	::"m" (*&__tmp.a),"m" (*&__tmp.b), \
	"d" (_TSS(n)),"c" ((long) task[n])); \
}

extern struct task_struct* task[NR_TASKS];

extern void sched_init();

extern struct task_struct* current;
#endif //KERNEL_SCHED_H
