#ifndef _MM_H
#define _MM_H

#define PAGE_SIZE 4096
unsigned long __get_free_page();
unsigned long get_free_page();
unsigned long free_page(unsigned long addr);
#endif