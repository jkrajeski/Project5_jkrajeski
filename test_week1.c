/* Week 1: make/mount/umount, create/delete/list, limits, free-block count. */
#include <stdio.h>
#include <string.h>
#include "../fs.h"
#include "../disk.h"
#include "check.h"

#define DISK "t1.disk"

int main(void)
{
    char names[100][FS_MAX_NAME + 1];
    char name[32];
    int i, n;

    remove(DISK);

    /* Nothing works before a file system is mounted. */
    CHECK(fs_create("a") == -1);
    CHECK(fs_delete("a") == -1);
    CHECK(fs_open("a") == -1);
    CHECK(fs_free_blocks() == -1);
    CHECK(fs_listfiles(names, 10) == -1);
    CHECK(umount_fs(DISK) == -1);

    /* A disk made with make_disk alone must be rejected. */
    CHECK(make_disk(DISK) == 0);
    CHECK(mount_fs(DISK) == -1);
    CHECK(mount_fs("does_not_exist.disk") == -1);

    /* make_fs, then mount. */
    CHECK(make_fs(DISK) == 0);
    CHECK(mount_fs(DISK) == 0);
    CHECK(mount_fs(DISK) == -1);               /* already mounted */
    CHECK(fs_free_blocks() == 4096);
    CHECK(fs_listfiles(names, 100) == 0);      /* empty directory */

    /* Create / duplicates / bad names. */
    CHECK(fs_create("hello") == 0);
    CHECK(fs_create("hello") == -1);           /* duplicate */
    CHECK(fs_create("") == -1);                /* empty */
    CHECK(fs_create("123456789012345") == 0);  /* exactly 15 chars: ok */
    CHECK(fs_create("1234567890123456") == -1);/* 16 chars: too long */
    CHECK(fs_create(NULL) == -1);

    n = fs_listfiles(names, 100);
    CHECK(n == 2);
    CHECK(fs_listfiles(names, 1) == 1);

    /* Delete. */
    CHECK(fs_delete("nope") == -1);
    CHECK(fs_delete("hello") == 0);
    CHECK(fs_delete("hello") == -1);
    CHECK(fs_listfiles(names, 100) == 1);
    CHECK(fs_delete("123456789012345") == 0);

    /* 64-file limit; deleted files don't count. */
    for (i = 0; i < FS_MAX_FILES; i++) {
        snprintf(name, sizeof(name), "f%d", i);
        CHECK(fs_create(name) == 0);
    }
    CHECK(fs_create("one_too_many") == -1);
    CHECK(fs_listfiles(names, 100) == 64);
    CHECK(fs_delete("f10") == 0);
    CHECK(fs_create("one_too_many") == 0);
    CHECK(fs_listfiles(names, 100) == 64);

    /* Unmount rules. */
    CHECK(umount_fs("wrong.disk") == -1);
    CHECK(umount_fs(DISK) == 0);
    CHECK(umount_fs(DISK) == -1);              /* nothing mounted now */

    /* Remount in the same process: directory must have been saved. */
    CHECK(mount_fs(DISK) == 0);
    CHECK(fs_listfiles(names, 100) == 64);
    CHECK(fs_free_blocks() == 4096);
    CHECK(umount_fs(DISK) == 0);

    remove(DISK);
    return check_report("week1");
}
