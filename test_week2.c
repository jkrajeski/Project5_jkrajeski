//Week 2 tests: fs_open, fs_close, fs_read, fs_write, fs_get_filesize, fs_lseek
//Sections follow the README's list of what Week 2 checks.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"
#include "disk.h"
#include "check.h"

#define DISK "t2.disk"

static unsigned char wbuf[30000], rbuf[30000];

//fill b with n bytes of the known pattern, starting at stream position start
static void fill(unsigned char *b, long start, long n)
{
    long i;
    for (i = 0; i < n; i++) b[i] = pattern(start + i);
}

//do the n bytes in b match the known pattern starting at stream position start?
static int matches(const unsigned char *b, long start, long n)
{
    long i;
    for (i = 0; i < n; i++)
        if (b[i] != pattern(start + i)) return 0;
    return 1;
}

//blocks needed for a file of this many bytes
static int blocks_for(long bytes)
{
    return (int)((bytes + BLOCK_SIZE - 1) / BLOCK_SIZE);
}

//invalid descriptors, missing files, closed descriptors
static void test_bad_descriptors(void)
{
    int fd;

    CHECK(fs_open("missing") == -1);
    CHECK(fs_close(0) == -1);                   //nothing open yet
    CHECK(fs_close(-1) == -1);
    CHECK(fs_close(FS_MAX_FD) == -1);
    CHECK(fs_close(1000) == -1);
    CHECK(fs_read(0, rbuf, 10) == -1);
    CHECK(fs_write(0, wbuf, 10) == -1);
    CHECK(fs_get_filesize(0) == -1);
    CHECK(fs_lseek(0, 0) == -1);

    CHECK(fs_create("e") == 0);
    fd = fs_open("e");
    CHECK(fd >= 0);
    CHECK(fs_close(fd) == 0);
    CHECK(fs_close(fd) == -1);                  //double close
    CHECK(fs_read(fd, rbuf, 10) == -1);         //closed descriptor is unusable
    CHECK(fs_write(fd, wbuf, 10) == -1);
    CHECK(fs_get_filesize(fd) == -1);
    CHECK(fs_lseek(fd, 0) == -1);
    CHECK(fs_delete("e") == 0);                 //not open anymore
}

//reading and writing data that spans several blocks, mid-block starts,
//and reads that cross block boundaries
static void test_span_blocks(void)
{
    int fd;

    CHECK(fs_create("data") == 0);
    fd = fs_open("data");
    CHECK(fd >= 0);
    CHECK(fs_get_filesize(fd) == 0);
    CHECK(fs_read(fd, rbuf, 100) == 0);         //empty file: 0 bytes read

    fill(wbuf, 0, 25000);
    CHECK(fs_write(fd, wbuf, 25000) == 25000);  //7 blocks
    CHECK(fs_get_filesize(fd) == 25000);
    CHECK(fs_free_blocks() == 4096 - 7);

    //whole file back
    CHECK(fs_lseek(fd, 0) == 0);
    memset(rbuf, 0, sizeof(rbuf));
    CHECK(fs_read(fd, rbuf, 25000) == 25000);
    CHECK(matches(rbuf, 0, 25000));
    CHECK(fs_read(fd, rbuf, 10) == 0);          //offset is at the end now

    //reads starting mid-block and crossing boundaries
    CHECK(fs_lseek(fd, 4090) == 0);
    CHECK(fs_read(fd, rbuf, 20) == 20);         //block 0 -> 1
    CHECK(matches(rbuf, 4090, 20));
    CHECK(fs_lseek(fd, 100) == 0);
    CHECK(fs_read(fd, rbuf, 9000) == 9000);     //starts mid-block 0, ends in block 2
    CHECK(matches(rbuf, 100, 9000));
    CHECK(fs_lseek(fd, 4096) == 0);
    CHECK(fs_read(fd, rbuf, 4096) == 4096);     //exactly one aligned block
    CHECK(matches(rbuf, 4096, 4096));
    CHECK(fs_lseek(fd, 8191) == 0);
    CHECK(fs_read(fd, rbuf, 2) == 2);           //last byte of a block + first of next
    CHECK(matches(rbuf, 8191, 2));
    CHECK(fs_lseek(fd, 24990) == 0);
    CHECK(fs_read(fd, rbuf, 100) == 10);        //clamped at end of file
    CHECK(matches(rbuf, 24990, 10));
    CHECK(fs_read(fd, rbuf, 100) == 0);

    CHECK(fs_close(fd) == 0);
    CHECK(fs_delete("data") == 0);
    CHECK(fs_free_blocks() == 4096);
}

