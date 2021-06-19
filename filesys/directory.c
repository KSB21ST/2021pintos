#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"

#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/thread.h"

// /* A directory. */
// struct dir {
// 	struct inode *inode;                /* Backing store. */
// 	off_t pos;                          /* Current position. */
// };

// /* A single directory entry. */
// struct dir_entry {
// 	disk_sector_t inode_sector;         /* Sector number of header. */
// 	char name[NAME_MAX + 1];            /* Null terminated file name. */
// 	bool in_use;                        /* In use or free? */
// };

/* Creates a directory with space for ENTRY_CNT entries in the
 * given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (disk_sector_t sector, size_t entry_cnt) {
	bool success = inode_create (sector, entry_cnt * sizeof (struct dir_entry));
	write_isdir(sector, true);
	return success;
}

/* Opens and returns the directory for the given INODE, of which
 * it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) {
	struct dir *dir = calloc (1, sizeof *dir);
	if (inode != NULL && dir != NULL) {
		dir->inode = inode;
		//start 20180109 - for subdir
		// dir->inode->_isdir = true;
		//end 20180109
		dir->pos = 0;
		return dir;
	} else {
		inode_close (inode);
		free (dir);
		return NULL;
	}
}

/* Opens the root directory and returns a directory for it.
 * Return true if successful, false on failure. */
struct dir *
dir_open_root (void) {
	return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
 * Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) {
	return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) {
	if (dir != NULL) {
		inode_close (dir->inode);
		free (dir);
	}
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) {
	return dir->inode;
}

/* Searches DIR for a file with the given NAME.
 * If successful, returns true, sets *EP to the directory entry
 * if EP is non-null, and sets *OFSP to the byte offset of the
 * directory entry if OFSP is non-null.
 * otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
		struct dir_entry *ep, off_t *ofsp) {
	struct dir_entry e;
	size_t ofs;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
			ofs += sizeof e){
		// if(!strcmp("a", name)) //for symlink-file test
		// 	printf("\n name: %s in lookup \n", e.name);
		if (e.in_use && !strcmp (name, e.name)) {
			if (ep != NULL)
				*ep = e;
			if (ofsp != NULL)
				*ofsp = ofs;
			return true;
		}
	}
	return false;
}

/* Searches DIR for a file with the given NAME
 * and returns true if one exists, false otherwise.
 * On success, sets *INODE to an inode for the file, otherwise to
 * a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
		struct inode **inode) {
	struct dir_entry e;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	if (lookup (dir, name, &e, NULL))
		*inode = inode_open (e.inode_sector);
	else
		*inode = NULL;

	return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
 * file by that name.  The file's inode is in sector
 * INODE_SECTOR.
 * Returns true if successful, false on failure.
 * Fails if NAME is invalid (i.e. too long) or a disk or memory
 * error occurs. */
bool
dir_add (struct dir *dir, const char *name, disk_sector_t inode_sector) {
	struct dir_entry e;
	off_t ofs;
	bool success = false;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	/* Check NAME for validity. */
	if (*name == '\0' || strlen (name) > NAME_MAX)
		return false;

	/* Check that NAME is not in use. */
	if (lookup (dir, name, NULL, NULL))
		goto done;

	/* Set OFS to offset of free slot.
	 * If there are no free slots, then it will be set to the
	 * current end-of-file.

	 * inode_read_at() will only return a short read at end of file.
	 * Otherwise, we'd need to verify that we didn't get a short
	 * read due to something intermittent such as low memory. */
	// printf("in dir_add sector: %d \n", inode_sector);
	for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
			ofs += sizeof e)
		if (!e.in_use)
			break;

	/* Write slot. */
	e.in_use = true;
	strlcpy (e.name, name, sizeof e.name);
	e.inode_sector = inode_sector;
	// printf("here? in dir_add name: %s dir: %d \n", e.name, dir->inode->sector);
	success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
done:
	return success;
}

/* Removes any entry for NAME in DIR.
 * Returns true if successful, false on failure,
 * which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) {
	struct dir_entry e;
	struct inode *inode = NULL;
	bool success = false;
	off_t ofs;

	ASSERT (dir != NULL);
	ASSERT (name != NULL);

	/* Find directory entry. */
	if (!lookup (dir, name, &e, &ofs))
		goto done;

	/* Open inode. */
	inode = inode_open (e.inode_sector);
	if (inode == NULL)
		goto done;

	/* Erase directory entry. */
	e.in_use = false;
	if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e)
		goto done;

	/* Remove inode. */
	inode_remove (inode);
	success = true;

done:
	inode_close (inode);
	return success;
}

