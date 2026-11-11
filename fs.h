#ifndef _FS_H_
#define _FS_H_

#include <stddef.h>
#include <sys/types.h>

#define FS_MAX_FILES   64
#define FS_MAX_NAME    15    /* characters, not counting the null byte */
#define FS_MAX_FD      32

/* File system management */
int make_fs(char *disk_name);
int mount_fs(char *disk_name);
int umount_fs(char *disk_name);

/* File system functions */
int fs_open(char *name);
int fs_close(int fildes);
int fs_create(char *name);
int fs_delete(char *name);
int fs_read(int fildes, void *buf, size_t nbyte);
int fs_write(int fildes, void *buf, size_t nbyte);
int fs_get_filesize(int fildes);
int fs_lseek(int fildes, off_t offset);
int fs_truncate(int fildes, off_t length);

/* Inspection functions */
int fs_listfiles(char names[][FS_MAX_NAME + 1], int max_names);
int fs_free_blocks(void);
 
#endif
