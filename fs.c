/*
 * Volume layout (4 KB blocks):
 *   block 0           super block
 *   blocks 1-4        FAT   (4096 entries x 4 bytes = 16 KB = 4 blocks)
 *   block 5           root directory (64 entries x 32 bytes = 2 KB)
 *   blocks 6-4095     unused
 *   blocks 4096-8191  data region (4096 blocks)
 *
 * FAT entries are indexed relative to the data region: FAT index i describes
 * disk block DATA_START + i.  An entry holds FAT_FREE, FAT_EOF, or the
 * (relative) index of the next block of the same file.
 */
#include <stdio.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

//layout constants
#define FS_MAGIC      0x46415331    //nonzero on purpose: make_disk zero-fills, so 0 would look valid
#define SB_BLOCK      0
#define FAT_START     1
#define FAT_BLOCKS    4
#define DIR_START     5
#define DATA_START    4096
#define DATA_BLOCKS   4096
#define FAT_PER_BLOCK (BLOCK_SIZE / (int)sizeof(int))   //1024 entries

#define FAT_FREE      (-2)   //block is unused
#define FAT_EOF       (-1)   //last block of a file
                             //0..4095 = index of the next block

//on-disk types
typedef struct {
    int magic;
    int fat_start;
    int dir_start;
    int data_start;
} superblock_t;

typedef struct {                       //32 bytes on disk
    char name[FS_MAX_NAME + 1];        //16
    int  size;                         //4
    int  first_block;                  //4  FAT index; -1 = no blocks yet
    int  in_use;                       //4
    char pad[4];                       //4
} dir_entry_t;

//in-memory state
typedef struct {                       //file descriptor slot (memory only)
    int   in_use;
    int   file_idx;                    //index into dir[]
    off_t offset;                      //never stored on disk
} fd_t;

static int          mounted = 0;
static char         mounted_name[256];
static superblock_t sb;
static int          fat[DATA_BLOCKS];
static dir_entry_t  dir[FS_MAX_FILES];
static fd_t         fdt[FS_MAX_FD];    //populated starting in week 2

//helpers

//Write super block, FAT and directory to the (already open) disk
static int write_meta(const superblock_t *s, const int *f, const dir_entry_t *d)
{
    char buf[BLOCK_SIZE];
    int i;

    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf, s, sizeof(*s));
    if (block_write(SB_BLOCK, buf) < 0) return -1;

    for (i = 0; i < FAT_BLOCKS; i++) {
        memcpy(buf, f + i * FAT_PER_BLOCK, BLOCK_SIZE);
        if (block_write(FAT_START + i, buf) < 0) return -1;
    }

    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf, d, sizeof(dir_entry_t) * FS_MAX_FILES);
    if (block_write(DIR_START, buf) < 0) return -1;

    return 0;
}

//Read and validate meta-information from the (already open) disk
static int read_meta(void)
{
    char buf[BLOCK_SIZE];
    int i;

    memset(buf, 0, BLOCK_SIZE);
    if (block_read(SB_BLOCK, buf) < 0) return -1;
    memcpy(&sb, buf, sizeof(sb));
    if (sb.magic != FS_MAGIC || sb.fat_start != FAT_START ||
        sb.dir_start != DIR_START || sb.data_start != DATA_START)
        return -1;

    for (i = 0; i < FAT_BLOCKS; i++) {
        if (block_read(FAT_START + i, buf) < 0) return -1;
        memcpy(fat + i * FAT_PER_BLOCK, buf, BLOCK_SIZE);
    }

    if (block_read(DIR_START, buf) < 0) return -1;
    memcpy(dir, buf, sizeof(dir_entry_t) * FS_MAX_FILES);
    return 0;
}

//Name must be non-NULL, 1..15 characters
static int valid_name(const char *name)
{
    size_t n;
    if (!name) return 0;
    n = strlen(name);
    return n >= 1 && n <= FS_MAX_NAME;
}

//Is fd an open descriptor on a mounted file system?
static int valid_fd(int fd)
{
    return mounted && fd >= 0 && fd < FS_MAX_FD && fdt[fd].in_use;
}

//Directory index of file `name`, or -1
static int find_file(const char *name)
{
    int i;
    for (i = 0; i < FS_MAX_FILES; i++)
        if (dir[i].in_use && strcmp(dir[i].name, name) == 0)
            return i;
    return -1;
}

static int find_free_dir_slot(void)
{
    int i;
    for (i = 0; i < FS_MAX_FILES; i++)
        if (!dir[i].in_use) return i;
    return -1;
}

//Take the first free data block, mark it EOF, return its index or -1
static int alloc_block(void)
{
    int i;
    for (i = 0; i < DATA_BLOCKS; i++) {
        if (fat[i] == FAT_FREE) {
            fat[i] = FAT_EOF;
            return i;
        }
    }
    return -1;
}

