# Project-5-Simple_File_System
**Project 5: Implementing a Simple File System**

**Overview**

The goal of this project is to implement a simple file system. The file system is built on a “virtual disk”: a single, ordinary file in the Linux file system that your code treats as if it were a raw disk divided into fixed-size blocks.

The file system you implement will include the basic features found in any file system: a directory of its files, storage of file data, management of free space, and a library of functions (“system calls”) that applications use to create, delete, open, close, read, write, seek within, and truncate files.

Your library stores all file data and all file system meta-information on the virtual disk. Because the virtual disk is itself a Linux file, the low-level helper functions we provide use ordinary Linux file operations to read and write whole blocks of it. Your library must never use Linux file operations directly, and it must not create any other “real” files on the Linux file system. All access to the virtual disk goes through the provided helper functions.

**The Virtual Disk**

We provide the header file disk.h and the source file disk.c. **Do not modify these two files.** They define the disk geometry and five helper functions:

#define DISK_BLOCKS 8192 /* number of blocks on the disk */

#define BLOCK_SIZE 4096 /* bytes per block */

int make_disk(char *name); /* create an empty virtual disk file */

int open_disk(char *name); /* open a virtual disk */

int close_disk(); /* close the open virtual disk */

int block_write(int block, char *buf); /* write one whole block */

int block_read(int block, char *buf); /* read one whole block */

The virtual disk has 8,192 blocks numbered 0 through 8,191, and each block holds 4,096 bytes (4 KB), for a total of 32 MB. block_read and block_write always transfer exactly one full block. Only one virtual disk can be open at a time.

**Requirements and Limits**

To keep the project manageable, the file system has the following fixed properties:

* **Single directory.** There is no directory hierarchy. All files live in one root directory.
* **At most 64 files** exist at any one time. Deleted files do not count against this limit.
* **File names** are 1 to 15 characters long, not counting the terminating null byte. A name therefore occupies at most 16 bytes on disk. An empty name or a name longer than 15 characters is an error.
* **Data region.** Exactly 4,096 blocks hold file data: **blocks 4096 through 8191**. Blocks 0 through 4095 are available for your super block, directory, file allocation table, and any other meta-information. You will not need all of them.
* **Maximum file size** is 16 MB (4,096 blocks × 4 KB). A single file may therefore fill the entire data region.
* **At most 32 file descriptors** may be open at the same time.
* Data blocks must be returned to the free pool when a file is deleted or truncated.

⠀
**Required Interface (fs.h)**

You must provide the supplied header file named fs.h that declares exactly the following functions, with these names and signatures. The instructor’s test programs are compiled against fs.h and linked with your library, so a function whose name or signature differs will fail every test that uses it.

#define FS_MAX_FILES 64

#define FS_MAX_NAME 15 /* characters, not counting the null byte */

#define FS_MAX_FD 32

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

**General Rules for All Functions**

* Every function returns -1 on failure. Unless stated otherwise, it returns 0 on success.
* Every function except make_fs and mount_fs fails (returns -1) when no file system is mounted.
* mount_fs fails when a file system is already mounted.
* A function that fails must leave the file system unchanged.
* Functions must not print to standard output. You may print diagnostic messages to standard error.

⠀
**File System Management Functions**

int make_fs(char *disk_name);

Creates a fresh, empty file system on a new virtual disk named disk_name. First call make_disk(disk_name) to create the disk. Then open it, write and initialize the meta-information your file system needs (super block, empty directory, file allocation table with every data block marked free), and close it. make_fs does **not** leave the file system mounted. Returns 0 on success and -1 if the disk could not be created, opened, or initialized.

int mount_fs(char *disk_name);

Mounts the file system stored on the virtual disk disk_name, making it “ready for use.” Open the disk and load into memory the meta-information needed by the functions below. Returns 0 on success. Returns -1 if the disk cannot be opened, if a file system is already mounted, or if the disk does not contain a valid file system. To detect a valid file system, your super block must contain a **magic number**, a fixed, **nonzero** value of your choice written by make_fs and checked by mount_fs. A disk created with make_disk alone, or any other file, must be rejected. Because make_disk fills the disk with zeros, a magic number of 0 would make an empty disk look valid.

int umount_fs(char *disk_name);

