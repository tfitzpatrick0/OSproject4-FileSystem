/* unit_disk.c: Unit tests for SimpleFS disk emulator */

#include "sfs/disk.h"
#include "sfs/logging.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include <unistd.h>

/* Constants */

#define DISK_PATH   "unit_disk.image"
#define DISK_BLOCKS (4)

/* Functions */

void test_cleanup() {
    unlink(DISK_PATH);
}

int test_00_disk_open() {
    debug("Check bad path");
    Disk *disk = disk_open("/asdf/NOPE", 10);
    assert(disk == NULL);
   
    debug("Check bad block size");
    disk = disk_open(DISK_PATH, LONG_MAX);
    assert(disk == NULL);

    debug("Check disk attributes");
    disk = disk_open(DISK_PATH, 10);
    assert(disk);
    assert(disk->fd      >= 0);
    assert(disk->blocks  == 10);
    assert(disk->reads   == 0);
    assert(disk->writes  == 0);
    disk_close(disk);

    return EXIT_SUCCESS;
}

int test_01_disk_read() {
    Disk *disk = disk_open(DISK_PATH, DISK_BLOCKS);
    assert(disk);

    char data[DISK_BLOCKS*BLOCK_SIZE] = {0};
    for (size_t i = 0; i < DISK_BLOCKS*BLOCK_SIZE; i++) {
        data[i] = i / BLOCK_SIZE;
    }
    assert(write(disk->fd, data, DISK_BLOCKS*BLOCK_SIZE) == DISK_BLOCKS*BLOCK_SIZE);
    
    debug("Check bad disk");
    assert(disk_read(NULL, 0, data) == DISK_FAILURE);
    
    debug("Check bad block");
    assert(disk_read(disk, DISK_BLOCKS, data) == DISK_FAILURE);
    
    debug("Check bad data");
    assert(disk_read(disk, DISK_BLOCKS, NULL) == DISK_FAILURE);

    for (size_t b = 0; b < DISK_BLOCKS; b++) {
        debug("Check read block %lu", b);
        assert(disk_read(disk, b, data) == BLOCK_SIZE);
        for (size_t i = 0; i < BLOCK_SIZE; i++) {
            assert(data[i] == b);
        }

        assert(disk->reads == b + 1);
    }
    disk_close(disk);
    return EXIT_SUCCESS;
}

int test_02_disk_write() {
    Disk *disk = disk_open(DISK_PATH, DISK_BLOCKS);
    assert(disk);
    
    char data[BLOCK_SIZE] = {0};

    debug("Check bad disk");
    assert(disk_write(NULL, 0, data) == DISK_FAILURE);
    
    debug("Check bad block");
    assert(disk_write(disk, DISK_BLOCKS, data) == DISK_FAILURE);
    
    debug("Check bad data");
    assert(disk_write(disk, DISK_BLOCKS, NULL) == DISK_FAILURE);

    for (size_t b = 0; b < DISK_BLOCKS; b++) {
        debug("Check write block %lu", b);
        memset(data, b, BLOCK_SIZE);
        assert(disk_write(disk, b, data) == BLOCK_SIZE);

        memset(data, 0, BLOCK_SIZE);
        assert(disk_read(disk, b, data) == BLOCK_SIZE);

        for (size_t i = 0; i < BLOCK_SIZE; i++) {
            assert(data[i] == b);
        }

        assert(disk->writes == b + 1);
    }
    disk_close(disk);
    return EXIT_SUCCESS;
}

/* Main execution */

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "Where NUMBER is right of the following:\n");
        fprintf(stderr, "    0. Test disk_open\n");
        fprintf(stderr, "    1. Test disk_read\n");
        fprintf(stderr, "    2. Test disk_write\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;

    atexit(test_cleanup);

    switch (number) {
        case 0:  status = test_00_disk_open(); break;
        case 1:  status = test_01_disk_read(); break;
        case 2:  status = test_02_disk_write(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }

    return status;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
