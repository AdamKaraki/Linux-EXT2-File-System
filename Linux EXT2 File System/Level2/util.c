//#include "type.h"
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
extern int inode_size, inodes_per_block, ifactor; // Used in mailman's algorithm
extern int bmap, imap, inodes_start, iblk;  // bitmap, inodes block numbers

extern int  fd, dev;
extern char cmd[16], pathname[128], parameter[128];
extern int  requests, hits;

/**************** util.c file **************/

int get_block(int dev, int blk, char buf[ ])
{
  lseek(dev, blk*BLKSIZE, SEEK_SET);
  int n = read(fd, buf, BLKSIZE);
  return n;
}

int put_block(int dev, int blk, char buf[ ])
{
  lseek(dev, blk*BLKSIZE, SEEK_SET);
  int n = write(fd, buf, BLKSIZE);
  return n; 
}       

int tokenize(char *pathname)
{
  strcpy(gline, pathname);
   char *s = strtok(gline, "/");
   n = 0;
   while(s) {
      name[n++] = s;
      s = strtok(NULL, "/");
   }
}

MINODE *dequeue(MINODE **list) {
  MINODE *temp = *list;
  MINODE *res = NULL;
  if(temp) {
    res = temp;
    
    //temp = temp->next; // Wasn't changing list pointer for some reason
    (*list) = (*list)->next;
    res->next = 0;
  }
  return res;
}

int enqueue(MINODE **cacheList, MINODE *mip) {
  // enter p into queue by cacheCount; FIFO if same cacheCount. Lowest cacheCount has highest priority
  MINODE *q = *cacheList;
    if (q == 0 || mip->cacheCount < q->cacheCount) {
        *cacheList = mip;
        mip->next = q;
    } else {
        while (q->next && mip->cacheCount >= q->next->cacheCount) 
          q = q ->next;
        
        mip->next = q->next;
        q->next = mip;
    }
}

MINODE *iget(int dev, int ino) // return minode pointer of (dev, ino)
{
  /********** Write code to implement these ***********
  1. search cacheList for minode=(dev, ino);
  if (found){
    inc minode's cacheCount by 1;
    inc minode's shareCount by 1;
    return minode pointer;
  }
  */

  MINODE *mip = cacheList;
  requests++;

  while(mip) {
    if(mip->dev == dev && mip->ino == ino) {
      hits++;
      mip->cacheCount++;
      mip->shareCount++;
      reorderCache(&cacheList);
      return mip;
    }
    mip = mip->next;
  }

  /*
  // needed (dev, ino) NOT in cacheList
  2. if (freeList NOT empty){
        remove a minode from freeList;
        set minode to (dev, ino), cacheCount=1 shareCount=1, modified=0;

        load INODE of (dev, ino) from disk into minode.INODE;

        enter minode into cacheList; 
        return minode pointer;
    }
  */

  mip = dequeue(&freeList);
  
  if(mip) {
    mip->dev = dev;
    mip->ino = ino;
    mip->cacheCount = mip->shareCount = 1;
    mip->modified = 0;

    char buf[BLKSIZE];
    int blk = (ino - 1) / inodes_per_block + iblk;
    int offset = (ino - 1) % inodes_per_block;
    get_block(dev, blk, buf);

    INODE *ip = (INODE *)buf + offset * ifactor;
    mip->INODE = *ip;
    enqueue(&cacheList, mip);
    return mip;  
  }

  /*
  // freeList empty case:
  3. find a minode in cacheList with shareCount=0, cacheCount=SMALLest
    set minode to (dev, ino), shareCount=1, cacheCount=1, modified=0
    return minode pointer;

  NOTE: in order to do 3:
      it's better to order cacheList by INCREASING cacheCount,
      with smaller cacheCount in front ==> search cacheList
  ************/

  mip = cacheList;

  while(mip && mip->shareCount != 0)
    mip = mip->next;

  if(!mip) {
    printf("No available nodes in cacheList\n");
    return 0;
  }

  mip->dev = dev;
  mip->ino = ino;
  mip->shareCount = mip->cacheCount = 1;
  mip->modified = 0;

  return mip;
}

int iput(MINODE *mip)  // release a mip
{
 
    if (mip==0)                return;

    mip->shareCount--;         // one less user on this minode

    if (mip->shareCount > 0)   return;
    if (!mip->modified)        return;

    /*******************
    // last user, INODE modified: MUST write back to disk
    Use Mailman's algorithm to write minode.INODE back to disk)
    // NOTE: minode still in cacheList;
    *****************/
    char buf[BLKSIZE];
    int blk = (mip->ino - 1) / inodes_per_block + iblk;
    int offset = (mip->ino - 1) % inodes_per_block;
    get_block(dev, blk, buf);

    INODE *ip = (INODE *)buf + offset * ifactor;

    *ip = mip->INODE;                 // Copy INODE to inode in block
    put_block(mip->dev, blk, buf); // Write back to disk
    mip->shareCount = 0;
} 