//offset moves forward after every read and write
static void test_offset_advances(void)
{
    int fd;

    CHECK(fs_create("seq") == 0);
    fd = fs_open("seq");
    CHECK(fs_write(fd, "abc", 3) == 3);
    CHECK(fs_write(fd, "def", 3) == 3);         //continues after "abc"
    CHECK(fs_get_filesize(fd) == 6);
    CHECK(fs_lseek(fd, 0) == 0);
    CHECK(fs_read(fd, rbuf, 2) == 2);
    CHECK(memcmp(rbuf, "ab", 2) == 0);
    CHECK(fs_read(fd, rbuf, 2) == 2);           //continues after "ab"
    CHECK(memcmp(rbuf, "cd", 2) == 0);
    CHECK(fs_read(fd, rbuf, 10) == 2);          //only "ef" left
    CHECK(memcmp(rbuf, "ef", 2) == 0);
    CHECK(fs_close(fd) == 0);
    CHECK(fs_delete("seq") == 0);
}

//overwriting the middle of a file does not change its size
static void test_overwrite_middle(void)
{
    int fd;

    CHECK(fs_create("ow") == 0);
    fd = fs_open("ow");
    fill(wbuf, 0, 20000);
    CHECK(fs_write(fd, wbuf, 20000) == 20000);

    memset(wbuf, 0xAB, 6000);
    CHECK(fs_lseek(fd, 4000) == 0);
    CHECK(fs_write(fd, wbuf, 6000) == 6000);    //bytes 4000..9999
    CHECK(fs_get_filesize(fd) == 20000);        //size unchanged
    CHECK(fs_free_blocks() == 4096 - 5);        //no new blocks

    CHECK(fs_lseek(fd, 3990) == 0);
    CHECK(fs_read(fd, rbuf, 6020) == 6020);
    CHECK(matches(rbuf, 3990, 10));             //before the overwrite
    CHECK(rbuf[10] == 0xAB && rbuf[6009] == 0xAB);
    CHECK(matches(rbuf + 6010, 10000, 10));     //after the overwrite
    CHECK(fs_lseek(fd, 0) == 0);
    CHECK(fs_read(fd, rbuf, 4000) == 4000);
    CHECK(matches(rbuf, 0, 4000));              //start untouched
    CHECK(fs_lseek(fd, 10000) == 0);
    CHECK(fs_read(fd, rbuf, 10000) == 10000);
    CHECK(matches(rbuf, 10000, 10000));         //end untouched

    CHECK(fs_close(fd) == 0);
    CHECK(fs_delete("ow") == 0);
}

//appending with fs_lseek(fd, size)
static void test_append(void)
{
    int fd;

    CHECK(fs_create("app") == 0);
    fd = fs_open("app");
    fill(wbuf, 0, 3000);
    CHECK(fs_write(fd, wbuf, 3000) == 3000);
    CHECK(fs_lseek(fd, fs_get_filesize(fd)) == 0);
    fill(wbuf, 3000, 5000);
    CHECK(fs_write(fd, wbuf, 5000) == 5000);    //crosses into block 1
    CHECK(fs_get_filesize(fd) == 8000);
    CHECK(fs_free_blocks() == 4096 - 2);
    CHECK(fs_lseek(fd, 0) == 0);
    CHECK(fs_read(fd, rbuf, 8000) == 8000);
    CHECK(matches(rbuf, 0, 8000));

    //a file of exactly one block, then one more byte
    CHECK(fs_lseek(fd, 0) == 0);
    CHECK(fs_close(fd) == 0);
    CHECK(fs_delete("app") == 0);
    CHECK(fs_create("one") == 0);
    fd = fs_open("one");
    fill(wbuf, 0, 4096);
    CHECK(fs_write(fd, wbuf, 4096) == 4096);
    CHECK(fs_free_blocks() == 4096 - 1);
    CHECK(fs_write(fd, wbuf, 1) == 1);          //needs a second block
    CHECK(fs_free_blocks() == 4096 - 2);
    CHECK(fs_get_filesize(fd) == 4097);
    CHECK(fs_write(fd, wbuf, 0) == 0);          //zero-byte write
    CHECK(fs_get_filesize(fd) == 4097);
    CHECK(fs_close(fd) == 0);
    CHECK(fs_delete("one") == 0);
}

//fs_lseek limits
static void test_lseek_limits(void)
{
    int fd;

    CHECK(fs_create("sk") == 0);
    fd = fs_open("sk");
    fill(wbuf, 0, 100);
    CHECK(fs_write(fd, wbuf, 100) == 100);
    CHECK(fs_lseek(fd, 0) == 0);
    CHECK(fs_lseek(fd, 100) == 0);              //equal to size is allowed
    CHECK(fs_lseek(fd, 101) == -1);             //past the end
    CHECK(fs_lseek(fd, -1) == -1);
    CHECK(fs_lseek(fd, 50) == 0);               //still usable after failures
    CHECK(fs_read(fd, rbuf, 100) == 50);
    CHECK(matches(rbuf, 50, 50));
    CHECK(fs_close(fd) == 0);
    CHECK(fs_delete("sk") == 0);
}

