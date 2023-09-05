typedef uint thread_t;      // [Project 2] 스레드 번호


// Per-CPU state
struct cpu
{
  uchar apicid;              // Local APIC ID
  struct context *scheduler; // swtch() here to enter scheduler
  struct taskstate ts;       // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS]; // x86 global descriptor table
  volatile uint started;     // Has the CPU started?
  int ncli;                  // Depth of pushcli nesting.
  int intena;                // W ere interrupts enabled before pushcli?
  struct proc *proc;         // The process running on this cpu or null
};

extern struct cpu cpus[NCPU];
extern int ncpu;

// PAGEBREAK: 17
//  Saved registers for kernel context switches.
//  Don't need to save all the segment registers (%cs, etc),
//  because they are constant across kernel contexts.
//  Don't need to save %eax, %ecx, %edx, because the
//  x86 convention is that the caller has saved them.
//  Contexts are stored at the bottom of the stack they
//  describe; the stack pointer is the address of the context.
//  The layout of the context matches the layout of the stack in swtch.S
//  at the "Switch stacks" comment. Switch doesn't save eip explicitly,
//  but it is on the stack and allocproc() manipulates it.
struct context
{
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate
{
  UNUSED,
  EMBRYO,
  SLEEPING,
  RUNNABLE,
  RUNNING,
  ZOMBIE
};

// Per-process state
struct proc
{
  uint sz;                    // Size of process memory (bytes) -> 공유
  pde_t *pgdir;               // Page table -> 공유
  int pid;                    // Process ID -> 공유
  struct proc *parent;        // 부모 스레드
  int killed;                 // If non-zero, have been killed -> 공유
  struct file *ofile[NOFILE]; // Open files ->
  struct inode *cwd;          // Current directory
  char name[16];              // Process name (debugging)

  // [Project 2] 추가 변수 (프로세스에서 필요한 애들)
  uint memlim;                // 메모리 리밋. 0이면 리밋없음
  int stacksize;              // 스택 몇장 받았는지.
  int tnum;                   // 이 프로세스가 몇개의 스레드 갖고 있는지, (자신 제외)
  int parent_pid;             // 부모 프로세스의 pid.


  // [Project 2] 스레드끼리 각자 가지고 있는 애들
  char *kstack;            // Bottom of kernel stack for this process -> 각자
  enum procstate state;    // Process state -> 각자
  struct trapframe *tf;    // Trap frame for current syscall -> 각자
  struct context *context; // swtch() here to run process -> 각자
  void *chan;                 // If non-zero, sleeping on chan -> 각자
  struct proc *main;          // 메인 스레드. 즉 어떤 프로세스의 스레드인지를 나타냄


  // [Project 2] 추가 변수 (스레드에서 필요한 애들)
  thread_t tid;                // 스레드이면 1이상의 값(nexttid로 결정), 프로세스이면 0임.
  void* retval;             // exit, join에서 쓰이는 return value
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap


// int thread_create(thread_t *thread, void *(*start_routine)(void *), void *arg);
// void thread_exit(void *retval);
// int thread_join(thread_t thread, void **retval);