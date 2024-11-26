//
// Created by gengke on 2024/11/23.
//

#ifndef KERNEL_STDARG_H
#define KERNEL_STDARG_H

#define STACK_SIZE 4

#ifndef NULL
#define NULL (char*)0
#endif

typedef char* va_list;

#define va_start(AP,n) (AP=((char*)&n + STACK_SIZE))
#define va_arg(AP,TYPE) (AP+=STACK_SIZE, *((TYPE*)(AP - STACK_SIZE)))
#define va_end (AP=NULL)
#endif //KERNEL_STDARG_H
