/*
 *  linux/fs/exec.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * #!-checking implemented by tytso.
 * 开始的程序检测部分是由 tytso 实现的。
 */

/*
 * Demand-loading implemented 01.12.91 - no need to read anything but
 * the header into memory. The inode of the executable is put into
 * "current->executable", and page faults do the actual loading. Clean.
 *
 * Once more I can proudly say that linux stood up to being changed: it
 * was less than 2 hours work to get demand-loading completely implemented.
 *
 * 需求时加载是于 1991.12.1 实现的 - 只需将执行文件头部分读进内存而无须
 * 将整个执行文件都加载进内存。执行文件的 i 节点被放在当前进程的可执行字段中
 * ("current->executable")，而页异常会进行执行文件的实际加载操作以及清理工作。
 * 我可以再一次自豪地说，linux 经得起修改:只用了不到 2 小时的工作时间就完全
 * 实现了需求加载处理。
 */

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <a.out.h>

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/segment.h>

extern int sys_exit(int exit_code);
extern int sys_close(int fd);

/*
 * MAX_ARG_PAGES defines the number of pages allocated for arguments
 * and envelope for the new program. 32 should suffice, this gives
 * a maximum env+arg of 128kB !
 *
 * MAX_ARG_PAGES 定义了新程序分配给参数和环境变量使用的内存最大页数。
 * 32 页内存应该足够了，这使得环境和参数(env+arg)空间的总合达到 128kB!
 */
#define MAX_ARG_PAGES 32

/*
 * create_tables() parses the env- and arg-strings in new user
 * memory and creates the pointer tables from them, and puts their
 * addresses on the "stack", returning the new stack pointer value.
 *
 * create_tables()函数在新用户内存中解析环境变量和参数字符串，由此
 * 创建指针表，并将它们的地址放到"堆栈"上，然后返回新栈的指针值。
 */

//// 在新用户堆栈中创建环境和参数变量指针表。
// 参数:p - 以数据段为起点的参数和环境信息偏移指针;argc - 参数个数;envc -环境变量数。
// 返回:堆栈指针。
static unsigned long * create_tables(char * p,int argc,int envc)
{
    unsigned long *argv,*envp;
    unsigned long * sp;

    // 堆栈指针是以 4 字节(1 节)为边界寻址的，因此这里让 sp 为 4 的整数倍。
    sp = (unsigned long *) (0xfffffffc & (unsigned long) p);
    // sp 向下移动，空出环境参数占用的空间个数，并让环境参数指针 envp 指向该处。
    sp -= envc+1;
    envp = sp;
    // sp 向下移动，空出命令行参数指针占用的空间个数，并让 argv 指针指向该处。
    // 下面指针加 1，sp 将递增指针宽度字节值。
    sp -= argc+1;
    argv = sp;
    // 将环境参数指针 envp 和命令行参数指针以及命令行参数个数压入堆栈。
    put_fs_long((unsigned long)envp,--sp);
    put_fs_long((unsigned long)argv,--sp);
    put_fs_long((unsigned long)argc,--sp);

    // 将命令行各参数指针放入前面空出来的相应地方，最后放置一个 NULL 指针。
    while (argc-->0) {
        put_fs_long((unsigned long) p,argv++);
        while (get_fs_byte(p++)) /* nothing */ ; // p指针前移4字节。
    }
    put_fs_long(0,argv);

    // 将环境变量各指针放入前面空出来的相应地方，最后放置一个 NULL 指针。
    while (envc-->0) {
        put_fs_long((unsigned long) p,envp++);
        while (get_fs_byte(p++)) /* nothing */ ;
    }
    put_fs_long(0,envp);

    return sp;                      // 返回构造的当前新堆栈指针。
}

/*
 * count() counts the number of arguments/envelopes
 *
 * count()函数计算命令行参数/环境变量的个数。
 */

//// 计算参数个数。
// 参数:argv - 参数指针数组，最后一个指针项是 NULL。
// 返回:参数个数。
static int count(char ** argv)
{
    int i=0;
    char ** tmp;

    if ((tmp = argv))
        while (get_fs_long((unsigned long *) (tmp++)))
            i++;

    return i;
}

