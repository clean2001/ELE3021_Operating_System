#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "mlfq_scheduler.h"


extern struct ptable {
  struct spinlock lock;
  struct proc proc[NPROC];

  int L0_size;
  int L1_size;
  int L2_size;

  int locked_proc;
} ptable;


void
mlfq_scheduler(void) {
    #if DEBUG_MODE
    cprintf("[INFO] mlfq scheduler started.\n");
    #endif
    struct cpu *c = mycpu();
    c->proc = 0;

    struct proc* p = 0;
    struct proc* cur = 0;

    for(;;) {

        p = 0; cur = 0;

        // 인터럽트 허용
        sti();

        // custom ptable에 lock 걸기. 인터럽트를 무시
        acquire(&ptable.lock);
        
        if(ptable.locked_proc != -1) { // lock 중인 proc이 있다면 걔를 먼저 처리
            for(cur = ptable.proc; cur < &ptable.proc[NPROC]; cur++) {
                if(cur->pid == ptable.locked_proc) { // lock중인 프로세스를 찾음
                    if(cur->state == ZOMBIE) {
                        // lock을 풀어줍니다.
                        ptable.locked_proc = -1;

                        int z_ql = cur->q_level;
                        int z_qn = cur->q_num;
                        struct proc* cp = 0;
                        for(cp = ptable.proc; cp < &ptable.proc[NPROC]; cp++) {
                            if(cp->q_level == z_ql && cp->q_num > z_qn) {
                                cp->q_num--;
                            }
                        }

                        break;
                    }
                    p = cur; break;
                }
            }
        } else { // lock 중인 proc이 없다면 -> mlfq로 동작
            // ptable을 완전 탐색
            for(cur = ptable.proc; cur < &ptable.proc[NPROC]; cur++) {

                if(cur->state != RUNNABLE)
                    continue;

                int ql = cur->q_level;

                if(ql == 0) { // cur가 L0에 속한 프로세스라면
                    if(p == 0) {
                        p = cur;
                    } else if(p->q_level > 0 || p->q_num > cur->q_num) { 
                        // 기존 p가 L1, L2 큐의 것이면 무조건 선탣
                        // p와 cur의 q_num 작은 것을 선택(FCFS이므로 큐의 앞에 있는 것을 선택)
                        p = cur;
                    }
                } else if(ql == 1) { // cur가 L1에 속한 프로세스라면
                    if(p != 0 && p->q_level == 0) continue;  // L0에 속한 프로세스는 검사하지 않음
                
                    if(p == 0) { 
                        p = cur;
                    } else if(p->q_level > 1) {  // L2에 속한 프로세스라면 L1에 속한 것이 우선이므로 cur를 선택
                        p = cur;
                    } else if(p->q_level == 1 && p->q_num > cur->q_num) { // p과 cur가 둘다 L1에 속한다면, FCFS에 의한 우선순위가 높은 프로세스를 선택
                        p = cur; 
                    }

                } else if(ql == 2) { // cur가 L2에 속한 프로세스라면

                    // min_priority: 현재까지 나온 L2 프로세스의 priority 중 가장 작은 값

                    if(p != 0 && p->q_level < 2) continue;  // L0, L1에 속한 프로세스는 검사하지 않음

                    if(p == 0) {
                        p = cur;
                    } else if(p->priority > cur->priority) {  // priority 값이 작은(우선순위가 높은)것을 선택
                        p = cur;
                    } else if(cur->priority == p->priority && p->q_num > cur->q_num) {
                        p = cur;   // cur와 p의 priority가 같다면 q_num이 작은 것을 선택
                    } else if(cur->priority == p->priority && p->q_num > cur->q_num && cur->pid < p->pid) {
                        p = cur;   // cur와 p의 q_num도 같다면 pid 작은 것을 선택
                    }

                } else {  // 나올 수 없는 케이스. 코드가 제대로 동작하는지를 확인하기 위해 넣은 부분입니다.
                    #if DEBUG_MODE
                    cprintf("[DEBUG MODE]: Invalid ql\n");
                    #endif
                }
                
            }
        }



        if(p == 0) {
            release(&ptable.lock); continue;
        }

        if(p->state != RUNNABLE) continue;

        // ptable 전체를 순회한 후, p가 최종적으로 선택된 프로세스입니다.
        // 프로세스 p가 cpu를 이용합니다.
        c->proc = p;
        switchuvm(p);
        p->state = RUNNING;

        swtch(&(c->scheduler), p->context);
        switchkvm();

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;


        // 여기서 호출된 popcli가 뭘까? -> 인터럽트 다시 허용인듯함.
        release(&ptable.lock);
    }

}


