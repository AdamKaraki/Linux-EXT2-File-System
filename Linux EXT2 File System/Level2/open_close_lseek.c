// #include "type.h"
// #include "util.c"

//#include "link_unlink.c"

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

int open_file(char *pathname, int mode) {
    MINODE *mip = path2inode(pathname);
    if (!mip){ // pathname does not exist:
        if (mode==0)    // READ: file must exist
	        return -1;
		    
        // mode=R|RW|APPEND: creat file first
        creat_file(pathname);

	    mip = path2inode(pathname);
	    // print mip's [dev, ino]
        printf("[%d %d]\n", mip->dev, mip->ino);
    }

    if(!S_ISREG(mip->INODE.i_mode)) {
        printf("Error: Not regular file\n");
        iput(mip);
        return -1;
    }

    // Check whether the file is ALREADY opened with INCOMPATIBLE mode:
    // If it's already opened for W, RW, APPEND : reject.
    // Only multiple READs of the SAME file are OK
    for(int i = 0; i < NOFT; i++) {
        OFT *oftp = &oft[i];
        if(oftp->shareCount != 0 && oftp->inodeptr->ino == mip->ino && oftp->mode != 0) {
            printf("ERROR: Already opened with incompatible mode\n");
            return -1;
        }       
    }
    
    OFT *oftp;
    // allocate a FREE OpenFileTable (OFT) and fill in values:
    for(int i = 0; i < NOFT; i++) {
        oftp = &oft[i];
        if(oftp->shareCount == 0) { // OFT is free
            oftp->mode = mode;
            oftp->shareCount = 1;
            oftp->inodeptr = mip;

            switch(mode) {
                case 0 : oftp->offset = 0;
                         break;
                case 1 : truncate(mip);
                         oftp->offset = 0;
                         break;
                case 2 : oftp->offset = 0;
                         break;
                case 3 : oftp->offset = mip->INODE.i_size; // APPEND
                         break;
                default: printf("invalid mode\n");
                         return -1;
            }            
            break;
        }
    }

    // Find the SMALLEST index i in running PROC's fd[ ] such that fd[i] is NULL
    // Let running->fd[i] point at the OFT entry
    int i;
    for(i = 0; i < NFD; i++) {
        if(running->fd[i] == 0) {
            running->fd[i] = oftp;
            break;
        }
    }
    
    // update INODE's time field
    mip->INODE.i_atime = time(0L);
    if(mode != 0) // W|RW|APPEND
        mip->INODE.i_mtime = time(0L);
    
    mip->modified = 1;
    return i; // Return i as file descriptor
}

int close_file(int fd)
{
    // Verify fd is within range.
    if(fd < 0 || fd >= NFD)
        return -1;

    // Verify running->fd[fd] is pointing at a OFT entry
    if(running->fd[fd] == NULL)
        return -1;
		    
     OFT *oftp = running->fd[fd];
     running->fd[fd] = 0;

     oftp->shareCount--;
     if (oftp->shareCount > 0) // Other PROCs still using oftp
        return 0;

     // last user of this OFT entry ==> dispose of its minode
     MINODE *mip = oftp->inodeptr;
     iput(mip);

     return 0; 
}

int my_lseek(int fd, int position)
{
    // From fd, find the OFT entry.
    OFT *oftp = running->fd[fd]; 

    // Change OFT entry's offset to position but make sure NOT to over run either end
    // of the file.
    if(position < 0 || position > (oftp->inodeptr->INODE.i_size)) {
        printf("ERROR: position out of bounds\n");
        return -1;
    }

    int originalPosition = oftp->offset;
    oftp->offset = position;

    return originalPosition;
}

// gets filename of inode with matching ino
int getFilename(MINODE *mip, int ino, char temp[])
{
    // show contents of mip DIR: same as in LAB5
    char sbuf[BLKSIZE];
    DIR *dp;
    char *cp;

    // ASSUME only one data block i_block[0]
    // YOU SHOULD print i_block[0] number here
    INODE *ip = &(mip->INODE);
    get_block(dev, ip->i_block[0], sbuf);  

    dp = (DIR *)sbuf;
    cp = sbuf;
  
    while(cp < sbuf + BLKSIZE){
        if(dp->inode == ino) {
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = 0;
            return 0;
        }
        
        cp += dp->rec_len;
        dp = (DIR *)cp;
    }

    return -1;
}

//   This function displays the currently opened files as follows:

//         fd     mode    offset    INODE
//        ----    ----    ------   --------
//          0     READ    1234   [dev, ino]  
//          1     WRITE      0   [dev, ino]
//       --------------------------------------
int pfd()
{
    printf("fd\t mode\t offset\t INODE\t\tfilename\t\n");
    printf("----\t ----\t ------\t --------\t---------\t\n");
    

    int i = 0;
    while(running->fd[i]) {
        OFT *ofp = running->fd[i];
        char *mode;
        switch(ofp->mode) {
            case 0:
                mode = "READ";
                break;
            case 1:
                mode = "WRITE";
                break;
            case 2:
                mode = "READWRITE";
                break;
            case 3:
                mode = "APPEND";
                break;
        }
        char name[256];
        getFilename(running->cwd, ofp->inodeptr->ino, name);
        printf("  %d\t %s\t %d\t [%d %d]\t\t%s\n", i, mode, ofp->offset, ofp->inodeptr->dev, ofp->inodeptr->ino, name);
        i++;
    }

    printf("-------------------------------------------------\n");
}

