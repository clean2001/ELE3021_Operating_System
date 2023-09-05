#define Q_MAX_LEN   64
#define H_MAX_LEN   65

#include "spinlock.h"


struct L_queue // L0, L1에 해당하는 원형 큐를 의미한다
{
    int q_size; // 큐의 크기 (현재 들어있는 프로세스 개수)
    int start; // 큐의 시작 포인터
    int end; // 큐의 끝 포인터 
    struct proc* queue[Q_MAX_LEN];
};

struct L_heap // L2에 해당하는 priority queue
{
    int h_size; // 큐의 크기 (현재 들어있는 프로세스 개수)
    int start; // 큐의 시작 포인터
    int end; // 큐의 끝 포인터 
    struct proc* heap[H_MAX_LEN];
};

// // mlfq schduler를 위한 custom ptable
// struct my_ptable {
//     struct spinlock lock;

//     struct L_queue L0_queue;
//     struct L_queue L1_queue;
//     struct L_heap L2_queue; // priority queue임 (min heap)
// };

void
update_proc(void);

void init_queue(struct L_queue* q);
void init_heap(struct L_heap* h);
struct proc* delete_heap(struct L_heap* h);
void h_insert(struct L_heap *h);

void insert_mlfq(struct proc* p);

void
init_mlfq_scheduler();

struct proc*
process_L0(struct cpu *c);

struct proc*
process_L1(struct cpu *c);

struct proc*
process_L2(struct cpu *c);

void
mlfq_scheduler(void);

void
priority_boosting(void);

void
_schedulerLock(int password);

void 
_schedulerUnlock(int password);

int getlev(void);

void
setPriority(int pid, int priority);