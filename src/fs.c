/* fs.c: SimpleFS file system */

#include "sfs/fs.h"
#include "sfs/logging.h"
#include "sfs/utils.h"

#include <stdio.h>
#include <string.h>


/* Internal Prototypes */

void    fs_initialize_free_block_bitmap(FileSystem *fs);
void    disk_clear_data(Disk *disk);

bool    fs_load_inode(FileSystem *fs, size_t inode_number, Inode *node);
bool    fs_save_inode(FileSystem *fs, size_t inode_number, Inode *node);


/* External Functions */

/**
 * Debug FileSystem by doing the following:
 *
 *  1. Read SuperBlock and report its information.
 *
 *  2. Read Inode Table and report information about each Inode.
 *
 * @param       disk        Pointer to Disk structure.
 **/
void    fs_debug(Disk *disk) {
    Block block;

    /* Read SuperBlock */
    if (disk_read(disk, 0, block.data) == DISK_FAILURE) {
        return;
    }

    printf("SuperBlock:\n");
    printf("    magic number is %s\n",
        (block.super.magic_number == MAGIC_NUMBER) ? "valid" : "invalid");
    printf("    %u blocks\n"         , block.super.blocks);
    printf("    %u inode blocks\n"   , block.super.inode_blocks);
    printf("    %u inodes\n"         , block.super.inodes);

    /* Read Inodes */

    Block inodeBlock;
    
    // loop through inode blocks
    for (uint32_t i = 1; i <= block.super.inode_blocks; ++i) {
        // read inode block
        disk_read(disk, i, inodeBlock.data);

        // loop through inodes in inode block
        for (uint32_t j = 0; j < INODES_PER_BLOCK; ++j) {
            Inode inode = inodeBlock.inodes[j];

            if (inode.valid) {
                printf("\n");
                printf("Inode %u:\n", i);
                printf("    size: %u\n", inode.size);
                printf("    direct blocks: %lu\n", (sizeof(inode.direct) / sizeof(uint32_t)));

            }

            // loop through direct pointers
            for (uint32_t k = 0; k < POINTERS_PER_INODE; ++k) {
                if (inode.direct[k]) {
                    printf(" %lu", (unsigned long)(inode.direct[k]));
                }
            }
            if (inode.indirect) {
                printf("\n");
                printf("    indirect block: %lu\n", (unsigned long)(inode.indirect));
                printf("    indirect data blocks:");


                Block pointerBlock;
                disk_read(disk, inode.indirect, pointerBlock.data);
                // loop through indirect pointers
                for (uint32_t a = 0; a < POINTERS_PER_BLOCK; ++a) {
                    if (pointerBlock.pointers[a]){
                        printf(" %d",(pointerBlock.pointers[a]));

                    }
                }
            }
        }

    }
}


* in disk.c
*
// helper function to clear data other than super block
void disk_clear_data(Disk *disk) {
    Block empty;
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        empty.data[i] = 0;
    }
    for (size_t j = 1; j < disk->blocks; ++j) {
        disk_write(disk, j, empty.data);

    }
}


/**
 * Format Disk by doing the following:
 *
 *  1. Write SuperBlock (with appropriate magic number, number of blocks,
 *  number of inode blocks, and number of inodes).
 *
 *  2. Clear all remaining blocks.
 *
 * Note: Do not format a mounted Disk!
 *
 * @param       fs      Pointer to FileSystem structure.
 * @param       disk    Pointer to Disk structure.
 * @return      Whether or not all disk operations were successful.
 **/
bool    fs_format(FileSystem *fs, Disk *disk) {
    // make sure not already mounted
    if (fs->disk == disk) {
        return false;
    }

    // write SuperBlock
    Block superBlock;
    superBlock.super.magic_number = MAGIC_NUMBER;
    superBlock.super.blocks = disk->blocks;

    // 10% of blocks must be inode blocks
    if (disk->blocks % 10 == 0) {
        superBlock.super.inode_blocks = disk->blocks / 10;
    }
    else {
        superBlock.super.inode_blocks = (disk->blocks / 10) + 1;
    }

    superBlock.super.inodes = superBlock.super.inode_blocks * INODES_PER_BLOCK;

    // write super block to first block on the disk
    disk_write(disk, 0, superBlock.data);
    

    // clear rest of the blocks in the disk
    disk_clear_data(disk);

    return true;
}

