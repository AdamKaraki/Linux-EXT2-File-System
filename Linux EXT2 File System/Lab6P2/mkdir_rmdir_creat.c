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

int rmdir(char *pathname)
{
    int my_ino, p_ino, i;
    MINODE *mip, *pip;
    char p_buf[256], c_buf[256], temp[256],buf[BLKSIZE];
    DIR *dp;
    char *cp;

    // my_ino = getino(pathname);     // get minode of pathname
    // if (my_ino == 0) {             //return error
    //     printf("ERROR: %s does not exist\n", pathname);
    //     return -1;
    // }

    // mip = iget(dev, my_ino);             // get the inode and check if it is a directory

    mip = path2inode(pathname);
    if (!S_ISDIR(mip->INODE.i_mode)) { //return error
        printf("ERROR: %s is not a directory\n", pathname);
        iput(mip);
        return -1;
    }

    if (mip->shareCount > 1) { //make sure directory is not busy
        printf("ERROR: %s is currently in use\n", pathname);
        iput(mip);
        return -1;
    }

    if (mip->INODE.i_links_count > 2) {// we also want to make sure if it's empty
        printf("ERROR: %s is not empty\n", pathname);
        iput(mip);
        return -1;
    }

    for (i = 0; i < 12; i++) {     //go through data blocks to check if directory is empty
        if (mip->INODE.i_block[i] == 0)
            continue;
        get_block(dev, mip->INODE.i_block[i], buf);
        dp = (DIR *)buf;
        cp = buf;

        while (cp < buf + BLKSIZE) {
            strncpy(temp, dp->name, dp->name_len);
            temp[dp->name_len] = '\0';
            if (strcmp(temp, ".") && strcmp(temp, "..")) {
                printf("ERROR: %s is not empty\n", pathname);
                iput(mip);
                return -1;
            }
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
    }

    strcpy(temp, pathname);
    strcpy(p_buf, dirname(temp));// get parent directory
    strcpy(temp, pathname);
    strcpy(c_buf, basename(temp)); 

    p_ino = getino(p_buf); //get parent minode: 
    pip = iget(dev, p_ino);

    rm_child(pip, c_buf);// remove child's entry from parent directory

    pip->INODE.i_links_count--; //decrement pip's link_count by 1
    pip->INODE.i_atime = pip->INODE.i_mtime = time(0L); //touch pip's atime, mtime fields;
    pip->modified = 1; //mark pip MODIFIED
    iput(pip); // this will write parent INODE out eventually

    for (i=0; i<12; i++){ // deallocate data blocks and inode of directory
         if (mip->INODE.i_block[i]==0)
             continue;
         bdalloc(mip->dev, mip->INODE.i_block[i]);
     }
    idalloc(mip->dev, mip->ino);
    iput(mip); //(which clears mip->sharefCount to 0, still in cache);;

    return 0;
}



int rm_child(MINODE *parent, char *name)
{
    DIR *current_dir, *prev_dir, *last_dir;
    char *name_ptr, *last_name_ptr, buffer[BLKSIZE], temp_name[256], *start_ptr, *end_ptr;
    INODE *parent_inode_ptr = &parent->INODE;

    for (int i = 0; i < 12; i++) {                      // loop through 12 direct blocks of the parent directory
        if (parent_inode_ptr->i_block[i] != 0)          // if current block not empty
        {
            get_block(parent->dev, parent_inode_ptr->i_block[i], buffer);    // read the block from disk
            current_dir = (DIR *)buffer;
            name_ptr = buffer;

            while (name_ptr < buffer + BLKSIZE) {           // loop through all directory entries in the current block
                strncpy(temp_name, current_dir->name, current_dir->name_len);     // copy the name from the current directory entry into a temporary variable
                temp_name[current_dir->name_len] = 0;

                
                if (!strcmp(temp_name, name)){          // if the name matches the one we want to delete
                    if (name_ptr == buffer && name_ptr + current_dir->rec_len == buffer + BLKSIZE) {  // if this is the first/only directory entry in the block
                        bdalloc(parent->dev, parent_inode_ptr->i_block[i]); // deallocate the block and update the parent inode
                        parent_inode_ptr->i_size -= BLKSIZE;

                       
                        while (parent_inode_ptr->i_block[i + 1] != 0 && i + 1 < 12){    // fill the hole in the i_blocks array caused by the block deallocation
                            i++;
                            get_block(parent->dev, parent_inode_ptr->i_block[i], buffer);
                            put_block(parent->dev, parent_inode_ptr->i_block[i - 1], buffer);
                        }}
                    
                    else if (name_ptr + current_dir->rec_len == buffer + BLKSIZE)  {   // If this is the last directory entry in the block
                        prev_dir->rec_len += current_dir->rec_len;   // merge the size of the current directory entry with the previous one
                        put_block(parent->dev, parent_inode_ptr->i_block[i], buffer);
                    }
                    
                    else{       // If this is a directory entry between others
                        prev_dir->rec_len += current_dir->rec_len + (name_ptr - (char *)current_dir);  // Merge the size of the current directory entry with the one before it
                        start_ptr = (char *)current_dir + current_dir->rec_len;
                        end_ptr = buffer + BLKSIZE;
                        memcpy(start_ptr, name_ptr + current_dir->rec_len, end_ptr - start_ptr);
                        put_block(parent->dev, parent_inode_ptr->i_block[i], buffer);
                    }
                    parent->modified = 1;       // Update the parent inode and mark it as dirty
                    iput(parent);

                    return 0;                   // Return success
                }

                prev_dir = current_dir;         // Save the previous directory entry and move the pointer to the next one
                name_ptr += current_dir->rec_len;
                current_dir = (DIR *)name_ptr;
            }
        }
    }
    printf("ERROR: child not found\n");
    return -1;
}