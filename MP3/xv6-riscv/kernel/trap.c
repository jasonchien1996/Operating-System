#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}
 
//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
  {
    if(p->timer_on) p->tick += 1;
    yield();
  }
  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()
  //invoke handler
  if(p->timer_on && p->tick >= p->delay){
    p->timer_on = 0;
    for(int i=0; i<MAX_THRD_NUM; i++){
      if(p->tf_list[i].free){
        p->tf_list[i].free = 0;
        p->tf_list[i].id = p->id;
        p->tf_list[i].trapframe = *(p->trapframe);
        //printf("\nstop id=%d\n",p->id);
        //printf("stop %p\n",&(p->tf_list[i].trapframe));
        break;
      }
      /*
      else{
        if(i==MAX_THRD_NUM-1) printf("stop not found\n");
      }
      */
    }
    p->trapframe->epc = p->handler;
    p->trapframe->a0 = p->handler_arg;
  }
  //resume
  else if(p->trapframe->a7 == 23){
    for(int i=0; i<MAX_THRD_NUM; i++){
      if(p->tf_list[i].free == 0 && p->tf_list[i].id == p->id){
        struct trapframe *new_tf = p->trapframe;
        struct trapframe *old_tf = &(p->tf_list[i].trapframe);
        new_tf->s0 = old_tf->s0;
        new_tf->s1 = old_tf->s1;
        new_tf->s2 = old_tf->s2;
        new_tf->s3 = old_tf->s3;
        new_tf->s4 = old_tf->s4;
        new_tf->s5 = old_tf->s5;
        new_tf->s6 = old_tf->s6;
        new_tf->s7 = old_tf->s7;
        new_tf->s8 = old_tf->s8;
        new_tf->s9 = old_tf->s9;
        new_tf->s10 = old_tf->s10;
        new_tf->s11 = old_tf->s11;
        new_tf->ra = old_tf->ra;
        new_tf->sp = old_tf->sp;
        new_tf->t0 = old_tf->t0;
        new_tf->t1 = old_tf->t1;
        new_tf->t2 = old_tf->t2;
        new_tf->t3 = old_tf->t3;
        new_tf->t4 = old_tf->t4;
        new_tf->t5 = old_tf->t5;
        new_tf->t6 = old_tf->t6;
        new_tf->a0 = old_tf->a0;
        new_tf->a1 = old_tf->a1;
        new_tf->a2 = old_tf->a2;
        new_tf->a3 = old_tf->a3;
        new_tf->a4 = old_tf->a4;
        new_tf->a5 = old_tf->a5;
        new_tf->a6 = old_tf->a6;
        new_tf->a7 = old_tf->a7;
        new_tf->gp = old_tf->gp;
        new_tf->tp = old_tf->tp;
        new_tf->epc = old_tf->epc;
        p->tf_list[i].free = 1;
        //printf("\nresume id=%d\n",p->id);
        //printf("resume %p\n",p->trapframe);
        break;
      }
      /*
      else {
        if(i==MAX_THRD_NUM-1) printf("resume not found\n");
      }
      */
    }
  }
  //cancel
  else if(p->trapframe->a7 == 24){
    if(p->is_exit && (p->id)!=-1 ){
      for(int i=0; i<MAX_THRD_NUM; i++){
        if(p->tf_list[i].id == p->id && p->tf_list[i].free == 0){
          p->tf_list[i].free = 1;
          break;
        }
      }
    }
    else if( !(p->is_exit) && (p->id)!=-1 ){
      for(int i=0; i<MAX_THRD_NUM; i++){
        if(p->tf_list[i].free){
          p->tf_list[i].free = 0;
          p->tf_list[i].id = p->id;
          p->tf_list[i].trapframe = *(p->trapframe);
          break;
        }
        /*
        else
          printf("cancel not found\n");
        */
      }
    }
  }
  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);
  
  //printf("goto %p\n", p->trapframe->epc);
  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);
  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
  {
    if(myproc()->timer_on) myproc()->tick += 1;//added
    //printf("\n%d %d\n", myproc()->tick, myproc()->delay);
    yield();
  }

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);

}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

