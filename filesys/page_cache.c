/* page_cache.c: Implementation of Page Cache (Buffer Cache). */

#include "vm/vm.h"
#include "threads/vaddr.h"
static bool page_cache_readahead (struct page *page, void *kva);
static bool page_cache_writeback (struct page *page);
static void page_cache_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations page_cache_op = {
	.swap_in = page_cache_readahead,
	.swap_out = page_cache_writeback,
	.destroy = page_cache_destroy,
	.type = VM_PAGE_CACHE,
};

tid_t page_cache_workerd;
struct lock cache_lock;

/* The initializer of file vm */
void
pagecache_init (void) {
	/* TODO: Create a worker daemon for page cache with page_cache_kworkerd */
	lock_init(&cache_lock);

}

/* Initialize the page cache */
bool
page_cache_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &page_cache_op;
	// list_push_back(&bf_list, &page->page_cache.buffer_data->bf_elem);
	struct page_cache *page_cache = &page->page_cache;
	page_cache->aux = (struct page_load *)(page->uninit).aux;
}

/* Utilze the Swap in mechanism to implement readhead */
static bool
page_cache_readahead (struct page *page, void *kva) {
	struct page_cache *page_cache = &page->page_cache;

   	struct page_load *aux = (struct page_load *)(page->page_cache).aux;
    struct file *file = aux->file;
   	off_t ofs = aux->ofs;
    size_t page_read_bytes = aux->read_bytes;
    size_t page_zero_bytes = (PGSIZE - page_read_bytes)%PGSIZE; 

	lock_acquire(&cache_lock);
	size_t _read_bytes = file_read_at(file, kva, page_read_bytes, ofs);
	// need to edit (read from disk)
	lock_release(&cache_lock);
	
	size_t _zero_bytes = PGSIZE - _read_bytes; 
	memset (kva + _read_bytes, 0, _zero_bytes);

	page->page_cache.buffer_data->page = page;
	page->page_cache.buffer_data->inode = file_get_inode(file);
	page->page_cache.buffer_data->dirty = 0;
	list_push_back(&cache_list, &page->page_cache.buffer_data->cache_elem);

   	return true;
}

/* Utilze the Swap out mechanism to implement writeback */
static bool
page_cache_writeback (struct page *page) {
	struct page_cache *page_cache = &page->page_cache;

    struct page_load * aux = (struct page_load *) (page->page_cache).aux;
        
    if(pml4_is_dirty(thread_current()->pml4, page->va)){
		file_seek (aux->file, aux->ofs);
		file_write(aux->file, page->va, aux->read_bytes);
	}
	page->frame = NULL;
	pml4_clear_page (thread_current()->pml4, page->va); // is this part necessary?
}

/* Destory the page_cache. */
static void
page_cache_destroy (struct page *page) {
}

/* Worker thread for page cache */
static void
page_cache_kworkerd (void *aux) {

	// thread_create ("pagecache", PRI_MAX, ???, aux);
	// thread_exit();
}
