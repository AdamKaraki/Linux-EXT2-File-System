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