//Block index of the n-th block (0-based) in a chain, or -1
static int nth_block(int first, long n)
{
    int b = first;
    while (n-- > 0 && b >= 0 && b < DATA_BLOCKS)
        b = fat[b];
    return (b >= 0 && b < DATA_BLOCKS) ? b : -1;
}

//Free every block in the chain starting at `first` (-1 = empty chain)
static void free_chain(int first)
{
    int b = first, guard = 0;
    while (b >= 0 && b < DATA_BLOCKS && guard++ < DATA_BLOCKS) {
        int next = fat[b];
        fat[b] = FAT_FREE;
        b = next;           //FAT_EOF / FAT_FREE are negative: loop ends
    }
}

//Is any open descriptor referring to directory entry "file_idx"?
static int file_is_open(int file_idx)
{
    int i;
    for (i = 0; i < FS_MAX_FD; i++)
        if (fdt[i].in_use && fdt[i].file_idx == file_idx) return 1;
    return 0;
}

//File system management

int make_fs(char *disk_name)
{
    superblock_t s;
    static int f[DATA_BLOCKS];
    static dir_entry_t d[FS_MAX_FILES];
    int i, rc;

    if (!disk_name || mounted) return -1;
    if (make_disk(disk_name) < 0) return -1;
    if (open_disk(disk_name) < 0) return -1;

    s.magic      = FS_MAGIC;
    s.fat_start  = FAT_START;
    s.dir_start  = DIR_START;
    s.data_start = DATA_START;
    for (i = 0; i < DATA_BLOCKS; i++) f[i] = FAT_FREE;   //every block free
    memset(d, 0, sizeof(d));                             //empty directory

    rc = write_meta(&s, f, d);
    if (close_disk() < 0) rc = -1;       //make_fs leaves nothing mounted
    return rc;
}

int mount_fs(char *disk_name)
{
    int i;

    if (!disk_name || mounted) return -1;
    if (open_disk(disk_name) < 0) return -1;

    if (read_meta() < 0) {               //bad magic / unreadable
        close_disk();
        return -1;
    }

    for (i = 0; i < FS_MAX_FD; i++) fdt[i].in_use = 0;
    strncpy(mounted_name, disk_name, sizeof(mounted_name) - 1);
    mounted_name[sizeof(mounted_name) - 1] = '\0';
    mounted = 1;
    return 0;
}

int umount_fs(char *disk_name)
{
    int i;

    if (!mounted || !disk_name || strcmp(disk_name, mounted_name) != 0)
        return -1;
    if (write_meta(&sb, fat, dir) < 0) return -1;   //flush FAT + directory

    for (i = 0; i < FS_MAX_FD; i++) fdt[i].in_use = 0;
    if (close_disk() < 0) return -1;
    mounted = 0;
    return 0;
}

//Directory-level functions

int fs_create(char *name)
{
    int slot;

    //validate everything first so a failure changes nothing
    if (!mounted || !valid_name(name)) return -1;
    if (find_file(name) >= 0) return -1;
    if ((slot = find_free_dir_slot()) < 0) return -1;

    memset(&dir[slot], 0, sizeof(dir[slot]));
    strncpy(dir[slot].name, name, FS_MAX_NAME);
    dir[slot].size = 0;
    dir[slot].first_block = -1;
    dir[slot].in_use = 1;
    return 0;
}

int fs_delete(char *name)
{
    int idx;

    if (!mounted || !valid_name(name)) return -1;
    if ((idx = find_file(name)) < 0) return -1;
    if (file_is_open(idx)) return -1;

    free_chain(dir[idx].first_block);        //blocks back to the free pool
    memset(&dir[idx], 0, sizeof(dir[idx]));  //in_use = 0
    return 0;
}

int fs_listfiles(char names[][FS_MAX_NAME + 1], int max_names)
{
    int i, n = 0;

    if (!mounted) return -1;
    if (max_names <= 0) return 0;
    if (!names) return -1;

    for (i = 0; i < FS_MAX_FILES && n < max_names; i++) {
        if (dir[i].in_use) {
            strncpy(names[n], dir[i].name, FS_MAX_NAME);
            names[n][FS_MAX_NAME] = '\0';
            n++;
        }
    }
    return n;
}

int fs_free_blocks(void)
{
    int i, n = 0;

    if (!mounted) return -1;
    for (i = 0; i < DATA_BLOCKS; i++)
        if (fat[i] == FAT_FREE) n++;
    return n;
}

//File descriptor functions

