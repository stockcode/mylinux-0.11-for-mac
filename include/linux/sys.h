extern int sys_setup();         // 系统启动初始化设置函数。 (kernel/blk_drv/hd.c,71)
extern int sys_exit();          // 程序退出。    (kernel/exit.c, 137)
extern int sys_fork();          // 创建进程。    (kernel/system_call.s, 208)
extern int sys_read();          // 读文件。     (fs/read_write.c, 55)
extern int sys_write();         // 写文件。         (fs/read_write.c, 83)
extern int sys_open();          // 打开文件。        (fs/open.c, 138)
extern int sys_close();         // 关闭文件。    (fs/open.c, 192)
extern int sys_waitpid();       // 等待进程终止。  (kernel/exit.c, 142)
extern int sys_creat();         // 创建文件。        (fs/open.c, 187)
extern int sys_link();          // 创建一个文件的硬连接。      (fs/namei.c, 721)
extern int sys_unlink();        // 删除一个文件名(或删除文件)。  (fs/namei.c, 663)
extern int sys_execve();        // 执行程序。                (kernel/system_call.s, 200)
extern int sys_chdir();         // 更改当前目录。              (fs/open.c, 75)
extern int sys_time();          // 取当前时间。               (kernel/sys.c, 102)
extern int sys_mknod();         // 建立块/字符特殊文件。      (fs/namei.c, 412)
extern int sys_chmod();         // 修改文件属性。              (fs/open.c, 105)
extern int sys_chown();         // 修改文件宿主和所属组。      (fs/open.c, 121)
extern int sys_break();         //                              (-kernel/sys.c, 21)
extern int sys_stat();          // 使用路径名取文件的状态信息。   (fs/stat.c, 36)
extern int sys_lseek();         // 重新定位读/写文件偏移。         (fs/read_write.c, 25)
extern int sys_getpid();        // 取进程 id。                  (kernel/sched.c, 348)
extern int sys_mount();         // 安装文件系统。              (fs/super.c, 200)
extern int sys_umount();        // 卸载文件系统。              (fs/super.c, 167)
extern int sys_setuid();        // 设置进程用户 id。           (kernel/sys.c, 143)
extern int sys_getuid();        // 取进程用户 id。            (kernel/sched.c, 358)
extern int sys_stime();         // 设置系统时间日期。            (-kernel/sys.c, 148)
extern int sys_ptrace();        // 程序调试。                (-kernel/sys.c, 26)
extern int sys_alarm();         // 设置报警。                (kernel/sched.c, 338)
extern int sys_fstat();         // 使用文件句柄取文件的状态信息。(fs/stat.c, 47)
extern int sys_pause();         // 暂停进程运行。 (kernel/sched.c, 144)
extern int sys_utime();         // 改变文件的访问和修改时间。 (fs/open.c, 24)
extern int sys_stty();          // 修改终端行设置。 (-kernel/sys.c, 31)
extern int sys_gtty();          // 取终端行设置信息。 (-kernel/sys.c, 36)
extern int sys_access();        // 检查用户对一个文件的访问权限。(fs/open.c, 47)
extern int sys_nice();          // 设置进程执行优先权。       (kernel/sched.c, 378)
extern int sys_ftime();         // 取日期和时间。              (-kernel/sys.c,16)
extern int sys_sync();          // 同步高速缓冲与设备中数据。        (fs/buffer.c, 44)
extern int sys_kill();          // 终止一个进程。              (kernel/exit.c, 60)
extern int sys_rename();        // 更改文件名。           (-kernel/sys.c, 41)
extern int sys_mkdir();         // 创建目录。            (fs/namei.c, 463)
extern int sys_rmdir();         // 删除目录。            (fs/namei.c, 587)
extern int sys_dup();           // 复制文件句柄。          (fs/fcntl.c, 42)
extern int sys_pipe();          // 创建管道。            (fs/pipe.c, 71)
extern int sys_times();         // 取运行时间。           (kernel/sys.c, 156)
extern int sys_prof();          // 程序执行时间区域。        (-kernel/sys.c, 46)
extern int sys_brk();           // 修改数据段长度。         (kernel/sys.c, 168)
extern int sys_setgid();        // 设置进程组 id。        (kernel/sys.c, 72)
extern int sys_getgid();        // 取进程组 id。         (kernel/sched.c, 368)
extern int sys_signal();        // 信号处理。            (kernel/signal.c, 48)
extern int sys_geteuid();       // 取进程有效用户 id。  (kernel/sched.c, 363)
extern int sys_getegid();       // 取进程有效组 id。   (kernel/sched.c, 373)
extern int sys_acct();          // 进程记帐。        (-kernel/sys.c, 77)
extern int sys_phys();          //                  (-kernel/sys.c, 82)
extern int sys_lock();          //                  (-kernel/sys.c, 87)
extern int sys_ioctl();         // 设备控制。            (fs/ioctl.c, 30)
extern int sys_fcntl();         // 文件句柄操作。      (fs/fcntl.c, 47)
extern int sys_mpx();           //                  (-kernel/sys.c, 92)
extern int sys_setpgid();       // 设置进程组 id。        (kernel/sys.c, 181)
extern int sys_ulimit();        //                  (-kernel/sys.c, 97)
extern int sys_uname();         // 显示系统信息。          (kernel/sys.c, 216)
extern int sys_umask();         // 取默认文件创建属性码。      (kernel/sys.c, 230)
extern int sys_chroot();        // 改变根系统。           (fs/open.c, 90)
extern int sys_ustat();         // 取文件系统信息。             (fs/open.c, 19)
extern int sys_dup2();          // 复制文件句柄。              (fs/fcntl.c, 36)
extern int sys_getppid();       // 取父进程 id。                 (kernel/sched.c, 353)
extern int sys_getpgrp();       // 取进程组 id，等于 getpgid(0)。(kernel/sys.c, 201)
extern int sys_setsid();        // 在新会话中运行程序。           (kernel/sys.c, 206)
extern int sys_sigaction();     // 改变信号处理过程。             (kernel/signal.c, 63)
extern int sys_sgetmask();      // 取信号屏蔽码。          (kernel/signal.c, 15)
extern int sys_ssetmask();      // 设置信号屏蔽码。          (kernel/signal.c, 20)
extern int sys_setreuid();      // 设置真实与/或有效用户 id。 (kernel/sys.c,118)
extern int sys_setregid();      // 设置真实与/或有效组 id。   (kernel/sys.c, 51)
extern int sys_iam();
extern int sys_whoami();