/* Reads the next directory entry in DIR and stores the name in
 * NAME.  Returns true if successful, false if the directory
 * contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1]) {
	struct dir_entry e;

	while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) {
		dir->pos += sizeof e;
		//start 20180109
		if(!strcmp(e.name, ".") || !strcmp(e.name, ".."))
			continue;
		if((e.name == ".") || (e.name == ".."))
			continue;
		//end 20180109
		if (e.in_use) {
			strlcpy (name, e.name, NAME_MAX + 1);
			return true;
		}
	}
	return false;
}

int
dir_path_parse(const char *path_name, char **argv)
{
   char *token, *last;
   int token_count = 0;
   token = strtok_r(path_name, "/", &last);
   argv[token_count] = token;
   while (token != NULL)
   {
      token = strtok_r(NULL, "/", &last);
      token_count++;
      argv[token_count] = token;
   }
   return token_count;
}

struct dir *
locate_dir(char **argv, int number, char *real_name)
{
	struct dir *t_dir = NULL;
	struct inode *t_inode = NULL;
	disk_sector_t ans = 0;
	bool absolute_path = false;
	if (real_name[0] == '/'){ //absolute path, 첫번쨰 경로가 0이면
		//open root directory
		t_dir = dir_open_root();
		absolute_path = true;
	}
	else{ //relative path
		// dir_lookup(t_dir, argv[0], &t_inode);
		// t_dir = dir_open(t_inode);
		if(thread_current()->t_sector ==NULL){
			t_dir = dir_open_root();
		}
		//printf("hehe\n", inode_get_inumber(dir_get_inode(dir)));
		else{
			t_dir = dir_open(inode_open(thread_current()->t_sector));
		}
	}
	for(int i=0;i<number-1;i++)
	{
		//absolute path 면 argv[0] 가 "0" 이다 -- TODO
		if(!dir_lookup(t_dir, argv[i], &t_inode)){ //dir이 존재하지 않으면
			if(i == 0){
				t_inode = t_dir->inode;
				ans = t_inode->sector;
				// dir_close(t_dir);
				return t_dir;
			}
			dir_close(t_dir);
			return NULL;
			// break;
		}
		if(!t_inode->data._isdir){ //t_inode 가 file inode 면
			break;
		}
		dir_close(t_dir);
		t_dir = dir_open(t_inode);
		if (t_dir == NULL)
			return NULL;
	}
	t_inode = t_dir->inode;
	ans = t_inode->sector;
	// dir_close(t_dir);
	return t_dir;
}

struct dir *
parse_n_locate(const char *path_name)
{
	char *fn_copy = palloc_get_page(0);
	strlcpy (fn_copy, path_name, PGSIZE);
	char *argv = palloc_get_page(PAL_ZERO);
	int argc = dir_path_parse(fn_copy, argv);
	struct dir * t_sector = locate_dir(argv, argc, path_name);
	palloc_free_page(fn_copy);
	palloc_free_page(argv);
	return t_sector;
}

struct dir*
parse_path(char *path_name, char *last_name)
{	
	struct dir *t_dir = NULL;
	struct inode *t_inode = NULL;
	disk_sector_t ans = 0;
	bool absolute_path = false;
	if (path_name[0] == '/'){ //absolute path, 첫번쨰 경로가 0이면
		//open root directory
		t_dir = dir_open_root();
		absolute_path = true;
	}
	else{ //relative path
		if(thread_current()->t_sector ==NULL){
			// t_dir = dir_open_root();
			return NULL;
		}
		//printf("hehe\n", inode_get_inumber(dir_get_inode(dir)));
		else{
			if (thread_current()->t_sector==ROOT_DIR_SECTOR)
				t_dir = dir_open_root();
			else
			t_dir = dir_open(inode_open(thread_current()->t_sector));
		} 
	}
	char *token, *last, *extra;
	token = strtok_r(path_name, "/", &last);
   	extra = strtok_r(NULL, "/", &last);
	int i = 0;
	while (extra != NULL)
	{
		// printf("\ntoken: %s, extra: %s\n", token, extra);
		//absolute path 면 argv[0] 가 "0" 이다 -- TODO
		if(!dir_lookup(t_dir, token, &t_inode)){ //dir이 존재하지 않으면
			if(i == 0){
				strlcpy (last_name, token, PGSIZE);
				return t_dir;
			}
			dir_close(t_dir);
			return NULL;
			// break;
		}
		if(!t_inode->data._isdir && !t_inode->data._issym){ //t_inode 가 file inode 면
			break;
		}
		if(!t_inode->data._isdir && t_inode->data._issym){ //t_node가 symlink 이면
			// return parse_path(t_inode->data.link_path, last_name);
			dir_close(t_dir);
			t_dir = dir_open(parse_path(t_inode->data.link_path, last_name)->inode);
			if(!dir_lookup(t_dir, last_name, &t_inode)){
				// printf("something's wrong!!\n");
			}
		}
		dir_close(t_dir);
		t_dir = dir_open(t_inode);
		if (t_dir == NULL){
			return NULL;
		}
		token = extra;
		extra = strtok_r (NULL, "/", &last);
		i++;
	}
	strlcpy (last_name, token, PGSIZE);
	return t_dir;
}