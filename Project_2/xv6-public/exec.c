#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"
#include "spinlock.h"

extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

// 나와 pid가 같은 애들을 나빼고 모두 종료
void
thread_remove(struct proc* curproc)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if(p == curproc) // 나는 죽이면 안됨
      continue;

    if(p->pid != curproc->pid) // 다른 프로세스의 스레드는 죽이면 안됨
      continue;

    if(p == curproc->main) {
      curproc->parent = p->parent;
    }

    //죽이자
    kfree(p->kstack); // 커널 스택 반납
    p->kstack = 0;

    p->pid = 0;
    p->tid = 0;
    p->parent = 0;
    p->name[0] = 0;
    p->state = UNUSED;
    
    p->memlim = 0;
    p->stacksize = 0;
    p->tnum = 0;
    p->pgdir = 0;

    p->retval = 0;
  }

  // 내 정보 갱신
  curproc->tid = 0;
  curproc->memlim = 0;
  curproc->stacksize = 0;
  curproc->tnum = 0;
  curproc->main = curproc;

}

int
exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *curproc = myproc();
  acquire(&ptable.lock);
  // 여기서 다른 스레드를 종료시키자. -> 나를 제외한 나와 pid가 같은 모두.
  thread_remove(curproc);
  ///////////////////////////////////////////////////////////////////
  release(&ptable.lock);


  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip);
  pgdir = 0;


  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf)) {
    cprintf("line 62\n");
    goto bad;

  }
  if(elf.magic != ELF_MAGIC) {
    cprintf("line 67\n");
    goto bad;

  }

  if((pgdir = setupkvm()) == 0) {
    cprintf("line 73\n");
    goto bad;

  }

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph)) {
      cprintf("line 82\n");
      goto bad;

    }
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz){
      cprintf("line 89\n");
      goto bad;

    }
    if(ph.vaddr + ph.memsz < ph.vaddr) {
      cprintf("line 94\n");
      goto bad;
    }

    acquire(&ptable.lock);
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0) {
      release(&ptable.lock);
      goto bad;
    }
    release(&ptable.lock);

    if(ph.vaddr % PGSIZE != 0) {
      cprintf("line 103\n");
      goto bad;

    }
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0) {
      cprintf("line 108\n");
      goto bad;

    }
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  acquire(&ptable.lock);

  sz = PGROUNDUP(sz);
  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0) {
    release(&ptable.lock);
    goto bad;

  }
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE));
  sp = sz;
  release(&ptable.lock);


  // [프젝 2] 만약 sz가 memlim보다 크면? (memlim == 0 일때 말고)
  if(curproc->memlim > 0 && curproc->memlim < sz) {
    cprintf("line 131\n");
    goto bad;

  }

  curproc->stacksize = 1; // exec는 스택 한장

  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG) {
    cprintf("line 140\n");
    goto bad;

    }
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0) {
    cprintf("line 146\n");
    goto bad;

    }
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0) {
    cprintf("line 160\n");
    goto bad;

  }

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

  // Commit to the user image.
  oldpgdir = curproc->pgdir;
  curproc->pgdir = pgdir;
  curproc->sz = sz;
  curproc->tf->eip = elf.entry;  // main
  curproc->tf->esp = sp;
  switchuvm(curproc);

  acquire(&ptable.lock);
  freevm(oldpgdir);
  
  if(curproc->state != RUNNABLE)
    curproc->state = RUNNABLE;
  release(&ptable.lock);
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}


// 나중에 구현-> 다른 스레드들을 정리해주는 과정이 필요하다.
// 정리: 스레드들의 상태 unused로 만들어주고, sbrk로 메모리 정리해주고, kstack 해제 해주고, 값을 다 정리해주자 -> 이걸 함수로
int
exec2(char *path, char **argv, int stacksize)
{
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3+MAXARG+1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  // 여기서 다른 스레드를 종료시키자. -> 나를 제외한 나와 pid가 같은 모두.
  thread_remove(curproc);
  ///////////////////////////////////////////////////////////////////
  release(&ptable.lock);

  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;

  // Load program into memory.
  sz = 0;
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;

    acquire(&ptable.lock);
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    release(&ptable.lock);

    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // 프젝 2. stacksize가 유효한지 검사
  if(stacksize <= 0 || stacksize > 100)
    goto bad;

  acquire(&ptable.lock);
  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE*(1+stacksize))) == 0) {
    release(&ptable.lock);
    goto bad;
  }
  clearpteu(pgdir, (char*)(sz - 2*PGSIZE*(1+stacksize)));
  sp = sz;
  release(&ptable.lock);


  // [프젝 2] 만약 sz가 memlim보다 크면? (memlim == 0 일때 말고)
  if(curproc->memlim > 0 && curproc->memlim < sz)
    goto bad;

  curproc->stacksize = stacksize; // exec2는 스택 stacksize장


  // Push argument strings, prepare rest of stack in ustack.
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3+argc] = sp;
  }
  ustack[3+argc] = 0;

  ustack[0] = 0xffffffff;  // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc+1)*4;  // argv pointer

  sp -= (3+argc+1) * 4;
  if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

  // Commit to the user image.
  oldpgdir = curproc->pgdir;
  curproc->pgdir = pgdir;
  curproc->sz = sz;
  curproc->tf->eip = elf.entry;  // main
  curproc->tf->esp = sp;
  switchuvm(curproc);
  acquire(&ptable.lock);
  freevm(oldpgdir);

  if(curproc->state != RUNNABLE)
    curproc->state = RUNNABLE;
  release(&ptable.lock);


  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