// 系统调用函数指针表。用于系统调用中断处理程序(int 0x80)，作为跳转表。
fn_ptr sys_call_table[] = { sys_setup, sys_exit, sys_fork, sys_read,
                            sys_write, sys_open, sys_close, sys_waitpid, sys_creat, sys_link,
                            sys_unlink, sys_execve, sys_chdir, sys_time, sys_mknod, sys_chmod,
                            sys_chown, sys_break, sys_stat, sys_lseek, sys_getpid, sys_mount,
                            sys_umount, sys_setuid, sys_getuid, sys_stime, sys_ptrace, sys_alarm,
                            sys_fstat, sys_pause, sys_utime, sys_stty, sys_gtty, sys_access,
                            sys_nice, sys_ftime, sys_sync, sys_kill, sys_rename, sys_mkdir,
                            sys_rmdir, sys_dup, sys_pipe, sys_times, sys_prof, sys_brk, sys_setgid,
                            sys_getgid, sys_signal, sys_geteuid, sys_getegid, sys_acct, sys_phys,
                            sys_lock, sys_ioctl, sys_fcntl, sys_mpx, sys_setpgid, sys_ulimit,
                            sys_uname, sys_umask, sys_chroot, sys_ustat, sys_dup2, sys_getppid,
                            sys_getpgrp, sys_setsid, sys_sigaction, sys_sgetmask, sys_ssetmask,
                            sys_setreuid,sys_setregid, sys_iam, sys_whoami };
