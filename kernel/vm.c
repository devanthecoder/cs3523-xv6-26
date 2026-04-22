#include "param.h"
#include "types.h"
#include "memlayout.h"
// #include "vminfo.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "proc.h"
#include "disk_swap.h"

/*
* the kernel's page table.
*/
pagetable_t kernel_pagetable;
struct swapslot
{
  int in_use;
  pagetable_t pagetable;
  uint64 va;
  int swappedOut;
};

// #define MAX_FRAMES 100
struct frame
{
  int ref_bit;
  int in_use;
  struct proc *curr_proc;
  uint64 va;
  uint64 pa;
};

extern char etext[];      // kernel.ld sets this to end of kernel code.
extern char trampoline[]; // trampoline.S
struct frame frame_table[NFRAME];
// extern struct frame frame_table[NFRAME];
struct swapslot swap_table[SWAPSIZE];
extern struct spinlock frame_lock, swap_lock;
extern struct proc proc[NPROC];
struct frame *clock_hand = frame_table;

// Make a direct-map page table for the kernel.
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t)kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // uart registers
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // allocate and map a kernel stack for each process.
  proc_mapstacks(kpgtbl);

  return kpgtbl;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if (mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// Initialize the kernel_pagetable, shared by all CPUs.
void kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// Switch the current CPU's h/w page table register to
// the kernel's page table, and enable paging.
void kvminithart()
{
  // wait for any previous writes to the page table memory to finish.
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // flush stale entries from the TLB.
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if (va >= MAXVA)
    panic("walk");

  for (int level = 2; level > 0; level--)
  {
    pte_t *pte = &pagetable[PX(level, va)];
    if (*pte & PTE_V)
    {
      pagetable = (pagetable_t)PTE2PA(*pte);
    }
    else
    {
      if (!alloc || (pagetable = (pde_t *)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if (va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if (pte == 0)
    return 0;
  if ((*pte & PTE_V) == 0)
    return 0;
  if ((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa.
// va and size MUST be page-aligned.
// Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  if ((va % PGSIZE) != 0)
    panic("mappages: va not aligned");

  if ((size % PGSIZE) != 0)
    panic("mappages: size not aligned");

  if (size == 0)
    panic("mappages: size");

  a = va;
  last = va + size - PGSIZE;
  for (;;)
  {
    if ((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if (*pte & PTE_V)
      panic("mappages: remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if (a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t)kalloc();
  if (pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. It's OK if the mappings don't exist.
// Optionally free the physical memory.
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;
  struct proc *p;
  if ((va % PGSIZE) != 0)
  panic("uvmunmap: not aligned");
  
  for (a = va; a < va + npages * PGSIZE; a += PGSIZE)
  {
    if ((pte = walk(pagetable, a, 0)) == 0) // leaf page table entry allocated?
    continue;
    acquire(&frame_lock);
    if ((*pte & PTE_V) == 0){
      if (*pte & PTE_S) {
        if (do_free) {
          acquire(&swap_lock);
          for (struct swapslot *s = swap_table; s < &swap_table[SWAPSIZE]; s++) {
            if (s->pagetable == pagetable && s->va == a && s->in_use) {
              s->in_use = 0; // Free the swap slot!
              s->pagetable = 0;
              s->va = 0;
              s->swappedOut = 0;
              break;
            }
          }
          release(&swap_lock);
        }
        *pte = 0;
      }
      release(&frame_lock);
      continue;
    } // has physical page been allocated?
    if (do_free)
    {
      uint64 pa = PTE2PA(*pte);
      for (struct frame *f = frame_table; f < &frame_table[NFRAME]; f++)
      {
        if (f->pa == pa)
        {
          p = f->curr_proc;
          f->curr_proc = 0;
          f->in_use = 0;
          f->va = 0;
          f->pa = 0;
          f->ref_bit = 0;
          if (p != 0) {
            p->resident_pages--;
          }
          break;
        }
      }
      kfree((void *)pa);
    }
    *pte = 0;
    release(&frame_lock);
  }
}

// Allocate PTEs and physical memory to grow a process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;
  struct proc *p = myproc();
  if (newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for (a = oldsz; a < newsz; a += PGSIZE)
  {
    mem = kalloc();
    if (mem == 0)
    {
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if (mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R | PTE_U | xperm) != 0)
    {
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    struct frame *f;
    acquire(&frame_lock);
    for (f = frame_table; f < &frame_table[NFRAME]; f++)
    {
      if (f->in_use == 0)
        break;
    }
    if (f == &frame_table[NFRAME])
    {
      release(&frame_lock);
      f = evict();
      acquire(&frame_lock);
      kfree((void *)f->pa);
    }
    f->curr_proc = myproc();
    f->in_use = 1;
    f->va = a;
    f->ref_bit = 1;
    f->pa = (uint64)mem;
    p->resident_pages++;
    release(&frame_lock);
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if (newsz >= oldsz)
    return oldsz;

  if (PGROUNDUP(newsz) < PGROUNDUP(oldsz))
  {
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for (int i = 0; i < 512; i++)
  {
    pte_t pte = pagetable[i];
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0)
    {
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    }
    else if (pte & PTE_V)
    {
      panic("freewalk: leaf");
    }
  }
  kfree((void *)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void uvmfree(pagetable_t pagetable, uint64 sz)
{
  if (sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz) / PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  // find the child proc that owns the new pagetable
  struct proc *p;
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p->pagetable == new)
      break;
  }

  for (i = 0; i < sz; i += PGSIZE)
  {
    if ((pte = walk(old, i, 0)) == 0)
      continue; // page table entry hasn't been allocated
    if ((*pte & PTE_V) == 0){
      if (*pte & PTE_S) { 
        // Page is swapped out! Bring it back into RAM before copying
        swap_in(old, i);
      } else {
        continue; // It's truly unallocated, safe to skip
      }
    }
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if ((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char *)pa, PGSIZE);
    if (mappages(new, i, PGSIZE, (uint64)mem, flags) != 0)
    {
      kfree(mem);
      goto err;
    }
    if (p && p < &proc[NPROC])
    {
      struct frame *f;
      acquire(&frame_lock);
      for (f = frame_table; f < &frame_table[NFRAME]; f++)
      {
        if (f->in_use == 0)
          break;
      }
      if (f == &frame_table[NFRAME])
      {
        release(&frame_lock);
        f = evict();
        acquire(&frame_lock);
        kfree((void *)f->pa);
      }
      f->curr_proc = p;
      f->in_use = 1;
      f->va = i;
      f->ref_bit = 1;
      f->pa = (uint64)mem;
      p->resident_pages++;
      release(&frame_lock);
    }
  }
  return 0;

err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;

  pte = walk(pagetable, va, 0);
  if (pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while (len > 0)
  {
    va0 = PGROUNDDOWN(dstva);
    if (va0 >= MAXVA)
      return -1;

    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
    {
      if ((pa0 = vmfault(pagetable, va0, 0)) == 0)
      {
        return -1;
      }
    }

    pte = walk(pagetable, va0, 0);
    // forbid copyout over read-only user text pages.
    if ((*pte & PTE_W) == 0)
      return -1;

    n = PGSIZE - (dstva - va0);
    if (n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while (len > 0)
  {
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
    {
      if ((pa0 = vmfault(pagetable, va0, 0)) == 0)
      {
        return -1;
      }
    }
    n = PGSIZE - (srcva - va0);
    if (n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while (got_null == 0 && max > 0)
  {
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if (n > max)
      n = max;

    char *p = (char *)(pa0 + (srcva - va0));
    while (n > 0)
    {
      if (*p == '\0')
      {
        *dst = '\0';
        got_null = 1;
        break;
      }
      else
      {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if (got_null)
  {
    return 0;
  }
  else
  {
    return -1;
  }
}

// allocate and map user memory if process is referencing a page
// that was lazily allocated in sys_sbrk().
// returns 0 if va is invalid or already mapped, or if
// out of physical memory, and physical address if successful.
uint64
vmfault(pagetable_t pagetable, uint64 va, int read)
{
  pte_t* pte = walk(pagetable, va, 0);
  uint64 mem;
  if(pte && (*pte & PTE_S)){
    mem = swap_in(pagetable, va);
    if(!mem) panic("swap in not found");
    else return mem;
  }
  struct proc *p = myproc();
  // printf("vmfault: called va=%lu pid=%d\n", va, p->pid);
  
  if (va >= p->sz)
  {
    // printf("Hello1\n");
    return 0;
  }
  va = PGROUNDDOWN(va);
  if (ismapped(pagetable, va))
  {
    // printf("Hello2\n");
    return 0;
  }
  struct frame *f;
  acquire(&frame_lock);
  for (f = frame_table; f < &frame_table[NFRAME]; f++)
  {
    if (f->in_use == 0)
    break;
  }
  int hasEvicted = 0;
  if (f == &frame_table[NFRAME])
  {
    release(&frame_lock);
    f = evict();
    acquire(&frame_lock);
    hasEvicted = 1;
  }
  if (!hasEvicted)
  {
    mem = (uint64)kalloc();
    if (mem == 0)
    {
      release(&frame_lock);
      // printf("Hello3\n");
      return 0;
    }
    f->pa = mem;
  }
  else
  {
    mem = f->pa;
    if(mem==0){
      mem = (uint64)kalloc();
      if (mem == 0)
      {
        release(&frame_lock);
        // printf("Hello3\n");
        return 0;
      }
      f->pa = mem;
    }
  }
  memset((void *)mem, 0, PGSIZE);
  f->curr_proc = p;
  f->in_use = 1;
  f->va = va;
  f->ref_bit = 1;
  if (mappages(p->pagetable, va, PGSIZE, mem, PTE_W | PTE_U | PTE_R) != 0)
  {
    if (!hasEvicted)
    kfree((void *)mem);
    f->curr_proc = 0;
    f->in_use = 0;
    f->va = 0;
    f->pa = 0;
    f->ref_bit = 0;
    release(&frame_lock);
    // printf("Hello4\n");
    return 0;
  }
  // p->pages_evicted++;
  p->resident_pages++;
  p->page_faults++;
  release(&frame_lock);
  return mem;
}
int ismapped(pagetable_t pagetable, uint64 va)
{
  pte_t *pte = walk(pagetable, va, 0);
  if (pte == 0)
  {
    return 0;
  }
  if (*pte & PTE_V)
  {
    // printf("niggs");
    return 1;
  }
  return 0;
}

void swap_out(struct frame *choice)
{
  // char *temp = kalloc();
  // if(!temp) panic("swap_out: no memory for temp");
  pte_t *pte = walk(choice->curr_proc->pagetable, choice->va, 0);
  acquire(&swap_lock);
  struct swapslot *s;
  for (s = swap_table; s < &swap_table[SWAPSIZE]; s++)
  {
    if (s->in_use == 0)
    break;
  }
  if (s == &swap_table[SWAPSIZE])
  panic("swap table full: cannot swap out");
  s->in_use = 1;
  s->pagetable = choice->curr_proc->pagetable;
  s->va = choice->va;
  struct proc *p = choice->curr_proc;
  int index = s - swap_table;
  // printf("%d\n", index);
  int NO_OF_BLOCKS = PGSIZE / BSIZE;
  int START_BLOCK = SWAPSTART + index*NO_OF_BLOCKS;
  // struct buf* b;
  // memmove((void *)temp, (void *)choice->pa, PGSIZE);
  // Invalidate PTE while both locks are still held.
  s->swappedOut = 0;
  *pte = (*pte & ~PTE_V) | PTE_S;
  sfence_vma();
  p->resident_pages--;
  p->pages_swapped_out++;
  // Release frame_lock before potentially sleeping so sched() sees noff==1.
  release(&frame_lock);
  // while(s->swappedOut == 1){
    //   sleep((void *)s, &swap_lock);
    // }
  release(&swap_lock);
  send_request(choice->pa, 1, START_BLOCK);
  // kfree(temp);
  acquire(&swap_lock);
  s->swappedOut = 1;
  wakeup((void *)s);
  release(&swap_lock);
  acquire(&frame_lock);
}
uint64
swap_in(pagetable_t pagetable, uint64 va)
{
  // printf("starting noff=%d\n", mycpu()->noff);
  // printf("swap_in: called\n");
  // printf("swap_in: called\n");
  // printf("swapin: called va=%lu pid=%d\n", va, myproc()->pid);
  va = PGROUNDDOWN(va);
  pte_t *pte = walk(pagetable, va, 0);
  uint64 mem;
  acquire(&swap_lock);
  struct swapslot *s;
  for (s = swap_table; s < &swap_table[SWAPSIZE]; s++)
  {
    if (s->pagetable == pagetable && s->in_use == 1 && s->va == va)
    break;
  }
  if (s == &swap_table[SWAPSIZE])
  {
    release(&swap_lock);
    // printf("nigg");
    return 0;
  }
  while(s->swappedOut == 0){
    sleep((void *)s, &swap_lock);
  }
  release(&swap_lock);
  acquire(&frame_lock);
  struct frame *f;
  int hasEvicted = 0;
  struct proc *p = myproc();
  for (f = frame_table; f < &frame_table[NFRAME]; f++)
  {
    if (f->in_use == 0)
    break;
  }
  if (f == &frame_table[NFRAME])
  {
    release(&frame_lock);
    f = evict();
    acquire(&frame_lock);
    hasEvicted = 1;
  }
  if (!hasEvicted)
  {
    mem = (uint64)kalloc();
    if (mem == 0)
    {
      release(&frame_lock);
      return 0;
    }
  }
  else
  {
    mem = f->pa;
    if(mem == 0){
      mem = (uint64)kalloc();
      if (mem == 0)
      {
        release(&frame_lock);
        return 0;
      }
    }
  }
  memset((void *)mem, 0, PGSIZE);
  f->pa = mem;
  f->curr_proc = p;
  f->in_use = 2;
  f->va = va;
  f->ref_bit = 1;
  int index = s - swap_table;
  int NO_OF_BLOCKS = PGSIZE / BSIZE;
  int START_BLOCK = SWAPSTART + index*NO_OF_BLOCKS;
  // printf("noff=%d\n", mycpu()->noff);
  release(&frame_lock);
  // if(mycpu()->noff == 1) release(&swap_lock);
  send_request(mem, 0, START_BLOCK);
  // struct buf* b;
  // for(int i=0;i<NO_OF_BLOCKS;i++){
  //   // printf("mem=%ld pte2pa=%ld\n", mem, PTE2PA(*pte));
  //   // printf("noff=%d\n", mycpu()->noff);
  //   b = bread(ROOTDEV, START_BLOCK + i);
  //   memmove((void *)(mem + i*BSIZE), (void *)b->data, BSIZE);
  //   brelse(b);
  // }
  // printf("mem=%ld pte2pa=%ld\n", mem, PTE2PA(*pte));
  acquire(&frame_lock);
  uint flags = PTE_FLAGS(*pte);
  flags &= ~PTE_S; 
  if (mappages(p->pagetable, va, PGSIZE, mem, flags | PTE_V) != 0)
  {
    if (!hasEvicted)
    kfree((void *)mem);
    f->curr_proc = 0;
    f->in_use = 0;
    f->va = 0;
    f->pa = 0;
    f->ref_bit = 0;
    release(&frame_lock);
    // release(&swap_lock);
    // printf("swapped in page\n");
    return 0;
  }
  f->in_use = 1;
  acquire(&swap_lock);
  s->in_use = 0;
  s->pagetable = 0;
  s->va = 0;
  // wakeup((void *)s);
  s->swappedOut = 0;
  p->resident_pages++;
  p->pages_swapped_in++;
  p->page_faults++;
  release(&frame_lock);
  release(&swap_lock);
  return mem;
}
struct frame *
evict()
{
  // printf("evict: called\n");
  // printf("evict: called pid=%d\n",myproc()->pid);
  acquire(&frame_lock);
  struct frame *choice = 0, *start;
  int no_of_loops = 0;
  while(choice==0){
    while (clock_hand->ref_bit == 1)
    {
      if (clock_hand->in_use == 1 && clock_hand->curr_proc != 0)
      clock_hand->ref_bit = 0;
      clock_hand = clock_hand < &frame_table[NFRAME - 1] ? (clock_hand + 1) : frame_table;
      // if(clock)
    }
    int firstRun = 1;
    int min_priority = -1;
    start = clock_hand;
    while (clock_hand->ref_bit == 0)
    {
      if (clock_hand->in_use == 1 && clock_hand->curr_proc != 0 && firstRun)
      {
        choice = clock_hand;
        // start = clock_hand;
        firstRun = 0;
      }
      if (clock_hand->in_use == 1 && clock_hand->curr_proc != 0 && clock_hand->curr_proc->level > min_priority)
      {
        // else{
        choice = clock_hand;
        min_priority = clock_hand->curr_proc->level;
        // }
      }
      clock_hand = clock_hand < &frame_table[NFRAME - 1] ? (clock_hand + 1) : frame_table;
      if (clock_hand == start)
        break;
    }
    if (choice == 0) {
        no_of_loops++;
        if (no_of_loops > 2) {
            release(&frame_lock);
            panic("no valid frame");
        }
    }
  }
  // if (choice == 0)
  //   panic("no valid frame");
  struct proc *p = choice->curr_proc;
  choice->in_use = 2;  // mark in-progress so other CPUs skip this frame
  // printf("evicting page of pid %d\n", p->pid);
  swap_out(choice);
  p->pages_evicted++;
  choice->curr_proc = 0;
  release(&frame_lock);
  return choice;
}
