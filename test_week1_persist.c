/*
 * Week 1 persistence test: the directory must survive unmount and be seen
 * by a SEPARATE program run.
 *     ./tests/test_week1_persist write
 *     ./tests/test_week1_persist read
 */
#include <stdio.h>
#include <string.h>
#include "../fs.h"
#include "../disk.h"
#include "check.h"

#define DISK "w1persist.disk"

static int do_write(void)
{
    remove(DISK);
    CHECK(make_fs(DISK) == 0);
    CHECK(mount_fs(DISK) == 0);
    CHECK(fs_create("alpha") == 0);
    CHECK(fs_create("beta") == 0);
    CHECK(fs_create("gamma") == 0);
    CHECK(fs_delete("beta") == 0);            /* deletion must persist too */
    CHECK(umount_fs(DISK) == 0);
    return check_report("week1_persist(write)");
}

static int do_read(void)
{
    char names[10][FS_MAX_NAME + 1];
    int n, i, saw_alpha = 0, saw_gamma = 0, saw_beta = 0;

    CHECK(mount_fs(DISK) == 0);
    n = fs_listfiles(names, 10);
    CHECK(n == 2);
    for (i = 0; i < n; i++) {
        if (strcmp(names[i], "alpha") == 0) saw_alpha = 1;
        if (strcmp(names[i], "gamma") == 0) saw_gamma = 1;
        if (strcmp(names[i], "beta") == 0)  saw_beta = 1;
    }
    CHECK(saw_alpha && saw_gamma && !saw_beta);
    CHECK(fs_create("alpha") == -1);          /* duplicate still detected */
    CHECK(fs_free_blocks() == 4096);
    CHECK(umount_fs(DISK) == 0);
    remove(DISK);
    return check_report("week1_persist(read)");
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "write") == 0) return do_write();
    if (argc == 2 && strcmp(argv[1], "read") == 0)  return do_read();
    fprintf(stderr, "usage: %s write|read\n", argv[0]);
    return 2;
}
