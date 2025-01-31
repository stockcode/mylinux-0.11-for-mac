/*
 *  linux/mm/memory.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * demand-loading started 01.12.91 - seems it is high on the list of
 * things wanted, and it should be easy to implement. - Linus
 * 需求加载是从 01.12.91 开始编写的 - 在程序编制表中似乎是最重要的程序，
 * 并且应该是很容易编制的 - linus
 */

/*
 * Ok, demand-loading was easy, shared pages a little bit tricker. Shared
 * pages started 02.12.91, seems to work. - Linus.
 *
 * Tested sharing by executing about 30 /bin/sh: under the old kernel it
 * would have taken more than the 6M I have free, but it worked well as
 * far as I could see.
 *
 * Also corrected some "invalidate()"s - I wasn't doing enough of them.
 *
 * OK，需求加载是比较容易编写的，而共享页面却需要有点技巧。共享页面程序是
 * 02.12.91 开始编写的，好象能够工作 - Linus。
 * 通过执行大约 30 个/bin/sh 对共享操作进行了测试:在老内核当中需要占用多于
 * 6M 的内存，而目前却不用。现在看来工作得很好。
 * 对"invalidate()"函数也进行了修正 - 在这方面我还做的不够。
 */
#include <signal.h>

#include <asm/system.h>

#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>

void do_exit(long code);        //进程退出处理函数，在kernel/exit.c，

//// 显示内存已用完出错信息，并退出。
static inline void oom(void)
{
    printk("out of memory\n\r");
    do_exit(SIGSEGV);              // do_exit()应该使用退出代码，这里用了信号值 SIGSEGV(11)
                                        // 相同值的出错码含义是“资源暂时不可用”，正好同义。
}

// 刷新页变换高速缓冲宏函数。
// 为了提高地址转换的效率，CPU 将最近使用的页表数据存放在芯片中高速缓冲中。在修改过页表
// 信息之后，就需要刷新该缓冲区。这里使用重新加载页目录基址寄存器 cr3 的方法来进行刷新。
// 下面 eax = 0，是页目录的基址。
#define invalidate() \
__asm__("movl %%eax,%%cr3"::"a" (0))       //重置 CR3 为 0，刷新“页变换高速缓存”

/* these are not to be changed without changing head.s etc */
/* 面定义若需要改动，则需要与 head.s 等文件中的相关信息一起改变 */
// linux 0.11 内核默认支持的最大内存容量是 16M，可以修改这些定义以适合更多的内存。
#define LOW_MEM 0x100000                        // 内存低端(1MB)。
#define PAGING_MEMORY (15*1024*1024)            // 分页内存 15MB。主内存区最多 15M。
#define PAGING_PAGES (PAGING_MEMORY >> 12)      // 分页后的物理内存页数。
#define MAP_NR(addr) (((addr) - LOW_MEM)>>12)   // 指定内存地址映射为页号
#define USED 100                                // 页面被占用标志


// 该宏用于判断给定地址是否位于当前进程的代码段中
#define CODE_SPACE(addr) ((((addr)+4095)&~4095) < \
current->start_code + current->end_code)

static long HIGH_MEMORY = 0;                    // 全局变量，存放实际物理内存最高端地址。

// 复制 1 页内存(4K 字节)。
#define copy_page(from,to) \
__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024))

// 内存映射字节图(1 字节代表 1 页内存)，每个页面对应的字节用于标志页面当前被引用(占用)次数。
// 它最大可以映射 15Mb 的内存空间。在初始化函数 mem_init()中，对于不能用作主内存区页面的位置
// 均都预先被设置成 USED(100)。
static unsigned char mem_map[PAGING_PAGES] = {0,};


/*
 * Get physical address of first (actually last :-) free page, and mark it
 * used. If no free pages left, return 0.
 *
 * 获取首个(实际上是最后 1 个:-)空闲页面，并标记为已使用。如果没有空闲页面，
 * 就返回0。
 */
