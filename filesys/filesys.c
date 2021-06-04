#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"

//20180109 start
#include "threads/thread.h"
#include "threads/vaddr.h"
struct dir *parse_path(char * path_name, char *file_name);

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

	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
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
	disk_sector_t inode_sector = 0;
	inode_sector = fat_create_chain(0);
	// struct dir *dir = dir_open_root ();
	// disk_sector_t t_sector= parse_n_locate(name);
	// ASSERT(_inode->data._isdir == true);
	// struct dir *dir = dir_open(inode_open(t_sector));
	// struct dir *dir = parse_n_locate(name);
	char *file_name = palloc_get_page(0);
	char *name_copy = palloc_get_page(0);
	strlcpy(name_copy, name, PGSIZE);
	struct dir * dir = parse_path(name_copy, file_name);
	// char tmp_name[14 +1];
	// struct dir *dir = parse_path(name, tmp_name);
	if(!dir){
		palloc_free_page(file_name);
		palloc_free_page(name_copy);
		return false;
	}

	bool i_1 = inode_sector;
	bool i_2 = inode_create (inode_sector, initial_size);
	bool i_3 = false;
	if(i_2)
		i_3 = dir_add (dir, file_name, inode_sector);

	bool success = (dir != NULL
			// && free_map_allocate (1, &inode_sector)
			&& i_1 //inode_sector
			&& i_2 //inode_create (inode_sector, initial_size)
			&& i_3); //dir_add (dir, name, inode_sector));
	write_isdir(inode_sector, false);
	if (!success && inode_sector != 0){
		// free_map_release (inode_sector, 1);
		// dir_remove(dir, file_name);
		fat_remove_chain(inode_sector, 0);
		// fat_put(inode_sector, 0);
	}
	dir_close (dir);
	palloc_free_page(file_name);
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
	struct inode *inode = NULL;

	if(!strcmp(name, "/")){
		struct dir *dir = dir_open_root();
		return file_open(dir->inode);
	}

	if(!strcmp(name, ".")){
		struct dir *dir = dir_open(inode_open(thread_current()->t_sector));
		return file_open(dir->inode);
	}

	char *file_name = palloc_get_page(0);
	char *name_copy = palloc_get_page(0);
	strlcpy(name_copy, name, PGSIZE);
	struct dir * dir = parse_path(name_copy, file_name);
	// printf("name: %s, file_name: %s, dir sector: %d in filesys_open\n", name_copy, file_name, dir->inode->sector);

	if(!dir){
		palloc_free_page(file_name);
		palloc_free_page(name_copy);
		return false;
	}

	if (dir != NULL)
		dir_lookup (dir, file_name, &inode);
	dir_close (dir);
	if (inode == NULL){
		palloc_free_page(file_name);
		palloc_free_page(name_copy);
		return false;
	}
		
	if(inode->data._issym){
		// printf("file_name: %s link sector: %d in filesys_open\n", file_name, inode->sector);
		struct file *ans = filesys_open (inode->data.link_path);
		palloc_free_page(file_name);
		palloc_free_page(name_copy);
		inode_close(inode);
		return ans;
	}
	palloc_free_page(file_name);
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
	uint32_t temp_s = thread_current()->t_sector;
	uint32_t temp_r, temp_p;
	
	//start 20180109
	if(!strcmp(name, "/")){ //warining --  direcotry root 는 무조건 못 지우게 만듦
		// struct dir *dir = dir_open_root();
		// bool success = dir != NULL && dir_remove (dir, ".");
		// dir_close (dir);
		// return success;
		return false;
	}
	if(!strcmp(name, "..") && thread_current()->t_sector){
		return false;
	}
	char *file_name = palloc_get_page(0);
	char *name_copy = palloc_get_page(0);
	strlcpy(name_copy, name, PGSIZE);
	struct dir * dir = parse_path(name_copy, file_name);

	if(!dir){
		palloc_free_page(file_name);
		palloc_free_page(name_copy);
		return false;
	}

	struct inode *r_inode;
	if(dir_lookup(dir, file_name, &r_inode)){
	// printf("file name: %s r_inode: %d in filesys_remove\n", file_name, r_inode->sector);
		if(r_inode->data._issym){
			bool sym_remove = dir_remove(dir, file_name);
			palloc_free_page(file_name);
			palloc_free_page(name_copy);
			return sym_remove;
		}
	}
	else{
		// printf("file name: %s in filesys_remove\n", file_name);
		palloc_free_page(file_name);
		palloc_free_page(name_copy);
		return true;
	}


	struct dir *r_dir = dir_open(r_inode);
	temp_r = r_dir->inode->sector;
	char t_name[NAME_MAX + 1];

	if(r_inode->data._isdir && dir_readdir(r_dir, t_name)){
		palloc_free_page(file_name);
		palloc_free_page(name_copy);
		dir_close(r_dir);
		dir_close (dir);
		return false;
	}
	if(thread_current()->t_sector == r_dir->inode->sector)
		thread_current()->t_sector = 0;
	dir_remove(r_dir, ".");
	// dir_remove(r_dir, "..");
	bool success = dir != NULL && dir_remove (dir, file_name);
	dir_close (dir);
	// free(file_name);
	palloc_free_page(file_name);
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
	// if (!dir_create (ROOT_DIR_SECTOR, 16))
	// 	PANIC ("root directory creation failed");
	// struct dir *t_dir = dir_open_root();
	// thread_current()->t_sector = ROOT_DIR_SECTOR;
	// dir_add (t_dir, ".", ROOT_DIR_SECTOR);
	// dir_add (t_dir, "..", ROOT_DIR_SECTOR);
	// dir_close(t_dir);

	fat_close ();
#else
	free_map_create ();
	if (!dir_create (ROOT_DIR_SECTOR, 16))
		PANIC ("root directory creation failed");
	free_map_close ();
#endif

	printf ("done.\n");
}

void
create_directory_root(){
	struct dir *t_dir = dir_open_root();
	t_dir->inode->data._isdir = true;
	thread_current()->t_sector = ROOT_DIR_SECTOR;
	dir_add (t_dir, ".", ROOT_DIR_SECTOR);
	dir_add (t_dir, "..", ROOT_DIR_SECTOR);
	dir_close(t_dir);
}