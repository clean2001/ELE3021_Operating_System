#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
int nexttid = 1; // 프젝2 스레드를 위한
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->tid = 0; // 프젝 2에서 추가. 메인 스레드(=프로세스)임을 의미
  p->main = p;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;

  release(&ptable.lock);
}


//PAGEBREAK: 32
// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  int limit;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  sz = curproc->sz;
  limit = curproc->memlim;

  if(limit > 0 && limit < sz+2*PGSIZE) { // 리밋 체크
    release(&ptable.lock);
    return -1;
  }

  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0) {
      release(&ptable.lock);
      return -1;
    }
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0) {
      release(&ptable.lock);
      return -1;
    }
  }
  // curproc->sz = sz;
  release(&ptable.lock);

  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();
  struct proc *main;

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // 메인 스레드 가져오기
  acquire(&ptable.lock);
  main = curproc->main;
  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = main->sz; // 이게 맞나..?

  np->parent = curproc; // 부모는 현재 스레드. 얘가 wait해줌.
  np->parent_pid = curproc->pid; // exit함수를 위한 변수

  *(np->tf) = *(curproc->tf);

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  // 다른 변수들을 init
  np->killed = 0;
  np->memlim = curproc->memlim;
  np->stacksize = curproc->stacksize;
  np->tnum = 0;


  np->state = RUNNABLE;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *main;
  struct proc *p, *parent;
  int fd, pid;


  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);
  // main을 가져오자
  main = curproc->main;
  parent = main->parent;
  pid = main->pid;

  // Parent might be sleeping in wait().
  curproc->parent = parent;
  wakeup1(parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent_pid == curproc->pid && p->pid != pid) { // 자신의 부모인 스레드가 죽은 것이므로 부모 바꿔주기
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }

    if(p->pid == pid) { 
      p->parent = parent;
      p->state = ZOMBIE;
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p, *t;
  int havekids, pid;
  struct proc *curproc = myproc();
  pde_t* pgdir;

  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      
      havekids = 1;


      if(p->state == ZOMBIE) { // 얘가 좀비라는건 다 exit()됐다는거임
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        pgdir = p->pgdir;
        freevm(pgdir);
        p->pgdir = 0;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;

        // [프젝 2] 새로 추가된 변수들
        p->memlim = 0;
        p->stacksize = 0;
        p->tnum = 0;
        p->tid = 0;
        p->retval = 0;
        
        p->state = UNUSED;

        // 나와 같은 프로세스에 속한 스레드 정리
        for(t = ptable.proc; t < &ptable.proc[NPROC]; t++) {
          if(t == p) // 방금 해제한 스레드이면 패스
            continue;

          if(t->pid == pid) { // 정리
            kfree(t->kstack);
            t->kstack = 0;
            t->pid = 0;
            t->parent = 0;
            t->name[0] = 0;
            t->killed = 0;
            t->pgdir = 0;

            t->memlim = 0;
            t->stacksize = 0;
            t->tnum = 0;
            t->tid = 0;
            t->retval = 0;
            t->state = UNUSED;
          }
        }
        release(&ptable.lock);
        return pid;
      }
    }


    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;

      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock

  myproc()->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid && p->tid == 0){ // 메인 스레드여야함
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}


/* [Project 2]
pmanager를 위한 시스템콜
*/

// list를 위한 pmlist
void
pmlist(void) {
    struct proc* p = 0;
    int sz = 0;

    cprintf("name\t pid\t stacksize\t memsize\t\t memlim\n");

    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){

      if(p->state != RUNNABLE && p->state != RUNNING && p->state != SLEEPING) {
        continue;
      }

      
      if(p->tid != 0) // 만약 메인 스레드가 아니라면 뛰어넘는다.
        continue;

      sz = p->sz; // main의 sz를 출력하는 것으로 통일

      cprintf("%s\t %d\t %d\t %d\t\t %d\n",p->name, p->pid, p->stacksize, sz, p->memlim);

    }

    release(&ptable.lock);
    cprintf("\n");
}

// memlim을 위한 setmemorylimit syscall
int
setmemorylimit(int pid, int limit) {
  struct proc* p = 0, *t;
  int memsize = 0;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid && p->tid == 0) // tid가 0이어야 프로세스임.
      goto found;
    
  }

  release(&ptable.lock);
  return -1;

//pid 인거 찾음
found:
  // sz+(thread개수)*2*4096이 limit보다 작거나 같은지 검사
  memsize = p->sz;
  if(limit > 0 && memsize > limit) {
    // cprintf("[INFO] Fail to setmemorylimit. New limit is lower than memsize of proc\n");
    release(&ptable.lock);
    return -1;
  }

  p->memlim = limit; // 성공적으로 limit 바꿈
  for(t = ptable.proc; t < &ptable.proc[NPROC]; t++){
    if(p == t)
      continue;
    
    if(t->pid == pid) // 스레드들도 모두 바꿔주기
      t->memlim = limit;
  }

  release(&ptable.lock);
  return 0;
}


