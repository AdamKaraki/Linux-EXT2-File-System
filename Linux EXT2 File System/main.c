/************** lab5base.c file ******************/
#include "type.h"

int fd;             // opened vdisk for READ
int inodes_block;   // inodes start block number
int bMapBlockNum;
int iMapBlockNum;

char gpath[128];    // token strings
char *name[32];
int n;

char *disk = "diskimage";
int dev = 0; // set this when opening disk

char rootBuf[BLKSIZE]; // Need buf to hold root so it doesnt get changed every time get_block is called.
INODE *root;


int get_block(int dev, int blk, char *buf)
{   
   lseek(dev, blk*BLKSIZE, SEEK_SET);
   return read(dev, buf, BLKSIZE);
}

// searches the DIR's data block for a name string; 
// return its inode number if found; return 0 if not.
int search(INODE *ip, char *name)
{
   char sbuf[BLKSIZE], temp[256];
   DIR *dp;
   char *cp;
   
   // ASSUME only one data block i_block[0]
   // YOU SHOULD print i_block[0] number here
   get_block(dev, ip->i_block[0], sbuf);

   dp = (DIR *)sbuf;
   cp = sbuf;
 
   while(cp < sbuf + BLKSIZE){
      strncpy(temp, dp->name, dp->name_len);
      temp[dp->name_len] = 0;

      if(strcmp(temp, name) == 0)
         return dp->inode;

       cp += dp->rec_len;
       dp = (DIR *)cp;
   }
   return 0;
}

int show_dir(INODE *ip)
{
   char sbuf[BLKSIZE], temp[256];
   DIR *dp;
   char *cp;
 
   // ASSUME only one data block i_block[0]
   // YOU SHOULD print i_block[0] number here
   get_block(dev, ip->i_block[0], sbuf);

   dp = (DIR *)sbuf;
   cp = sbuf;
 
   while(cp < sbuf + BLKSIZE){
       strncpy(temp, dp->name, dp->name_len);
       temp[dp->name_len] = 0;
      
       printf("%4d %4d %4d %s\n", dp->inode, dp->rec_len, dp->name_len, temp);
       //printf("%d %d\n", cp, sbuf + BLKSIZE);
       cp += dp->rec_len;
       dp = (DIR *)cp;
   }
}

// mount root: Let INODE *root point at root INODE (ino=2) in memory
int mount_root()
{
   get_block(dev, inodes_block, rootBuf);
   root = (INODE *)rootBuf + 1;
}

/*************************************************************************/
int tokenize(char *pathname)
{
   strcpy(gpath, pathname);
   char *s = strtok(gpath, "/");
   n = 0;
   while(s) {
      name[n++] = s;
      s = strtok(NULL, "/");
   }
} 

// the start of the "showblock" program
// usage: "./a.out /a/b/c/d"
// that means you will have to parse argv for the path
// see the example 'lab5.bin' program
// example usage: ./lab5.bin lost+found
int main(int argc, char *argv[])
{
   // follow the steps here: https://eecs.wsu.edu/~cs360/LAB5.html
   // Read in super block (block 1 for 1k block size)
   dev = open(disk, O_RDONLY);
   char buf[BLKSIZE];
   get_block(dev, 1, buf);

   SUPER *sp = (SUPER *) buf; // Change buf to a super pointer
   // if s_magic == 0xEF53, then diskimage is a ext2 file system
   if(sp->s_magic == 0xEF53)
      printf("Disk image is an ext2 file system\n");

   // Read in Group Descriptor 0 (in block #2) to get block number of 
   // bmap, imap, inodes_start;  Print their values.
   get_block(dev, 2, buf);
   GD *gd = (GD *)buf;
   bMapBlockNum = gd->bg_block_bitmap;
   iMapBlockNum = gd->bg_inode_bitmap;
   inodes_block = gd->bg_inode_table;
   printf("Bitmap Block Number: %d; iMap Block Number: %d; inodes_start Block Number: %d\n", bMapBlockNum, iMapBlockNum, inodes_block);
   
   mount_root();
   show_dir(root); // Show contents of root
   // Prompt user to press any key to continue
   printf("\nPress any key to continue...");
   getchar();

   tokenize(argv[1]); // tokenize pathname passed into a.out
   printf("tokenize %s\n",argv[1]);
   for (int i = 0; i < 32; i++) {
      if (name[i] != NULL) {
         printf("name[%d] = %s\n",i, name[i]);
      }
    }
   printf("\n");
   printf("\nPress any key to continue...\n");
   getchar();
   INODE *ip = root;
   int ino, blk, offset;

   char sbuf[BLKSIZE];

   for (int i = 0; i < n; i++) {
      ino = search(ip, name[i]);
      printf("searching for %s\n", name[i]);
      if (ino == 0) {
         printf("can't find %s\n", name[i]);
         show_dir(ip); 
         exit(1);
      }
      show_dir(ip);
      // Use Mailman's algorithm to Convert (dev, ino) to newINODE pointer
      blk = (ino - 1) / 8 + inodes_block;
      offset = (ino - 1) % 8;
      get_block(dev, blk, sbuf);
      ip = (INODE *) sbuf + offset;
   }

   if (!ip) {
      printf("Error: ip is null.\n");
      exit(1);
   }

   // At this point, ip should point to the INODE of the final component of pathname


   //       Print direct block numbers;
   printf("\n----------- DIRECT BLOCKS ----------------\n");
   for (int i = 0; i < 12 && ip->i_block[i] != 0; i++) {
      printf("%d ", ip->i_block[i]);
   }
   printf("\n");
   //       Print indirect block numbers; 
   if (ip->i_block[12] != 0) {
      printf("----------- INDIRECT BLOCKS ----------------\n");
      char buf2[BLKSIZE];
      get_block(dev, ip->i_block[12], buf2);
      int *indirect_block = (int *) buf2;
      for (int i = 0; i < BLKSIZE/sizeof(int); i++) {
         if (indirect_block[i] != 0) {
               printf("%d ", indirect_block[i]);
         }
      }
      printf("\n");
   }

   //       Print double indirect block numbers, if any
   if (ip->i_block[13] != 0) {
      printf("----------- DOUBLE INDIRECT BLOCKS ----------------\n");
      int *double_indirect_block = (int *) malloc(BLKSIZE);
      get_block(dev, ip->i_block[13], (char *)double_indirect_block);
      for (int i = 0; i < BLKSIZE/sizeof(int); i++) {
         if (double_indirect_block[i] != 0) {
               int *indirect_block = (int *) malloc(BLKSIZE);
               get_block(dev, double_indirect_block[i], (char *)indirect_block);
               for (int j = 0; j < BLKSIZE/sizeof(int); j++) {
                  if (indirect_block[j] != 0) {
                     printf("%d ", indirect_block[j]);
                  }
               }
               printf("\n");
               free(indirect_block);
         }
      }
      free(double_indirect_block);
   }

}