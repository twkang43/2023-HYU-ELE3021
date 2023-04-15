#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table (shared by all CPUs).
struct gatedesc idt[256];
extern uint vectors[];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint ticks;

void
tvinit(void)
{
  int i;

  for(i = 0; i < 256; i++)
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0); // default : kernel mode
  SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER); // exception : system call - user level
  SETGATE(idt[T_USERINT], 1, SEG_KCODE<<3, vectors[T_USERINT], DPL_USER); // user interrupt (Practice)
  SETGATE(idt[T_SCHLOCK], 1, SEG_KCODE<<3, vectors[T_SCHLOCK], DPL_USER); // scheduler lock - user level
  SETGATE(idt[T_SCHUNLOCK], 1, SEG_KCODE<<3, vectors[T_SCHUNLOCK], DPL_USER); // scheduler unlock - user level

  initlock(&tickslock, "time");
}

void
idtinit(void)
{
  lidt(idt, sizeof(idt));
}

//PAGEBREAK: 41
void
trap(struct trapframe *tf)
{
  if(tf->trapno == T_SYSCALL){
    if(myproc()->killed)
      exit();
    myproc()->tf = tf;
    syscall();
    if(myproc()->killed)
      exit();
    return;
  }

  switch(tf->trapno){
  case T_IRQ0 + IRQ_TIMER:
    if(cpuid() == 0){
      acquire(&tickslock);
      ticks++;

      // Increment a runtime
      if(myproc() && myproc()->state==RUNNING){
        ++myproc()->runtime;
        //cprintf("pid '%d' : runtime - %d\n", myproc()->pid, myproc()->runtime);
      }
      
      wakeup(&ticks);
      release(&tickslock);
    }
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE:
    ideintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_IDE+1:
    // Bochs generates spurious IDE1 interrupts.
    break;
  case T_IRQ0 + IRQ_KBD:
    kbdintr();
    lapiceoi();
    break;
  case T_IRQ0 + IRQ_COM1:
    uartintr();
    lapiceoi();
    break;
  case T_IRQ0 + 7:
  case T_IRQ0 + IRQ_SPURIOUS:
    cprintf("cpu%d: spurious interrupt at %x:%x\n",
            cpuid(), tf->cs, tf->eip);
    lapiceoi();
    break;
  case T_USERINT: // User Interrupt
    mycall();
    break;
  case T_SCHLOCK:
    schedulerLock(2021025205); // parameter 확인 필요
    break;
  case T_SCHUNLOCK:
    schedulerUnlock(2021025205);
    break;

  //PAGEBREAK: 13
  default:
    if(myproc() == 0 || (tf->cs&3) == 0){
      // In kernel, it must be our mistake.
      cprintf("unexpected trap %d from cpu %d eip %x (cr2=0x%x)\n",
              tf->trapno, cpuid(), tf->eip, rcr2());
      panic("trap");
    }
    // In user space, assume process misbehaved.
    cprintf("pid %d %s: trap %d err %d on cpu %d "
            "eip 0x%x addr 0x%x--kill proc\n",
            myproc()->pid, myproc()->name, tf->trapno,
            tf->err, cpuid(), tf->eip, rcr2());
    myproc()->killed = 1;
  }

  // Force process exit if it has been killed and is in user space.
  // (If it is still executing in the kernel, let it keep running
  // until it gets to the regular system call return.)
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  if(myproc() && myproc()->state == RUNNING &&
    tf->trapno == T_IRQ0 + IRQ_TIMER){
     // Check if there is a process which spent all of time it got.
     // L0 Queue Timeout
    if(myproc()->runtime >= 4 && myproc()->queue == L0 && myproc()->monopoly == 0){
      myproc()->queue = L1; // Move the current process to the L1 queue.

      deleteList(myproc(), myqueue(L0));
      addListEnd(myproc(), myqueue(L1));

      /*
      printList();
      cprintf("pid '%d' : L0->L1\n", myproc()->pid);
      */

      myproc()->runtime = 0;
    }
    // L1 Queue Timeout
    if(myproc()->runtime >= 6 && myproc()->queue == L1 && myproc()->monopoly == 0){
      myproc()->queue = L2; // Move the current process to the L2 queue.

      deleteList(myproc(), myqueue(L1));
      addListEnd(myproc(), myqueue(L2));

      /*
      printList();
      cprintf("pid '%d' : L1->L2\n", myproc()->pid);
      */

      myproc()->runtime = 0;
    }
    // L2 Queue Timeout
    if(myproc()->runtime >= 8 && myproc()->queue == L2 && myproc()->monopoly == 0){
      if(myproc()->priority > 0)
        --myproc()->priority;

      myproc()->runtime = 0;
      //printList();
    }

    yield();
  }

  // Priority boosting when ticks >= 100.
  if(ticks >= 100){
    if(schlock)
      schedulerUnlock(2021025205);
    boosting();
  }
  

  // Check if the process has been killed since we yielded
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER)
    exit();
}
