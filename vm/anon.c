/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "threads/vaddr.h" // edit to use PGSIZE
#include "lib/kernel/bitmap.h" // edit to use bitmap
#include "threads/mmu.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

struct lock swap_lock;

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
	swap_disk = disk_get(1, 1);
	int swap_slot = ((int)disk_size(swap_disk))/8; //sector 8당 1 swap_slot이므로
	//printf("disk size: %d, swap_slot: %d\n", disk_size(swap_disk), swap_slot);
	swap_table = bitmap_create((size_t)swap_slot);
	lock_init(&swap_lock);
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
	struct anon_page *anon_page = &page->anon;
//	printf("start swap_in\n");
	int slot_num = page->swap_slot;
	if(slot_num < 0){
		return false;
	}
	int sector_num = slot_num * 8;
	lock_acquire(&swap_lock);
	for(int i = 0; i < 8; i++){
		//printf("before disk read\n");
		disk_read(swap_disk, (disk_sector_t)(sector_num + i), kva + (512 * i));
	}
	page->swap_slot = -1;
	pml4_set_page(thread_current()->pml4, page->va, kva, page->writable);
	bitmap_set(swap_table, (size_t)slot_num, 0);
	lock_release(&swap_lock);

	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	bool success = false;
//	printf("start anon_swap_out");
	void *p = pml4_get_page(thread_current()->pml4, page->va);

	if(bitmap_contains(swap_table, (size_t)0, bitmap_size(swap_table), 0)){ // there is an empty swap slot
		int slot_num = bitmap_scan(swap_table, (size_t)0, bitmap_size(swap_table), 0); // find free swap slot
		//printf("slot_num: %d\n", slot_num);
		int sector_num = slot_num * 8; // because one sector is 512 byte and one slot is 4096 byte.
		lock_acquire(&swap_lock);
		for(int i = 0; i < 8; i++){
			//printf("before disk_write\n");
			//printf("sector_num: %d\n", (sector_num + i));
			disk_write(swap_disk, (disk_sector_t)(sector_num + i), p + (512 * i)); // write to 4 sectors // page? or anon_page?
		}
		page->swap_slot = slot_num; // page에 어디 slot에 넣어놨는지 저장
		lock_release(&swap_lock);
		bitmap_set(swap_table, (size_t)slot_num, 1); // change bit for slot_num
		return true;
	}else{
		PANIC("no more swap slot!!");
	}
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	free(page->frame);//edit
}
