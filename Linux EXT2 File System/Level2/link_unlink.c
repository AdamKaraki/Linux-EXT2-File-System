// #include "util.c"
// #include "type.h"

int link(char *pathname, char *parameter) {
    MINODE *mip = path2inode(pathname);

    if(S_ISDIR(mip->INODE.i_mode)) {
        printf("ERROR: %s is a DIR\n", pathname);
        return -1;
    }

    char *parent = dirname(parameter);
    char *child = basename(parameter);

    int pino = getino(parent);
    int cino = getino(child);
    MINODE *pip = iget(dev, pino);
    if(!(pino && S_ISDIR(pip->INODE.i_mode) && !cino)) {
        printf("ERROR: parent does not exist or is not a DIR or child exists\n");
        return -1;
    }
    enter_child(pip, mip->ino, child);

    mip->INODE.i_links_count++;
    iput(mip);
    iput(pip);
}

int unlink(char *pathname) {
    MINODE *mip = path2inode(pathname);

    if(S_ISDIR(mip->INODE.i_mode)) {
        printf("ERROR: Cannot unlink DIR\n");
        return -1;
    }

    mip->INODE.i_links_count--;

    if(mip->INODE.i_links_count == 0) {
        printf("mip->INODE.i_links_count == 0\n");
        truncate(mip);
    } 
    
    char *parent = dirname(pathname);
    char *child = basename(pathname);
    int pino = getino(parent);
    MINODE *pip = iget(dev, pino); // Parent directory MINODE ptr
    rm_child(pip, child);
    pip->modified = 1;
    iput(pip);
    iput(mip);
}

// Deallocate data blocks and inode of file
int truncate(MINODE *mip) {
    for (int i = 0; i < 12; i++) { 
         if (mip->INODE.i_block[i]==0)
            continue;
        bdalloc(mip->dev, mip->INODE.i_block[i]);
    }

     //       Deallocate indirect block numbers 
   if (mip->INODE.i_block[12] != 0) {
      char buf2[BLKSIZE];
      get_block(dev, mip->INODE.i_block[12], buf2);
      int *indirect_block = (int *) buf2;
      for (int i = 0; i < BLKSIZE/sizeof(int); i++) {
         if (indirect_block[i] != 0) 
            bdalloc(mip->dev, indirect_block[i]);
      }
   }

   //       Deallocate double indirect block numbers, if any
   if (mip->INODE.i_block[13] != 0) {
      int *double_indirect_block = (int *) malloc(BLKSIZE);
      get_block(dev, mip->INODE.i_block[13], (char *)double_indirect_block);
      for (int i = 0; i < BLKSIZE/sizeof(int); i++) {
         if (double_indirect_block[i] != 0) { // If indirect block not null
            int *indirect_block = (int *) malloc(BLKSIZE);
            get_block(dev, double_indirect_block[i], (char *)indirect_block);
            for (int j = 0; j < BLKSIZE/sizeof(int); j++) {
                if (indirect_block[j] != 0) 
                    bdalloc(mip->dev, indirect_block[i]);
            }
            free(indirect_block);
         }
      }
      free(double_indirect_block);
    }

    // Update INODE's time fields
    mip->INODE.i_atime = mip->INODE.i_ctime = mip->INODE.i_dtime = mip->INODE.i_mtime = time(0L);

    // Set INODE's size to 0 and mark minode MODIFIED
    mip->INODE.i_size = 0;
    mip->modified = 1;
    idalloc(mip->dev, mip->ino);
}