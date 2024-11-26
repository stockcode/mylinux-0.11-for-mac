//
// Created by gengke on 2024/11/26.
//

#ifndef KERNEL_SYS_H
#define KERNEL_SYS_H

#include<linux/sched.h>

typedef int (*fn_ptr)();
#define nr_sys_calls 72

extern int sys_fork();
fn_ptr sys_call_table[nr_sys_calls]={NULL,NULL, sys_fork};

#endif //KERNEL_SYS_H
