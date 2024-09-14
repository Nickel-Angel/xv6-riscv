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
  struct buf bucket_head[NBUFBUCKET];
  struct spinlock bucket_lock[NBUFBUCKET];
} bcache;

void
binit(void)
{
  struct buf *b;
  char lkname[7];
  int i;

  initlock(&bcache.lock, "bcache");
  for(i = 0; i < NBUFBUCKET; i++){
    snprintf(lkname, sizeof(lkname), "bcache_%d", i);
    initlock(&bcache.bucket_lock[i], lkname);
    bcache.bucket_head[i].next = 0;
    bcache.bucket_head[i].prev = 0;
  }

  // Create linked list of buffers
  i = 0;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.bucket_head[i].next;
    b->prev = &bcache.bucket_head[i];
    b->blockno = i; // init hash result
    b->timestamp = 0;
    b->refcnt = 0;
    initsleeplock(&b->lock, "buffer");
    if(bcache.bucket_head[i].next)
      bcache.bucket_head[i].next->prev = b;
    bcache.bucket_head[i].next = b;
    i = (i + 1) % NBUFBUCKET;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int bucno = blockno % NBUFBUCKET;

  acquire(&bcache.bucket_lock[bucno]);

  // Is the block already cached?
  for(b = bcache.bucket_head[bucno].next; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bucket_lock[bucno]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  
  // Not cached.
  release(&bcache.bucket_lock[bucno]);
  
  acquire(&bcache.lock);

  for(b = bcache.bucket_head[bucno].next; b != 0; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      acquire(&bcache.bucket_lock[bucno]);
      b->refcnt++;
      release(&bcache.bucket_lock[bucno]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
    
  // Still not cached.
  // Recycle the least recently used (LRU) unused buffer.
  struct buf *select_buf = 0;
  int pre_bucket = -1; // previous selected buf in which bucket
  for(int i = 0; i < NBUFBUCKET; ++i){
    acquire(&bcache.bucket_lock[i]);
    for(b = bcache.bucket_head[i].next; b != 0; b = b->next){
      if(b->refcnt == 0 && (select_buf == 0 || select_buf->timestamp > b->timestamp)){
        select_buf = b;
        if (pre_bucket != i && pre_bucket != -1)
            release(&bcache.bucket_lock[pre_bucket]);
        pre_bucket = i;
      }
    }
    if(pre_bucket != i)
      release(&bcache.bucket_lock[i]);
  }

  if(select_buf)
  {
    if(pre_bucket != bucno)
    {
      // delete from original bucket
      if(select_buf->prev)
        select_buf->prev->next = select_buf->next;
      if(select_buf->next)
        select_buf->next->prev = select_buf->prev;
      release(&bcache.bucket_lock[pre_bucket]);
      
      acquire(&bcache.bucket_lock[bucno]);
      select_buf->prev = &bcache.bucket_head[bucno];
      select_buf->next = bcache.bucket_head[bucno].next;
      if(bcache.bucket_head[bucno].next)
        bcache.bucket_head[bucno].next->prev = select_buf;
      bcache.bucket_head[bucno].next = select_buf;     
    }
    select_buf->dev = dev;
    select_buf->blockno = blockno;
    select_buf->valid = 0;
    select_buf->refcnt = 1;
    release(&bcache.bucket_lock[bucno]);
    release(&bcache.lock);
    acquiresleep(&select_buf->lock);
    return select_buf;
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

  releasesleep(&b->lock);

  int bucno = b->blockno % NBUFBUCKET;

  acquire(&bcache.bucket_lock[bucno]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->timestamp = ticks;
  }
  
  release(&bcache.bucket_lock[bucno]);
}

void
bpin(struct buf *b) {
  int bucno = b->blockno % NBUFBUCKET;
  acquire(&bcache.bucket_lock[bucno]);
  b->refcnt++;
  release(&bcache.bucket_lock[bucno]);
}

void
bunpin(struct buf *b) {
  int bucno = b->blockno % NBUFBUCKET;
  acquire(&bcache.bucket_lock[bucno]);
  b->refcnt--;
  release(&bcache.bucket_lock[bucno]);
}