// [Project 1] my custom system call functions

// 현재 cpu가 처리하고 있는 프로세스의 q_level을 반환합니다.
int
getLevel(void) {
    int ret;
    acquire(&ptable.lock);
    struct proc *p = myproc();

    ret = p->q_level;

    release(&ptable.lock);

    return ret;
}

void
setPriority(int pid, int priority) {
    // cprintf("[debug] %d %d", pid, priority);

    // exception: priority 값이 0~3이 아닐 경우 -> 콘솔에 에러문을 띄우고 함수를 종료합니다.
    if(priority < 0 || priority > 3) {
        cprintf("[Error] setPriority failed (invaild priority %d)\n", priority);
        return; // 프로세스는 종료하지 않고 return을 하며 나옵니다.
    }
    struct proc* cur = 0;
    int is_found = 0;  // pid가 일치하는 프로세스를 찾았는지 검사하는 변수입니다.

    acquire(&ptable.lock);
    for(cur = ptable.proc; cur < &ptable.proc[NPROC]; cur++) {

        if(cur->state == ZOMBIE || cur->state == UNUSED) continue;

        if(cur->pid == pid) {
            cur->priority = priority;
            is_found = 1;
            break;
        }
    }
    release(&ptable.lock);

    // exception: 해당 pid를 가진 프로세스가 존재하지 않을 경우 -> 콘솔에 에러문을 띄우고 함수를 종료합니다.
    if(is_found == 0) {  // 
        cprintf("[ERROR] setPriority failed (there's no matching pid)\n");
    }

}



//  [Project 1] priority boosting 함수
void
priority_boosting(void) {

    struct proc* cur = 0;
    // int locked = -1;
    int i=0;
    ptable.L0_size = 0;
    ptable.L1_size = 0;
    ptable.L2_size = 0;

    if(ptable.locked_proc != -1) { //뭔가 우선적으로 처리되고 있었다면 lock을 풀어줍니다.
        _schedulerUnlock(2020005269); // locked 된게 있었다면 이 안에서 L0 queue의 맨앞에 locked 됐던 process가 위치하게 됩니다.
    } else { //lock된 것이 없다면 L0에 있는 프로세스를 먼저 L0에 담습니다.
        // L0 프로세스를 L0으로 이동시키고, priority = 3으로 설정하고, time quantum을 초기화합니다.
        for(cur = ptable.proc; cur < &ptable.proc[NPROC]; cur++) {
            if(cur->state == UNUSED || cur->state == ZOMBIE) continue;
            if(cur->q_level != 0) continue;

            cur->q_level = 0; // L0로 이동
            cur->priority = 3; // priority = 3으로 초기화
            cur->proc_ticks = 0; // time quantum을 초기화
            cur->q_num = i++;
            ptable.L0_size++;
        }

    }

    // L1 프로세스를 L0으로 이동시키고, priority = 3으로 설정하고, time quantum을 초기화합니다.
    for(cur = ptable.proc; cur < &ptable.proc[NPROC]; cur++) {
        if(cur->state == UNUSED || cur->state == ZOMBIE) continue;
        if(cur->q_level != 1) continue;

        cur->q_level = 0; // L0로 이동
        cur->priority = 3; // priority = 3으로 초기화
        cur->proc_ticks = 0; // time quantum을 초기화
        cur->q_num = i++;
        ptable.L0_size++;

    }

    // L2 프로세스를 L0으로 이동시키고, priority = 3으로 설정하고, time quantum을 초기화합니다.
    for(cur = ptable.proc; cur < &ptable.proc[NPROC]; cur++) {
        if(cur->state == UNUSED || cur->state == ZOMBIE) continue;
        if(cur->q_level != 2) continue;

        cur->q_level = 0; // L0로 이동
        cur->priority = 3; // priority = 3으로 초기화
        cur->proc_ticks = 0; // time quantum을 초기화
        cur->q_num = i++;
        ptable.L0_size++;

    }

    acquire(&tickslock);
    ticks = 0; // global ticks를 초기화.
    release(&tickslock);
}