/**
 * Mount specified FileSystem to given Disk by doing the following:
 *
 *  1. Read and check SuperBlock (verify attributes).
 *
 *  2. Verify and record FileSystem disk attribute. 
 *
 *  3. Copy SuperBlock to FileSystem meta data attribute
 *
 *  4. Initialize FileSystem free blocks bitmap.
 *
 * Note: Do not mount a Disk that has already been mounted!
 *
 * @param       fs      Pointer to FileSystem structure.
 * @param       disk    Pointer to Disk structure.
 * @return      Whether or not the mount operation was successful.
 **/
bool    fs_mount(FileSystem *fs, Disk *disk) {
   // make sure not already mounted
    if (fs->disk == disk) {
        return false;
    }


    // verify SuperBlock
    Block superBlock;
    disk_read(disk, 0, superBlock.data);

    
    // magic number
    if (superBlock.super.magic_number != MAGIC_NUMBER) {
        return false;
    }

    // blocks
    if (superBlock.super.blocks != disk->blocks) {
        return false;
    }

    // number of inode blocks
    if (disk->blocks % 10 == 0) {
        if (superBlock.super.inode_blocks != disk->blocks / 10) {
            return false;
        }
    }
    else {
        if (superBlock.super.inode_blocks != (disk->blocks / 10) + 1) {
            return false;
        }
    }

    // number of inodes
    if (superBlock.super.inodes != superBlock.super.inode_blocks * INODES_PER_BLOCK) {
        return false;
    }

    // record file system disk attributes
    fs->disk = disk;

    // copy super block to meta data
    fs->meta_data.magic_number = superBlock.super.magic_number;
    fs->meta_data.blocks = superBlock.super.blocks;
    fs->meta_data.inode_blocks = superBlock.super.inode_blocks;
    fs->meta_data.inodes = superBlock.super.inodes;


    // initalize free blocks bitmap
    fs->free_blocks = malloc(fs->meta_data.blocks * sizeof(bool));
    fs_initialize_free_block_bitmap(fs); 
    
    
    return true;
}


/**
 * Unmount FileSystem from internal Disk by doing the following:
 *
 *  1. Set FileSystem disk attribute.
 *
 *  2. Release free blocks bitmap.
 *
 * @param       fs      Pointer to FileSystem structure.
 **/
void    fs_unmount(FileSystem *fs) {
    fs->disk = NULL;
    free(fs->free_blocks);
}

/**
 * Allocate an Inode in the FileSystem Inode table by doing the following:
 *
 *  1. Search Inode table for free inode.
 *
 *  2. Reserve free inode in Inode table.
 *
 * Note: Be sure to record updates to Inode table to Disk.
 *
 * @param       fs      Pointer to FileSystem structure.
 * @return      Inode number of allocated Inode.
 **/
ssize_t fs_create(FileSystem *fs) {

    Block block;

    // loop through all inode blocks
    for (uint32_t i = 0; i < fs->meta_data.inode_blocks; ++i) {

        // read current inode block into block
        disk_read(fs->disk, i+1, block.data);

        // loop through inodes in the current inode block
        for (uint32_t j = 0; j < INODES_PER_BLOCK; ++j) {

            int base = i * INODES_PER_BLOCK;
            int offset = j;

            // find free inode in current inode block and create inode
            if (!block.inodes[j].valid) {
                block.inodes[j].valid = 1;
                disk_write(fs->disk, i+1, block.data);

                // return the created inode block number
                return (base + offset);
            }
        }
    }

    return -1;
}

/**
 * Remove Inode and associated data from FileSystem by doing the following:
 *
 *  1. Load and check status of Inode.
 *
 *  2. Release any direct blocks.
 *
 *  3. Release any indirect blocks.
 *
 *  4. Mark Inode as free in Inode table.
 *
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to remove.
 * @return      Whether or not removing the specified Inode was successful.
 **/
bool    fs_remove(FileSystem *fs, size_t inode_number) {

    // sanity check
    if (fs->disk == NULL) {
        return false;
    }

    // load inode information
    Inode remove_inode;
    bool valid_inode = fs_load_inode(fs, inode_number, &remove_inode);

    if (!valid_inode) {
        return false;
    }

    for (uint32_t i = 0; i < POINTERS_PER_INODE; ++i) {
        remove_inode.direct[i] = 0;
        fs->free_blocks[remove_inode.direct[i]] = true;
    }

    return false;
}

// load inode from specified number into Inode structure
bool    fs_load_inode(FileSystem *fs, size_t inode_number, Inode *node) {
    Block inodeBlock;
    
    // sanity check
    if (fs->disk == NULL) {
        return false;
    }

    // calculate block to read from
    size_t inode_block_num = (inode_number / INODES_PER_BLOCK) + 1;

    if (inode_block_num > fs.meta_data.inode_blocks) {
        return false;
    }
    
    // calculate inode in block to get
    uint32_t inode_num = (inode_number % INODES_PER_BLOCK)
    
    // read from disk
    disk_read(fs->disk, inode_block_num, inodeBlock.data);

    // set output
    node = inodeBlock.inodes[inode_num];
    
    // check node is valid before returning
    if (!node.valid) {
        return false;
    }    

    return true;
}

