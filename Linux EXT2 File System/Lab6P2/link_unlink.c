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
    for (int i = 0; i < 12; i++){ 
         if (mip->INODE.i_block[i]==0)
            continue;
        bdalloc(mip->dev, mip->INODE.i_block[i]);
     }
    idalloc(mip->dev, mip->ino);
}