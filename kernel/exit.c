/*
 *  linux/kernel/exit.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/tty.h>
#include <asm/segment.h>

int sys_pause(void);        // 把进程置为睡眠状态的系统调用。(kernel/sched.c，144 行)
int sys_close(int fd);      // 关闭指定文件的系统调用。(fs/open.c，192 行)

//// 释放指定进程占用的任务槽及其任务数据结构所占用的内存。
// 参数 p 是任务数据结构的指针。该函数在后面的 sys_kill()和 sys_waitpid()中被调用。
// 扫描任务指针数组表 task[]以寻找指定的任务。如果找到，则首先清空该任务槽，然后释放
// 该任务数据结构所占用的内存页面，最后执行调度函数并在返回时立即退出。如果在任务数组
// 表中没有找到指定任务对应的项，则内核 panic☺。
void release(struct task_struct * p)
{
    int i;

    if (!p)                 // 如果进程数据结构指针是 NULL，则什么也不做，退出。
        return;
    for (i=1 ; i<NR_TASKS ; i++)        // 扫描任务数组，寻找指定任务。
        if (task[i]==p) {
            task[i]=NULL;               // 置空该任务项并释放相关内存页。
            free_page((long)p);
            schedule();                 // 重新调度(似乎没有必要)。
            return;
        }
    panic("trying to release non-existent task");       // 指定任务若不存在则死机。
}

//// 向指定任务 p 发送信号 sig，权限为 priv。
// 参数:sig -- 信号值;
// p -- 指定任务的指针;
// priv -- 强制发送信号的标志。即不需要考虑进程用户属性或级别而能发送信号的权利。
// 该函数首先判断参数的正确性，然后判断条件是否满足。如果满足就向指定进程发送信号 sig
// 并退出，否则返回未许可错误号。
static inline int send_sig(long sig,struct task_struct * p,int priv)
{
    // 若信号不正确或任务指针为空则出错退出。
    if (!p || sig<1 || sig>32)
        return -EINVAL;

    // 如果强制发送标志置位，或者当前进程的有效用户标识符(euid)就是指定进程的 euid(也即是自己)，
    // 或者当前进程是超级用户，则向进程 p 发送信号 sig，即在进程 p 位图中添加该信号，否则出错退出。
    // 其中 suser()定义为(current->euid==0)，用于判断是否是超级用户。
    if (priv || (current->euid==p->euid) || suser())
        p->signal |= (1<<(sig-1));
    else
        return -EPERM;
    return 0;
}

//// 终止会话(session)。
// 进程会话的概念请参见第 4 章中有关进程组和会话的说明。
static void kill_session(void)
{
    struct task_struct **p = NR_TASKS + task;           // 指针*p 首先指向任务数组最末端。

    // 扫描任务指针数组，对于所有的任务(除任务 0 以外)，如果其会话号 session 等于当前进程的
    // 会话号就向它发送挂断进程信号 SIGHUP。
    while (--p > &FIRST_TASK) {
        if (*p && (*p)->session == current->session)
            (*p)->signal |= 1<<(SIGHUP-1);               // 发送挂断进程信号。
    }
}

/*
 * XXX need to check permissions needed to send signals to process
 * groups, etc. etc.  kill() permissions semantics are tricky!
 *
 * 为了向进程组等发送信号，XXX 需要检查许可。kill()的许可机制非常巧妙!
 */

//// 系统调用 kill()可用于向任何进程或进程组发送任何信号，而并非只是杀死进程☺。
// 参数 pid 是进程号;sig 是需要发送的信号。
// 如果 pid 值>0，则信号被发送给进程号是 pid 的进程。
// 如果 pid=0，那么信号就会被发送给当前进程的进程组中的所有进程。
// 如果 pid=-1，则信号 sig 就会发送给除第一个进程(初始进程 init)外的所有进程。
// 如果 pid < -1，则信号 sig 将发送给进程组-pid 的所有进程。
// 如果信号 sig 为 0，则不发送信号，但仍会进行错误检查。如果成功则返回 0。
// 该函数扫描任务数组表，并根据 pid 的值对满足条件的进程发送指定的信号 sig。若 pid 等于 0，
// 表明当前进程是进程组组长，因此需要向所有组内的进程强制发送信号 sig。
int sys_kill(int pid,int sig)
{
    struct task_struct **p = NR_TASKS + task;
    int err, retval = 0;

    if (!pid) while (--p > &FIRST_TASK) {
            if (*p && (*p)->pgrp == current->pid)
                if ((err=send_sig(sig,*p,1)))       // 强制发送信号。
                    retval = err;
        } else if (pid>0) while (--p > &FIRST_TASK) {
            if (*p && (*p)->pid == pid)
                if ((err=send_sig(sig,*p,0)))
                    retval = err;
        } else if (pid == -1) while (--p > &FIRST_TASK) {
            if ((err = send_sig(sig,*p,0)))
                retval = err;
        } else while (--p > &FIRST_TASK)
            if (*p && (*p)->pgrp == -pid)
                if ((err = send_sig(sig,*p,0)))
                    retval = err;
    return retval;
}

