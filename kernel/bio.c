// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
} bcache;

struct {
  struct buf head;
  struct spinlock lock;
} hashtable[BCACHEBUC];

void
binit(void)
{
  struct buf *b;
  char lkname[7];
  int i;

  initlock(&bcache.lock, "bcache");
  for(i = 0; i < BCACHEBUC; i++){
    snprintf(lkname, sizeof(lkname), "bcache_%d", i);
    initlock(&hashtable[i].lock, lkname);
    hashtable[i].head.next = 0;
  }

  // Create linked list of buffers
  i = 0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = hashtable[i].head.next;
    b->prev = &hashtable[i].head;
    initsleeplock(&b->lock, "buffer");
    if(hashtable[i].head.next)
    {
      hashtable[i].head.next->prev = b;
      hashtable[i].head.next = b;
    }
    i = (i + 1) % BCACHEBUC;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucno = blockno % BCACHEBUC;

  acquire(&hashtable[bucno].lock);

  // Is the block already cached?
  for(b = hashtable[bucno].head.next; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&hashtable[bucno].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  uint time = __INT_MAX__;
  struct buf *select_buf = 0;
  
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = hashtable[bucno].head.next; b != 0; b = b->next){
    if(b->refcnt == 0 && time > b->timestamp){
      select_buf = b;
      time = b->timestamp;
    }
  }

  if(select_buf)
  {
    select_buf->dev = dev;
    select_buf->blockno = blockno;
    select_buf->valid = 0;
    select_buf->refcnt = 1;
    release(&hashtable[bucno].lock);
    acquiresleep(&select_buf->lock);
    return select_buf;  
  }

  acquire(&bcache.lock);

global_find:
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    if(b->refcnt == 0 && time > b->timestamp){
        select_buf = b;
        time = b->timestamp;
    }
  }

  if(select_buf)
  {
    if(select_buf->blockno % BCACHEBUC == bucno)
    {
      release(&bcache.lock);
      select_buf->dev = dev;
      select_buf->blockno = blockno;
      select_buf->valid = 0;
      select_buf->refcnt = 1;
      release(&hashtable[bucno].lock);
      acquiresleep(&select_buf->lock);
      return select_buf;
    } else {
      acquire(&hashtable[select_buf->blockno % BCACHEBUC].lock);
      if(select_buf->refcnt != 0)
      {
        release(&hashtable[select_buf->blockno % BCACHEBUC].lock);
        goto global_find;
      }
      // delete from original bucket
      select_buf->prev->next = select_buf->next;
      if(select_buf->next)
        select_buf->next->prev = select_buf->prev;
      release(&hashtable[select_buf->blockno % BCACHEBUC].lock);
      
      select_buf->prev = &hashtable[bucno].head;
      select_buf->next = hashtable[bucno].head.next;
      if(hashtable[bucno].head.next)
      {
        hashtable[bucno].head.next->prev = select_buf;
        hashtable[bucno].head.next = select_buf;
      }
      release(&bcache.lock);

      select_buf->dev = dev;
      select_buf->blockno = blockno;
      select_buf->valid = 0;
      select_buf->refcnt = 1;
      release(&hashtable[bucno].lock);
      acquiresleep(&select_buf->lock);
      return select_buf;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  int bucno = b->blockno % BCACHEBUC;

  releasesleep(&b->lock);

  acquire(&hashtable[bucno].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->timestamp = ticks;
  }
  
  release(&hashtable[bucno].lock);
}

void
bpin(struct buf *b) {
  int bucno = b->blockno % BCACHEBUC;
  acquire(&hashtable[bucno].lock);
  b->refcnt++;
  release(&hashtable[bucno].lock);
}

void
bunpin(struct buf *b) {
  int bucno = b->blockno % BCACHEBUC;
  acquire(&hashtable[bucno].lock);
  b->refcnt--;
  release(&hashtable[bucno].lock);
}


