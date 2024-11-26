//
// Created by gengke on 2024/11/21.
//

#ifndef KERNEL_HEAD_H
#define KERNEL_HEAD_H

typedef struct desc_struct {
    long a,b;
} desc_table[256];

extern desc_table gdt,idt;
extern long* pg_dir;

#endif //KERNEL_HEAD_H