/**
 * Return size of specified Inode.
 *
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to remove.
 * @return      Size of specified Inode (-1 if does not exist).
 **/
ssize_t fs_stat(FileSystem *fs, size_t inode_number) {
    return -1;
}

/**
 * Read from the specified Inode into the data buffer exactly length bytes
 * beginning from the specified offset by doing the following:
 *
 *  1. Load Inode information.
 *
 *  2. Continuously read blocks and copy data to buffer.
 *
 *  Note: Data is read from direct blocks first, and then from indirect blocks.
 *
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to read data from.
 * @param       data            Buffer to copy data to.
 * @param       length          Number of bytes to read.
 * @param       offset          Byte offset from which to begin reading.
 * @return      Number of bytes read (-1 on error).
 **/
ssize_t fs_read(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset) {
    return -1;
}

/**
 * Write to the specified Inode from the data buffer exactly length bytes
 * beginning from the specified offset by doing the following:
 *
 *  1. Load Inode information.
 *
 *  2. Continuously copy data from buffer to blocks.
 *
 *  Note: Data is read from direct blocks first, and then from indirect blocks.
 *
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to write data to.
 * @param       data            Buffer with data to copy
 * @param       length          Number of bytes to write.
 * @param       offset          Byte offset from which to begin writing.
 * @return      Number of bytes read (-1 on error).
 **/
ssize_t fs_write(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset) {
    return -1;
}



// helper function to initialize bitmap
void    fs_initialize_free_block_bitmap(FileSystem *fs) {

    // initialize all blocks to true (Free)
    for (uint32_t i = 0; i < fs->meta_data.blocks; i++) {       
        fs->free_blocks[i] = true;
    }

    // set super block to false (Occupied)
    fs->free_blocks[0] = false;
    
    // mark inodes, the direct blocks, the indirect blocks, and the pointers in the indirect blocks
    Block inodeBlock;
    for (uint32_t i = 0; i < fs->meta_data.inode_blocks; ++i) {
        fs->free_blocks[i+1] = false;
        
        disk_read(disk, i+1, inodeBlock.data);

        // loop through inodes in the inode block

        for (uint32_t j = 0; j < INODES_PER_BLOCK; ++j) {
            Inode inode = inodeBlock.inodes[j];
            
            // check if valid
            if (inode.valid) {
                    
                // loop through direct pointers
                for (uint32_t k = 0; k < POINTERS_PER_INODE; ++k) {
                    if (inode.direct[k]) {
                        fs->free_blocks[inode.direct[k]] = false;
                    }
                }

                // check indirect pointer
                if (inode.indirect) {
                    fs->free_blocks[inode.indirect] = false;

                    Block pointerBlock;
                    disk_read(disk, inode.indirect, pointerBlock.data);

                    // loop through indirect block
                    for (uint32_t a = 0; a < POINTERS_PER_BLOCK; ++a) {

                        if (pointerBlock.pointers[a]) {
                            fs->free_blocks[pointerBlock.pointers[a]] = false;

                        }
                    }
                }
            }
        }
    }

}

// helper function to clear data other than super block
void    disk_clear_data(Disk *disk) {

    // initialize new block to all 0 (empty)
    Block block;
    for (size_t i = 0; i < BLOCK_SIZE; ++i) {
        block.data[i] = 0;
    }

    // write the empty block to all blocks on disk except superblock
    for (size_t j = 1; j < disk->blocks; ++j) {
        disk_write(disk, j, block.data);
    }
}




// load inode from specified number into Inode structure
bool    fs_load_inode(FileSystem *fs, size_t inode_number, Inode *node) {
    Block inodeBlock;
    
    // sanity check
    if (fs->disk == NULL) {
        return false;
    }

    // calculate block to read from
    size_t inode_block_num = (inode_number / INODES_PER_BLOCK) + 1;

    if (inode_block_num > fs.meta_data.inode_blocks) {
        return false;
    }
    
    // calculate inode in block to get
    uint32_t inode_num = (inode_number % INODES_PER_BLOCK)

    
    // read from disk
    disk_read(fs->disk, inode_block_num, inodeBlock.data);

    // set output
    node = inodeBlock.inodes[inode_num];
    
    // check node is valid before returning
    if (!node.valid) {
        return false;
    }    

    return true;

}


/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
