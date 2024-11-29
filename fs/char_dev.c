#include<errno.h>
#include<sys/types.h>
#include<linux/sched.h>
#include<linux/fs.h>

extern int tty_read(unsigned minor,char * buf,int count);
extern int tty_write(unsigned minor,char * buf,int count);

typedef int (*crw_ptr)(int rw,unsigned minor,char * buf,int count,off_t * pos);

static int rw_ttyx(int rw,unsigned minor,char * buf,int count,off_t * pos)
{
    return ((rw==READ)?tty_read(minor,buf,count):
            tty_write(minor,buf,count));
}

static int rw_tty(int rw,unsigned minor,char * buf,int count, off_t * pos)
{
    if (current->tty<0)
        return -EPERM;
    return rw_ttyx(rw,current->tty,buf,count,pos);
}

#define NRDEVS ((sizeof (crw_table))/(sizeof (crw_ptr)))

static crw_ptr crw_table[]={
        NULL,		/* nodev */
        NULL,	/* /dev/mem etc */
        NULL,		/* /dev/fd */
        NULL,		/* /dev/hd */
        rw_ttyx,	/* /dev/ttyx */
        rw_tty,		/* /dev/tty */
        NULL,		/* /dev/lp */
        NULL};		/* unnamed pipes */

int rw_char(int rw,int dev, char * buf, int count, off_t * pos)
{
    crw_ptr call_addr;

    if (MAJOR(dev)>=NRDEVS)
        return -ENODEV;
    if (!(call_addr=crw_table[MAJOR(dev)]))
        return -ENODEV;
    return call_addr(rw,MINOR(dev),buf,count,pos);
}