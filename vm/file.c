/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"

//start 20180109
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"

struct lock unmap_lock;
#include "devices/disk.h"
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "filesys/fat.h"
off_t swap_file_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset);
off_t swap_file_write_at (struct inode *inode, const void *buffer_, off_t size, off_t offset);
//end 20180109


static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
	lock_init(&unmap_lock);
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;
	struct file_page *file_page = &page->file;
	file_page->aux = (struct page_load *)(page->uninit).aux;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	// printf("file_backed_swap_in\n");
	struct file_page *file_page UNUSED = &page->file;

   	struct page_load *aux = (struct page_load *)(page->file).aux;
    struct file *file = aux->file;
   	off_t ofs = aux->ofs;
    size_t page_read_bytes = aux->read_bytes;
    size_t page_zero_bytes = (PGSIZE - page_read_bytes)%PGSIZE; 
	// lock_acquire(&unmap_lock);
	size_t _read_bytes = file_read_at(file, kva, page_read_bytes, ofs);
	// lock_release(&unmap_lock);
	size_t _zero_bytes = PGSIZE - _read_bytes; 
	memset (kva + _read_bytes, 0, _zero_bytes);
   	list_push_back(&victim_list, &page->victim);
   	return true;
}

off_t
swap_file_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) {
	uint8_t *buffer = buffer_;
	off_t bytes_read = 0;
	uint8_t *bounce = NULL;

	if(inode == NULL)
		return NULL;
	
	while (size > 0) {
		/* Disk sector to read, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector (inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;

		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually copy out of this sector. */
		int chunk_size = size < min_left ? size : min_left;
		if (chunk_size <= 0)
			break;

		//######################for proj4 ###########################3

		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Read full sector directly into caller's buffer. */
			disk_read (filesys_disk, cluster_to_sector(sector_idx), buffer + bytes_read); 
		} else {
			/* Read sector into bounce buffer, then partially copy
			 * into caller's buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}
			disk_read (filesys_disk, cluster_to_sector(sector_idx), bounce);
			memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
		}
		// page_cache_read_len (cluster_to_sector(sector_idx), buffer + bytes_read, sector_ofs, chunk_size); 

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_read += chunk_size;
	}
	free (bounce);

	return bytes_read;
}




/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;

    struct page_load * aux = (struct page_load *) (page->file).aux;
        
    if(pml4_is_dirty(thread_current()->pml4, page->va)){
		// file_seek (aux->file, aux->ofs);
		file_write_at(aux->file, page->va, aux->read_bytes, aux->ofs);
	}
	page->frame = NULL;
	pml4_clear_page (thread_current()->pml4, page->va);
}

off_t
swap_file_write_at (struct inode *inode, const void *buffer_, off_t size, //offset은 inode의 offset 이다
		off_t offset) {
	const uint8_t *buffer = buffer_;
	off_t bytes_written = 0;
	uint8_t *bounce = NULL;

	// struct dir_entry *e = buffer_;
	// printf(" %s in inode_write \n", e->name);
	if (inode->deny_write_cnt){
		return 0;
	}

	while (size > 0) {
		/* Sector to write, starting byte offset within sector. */
		disk_sector_t sector_idx = byte_to_sector (inode, offset);
		int sector_ofs = offset % DISK_SECTOR_SIZE;
		/* Bytes left in inode, bytes left in sector, lesser of the two. */
		off_t inode_left = inode_length (inode) - offset;
		int sector_left = DISK_SECTOR_SIZE - sector_ofs;
		int min_left = inode_left < sector_left ? inode_left : sector_left;

		/* Number of bytes to actually write into this sector. */
		int chunk_size = size < min_left ? size : min_left;
		//start 20180109 - file growth - implement file growth when file is running out of space
		if (chunk_size <= 0){
			// break;
			off_t total_length = offset + size; //after write, this should be inode length
			off_t alloc_amt = total_length - inode_length(inode);
			off_t alloc_sector = bytes_to_sectors(alloc_amt);
			cluster_t clst = fat_get_end(inode->data.start);
			// printf("data start: %d in inode_write_at \n", inode->data.start);
			static char zeros[DISK_SECTOR_SIZE];
			disk_write (filesys_disk, cluster_to_sector(clst), zeros);
			// page_cache_write(cluster_to_sector(clst), zeros);
			for(int i=0;i<alloc_sector;i++)
			{
				// printf("clst: %d \n", clst);
				// static char zeros[DISK_SECTOR_SIZE];
				clst = fat_create_chain(clst);
				disk_write (filesys_disk, cluster_to_sector(clst), zeros);
				// page_cache_write(cluster_to_sector(clst), zeros);
			}
			// inode->data.length += size;
			inode->data.length = offset+size;
			// chunk_size = size;
			continue;
		}

		//######################for porj4##############################
		if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) {
			/* Write full sector directly to disk. */
			disk_write (filesys_disk, cluster_to_sector(sector_idx), buffer + bytes_written); 
		} else {
			/* We need a bounce buffer. */
			if (bounce == NULL) {
				bounce = malloc (DISK_SECTOR_SIZE);
				if (bounce == NULL)
					break;
			}

			/* If the sector contains data before or after the chunk
			   we're writing, then we need to read in the sector
			   first.  Otherwise we start with a sector of all zeros. */
			if (sector_ofs > 0 || chunk_size < sector_left) 
				disk_read (filesys_disk, cluster_to_sector(sector_idx), bounce);
			else
				memset (bounce, 0, DISK_SECTOR_SIZE);
			memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
			disk_write (filesys_disk, cluster_to_sector(sector_idx), bounce); 
		}
		// page_cache_write_len (cluster_to_sector(sector_idx), buffer + bytes_written, sector_ofs, chunk_size);

		/* Advance. */
		size -= chunk_size;
		offset += chunk_size;
		bytes_written += chunk_size;
	}
	//for proj4
	disk_write(filesys_disk, cluster_to_sector(inode->sector), &inode->data);
	// page_cache_write(cluster_to_sector(inode->sector), &inode->data);

	// free (bounce);

	return bytes_written;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
	size_t read_bytes = length < file_length(file) ? length : file_length(file);
	size_t zero_bytes = (PGSIZE - read_bytes)%PGSIZE;

	void *temp_addr = addr;

	struct file *reopen_file = file_reopen(file);


	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		* We will read PAGE_READ_BYTES bytes from FILE
		* and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = page_read_bytes % PGSIZE == 0 ? 0 : (PGSIZE - page_read_bytes)%PGSIZE;
		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		struct page_load *aux = aux_load(reopen_file, offset, page_read_bytes, page_zero_bytes);
		// lock_acquire(&unmap_lock);
		if (!vm_alloc_page_with_initializer (VM_FILE, pg_round_down(temp_addr),
				writable, file_lazy_load_segment, aux)){
			// lock_release(&unmap_lock);
			return NULL;
		}
		// lock_release(&unmap_lock);
		/* Advance. */
		zero_bytes -= page_zero_bytes;
		read_bytes -= page_read_bytes;
		offset += page_read_bytes;
		temp_addr += PGSIZE;
	}
	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) 
{	
	struct page *upage = spt_find_page(&thread_current()->spt, addr);
	while(upage != NULL){
		struct page_load *temp_aux = (struct page_load *)(upage->file).aux;
		if(temp_aux == NULL)
			break;
		if(pml4_is_dirty(thread_current()->pml4, upage->va)){
			// lock_acquire(&unmap_lock);
			file_seek (temp_aux->file, temp_aux->ofs);
			file_write(temp_aux->file, addr, temp_aux->read_bytes);
			// file_write_at(temp_aux->file, addr, temp_aux->read_bytes, temp_aux->ofs);
			// lock_release(&unmap_lock);
			pml4_set_dirty(thread_current()->pml4, upage->va, 0);
		}
		pml4_clear_page (thread_current()->pml4, upage->va);
		addr += PGSIZE;
		upage = spt_find_page(&thread_current()->spt, addr);
	}
}	