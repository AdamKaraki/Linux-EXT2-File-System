// #include "type.h"
// #include "util.c"

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

int read_file(int fd, int nbytes)
{
	//     verify fd is indeed opened for RD or RW;
    if(running->fd[fd]->mode != 0 && running->fd[fd]->mode != 2) {
        printf("ERROR: file not opened for R or RW\n");
        return -1;
    }
    char buf[BLKSIZE];
    return myread(fd, buf, nbytes);
}

int myread(int fd, char buf[ ], int nbytes) {
    OFT *oftp = running->fd[fd];
    int count = 0;
    int avil = oftp->inodeptr->INODE.i_size - oftp->offset; // number of bytes still available in file.
    char *cq = buf;                // cq points at buf[ ]

    while (nbytes && avil > 0) {
        // Compute LOGICAL BLOCK number lbk and startByte in that block from offset;

        int lbk       = oftp->offset / BLKSIZE;
        int startByte = oftp->offset % BLKSIZE;
        
        int blk; // block to read from

        MINODE *mip = oftp->inodeptr;
        int blkNumsPerBLK = BLKSIZE/sizeof(int);
        if (lbk < 12){                     // lbk is a direct block
            blk = mip->INODE.i_block[lbk]; // map LOGICAL lbk to PHYSICAL blk
        }
        else if (lbk >= 12 && lbk < blkNumsPerBLK + 12) {  
            //  indirect blocks
            int buf[blkNumsPerBLK];
            get_block(dev, mip->INODE.i_block[12], buf);
            lbk -= 12;
            blk = buf[lbk];
        }
        else { 
            //  double indirect blocks
            int buf[blkNumsPerBLK]; // Get doubly indirect block
            get_block(dev, mip->INODE.i_block[13], buf);
            
            lbk -= (blkNumsPerBLK + 12); // Zero out lbk
            int indirectBlkInd = lbk/blkNumsPerBLK; // Find out which indirect block lbk is in
            int indirectBlkNum = buf[indirectBlkInd];
            
            get_block(dev, indirectBlkNum, buf); // Get indirect block
            int offset = lbk % blkNumsPerBLK; // Find out which block in the indirect block lbk is
            blk = buf[offset];
        } 
        
        
        /* get the data block into readbuf[BLKSIZE] */
        char readbuf[BLKSIZE];
        get_block(mip->dev, blk, readbuf);
        

        /* copy from startByte to buf[ ], at most remain bytes in this block */
        char *cp = readbuf + startByte;   
        int remain = BLKSIZE - startByte;   // number of bytes remain in readbuf[]

        // Read bytes one at a time into buf
        // while (remain > 0){
        //         *cq++ = *cp++;             // copy byte from readbuf[] into buf[]
        //         oftp->offset++;           // advance offset 
        //         count++;                  // inc count as number of bytes read
        //         avil--; nbytes--;  remain--;
        //         if (nbytes <= 0 || avil <= 0) 
        //             break;
        // }

        // Read a max chunk of data at a time
        int maxBytes = min(remain, nbytes, avil); // max bytes we can read
        memcpy(cq, cp, maxBytes); // dest = buf, src = readbuf
        cq += (maxBytes - 1); cp += (maxBytes - 1);
        oftp->offset += maxBytes;
        count += maxBytes;
        avil -= maxBytes; nbytes -= maxBytes; remain -= maxBytes;
        // if one data block is not enough, loop back to OUTER while for more
    }
    // printf("----------------------------------------------");
    // printf("myread: read %d char from file descriptor %d\n", count, fd);
    return count;   // count is the actual number of bytes read
}

int min(int a, int b, int c) {
    //printf("%d %d\n", a, b);
    int firstSmallest = (a < b) ? a : b;
    return (firstSmallest < c) ? firstSmallest : c;
}