//遍历 mem_map[]，找到主内寸中（从高地址开始）第一个空闲页面
//// 取空闲页面。如果已经没有可用内存了，则返回 0。
// 输入:%1(ax=0) - 0;%2(LOW_MEM);%3(cx=PAGING PAGES);%4(edi=mem_map+PAGING_PAGES-1)。
// 输出:返回%0(ax=页面起始地址)。
// 上面%4 寄存器实际指向 mem_map[]内存字节图的最后一个字节。本函数从字节图末端开始向前扫描
// 所有页面标志(页面总数为 PAGING_PAGES)，若有页面空闲(其内存映像字节为 0)则返回页面地址。
// 注意!本函数只是指出在主内存区的一页空闲页面，但并没有映射到某个进程的线性地址去。后面
// 的 put_page()函数就是用来作映射的。
unsigned long get_free_page(void)
{
    register unsigned long __res asm("ax");

    __asm__("std ; repne ; scasb\n\t"       //反向扫描串（mem_map[]）,al(0)与 di 不等则重复
            "jne 1f\n\t"                    //找不到空闲页，跳转到1
            "movb $1,1(%%edi)\n\t"          //将1赋值给 edi+1的位置，在 mem_map[]中，将找到0的项的引用计数置为1
            "sall $12,%%ecx\n\t"            //ecx算数左移12位，页的相对地址
            "addl %2,%%ecx\n\t"             //LOW_MEMORY+ecx，页的物理地址
            "movl %%ecx,%%edx\n\t"          // 将页面实际起始地址->edx 寄存器。
            "movl $1024,%%ecx\n\t"          // 寄存器 ecx 置计数值 1024。
            "leal 4092(%%edx),%%edi\n\t"    //将 edc+4KB的有效地址赋给 edi
            "rep ; stosl\n\t"               //将 eax（即"0"(0)）赋给 edc指向的地址，目的是页面清零
            " movl %%edx,%%eax\n"           // 将页面起始地址->eax(返回值)。
            "1: cld"
            :"=a" (__res)
            :"0" (0),"i" (LOW_MEM),"c" (PAGING_PAGES),
            "D" (mem_map+PAGING_PAGES-1)    //edx，mem_map[]的最后一个元素
            );
    return __res;                           // 返回空闲页面地址(如果无空闲也则返回 0)。
}

/*
 * Free a page of memory at physical address 'addr'. Used by
 * 'free_page_tables()'
 *
 * 释放物理地址'addr'开始的一页内存。用于函数'free_page_tables()'。
 */

//// 释放物理地址 addr 开始的一页面内存。
// 1MB 以下的内存空间用于内核程序和缓冲，不作为分配页面的内存空间。
void free_page(unsigned long addr)
{
    if (addr < LOW_MEM) return;         // 如果物理地址 addr 小于内存低端(1MB)，则返回。
    if (addr >= HIGH_MEMORY)            // 如果物理地址 addr>=内存最高端，则显示出错信息。
        panic("trying to free nonexistent page");
    addr -= LOW_MEM;                    // 物理地址减去低端内存位置，再除以 4KB，得页面号。
    addr >>= 12;
    if (mem_map[addr]--) return;        // 如果对应内存页面映射字节不等于 0，则减 1 返回。
    mem_map[addr]=0;                    // 否则置对应页面映射字节为 0，并显示出错信息，死机。
    panic("trying to free free page");
}

/*
 * This function frees a continuos block of page tables, as needed
 * by 'exit()'. As does copy_page_tables(), this handles only 4Mb blocks.
 *
 * 下面函数释放页表连续的内存块，'exit()'需要该函数。与 copy_page_tables()
 * 类似，该函数仅处理 4Mb 的内存块。
 */

