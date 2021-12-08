/* disk.h: SimpleFS disk emulator */

#ifndef DISK_H
#define DISK_H

#include <stdbool.h>
#include <stdlib.h>

/* Disk Constants */

#define BLOCK_SIZE      (1<<12)
#define DISK_FAILURE    (-1)

/* Disk Structure */

typedef struct Disk Disk;

struct Disk {
    int	    fd;	        /* File descriptor of disk image	*/
    size_t  blocks;     /* Number of blocks in disk image	*/
    size_t  reads;      /* Number of reads to disk image	*/
    size_t  writes;     /* Number of writes to disk image	*/
}; 

/* Disk Functions */

Disk *	disk_open(const char *path, size_t blocks);
void	disk_close(Disk *disk);

ssize_t	disk_read(Disk *disk, size_t block, char *data);
ssize_t	disk_write(Disk *disk, size_t block, char *data);

#endif

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
