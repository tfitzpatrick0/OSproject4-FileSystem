/* fs.c: SimpleFS file system */

#include "sfs/fs.h"
#include "sfs/logging.h"
#include "sfs/utils.h"

#include <stdio.h>
#include <string.h>

/* Internal Prototypes */

void    fs_initialize_free_block_bitmap(FileSystem *fs);
ssize_t fs_allocate_free_block(FileSystem *fs);
void    disk_clear_data(Disk *disk);
void    block_clear_data(Block *block);

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
    printf("    %u inodes"         , block.super.inodes);

    /* Read Inodes */
    Block iblock;
    
    // loop through inode blocks
    for (uint32_t i = 0; i < block.super.inode_blocks; ++i) {
        // read inode block
        disk_read(disk, i+1, iblock.data);

        // loop through inodes in inode block
        for (uint32_t j = 0; j < INODES_PER_BLOCK; ++j) {
            Inode inode = iblock.inodes[j];
            if (inode.valid) {
                printf("\n");
                printf("Inode %u:\n", (i * INODES_PER_BLOCK) + j);
                printf("    size: %u bytes\n", inode.size);
                printf("    direct blocks:");
            

                // loop through direct pointers
                for (uint32_t k = 0; k < POINTERS_PER_INODE; ++k) {
                    //printf("\nk = %u\n", k);
                    if (inode.direct[k]) {
                        printf(" %lu", (unsigned long)(inode.direct[k]));
                    }
                }
                if (inode.indirect) { 
                    printf("\n");
                    printf("    indirect block: %lu\n", (unsigned long)(inode.indirect));
                    printf("    indirect data blocks:");

                    Block inblock;
                    disk_read(disk, inode.indirect, inblock.data);
                    // loop through indirect pointers
                    for (uint32_t a = 0; a < POINTERS_PER_BLOCK; ++a) {
                        if (inblock.pointers[a]){
                            printf(" %d",(inblock.pointers[a]));
                        }
                    }

                }
            }
        }
    }
    printf("\n");
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
    
    block_clear_data(&superBlock);

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
    
    // mark inodes, the direct blocks, the indirect blocks, and the pointers in the indirect blocks
    Block inodeBlock;
    for (uint32_t i = 0; i < fs->meta_data.inode_blocks; ++i) {
        
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
    //fprintf(stderr, "\ndisk = NULL\n");
    free(fs->free_blocks);
    fs->free_blocks = NULL;
    //fprintf(stderr, "\nfree_blocks freed\n");
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

                /*    
                // lets see if this helps
                block.inodes[j].size = 0;
                for (uint32_t k; k < POINTERS_PER_INODE; ++k) {
                    block.inodes[j].direct[k] = 0;
                }
                block.inodes[j].indirect = 0;
                */

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
    if (!fs) {
        fprintf(stderr, "No fs");
        return false;
    }

    // load inode information
    Inode remove_inode;
    if (!fs_load_inode(fs, inode_number, &remove_inode)) {
        fprintf(stderr, "load inode error\n");
        return false;
    }

    remove_inode.valid = false;
    remove_inode.size = 0;

    // free direct blocks
    for (uint32_t i = 0; i < POINTERS_PER_INODE; i++) {
        if (remove_inode.direct[i] != 0) {
            fs->free_blocks[remove_inode.direct[i]] = true;
        }
        remove_inode.direct[i] = 0;
    }

    // free indirect blocks
    if (remove_inode.indirect) {
        Block pointerBlock;
        disk_read(fs->disk, remove_inode.indirect, pointerBlock.data);

        for (uint32_t i = 0; i < POINTERS_PER_BLOCK; i++) {
            if (pointerBlock.pointers[i] != 0) {
                fs->free_blocks[pointerBlock.pointers[i]] = true;
            }
        }

        fs->free_blocks[remove_inode.indirect] = true;
    }

    remove_inode.indirect = 0;

    return (fs_save_inode(fs, inode_number, &remove_inode));
}

/**
 * Return size of specified Inode.
 *
 * @param       fs              Pointer to FileSystem structure.
 * @param       inode_number    Inode to remove.
 * @return      Size of specified Inode (-1 if does not exist).
 **/
