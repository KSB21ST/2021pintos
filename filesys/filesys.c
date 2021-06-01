#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "filesys/fat.h"
#include "threads/vaddr.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
 * If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) {
	filesys_disk = disk_get (0, 1);
	if (filesys_disk == NULL)
		PANIC ("hd0:1 (hdb) not present, file system initialization failed");

	inode_init ();

#ifdef EFILESYS
	fat_init ();

	if (format)
		do_format ();
	fat_open ();
	inode_create (ROOT_DIR_SECTOR, DISK_SECTOR_SIZE, true);
	thread_current()->cur_sector = ROOT_DIR_SECTOR;
	dir_add(dir_open(inode_open(thread_current()->cur_sector)), ".", ROOT_DIR_SECTOR);
#else
	/* Original FS */
	free_map_init ();

	if (format)
		do_format ();

	free_map_open ();
#endif
}

/* Shuts down the file system module, writing any unwritten data
 * to disk. */
void
filesys_done (void) {
	/* Original FS */
#ifdef EFILESYS
	fat_close ();
#else
	free_map_close ();
#endif
}

/* Creates a file named NAME with the given INITIAL_SIZE.
 * Returns true if successful, false otherwise.
 * Fails if a file named NAME already exists,
 * or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) {
	// printf("in filesys_create name: %s \n", name);
	disk_sector_t inode_sector;
	//start 20180109
	cluster_t clst = fat_create_chain(0);
	inode_sector = cluster_to_sector(clst);
	// printf("in filesys_create sector: %d \n", inode_sector);
	//TODO-subdir
	//end 20180109

	// struct dir *dir = dir_open_root ();
	// struct dir *dir = thread_current()->cur_dir;

	char **argv;
	argv = palloc_get_page(PAL_ZERO);

	char *name_copy = palloc_get_page(PAL_ZERO);
	strlcpy (name_copy, name, PGSIZE);


	struct dir *dir = directory_parse(name_copy, argv);
	// printf("filesys_create::: argv[0]: %s\n", argv[0]);

	// printf("dir != NULL (%d)\n inode_sector (%d)\n", dir!=NULL, inode_sector);
	bool success = (dir != NULL
			// && free_map_allocate (1, &inode_sector)
			&& inode_sector
			&& inode_create (inode_sector, initial_size, false)
			// && dir_add (dir, name, inode_sector));
			&& dir_add (dir, argv[0], inode_sector));
	if (!success && inode_sector != 0)
		// free_map_release (inode_sector, 1);
		// fat_remove_chain(inode_sector, 0);
		fat_put(inode_sector, 0);
	
	if(dir == NULL){
		// printf("dir is null\n");
	}
	// printf("before dir_close\n");
	dir_close (dir);
	// printf("after dir_close\n");
	palloc_free_page(argv);
	palloc_free_page(name_copy);
	return success;
}

/* Opens the file with the given NAME.
 * Returns the new file if successful or a null pointer
 * otherwise.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name) {
	// printf("name: %s\n", name);
	//TODO-for subdir

	// struct dir *dir = dir_open_root ();
	// struct dir *dir = thread_current()->cur_dir;
//start	
	char **argv;
	argv = palloc_get_page(PAL_ZERO);

	char *name_copy = palloc_get_page(PAL_ZERO);
	strlcpy (name_copy, name, PGSIZE);

	struct dir *dir = directory_parse(name_copy, argv);
	// printf("filesys_open::: argv[0]: %s\n", argv[0]);

//end

	struct inode *inode = NULL;
	// printf("dir != NULL : %d, dir sector: %d\n", dir != NULL, dir->inode->sector);
	if (dir != NULL)
		// dir_lookup (dir, name, &inode);
		dir_lookup(dir, argv[0], &inode);
		// printf("dir sector: %d\n", dir->inode->sector);
	if(inode == NULL){
		// printf("inode is null\n");
		// return NULL;
	}
	if(dir == NULL){
		// printf("dir is NULL\n");
		return false;
	}
	// if(inode == NULL && !memcmp(argv[0], ".", sizeof("."))){
		// inode = dir->inode;
	// }
	dir_close (dir);
	// printf("after close dir\n");
	// inode->_isdir = false; //edit for subdirectory

	palloc_free_page(argv);
	palloc_free_page(name_copy);
	return file_open (inode);
}

/* Deletes the file named NAME.
 * Returns true if successful, false on failure.
 * Fails if no file named NAME exists,
 * or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) {
	// struct dir *dir = dir_open_root ();

	char **argv;
	argv = palloc_get_page(PAL_ZERO);

	char *name_copy = palloc_get_page(PAL_ZERO);
	strlcpy (name_copy, name, PGSIZE);

	struct dir *dir = directory_parse(name_copy, argv);
	if(dir == NULL){
		// printf("dir is NULL (in remove)\n");
		return false;
	}
	struct inode *r_inode;
	struct dir *r_dir;
	dir_lookup(dir, argv[0], &r_inode);
	r_dir = dir_open(r_inode);
	char temp[15];
	if(dir_readdir(r_dir, temp)){
		dir_close(r_dir);
		dir_close(dir);
		palloc_free_page(argv);
		palloc_free_page(name_copy);
		return false;
	}
	if(r_dir->inode->sector == thread_current()->cur_sector){
		thread_current()->cur_sector = 0;
	}

	dir_remove(r_dir, ".");
	// dir_remove(r_dir, "..");
	dir_close(r_dir);


	// bool success = dir != NULL && dir_remove (dir, name);
	bool success = dir != NULL && dir_remove (dir, argv[0]);
	dir_close (dir);

	palloc_free_page(argv);
	palloc_free_page(name_copy);
	return success;
}

/* Formats the file system. */
static void
do_format (void) {
	printf ("Formatting file system...");

#ifdef EFILESYS
	/* Create FAT and save it to the disk. */
	fat_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	// thread_current()->cur_dir = dir_open_root(); // edit for subdir
	// close(thread_current()->cur_dir);
	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}

//TODO: PARSING DIRECTORY PATHS
//TODO: CREATE DIRECTORY