//// 根据指定的线性地址和限长(页表个数)，释放对应内存页表所指定的内存块并置表项空闲。
// 页目录位于物理地址 0 开始处，共 1024 项，占 4K 字节。每个目录项指定一个页表。
// 页表从物理地址 0x1000 处开始(紧接着目录空间)，每个页表有 1024 项，也占 4K 内存。
// 每个页表项对应一页物理内存(4K)。目录项和页表项的大小均为 4 个字节。
// 参数:from - 起始基地址;size - 释放的长度。
int free_page_tables(unsigned long from,unsigned long size)
{
    unsigned long *pg_table;
    unsigned long * dir, nr;

    if (from & 0x3fffff)                    // 要释放内存块的地址需以 4M 为边界。
        panic("free_page_tables called with wrong alignment");
    if (!from)                              // 出错，试图释放内核和缓冲所占空间。
        panic("Trying to free up swapper memory space");
    // 计算所占页目录项数(4M 的进位整数倍)，也即所占页表数。
    size = (size + 0x3fffff) >> 22;
    // 下面一句计算起始目录项。对应的目录项号=from>>22，因每项占 4 字节，并且由于页目录是从
    // 物理地址 0 开始，因此实际的目录项指针=目录项号<<2，也即(from>>20)。与上 0xffc 确保
    // 目录项指针范围有效。
    dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */
    for ( ; size-->0 ; dir++) {             // size 现在是需要被释放内存的目录项数。
        if (!(1 & *dir))                    // 如果该目录项无效(P 位=0)，则继续。
            continue;                       // 目录项的位 0(P 位)表示对应页表是否存在。
        pg_table = (unsigned long *) (0xfffff000 & *dir);   // 取目录项中页表地址。
        for (nr=0 ; nr<1024 ; nr++) {       // 每个页表有 1024 个页项。
            if (1 & *pg_table)              // 若该页表项有效(P 位=1)，则释放对应内存页。
                free_page(0xfffff000 & *pg_table);
            *pg_table = 0;                  // 该页表项内容清零。
            pg_table++;                     // 指向页表中下一项。
        }
        free_page(0xfffff000 & *dir);  // 释放该页表所占内存页面。
        *dir = 0;                           // 对相应页表的目录项清零。
    }
    invalidate();                           // 刷新页变换高速缓冲。
    return 0;
}

/*
 *  Well, here is one of the most complicated functions in mm. It
 * copies a range of linerar addresses by copying only the pages.
 * Let's hope this is bug-free, 'cause this one I don't want to debug :-)
 *
 * Note! We don't copy just any chunks of memory - addresses have to
 * be divisible by 4Mb (one page-directory entry), as this makes the
 * function easier. It's used only by fork anyway.
 *
 * NOTE 2!! When from==0 we are copying kernel space for the first
 * fork(). Then we DONT want to copy a full page-directory entry, as
 * that would lead to some serious memory waste - we just copy the
 * first 160 pages - 640kB. Even that is more than we need, but it
 * doesn't take any more memory - we don't copy-on-write in the low
 * 1 Mb-range, so the pages can be shared with the kernel. Thus the
 * special case for nr=xxxx.
 *
 * 好了，下面是内存管理 mm 中最为复杂的程序之一。它通过只复制内存页面
 * 来拷贝一定范围内线性地址中的内容。希望代码中没有错误，因为我不想
* 再调试这块代码了☺。 *
* 注意!我们并不复制任何内存块 - 内存块的地址需要是 4Mb 的倍数(正好
* 一个页目录项对应的内存大小)，因为这样处理可使函数很简单。不管怎样，
* 它仅被 fork()使用(fork.c 第 56 行)。 *
* 注意 2!!当 from==0 时，是在为第一次 fork()调用复制内核空间。此时我们
* 不想复制整个页目录项对应的内存，因为这样做会导致内存严重的浪费 - 我们
* 只复制头 160 个页面 - 对应 640kB。即使是复制这些页面也已经超出我们的需求，
* 但这不会占用更多的内存 - 在低 1Mb 内存范围内我们不执行写时复制操作，所以
* 这些页面可以与内核共享。因此这是 nr=xxxx 的特殊情况(nr 在程序中指页面数)。
 */
