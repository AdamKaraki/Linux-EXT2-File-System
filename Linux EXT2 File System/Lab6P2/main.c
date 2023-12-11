#include "type.h"

/********** globals **************/
PROC   proc[NPROC];
PROC   *running;

MINODE minode[NMINODE];   // in memory INODES
MINODE *freeList;         // free minodes list
MINODE *cacheList;        // cached minodes list

MINODE *root;             // root minode pointer

OFT    oft[NOFT];         // for level-2 only

char gline[256];          // global line hold token strings of pathname
char *name[64];           // token string pointers
int  n;                   // number of token strings                    

int ninodes, nblocks;     // ninodes, nblocks from SUPER block
int bmap, imap, inodes_start, iblk;  // bitmap, inodes block numbers

int inode_size, inodes_per_block, ifactor; // Used in mailman's algorithm

int  fd, dev;
char cmd[16], pathname[128],pathname2[128], parameter[128];
int  requests, hits;

// start up files
#include "util.c"
#include "symlink.c"
#include "cd_ls_pwd.c"
#include "mkdir_rmdir_creat.c"
#include "dalloc.c"
#include "alloc.c"
#include "link_unlink.c"


int init()
{
  int i, j;
  // initialize minodes into a freeList
  for (i=0; i<NMINODE; i++){
    MINODE *mip = &minode[i];
    mip->dev = mip->ino = 0;
    mip->id = i;
    mip->next = &minode[i+1];
  }
  minode[NMINODE-1].next = 0;
  freeList = &minode[0];       // free minodes list

  cacheList = 0;               // cacheList = 0

  for (i=0; i<NOFT; i++)
    oft[i].shareCount = 0;     // all oft are FREE
 
  for (i=0; i<NPROC; i++){     // initialize procs
     PROC *p = &proc[i];    
     p->uid = p->gid = i;      // uid=0 for SUPER user
     p->pid = i+1;             // pid = 1,2,..., NPROC-1

     for (j=0; j<NFD; j++)
       p->fd[j] = 0;           // open file descritors are 0
  }
  
  running = &proc[0];          // P1 is running
  requests = hits = 0;         // for hit_ratio of minodes cache
}

char *disk = "diskimage";

int main(int argc, char *argv[ ]) 
{
  char line[128];
  char buf[BLKSIZE];

  init();
  fd = dev = open(disk, O_RDWR);
  printf("dev = %d\n", dev);  // YOU should check dev value: exit if < 0
  if(dev < 0)
    exit(1);

  // get super block of dev
  get_block(dev, 1, buf);
  SUPER *sp = (SUPER *)buf;  // you should check s_magic for EXT2 FS
  if(sp->s_magic == 0xEF53)
    printf("Disk image is an ext2 file system\n");
  else {
    printf("Disk image not a ext2 file system\n");
    exit(2);
  }
  
  ninodes = sp->s_inodes_count;
  nblocks = sp->s_blocks_count;
  printf("ninodes=%d  nblocks=%d\n", ninodes, nblocks);

  inode_size = sp->s_inode_size;
  inodes_per_block = BLKSIZE / inode_size;
  ifactor = inode_size/sizeof(INODE);
  printf("inode_size = %d inodes_per_block = %d ifactor = %d\n", inode_size, inodes_per_block, ifactor);

  get_block(dev, 2, buf);
  GD *gp = (GD *)buf;

  bmap = gp->bg_block_bitmap;
  imap = gp->bg_inode_bitmap;
  iblk = inodes_start = gp->bg_inode_table;
  printf("bmap=%d  imap=%d  iblk=%d\n", bmap, imap, iblk);


  root = iget(dev, 2); // Set root and cwd
  running->cwd = iget(dev, 2);

  while(1){
    printf("P%d running\n", running->pid);
    pathname[0] = parameter[0] = 0; 
    printf("enter command [cd|ls|pwd|mkdir|rmdir|creat|link|unlink|symlink|show|hits|exit] : ");
    fgets(line, 128, stdin);
    line[strlen(line)-1] = 0;    // kill \n at end

    if (line[0]==0)
        continue;
    sscanf(line, "%s %s %64c", cmd, pathname, parameter);
    printf("pathname=%s parameter=%s\n", pathname, parameter);
      
    if (strcmp(cmd, "ls")==0)
      ls(pathname);
    if (strcmp(cmd, "cd")==0)
       chdir(pathname);
    if (strcmp(cmd, "pwd")==0)
       pwd(running->cwd);    
    if (strcmp(cmd, "show")==0) 
      show_dir(running->cwd);
    if (strcmp(cmd, "hits")==0)
      hit_ratio();
    if(strcmp(cmd, "mkdir") == 0)
      make_dir(pathname);
    if(strcmp(cmd, "rmdir") == 0){
      if (!strcmp(pathname, ".") || !strcmp(pathname, "..") || !strcmp(pathname, "/")) {
          printf("ERROR: %s cannot be deleted\n", pathname);
          continue;
          }
      rmdir(pathname);}
    if(strcmp(cmd, "link") == 0)
      link(pathname, parameter);
    if(strcmp(cmd, "unlink") == 0)
      unlink(pathname);
    if(strcmp(cmd, "symlink") == 0){
      sscanf(line, "%s %s %s", cmd, pathname, pathname2);
      symlink(pathname,pathname2);}
    if(strcmp(cmd, "creat") == 0)
      creat_file(pathname);
    if (strcmp(cmd, "exit")==0)
      quit();
  }
}

int show_dir(MINODE *mip)
{
    // show contents of mip DIR: same as in LAB5
    char sbuf[BLKSIZE], temp[256];
    DIR *dp;
    char *cp;

    // ASSUME only one data block i_block[0]
    // YOU SHOULD print i_block[0] number here
    mip = iget(dev, mip->ino); // Only here to make hits match
    INODE *ip = &(mip->INODE);
    get_block(dev, ip->i_block[0], sbuf);  

    dp = (DIR *)sbuf;
    cp = sbuf;
  
    while(cp < sbuf + BLKSIZE){
        strncpy(temp, dp->name, dp->name_len);
        temp[dp->name_len] = 0;
        
        printf("%4d %4d %4d %s\n", dp->inode, dp->rec_len, dp->name_len, temp);
        
        cp += dp->rec_len;
        dp = (DIR *)cp;
    }

    iput(mip);
}

int hit_ratio()
{
  printCacheList();

  // compute and print hit_ratio
  double hit_ratio = 0.0;
  if(requests != 0)
    hit_ratio = (hits * 1.0)/requests;
  printf("requests = %d hits = %d hit ratio: %f\n", requests, hits, hit_ratio);
}

int quit()
{
   MINODE *mip = cacheList;
   while(mip){
     if (mip->shareCount){
        mip->shareCount = 1;
        iput(mip);    // write INODE back if modified
     }
     mip = mip->next;
   }
   exit(0);
}

void printFreeList() {
  MINODE *mip = freeList;

  while(mip) {
    printf("(%d, %d) ", dev, mip->ino);
    mip = mip->next;
  }

  printf("\n");
}


