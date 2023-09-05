#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

// #define B_DIRTY 0x4

// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

// Contents of the header block, used for both the on-disk header block
// and to keep track in memory of logged block# before commit.

extern struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  struct buf head;
} bcache;

struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // how many FS sys calls are executing.
  int committing;  // in commit(), please wait.
  int dev;
  struct logheader lh;
};
struct log log;

static void recover_from_log(void);
static void commit();

void
initlog(int dev)
{
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  struct superblock sb;
  initlock(&log.lock, "log");
  readsb(dev, &sb);
  log.start = sb.logstart;
  log.size = sb.nlog;
  log.dev = dev;
  recover_from_log();
}

// Copy committed blocks from log to their home location
static void
install_trans(void)
{
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start+tail+1); // read log block
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]); // read dst
    memmove(dbuf->data, lbuf->data, BSIZE);  // copy block to dst
    bwrite(dbuf);  // write dst to disk
    brelse(lbuf);
    brelse(dbuf);
  }
}

// 프젝 3에서 만든 함수
static void
flush_to_disk(void)
{
  int i = 0;
  struct buf *b;
  // acquire(&log.lock);
  for (i=0; i<NBUF; i++) {
    cprintf("line 104 %d\n", i);
    b = &(bcache.buf[i]);

    if(b->flags & B_DIRTY) {
      struct buf *dbuf = bread(b->dev, b->blockno);
      bwrite(dbuf);
      brelse(dbuf);
    }
  }

  log.lh.n = 0;
  cprintf("line 114\n");

  // release(&log.lock);
}


// Read the log header from disk into the in-memory log header
static void
read_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// Write in-memory log header to disk.
// This is the true point at which the
// current transaction commits.
static void
write_head(void)
{
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *hb = (struct logheader *) (buf->data); // 바뀐 애들이 저장됨 // 로그 헤더로 옮기는거구나
  int i;
  hb->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    hb->block[i] = log.lh.block[i];
  }
  bwrite(buf); // 이거는 왜써주는 걸까. 이게 로그를 쓰는 걸까. 이게 디스크에 쓰는 로그인가
  brelse(buf);
}

static void
recover_from_log(void)
{
  read_head();
  install_trans(); // if committed, copy from log to disk
  log.lh.n = 0;
  write_head(); // clear the log
}

// called at the start of each FS system call.
// void
// begin_op(void)
// {
//   acquire(&log.lock);
//   while(1){
//     if(log.committing){
//       sleep(&log, &log.lock);
//     } else if(log.lh.n + (log.outstanding+1)*MAXOPBLOCKS > LOGSIZE){
//       // this op might exhaust log space; wait for commit.
//       sleep(&log, &log.lock);
//     } else {
//       log.outstanding += 1;
//       release(&log.lock);
//       break;
//     }
//   }
// }

void begin_op(void)
{
  cprintf("line 145");
  acquire(&log.lock);
  cprintf("line 146\n");
  while(1){
    if(log.committing){
      // cprintf("sleep!!\n");
      // sleep(&log, &log.lock);
      // cprintf("wake!!\n");
    }else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
  // release(&log.lock);
}

// called at the end of each FS system call.
// commits if this was the last outstanding operation.
// void
// end_op(void)
// {
//   int do_commit = 0;

//   acquire(&log.lock);
//   log.outstanding -= 1;
//   if(log.committing)
//     panic("log.committing");
//   if(log.outstanding == 0){
//     do_commit = 1;
//     log.committing = 1;
//   } else {
//     // begin_op() may be waiting for log space,
//     // and decrementing log.outstanding has decreased
//     // the amount of reserved space.
//     wakeup(&log);
//   }
//   release(&log.lock);

//   if(do_commit){
//     // call commit w/o holding locks, since not allowed
//     // to sleep with locks.
//     commit();
//     acquire(&log.lock);
//     log.committing = 0;
//     wakeup(&log);
//     release(&log.lock);
//   }
// }

void
end_op(void) 
{
  cprintf("line 198\n");
  acquire(&log.lock);
  cprintf("line 200\n");
  log.outstanding -= 1;
  release(&log.lock);
}

// Copy modified blocks from cache to log.
// static void
// write_log(void)
// {
//   int tail;

//   for (tail = 0; tail < log.lh.n; tail++) {
//     struct buf *to = bread(log.dev, log.start+tail+1); // log block
//     struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block
//     memmove(to->data, from->data, BSIZE);
//     bwrite(to);  // write the log
//     brelse(from);
//     brelse(to);
//   }
// }

static void
commit()
{
  // acquire(&log.lock);
  if (log.lh.n > 0) {
    cprintf("line 264\n");
    // write_log();     // Write modified blocks from cache to log -> 일부러 크래시 내는 일 없을테니까 일단 지움
    // write_head();    // Write header to disk -- the real commit -> 이제부터 로그 헤더 안쓸거니까 지움
    // install_trans(); // Now install writes to home locations
    // acquire(&log.lock);
    flush_to_disk();  // 디스크로 내려가!..
    // log.lh.n = 0;
    // release(&log.lock);
    // write_head();    // Erase the transaction from the log
  }
  // release(&log.lock);
}

// Caller has modified b->data and is done with the buffer.
// Record the block number and pin in the cache with B_DIRTY.
// commit()/write_log() will do the disk write.
//
// log_write() replaces bwrite(); a typical use is:
//   bp = bread(...)
//   modify bp->data[]
//   log_write(bp)
//   brelse(bp)
void
log_write(struct buf *b)
{
  // int i;

  // if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1) {
  //   panic("too big a transaction");
  // }

  if (log.outstanding < 1)
    panic("log_write outside of trans");

  acquire(&log.lock);
  // for (i = 0; i < log.lh.n; i++) {
  //   if (log.lh.block[i] == b->blockno)   // log absorbtion
  //     break;
  // }
  // log.lh.block[i] = b->blockno;
  // if (i == log.lh.n)
  //   log.lh.n++;

  if(b->flags & B_DIRTY) { //처음 바뀐거니까 카운트
    log.lh.n++;
  }

  b->flags |= B_DIRTY; // prevent 
  
  release(&log.lock);
}

// 실패시 -1, 성공시 플러시된 블록 수 반환
int
sync(void)
{
  cprintf("싱크!\n");
  int do_commit = 0;
  int ret = 0;
  acquire(&log.lock);
  // log.outstanding -= 1;

  if(log.committing)
    panic("log.committing");

  if(log.outstanding == 1){ // 나만 쓰고 있음 플러시 해도 됨
    do_commit = 1;
    log.committing = 1;
    ret = log.lh.n; // 플러시된 블록 수를 반환
  } else {
    // begin_op() may be waiting for log space,
    // and decrementing log.outstanding has decreased
    // the amount of reserved space.
    // wakeup(&log);
  }
  log.committing = 1;
  ret = log.lh.n;
  do_commit = 1;
  release(&log.lock);

  if(do_commit){
    // call commit w/o holding locks, since not allowed
    // to sleep with locks.
    // acquire(&log.lock);
    commit();
    log.committing = 0;
    // wakeup(&log);
    return ret;
  }

  // release(&log.lock);
  return -1;

}