//two descriptors on the same file: independent offsets, shared data and size
static void test_two_descriptors(void)
{
    int fd, fd2;

    CHECK(fs_create("two") == 0);
    fd = fs_open("two");
    fd2 = fs_open("two");
    CHECK(fd >= 0 && fd2 >= 0 && fd != fd2);

    fill(wbuf, 0, 25000);
    CHECK(fs_write(fd, wbuf, 25000) == 25000);
    CHECK(fs_get_filesize(fd2) == 25000);       //size visible through fd2
    CHECK(fs_read(fd2, rbuf, 10) == 10);        //fd2 offset is still 0
    CHECK(matches(rbuf, 0, 10));

    CHECK(fs_lseek(fd, 0) == 0);
    CHECK(fs_lseek(fd2, 20000) == 0);
    CHECK(fs_read(fd, rbuf, 10) == 10);
    CHECK(fs_read(fd2, rbuf + 10, 10) == 10);
    CHECK(matches(rbuf, 0, 10));                //independent offsets
    CHECK(matches(rbuf + 10, 20000, 10));
    CHECK(fs_read(fd, rbuf, 10) == 10);         //fd moved on by itself
    CHECK(matches(rbuf, 10, 10));

    //write through fd2 is read back through fd
    memset(wbuf, 0xCD, 50);
    CHECK(fs_lseek(fd2, 500) == 0);
    CHECK(fs_write(fd2, wbuf, 50) == 50);
    CHECK(fs_lseek(fd, 500) == 0);
    CHECK(fs_read(fd, rbuf, 50) == 50);
    CHECK(rbuf[0] == 0xCD && rbuf[49] == 0xCD);

    //extending through one descriptor is seen by the other immediately
    CHECK(fs_lseek(fd, 25000) == 0);
    CHECK(fs_write(fd, wbuf, 500) == 500);
    CHECK(fs_get_filesize(fd2) == 25500);
    CHECK(fs_lseek(fd2, 25400) == 0);           //valid only because size grew
    CHECK(fs_read(fd2, rbuf, 200) == 100);

    CHECK(fs_close(fd) == 0);
    CHECK(fs_get_filesize(fd2) == 25500);       //fd2 unaffected by fd closing
    CHECK(fs_close(fd2) == 0);
    CHECK(fs_delete("two") == 0);
}

//the 32-descriptor limit and descriptor reuse
static void test_descriptor_limit(void)
{
    int fds[FS_MAX_FD], i, again;

    CHECK(fs_create("many") == 0);
    for (i = 0; i < FS_MAX_FD; i++) {
        fds[i] = fs_open("many");
        CHECK(fds[i] >= 0 && fds[i] < FS_MAX_FD);
    }
    CHECK(fs_open("many") == -1);               //33rd open fails
    CHECK(fs_create("other") == 0);
    CHECK(fs_open("other") == -1);              //limit is across all files
    CHECK(fs_close(fds[5]) == 0);
    again = fs_open("many");
    CHECK(again == fds[5]);                     //freed slot is reused
    CHECK(fs_open("many") == -1);
    for (i = 0; i < FS_MAX_FD; i++) fs_close(fds[i]);
    CHECK(fs_delete("many") == 0);
    CHECK(fs_delete("other") == 0);
}

//fs_delete must fail while any descriptor is open on the file
static void test_delete_open(void)
{
    int fd, fd2;

    CHECK(fs_create("busy") == 0);
    fd = fs_open("busy");
    fd2 = fs_open("busy");
    fill(wbuf, 0, 5000);
    CHECK(fs_write(fd, wbuf, 5000) == 5000);

    CHECK(fs_delete("busy") == -1);
    CHECK(fs_close(fd) == 0);
    CHECK(fs_delete("busy") == -1);             //fd2 still open
    CHECK(fs_free_blocks() == 4096 - 2);        //failed delete freed nothing
    CHECK(fs_lseek(fd2, 0) == 0);
    CHECK(fs_read(fd2, rbuf, 5000) == 5000);    //data still intact
    CHECK(matches(rbuf, 0, 5000));
    CHECK(fs_close(fd2) == 0);
    CHECK(fs_delete("busy") == 0);
    CHECK(fs_free_blocks() == 4096);
    CHECK(fs_open("busy") == -1);               //really gone
}