//// 通知父进程 -- 向进程 pid 发送信号 SIGCHLD:默认情况下子进程将停止或终止。
// 如果没有找到父进程，则自己释放。但根据 POSIX.1 要求，若父进程已先行终止，则子进程应该
// 被初始进程 1 收容。
static void tell_father(int pid)
{
    int i;

    if (pid)
        // 扫描进程数组表寻找指定进程 pid，并向其发送子进程将停止或终止信号 SIGCHLD。
        for (i=0;i<NR_TASKS;i++) {
            if (!task[i])
                continue;
            if (task[i]->pid != pid)
                continue;
            task[i]->signal |= (1<<(SIGCHLD-1));
            return;
        }
/* if we don't find any fathers, we just release ourselves */
/* This is not really OK. Must change it to make father 1 */
/* 如果没有找到父进程，则进程就自己释放。这样做并不好，必须改成由进程 1 充当其父进程。 */
    printk("BAD BAD - no father found\n\r");
    release(current);       // 如果没有找到父进程，则自己释放。
}

//// 程序退出处理函数。在下面的系统调用 sys_exit()中被调用。
// 参数 code 是错误码。
int do_exit(long code)
{
    int i;
    // 释放当前进程代码段和数据段所占的内存页(free_page_tables()在 mm/memory.c,105 行)。
    free_page_tables(get_base(current->ldt[1]),get_limit(0x0f));
    free_page_tables(get_base(current->ldt[2]),get_limit(0x17));
    // 如果当前进程有子进程，就将子进程的 father 置为 1(其父进程改为进程 1，即 init 进程)。
    // 如果该子进程已经处于僵死(ZOMBIE)状态，则向进程 1 发送子进程终止信号 SIGCHLD。
    for (i=0 ; i<NR_TASKS ; i++)
        if (task[i] && task[i]->father == current->pid) {
            task[i]->father = 1;
            if (task[i]->state == TASK_ZOMBIE)
                /* assumption task[1] is always init */
                /* 这里假设task[1]肯定是进程init */
                (void) send_sig(SIGCHLD, task[1], 1);
        }
    // 关闭当前进程打开着的所有文件。
    for (i=0 ; i<NR_OPEN ; i++)
        if (current->filp[i])
            sys_close(i);

    // 对当前进程的工作目录 pwd、根目录 root 以及程序的 i 节点进行同步操作，并分别置空(释放)。
    iput(current->pwd);
    current->pwd=NULL;
    iput(current->root);
    current->root=NULL;
    iput(current->executable);
    current->executable=NULL;

    // 如果当前进程是会话头领(leader)进程并且其有控制终端，则释放该终端。
    if (current->leader && current->tty >= 0)
        tty_table[current->tty].pgrp = 0;

    // 如果当前进程上次使用过协处理器，则将 last_task_used_math 置空。
    if (last_task_used_math == current)
        last_task_used_math = NULL;

    // 如果当前进程是 leader 进程，则终止该会话的所有相关进程。
    if (current->leader)
        kill_session();

    // 把当前进程置为僵死状态，表明当前进程已经释放了资源。并保存将由父进程读取的退出码。
    current->state = TASK_ZOMBIE;
    current->exit_code = code;

    // 通知父进程，也即向父进程发送信号 SIGCHLD -- 子进程将停止或终止。
    tell_father(current->father);
    schedule();     // 重新调度进程运行，以让父进程处理僵死进程其他的善后事宜。
    return (-1);	/* just to suppress warnings */
}

//系统调用 exit()，终止进程。
int sys_exit(int error_code)
{
    return do_exit((error_code&0xff)<<8);
}

