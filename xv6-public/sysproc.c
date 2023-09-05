#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"



int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
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

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}


// project #1을 위한 시스템 콜의 wrapper functions

// [Project 1] 시스템 콜 wrapper functions
// syscall.c의 line 117부터 보면 모든 wrapper function의 반환타입을 int로 하여 관리하는 듯합니다.
// 따라서 시스템 콜의 반환 타입이 있는 경우에 래퍼 펑션은 그 반환 값을 그대로 리턴
// void인 경우에는 (성공적으로 마쳤을 때) 0을 리턴 하도록 하였습니다.
int
sys_yield(void)
{
  yield(); return 0;
}

int
sys_getLevel(void) 
{ 
  int ret = getLevel();
  return ret;
}

int
sys_setPriority(void) {
    int pid, priority;

  // argstr -> argument를 decode해서 str 포인터를 가리키도록 한다?
  if(argint(0, &pid) < 0) {
      cprintf("[ERROR]: Insufficient arguments. SetPriority faild\n");
      return -1;
  }

  if(argint(1, &priority) < 0) {
    cprintf("[ERROR]: Insufficient arguments. SetPriority faild\n");
    return -1;
  }
  

  setPriority(pid, priority); return 0;

}

// int
// sys_setpriority(void) // args: int pid, int priority
// {

//   int pid, priority;

//   // argstr -> argument를 decode해서 str 포인터를 가리키도록 한다?
//   if(argint(0, &pid) < 0) {
//       cprintf("[ERROR]: Insufficient arguments. SetPriority faild\n");
//       return -1;
//   }

//   if(argint(0, &priority) < 0) {
//     cprintf("[ERROR]: Insufficient arguments. SetPriority faild\n");
//     return -1;
//   }
  
//   // setPriority(pid, priority); return 0;
//   return setpriority(pid, priority);
// }


// schedulerLock, Unlock 함수를 위한 help function
// 리턴 값이 0이면 num, 아니면 다른 문자가 섞임
int is_num(char* str) {
  int ret = 0;
  for(int i=0; str[i] != '\0'; ++i) {
    int digit = str[i] - '0';

    if(digit > 9 || digit < 0) {
      ret = -1; break;
    }
  }

  return ret;
}

//ulib.c에 있는 함수를 복사해왔습니다.
int
atoi(const char *s)
{
  int n;

  n = 0;
  while('0' <= *s && *s <= '9')
    n = n*10 + *s++ - '0';
  return n;
}

int
sys_schedulerLock(void) // args: int password
{
  int password;

  if(argint(0, &password) < 0) {
      cprintf("[ERROR]: SchedulerLock faild.(There's no password)\n");
      return -1;
  }

  #if DEBUG_MODE
  cprintf("line 140! lock: password is %d, arg is %d\n", PASSWORD, password);
  #endif


  if(password == PASSWORD) { //password가 일치
    schedulerLock(password); return 0; // 성공했으므로 0을 리턴합니다.
  }

  cprintf("[ERROR]: SchedulerLock faild. (Invaild password)\n");
  return -1; // 실패시에 -1을 리턴합니다

}

int
sys_schedulerUnlock(void) // args: int password 
{
  int password;

  if(argint(0, &password) < 0) {
      cprintf("[ERROR]: SchedulerUnlock faild.(There's no password)\n");
      return -1;
  }

  #if DEBUG_MODE
  cprintf("line 191! unlock: password is %d, arg is %d\n", PASSWORD, password);
  #endif


  
  if(password == PASSWORD) { //password가 일치
    schedulerUnlock(password); return 0; // 성공했으므로 0을 리턴합니다.
  }

  cprintf("[ERROR]: SchedulerUnlock faild. (Invaild password)\n");
  return -1; // 실패시에 -1을 리턴합니다

  return 0;
}

// int sys_getpid(void) {
//   return getpid();
// }


