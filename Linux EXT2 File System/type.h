#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>

#include <ext2fs/ext2_fs.h>

// define shorter TYPES, save typing efforts

typedef struct ext2_super_block SUPER;
typedef struct ext2_group_desc  GD;
typedef struct ext2_inode       INODE;
typedef struct ext2_dir_entry_2 DIR; 

#define BLKSIZE          1024 

#define NPROC               2
#define NMINODE           128
#define NFD                 8
#define NOFT               64

// In-memory inodes structure: as an in-memory INODEs cache
typedef struct minode{		
  INODE INODE;            // disk INODE
  int   dev, ino;         // dev and ino INODE
  int   cached;           // in minodes cache
  int   modified;         // modified flag
  int   shareCount;       // number of users on this MINODE
}MINODE;

// Open File Table        // For Level-2 
typedef struct oft{
  int   mode;
  int   shareCount;
  struct minode *inodeptr;
  int   offset; 
} OFT;

// PROC structure
typedef struct proc{
  int   uid;            // uid = 0 or nonzero
  int   gid;            // group ID; samse as uid
  int   pid;            // pid = 1 or 2
  struct minode *cwd;   // proc's CWD pointer
  OFT   *fd[NFD];       // proc's open file descriptor array
} PROC;