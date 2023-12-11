// create a symbolic link file
// symlink old_name new_name
int symlink(char* old_name, char* new_name) {
    // Step 1: Verify old_name exists (either a DIR or a REG file)
    int inode_num_old,inode_num_new;
    if (old_name[0] == '/') {
        // use root's device if path is absolute
        dev = root->dev;
    } else {
        // use running process's current working directory's device if path is relative
        dev = running->cwd->dev;
    }
    inode_num_old = getino(old_name);
    if (inode_num_old == 0) {
        printf("%s does not exist\n", old_name);
        return -1;
    }

    // Step 2: Create a new file
    if (new_name[0] == '/') {
        // use root's device if path is absolute
        dev = root->dev;
    } else {
        // use running process's current working directory's device if path is relative
        dev = running->cwd->dev;
    }
    creat_file(new_name);
    inode_num_new = getino(new_name);

    // Step 3: Set the type of new file to LNK (0120000)=(b1010.....)=0xA...
    MINODE * mip = iget(dev, inode_num_new);
    mip->INODE.i_mode = 0xA1FF;// set file type to symlink
    mip->modified = 1;

    // Step 4: Write the string old_name into the i_block[ ], which has room for 60 chars.
    // Set new file size = number of chars in old_name
    strncpy(mip->INODE.i_block, old_name, 60);
    mip->INODE.i_size = strlen(old_name) + 1; 

    // Step 5: Write the INODE of new back to disk
    mip->modified = 1;
    iput(mip);

    
    return 0;
}

char *read_link(char *pathname) {
    MINODE *mip;
    int ino;

    // get inode of pathname into minode[]
    ino = getino(pathname);
    if (ino < 1) {
        printf("%s does not exist.\n", pathname);
        return NULL;
    }

    // get the inode's minode structure
    mip = iget(dev, ino);

    // check if inode is a symbolic link
    if (!S_ISLNK(mip->INODE.i_mode)) {
        printf("%s is not a symbolic link.\n", pathname);
        iput(mip);
        return NULL;
    }

    // return contents of symbolic link
    char *link_contents = (char *)malloc(mip->INODE.i_size + 1);
    strncpy(link_contents, mip->INODE.i_block, mip->INODE.i_size);
    link_contents[mip->INODE.i_size] = '\0';

    // release minode
    iput(mip);

    return link_contents;
}