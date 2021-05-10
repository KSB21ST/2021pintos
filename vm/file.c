/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"

//start 20180109
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "threads/mmu.h"

struct lock unmap_lock;
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
	// file_page->page_cnt ++;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
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
	// uint32_t read_bytes = (uint32_t)length;
	// uint32_t zero_bytes = (PGSIZE - read_bytes)%PGSIZE; //why?????
	// size_t read_bytes = length;
	size_t read_bytes = length < file_length(file) ? length : file_length(file);
	size_t zero_bytes = (PGSIZE - read_bytes)%PGSIZE; //why?????

	void *temp_addr = addr;


	while (read_bytes > 0 || zero_bytes > 0) {
		/* Do calculate how to fill this page.
		* We will read PAGE_READ_BYTES bytes from FILE
		* and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = page_read_bytes % PGSIZE == 0 ? 0 : (PGSIZE - page_read_bytes)%PGSIZE;
		// size_t page_zero_bytes = PGSIZE - page_read_bytes;
		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		// void *aux = NULL;
		//start 20180109
		struct page_load *aux = malloc(sizeof(struct page_load));
		if(aux == NULL){
			free(aux);
			return NULL;
		}
		aux->file = file;
		aux->ofs = offset;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;
		//end 20180109
		lock_acquire(&unmap_lock);
		if (!vm_alloc_page_with_initializer (VM_FILE, pg_round_down(temp_addr),
				writable, file_lazy_load_segment, aux)){
			lock_release(&unmap_lock);
			return NULL;
		}
			
		lock_release(&unmap_lock);
		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		temp_addr += PGSIZE;
		offset += page_read_bytes;
	}
	return addr;
}

/* Do the munmap */
void
do_munmap (void *addr) 
{	
	struct page *upage = spt_find_page(&thread_current()->spt, addr);
	while(upage != NULL){
		struct page_load *temp_aux = (struct page_load *)(upage->uninit).aux;
		if(temp_aux == NULL)
			break;
		if(pml4_is_dirty(thread_current()->pml4, upage->va)){ //why???
			lock_acquire(&unmap_lock);
			// file_seek (temp_aux->file, temp_aux->ofs);
			// file_write(temp_aux->file, addr, temp_aux->read_bytes);
			file_write_at(temp_aux->file, addr, temp_aux->read_bytes, temp_aux->ofs);
			lock_release(&unmap_lock);
			pml4_set_dirty(thread_current()->pml4, upage->va, 0);
		}
		pml4_clear_page (thread_current()->pml4, upage->va);
		addr += PGSIZE;
		upage = spt_find_page(&thread_current()->spt, addr);
	}
}	