int copy_page_tables(unsigned long from,unsigned long to,long size)
{
    unsigned long * from_page_table;
    unsigned long * to_page_table;
    unsigned long this_page;
    unsigned long * from_dir, * to_dir;
    unsigned long nr;

    /* 0x3fffff是4MB，是一个页表的管辖范围，二进制是22个1，||的两边必须同时为0，
     * 所以，from和 to后22为必须都为0，即4MB 的整数倍，意思是一个页表对应4MB 连续的线性地址空间
     * 必须从0x000000开始的4MB 的整数倍的线性地址，不能是任意地址开始的4MB，才符合分页的要求
     */
    if ((from&0x3fffff) || (to&0x3fffff))
        panic("copy_page_tables called with wrong alignment");

    /*
     * 一个页目录项的管理范围是4MB，一项是4字节，项的地址就是项数X4，也就是项管理的线性地址起始地址的 M 数，
     * 比如：0项的地址是0，管理范围是0～4MB，1项的地址是4，管理范围是：4～8MB，2项的地址是8，管理范围是8～12MB，以此类推
     * >>20就是地址的 MB 数，&0xffc就是&111111111100b，就是4MB 以下部分清零的地址的 MB 数，也就是页目录项的地址
     */
    from_dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */
    to_dir = (unsigned long *) ((to>>20) & 0xffc);
    size = ((unsigned) (size+0x3fffff)) >> 22;
    for( ; size-->0 ; from_dir++,to_dir++) {
        if (1 & *to_dir)
            panic("copy_page_tables: already exist");
        if (!(1 & *from_dir))
            continue;

        //*from_dir是页目录项中的地址，0xfffff000&是将低12位清零，高20位是页表的地址
        from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
        if (!(to_page_table = (unsigned long *) get_free_page()))
            return -1;	/* Out of memory, see freeing */
        *to_dir = ((unsigned long) to_page_table) | 7; //7即111
        nr = (from==0)?0xA0:1024;                       //0xA0即160，复制页表的项数
        for ( ; nr-- > 0 ; from_page_table++,to_page_table++) { //复制父进程页表
            this_page = *from_page_table;
            if (!(1 & this_page))
                continue;
            this_page &= ~2;                            //设置页表属性，2是010，～2是101，代表用户、只读、存在
            *to_page_table = this_page;
            if (this_page > LOW_MEM) {                  //1MB 以内的内核区不参与用户分页管理
                *from_page_table = this_page;           //令源页表项也只读
                this_page -= LOW_MEM;
                this_page >>= 12;
                mem_map[this_page]++;                   //增加引用计数，参看 mem_init
            }
        }
    }
    invalidate();                                       //重置 CR3 为 0，刷新“页变换高速缓存”
    return 0;
}

/*
 * This function puts a page in memory at the wanted address.
 * It returns the physical address of the page gotten, 0 if
 * out of memory (either when trying to access page-table or
 * page.)
 *
 * 下面函数将一内存页面放置(映射)到指定线性地址处。它返回页面的物理地址，如果
 * 内存不够(在访问页表或页面时)，则返回 0。
 */
//// 把一物理内存页面映射到指定的线性地址处。
// 主要工作是在页目录和页表中设置指定页面的信息。若成功则返回页面地址。
// 在缺页异常的 C 函数 do_no_page()中会调用此函数。对于缺页引起的异常，由于任何缺页缘故而
// 对页表作修改时，并不需要刷新 CPU 的页变换缓冲(或称 Translation Lookaside Buffer，TLB)，
// 即使页表项中标志 P 被从 0 修改成 1。因为无效页项不会被缓冲，因此当修改了一个无效的页表项
// 时不需要刷新。在此就表现为不用调用 Invalidate()函数。
// 参数 page 是分配的主内存区中页面(页帧，页框)的指针;address 是线性地址。
unsigned long put_page(unsigned long page,unsigned long address)
{
    unsigned long tmp, *page_table;

/* NOTE !!! This uses the fact that _pg_dir=0 */
/* 注意!!!这里使用了页目录基址_pg_dir=0 的条件 */

    // 如果申请的页面位置低于 LOW_MEM(1MB)或超出系统实际含有内存高端 HIGH_MEMORY，则发出警告。
    // LOW_MEM 是主内存区可能有的最小起始位置(当系统物理内存小于或等于 6MB 时)。
    if (page < LOW_MEM || page >= HIGH_MEMORY)
        printk("Trying to put page %p at %p\n",page,address);
    // 如果申请的页面在内存页面映射字节图中没有置位，则显示警告信息。
    if (mem_map[(page-LOW_MEM)>>12] != 1)
        printk("mem_map disagrees with %p at %p\n",page,address);
    // 计算指定地址在页目录表中对应的目录项指针。
    page_table = (unsigned long *) ((address>>20) & 0xffc);
    // 如果该目录项有效(P=1)(也即指定的页表在内存中)，则从中取得指定页表的地址->page_table。
    if ((*page_table)&1)
        page_table = (unsigned long *) (0xfffff000 & *page_table);
    else {
        // 否则，申请空闲页面给页表使用，并在对应目录项中置相应标志 7(User, U/S, R/W)。然后将
        // 该页表的地址->page_table。
        if (!(tmp=get_free_page()))
            return 0;
        *page_table = tmp|7;
        page_table = (unsigned long *) tmp;
    }
    // 在页表中设置指定地址的物理内存页面的页表项内容。每个页表共可有 1024 项(0x3ff)。
    page_table[(address>>12) & 0x3ff] = page | 7;
/* no need for invalidate */
/* 不需要刷新页变换高速缓冲 */
    return page;            // 返回页面地址。
}

