//
// Created by gengke on 2024/11/23.
//
#include<linux/sched.h>
#include<errno.h>
#include<asm/system.h>
#include<linux/mm.h>
#include<klib.h>

long last_pid=0;
int find_empty_process(void);

int sys_fork() {

    struct task_struct *p = (struct task_struct *) get_free_page();
    if (!p) {
        disp_str("memory out");
        return -ENOMEM;
    }
    disp_str("\nfree_page_index:");
    disp_int((int) p);
//    int *ebp;
//    // 获取当前函数栈帧指针（这是一种简化的假设，实际情况可能更复杂）
//    __asm__("movl %%ebp, %0" : "=r"(ebp));
//    // 假设第一个参数位于栈帧指针偏移量为8字节的位置（这是非常简化的假设）
//
//    int nr;
////    *tsk = *current;
//    nr=find_empty_process();
//    if(nr<0)
//        return -1;
//
//    tsk->pid = last_pid;
//    tsk->tss.eax = 0;
//    tsk->tss.cs=0xf;
//    tsk->tss.eip= *(ebp+1);
//    tsk->tss.ds=0x17;
//    tsk->tss.es=0x17;
//    tsk->tss.fs=0x17;
//    tsk->tss.ss=0x17;
//    tsk->tss.gs=0x1b;
//    tsk->tss.ldt=_LDT(nr);
//    tsk->tss.trace_bitmap=0x80000000;
//    tsk->tss.esp = userstack;
//
//    set_tss_desc((char*)(gdt+FIRST_TSS_ENTRY+(nr<<1)), &(tsk->tss));
//    set_ldt_desc((char*)(gdt+FIRST_LDT_ENTRY+(nr<<1)), &(tsk->ldt));
//
//    tsk->state= TASK_RUNNING;
//    task[nr]=tsk;
//    return last_pid;
    disp_str("\nin fork...");
    return 1;
}

int find_empty_process(void)
{
	int i;

	repeat:
		if ((++last_pid)<0) last_pid=1;
		for(i=0 ; i<NR_TASKS ; i++)
			if (task[i] && task[i]->pid == last_pid) goto repeat;
	for(i=1 ; i<NR_TASKS ; i++)
		if (!task[i])
			return i;
	return -EAGAIN;
}