#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/disk.h"

//start 20180109
#include <list.h>
//end 20180109

/* On-disk inode.
 * Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk {
	disk_sector_t start;                /* First data sector. */
	off_t length;                       /* File size in bytes. */
	unsigned magic;                     /* Magic number. */
	//start 20180109 - for subdir
	bool _isdir; 
	bool _issym;
	bool b_unused[2]; /*total 4byte*/

	char link_path[100];  /*for symlink, 100byte*/
	//eid 20180109
	// uint32_t unused[125];               /* Not used. */ total 500byte
	// uint32_t unused[124];               /* Not used. */ // uint32_t: 4byte, bool, char: 1byte
	uint32_t unused[99];  /*99*4 = 396*/
};


/* In-memory inode. */
struct inode {
	struct list_elem elem;              /* Element in inode list. */
	disk_sector_t sector;               /* Sector number of disk location. */
	int open_cnt;                       /* Number of openers. */
	bool removed;                       /* True if deleted, false otherwise. */
	int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
	struct inode_disk data;             /* Inode content. */
	bool _issym;

	//start 20180109 - for subdir
	// bool _isdir;
	//eid 20180109
};

struct bitmap;

void inode_init (void);
bool inode_create (disk_sector_t, off_t);
struct inode *inode_open (disk_sector_t);
struct inode *inode_reopen (struct inode *);
disk_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

//start 20180109 - for isdir, subdir
void write_isdir(disk_sector_t sector, bool isdir);
//end 20180109

#endif /* filesys/inode.h */