/*
 * 'copy_string()' copies argument/envelope strings from user
 * memory to free pages in kernel mem. These are in a format ready
 * to be put directly into the top of new user memory.
 *
 * Modified by TYT, 11/24/91 to add the from_kmem argument, which specifies
 * whether the string and the string array are from user or kernel segments:
 *
 * from_kmem     argv *        argv **
 *    0          user space    user space
 *    1          kernel space  user space
 *    2          kernel space  kernel space
 *
 * We do this by playing games with the fs segment register.  Since it
 * it is expensive to load a segment register, we try to avoid calling
 * set_fs() unless we absolutely have to.
 *
 * 'copy_string()'函数从用户内存空间拷贝参数和环境字符串到内核空闲页面内存中。
 * 这些已具有直接放到新用户内存中的格式。
 * 由 TYT(Tytso)于 1991.12.24 日修改，增加了 from_kmem 参数，该参数指明了字符串或
 * 字符串数组是来自用户段还是内核段。
 * from_kmem     argv *        argv **
 *    0          user space    user space
 *    1          kernel space  user space
 *    2          kernel space  kernel space
 *
 * 我们是通过巧妙处理 fs 段寄存器来操作的。由于加载一个段寄存器代价太大，
 * 所以 我们尽量避免调用 set_fs()，除非实在必要。
 */

//// 复制指定个数的参数字符串到参数和环境空间。
// 参数:argc - 欲添加的参数个数;argv - 参数指针数组;page - 参数和环境空间页面指针数组。
// p -在参数表空间中的偏移指针，始终指向已复制串的头部;from_kmem - 字符串来源标志。
// 在 do_execve()函数中，p 初始化为指向参数表(128kB)空间的最后一个长字处，参数字符串
// 是以堆栈操作方式逆向往其中复制存放的，因此 p 指针会始终指向参数字符串的头部。
// 返回:参数和环境空间当前头部指针。
static unsigned long copy_strings(int argc,char ** argv,unsigned long *page,
                                  unsigned long p, int from_kmem)
{
    char *tmp, *pag=NULL;
    int len, offset = 0;
    unsigned long old_fs, new_fs;

    if (!p)
        return 0;	/* bullet-proofing */
    new_fs = get_ds();
    old_fs = get_fs();
    if (from_kmem==2)
        set_fs(new_fs);
    while (argc-- > 0) {
        if (from_kmem == 1)
            set_fs(new_fs);
        if (!(tmp = (char *)get_fs_long(((unsigned long *)argv)+argc)))
            panic("argc is wrong");
        if (from_kmem == 1)
            set_fs(old_fs);
        len=0;		/* remember zero-padding */
        do {
            len++;
        } while (get_fs_byte(tmp++));
        if (p-len < 0) {	/* this shouldn't happen - 128kB */
            set_fs(old_fs);
            return 0;
        }
        while (len) {
            --p; --tmp; --len;
            if (--offset < 0) {
                offset = p % PAGE_SIZE;
                if (from_kmem==2)
                    set_fs(old_fs);
                if (!(pag = (char *) page[p/PAGE_SIZE]) &&
                    !(pag = (char *) (page[p/PAGE_SIZE] =
                                              get_free_page())))
                    return 0;
                if (from_kmem==2)
                    set_fs(new_fs);

            }
            *(pag + offset) = get_fs_byte(tmp);
        }
    }
    if (from_kmem==2)
        set_fs(old_fs);
    return p;
}

//// 修改局部描述符表中的描述符基址和段限长，并将参数和环境空间页面放置在数据段末端。
// 参数:text_size - 执行文件头部中 a_text 字段给出的代码段长度值;
// page - 参数和环境空间页面指针数组。
// 返回:数据段限长值(64MB)。
static unsigned long change_ldt(unsigned long text_size,unsigned long * page)
{
    unsigned long code_limit,data_limit,code_base,data_base;
    int i;

    // 根据执行文件头部 a_text 值，计算以页面长度为边界的代码段限长。并设置数据段长度为 64MB。
    code_limit = text_size+PAGE_SIZE -1;
    code_limit &= 0xFFFFF000;
    data_limit = 0x4000000;
    // 取当前进程中局部描述符表代码段描述符中代码段基址，代码段基址与数据段基址相同。
    code_base = get_base(current->ldt[1]);
    data_base = code_base;
    // 重新设置局部表中代码段和数据段描述符的基址和段限长。
    set_base(current->ldt[1],code_base);
    set_limit(current->ldt[1],code_limit);
    set_base(current->ldt[2],data_base);
    set_limit(current->ldt[2],data_limit);
/* make sure fs points to the NEW data segment */
/* 要确信 fs 段寄存器已指向新的数据段 */
    // fs 段寄存器中放入局部表数据段描述符的选择符(0x17)。
    __asm__("pushl $0x17\n\tpop %%fs"::);
    // 将参数和环境空间已存放数据的页面(共可有 MAX_ARG_PAGES 页，128kB)放到数据段线性地址的
    // 末端。是调用函数 put_page()进行操作的(mm/memory.c, 197)。
    data_base += data_limit;
    for (i=MAX_ARG_PAGES-1 ; i>=0 ; i--) {
        data_base -= PAGE_SIZE;
        if (page[i])                    // 如果该页面存在，就放置该页面。
            put_page(page[i],data_base);
    }
    return data_limit;                  // 最后返回数据段限长(64MB)。
}

