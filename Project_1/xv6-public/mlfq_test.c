#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_LOOP 100000
#define NUM_YIELD 20000
#define NUM_SLEEP 1000

#define NUM_THREAD 8
#define MAX_LEVEL 3

int parent;

int fork_children()
{
  int i, p;
  for (i = 0; i < NUM_THREAD; i++)
    if ((p = fork()) == 0)
    {
      sleep(20);

      // yield();
      // if(getpid() == 7)
      //   setPriority(getpid(), 0);

      return getpid();
    }
  return parent;
}


int fork_children2()
{
  int i, p;
  for (i = 0; i < NUM_THREAD; i++)
  {
    if ((p = fork()) == 0)
    {
      sleep(300);
      return getpid();

    }
    else
    {
      setPriority(p, i);
      // if (r < 0)
      // {
      //   printf(1, "setpriority returned %d\n", r);
      //   exit();
      // }
    }
  }
  return parent;
}

int max_level;

int fork_children3()
{
  int i, p;
  for (i = 0; i < NUM_THREAD; i++)
  {
    if ((p = fork()) == 0)
    {
      sleep(10);
      max_level = i;
      return getpid();
    } else {
      wait();
    }
  }
  return parent;
}
void exit_children()
{
  if (getpid() != parent)
    exit();
  while (wait() != -1);
}
int main(int argc, char *argv[])
{
  int i, pid;
  int count[MAX_LEVEL] = {0};
//  int child;

  parent = getpid();

  printf(1, "MLFQ test start\n");

  printf(1, "[Test 1] default\n");
  pid = fork_children();

  if (pid != parent)
  {
    for (i = 0; i < NUM_LOOP; i++)
    {
      int x = getLevel();
      if (x < 0 || x > 4)
      {
        printf(1, "Wrong level: %d\n", x);
        exit();
      }

      // if(i % 10000 == 0) {
      //   if(pid % 4 == 3) {
      //     // __asm__("int $129");
      //     schedulerLock(2020005269);
      //     if(i % 50000) {
      //       printf(1, "getLevel: %d   %d\n",pid, x);
      //     }
      //   }
      // }

      // if(i % 10001 == 0) {
      //   if(pid % 4 == 3) {
      //     // __asm__("int $130");
      //     schedulerUnlock(2020005269);

      //   }
      // }




      count[x]++;
    }
    printf(1, "Process %d\n", pid);
    for (i = 0; i < MAX_LEVEL; i++)
      printf(1, "L%d: %d\n", i, count[i]);
  } 

  exit_children();

  printf(1, "[Test 1] finished\n");
  printf(1, "done\n");
  exit();
}
