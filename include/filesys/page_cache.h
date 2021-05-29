#ifndef FILESYS_PAGE_CACHE_H
#define FILESYS_PAGE_CACHE_H
#include "vm/vm.h"

struct page;
enum vm_type;

struct page_cache {
    // edit for buffer cache
    struct buffer_data *buffer_data;
    struct file *file;
	struct page_load *aux;
};

void page_cache_init (void);
bool page_cache_initializer (struct page *page, enum vm_type type, void *kva);
#endif