//free-block count as files grow and are deleted
static void test_free_count(void)
{
    int f1, f2, f3, i, fd;
    long total = 0;

    CHECK(fs_create("f1") == 0);
    CHECK(fs_create("f2") == 0);
    CHECK(fs_create("f3") == 0);
    f1 = fs_open("f1");
    f2 = fs_open("f2");
    f3 = fs_open("f3");
    CHECK(fs_write(f1, wbuf, 3 * 4096) == 3 * 4096);   //3 blocks
    CHECK(fs_write(f2, wbuf, 100) == 100);             //1 block
    CHECK(fs_write(f3, wbuf, 4097) == 4097);           //2 blocks
    CHECK(fs_free_blocks() == 4096 - 6);
    CHECK(fs_close(f1) == 0);
    CHECK(fs_close(f2) == 0);
    CHECK(fs_close(f3) == 0);
    CHECK(fs_delete("f2") == 0);
    CHECK(fs_free_blocks() == 4096 - 5);
    CHECK(fs_delete("f1") == 0);
    CHECK(fs_free_blocks() == 4096 - 2);
    CHECK(fs_delete("f3") == 0);
    CHECK(fs_free_blocks() == 4096);

    //growing one file in small steps: count follows ceil(bytes / 4096)
    CHECK(fs_create("grow") == 0);
    fd = fs_open("grow");
    for (i = 0; i < 25; i++) {
        CHECK(fs_write(fd, wbuf, 1000) == 1000);
        total += 1000;
        CHECK(fs_free_blocks() == 4096 - blocks_for(total));
    }
    CHECK(fs_get_filesize(fd) == 25000);
    CHECK(fs_close(fd) == 0);
    CHECK(fs_delete("grow") == 0);
    CHECK(fs_free_blocks() == 4096);

    //freed blocks can be reused by a new file
    CHECK(fs_create("again") == 0);
    fd = fs_open("again");
    CHECK(fs_write(fd, wbuf, 30000) == 30000);
    CHECK(fs_free_blocks() == 4096 - 8);
    CHECK(fs_close(fd) == 0);
    CHECK(fs_delete("again") == 0);
    CHECK(fs_free_blocks() == 4096);
}

//two files written in alternating chunks must not mix their data
static void test_files_do_not_mix(void)
{
    static unsigned char a[20000], b[20000], ra[20000], rb[20000];
    int fa, fb, i;
    long k;

    for (k = 0; k < 20000; k++) {
        a[k] = pattern(k);
        b[k] = pattern(k + 500);
    }
    CHECK(fs_create("A") == 0);
    CHECK(fs_create("B") == 0);
    fa = fs_open("A");
    fb = fs_open("B");
    for (i = 0; i < 4; i++) {                   //interleaves blocks on disk
        CHECK(fs_write(fa, a + i * 5000, 5000) == 5000);
        CHECK(fs_write(fb, b + i * 5000, 5000) == 5000);
    }
    CHECK(fs_lseek(fa, 0) == 0);
    CHECK(fs_lseek(fb, 0) == 0);
    CHECK(fs_read(fa, ra, 20000) == 20000);
    CHECK(fs_read(fb, rb, 20000) == 20000);
    CHECK(memcmp(a, ra, 20000) == 0);
    CHECK(memcmp(b, rb, 20000) == 0);
    CHECK(fs_close(fa) == 0);
    CHECK(fs_close(fb) == 0);
    CHECK(fs_delete("A") == 0);
    CHECK(fs_delete("B") == 0);
    CHECK(fs_free_blocks() == 4096);
}

//unmount closes descriptors; data is still there after a remount
static void test_unmount_remount(void)
{
    int fd, i;

    CHECK(fs_create("keep") == 0);
    fd = fs_open("keep");
    fill(wbuf, 0, 9000);
    CHECK(fs_write(fd, wbuf, 9000) == 9000);
    for (i = 1; i < FS_MAX_FD; i++) fs_open("keep");   //leave 32 open
    CHECK(umount_fs(DISK) == 0);
    CHECK(fs_read(fd, rbuf, 10) == -1);         //nothing mounted
    CHECK(mount_fs(DISK) == 0);
    CHECK(fs_read(fd, rbuf, 10) == -1);         //old descriptor is not valid
    fd = fs_open("keep");
    CHECK(fd == 0);                             //table was cleared
    CHECK(fs_get_filesize(fd) == 9000);
    CHECK(fs_read(fd, rbuf, 9000) == 9000);
    CHECK(matches(rbuf, 0, 9000));
    CHECK(fs_close(fd) == 0);
    CHECK(fs_delete("keep") == 0);              //no stale descriptors left
}

int main(void)
{
    remove(DISK);
    CHECK(make_fs(DISK) == 0);
    CHECK(mount_fs(DISK) == 0);

    test_bad_descriptors();
    test_span_blocks();
    test_offset_advances();
    test_overwrite_middle();
    test_append();
    test_lseek_limits();
    test_two_descriptors();
    test_descriptor_limit();
    test_delete_open();
    test_free_count();
    test_files_do_not_mix();
    test_unmount_remount();

    CHECK(fs_free_blocks() == 4096);
    CHECK(umount_fs(DISK) == 0);
    remove(DISK);
    return check_report("week2");
}
