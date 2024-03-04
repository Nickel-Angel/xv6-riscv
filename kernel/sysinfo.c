#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sysinfo.h"

uint64 
sys_sysinfo(void)
{   
    struct proc *p = myproc();
    struct sysinfo info;
    uint64 addr; // user pointer to struct info
    
    if (argaddr(0, &addr) < 0)
        return -1;

    info.freemem = kcount() * PGSIZE;
    info.nproc = proccount();

    if(copyout(p->pagetable, addr, (char *)&info, sizeof(info)))
        return -1;
    return 0;
}