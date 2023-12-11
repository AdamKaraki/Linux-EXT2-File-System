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

int cat(char *pathname) {
    char mybuf[BLKSIZE], dummy = 0;  // a null char at end of mybuf[ ]
    int n;

    int fd = open_file(pathname, 0); // open filename for READ
    int bytesRead = 0;
    
    while(n = myread(fd, mybuf, BLKSIZE)) {
        bytesRead += n;
        mybuf[n] = 0;             // as a null terminated string
        printf("%s", mybuf);   // <=== THIS works but not good
        //spit out chars from mybuf[ ] but handle \n properly;
    }
    printf("cat %d bytes\n", bytesRead);
    close_file(fd);
}

int my_cp(char *src, char *dest){
    char buf[BLKSIZE] = {0}; // buffer for reading and writing data
    int n = 0; // number of bytes read from source file
    int fd = open_file(src, READ); // open source file for reading
    int gd = open_file(dest, READ_WRITE); // open or create destination file for writing
    
    printf("fd: %d\n", fd);
    printf("gd: %d\n", gd);
    
    // check if either file failed to open
    if (fd == -1 || gd == -1) {
        if (fd != -1) close_file(fd);
        if (gd != -1) close_file(gd);
        return -1;
    }
    
    // read and write loop
    while ((n = myread(fd, buf, BLKSIZE))) {
        mywrite(gd, buf, n);
        memset(buf, '\0', BLKSIZE); // clear the buffer
    }
    
    // close files
    close_file(fd);
    close_file(gd);
    
    return 0;
}