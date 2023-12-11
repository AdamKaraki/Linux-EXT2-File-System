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