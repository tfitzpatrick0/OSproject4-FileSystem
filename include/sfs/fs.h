/* fs.h: SimpleFS file system */

#ifndef FS_H
#define FS_H

#include "sfs/disk.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

/* File System Constants */

#define MAGIC_NUMBER        (0xf0f03410)
#define INODES_PER_BLOCK    (128)               /* Number of inodes per block */
#define POINTERS_PER_INODE  (5)                 /* Number of direct pointers per inode */
#define POINTERS_PER_BLOCK  (1024)              /* Number of pointers per block */

/* File System Structures */

typedef struct SuperBlock SuperBlock;
struct SuperBlock {
    uint32_t    magic_number;                   /* File system magic number */
    uint32_t    blocks;                         /* Number of blocks in file system */
    uint32_t    inode_blocks;                   /* Number of blocks reserved for inodes */
    uint32_t    inodes;                         /* Number of inodes in file system */
};

typedef struct Inode      Inode;
struct Inode {
    uint32_t    valid;                          /* Whether or not inode is valid */
    uint32_t    size;                           /* Size of file */
    uint32_t    direct[POINTERS_PER_INODE];     /* Direct pointers */
    uint32_t    indirect;                       /* Indirect pointers */
};

typedef union  Block      Block;
union Block {
    SuperBlock  super;                          /* View block as superblock */
    Inode       inodes[INODES_PER_BLOCK];       /* View block as inode */
    uint32_t    pointers[POINTERS_PER_BLOCK];   /* View block as pointers */
    char        data[BLOCK_SIZE];               /* View block as data */
};

typedef struct FileSystem FileSystem;
struct FileSystem {
    Disk        *disk;                          /* Disk file system is mounted on */
    bool        *free_blocks;                   /* Free block bitmap */
    SuperBlock   meta_data;                     /* File system meta data */
};

/* File System Functions */

void    fs_debug(Disk *disk);
bool    fs_format(FileSystem *fs, Disk *disk);

bool    fs_mount(FileSystem *fs, Disk *disk);
void    fs_unmount(FileSystem *fs);

ssize_t fs_create(FileSystem *fs);
bool    fs_remove(FileSystem *fs, size_t inode_number);
ssize_t fs_stat(FileSystem *fs, size_t inode_number);

ssize_t fs_read(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset);
ssize_t fs_write(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset);

#endif

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
