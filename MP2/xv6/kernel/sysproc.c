#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int sz;
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
  struct proc *p = myproc();
  sz = p->sz;
  addr = sz-1;
  
  if(n>0){
    // find the section of address, i.e. [first, last], to be lazily allocated 
    uint64 first, last;
    if(addr==-1) first = 0;
    else if(addr%PGSIZE==0) first = PGROUNDUP(addr+1);
    else first = PGROUNDUP(addr);
    last = PGROUNDDOWN(addr+n);
    
    // modify the page table entries corresponding to [first, last]
    pte_t *pte;   
    for(uint64 a=first; a<=last; a+=PGSIZE){
      if((pte = walk(p->pagetable, a, 1)) == 0) return -1;
      if(*pte & PTE_V) panic("mappages: remap");
      *pte = PTE_W|PTE_X|PTE_R|PTE_U;
    }
  }
  
  else if(n<0){
    // find the section of address, i.e. [first, last], to be freed
    uint64 first, last;
    last = PGROUNDDOWN(addr);
    addr = addr+n;
    if(addr<0) first = 0;
    else if(addr%PGSIZE==0) first = PGROUNDUP(addr+1);
    else first = PGROUNDUP(addr);
    
    int npages = (last - first) / PGSIZE + 1;
    if(npages>0) uvmunmap(p->pagetable, first, npages, 1);
  }
  
  myproc()->sz+=n;
  //printf("end of sbrk()\n");
  return sz;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
