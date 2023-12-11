// cd_ls_pwd.c file
//#include "type.h"

int chdir(char *pathname) {
  int ino = getino(pathname); // get inode number of the path
  if (ino == 0) {
  printf("ERROR: %s does not exist.\n", pathname);
  return -1;
  }
  MINODE *mip = find_inode_in_cache(dev, ino); // check cacheList for existing MINODE
  if (!mip) { // if MINODE doesn't exist in cacheList, create one
    mip = iget(dev, ino);
  }
  if (!S_ISDIR(mip->INODE.i_mode)) {
    printf("ERROR: %s is not a directory.\n", pathname);
    iput(mip); // release the MINODE
    return -1;
  }
  iput(running->cwd); // release old cwd
  running->cwd = mip; // set new cwd
  return 0;
}


int ls_file(MINODE *mip, char *name)
{
  char linkname[256];
  // print -> linkname if symbolic file
  if ((mip->INODE.i_mode & 0xF000)== 0xA000){
      // use readlink() to read linkname
      int stdout_copy = dup(1);
      close(1);
      strcpy(linkname, read_link(name));
      dup2(stdout_copy, 1);
  }

  char *t1 = "xwrxwrxwr ";
  char *t2 = "          ";
  int i;
  // use mip->INODE to ls_file
  if ((mip->INODE.i_mode & 0xF000) == 0x8000) // if (S ISREG())
        printf("%c", 'r');
  if ((mip->INODE.i_mode & 0xF000) == 0x4000) // if (S ISDIR())
      printf("%c",'d');
  if ((mip->INODE.i_mode & 0xF000) == 0xA000) // if (S ISLNK())
      printf("%c",'l');

  for (i=8; i >= 0; i--){
      if (mip->INODE.i_mode & (1 << i)) // print r|w|x
          printf("%c", t1[i]);
      else
          printf("%c", t2[i]); // or print
  }
  
  printf("%4d ",mip->INODE.i_links_count); // link count
  printf("%4d ",mip->INODE.i_gid); // gid
  printf("%4d ",mip->INODE.i_uid); // uid
  printf("%8d ",mip->INODE.i_size); // file size
  
  // print time
  char ftime[64];
  time_t temp = mip->INODE.i_ctime;
  strcpy(ftime, ctime(&temp)); // print time in calendar form
  ftime[strlen(ftime) - 1] = 0; // kill \n at end
  printf("%s ",ftime);
  
  // print name
  printf("%s", basename(name)); // print file basename
  
  if ((mip->INODE.i_mode & 0xF000)== 0xA000)
      printf(" -> %s", linkname); // print linked name
  
  printf("\n");
}
  
int ls_dir(MINODE *pip)
{
  char sbuf[BLKSIZE], name[256];
  DIR  *dp;
  char *cp;

  get_block(dev, pip->INODE.i_block[0], sbuf);
  dp = (DIR *)sbuf;
  cp = sbuf;
   
  while (cp < sbuf + BLKSIZE){
    strncpy(name, dp->name, dp->name_len);
    name[dp->name_len] = 0;
    
    MINODE *mip = iget(dev, dp->inode);
    ls_file(mip, name);
    iput(mip);

    cp += dp->rec_len;
    dp = cp;
  }

}

int ls(char *pathname)
{
  MINODE *mip;
  if(pathname[0]) {
    printf("pathname[0]: %c\n", pathname[0]);
    mip = path2inode(pathname);
  }
  else
    mip = iget(dev, running->cwd->ino);
  
  if(S_ISDIR(mip->INODE.i_mode))
    ls_dir(mip);
  else
    ls_file(mip, basename(pathname));

  iput(mip);
}

void rpwd(MINODE *wd) {
    if (wd->ino == root->ino) {
        return;
    }
    char buf[BLKSIZE], my_name[256];
    get_block(dev, wd->INODE.i_block[0], buf);
    char *cp = buf;
    DIR* dp = (DIR *)buf;
    int my_ino = dp->inode;

    cp += dp->rec_len;
    dp = (DIR *)cp;
    int parent_ino = dp->inode;

    MINODE *pip = iget(dev, parent_ino);

    findmyname(pip, my_ino, my_name);

    rpwd(pip);
    iput(pip);
    printf("/%s", my_name);
    
    return;
}

void pwd(MINODE *wd) {
    if (wd->ino == root->ino) { //if root
        printf("/");
    } else { //not root
        rpwd(wd);
    }
    printf("\n");
}