#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "mlfq_scheduler.h"
#include "traps.h"

extern struct ptable {
  struct spinlock lock;
  struct proc proc[NPROC];

  int L0_size;
  int L1_size;
  int L2_size;

  int locked_proc;
} ptable;

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
    SETGATE(idt[i], 0, SEG_KCODE<<3, vectors[i], 0);
    SETGATE(idt[T_SYSCALL], 1, SEG_KCODE<<3, vectors[T_SYSCALL], DPL_USER);
    SETGATE(idt[USERINT], 1, SEG_KCODE<<3, vectors[USERINT], DPL_USER);

    // [Project 1] user mode에서도 인터럽트가 호출될 수 있도록 함
    SETGATE(idt[T_SCHE_LOCK], 1, SEG_KCODE<<3, vectors[T_SCHE_LOCK], DPL_USER);
    SETGATE(idt[T_SCHE_UNLOCK], 1, SEG_KCODE<<3, vectors[T_SCHE_UNLOCK], DPL_USER);
    SETGATE(idt[T_BOOST], 1, SEG_KCODE<<3, vectors[T_BOOST], DPL_USER);


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
      wakeup(&ticks);
      release(&tickslock);
    }

  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER) {
        // 현재 프로세스 proc tick 증가시키기
        myproc()->proc_ticks++;

        // cprintf("[DEBUG q_num , level is] %d %d\n", myproc()->q_num, myproc()->q_level);

        update_proc();

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
  case USERINT:
    mycall();
    break;
  // [Project 1] priority boosting (T_BOOST == 131)
  case T_BOOST:
    priority_boosting();
    break; 
  // [Project 1] SchedulerLock (T_SCHE_LOCK == 129)
  case T_SCHE_LOCK:
    schedulerLock(2020005269);
    break;
    // [Project 1] SchedulerUnlock (T_SCHE_LOCK == 130)
  case T_SCHE_UNLOCK:
    schedulerUnlock(2020005269);
    break;


  //PAGEBREAK: 13
  default:
    // 유저모드에서 실행중인지 확인하는 코드, 커널에서 실행중이라는 뜻임
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
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER) {
    exit();
  }

  // [Project 1] priority boosting (131 interrupt)
  if(ticks == 100 && tf->trapno == T_IRQ0+IRQ_TIMER) {
    priority_boosting();
  }

  // Force process to give up CPU on clock tick.
  // If interrupts were on while locks held, would need to check nlock.
  // 기존에는 1 tick 마다 yield 하던 부분을 L0~L2 큐가 각각 다른 타임퀀텀을 갖도록 수정했습니다.
  if(myproc() && myproc()->state == RUNNING &&
     tf->trapno == T_IRQ0+IRQ_TIMER) {
      #if DEBUG_MODE
      cprintf("[DEBUG] pid is %d\n", myproc()->pid);
      #endif

      yield();
  }

  // Check if the process has been killed since we yielded
  // 비트 연산을 통해서 모드 판단하는 부분
  if(myproc() && myproc()->killed && (tf->cs&3) == DPL_USER) {
    exit();
  }
}
