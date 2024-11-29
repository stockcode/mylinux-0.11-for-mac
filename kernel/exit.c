#include<linux/sched.h>

int do_exit(long code)
{
    int i;
    schedule();
    return (-1);	/* just to suppress warnings */
}