//// 取消写保护页面函数。用于页异常中断过程中写保护异常的处理(写时复制)。
// 输入参数为页表项指针，是线性地址。
// [ un_wp_page 意思是取消页面的写保护:Un-Write Protected。]
// 在程序创建进程时，新进程与原进程共享代码和数据内存页面。而当新进程或原进程需要向内存
// 页面写数据时 CPU 就会检测到这个情况并产生页面写保护异常。于是在这个函数中内核就会首先
// 判断要写的页面是否被共享。若是则需要重新申请一新页面给写进程单独使用。共享被取消。
void un_wp_page(unsigned long * table_entry)
{
    unsigned long old_page,new_page;

    old_page = 0xfffff000 & *table_entry;   // 取指定页表项内物理页面位置(地址)。

    // 如果原页面地址大于内存低端 LOW_MEM(1Mb)，并且其在页面映射字节图数组中值为 1(表示仅
    // 被引用 1 次，页面没有被共享)，则在该页面的页表项中置 R/W 标志(可写)，并刷新页变换
    // 高速缓冲，然后返回。
    // 判断原页面地址是否大于内存低端的作用是看原页面是否在主内存区中。如果该内存页面此时
    // 只被一个进程使用,就直接把属性改为可写即可，不用再重新新申请一个新页面。
    if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)]==1) {
        *table_entry |= 2;
        invalidate();
        return;
    }

    // 否则，在主内存区内申请一页空闲页面。
    if (!(new_page=get_free_page()))
        oom();                          // Out of Memory。内存不够处理。

    // 如果原页面大于内存低端(则意味着 mem_map[]>1，页面是共享的)，则将原页面的页面映射
    // 数组值递减 1。然后将指定页表项内容更新为新页面的地址，并置可读写等标志(U/S, R/W, P)。
    // 刷新页变换高速缓冲。最后将原页面内容复制到新页面。
    if (old_page >= LOW_MEM)
        mem_map[MAP_NR(old_page)]--;
    *table_entry = new_page | 7;
    invalidate();
    copy_page(old_page,new_page);
}

/*
 * This routine handles present pages, when users try to write
 * to a shared page. It is done by copying the page to a new address
 * and decrementing the shared-page counter for the old page.
 *
 * If it's in code space we exit with a segment error.
 *
 * 当用户试图往一个共享页面上写时，该函数处理已存在的内存页面，(写时复制)
 * 它是通过将页面复制到一个新地址上并递减原页面的共享页面计数值实现的。
 * 如果它在代码空间，我们就以段错误信息退出。
 */

//// 页异常中断处理调用的 C 函数。写共享页面处理函数。在 page.s 程序中被调用。
// 参数 error_code 是由 CPU 自动产生，address 是页面线性地址。
// 写共享页面时，需复制页面(写时复制)。
void do_wp_page(unsigned long error_code,unsigned long address)
{
#if 0
    /* we cannot do this yet: the estdio library writes to code space */
/* stupid, stupid. I really want the libc.a from GNU */
/* 我们现在还不能这样做:因为 estdio 库会在代码空间执行写操作 */
/* 真是太愚蠢了。我真想从 GNU 得到 libc.a 库。*/
	if (CODE_SPACE(address))                // 如果地址位于代码空间，则终止执行程序。
		do_exit(SIGSEGV);
#endif
    // 处理取消页面保护。参数指定页面在页表中的页表项指针，其计算方法是:
    // ((address>>10) & 0xffc):计算指定地址的页面在页表中的偏移地址;
    // (0xfffff000 & *((address>>20) &0xffc)):取目录项中页表的地址值，
    // 其中((address>>20) &0xffc)计算页面所在页表的目录项指针;
    // 两者相加即得指定地址对应页面的页表项指针。这里对共享的页面进行复制。
    un_wp_page((unsigned long *)
                       (((address>>10) & 0xffc) + (0xfffff000 &
                                                   *((unsigned long *) ((address>>20) &0xffc)))));

}

//// 写页面验证。
// 若页面不可写，则复制页面。在 fork.c 第 34 行被调用。
void write_verify(unsigned long address)
{
    unsigned long page;

    // 判断指定地址所对应页目录项的页表是否存在(P)，若不存在(P=0)则返回。
    if (!( (page = *((unsigned long *) ((address>>20) & 0xffc)) )&1))
        return;

    // 取页表的地址，加上指定地址的页面在页表中的页表项偏移值，得对应物理页面的页表项指针。
    page &= 0xfffff000;
    page += ((address>>10) & 0xffc);
    // 如果该页面不可写(标志 R/W 没有置位)，则执行共享检验和复制页面操作(写时复制)。
    if ((3 & *(unsigned long *) page) == 1)  /* non-writeable, present */
        un_wp_page((unsigned long *) page);
    return;
}