ssize_t fs_stat(FileSystem *fs, size_t inode_number) {
    Inode inode;
    bool valid_inode = fs_load_inode(fs, inode_number, &inode);

    if (!valid_inode) {
        fprintf(stderr, "fs_stat: load inode failed\n");
        return -1;
    }

    return inode.size;
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
    // load inode and error check
    Inode inode;


    bool valid_inode = fs_load_inode(fs, inode_number, &inode);
    if (!valid_inode) {
        fprintf(stderr, "fs_read: invalid inode\n");
        return -1;
    }
    
    if (length < 0) {
        fprintf(stderr, "fs_read: length < 0\n");
        return -1;
    }

    if (offset < 0) {
        fprintf(stderr, "fs_read: offset < 0\n");
        return -1;
    }

    if (offset == inode.size) {
        fprintf(stderr, "return HERE?!?");
        return 0;
    }

    
    uint32_t size_check = inode.size;


    // block to load data into
    Block dataBlock;

    // block to store indirect pointers
    Block pointerBlock;

    // counters  
    size_t before_offset = offset;

    uint32_t direct_num = 0;
    uint32_t indirect_num = 0;   
    
    for (direct_num = 0; direct_num < POINTERS_PER_INODE; ++direct_num) {
        // *** could use free list
        if (inode.direct[direct_num] == 0) {
            fprintf(stderr, "fs_read: direct block not found, offset too large\noffset is %lu, direct block searched for was %u\n", offset, direct_num);
            return -1; 
        }
        
        disk_read(fs->disk, inode.direct[direct_num], dataBlock.data);

        // get to correct starting block
        if (before_offset < BLOCK_SIZE) {
            fprintf(stderr, "found direct block pointer # %u to start\n", direct_num);
            ++direct_num;
            break;
        }
        else {
            before_offset -= BLOCK_SIZE;
        }
    }
    
    if (before_offset >= BLOCK_SIZE) {
        
    
        // make sure indirect block exists
        // *** could use free list
        if (inode.indirect == 0) {
            fprintf(stderr, "fs_read: indirect block not found, offset too large\n");
            return -1;
        }

        // read in indirect pointer block 
        disk_read(fs->disk, inode.indirect, pointerBlock.data);

        for (indirect_num = 0; indirect_num < POINTERS_PER_BLOCK; ++indirect_num) {
            // check for valid data block
            // *** could use free list
            if (pointerBlock.pointers[indirect_num] == 0) {
                fprintf(stderr, "fs_read: indirect block at index not found, offset too large\n");
                return -1; 
            }
            
            disk_read(fs->disk, pointerBlock.pointers[indirect_num], dataBlock.data);

            // get to correct starting block
            if (before_offset < BLOCK_SIZE) {
                fprintf(stderr, "found indirect block pointer # %u to start\n", indirect_num);
                ++indirect_num;
                break;
            }
            else {
                before_offset -= BLOCK_SIZE;
            }
        }
    }
        
    if (before_offset >= BLOCK_SIZE) {
        fprintf(stderr, "fs_read: all indirect blocks checked, offset too large\n");
        return -1;
    }


    // if we got here, then:
    // dataBlock should contain the data from the correct block to start with
    // before_offset should be between 0 and BLOCK_SIZE - 1

    // we need to:
    // start reading from dataBlock at before_offset
    // go until no more blocks or length is reached


    // output container
    char *tempData = (char*)malloc((sizeof(char) * length) + 1);

    // accounting
    size_t upto = length;
    
    //tempData = "";    
    
    if (size_check > BLOCK_SIZE) {

            // append to tempData
            upto -= snprintf(tempData, upto + 1, "%.*s", BLOCK_SIZE, dataBlock.data);  
            
            size_check -= BLOCK_SIZE;
        }
        else {
            upto -= snprintf(tempData, size_check + 1, "%.*s", BLOCK_SIZE, dataBlock.data);  
            fprintf(stderr, "should exit here 1\n"); 
                
            strcpy(data, tempData);
        
            free(tempData);

            return strlen(data); 


        }

    //read in offset amount and update how much reading remains
    //upto -= snprintf(tempData, upto + 1, "%.*s", BLOCK_SIZE, dataBlock.data + before_offset);    

    //fprintf(stderr, "\n\n tempData after first snprintf: %s\n\n", tempData); 

    // always check if upto > 0
    for (; direct_num < POINTERS_PER_INODE; ++direct_num) {
        fprintf(stderr, "looking at direct block pointer # %u to read in\n", direct_num);

        // *** could use free list
        if (inode.direct[direct_num] == 0 || upto <= 0) {
            //fprintf(stderr, "fs_read: direct block not found, offset too large\n");
                
            //tempData[length + 1] = '\0';
            
            strcpy(data, tempData);
                        
            //fprintf(stderr, "\n\n tempData before returning: %s\n\n", tempData); 
            free(tempData);

            //fprintf(stderr, "\n\n data before returning: %s\n\n", data); 
            
            //free(tempData);
            
            return strlen(data);
        }

        // read next data block
        disk_read(fs->disk, inode.direct[direct_num], dataBlock.data);

        if (size_check > BLOCK_SIZE) {

            // append to tempData
            upto -= snprintf(tempData + strlen(tempData), upto + 1, "%.*s", BLOCK_SIZE, dataBlock.data);  
            
            size_check -= BLOCK_SIZE;
        }
        else {
            upto -= snprintf(tempData + strlen(tempData), size_check + 1, "%.*s", BLOCK_SIZE, dataBlock.data);  
            fprintf(stderr, "should exit here 2\n"); 
            strcpy(data, tempData);
        
            free(tempData);

            return strlen(data); 


        }
        //fprintf(stderr, "\n\n tempData after another snprintf: %s\n\n", tempData); 
    }

    // check if there is an indirect block
    if (inode.indirect == 0) {
        //fprintf(stderr, "fs_read: indirect block not found, offset too large");

        //tempData[length + 1] = '\0';
    
        strcpy(data, tempData);
        
        free(tempData);

        return strlen(data); 
    }

    // read in indirect pointer block 
    disk_read(fs->disk, inode.indirect, pointerBlock.data);

    

    for (; indirect_num < POINTERS_PER_BLOCK; ++indirect_num) {
            fprintf(stderr, "looking at indirect block pointer # %u to read from\n", indirect_num);

            // check for valid data block
            // *** could use free list
            if (pointerBlock.pointers[indirect_num] == 0 || upto <= 0) {
                //fprintf(stderr, "fs_read: indirect block at index not found, offset too large\n");
                //return -1; 
                
                //tempData[length + 1] = '\0';

                strcpy(data, tempData);

                free(tempData);

                return strlen(data);
            }
            
            // read next data block
            disk_read(fs->disk, pointerBlock.pointers[indirect_num], dataBlock.data);
            
            if (size_check > BLOCK_SIZE) {

                // append to tempData
                upto -= snprintf(tempData + strlen(tempData), upto + 1, "%.*s", BLOCK_SIZE, dataBlock.data);  
            
                size_check -= BLOCK_SIZE;
            }
            else {
                upto -= snprintf(tempData + strlen(tempData), size_check + 1, "%.*s", BLOCK_SIZE, dataBlock.data); 
                fprintf(stderr, "should exit here 3\n"); 
                strcpy(data, tempData);
        
                free(tempData);

                return strlen(data); 

            }

            // append to tempData
            //upto -= snprintf(tempData + strlen(tempData), upto + 1, "%.*s", BLOCK_SIZE, dataBlock.data); 

    }    

    //tempData[length + 1] = '\0';

    strcpy(data, tempData);

    free(tempData);

    return strlen(data);

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
 * @return      Number of bytes written (-1 on error).
 **/

/*
ssize_t fs_write(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset) {

    //fprintf(stderr, "Starting fs_write\n");
    //fprintf(stderr, "length = %u\n", length);

    if (!fs || length < 0 || offset < 0) {
        return -1;
    }

    Inode write_inode;
    if (!fs_load_inode(fs, inode_number, &write_inode)) {
        return -1;
    }

    // lets see if this helps
    write_inode.size = 0;
    for (uint32_t k = 0; k < POINTERS_PER_INODE; ++k) {
        write_inode.direct[k] = 0;
    }
    
    write_inode.indirect = 0;

    write_inode.valid = 1;


    ssize_t bytes_written = 0;
    Block buffer;

    block_clear_data(&buffer);
    

    uint32_t total_blks = (length / BLOCK_SIZE);
    uint32_t remainder = (length % BLOCK_SIZE);

    if (remainder != 0) {
        total_blks++;
    }

    //fprintf(stderr, "total_blks = %u, remainder = %u\n", total_blks, remainder);

    for (uint32_t blks = 0; blks < total_blks; blks++) {
        
        bool use_indirect = true;

        if ((blks < total_blks-1) || remainder == 0) {
            bytes_written = (ssize_t)BLOCK_SIZE;
        
            strncpy(buffer.data, data+(blks*BLOCK_SIZE), BLOCK_SIZE);
            //bytes_written += BLOCK_SIZE;
        }
        else {
            strncpy(buffer.data, data+(blks*BLOCK_SIZE), remainder);
            bytes_written = remainder;
        }

        // check direct pointers
        for (uint32_t i = 0; i < POINTERS_PER_INODE; i++) {
            if (write_inode.direct[i]) {
                continue;
            }
            else {
                // find available block
                ssize_t block_num = fs_allocate_free_block(fs);

                if (block_num > fs->meta_data.blocks) {
                    fprintf(stderr, "no more blocks available, couldnt make direct block %u: exiting write\n", i);
                    //write_inode.size += bytes_written;
                    if (!fs_save_inode(fs, inode_number, &write_inode)) {
                        return -1;
                    }

                    return write_inode.size;

                }


                // clear block
                Block tempBlock;
                disk_read(fs->disk, block_num, tempBlock.data);
            
                block_clear_data(&tempBlock);
    
                disk_write(fs->disk, block_num, tempBlock.data);
    

                // add block_num to direct pointers list
                write_inode.direct[i] = block_num;
                // fprintf(stderr, "\n\nDirect link [%u]: %u\n\n", i, write_inode.direct[i]);

                // write buffer to block @ block_num
                disk_write(fs->disk, block_num, buffer.data);
                
                write_inode.size += bytes_written;    

                // set use_indirect flag to false
                use_indirect = false;

                // exit loop
                break;
            }
        }

        // Use indirect blocks if direct pointers are full
        if (use_indirect) {

            if (!write_inode.indirect) {
                // allocate block to hold indirect pointers
                write_inode.indirect = fs_allocate_free_block(fs);
                
                fprintf(stderr, "indirect block at %u\n", write_inode.indirect);
                
                if (write_inode.indirect > fs->meta_data.blocks) {
                    fprintf(stderr, "no more blocks available, couldnt make indirect block: exiting write\n");
                    //write_inode.size += bytes_written;
                    if (!fs_save_inode(fs, inode_number, &write_inode)) {
                        return -1;
                    }

                    //return bytes_written;
                    return write_inode.size;

                }

                fprintf(stderr, "about to clear the indirect block\n");
        
                // clear block
                Block tempBlock;
                disk_read(fs->disk, write_inode.indirect, tempBlock.data);
            
                block_clear_data(&tempBlock);
    
                disk_write(fs->disk, write_inode.indirect, tempBlock.data);

                fprintf(stderr, "block has (hopefully) been cleared\n");
            }

            Block pointerBlock;
            disk_read(fs->disk, write_inode.indirect, pointerBlock.data);

            // loop through indirect block to find free pointer
            for (uint32_t i = 0; i < POINTERS_PER_BLOCK; i++) {
                fprintf(stderr, "was block cleared? who knows, pointerBlock.pointers[%u] = %u\n", i, pointerBlock.pointers[i]);
                if (!pointerBlock.pointers[i]) {

                    fprintf(stderr, "block pointer %u\n", i);    

                    // find available block
                    ssize_t block_num = fs_allocate_free_block(fs);

                    if (block_num > fs->meta_data.blocks) {
                        fprintf(stderr, "no more blocks available, couldnt make indirect pointer: exiting write\n");
                        //write_inode.size += bytes_written;
                        if (!fs_save_inode(fs, inode_number, &write_inode)) {
                            return -1;
                        }

                        //return bytes_written;
                        return write_inode.size;

                    }


                    fprintf(stderr, "found indirect block pointer %lu\n", block_num);

                    pointerBlock.pointers[i] = block_num;

                
                    // clear block
                    Block tempBlock;
                    disk_read(fs->disk, block_num, tempBlock.data);
            
                    block_clear_data(&tempBlock);
    
                    disk_write(fs->disk, block_num, tempBlock.data);


                    
                    // write buffer to block @ block_num
                    disk_write(fs->disk, block_num, buffer.data);
                    write_inode.size += bytes_written;


                    // update indirect pointer block and exit loop
                    disk_write(fs->disk, write_inode.indirect, pointerBlock.data);
                    break;
                }
            }
        }
    }

    write_inode.size += bytes_written;
    if (!fs_save_inode(fs, inode_number, &write_inode)) {
        return -1;
    }

    //fprintf(stderr, "About to return\n\n");
    return write_inode.size;
}
*/

// tim's fs_write
ssize_t fs_write(FileSystem *fs, size_t inode_number, char *data, size_t length, size_t offset) {

    //fprintf(stderr, "Starting fs_write\n");
    //fprintf(stderr, "length = %u\n", length);

    if (!fs || length < 0 || offset < 0) {
        return -1;
    }

    Inode write_inode;
    if (!fs_load_inode(fs, inode_number, &write_inode)) {
        return -1;
    }

    // lets see if this helps
    write_inode.size = 0;
    for (uint32_t k = 0; k < POINTERS_PER_INODE; ++k) {
        write_inode.direct[k] = 0;
    }
    
    write_inode.indirect = 0;

    write_inode.valid = 1;


    ssize_t bytes_written = 0;
    Block buffer;

    block_clear_data(&buffer);
    

    uint32_t total_blks = (length / BLOCK_SIZE);
    uint32_t remainder = (length % BLOCK_SIZE);

    if (remainder != 0) {
        total_blks++;
    }

    //fprintf(stderr, "total_blks = %u, remainder = %u\n", total_blks, remainder);

    for (uint32_t blks = 0; blks < total_blks; blks++) {

        bool use_indirect = true;

        if ((blks < total_blks-1) || remainder == 0) {
            strncpy(buffer.data, data+(blks*BLOCK_SIZE), BLOCK_SIZE);
            bytes_written = (ssize_t)BLOCK_SIZE;
        }
        else {
            strncpy(buffer.data, data+(blks*BLOCK_SIZE), remainder);
            bytes_written = remainder;
        }

        // check direct pointers
        for (uint32_t i = 0; i < POINTERS_PER_INODE; i++) {
            if (write_inode.direct[i]) {
                continue;
            }
            else {
                // find available block
                ssize_t block_num = fs_allocate_free_block(fs);

                if (block_num > fs->meta_data.blocks) {
                    fprintf(stderr, "no more blocks available, couldnt make direct block %u: exiting write\n", i);
                    // write_inode.size += bytes_written;
                    if (!fs_save_inode(fs, inode_number, &write_inode)) {
                        return -1;
                    }

                    return write_inode.size;

                }


                // clear block
                Block tempBlock;
                disk_read(fs->disk, block_num, tempBlock.data);
            
                block_clear_data(&tempBlock);
    
                disk_write(fs->disk, block_num, tempBlock.data);
    

                // add block_num to direct pointers list
                write_inode.direct[i] = block_num;
                // fprintf(stderr, "\n\nDirect link [%u]: %u\n\n", i, write_inode.direct[i]);

                // write buffer to block @ block_num
                disk_write(fs->disk, block_num, buffer.data);
                write_inode.size += bytes_written;

                // set use_indirect flag to false
                use_indirect = false;

                // exit loop
                break;
            }
        }

        // Use indirect blocks if direct pointers are full
        if (use_indirect) {

            if (!write_inode.indirect) {
                // allocate block to hold indirect pointers
                write_inode.indirect = fs_allocate_free_block(fs);
                
                fprintf(stderr, "indirect block at %u\n", write_inode.indirect);
                
                if (write_inode.indirect > fs->meta_data.blocks) {
                    fprintf(stderr, "no more blocks available, couldnt make indirect block: exiting write\n");
                    // write_inode.size += bytes_written;
                    if (!fs_save_inode(fs, inode_number, &write_inode)) {
                        return -1;
                    }

                    return write_inode.size;

                }

                fprintf(stderr, "about to clear the indirect block\n");
        
                // clear block
                Block tempBlock;
                disk_read(fs->disk, write_inode.indirect, tempBlock.data);
            
                block_clear_data(&tempBlock);
    
                disk_write(fs->disk, write_inode.indirect, tempBlock.data);

                fprintf(stderr, "block has (hopefully) been cleared\n");
            }

            Block pointerBlock;
            disk_read(fs->disk, write_inode.indirect, pointerBlock.data);

            // loop through indirect block to find free pointer
            for (uint32_t i = 0; i < POINTERS_PER_BLOCK; i++) {
                fprintf(stderr, "was block cleared? who knows, pointerBlock.pointers[%u] = %u\n", i, pointerBlock.pointers[i]);
                if (!pointerBlock.pointers[i]) {

                    fprintf(stderr, "block pointer %u\n", i);    

                    // find available block
                    ssize_t block_num = fs_allocate_free_block(fs);

                    if (block_num > fs->meta_data.blocks) {
                        fprintf(stderr, "no more blocks available, couldnt make indirect pointer: exiting write\n");
                        // write_inode.size += bytes_written;
                        if (!fs_save_inode(fs, inode_number, &write_inode)) {
                            return -1;
                        }

                        return write_inode.size;

                    }


                    fprintf(stderr, "found indirect block pointer %lu\n", block_num);

                    pointerBlock.pointers[i] = block_num;

                
                    // clear block
                    Block tempBlock;
                    disk_read(fs->disk, block_num, tempBlock.data);
            
                    block_clear_data(&tempBlock);
    
                    disk_write(fs->disk, block_num, tempBlock.data);


                    
                    // write buffer to block @ block_num
                    disk_write(fs->disk, block_num, buffer.data);
                    write_inode.size += bytes_written;

                    // update indirect pointer block and exit loop
                    disk_write(fs->disk, write_inode.indirect, pointerBlock.data);
                    break;
                }
            }
        }
    }

    if (!fs_save_inode(fs, inode_number, &write_inode)) {
        return -1;
    }

    //fprintf(stderr, "About to return\n\n");
    return write_inode.size;
}

// helper function to initialize bitmap
void    fs_initialize_free_block_bitmap(FileSystem *fs) {

    // initialize all blocks to true (Free)
    for (uint32_t i = 0; i < fs->meta_data.blocks; i++) {       
        fs->free_blocks[i] = true;
    }

    // set super block to false (Occupied)
    fs->free_blocks[0] = false;

    // set inode blocks to false (Occupied)
    for (uint32_t i = 0; i < fs->meta_data.inode_blocks; i++) {
        fs->free_blocks[i+1] = false;
    }
}

// helper function to allocate a free block
ssize_t fs_allocate_free_block(FileSystem *fs) {

    fprintf(stderr, "fs_allocate: fs->meta_data.blocks = %u\n", fs->meta_data.blocks); 

    // loop through free blocks bitmap
    for (uint32_t i = 0; i < fs->meta_data.blocks; i++) {
        fprintf(stderr, "fs_allocate: looking at block %u\n", i);
    
        if (fs->free_blocks[i] == true) {
            fprintf(stderr, "found %u\n", i);
            // If a free block is found, occupy it and return the block number
            fs->free_blocks[i] = false;
            return i;
        }
    }
    return fs->meta_data.blocks + 1;
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


// helper function to clear a single block data
void    block_clear_data(Block *block) {
    for (size_t i = 0; i < BLOCK_SIZE; ++i) {
        block->data[i] = 0;
    }
}


// helper function to load inode @ inode_number into node
bool    fs_load_inode(FileSystem *fs, size_t inode_number, Inode *node) {  
    Block inodeBlock;

    // calculate block to read from
    size_t inode_block_num = (inode_number / INODES_PER_BLOCK) + 1;

    if (inode_block_num > fs->meta_data.inode_blocks) {
        fprintf(stderr, "fs_load: block num > blocks\nblock_num = %lu, iblocks = %d", inode_block_num, fs->meta_data.inode_blocks);
        return false;
    }
    
    // read from disk
    disk_read(fs->disk, inode_block_num, inodeBlock.data);

    // calculate inode in block to get
    uint32_t inode_offset = (inode_number % INODES_PER_BLOCK);

    // set output
    *node = inodeBlock.inodes[inode_offset];
    
    // check node is valid before returning
    if (!node->valid) {
        fprintf(stderr, "fs_load: inode not valid\n");
        return false;
    }    

    return true;
}

// helper function to save data in node to inode @ inode_number
bool    fs_save_inode(FileSystem *fs, size_t inode_number, Inode *node) {
    Block inodeBlock;

    // calculate block to read from
    size_t inode_block_num = (inode_number / INODES_PER_BLOCK) + 1;

    if (inode_block_num > fs->meta_data.inode_blocks) {
        return false;
    }

    // read from disk
    disk_read(fs->disk, inode_block_num, inodeBlock.data);

    // calculate inode in block to get
    uint32_t inode_offset = (inode_number % INODES_PER_BLOCK);

    // set inodeblock to write back to disk
    inodeBlock.inodes[inode_offset] = *node;

    disk_write(fs->disk, inode_block_num, inodeBlock.data);
    return true;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