int search(MINODE *mip, char *name)
{
  /******************
  search mip->INODE data blocks for name:
  if (found) return its inode number;
  else       return 0;
  ******************/
  int i;
  char *cp, temp[256], sbuf[BLKSIZE];
  DIR *dp;
  for (i=0; i<12; i++) { // search DIR direct blocks only
    if (mip->INODE.i_block[i] == 0)
      return 0;
    get_block(mip->dev, mip->INODE.i_block[i], sbuf);
    dp = (DIR *)sbuf;
    cp = sbuf;
    
    while (cp < sbuf + BLKSIZE) {
      strncpy(temp, dp->name, dp->name_len);
      temp[dp->name_len] = 0;
      printf("%8d%8d%8u %s\n",
      dp->inode, dp->rec_len, dp->name_len, temp);
      if (strcmp(name, temp)==0){
        printf("found %s : inumber = %d\n", name, dp->inode);
        return dp->inode;
      }
      cp += dp->rec_len;
      dp = (DIR *)cp;
    }
  }
  return 0;
}

MINODE *path2inode(char *pathname) 
{
  /*******************
  return minode pointer of pathname;
  return 0 if pathname invalid;
  
  This is same as YOUR main loop in LAB5 without printing block numbers
  *******************/

  MINODE *mip;

  if(pathname[0] == '/')
    mip = root;
  else
    mip = running->cwd;
  
  tokenize(pathname);

  for(int i = 0; i < n; i++) {
    if(!S_ISDIR(mip->INODE.i_mode)) {
      printf("One of the items in the path was not a DIR\n");
      return 0;
    }
    
    int ino = search(mip, name[i]);
    if(ino == 0) {
      printf("Can't find %s\n", name[i]);
      return 0;
    }

    iput(mip); // release current mip
    mip = iget(dev, ino); // change to mip of dev, ino
  }
  return mip;
}   

int findmyname(MINODE *pip, int myino, char myname[ ]) 
{
  /****************
  pip points to parent DIR minode: 
  search for myino;    // same as search(pip, name) but search by ino
  copy name string into myname[256]
  ******************/
    int i;
    char *cp, temp[256], sbuf[BLKSIZE];
    DIR *dp;
    MINODE *mip = pip;

    for (i = 0; i < 12; i++)
    { //Search DIR direct blcoks only
        if (mip->INODE.i_block[i] == 0)
            return -1;
        get_block(mip->dev, mip->INODE.i_block[i], sbuf);
        dp = (DIR *)sbuf;
        cp = sbuf;
        while (cp < sbuf + BLKSIZE)
        {

            if (dp->inode == myino)
            {
                strncpy(myname, dp->name, dp->name_len);
                myname[dp->name_len] = 0;
                return 0;
            }
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }
    return -1;
}
 
int findino(MINODE *mip, int *myino) 
{
  /*****************
  mip points at a DIR minode
  i_block[0] contains .  and  ..
  get myino of .
  return parent_ino of ..
  *******************/
  INODE *ip = (INODE *) &(mip->INODE);
  char sbuf[BLKSIZE], temp[256];
  DIR *dp;
  char *cp;

  get_block(mip->dev, ip->i_block[0], sbuf);

  dp = (DIR *)sbuf;
  cp = sbuf;

  *myino = dp->inode; // Get myino of .
  
  cp += dp->rec_len; // Move to ..
  dp = (DIR *)cp;

  return dp->inode;
}

// Keeps cache in order of lowest cacheCount to highest. Called when the cacheCount of a MINODE is updated.
void reorderCache(MINODE **cacheList) {
  MINODE *p, *temp = 0;

  while(p = dequeue(cacheList))
    enqueue(&temp, p);
  
  *cacheList = temp; 
}

void printCacheList() {
 MINODE *temp = cacheList;
  printf("Start of cacheList\n");
  while(temp) {
    printf("(%d, %d) cacheCount: %d shareCount: %d modified: %d\n", dev, temp->ino, temp->cacheCount, temp->shareCount, temp->modified);
    temp = temp->next;
  }
  printf("End of cacheList\n");
}


int getino(char *pathname)
{
  MINODE *mip;
  int i, ino;
  if (strcmp(pathname, "/")==0){
    return 2; // return root ino=2
  }
  if (pathname[0] == "/")
    mip = root; // if absolute pathname: start from root
  else
    mip = running->cwd; // if relative pathname: start from CWD
  
  mip->shareCount++; // in order to iput(mip) later
 
  tokenize(pathname); // assume: name[ ], nname are globals
  for (i=0; i<n; i++){ // search for each component string
    if (!S_ISDIR(mip->INODE.i_mode)){ // check DIR type
    printf("%s is not a directory\n", name[i]);
    iput(mip);
    return 0;
    }
    ino = search(mip, name[i]);
    if (!ino){
    printf("no such component name %s\n", name[i]);
    iput(mip);
    return 0;
    }
    iput(mip); // release current minode
    mip = iget(dev, ino); // switch to new minode
  }
  iput(mip);
  return ino;
}

MINODE *find_inode_in_cache(int dev, int ino) {
  MINODE *mip = cacheList;
  while (mip) {
      if (mip->dev == dev && mip->ino == ino) {
          mip->shareCount++; // Added to make shareCount match example
          return mip;
      }
      mip = mip->next;
  }
  return NULL;
}