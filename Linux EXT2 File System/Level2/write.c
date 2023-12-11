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

int write_file() {
    int fd;
    char *buf = 0;

    //Get input
    printf("Enter a file descriptor: ");
    scanf("%d", &fd);
    printf("Enter what you want to write: ");
    scanf("%ms", &buf);

    //Confirm input
    printf("\nbuf:%s \n", buf);
    printf("\nsize of buf:%ld \n", strlen(buf));
    
    // verify fd is open for W or RW
    if (running->fd[fd]->mode != WRITE && running->fd[fd]->mode != READ_WRITE && running->fd[fd]->mode != APPEND) {
        printf("error: fd in write_file() is not open for write\n"); return -1;
    }

    int size = strlen(buf);// Get the length of the input buffer buf
    char tbuf[BLKSIZE];
    strncpy(tbuf,buf,size); // The strncpy() function is used to copy size number of characters from buf to tbuf
    free(buf); //Free the original buffer buf
    return mywrite(fd, tbuf, size);
}

int mywrite(int fd, char *buf, int n_bytes)
{
    // Get the OFT entry and corresponding MINODE and INODE structures
    OFT *oftp = running->fd[fd];
    MINODE *mip = oftp->inodeptr;
    INODE *ip = &mip->INODE;

    // Declare variables
    int lbk, start_byte, blk, extra, double_blk;
    char ibuf[BLKSIZE], double_ibuf[BLKSIZE];
    char *cq = buf;

    // Loop through the buffer and write to the file
    while (n_bytes > 0)
    {        
        lbk = oftp->offset / BLKSIZE;           // Calculate the logical block number
        start_byte = oftp->offset % BLKSIZE;    // and starting byte offset

        // Get the physical block number
        if (lbk < 12) // direct blocks
        {
            if(ip->i_block[lbk] == 0)
            {
                ip->i_block[lbk] = balloc(mip->dev);    // Allocate a new data block if one doesn't exist
            }
            blk = ip->i_block[lbk];
        }

        else if (lbk >= 12 && lbk < 256 + 12 )
        {                   // Indirect blocks
            char tbuf[BLKSIZE] = { 0 };

            if(ip->i_block[12] == 0)
            {                   // Allocate a new indirect block if one doesn't exist
                int block_12 = ip->i_block[12] = balloc(mip->dev);

                if (block_12 == 0) 
                    return 0;

                // Clear out the block
                get_block(mip->dev, ip->i_block[12], ibuf);
                int *ptr = (int *)ibuf;
                for(int i = 0; i < (BLKSIZE / sizeof(int)); i++)
                {
                    ptr[i] = 0; 
                }

                put_block(mip->dev, ip->i_block[12], ibuf);
                mip->INODE.i_blocks++;
            }            // Get the indirect block
            int indir_buf[BLKSIZE / sizeof(int)] = { 0 };
            get_block(mip->dev, ip->i_block[12], (char *)indir_buf);
            // Get the physical block
            blk = indir_buf[lbk - 12];

            if(blk == 0){
                // Allocate a new data block if one doesn't exist
                blk = indir_buf[lbk - 12] = balloc(mip->dev);
                ip->i_blocks++;
                put_block(mip->dev, ip->i_block[12], (char *)indir_buf);

            }
        }

        else
    {
        // Calculate the index of double indirect block
        lbk = lbk - (BLKSIZE/sizeof(int)) - 12;
        // If the double indirect block doesn't exist, allocate it
        if(mip->INODE.i_block[13] == 0)
        {
            // Allocate a new block for the double indirect block
            int block_13 = ip->i_block[13] = balloc(mip->dev);
            if (block_13 == 0) 
                return 0;
            // Initialize the double indirect block to 0s
            get_block(mip->dev, ip->i_block[13], ibuf);
            int *ptr = (int *)ibuf;
            for(int i = 0; i < (BLKSIZE / sizeof(int)); i++){
                ptr[i] = 0;
            }
            put_block(mip->dev, ip->i_block[13], ibuf);
            // Update the number of blocks used by the inode
            ip->i_blocks++;
        }
        // Read the double indirect block
        int doublebuf[BLKSIZE/sizeof(int)] = {0};
        get_block(mip->dev, ip->i_block[13], (char *)doublebuf);
        // Calculate the index of the block pointed to by the double indirect block
        double_blk = doublebuf[lbk/(BLKSIZE / sizeof(int))];
        // If the block pointed to by the double indirect block doesn't exist, allocate it
        if(double_blk == 0){
            double_blk = doublebuf[lbk/(BLKSIZE / sizeof(int))] = balloc(mip->dev);
            if (double_blk == 0) 
                return 0;
            // Initialize the block to 0s
            get_block(mip->dev, double_blk, double_ibuf);
            int *ptr = (int *)double_ibuf;
            for(int i = 0; i < (BLKSIZE / sizeof(int)); i++){
                ptr[i] = 0;
            }
            put_block(mip->dev, double_blk, double_ibuf);
            // Update the number of blocks used by the inode
            ip->i_blocks++;
            // Write the updated double indirect block back to disk
            put_block(mip->dev, mip->INODE.i_block[13], (char *)doublebuf);
        }
        // Read the block pointed to by the double indirect block
        memset(doublebuf, 0, BLKSIZE / sizeof(int));
        get_block(mip->dev, double_blk, (char *)doublebuf);
        // If the block doesn't exist, allocate it
        if (doublebuf[lbk % (BLKSIZE / sizeof(int))] == 0) {
        blk = doublebuf[lbk % (BLKSIZE / sizeof(int))] = balloc(mip->dev);
        if (blk == 0)
            return 0;
        // Update the number of blocks used by the inode
        ip->i_blocks++;
        // Write the updated double indirect block back to disk
        put_block(mip->dev, double_blk, (char *)doublebuf);
            }
        }

        // Allocate a buffer to hold the data to be written
        char writebuf[BLKSIZE] = { 0 };
        // Read the block pointed to by blk into the buffer
        get_block(mip->dev, blk, writebuf);
        // Calculate the starting byte offset within the block
        char *cp = writebuf + start_byte;
        extra = BLKSIZE - start_byte;

        // Calculate the remaining space in the block that needs to be written to.
        if(extra <= n_bytes)
        {
            memcpy(cp, cq, extra);
            cq += extra;
            cp += extra;
            oftp->offset += extra;
            n_bytes -= extra;
        } else {
            memcpy(cp, cq, n_bytes); 
            cq += n_bytes;
            cp += n_bytes;
            oftp->offset += n_bytes;
            n_bytes -= n_bytes;
        }
        if(oftp->offset > mip->INODE.i_size)
            mip->INODE.i_size = oftp->offset;

        put_block(mip->dev, blk, writebuf);
    }

        // mark the inode as dirty
        mip->modified = 1;
        return n_bytes;
}