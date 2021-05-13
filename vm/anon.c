/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"

//start 20180109
#include "threads/vaddr.h"
#include <bitmap.h>
// struct bitmap *swap_table;
// size_t swap_size;
static void anon_disk_connect(bool read, int index, void *kva);
//end 2018019

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	// swap_disk = NULL;

	swap_disk = disk_get(1, 1);
    swap_size = disk_size(swap_disk) / 8;
    swap_table = bitmap_create(swap_size);
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
    // printf("swap in, %#x \n", page->va);
	struct anon_page *anon_page = &page->anon;
    // ASSERT((page->uninit).type == VM_ANON);
    ASSERT(page != NULL);
    ASSERT(kva != NULL);
    ASSERT(is_kernel_vaddr(kva));
    bool success = false;
    int page_no = anon_page->swap_index;

    ASSERT(page_no <= swap_size);
    if (bitmap_test(swap_table, page_no)){
        anon_disk_connect(true, page_no, kva);
        success = true;
    }
    page->frame->page = page;
    anon_page->swap_index = page_no;
    list_push_back(&victim_list, &page->victim);
    return success;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
    // printf("swap out, %#x \n", page->va);
	struct anon_page *anon_page = &page->anon;
    // ASSERT((page->uninit).type == VM_ANON);
    ASSERT(page != NULL);
    ASSERT(page->frame != NULL);

    void *kva = page->frame->kva;
	int page_no = bitmap_scan(swap_table, 0, 1, false);
    anon_disk_connect(false, page_no, kva);
    pml4_clear_page(thread_current()->pml4, page->va);
    anon_page->swap_index = page_no;
    page->frame = NULL;
    list_remove(&page->victim);
    return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	// free(page->frame);//edit
}

static void
anon_disk_connect(bool read, int index, void *kva)
{
    bool set_bitmap = true;
    for (int i = 0; i < 8; i++)
        {
            if(read){
                set_bitmap = false;
                disk_read(swap_disk, index * 8 + i, kva + 512 * i);
            }
            else
                disk_write(swap_disk, index * 8 + i, kva + 512 * i);
        }
    bitmap_set(swap_table, index, set_bitmap);
}