**********************************************
In-memory File system implementation
**********************************************

System calls implemented : 
    - getattr
    - readdir
    - open
    - read
    - mkdir
    - rmdir
    - rename
    - write
    - truncate
    - symlink
    - readlink
    - mknod
    - access
    - unlink
    - utimens

Following commands were tested : touch , cat , vim , output redirections
( > and >> ) , tree , mv , rmdir , mkdir , ln -s , echo , ls.

When a filesystem is mounted the following directory structure is created

/*
 *  /
 *  |
 *  |____ file1 { Contents : "First file in root directory using fuse!\n" }
 *  |
 *  |____ directory1
 *      |
 *      |____ file2 { Contents : "file2 in directory1 using fuse!\n" }
 *      |
 *      |____ file3 { Contents : "file3 in directory1 using fuse!\n" }
 *      |
 *      |____ directory2
 *          |
 *          |____ file4 { Contents : "file4 in directory2 using fuse!\n" }
 * 
 */

In read only FS , only read only operations can be performed ; so for checking out
the read-only functionality the files and directories are pre-populated.

In read-write FS , the files and directories can be easily created / truncated /
deleted / renamed / updated easily using system calls.