int fs_open(char *name)
{
    int idx, fd;

    if (!mounted || !valid_name(name)) return -1;
    if ((idx = find_file(name)) < 0) return -1;

    for (fd = 0; fd < FS_MAX_FD; fd++) {
        if (!fdt[fd].in_use) {
            fdt[fd].in_use = 1;
            fdt[fd].file_idx = idx;
            fdt[fd].offset = 0;
            return fd;
        }
    }
    return -1;          //all 32 descriptors in use
}

int fs_close(int fildes)
{
    if (!valid_fd(fildes)) return -1;
    fdt[fildes].in_use = 0;
    return 0;
}

int fs_get_filesize(int fildes)
{
    if (!valid_fd(fildes)) return -1;
    return dir[fdt[fildes].file_idx].size;
}

int fs_lseek(int fildes, off_t offset)
{
    if (!valid_fd(fildes)) return -1;
    if (offset < 0 || offset > dir[fdt[fildes].file_idx].size) return -1;
    fdt[fildes].offset = offset;
    return 0;
}

//Read and write

int fs_read(int fildes, void *buf, size_t nbyte)
{
    fd_t *f;
    dir_entry_t *e;
    char block[BLOCK_SIZE];
    size_t done = 0, avail;
    long off;
    int blk;

    if (!valid_fd(fildes) || (!buf && nbyte > 0)) return -1;
    f = &fdt[fildes];
    e = &dir[f->file_idx];
    off = (long)f->offset;

    if (off >= e->size || nbyte == 0) return 0;     //at or after end of file
    avail = (size_t)(e->size - off);
    if (nbyte > avail) nbyte = avail;

    blk = nth_block(e->first_block, off / BLOCK_SIZE);
    while (done < nbyte && blk >= 0) {
        size_t boff  = (size_t)((off + (long)done) % BLOCK_SIZE);
        size_t chunk = BLOCK_SIZE - boff;
        if (chunk > nbyte - done) chunk = nbyte - done;

        if (block_read(DATA_START + blk, block) < 0) break;
        memcpy((char *)buf + done, block + boff, chunk);
        done += chunk;
        blk = fat[blk];
    }

    if (done == 0) return -1;       //only reachable on an I/O or chain error
    f->offset += (off_t)done;
    return (int)done;
}

int fs_write(int fildes, void *buf, size_t nbyte)
{
    fd_t *f;
    dir_entry_t *e;
    char block[BLOCK_SIZE];
    long off, size, room, cur_blocks, need_blocks, added = 0, limit;
    int tail = -1, blk;
    size_t done = 0;

    if (!valid_fd(fildes) || (!buf && nbyte > 0)) return -1;
    if (nbyte == 0) return 0;

    f = &fdt[fildes];
    e = &dir[f->file_idx];
    off = (long)f->offset;
    size = e->size;

    //respect the 16 MB maximum file size
    room = (long)DATA_BLOCKS * BLOCK_SIZE - off;
    if (room <= 0) return 0;
    if ((long)nbyte > room) nbyte = (size_t)room;

    //allocate any extra blocks the write will need
    cur_blocks  = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    need_blocks = (off + (long)nbyte + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (cur_blocks > 0) {
        tail = nth_block(e->first_block, cur_blocks - 1);
        if (tail < 0) return -1;
    }
    while (cur_blocks + added < need_blocks) {
        int nb = alloc_block();
        if (nb < 0) break;                       //disk is full
        if (tail < 0) e->first_block = nb;       //first block of the file
        else          fat[tail] = nb;            //link onto the end of the chain
        tail = nb;
        added++;
    }

    //if we ran out of space, shrink the write to what fits
    limit = (cur_blocks + added) * BLOCK_SIZE;
    if (off + (long)nbyte > limit) nbyte = (size_t)(limit - off);
    if (nbyte == 0) return 0;

    //write the data block by block (read-modify-write for partial blocks)
    blk = nth_block(e->first_block, off / BLOCK_SIZE);
    while (done < nbyte && blk >= 0) {
        size_t boff  = (size_t)((off + (long)done) % BLOCK_SIZE);
        size_t chunk = BLOCK_SIZE - boff;
        if (chunk > nbyte - done) chunk = nbyte - done;

        if (chunk < BLOCK_SIZE && block_read(DATA_START + blk, block) < 0)
            break;
        memcpy(block + boff, (char *)buf + done, chunk);
        if (block_write(DATA_START + blk, block) < 0)   //data goes out now
            break;
        done += chunk;
        blk = fat[blk];
    }

    if (done == 0) return -1;
    f->offset += (off_t)done;
    if (f->offset > e->size) e->size = (int)f->offset;
    return (int)done;
}

int fs_truncate(int fildes, off_t length)
{ (void)fildes; (void)length; return -1; }