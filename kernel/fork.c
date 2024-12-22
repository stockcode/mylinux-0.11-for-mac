//
// Created by gengke on 2024/11/23.
//
#include<linux/sched.h>
#include<errno.h>
#include<asm/system.h>
#include<linux/mm.h>
#include<linux/kernel.h>

extern void write_verify(unsigned long address);

long last_pid=0;

void verify_area(void * addr,int size)
{
    unsigned long start;

    start = (unsigned long) addr;
    size += start & 0xfff;
    start &= 0xfffff000;
    start += get_base(current->ldt[2]);
    while (size>0) {
        size -= 4096;
        write_verify(start);
        start += 4096;
    }
}

int copy_mem(int nr,struct task_struct * p)
{
    //局部描述符 LDT 赋值
    unsigned long old_data_base,new_data_base,data_limit;
    unsigned long old_code_base,new_code_base,code_limit;

    //取子进程的代码、数据段限长
    code_limit=get_limit(0x0f); //0x0f即1111：代码段、LDT、3特权级
    data_limit=get_limit(0x17); //0x17即10111：数据段、LDT、3特权级

    //获取父进程（现在是进程0）的代码段、数据段基址
    old_code_base = get_base(current->ldt[1]);
    old_data_base = get_base(current->ldt[2]);
    if (old_data_base != old_code_base)
        panic("We don't support separate I&D");
    if (data_limit < code_limit)
        panic("Bad data_limit");
    new_data_base = new_code_base = nr * 0x4000000; //现在 nr是1，0x4000000是64MB
    p->start_code = new_code_base;
    set_base(p->ldt[1],new_code_base);      //设置子进程代码段基址
    set_base(p->ldt[2],new_data_base);      //设置子进程数据段基址

    //为进程1创建第一个页表、复制进程0的页表，设置进程1的页目录项
    if (copy_page_tables(old_data_base,new_data_base,data_limit)) {
        printk("free_page_tables: from copy_mem\n");
        free_page_tables(new_data_base,data_limit);
        return -ENOMEM;
    }
    return 0;
}

/*
 *  Ok, this is the main fork-routine. It copies the system process
 * information (task[nr]) and sets up the necessary registers. It
 * also copies the data segment in it's entirety.
 */
int copy_process(int nr,long ebp,long edi,long esi,long gs,long none,
                 long ebx,long ecx,long edx,
                 long fs,long es,long ds,
                 long eip,long cs,long eflags,long esp,long ss)
//这些参数是 int 0x80、system_call、sys_fork多次累积压栈的结果，顺序是完全一致的
{
    struct task_struct *p;
    int i;
    struct file *f;

    //在16MB 内存的最高端获取一页，强制类型转换的潜台词是将这个页当 task_union用
    p = (struct task_struct *) get_free_page();
    if (!p)
        return -EAGAIN;
    task[nr] = p;       //此时的 nr就是1

    // NOTE!: the following statement now work with gcc 4.3.2 now, and you
    // must compile _THIS_ memcpy without no -O of gcc.#ifndef GCC4_3
    /*
     * current指向当前进程的 task_struct的指针，当前进程是进程0。
     * 下面这行的意思：将父进程的 task_struct赋给子进程。这是父子进程创建机制的重要体现。
     * 这行代码执行后，父子进程的 task_struct将完全一样。
     * 重要！！注意指针类型，只复制 task_struct，并未将4KB都复制，即进程0的内核栈并未复制
     */
    *p = *current;	/* NOTE! this doesn't copy the supervisor stack */
    p->state = TASK_UNINTERRUPTIBLE;    //只有内核代码中明确表示将该进程设置为就绪状态才能被唤醒
                                        //除此之外，没有任何办法将其唤醒
    p->pid = last_pid;                  //开始子进程的个性化设置
    p->father = current->pid;
    p->counter = p->priority;
    p->signal = 0;
    p->alarm = 0;
    p->leader = 0;		/* process leadership doesn't inherit */
    p->utime = p->stime = 0;
    p->cutime = p->cstime = 0;
    p->start_time = jiffies;

    p->tss.back_link = 0;               //开始设置子进程的 TSS
    p->tss.esp0 = PAGE_SIZE + (long) p; //esp0 是内核栈的栈顶指针
    p->tss.ss0 = 0x10;                  //0x10就是10000，0特权级，GDT，数据段
    p->tss.eip = eip;                   //重要！就是参数的 EIP，是 int 0x80压栈的，指向得是：if (__res >=0)
    p->tss.eflags = eflags;
    p->tss.eax = 0;                     //重要！决定 main()函数中 if(!fork())后面的分支走向
    p->tss.ecx = ecx;
    p->tss.edx = edx;
    p->tss.ebx = ebx;
    p->tss.esp = esp;
    p->tss.ebp = ebp;
    p->tss.esi = esi;
    p->tss.edi = edi;
    p->tss.es = es & 0xffff;
    p->tss.cs = cs & 0xffff;
    p->tss.ss = ss & 0xffff;
    p->tss.ds = ds & 0xffff;
    p->tss.fs = fs & 0xffff;
    p->tss.gs = gs & 0xffff;
    p->tss.ldt = _LDT(nr);              //挂接子进程的 LDT
    p->tss.trace_bitmap = 0x80000000;

    if (last_task_used_math == current)
        __asm__("clts ; fnsave %0"::"m" (p->tss.i387));

    if (copy_mem(nr,p)) {               //设置子进程的代码段、数据段及创建、复制子进程的第一个页表
        task[nr] = NULL;                //现在不会出现这种情况
        free_page((long) p);
        return -EAGAIN;
    }

    for (i=0; i<NR_OPEN;i++)            //下面将父进程相关文件属性的引用计数加1，表明父子进程共享文件
        if ((f=p->filp[i]))
            f->f_count++;
    if (current->pwd)
        current->pwd->i_count++;
    if (current->root)
        current->root->i_count++;
    if (current->executable)
        current->executable->i_count++;
    set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY,&(p->tss));
    set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&(p->ldt));
    p->state = TASK_RUNNING;	/* do this last, just in case */
    return last_pid;
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