//// 系统调用 waitpid()。挂起当前进程，直到 pid 指定的子进程退出(终止)或者收到要求终止
// 该进程的信号，或者是需要调用一个信号句柄(信号处理程序)。如果 pid 所指的子进程早已
// 退出(已成所谓的僵死进程)，则本调用将立刻返回。子进程使用的所有资源将释放。
// 如果 pid > 0, 表示等待进程号等于 pid 的子进程。
// 如果 pid = 0, 表示等待进程组号等于当前进程组号的任何子进程。
// 如果 pid < -1, 表示等待进程组号等于 pid 绝对值的任何子进程。
// [ 如果 pid = -1, 表示等待任何子进程。]
// 若 options = WUNTRACED，表示如果子进程是停止的，也马上返回(无须跟踪)。
// 若 options = WNOHANG，表示如果没有子进程退出或终止就马上返回。
// 如果返回状态指针 stat_addr 不为空，则就将状态信息保存到那里。
int sys_waitpid(pid_t pid,unsigned long * stat_addr, int options)
{
    int flag, code;
    struct task_struct ** p;

    verify_area(stat_addr,4);
    repeat:
    flag=0;
    // 从任务数组末端开始扫描所有任务，跳过空项、本进程项以及非当前进程的子进程项。
    for(p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
        if (!*p || *p == current)           // 跳过空项和本进程项。
            continue;
        if ((*p)->father != current->pid)   // 如果不是当前进程的子进程则跳过。
            continue;

        // 此时扫描选择到的进程 p 肯定是当前进程的子进程。
        // 如果等待的子进程号 pid>0，但与被扫描子进程 p 的 pid 不相等，说明它是当前进程另外的子进程，
        // 于是跳过该进程，接着扫描下一个进程。
        if (pid>0) {
            if ((*p)->pid != pid)
                continue;
        } else if (!pid) {
            // 否则，如果指定等待进程的 pid=0，表示正在等待进程组号等于当前进程组号的任何子进程。如果
            // 此时被扫描进程 p 的进程组号与当前进程的组号不等，则跳过。
            if ((*p)->pgrp != current->pgrp)
                continue;
        } else if (pid != -1) {
            // 否则，如果指定的 pid<-1，表示正在等待进程组号等于 pid 绝对值的任何子进程。如果此时被扫描
            // 进程 p 的组号与 pid 的绝对值不等，则跳过。
            if ((*p)->pgrp != -pid)
                continue;
        }

        // 如果前 3 个对 pid 的判断都不符合，则表示当前进程正在等待其任何子进程，也即 pid=-1 的情况。
        // 此时所选择到的进程 p 正是所等待的子进程。接下来根据这个子进程 p 所处的状态来处理。
        switch ((*p)->state) {
            // 子进程 p 处于停止状态时，如果此时 WUNTRACED 标志没有置位，表示程序无须立刻返回，于是继续
            // 扫描处理其他进程。如果 WUNTRACED 置位，则把状态信息 0x7f 放入*stat_addr，并立刻返回子进程
            // 号 pid。这里 0x7f 表示的返回状态使 WIFSTOPPED()宏为真。参见 include/sys/wait.h，14 行。
            case TASK_STOPPED:
                if (!(options & WUNTRACED))
                    continue;
                put_fs_long(0x7f,stat_addr);
                return (*p)->pid;

            // 如果子进程 p 处于僵死状态，则首先把它在用户态和内核态运行的时间分别累计到当前进程(父进程)
            // 中，然后取出子进程的 pid 和退出码，并释放该子进程。最后返回子进程的退出码和 pid。
            case TASK_ZOMBIE:
                current->cutime += (*p)->utime;
                current->cstime += (*p)->stime;
                flag = (*p)->pid;                       // 临时保存子进程 pid。
                code = (*p)->exit_code;                 // 取子进程的退出码。
                release(*p);                            // 释放该子进程。
                put_fs_long(code,stat_addr);   // 置状态信息为退出码值。
                return flag;                            // 退出，返回子进程的 pid.

            // 如果这个子进程 p 的状态既不是停止也不是僵死，那么就置 flag=1。表示找到过一个符合要求的
            // 子进程，但是它处于运行态或睡眠态。
            default:
                flag=1;
                continue;
        }
    }

    // 在上面对任务数组扫描结束后，如果 flag 被置位，说明有符合等待要求的子进程并没有处于退出
    // 或僵死状态。如果此时已设置 WNOHANG 选项(表示若没有子进程处于退出或终止态就立刻返回)，
    // 就立刻返回 0，退出。否则把当前进程置为可中断等待状态并重新执行调度。
    if (flag) {
        if (options & WNOHANG)              // 若 options = WNOHANG，则立刻返回。
            return 0;
        current->state=TASK_INTERRUPTIBLE;  // 置当前进程为可中断等待状态。
        schedule();                         // 重新调度。

        // 当又开始执行本进程时，如果本进程没有收到除 SIGCHLD 以外的信号，则还是重复处理。否则，
        // 返回出错码并退出。
        if (!(current->signal &= ~(1<<(SIGCHLD-1))))
            goto repeat;
        else
            return -EINTR;      // 退出，返回出错码。
    }
    // 若没有找到符合要求的子进程，则返回出错码。
    return -ECHILD;
}