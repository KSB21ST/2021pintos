#ifndef FILESYS_DIRECTORY_H
#define FILESYS_DIRECTORY_H

#include <stdbool.h>
#include <stddef.h>
#include "devices/disk.h"

//start 20180109
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"

#include "threads/palloc.h"
#include "threads/vaddr.h"
//end 20180109

/* Maximum length of a file name component.
 * This is the traditional UNIX maximum length.
 * After directories are implemented, this maximum length may be
 * retained, but much longer full path names must be allowed. */
#define NAME_MAX 14

struct inode;

/* A directory. */
struct dir {
	struct inode *inode;                /* Backing store. */
	off_t pos;                          /* Current position. */
};

/* A single directory entry. */
struct dir_entry {
	disk_sector_t inode_sector;         /* Sector number of header. */
	char name[NAME_MAX + 1];            /* Null terminated file name. */
	bool in_use;                        /* In use or free? */
};

/* Opening and closing directories. */
bool dir_create (disk_sector_t sector, size_t entry_cnt);
struct dir *dir_open (struct inode *);
struct dir *dir_open_root (void);
struct dir *dir_reopen (struct dir *);
void dir_close (struct dir *);
struct inode *dir_get_inode (struct dir *);

/* Reading and writing. */
bool dir_lookup (const struct dir *, const char *name, struct inode **);
bool dir_add (struct dir *, const char *name, disk_sector_t);
bool dir_remove (struct dir *, const char *name);
bool dir_readdir (struct dir *, char name[NAME_MAX + 1]);

//start 20180109
struct dir * parse_n_locate(const char *path_name);
struct dir * locate_dir(char **argv, int number, char *real_name);
int dir_path_parse(const char *path_name, char **argv);
struct dir * parse_path(char *path_name, char *last_name);
//end 20180109

bool dir_create_scratch (disk_sector_t sector, size_t entry_cnt);
struct dir *dir_open_scratch (struct inode *);
// struct dir *dir_open_root (void);
struct dir *dir_reopen_scratch (struct dir *);
void dir_close_scratch (struct dir *);
// struct inode *dir_get_inode (struct dir *);

/* Reading and writing. */
bool dir_lookup_scratch (const struct dir *, const char *name, struct inode **);
bool dir_add_scratch (struct dir *, const char *name, disk_sector_t);
// bool dir_remove_scratch (struct dir *, const char *name);
// bool dir_readdir (struct dir *, char name[NAME_MAX + 1]);


#endif /* filesys/directory.h */