/*
 * 'do_execve()' executes a new program.
 * 'do_execve()'函数执行一个新程序。
 */

//// execve()系统中断调用函数。加载并执行子进程(其他程序)。
// 该函数系统中断调用(int 0x80)功能号__NR_execve 调用的函数。
// 参数:eip - 指向堆栈中调用系统中断的程序代码指针 eip 处，参见 kernel/system_call.s 程序
// 开始部分的说明;tmp - 系统中断中在调用_sys_execve 时的返回地址，无用;
// filename - 被执行程序文件名;argv - 命令行参数指针数组;envp - 环境变量指针数组。
// 返回:如果调用成功，则不返回;否则设置出错号，并返回-1。
int do_execve(unsigned long * eip,long tmp,char * filename,
              char ** argv, char ** envp)
{
    struct m_inode * inode;             // 内存中 I 节点指针结构变量。
    struct buffer_head * bh;            // 高速缓存块头指针。
    struct exec ex;                     // 执行文件头部数据结构变量。
    unsigned long page[MAX_ARG_PAGES];  // 参数和环境字符串空间的页面指针数组。
    int i,argc,envc;
    int e_uid, e_gid;                   // 有效用户 id 和有效组 id。
    int retval;                         // 返回值。
    int sh_bang = 0;                    // 控制是否需要执行脚本处理代码
    // 参数和环境字符串空间中的偏移指针，初始化为指向该空间的最后一个长字处。
    unsigned long p=PAGE_SIZE*MAX_ARG_PAGES-4;



    printk("do_execve->filename:%s\n\r", get_from_fs(filename));

    // eip[1]中是原代码段寄存器 cs，其中的选择符不可以是内核段选择符，也即内核不能调用本函数。
    if ((0xffff & eip[1]) != 0x000f)
        panic("execve called from supervisor mode");
    // 初始化参数和环境串空间的页面指针数组(表)。
    for (i=0 ; i<MAX_ARG_PAGES ; i++)	/* clear page-table */
        page[i]=0;
    // 取可执行文件的对应 i 节点号。
    if (!(inode=namei(filename)))		/* get executables inode */
        return -ENOENT;
    // 计算参数个数和环境变量个数。
    argc = count(argv);
    envc = count(envp);

    // 执行文件必须是常规文件。若不是常规文件则置出错返回码，跳转到 exec_error2
    restart_interp:
    if (!S_ISREG(inode->i_mode)) {	/* must be regular file */
        retval = -EACCES;
        goto exec_error2;
    }
    // 以下检查被执行文件的执行权限。根据其属性(对应 i 节点的 uid 和 gid)，看本进程是否有权执行它。
    i = inode->i_mode;      // 取文件属性字段值。
    // 如果文件的设置用户 ID 标志(set-user-id)置位的话，则后面执行进程的有效用户 ID(euid)就
    // 设置为文件的用户 ID，否则设置成当前进程的 euid。这里将该值暂时保存在 e_uid 变量中。
    // 如果文件的设置组 ID 标志(set-group-id)置位的话，则执行进程的有效组 ID(egid)就设置为
    // 文件的组 ID。否则设置成当前进程的 egid。
    e_uid = (i & S_ISUID) ? inode->i_uid : current->euid;
    e_gid = (i & S_ISGID) ? inode->i_gid : current->egid;
    // 如果文件属于运行进程的用户，则把文件属性字右移 6 位，则最低 3 位是文件宿主的访问权限标志。
    // 否则的话如果文件与运行进程的用户属于同组，则使属性字最低 3 位是文件组用户的访问权限标志。
    // 否则属性字最低 3 位是其他用户访问该文件的权限。
    if (current->euid == inode->i_uid)
        i >>= 6;
    else if (current->egid == inode->i_gid)
        i >>= 3;
    // 如果上面相应用户没有执行权并且其他用户也没有任何权限，并且不是超级用户，则表明该文件不
    // 能被执行。于是置不可执行出错码，跳转到 exec_error2 处去处理。
    if (!(i & 1) &&
        !((inode->i_mode & 0111) && suser())) {
        retval = -ENOEXEC;
        goto exec_error2;
    }
    // 读取执行文件的第一块数据到高速缓冲区，若出错则置出错码，跳转到 exec_error2 处去处理。
    if (!(bh = bread(inode->i_dev,inode->i_zone[0]))) {
        retval = -EACCES;
        goto exec_error2;
    }
    // 下面对执行文件的头结构数据进行处理，首先让 ex 指向执行头部分的数据结构。
    ex = *((struct exec *) bh->b_data);	/* read exec-header */  /* 读取执行头部分 */
    // 如果执行文件开始的两个字节为'#!'，并且 sh_bang 标志没有置位，则处理脚本文件的执行。
    if ((bh->b_data[0] == '#') && (bh->b_data[1] == '!') && (!sh_bang)) {
        /*
         * This section does the #! interpretation.
         * Sorta complicated, but hopefully it will work.  -TYT
         *
         * 这部分处理对'#!'的解释，有些复杂，但希望能工作。-TYT
         */

        char buf[1023], *cp, *interp, *i_name, *i_arg;
        unsigned long old_fs;

        strncpy(buf, bh->b_data+2, 1022);
        brelse(bh);
        iput(inode);
        buf[1022] = '\0';
        if ((cp = strchr(buf, '\n'))) {
            *cp = '\0';
            for (cp = buf; (*cp == ' ') || (*cp == '\t'); cp++);
        }
        if (!cp || *cp == '\0') {
            retval = -ENOEXEC; /* No interpreter name found */
            goto exec_error1;
        }
        interp = i_name = cp;
        i_arg = 0;
        for ( ; *cp && (*cp != ' ') && (*cp != '\t'); cp++) {
            if (*cp == '/')
                i_name = cp+1;
        }
        if (*cp) {
            *cp++ = '\0';
            i_arg = cp;
        }
        /*
         * OK, we've parsed out the interpreter name and
         * (optional) argument.
         */
        if (sh_bang++ == 0) {
            p = copy_strings(envc, envp, page, p, 0);
            p = copy_strings(--argc, argv+1, page, p, 0);
        }
        /*
         * Splice in (1) the interpreter's name for argv[0]
         *           (2) (optional) argument to interpreter
         *           (3) filename of shell script
         *
         * This is done in reverse order, because of how the
         * user environment and arguments are stored.
         */
        p = copy_strings(1, &filename, page, p, 1);
        argc++;
        if (i_arg) {
            p = copy_strings(1, &i_arg, page, p, 2);
            argc++;
        }
        p = copy_strings(1, &i_name, page, p, 2);
        argc++;
        if (!p) {
            retval = -ENOMEM;
            goto exec_error1;
        }
        /*
         * OK, now restart the process with the interpreter's inode.
         */
        old_fs = get_fs();
        set_fs(get_ds());
        if (!(inode=namei(interp))) { /* get executables inode */
            set_fs(old_fs);
            retval = -ENOENT;
            goto exec_error1;
        }
        set_fs(old_fs);
        goto restart_interp;
    }
    // 释放该缓冲区。
    brelse(bh);
    // 下面对执行头信息进行处理。
    // 对于下列情况，将不执行程序:如果执行文件不是需求页可执行文件(ZMAGIC)、或者代码重定位部分
    // 长度 a_trsize 不等于 0、或者数据重定位信息长度不等于 0、或者代码段+数据段+堆段长度超过 50MB、
    // 或者 i 节点表明的该执行文件长度小于代码段+数据段+符号表长度+执行头部分长度的总和。
    if (N_MAGIC(ex) != ZMAGIC || ex.a_trsize || ex.a_drsize ||
        ex.a_text+ex.a_data+ex.a_bss>0x3000000 ||
        inode->i_size < ex.a_text+ex.a_data+ex.a_syms+N_TXTOFF(ex)) {
        retval = -ENOEXEC;
        goto exec_error2;
    }
    // 如果执行文件执行头部分长度不等于一个内存块大小(1024 字节)，也不能执行。转 exec_error2。
    if (N_TXTOFF(ex) != BLOCK_SIZE) {
        printk("%s: N_TXTOFF != BLOCK_SIZE. See a.out.h.", filename);
        retval = -ENOEXEC;
        goto exec_error2;
    }
    // 如果 sh_bang 标志没有设置，则复制指定个数的环境变量字符串和参数到参数和环境空间中。
    // 若 sh_bang 标志已经设置，则表明是将运行脚本程序，此时环境变量页面已经复制，无须再复制。
    if (!sh_bang) {
        p = copy_strings(envc,envp,page,p,0);
        p = copy_strings(argc,argv,page,p,0);
        // 如果 p=0，则表示环境变量与参数空间页面已经被占满，容纳不下了。转至出错处理处。
        if (!p) {
            retval = -ENOMEM;
            goto exec_error2;
        }
    }
/* OK, This is the point of no return */
/* OK，下面开始就没有返回的地方了 */
    // 如果原程序也是一个执行程序，则释放其 i 节点，并让进程 executable 字段指向新程序 i 节点。
    if (current->executable)
        iput(current->executable);
    current->executable = inode;
    // 清复位所有信号处理句柄。但对于 SIG_IGN 句柄不能复位，因此在 322 与 323 行之间需添加一条
    // if 语句:if (current->sa[I].sa_handler != SIG_IGN)。这是源代码中的一个 bug。
    for (i=0 ; i<32 ; i++)
        current->sigaction[i].sa_handler = NULL;

    // 根据执行时关闭(close_on_exec)文件句柄位图标志，关闭指定的打开文件，并复位该标志。
    for (i=0 ; i<NR_OPEN ; i++)
        if ((current->close_on_exec>>i)&1)
            sys_close(i);
    current->close_on_exec = 0;

    // 根据指定的基地址和限长，释放原来进程代码段和数据段所对应的内存页表指定的内存块及页表本
    // 此时被执行程序没有占用主内存区任何页面。在执行时会引起内存管理程序执行缺页处理而为其申请
    // 内存页面，并把程序读入内存。
    free_page_tables(get_base(current->ldt[1]),get_limit(0x0f));
    free_page_tables(get_base(current->ldt[2]),get_limit(0x17));
    // 如果“上次任务使用了协处理器”指向的是当前进程，则将其置空，并复位使用了协处理器的标志。
    if (last_task_used_math == current)
        last_task_used_math = NULL;
    current->used_math = 0;

    // 根据 a_text 修改局部表中描述符基址和段限长，并将参数和环境空间页面放置在数据段末端。
    // 执行下面语句之后，p 此时是以数据段起始处为原点的偏移值，仍指向参数和环境空间数据开始处，
    // 也即转换成为堆栈的指针。
    p += change_ldt(ex.a_text,page)-MAX_ARG_PAGES*PAGE_SIZE;
    // create_tables()在新用户堆栈中创建环境和参数变量指针表，并返回该堆栈指针。
    p = (unsigned long) create_tables((char *)p,argc,envc);

    // 修改当前进程各字段为新执行程序的信息。令进程代码段尾值字段 end_code = a_text;令进程数据
    // 段尾字段 end_data = a_data + a_text;令进程堆结尾字段 brk = a_text + a_data + a_bss。
    current->brk = ex.a_bss +
                   (current->end_data = ex.a_data +
                                        (current->end_code = ex.a_text));

    // 设置进程堆栈开始字段为堆栈指针所在的页面，并重新设置进程的有效用户 id 和有效组 id。
    current->start_stack = p & 0xfffff000;
    current->euid = e_uid;
    current->egid = e_gid;

    // 初始化一页 bss 段数据，全为零。
    i = ex.a_text+ex.a_data;
    while (i&0xfff)
        put_fs_byte(0,(char *) (i++));

    // 将原调用系统中断的程序在堆栈上的代码指针替换为指向新执行程序的入口点，并将堆栈指针替换
    // 为新执行程序的堆栈指针。返回指令将弹出这些堆栈数据并使得 CPU 去执行新的执行程序，因此不会
    // 返回到原调用系统中断的程序中去了。
    eip[0] = ex.a_entry;		/* eip, magic happens :-) */    /* eip，魔法起作用了*/
    eip[3] = p;			/* stack pointer */                     /* esp，堆栈指针 */
    return 0;
    exec_error2:
    iput(inode);
    exec_error1:
    for (i=0 ; i<MAX_ARG_PAGES ; i++)
        free_page(page[i]);
    return(retval);
}