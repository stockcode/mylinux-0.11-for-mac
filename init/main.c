#include<klib.h>
#include<linux/sched.h>
#include<asm/system.h>
#include<unistd.h>

extern void mem_init(long start_mem, long end_mem);
extern void trap_init();
int count=0;
int a=10;

#define EXT_MEM_K (*(unsigned short *)0x90002)
static long memory_end;
static long buffer_memory_end;
static long main_memory_start;

_syscall0(int,fork);

int kernel_start()
{
    int child;
    int result=0;
    clear();
    memory_end = (1<<20) + (EXT_MEM_K << 10);
    memory_end = memory_end & 0xfffff000;
    disp_str("mem_size:");
    disp_int(memory_end);

    memory_end = (1<<20) + (EXT_MEM_K<<10);
    memory_end &= 0xfffff000;
    if (memory_end > 16*1024*1024)
        memory_end = 16*1024*1024;
    if (memory_end > 12*1024*1024)
        buffer_memory_end = 4*1024*1024;
    else if (memory_end > 6*1024*1024)
        buffer_memory_end = 2*1024*1024;
    else
        buffer_memory_end = 1*1024*1024;
    main_memory_start = buffer_memory_end;
    mem_init(main_memory_start, memory_end);

    disp_str("\nHello World.");
    sched_init();
    trap_init();
    sti();
    move_to_user_mode();
    child = fork();

    if (!child) {
        disp_str("\nI am the child!");
    }
    while(1)
    {
        disp_str("\n I am the parent!");
        for(;;);
        count++;
        if(count>100) {
            count=0;
            clear();
        }
        for(int i=0;i<1000000;i++);
    }
    return 0;
}