Unmounts the file system on the virtual disk disk_name. Any file descriptors that are still open are closed automatically. Write all meta-information that is cached in memory back to the disk, then close the disk. Returns 0 on success and -1 if no file system is mounted, if disk_name is not the mounted disk, or if the data could not be written or the disk could not be closed.

**Persistence**

Your **file system must provide persistent storage.** Suppose a program creates a file system, mounts it, creates several files, writes data to them, and unmounts it. A different program that later mounts the same virtual disk must see the same files with the same contents.

**File data blocks must be written to disk as soon as** fs_write **changes them.** Meta-information (the directory and the file allocation table) may be kept in memory while the file system is mounted, provided it is written back no later than umount_fs. Be prepared to explain in your demo what would be lost if a program crashed before calling umount_fs.

**File System Functions**

These functions require a mounted file system. They behave much like the corresponding Linux system calls.

int fs_create(char *name);

Creates a new, empty file named name in the root directory. Returns 0 on success. Fails if a file named name already exists, if the name is empty or longer than 15 characters, or if 64 files already exist. Creating a file does not open it. It must be opened with fs_open before it can be read or written.

int fs_delete(char *name);

Deletes the file name from the root directory and returns all of its data blocks to the free pool. Returns 0 on success. Fails if the file does not exist or if any open file descriptor refers to it. When it fails, the file is not deleted.

int fs_open(char *name);

Opens the file name for reading and writing and returns a file descriptor: an integer from 0 to 31. The descriptor’s file offset is set to 0, the beginning of the file. Fails if the file does not exist or if 32 descriptors are already open.

The same file may be opened more than once. Each call returns a separate, independent descriptor with its own offset. All descriptors for the same file share the file’s data and size. If a write through one descriptor extends the file, the new size is visible immediately through every other descriptor for that file.

int fs_close(int fildes);

Closes the file descriptor fildes. A closed descriptor can no longer be used. Returns 0 on success. Fails if fildes is out of range or is not open.

int fs_read(int fildes, void *buf, size_t nbyte);

Reads up to nbyte bytes from the file, starting at the descriptor’s current offset, into buf. You may assume buf holds at least nbyte bytes. Reading stops at the end of the file. Returns the number of bytes actually read, which is less than nbyte when the end of the file is reached, and 0 when the offset is already at the end of the file. The offset advances by the number of bytes read. Fails if fildes is not a valid open descriptor.

int fs_write(int fildes, void *buf, size_t nbyte);

Writes nbyte bytes from buf to the file, starting at the descriptor’s current offset. Bytes that fall inside the existing file **overwrite** the existing data. Bytes written past the current end of the file **extend** the file, and new data blocks are allocated as needed.

If the disk runs out of free data blocks, or the file reaches the 16 MB maximum size, write as many bytes as possible and stop. Returns the number of bytes actually written, which is 0 if no space at all was available. The offset advances by the number of bytes written. Fails if fildes is not a valid open descriptor.

int fs_get_filesize(int fildes);

Returns the current size, in bytes, of the file referred to by fildes. Fails if fildes is not a valid open descriptor.

int fs_lseek(int fildes, off_t offset);

Sets the descriptor’s file offset to offset, measured in bytes from the beginning of the file. The offset may be anywhere from 0 to the file size, inclusive. Setting it equal to the file size positions the descriptor at the end of the file, for example to append: fs_lseek(fd, fs_get_filesize(fd));. Returns 0 on success. Fails if fildes is invalid, if offset is negative, or if offset is greater than the file size.

int fs_truncate(int fildes, off_t length);

Shrinks the file referred to by fildes to length bytes. Data beyond length is discarded, and any data blocks no longer needed are returned to the free pool. fs_truncate cannot extend a file. Any open descriptor for this file whose offset is now beyond length has its offset set to length. Returns 0 on success. Fails if fildes is invalid, if length is negative, or if length is greater than the current file size.

**Inspection Functions**

These two functions let your demonstration programs, and the instructor’s tests, show the state of the file system.

int fs_listfiles(char names[][FS_MAX_NAME + 1], int max_names);

Copies the names of up to max_names existing files into the array names, one null-terminated name per row, and returns the number of names copied. The order of the names is not specified. Returns 0 for an empty directory.

int fs_free_blocks(void);

Returns the number of free blocks in the data region. Immediately after make_fs and mount_fs, it returns 4096.

**Implementation**

A file system could be organized in several ways, such as a FAT (file allocation table) design or the more complex Unix inode design. **For this project you must use the FAT design.** You will need the following on-disk structures:

