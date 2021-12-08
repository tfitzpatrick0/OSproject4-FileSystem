/* unit_fs.c: Unit tests for SimpleFS file system */

#include "sfs/fs.h"
#include "sfs/logging.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

#include <unistd.h>

/* Functions */

void test_cleanup() {
    unlink("data/image.unit");
}

int test_00_fs_mount() {
    Disk *disk = disk_open("data/image.5", 5);
    assert(disk);

    FileSystem fs = {0};
    debug("Check mounting filesystem");
    assert(fs_mount(&fs, disk));
    assert(fs.disk           == disk);
    assert(fs.free_blocks);
    assert(fs.free_blocks[0] == false);
    assert(fs.free_blocks[1] == false);
    assert(fs.free_blocks[2] == false);
    assert(fs.free_blocks[3] == true);
    assert(fs.free_blocks[4] == true);

    debug("Check mounting filesystem (already mounted)");
    assert(fs_mount(&fs, disk) == false);

    fs_unmount(&fs);
    disk_close(disk);
    
    disk = disk_open("data/image.20", 20);
    assert(disk);

    debug("Check mounting filesystem");
    assert(fs_mount(&fs, disk));
    assert(fs.disk           == disk);
    assert(fs.free_blocks);
    assert(fs.free_blocks[0] == false);
    assert(fs.free_blocks[1] == false);
    assert(fs.free_blocks[2] == false);
    assert(fs.free_blocks[3] == true);
    assert(fs.free_blocks[4] == false);
    assert(fs.free_blocks[5] == false);
    assert(fs.free_blocks[6] == false);
    assert(fs.free_blocks[7] == false);
    assert(fs.free_blocks[8] == false);
    assert(fs.free_blocks[9] == false);
    assert(fs.free_blocks[10] == false);
    assert(fs.free_blocks[11] == false);
    assert(fs.free_blocks[12] == false);
    assert(fs.free_blocks[13] == false);
    assert(fs.free_blocks[14] == false);
    assert(fs.free_blocks[15] == true);
    assert(fs.free_blocks[16] == true);
    assert(fs.free_blocks[17] == true);
    assert(fs.free_blocks[18] == true);
    assert(fs.free_blocks[19] == true);

    debug("Check mounting filesystem (already mounted)");
    assert(fs_mount(&fs, disk) == false);

    fs_unmount(&fs);
    disk_close(disk);
    return EXIT_SUCCESS;
}

int test_01_fs_create() {
    assert(system("cp data/image.5 data/image.unit") == EXIT_SUCCESS);

    Disk *disk = disk_open("data/image.unit", 5);
    assert(disk);

    FileSystem fs = {0};
    assert(fs_mount(&fs, disk));

    debug("Check creating inodes");
    assert(fs_create(&fs) == 0);
    for (size_t i = 2; i < 128; i++) {
        assert(fs_create(&fs) == i);

        Block block;
        assert(disk_read(fs.disk, 1, block.data) != DISK_FAILURE);
        assert(block.inodes[i].valid == true);
        assert(block.inodes[i].size  == 0);
    }

    debug("Check creating inodes (table full)");
    assert(fs_create(&fs) < 0);
    assert(fs_create(&fs) < 0);

    fs_unmount(&fs);
    disk_close(disk);
    return EXIT_SUCCESS;
}

int test_02_fs_remove() {
    assert(system("cp data/image.20 data/image.unit") == EXIT_SUCCESS);

    Disk *disk = disk_open("data/image.unit", 20);
    assert(disk);

    FileSystem fs = {0};
    assert(fs_mount(&fs, disk));

    debug("Check removing inode 0");
    assert(fs_remove(&fs, 0) == false);

    debug("Check removing inode 2");
    assert(fs_remove(&fs, 2));
    assert(fs.free_blocks[4]);
    assert(fs.free_blocks[5]);
    assert(fs.free_blocks[6]);
    assert(fs.free_blocks[7]);
    assert(fs.free_blocks[8]);
    assert(fs.free_blocks[9]);
    assert(fs.free_blocks[13]);
    assert(fs.free_blocks[14]);

    Block block;
    assert(disk_read(fs.disk, 1, block.data) != DISK_FAILURE);
    assert(block.inodes[2].valid == false);
    assert(block.inodes[2].size  == 0);

    debug("Check removing inode 2 (already removed)");
    assert(fs_remove(&fs, 1) == false);

    fs_unmount(&fs);
    disk_close(disk);
    return EXIT_SUCCESS;
}

int test_03_fs_stat() {
    Disk *disk = disk_open("data/image.5", 5);
    assert(disk);

    FileSystem fs = {0};
    assert(fs_mount(&fs, disk));

    debug("Check stat on inode 1");
    assert(fs_stat(&fs, 1) == 965);
    assert(fs_stat(&fs, 2) == -1);

    fs_unmount(&fs);
    disk_close(disk);
    
    disk = disk_open("data/image.20", 20);
    assert(disk);
    assert(fs_mount(&fs, disk));

    debug("Check stat on inode 2");
    assert(fs_stat(&fs, 1) == -1);
    assert(fs_stat(&fs, 2) == 27160);

    fs_unmount(&fs);
    disk_close(disk);
    return EXIT_SUCCESS;
}

/* Main execution */

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s NUMBER\n\n", argv[0]);
        fprintf(stderr, "Where NUMBER is right of the following:\n");
        fprintf(stderr, "    0. Test fs_mount\n");
        fprintf(stderr, "    1. Test fs_create\n");
        fprintf(stderr, "    2. Test fs_remove\n");
        fprintf(stderr, "    3. Test fs_stat\n");
        return EXIT_FAILURE;
    }

    int number = atoi(argv[1]);
    int status = EXIT_FAILURE;

    assert(atexit(test_cleanup) == EXIT_SUCCESS);

    switch (number) {
        case 0:  status = test_00_fs_mount(); break;
        case 1:  status = test_01_fs_create(); break;
        case 2:  status = test_02_fs_remove(); break;
        case 3:  status = test_03_fs_stat(); break;
        default: fprintf(stderr, "Unknown NUMBER: %d\n", number); break;
    }

    return status;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
