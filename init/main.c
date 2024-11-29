#include<klib.h>
#include<linux/sched.h>
#include<asm/system.h>
#include<unistd.h>
#include<stdarg.h>
#include<linux/tty.h>
#include<linux/kernel.h>

static char printbuf[1024];

extern int vsprintf();
extern void init(void);

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
    memory_end = (1<<20) + (EXT_MEM_K << 10);
    memory_end = memory_end & 0xfffff000;

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

    trap_init();
    tty_init();
    sched_init();
    sti();
    move_to_user_mode();

    printk("init finished...");
    while (1);
    if (!fork()) {		/* we count on this going ok */
        init();
    }
    /*
     *   NOTE!!   For any other task 'pause()' would mean we have to get a
     * signal to awaken, but task0 is the sole exception (see 'schedule()')
     * as task 0 gets activated at every idle moment (when no other tasks
     * can run). For task0 'pause()' just means we go check if some other
     * task can run, and if not we return here.
     */
    for(;;); // pause();
}

int printf(const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);
    write(1,printbuf,i=vsprintf(printbuf, fmt, args));
    va_end(args);
    return i;
}

void init(void) {
}

//    int pid,i;
//
//    disp_str("Free mem: %d bytes\n\r");
//    disp_int(memory_end-main_memory_start);
//    if (!(pid=fork())) {
//        close(0);
//        if (open("/etc/rc",O_RDONLY,0))
//            _exit(1);
//        execve("/bin/sh",argv_rc,envp_rc);
//        _exit(2);
//    }
//    if (pid>0)
//        while (pid != wait(&i))
//            /* nothing */;
//    while (1) {
//        if ((pid=fork())<0) {
//            printf("Fork failed in init\r\n");
//            continue;
//        }
//        if (!pid) {
//            close(0);close(1);close(2);
//            setsid();
//            (void) open("/dev/tty0",O_RDWR,0);
//            (void) dup(0);
//            (void) dup(0);
//            _exit(execve("/bin/sh",argv,envp));
//        }
//        while (1)
//            if (pid == wait(&i))
//                break;
//        printf("\n\rchild %d died with code %04x\n\r",pid,i);
//        sync();
//    }
//    _exit(0);	/* NOTE! _exit, not exit() */
//}