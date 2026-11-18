//Persistence test: file data must survive unmounts and be seen by a
//separate program. Run as three SEPARATE processes, in this order:
//    ./test_persist write     create files, write data, unmount
//    ./test_persist append    new process: add data to a file, unmount
//    ./test_persist read      new process: verify everything, then delete
#include <stdio.h>
#include <string.h>
#include "fs.h"
#include "disk.h"
#include "check.h"

#define DISK "persist.disk"

static unsigned char buf[40000];

//do the n bytes at b match the known pattern starting at stream position start?
static int matches(const unsigned char *b, long start, long n)
{
    long i;
    for (i = 0; i < n; i++)
        if (b[i] != pattern(start + i)) return 0;
    return 1;
}

static int do_write(void)
{
    long i;
    int fd;

    remove(DISK);
    CHECK(make_fs(DISK) == 0);
    CHECK(mount_fs(DISK) == 0);
    CHECK(fs_create("alpha") == 0);
    CHECK(fs_create("beta") == 0);
    CHECK(fs_create("gamma") == 0);
    CHECK(fs_create("empty") == 0);

    for (i = 0; i < 30000; i++) buf[i] = pattern(i);
    fd = fs_open("alpha");
    CHECK(fs_write(fd, buf, 30000) == 30000);           //8 blocks
    CHECK(fs_close(fd) == 0);

    for (i = 0; i < 5000; i++) buf[i] = pattern(i + 1000);
    fd = fs_open("beta");
    CHECK(fs_write(fd, buf, 5000) == 5000);             //2 blocks
    memset(buf, 0xEE, 200);
    CHECK(fs_lseek(fd, 100) == 0);
    CHECK(fs_write(fd, buf, 200) == 200);               //overwrite bytes 100..299
    CHECK(fs_close(fd) == 0);

    for (i = 0; i < 4096; i++) buf[i] = pattern(i + 2000);
    fd = fs_open("gamma");
    CHECK(fs_write(fd, buf, 4096) == 4096);             //exactly 1 block
    CHECK(fs_close(fd) == 0);

    //"empty" is created but never written
    CHECK(umount_fs(DISK) == 0);
    return check_report("persist(write)");
}

static int do_append(void)
{
    long i;
    int fd;

    CHECK(mount_fs(DISK) == 0);
    fd = fs_open("alpha");
    CHECK(fd >= 0);
    CHECK(fs_get_filesize(fd) == 30000);                //seen from the first run
    CHECK(fs_lseek(fd, 30000) == 0);
    for (i = 0; i < 7000; i++) buf[i] = pattern(30000 + i);
    CHECK(fs_write(fd, buf, 7000) == 7000);             //alpha is now 37000 = 10 blocks
    CHECK(fs_close(fd) == 0);
    CHECK(umount_fs(DISK) == 0);
    return check_report("persist(append)");
}

static int do_read(void)
{
    char names[10][FS_MAX_NAME + 1];
    long i;
    int fd, ok;

    CHECK(mount_fs(DISK) == 0);
    CHECK(fs_listfiles(names, 10) == 4);
    CHECK(fs_free_blocks() == 4096 - 10 - 2 - 1);       //alpha + beta + gamma

    //alpha: data from both sessions, continuous
    fd = fs_open("alpha");
    CHECK(fd >= 0);
    CHECK(fs_get_filesize(fd) == 37000);
    memset(buf, 0, sizeof(buf));
    CHECK(fs_read(fd, buf, 37000) == 37000);
    CHECK(matches(buf, 0, 37000));
    CHECK(fs_close(fd) == 0);

    //beta: original data with the overwritten middle
    fd = fs_open("beta");
    CHECK(fs_get_filesize(fd) == 5000);
    CHECK(fs_read(fd, buf, 5000) == 5000);
    CHECK(matches(buf, 1000, 100));                     //before the overwrite
    for (ok = 1, i = 100; i < 300; i++)
        if (buf[i] != 0xEE) ok = 0;
    CHECK(ok);                                          //the overwritten bytes
    CHECK(matches(buf + 300, 1300, 4700));              //after the overwrite
    CHECK(fs_close(fd) == 0);

    //gamma: exactly one block
    fd = fs_open("gamma");
    CHECK(fs_get_filesize(fd) == 4096);
    CHECK(fs_read(fd, buf, 4096) == 4096);
    CHECK(matches(buf, 2000, 4096));
    CHECK(fs_close(fd) == 0);

    //empty: still there, still empty
    fd = fs_open("empty");
    CHECK(fd >= 0);
    CHECK(fs_get_filesize(fd) == 0);
    CHECK(fs_read(fd, buf, 10) == 0);
    CHECK(fs_close(fd) == 0);

    //deleting after a remount returns the right number of blocks
    CHECK(fs_delete("alpha") == 0);
    CHECK(fs_free_blocks() == 4096 - 2 - 1);
    CHECK(fs_delete("beta") == 0);
    CHECK(fs_delete("gamma") == 0);
    CHECK(fs_delete("empty") == 0);
    CHECK(fs_free_blocks() == 4096);

    CHECK(umount_fs(DISK) == 0);
    remove(DISK);
    return check_report("persist(read)");
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "write") == 0)  return do_write();
    if (argc == 2 && strcmp(argv[1], "append") == 0) return do_append();
    if (argc == 2 && strcmp(argv[1], "read") == 0)   return do_read();
    fprintf(stderr, "usage: %s write|append|read\n", argv[0]);
    return 2;
}