//// 取得一页空闲内存并映射到指定线性地址处。
// 与 get_free_page()不同。get_free_page()仅是申请取得了主内存区的一页物理内存。而该函数
// 不仅是获取到一页物理内存页面，还进一步调用 put_page()，将物理页面映射到指定的线性地址处。
void get_empty_page(unsigned long address)
{
    unsigned long tmp;

    // 若不能取得一空闲页面，或者不能将页面放置到指定地址处，则显示内存不够的信息。
    // 下面英文注释的含义是:free_page()函数的参数是 0(tmp 是 0)也没有关系，该函数会忽
    // 略它并正常返回。
    if (!(tmp=get_free_page()) || !put_page(tmp,address)) {
        free_page(tmp);		/* 0 is ok - ignored */
        oom();
    }
}

/*
 * try_to_share() checks the page at address "address" in the task "p",
 * to see if it exists, and if it is clean. If so, share it with the current
 * task.
 *
 * NOTE! This assumes we have checked that p != current, and that they
 * share the same executable.
 */
static int try_to_share(unsigned long address, struct task_struct * p)
{
    unsigned long from;
    unsigned long to;
    unsigned long from_page;
    unsigned long to_page;
    unsigned long phys_addr;

    from_page = to_page = ((address>>20) & 0xffc);
    from_page += ((p->start_code>>20) & 0xffc);
    to_page += ((current->start_code>>20) & 0xffc);
/* is there a page-directory at from? */
    from = *(unsigned long *) from_page;
    if (!(from & 1))
        return 0;
    from &= 0xfffff000;
    from_page = from + ((address>>10) & 0xffc);
    phys_addr = *(unsigned long *) from_page;
/* is the page clean and present? */
    if ((phys_addr & 0x41) != 0x01)
        return 0;
    phys_addr &= 0xfffff000;
    if (phys_addr >= HIGH_MEMORY || phys_addr < LOW_MEM)
        return 0;
    to = *(unsigned long *) to_page;
    if (!(to & 1)) {
        if ((to = get_free_page()))
            *(unsigned long *) to_page = to | 7;
        else
            oom();
    }
    to &= 0xfffff000;
    to_page = to + ((address>>10) & 0xffc);
    if (1 & *(unsigned long *) to_page)
        panic("try_to_share: to_page already exists");
/* share them: write-protect */
    *(unsigned long *) from_page &= ~2;
    *(unsigned long *) to_page = *(unsigned long *) from_page;
    invalidate();
    phys_addr -= LOW_MEM;
    phys_addr >>= 12;
    mem_map[phys_addr]++;
    return 1;
}

/*
 * share_page() tries to find a process that could share a page with
 * the current one. Address is the address of the wanted page relative
 * to the current data space.
 *
 * We first check if it is at all feasible by checking executable->i_count.
 * It should be >1 if there are other tasks sharing this inode.
 */
static int share_page(unsigned long address)
{
    struct task_struct ** p;

    if (!current->executable)
        return 0;
    if (current->executable->i_count < 2)
        return 0;
    for (p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
        if (!*p)
            continue;
        if (current == *p)
            continue;
        if ((*p)->executable != current->executable)
            continue;
        if (try_to_share(address,*p))
            return 1;
    }
    return 0;
}

