// #include "util.c"
// #include "type.h"

int make_dir(char *pathname) {
    char *parent = dirname(pathname);
    char *child = basename(pathname);

    MINODE *pip = path2inode(parent);
    
    if(!pip) {
        printf("Error: parent is null\n");
        return -1;
    }

    if(S_ISDIR(pip->INODE.i_mode) && search(pip, child) == 0)
        mymkdir(pip, child);
    
    pip->INODE.i_links_count++;
    pip->INODE.i_atime = time(0L);
    pip->modified = 1;
    iput(pip);    
}

int mymkdir(MINODE *pip, char *name) {
    int ino = ialloc(dev);
    int bno = balloc(dev);

    // load inode into a minode[] (in order to
    // write contents to the INODE in memory
    MINODE *mip = iget(dev, ino); 
    
    INODE *ip = &mip->INODE;

    // Write contents to mip->INODE to make it a DIR_INODE
    ip->i_mode = 0x41ED;		// OR 040755: DIR type and permissions
    ip->i_uid  = running->uid;	// Owner uid 
    ip->i_gid  = running->gid;	// Group Id
    ip->i_size = BLKSIZE;		// Size in bytes 
    ip->i_links_count = 2;	                                // Links count=2 because of . and ..
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);     // set to current time
    ip->i_blocks = 2;                	                    // LINUX: Blocks count in 512-byte chunks 
    ip->i_block[0] = bno;                                   // new DIR has one data block

    for(int i = 1; i <= 14; i++) 
        ip->i_block[i] = 0;
    
    mip->modified = 1;
    iput(mip);

    // Create data block for new DIR containing . and .. entries
    char buf[BLKSIZE];
    bzero(buf, BLKSIZE); // optional: clear buf[ ] to 0
    DIR *dp = (DIR *)buf;
    // make . entry
    dp->inode = ino;
    dp->rec_len = 12;
    dp->name_len = 1;
    dp->name[0] = '.';
    // make .. entry: pino=parent DIR ino, blk=allocated block
    dp = (char *)dp + 12; // Move to next entry
    dp->inode = pip->ino;
    dp->rec_len = BLKSIZE - 12; // rec len spans block
    dp->name_len = 2;
    dp->name[0] = dp->name[1] = '.';
    put_block(dev, bno, buf); // write to bno on disk

    // Enter name ENTRY
    enter_child(pip, ino, name);
}

int enter_child(MINODE *pip, int myino, char *myname) {
    int NEED_LEN = 4*((8 + strlen(myname) + 3)/4); // a multiple of 4

    INODE *ip = &pip->INODE;
    // Loop through direct blocks. Implements first fit algorithm, where we try and
    // find the FIRST slot big enough for the new entry.
    for(int i = 0; i < 12; i++) {
        if(ip->i_block[i] == 0)
            break;
        char buf[BLKSIZE];
        get_block(dev, ip->i_block[i], buf); // Get parent's data block in a buf
        DIR *dp = (DIR *)buf;
        char *cp = buf;

        // Step through each DIR entry in the data block to see if there's enough space for the new dir
        while (cp < buf + BLKSIZE) {
            int IDEAL_LEN = 4*((8 + dp->name_len + 3)/4);
            int REMAIN = dp->rec_len - IDEAL_LEN;

            if(REMAIN >= NEED_LEN) { // Found space for the new dir
                printf("REMAIN: %d NEED: %d\n", REMAIN, NEED_LEN);
                dp->rec_len = IDEAL_LEN; // trim dp's rec_len to its IDEAL_LEN
                
                // Enter new entry
                cp += dp->rec_len;
                dp = (DIR *)cp;

                // Set new information in directory
                dp->inode = myino;
                dp->rec_len = REMAIN;
                dp->name_len = strlen(myname);
                strcpy(dp->name, myname);
                put_block(dev, ip->i_block[i], buf); // Write new dir back to disk
                return 0; // Successfully made dir
            }

            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }

    // No space in existing data blocks
    // Allocate new data block
    int bno = balloc(dev);
    pip->INODE.i_size += BLKSIZE; // Inc parents size by blksize
    
    char buf[BLKSIZE];
    get_block(dev, bno, buf);

    DIR *dp = (DIR *)buf;
    char *cp = buf;

    dp->inode = myino;
    dp->rec_len = BLKSIZE;
    dp->name_len = strlen(myname);
    strcpy(dp->name, myname);
    put_block(dev, bno, buf); // Write data block back to disk
}

int creat_file(char *pathname) {
    char *parent = dirname(pathname);
    char *child = basename(pathname);

    MINODE *pip = path2inode(parent);

    if(!pip) {
        printf("Error: parent is null\n");
        return -1;
    }

    // Make sure parent is a DIR and file does not exist
    if(S_ISDIR(pip->INODE.i_mode) && search(pip, child) == 0)
        my_creat(pip, child);

    pip->INODE.i_atime = time(0L);
    pip->modified = 1;
    iput(pip); 
}

int my_creat(MINODE *pip, char *name)
{
    int ino = ialloc(dev);
    int bno = balloc(dev);

    // load inode into a minode[] (in order to
    // write contents to the INODE in memory
    MINODE *mip = iget(dev, ino); 
    
    INODE *ip = &mip->INODE;

    // Write contents to mip->INODE to make it a REG_INODE
    ip->i_mode = 0x81A4;		// REGULAR file
    ip->i_uid  = running->uid;	// Owner uid 
    ip->i_gid  = running->gid;	// Group Id
    ip->i_size = 0;		// No data block 
    ip->i_links_count = 1;	                                // Links count=2 because of . and ..
    ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);     // set to current time
    ip->i_blocks = 2;                	                    // LINUX: Blocks count in 512-byte chunks 
    ip->i_block[0] = bno;                                   // new DIR has one data block

    for(int i = 1; i <= 14; i++) 
        ip->i_block[i] = 0;
    
    mip->modified = 1;
    iput(mip);

    // Enter name ENTRY
    enter_child(pip, ino, name);
}