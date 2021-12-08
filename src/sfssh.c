/* sfssh.c: SimpleFS shell */

#include "sfs/disk.h"
#include "sfs/fs.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

/* Macros */

#define streq(a, b)	(strcmp((a), (b)) == 0)

/* Command Prototyes */

void do_debug(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2);
void do_format(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2);
void do_mount(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2);
void do_create(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2);
void do_remove(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2);
void do_stat(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2);
void do_copyout(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2);
void do_cat(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2);
void do_copyin(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2);
void do_help(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2);

/* Utility Prototypes */

bool copyout(FileSystem *fs, size_t inode_number, const char *path);
bool copyin(FileSystem *fs, const char *path, size_t inode_number);

/* Main Execution */

int main(int argc, char *argv[]) {
    if (argc != 3) {
	fprintf(stderr, "Usage: %s <diskfile> <nblocks>\n", argv[0]);
	return EXIT_FAILURE;
    }

    Disk *disk = disk_open(argv[1], atoi(argv[2]));
    if (!disk) {
    	return EXIT_FAILURE;
    }

    FileSystem fs = {0};
    while (true) {
	char line[BUFSIZ], cmd[BUFSIZ], arg1[BUFSIZ], arg2[BUFSIZ];
	fprintf(stderr, "sfs> ");
	fflush(stderr);

	if (fgets(line, BUFSIZ, stdin) == NULL) {
	    break;
	}

	int args = sscanf(line, "%s %s %s", cmd, arg1, arg2);
	if (args == 0) {
	    continue;
	}


	if (streq(cmd, "debug")) {
	    do_debug(disk, &fs, args, arg1, arg2);
        } else if (streq(cmd, "format")) {
	    do_format(disk, &fs, args, arg1, arg2);
        } else if (streq(cmd, "mount")) {
	    do_mount(disk, &fs, args, arg1, arg2);
        } else if (streq(cmd, "create")) {
	    do_create(disk, &fs, args, arg1, arg2);
        } else if (streq(cmd, "remove")) {
	    do_remove(disk, &fs, args, arg1, arg2);
        } else if (streq(cmd, "stat")) {
	    do_stat(disk, &fs, args, arg1, arg2);
        } else if (streq(cmd, "copyout")) {
	    do_copyout(disk, &fs, args, arg1, arg2);
        } else if (streq(cmd, "cat")) {
	    do_cat(disk, &fs, args, arg1, arg2);
        } else if (streq(cmd, "copyin")) {
	    do_copyin(disk, &fs, args, arg1, arg2);
        } else if (streq(cmd, "help")) {
	    do_help(disk, &fs, args, arg1, arg2);
	} else if (streq(cmd, "exit") || streq(cmd, "quit")) {
	    break;
	} else {
	    printf("Unknown command: %s", line);
	    printf("Type 'help' for a list of commands.\n");
	}
    }

    fs_unmount(&fs);
    assert(fs.disk == NULL);
    assert(fs.free_blocks == NULL);
    disk_close(disk);
    return EXIT_SUCCESS;
}

/* Command Functions */

void do_debug(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2) {
    if (args != 1) {
	printf("Usage: debug\n");
	return;
    }
    fs_debug(disk);
}

void do_format(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2) {
    if (args != 1) {
	printf("Usage: format\n");
	return;
    }

    if (fs_format(fs, disk)) {
        printf("disk formatted.\n");
    } else {
        printf("format failed!\n");
    }
}

void do_mount(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2) {
    if (args != 1) {
	printf("Usage: mount\n");
	return;
    }

    if (fs_mount(fs, disk)) {
        printf("disk mounted.\n");
    } else {
        printf("mount failed!\n");
    }
}

void do_create(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2) {
    if (args != 1) {
        printf("Usage: create\n");
        return;
    }

    ssize_t inode_number = fs_create(fs);
    if (inode_number >= 0) {
        printf("created inode %ld.\n", inode_number);
    } else {
        printf("create failed!\n");
    }
}

void do_remove(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2) {
    if (args != 2) {
        printf("Usage: remove <inode>\n");
        return;
    }

    size_t inode_number = atoi(arg1);
    if (fs_remove(fs, inode_number)) {
        printf("removed inode %ld.\n", inode_number);
    } else {
        printf("remove failed!\n");
    }
}

void do_stat(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2) {
    if (args != 2) {
        printf("Usage: stat <inode>\n");
        return;
    }

    ssize_t inode_number = atoi(arg1);
    ssize_t bytes        = fs_stat(fs, inode_number);
    if (bytes >= 0) {
        printf("inode %ld has size %ld bytes.\n", inode_number, bytes);
    } else {
        printf("stat failed!\n");
    }
}

void do_copyout(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2) {
    if (args != 3) {
        printf("Usage: copyout <inode> <file>\n");
        return;
    }

    if (!copyout(fs, atoi(arg1), arg2)) {
        printf("copyout failed!\n");
    }
}

void do_cat(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2) {
    if (args != 2) {
        printf("Usage: cat <inode>\n");
        return;
    }

    if (!copyout(fs, atoi(arg1), "/dev/stdout")) {
        printf("cat failed!\n");
    }
}

void do_copyin(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2) {
    if (args != 3) {
        printf("Usage: copyin <file> <inode>\n");
        return;
    }

    if (!copyin(fs, arg1, atoi(arg2))) {
        printf("copyout failed!\n");
    }
}

void do_help(Disk *disk, FileSystem *fs, int args, char *arg1, char *arg2) {
    printf("Commands are:\n");
    printf("    format\n");
    printf("    mount\n");
    printf("    debug\n");
    printf("    create\n");
    printf("    remove  <inode>\n");
    printf("    cat     <inode>\n");
    printf("    stat    <inode>\n");
    printf("    copyin  <file> <inode>\n");
    printf("    copyout <inode> <file>\n");
    printf("    help\n");
    printf("    quit\n");
    printf("    exit\n");
}

/* Utility Functions */

bool copyin(FileSystem *fs, const char *path, size_t inode_number) {
    FILE *stream = fopen(path, "r");
    if (!stream) {
        fprintf(stderr, "Unable to open %s: %s\n", path, strerror(errno));
        return false;
    }

    char buffer[4*BUFSIZ] = {0};
    size_t offset = 0;
    while (true) {
        ssize_t result = fread(buffer, 1, sizeof(buffer), stream);
        if (result <= 0) {
            break;
        }
        ssize_t actual = fs_write(fs, inode_number, buffer, result, offset);
        if (actual < 0) {
            fprintf(stderr, "fs_write returned invalid result %ld\n", actual);
            break;
        }
        offset += actual;
        if (actual != result) {
            fprintf(stderr, "fs_write only wrote %ld bytes, not %ld bytes\n", actual, result);
            break;
        }
    }
    printf("%lu bytes copied\n", offset);
    fclose(stream);
    return true;
}

bool copyout(FileSystem *fs, size_t inode_number, const char *path) {
    FILE *stream = fopen(path, "w");
    if (!stream) {
        fprintf(stderr, "Unable to open %s: %s\n", path, strerror(errno));
        return false;
    }

    char buffer[4*BUFSIZ] = {0};
    size_t offset = 0;
    while (true) {
        ssize_t result = fs_read(fs, inode_number, buffer, sizeof(buffer), offset);
        if (result <= 0) {
            break;
        }
        fwrite(buffer, 1, result, stream);
        offset += result;
    }
    printf("%lu bytes copied\n", offset);
    fclose(stream);
    return true;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