//// 页异常中断处理调用的函数。处理缺页异常情况。在 page.s 程序中被调用。
// 参数 error_code 是由 CPU 自动产生，address 是页面线性地址。
void do_no_page(unsigned long error_code,unsigned long address)
{
    int nr[4];
    unsigned long tmp;
    unsigned long page;
    int block,i;

    address &= 0xfffff000;                  // 页面地址。
    // 首先算出指定线性地址在进程空间中相对于进程基址的偏移长度值。
    tmp = address - current->start_code;

    // 若当前进程的 executable 空，或者指定地址超出代码+数据长度，则申请一页物理内存，并映射
    // 到指定的线性地址处。executable 是进程的 i 节点结构。如果该值为 0，表明进程刚开始设置，
    // 需要内存;而指定的线性地址超出代码加数据长度，表明进程在申请新的内存空间，也需要给予。
    // 因此就直接调用 get_empty_page()函数，申请一页物理内存并映射到指定线性地址处即可。
    // start_code 是进程代码段地址，end_data 是代码加数据长度。对于 Linux 内核，它的代码段和
    // 数据段是起始基址是相同的。
    if (!current->executable || tmp >= current->end_data) {
        get_empty_page(address);
        return;
    }
    // 如果尝试共享页面成功，则退出。
    if (share_page(tmp))
        return;
    // 取空闲页面，如果内存不够了，则显示内存不够，终止进程。
    if (!(page = get_free_page()))
        oom();
/* remember that 1 block is used for header */
/* 记住，(程序)头要使用 1 个数据块 */

    // 因为块设备上存放的执行文件映像第 1 块数据是程序头，因此在读取该文件时需要跳过第 1 块数据。
    // 首先计算缺页所在的数据块项。BLOCK_SIZE = 1024 字节，因此一页内存需要 4 个数据块。
    block = 1 + tmp/BLOCK_SIZE;
    // 根据 i 节点信息，取数据块在设备上的对应的逻辑块号。
    for (i=0 ; i<4 ; block++,i++)
        nr[i] = bmap(current->executable,block);
    // 读设备上一个页面的数据(4 个逻辑块)到指定物理地址 page 处。
    bread_page(page,current->executable->i_dev,nr);
    // 在增加了一页内存后，该页内存的部分可能会超过进程的 end_data 位置。下面的循环即是对物理
    // 页面超出的部分进行清零处理。
    i = tmp + 4096 - current->end_data;
    tmp = page + 4096;
    while (i-- > 0) {
        tmp--;
        *(char *)tmp = 0;
    }
    // 如果把物理页面映射到指定线性地址的操作成功，就返回。否则就释放内存页，显示内存不够。
    if (put_page(page,address))
        return;
    free_page(page);
    oom();
}

//// 物理内存初始化。
// 参数:start_mem - 可用作分页处理的物理内存起始位置(已去除 RAMDISK 所占内存空间等)。
// end_mem - 实际物理内存最大地址。
// 在该版的 Linux 内核中，最多能使用 16Mb 的物理内存，大于 16Mb 的内存将不于考虑，弃置不用。
// 对于具有 16Mb 物理内存的系统，在没有 RAMDISK 的情况下 start_mem 通常是 4Mb，end_mem 是 16Mb。
// 0 - 1Mb 内存空间用于内核系统(其实是 0-640Kb)。
void mem_init(long start_mem, long end_mem)
{
    int i;

    HIGH_MEMORY = end_mem;              // 设置内存最高端。
    // 首先置对应从 1Mb 到 16Mb 范围的所有页面为已占用状态，即将页面映射数组全置成 USED(100)。
    for (i=0 ; i<PAGING_PAGES ; i++)
        mem_map[i] = USED;

    // 然后计算可使用起始内存页面号 i 和可作为主内存区进行分页处理的内存大小。从而计算出可用于
    // 分页处理的页面数。
    i = MAP_NR(start_mem);      // 得到可用起始页面号。
    end_mem -= start_mem;
    end_mem >>= 12;             // 得到可用于分页处理的页面数。

    // 最后将这些可用页面对应的页面映射数组项清零。对于有 16Mb 的系统，mem_map[]中对应 4Mb-16Mb
    // 的项被清零。
    while (end_mem-->0)
        mem_map[i++]=0;
}

// 计算内存空闲页面数并显示。
void calc_mem(void)
{
    int i,j,k,free=0;
    long * pg_tbl;

    // 扫描内存页面映射数组 mem_map[]，获取空闲页面数并显示。
    for(i=0 ; i<PAGING_PAGES ; i++)
        if (!mem_map[i]) free++;
    printk("%d pages free (of %d)\n\r",free,PAGING_PAGES);

    // 扫描所有页目录项(除 0，1 项)，如果页目录项有效，则统计对应页表中有效页面数，并显示。
    for(i=2 ; i<1024 ; i++) {
        if (1&pg_dir[i]) {
            pg_tbl=(long *) (0xfffff000 & pg_dir[i]);
            for(j=k=0 ; j<1024 ; j++)
                if (pg_tbl[j]&1)
                    k++;
            printk("Pg-dir[%d] uses %d pages\n",i,k);
        }
    }
}