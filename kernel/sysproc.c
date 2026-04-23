#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "mlfqinfo.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "disk_swap.h"

extern struct proc proc[NPROC];
extern struct spinlock wait_lock;

uint64
sys_exit(void)
{
  // struct proc* p = myproc();
  // acquire(&p->lock);
  // p->syscallcount++;
  // release(&p->lock);
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  // struct proc* p = myproc();
  // acquire(&p->lock);
  // p->syscallcount++;
  // release(&p->lock);
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  // struct proc* p = myproc();
  // acquire(&p->lock);
  // p->syscallcount++;
  // release(&p->lock);
  return kfork();
}

uint64
sys_wait(void)
{
  // struct proc* pp = myproc();
  // acquire(&pp->lock);
  // pp->syscallcount++;
  // release(&pp->lock);
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  // struct proc* p = myproc();
  // acquire(&p->lock);
  // p->syscallcount++;
  // release(&p->lock);
  uint64 addr;
  int t;
  int n;
  
  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;
  
  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      printf("hello w");
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
    return -1;
    if(addr + n > TRAPFRAME)
    return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  // struct proc* p = myproc();
  // acquire(&p->lock);
  // p->syscallcount++;
  // release(&p->lock);
  int n;
  uint ticks0;
  
  argint(0, &n);
  if(n < 0)
  n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  // struct proc* p = myproc();
  // acquire(&p->lock);
  // p->syscallcount++;
  // release(&p->lock);
  int pid;
  
  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  // struct proc* p = myproc();
  // acquire(&p->lock);
  // p->syscallcount++;
  // release(&p->lock);
  uint xticks;
  
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// prints hello message
uint64
sys_hello(void)
{
  // struct proc* p = myproc();
  // acquire(&p->lock);
  // p->syscallcount++;
  // release(&p->lock);
  printf("Hello from the kernel!\n");
  return 0;
}

// returns pid
uint64
sys_getpid2(void)
{
  // struct proc* p = myproc();
  // acquire(&p->lock);
  // p->syscallcount++;
  // release(&p->lock);
  return myproc()->pid;
}

// returns pid of parent process
uint64
sys_getppid(void)
{
  // struct proc* p = myproc();
  // acquire(&p->lock);
  // p->syscallcount++;
  // release(&p->lock);
  if(myproc()->parent) return myproc()->parent->pid;
  else return -1;
}

// returns number of children
uint64
sys_getnumchild(void)
{
  // struct proc* p = myproc();
  // acquire(&p->lock);
  // p->syscallcount++;
  // release(&p->lock);
  int num = 0;
  acquire(&wait_lock);
  for(struct proc* p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->parent == myproc()){
      if(p->state != ZOMBIE) num++;
    }
    release(&p->lock);
  }
  release(&wait_lock);
  return num;
}

uint64
sys_getsyscount(void)
{
  // struct proc* p = myproc();
  // acquire(&p->lock);
  // p->syscallcount++;
  // release(&p->lock);
  return myproc()->syscallcount;
}

uint64
sys_getchildsyscount(void)
{
  int pid;
  argint(0, &pid);
  acquire(&wait_lock);
  for(struct proc* p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if(p->pid == pid && p->parent == myproc()){
      release(&p->lock);
      release(&wait_lock);
      return p->syscallcount;
    }
    release(&p->lock);
  }
  release(&wait_lock);
  return -1;
}

uint64
sys_getlevel(void)
{
  return myproc()->level;
}

uint64
sys_getmlfqinfo(void)
{
  int pid;
  argint(0, &pid);
  uint64 info;
  argaddr(1, &info);
  return mlfqstat(pid, info);
}

uint64
sys_getvmstats(void){
  int pid;
  argint(0, &pid);
  uint64 info;
  argaddr(1, &info);
  return vmstat(pid, info);
  // int pid;
}

uint64
sys_setdisksched(void){
  int Policy;
  argint(0, &Policy);
  if(Policy != 1 && Policy != 0) return -1;
  policy = Policy;
  return 0;
  // int pid;
}