* **Super block (boot block).** Normally block 0. It records the magic number and where the other structures are located: the first block of the FAT, the first block of the directory, and the first block of the data region.
* **Root directory (the logical directory).** One entry per file. Each entry holds the file’s name, its size in bytes, whether the entry is in use, and the number of the file’s first data block. This is the information that identifies and describes a file.
* **File allocation table (the physical directory).** One entry for each of the 4,096 data blocks. The entry for a block either marks the block as free, or gives the number of the next block in the same file, or marks the block as the last block of its file. Following these links from a file’s first block yields all of its blocks in order. The FAT therefore records both free space and the mapping from files to blocks.

⠀
The **link between the logical and physical directories** is the first-block number stored in each directory entry: it is where the chain of blocks for that file begins in the FAT.

Separate from the on-disk structures, you need an in-memory **file descriptor table** with 32 slots. Each open descriptor records which file it refers to and its current offset. The offset is **not** file meta-information and is **never** stored on disk. Descriptors exist only while a program is running and the file system is mounted.

**Implementation Hints**

* **Always use a full-block buffer.** block_read writes exactly 4,096 bytes into the buffer you give it. Reading a block directly into a smaller structure overwrites whatever memory follows it. Read into char buf[BLOCK_SIZE] and then copy out with memcpy, and do the reverse when writing.
* **The FAT does not fit in one block.** With 4,096 entries, a FAT of 2-byte entries needs 2 blocks and one of 4-byte entries needs 4 blocks. Your code for loading and saving the FAT must handle several blocks.
* **Choose distinct sentinel values** for “free” and “end of file” in the FAT, and make sure neither can be mistaken for a valid block number.
* **Partial-block writes need read-modify-write.** To change part of a block, read the block, change the bytes, and write the whole block back. Most fs_write bugs occur when a write starts or ends in the middle of a block, or crosses a block boundary.
* An offset in a file maps to block index offset / BLOCK_SIZE in the file’s chain and byte offset % BLOCK_SIZE within that block.

⠀
**Project Implementation Information**

* Development is done in your GitHub repository for this project, and the repository is the official record of your work. Your code must be written in C. Build your executables from your own sources together with the provided disk.h and disk.c, unchanged.
* Your repository must contain all sources, your fs.h, and a Makefile. Running make must build your file system library and your demonstration programs with no errors. Do not commit object files, executables, or virtual disk files.
* Before each weekly deadline, tag the commit to be graded: week1, week2, and final. If the tag is missing, the last commit on main before the deadline is graded.

⠀
**Tagging your work**

Each week, after committing the work you want graded, run two commands with that week’s tag name (week1, week2, or final):

git tag week1

git push origin week1

git tag labels your current commit on your own computer. git push origin week1 uploads that label to GitHub, where the grader looks for it. A plain git push does **not** upload tags, so the second command is required. To check, open your repository on GitHub and look under **Tags**; if the tag is not listed there, the grader cannot see it.

**Testing Your Code**

The instructor’s tests are not released. **Writing your own tests is part of the project:** deciding what to test makes you work out exactly what each function must do, including its error cases. Keep your test programs in your repository. During demos the TA may ask what you tested and how.

Every requirement in this document is something a test could check. Good places to start are the error returns of each function, reads and writes that start or end in the middle of a block or cross a block boundary, files of exactly 4,096 bytes, the 64-file and 32-descriptor limits, a completely full disk, and persistence checked by a **second, separate program run** after unmounting. A test that runs both phases in one process cannot detect data kept only in memory.

At each weekly demo you will see which of the instructor’s tests your tagged commit failed.

**Design Document**

Prepare a design document (2 to 4 pages) that describes:

a) the volume layout: which blocks hold the super block, the FAT, the directory, and the data region;

b) the structure of the physical directory (the FAT): the size of each entry, the sentinel values, and how the FAT is stored across blocks;

c) the method of space allocation, including how free blocks are found, allocated to a growing file, and returned when a file is deleted or truncated;

d) the logical directory structure, including the layout of a directory entry (name, size, first block, in-use flag) and its size in bytes;

e) the relationship between the logical and physical directories;

f) the file descriptor table and how descriptors are allocated and released; and

g) a short description of each internal helper function you write to manage the disk, in addition to the required interface functions.

