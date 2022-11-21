#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "defs.h"
#include "proc.h"

/* NTU OS 2022 */
/* Page fault handler */
int handle_pgfault() {
  /* Find the address that caused the fault */
  uint64 va = r_stval();
  va = PGROUNDDOWN(va);
  
  void *pa = kalloc();
  memset(pa, 0, PGSIZE);
    
  struct proc *p = myproc();
  pte_t *pte = walk(p->pagetable, va,1);
  
  // the page is in disk
  if(*pte&PTE_S){ 
    uint blockno = (uint)PTE2BLOCKNO(*pte);
        
    // swap the page to RAM
    begin_op();
    read_page_from_disk(ROOTDEV, pa, blockno);
    bfree_page(ROOTDEV, blockno);
    end_op();
    
    uint64 mask = 0b1111111111000000000000000000000000000000000000000000001111111111;
    *pte = (*pte&mask)|PA2PTE((uint64)pa);
    *pte = (*pte ^ PTE_S) | PTE_V;        
  }
  // lazy allocation
  else{  
    if(mappages(p->pagetable, va, PGSIZE, (uint64)pa, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
      // if creating page entry fails
      kfree(pa);
      return 0;
    }
  }
  return 1;
}