// 정상 -> 0 ,  에러 -> -1
int thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg) {
  struct proc *t, *curproc, *main;
  uint st = 0;
  uint sz = 0;
  int i = 0;
  char *sp;
  // uint ustack[2];
  pde_t * pgdir;

  curproc = myproc();
  

  acquire(&ptable.lock);

  for(t = ptable.proc; t < &ptable.proc[NPROC]; t++)
    if(t->state == UNUSED)
      goto found;

  release(&ptable.lock);
  cprintf("[thread_create] fail");
  return -1;

found:
  t->state = EMBRYO;

  // release(&ptable.lock);

  // Allocate kernel stack.
  if((t->kstack = kalloc()) == 0){
    t->state = UNUSED;
    return 0;
  }
  sp = t->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *t->tf;
  t->tf = (struct trapframe*)sp;
  *(t->tf) = *(curproc->tf); // 부모 스레드의 tf값 복사

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;


  sp -= sizeof *t->context;
  t->context = (struct context*)sp;
  memset(t->context, 0, sizeof *t->context);
  t->context->eip = (uint)forkret;
  /////////////////////////////////////////////// thread alloc

  // 메인 스레드 가져오기
  main = curproc->main;

  pgdir = main->pgdir;


  t->tid = nexttid++;
  *thread = t->tid;

  t->parent = curproc; // 부모는 현재 스레드이다. 얘가 join 해줌
  t->main = main; // 메인 스레드 넣어주기
  t->pid = main->pid;
  t->pgdir = pgdir;

  // 프젝 2에서 추가된 변수들도 다 카피
  t->memlim = curproc->memlim;
  t->stacksize = curproc->stacksize;
  t->tnum = curproc->tnum;
  
  t->retval = 0; // 그냥 초기화 -> 사실 안해도 될것 같은데
  t->parent_pid = main->pid;   // exit함수를 위한 변수

  sz = main->sz;

  // 새로운 스레드가 생겨도 memlim을 넘지 않는지 확인
  if(main->memlim > 0 && main->memlim < sz + 2*PGSIZE) { 
    cprintf("[thread_create] fail. exceed memlim\n");
    t->state = UNUSED;
    kfree(t->kstack);
    release(&ptable.lock);
    return -1;
  }


  if((sz = allocuvm(pgdir, sz, sz+2*PGSIZE)) == -1) { // 얘는 exit에서 freevm에서 한꺼번에 해제
    t->state = UNUSED;
    kfree(t->kstack);

    release(&ptable.lock);
    return -1;
  }
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));


  t->sz = sz; // 초기 sz는 페이지 2장
  main->sz = sz;
  st = sz;


  safestrcpy(t->name, main->name, sizeof(main->name));

  st -= 4;
  *(uint *)st = (uint)arg;
  st -= 4;
  *(uint *)st = (uint)0xffffffff;
  t->sz = sz;


  t->tf->eip = (uint)start_routine; // 여기도 문제가 아닌거 같고
  t->tf->esp = (uint)st; //이게 문제가 아니네
  release(&ptable.lock);


  for(i = 0; i < NOFILE; i++)
    if(main->ofile[i])
      t->ofile[i] = filedup(main->ofile[i]); // Incrementing file ref count
  t->cwd = idup(main->cwd);

  t->state = RUNNABLE;


  return 0;
}


// exit에서 reval의 값을 lwp 안의 retval에 넣어주고, join에서 그 retval을 받는건가
// 
void thread_exit(void *retval) {
  // cprintf("thread exit!\n");
  int fd;
  // int heap_size = 0;
  struct proc* cur = myproc(); // 현재 스레드


  if(cur == initproc)
    panic("init exiting");


  acquire(&ptable.lock);
  cur->retval = retval;
  // 더 할당받은 영역이 있다면? -> growproc으로 빼주기
  // heap_size = cur->heap_size;
  release(&ptable.lock);


  // Close all open files. -> 래퍼런스 감소
  for(fd = 0; fd < NOFILE; fd++){
    if(cur->ofile[fd]){
      fileclose(cur->ofile[fd]);
      cur->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(cur->cwd);
  end_op();
  cur->cwd = 0;


  // if(heap_size) {
  //   if(growproc(-heap_size) == -1) {
  //     panic("[thread_exit] something went wrong.\n");
  //   }
  // }

  acquire(&ptable.lock);
  wakeup1(cur->parent); // 부모 스레드 깨우기. (메인 스레드)
  cur->state = ZOMBIE; // join에서 ZOMBIE를 UNUSED로 바꿔주기
  sched();
  panic("[thread_exit] zombie exit\n");
}

// wait 함수 같은거임
// 무한 루프 돌면서 자다가 자식이 ZOMBIE되면 UNUSED로 바꿔주자.
// heap 공간은.. exit에서 해제해줘야겠다...
int thread_join(thread_t thread, void **retval) {

  struct proc *curproc = myproc();
  struct proc *main;
  struct proc *t;
  
  acquire(&ptable.lock);

  // 메인 스레드 가져오기
  main = curproc->main;
  for(t = ptable.proc; t < &ptable.proc[NPROC]; t++){
    if(t->tid == thread) {
      if(t->pid != curproc->pid) {
        cprintf("[thread_join] join failed. The thread is not mine\n");
        return -1;
      }
      goto found;
    }
  }

  cprintf("[thread_join] no tid thread %u\n", thread);
  return -1;

found:
  for(;;) {
    if(t->state == ZOMBIE) {
      kfree(t->kstack); // 커널 스택 반납
      t->kstack = 0;

      t->pid = 0;
      t->tid = 0;
      t->parent = 0;
      t->name[0] = 0;
      t->state = UNUSED;
      
      t->memlim = 0;
      t->stacksize = 0;
      t->tnum = 0;
      t->pgdir = 0;

      *retval = t->retval;

      t->retval = 0;

      release(&ptable.lock);

      return 0;
    }

    // 메인 스레드 죽었음 -> exit가서 강제 종료
    if(curproc->killed || main->killed) {
      release(&ptable.lock);
      return -1;
    }

    // 기다리는 스레드가 좀비가 될 때까지 자자
    sleep(curproc, &ptable.lock);
  }

}