Commit the design document (PDF) to your repository before the week1 tag. If your design changes, commit an updated version before the final tag.

**Demonstration Application**

In addition to the library, write one or more small programs that **demonstrate the functionality of your file system**. Together they must:

a) create and mount a new virtual disk;

b) create a file;

c) open the file, write at least 20 KB of known data to it (so the file spans several blocks), close the file, and unmount the file system;

d) in a **second, separate program run** after the first has unmounted, mount the file system, open the file, read data from several locations in the file (including reads that cross block boundaries), and compare the data to what was written;

e) copy the file to a new file with a different name, verify the copy, and then delete the original; and

f) show, using fs_listfiles and fs_free_blocks, that the directory was updated correctly and that the blocks of the deleted file were returned to the free pool.

**Deliverables and Weekly Demos**

The project runs for three weeks, with a deadline and a short demo with a TA at the end of each week. **The three weekly demos together are 60% of the project grade. The autograde tests are 50% of the weekly demo**

**How each demo works**

* **Before the demo**, the TA runs the instructor’s tests for that week on your tagged commit.
* **The demo itself takes about 5 minutes.** The TA tells you your test score and which tests failed, then asks you two questions about your code. One is about that week’s design; the other is about a failed test or a line of your code chosen by the TA.
* **Come with your repository open** in an editor. Every answer should point to your own code.
* Each demo is scored out of 100: half from the test score and half from your two answers.

⠀
Your programs are not run during the demo; the tests do that beforehand. Each week below lists what the tests for that week check, so you know what must work by the deadline.

**Week 1: Design, Volume Management, and the Directory**

* Review the features of a FAT file system: space allocation, components, and directory requirements.
* Design the super block, the physical directory (FAT), the logical directory, the link between them, and the file descriptor table.
* Implement make_fs, mount_fs, umount_fs, fs_create, fs_delete, fs_listfiles, and fs_free_blocks.

⠀
**Week 1 tests check:** creating, mounting and unmounting a file system; rejecting a disk created with make_disk alone; creating, listing and deleting files; duplicate, empty and over-long names; the 64-file limit; the free-block count; and that the directory survives an unmount and is seen by a separate program. The design document is due at this deadline.

**Week 2: Reading and Writing Files**

* Implement fs_open, fs_close, fs_read, fs_write, fs_get_filesize, and fs_lseek.

⠀
**Week 2 tests check:** reading and writing data that spans several blocks, at offsets that start mid-block and cross block boundaries; overwriting the middle of a file without changing its size; independent offsets for two descriptors on the same file; the 32-descriptor limit; that fs_delete fails on an open file; the free-block count as files grow and are deleted; and that file data survives an unmount and is seen by a separate program.

**Week 3: Truncation, Testing, and the Demonstration Application**

* Implement fs_truncate.
* Complete the demonstration application described above.
* Test edge cases, including filling the disk completely and writing a file of the maximum size.

⠀
**Week 3 tests check:** truncation, including to 0 and to exact block boundaries, and its effect on other descriptors’ offsets; a 16 MB file; writing to a full disk and reclaiming space by deleting; copying a file and deleting the original; and a long random sequence of operations checked against the expected file contents. 

**Final Test Run:** The final run will run using your final tagged commit in the repository and will execute your demonstration application. The demonstration application is described above. It has several steps to complete the demonstration. You should be prepared to execute all of the steps. 

**Carry-forward rule**

If a test fails at its own week’s demo and passes at the next week’s demo, you earn **half** of that test’s points. Week 1 tests are rerun at the Week 2 demo, and Week 2 tests at the Week 3 demo. Week 1 tests cannot be carried into Week 3.

**Grading**

|   **Component**  |   **Weight**  |   **How it is assessed**  |  
|---|---|---|
|   Week 1 demo  |   20%  |   Week 1 tests (half) and two questions about your code (half)  |
|   Week 2 demo  |   20%  |   Week 2 tests (half) and two questions (half)  |
|   Week 3 (final) demo  |   20%  |   Week 3 tests (half) and two questions (half)  |
|   Final test run  |   25%  |   Rerun all instructor’s tests, from all three weeks, using your final tagged commit. Execute the demonstration application  |
|   Design document  |   10%  |   Completeness and clarity of items (a) through (g)  |
|   Code quality  |   5%  |   Structure, comments, error checking, your own tests, clean build with no warnings  |
|   **Total**  |   **100%**  |      |  