// 인터럽트로 인한 호출을 위한 ptable.lock을 걸지 않는 버전입니다.
void _schedulerLock(int password) {

    if(password != PASSWORD) { // 안전성을 위해 한번 더 검사
        cprintf("[ERROR]: SchedulerLock faild. (Password is incorrect)\n");
        return;
    }

    if(ptable.locked_proc != -1) { // exception: lock을 걸었는데 이미 걸려있다면 그냥 리턴합니다.
        cprintf("[INFO]: Already locked!\n");
        return;
    }
    
    if(myproc()->state == ZOMBIE) {
        cprintf("[INFO]: zombie lock.\n");
        return;
    }
    ptable.locked_proc = mycpu()->proc->pid;
    
}

void _schedulerUnlock(int password) {

    if(password != PASSWORD) { // 안전성을 위해 한번 더 검사
        cprintf("[ERROR]: SchedulerUnlock faild. (Password is incorrect)\n");
        return;
    }


    if(ptable.locked_proc == -1) { // exception: unlock을 걸었는데 lock이 걸려있지 않다면 그냥 리턴합니다
        cprintf("[INFO]: Already Unlocked!\n");
        return;
    }

    if(myproc()->q_level == 1) ptable.L1_size--;
    else if(myproc()->q_level == 2) ptable.L2_size--;

    int pid = ptable.locked_proc;
    ptable.locked_proc = -1; // lock이 걸려있지 않은 상태로 만들어줍니다.

    // int q_num = -1;

    ptable.L0_size = 0;

    // L0 queue의 맨 앞으로 돌려보내야함
    struct proc* cur = 0;
    for(cur = ptable.proc; cur < &ptable.proc[NPROC]; cur++) {
        if(cur->pid == pid) {
            ptable.L0_size++;
            cur->q_level = 0;
            cur->priority = 3;
            cur->proc_ticks = 0;
            cur->q_num = 0;

            break;
        }

    }

    for(cur = ptable.proc; cur < &ptable.proc[NPROC]; cur++) {
        if(cur->pid == pid) continue;
        if(cur->q_level == 0) {
            ptable.L0_size++;
            cur->q_num++;
        }
    }

    return;
    
}


void schedulerLock(int password) {

    acquire(&ptable.lock);
    _schedulerLock(password);    
    release(&ptable.lock);
}


void schedulerUnlock(int password) {

    acquire(&ptable.lock);
    _schedulerUnlock(password);
    release(&ptable.lock);

    return;
    
}


void
update_proc(void) {


    int ql = myproc()->q_level;

        
    struct proc* cur = 0;

    // 1 tick 마다 yield는 발생해야하므로 FCFS에 의한 우선 순위를 조정해준다.
    for(cur = ptable.proc; cur < &ptable.proc[NPROC]; cur++) {
        if(cur->pid == myproc()->pid) continue;
        if(cur->q_level == ql && cur->q_num > 0) {
            cur->q_num--;
        }

    }

    // 현재 proc의 우선 순위는 맨 나중으로 조정.
    if(ql == 0) {
        myproc()->q_num = ptable.L0_size-1;
    } else if(ql == 1) {
        myproc()->q_num = ptable.L1_size-1;

    } else {
        myproc()->q_num = ptable.L2_size-1;
    }

    /************타임퀀텀 체크************/
    if(ql == 0 && myproc()->proc_ticks == L0_TIME_QUANTUM) { // 타임 퀀텀 다 되면

    #if DEBUG_MODE
    cprintf("[DEBUG MODE] q level down 0 -> 1\n");
    #endif
    // L1 큐로 내려주기
    myproc()->q_level++;
    ptable.L0_size--;
    ptable.L1_size++;
    // FCFS에 따른 우선순위를 바꿔주기
    myproc()->q_num = ptable.L1_size-1;

    // local tick 초기화
    myproc()->proc_ticks = 0;


    } else if(ql == 1 && myproc()->proc_ticks == L1_TIME_QUANTUM) { // L1에 속하고, 타임 퀀텀 다 되면

    #if DEBUG_MODE
    cprintf("[DEBUG MODE] q level down 1 -> 2\n");
    #endif

    // L2 큐로 내려주기
    myproc()->q_level++;
    ptable.L1_size--;
    ptable.L2_size++;


    // FCFS에 따른 우선순위를 바꿔주기
    myproc()->q_num = ptable.L2_size-1;

    // local tick 초기화
    myproc()->proc_ticks = 0;

    } else if(ql == 2 && myproc()->proc_ticks == L2_TIME_QUANTUM) { // L2에 속하고, 타임 퀀텀 다 되면
        // local tick 초기화
        myproc()->proc_ticks = 0;

        if(myproc()->priority >= 1) myproc()->priority--;

        // int q_n = myproc()->q_num;

        myproc()->q_num = ptable.L2_size-1;


    }


}
