//
// Created by gengke on 2024/11/26.
//

#ifndef KERNEL_KERNEL_H
#define KERNEL_KERNEL_H

void panic(const char * str);
int printf(const char * fmt, ...);
int printk(const char * fmt, ...);
int tty_write(unsigned ch,char * buf,int count);

#endif //KERNEL_KERNEL_H
