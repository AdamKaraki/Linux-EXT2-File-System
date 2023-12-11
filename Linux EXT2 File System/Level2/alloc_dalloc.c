/*********** globals in main.c ***********/
extern PROC   proc[NPROC];
extern PROC   *running;

extern MINODE minode[NMINODE];   // minodes
extern MINODE *freeList;         // free minodes list
extern MINODE *cacheList;        // cacheCount minodes list

extern MINODE *root;

extern OFT    oft[NOFT];

extern char gline[256];   // global line hold token strings of pathname
extern char *name[64];    // token string pointers
extern int  n;            // number of token strings                    

extern int ninodes, nblocks;
extern int bmap, imap, inodes_start, iblk;  // bitmap, inodes block numbers

extern int  fd, dev;
extern char cmd[16], pathname[128], parameter[128];
extern int  requests, hits;

int tst_bit(char *buf, int bit)
{
  int i = bit / 8;
  int j = bit % 8;
  return buf[i] & (1 << j); 
}

// Set a bit to 1
int set_bit(char *buf, int bit)
{
    // in Chapter 11.3.1
    int i = bit / 8;
    int j = bit % 8;
    buf[i] |= (1 << j);
}

int decFreeInodes(int dev)
{
  char buf[BLKSIZE];

  // dec free inodes count by 1 in SUPER and GD
  get_block(dev, 1, buf);
  SUPER *sp = (SUPER *)buf;
  sp->s_free_inodes_count--;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  GD *gp = (GD *)buf;
  gp->bg_free_inodes_count--;
  put_block(dev, 2, buf);
}

int ialloc(int dev)  // allocate an inode number from imap block
{
  int  i;
  char buf[BLKSIZE];

  // read inode_bitmap block
  get_block(dev, imap, buf);

  for (i=0; i < ninodes; i++){
    if (tst_bit(buf, i)==0){
       set_bit(buf,i);
       put_block(dev, imap, buf);
		
       decFreeInodes(dev);

       printf("ialloc : ino=%d\n", i+1);		
       return i+1;
    }
  }
  return 0;
}

int decFreeBlocks(int dev)
{
  char buf[BLKSIZE];

  // dec free inodes count by 1 in SUPER and GD
  get_block(dev, 1, buf);
  SUPER *sp = (SUPER *)buf;
  sp->s_free_blocks_count--;
  put_block(dev, 1, buf);

  get_block(dev, 2, buf);
  GD *gp = (GD *)buf;
  gp->bg_free_blocks_count--;
  put_block(dev, 2, buf);
}

int balloc(int dev) { // Returns a free disk block number
    int  i;
    char buf[BLKSIZE];

    // read inode_bitmap block
    get_block(dev, bmap, buf);

    for (i=0; i < nblocks; i++){
        if (tst_bit(buf, i)==0){
        set_bit(buf,i);
        put_block(dev, bmap, buf);
            
        decFreeBlocks(dev);

        printf("balloc : bno=%d\n", i+1);		
        return i+1;
        }
    }
    return 0;
}

int clr_bit(char *buf, int bit) // clear bit in char buf[BLKSIZE]
{ buf[bit/8] &= ~(1 << (bit%8)); }

int incFreeInodes(int dev)
{
  char buf[BLKSIZE];

  // inc free inodes count in SUPER and GD
  SUPER *sp = (SUPER *) buf;
  get_block(dev, 1, buf);
  sp = (SUPER *)buf;
  sp->s_free_inodes_count++;
  put_block(dev, 1, buf);

  GD *gp = (SUPER *)buf;
  get_block(dev, 2, buf);
  gp = (GD *) buf;
  gp->bg_free_inodes_count++;
  put_block(dev, 2, buf);
}

int idalloc(int dev, int ino)  // deallocate an ino number
{
  int i;  
  char buf[BLKSIZE];

  // return 0 if ino < 0 OR > ninodes

  // get inode bitmap block
  get_block(dev, imap, buf);
  clr_bit(buf, ino-1);

  // write buf back
  put_block(dev, imap, buf);

  // update free inode count in SUPER and GD
  incFreeInodes(dev);
  return 1;
}
int incFreeBlocks(int dev)
{
    char buf[BLKSIZE];

    get_block(dev, 1, buf);
    SUPER *sp = (SUPER *)buf;
    sp->s_free_blocks_count++;
    put_block(dev, 1, buf);

    get_block(dev, 2, buf);
    GD *gp = (GD *)buf;
    gp->bg_free_blocks_count++;
    put_block(dev, 2, buf);
}
int bdalloc(int dev, int blk) // deallocate a blk number
{
  char buf[BLKSIZE];

  // return 0 if blk < 0 OR > nblocks
  if (blk < 0 || blk >= nblocks) {
    return 0;
  }

  // get block bitmap block
  get_block(dev, bmap, buf);

  // clear the corresponding bit in the bitmap block
  clr_bit(buf, blk-1);

  // write buf back to disk
  put_block(dev, bmap, buf);

  // update free block count in SUPER and GD
  incFreeBlocks(dev);

  return 1; // success
}