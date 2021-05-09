/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"

//start 20180109
#include <hash.h>
#include "threads/vaddr.h"
#include "threads/mmu.h"
//end 20180109

//start 20180109
struct lock frame_lock;
struct list frame_list;
void hash_file_backup(struct hash_elem *e, void *aux);
static uint64_t vm_hash_func(const struct hash_elem *e, void * aux ){
    struct page * temp = hash_entry(e, struct page, h_elem);
    return hash_int(temp->va); //20180109 - I'm not sure I should put temp->va as argument
}
static bool vm_less_func(const struct hash_elem *a, const struct hash_elem *b){
    const struct page * fst = hash_entry(a,struct page, h_elem);
    const struct page * snd = hash_entry(b,struct page, h_elem);
    return fst->va < snd->va;
}
//end 20180109
void spt_destroy(struct hash_elem *e, void *aux);

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
   vm_anon_init ();
   vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
   pagecache_init ();
#endif
   register_inspect_intr ();
   /* DO NOT MODIFY UPPER LINES. */
   /* TODO: Your code goes here. */
   //start 20180109
   list_init(&frame_list);
   lock_init(&frame_lock);
   //end 20180109
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
   int ty = VM_TYPE (page->operations->type);
   switch (ty) {
      case VM_UNINIT:
         return VM_TYPE (page->uninit.type);
      default:
         return ty;
   }
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
      vm_initializer *init, void *aux) {

   ASSERT (VM_TYPE(type) != VM_UNINIT)

   struct supplemental_page_table *spt = &thread_current ()->spt;

   /* Check wheter the upage is already occupied or not. */
   if (spt_find_page (spt, upage) == NULL) {
      /* TODO: Create the page, fetch the initialier according to the VM type,
       * TODO: and then create "uninit" page struct by calling uninit_new. You
       * TODO: should modify the field after calling the uninit_new. */
      struct page * p = malloc(sizeof(struct page)); //20180109 - watch out for memory leak
      if(VM_TYPE(type) == VM_ANON){
         uninit_new(p, upage, init, type, aux, &anon_initializer);
      }
      if(VM_TYPE(type) == VM_FILE){
         uninit_new(p, upage, init, type, aux, &file_backed_initializer);
      }
      p->writable = writable;
      /* TODO: Insert the page into the spt. */
      if(spt_insert_page(spt, p))
         return true;
      else
         goto err;
   }
err:
   return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
   struct page* p = (struct page*)malloc(sizeof(struct page));
    ASSERT(thread_current() != NULL);
   /* TODO: Fill this function. */
   //pg_round_down: Returns the start of the virtual page that va points within, that is, va with the page offset set to 0.
   p->va = pg_round_down(va); 
   struct hash_elem * temp_elem= hash_find(&spt->spt_table, &p->h_elem); 
   free(p);
    if (temp_elem==NULL)
        return NULL;
    return hash_entry(temp_elem, struct page, h_elem);
   // return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {
   int succ = false;
   /* TODO: Fill this function. */
   if (spt_find_page(spt, page->va) == NULL){
      if(hash_insert(&spt->spt_table, &page->h_elem) ==NULL)
           succ=true;
   }
   return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
   vm_dealloc_page (page);
   return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
   struct frame *victim = NULL;
    /* TODO: The policy for eviction is up to you. */

   return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
   struct frame *victim UNUSED = vm_get_victim ();
   /* TODO: swap out the victim and return the evicted frame. */
   PANIC('TODO');
   return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
   struct frame *frame = NULL;
   /* TODO: Fill this function. */
   frame = malloc(sizeof(struct frame));
   frame->kva = palloc_get_page(PAL_USER);
   if (frame->kva == NULL){
      // free(frame); //not sure of this
      // lock_acquire(&frame_lock);
      // frame = vm_evict_frame();
      // lock_release(&frame_lock);
      PANIC("TODO VM_GET_FRAME");
   }
   frame->page = NULL;
   list_push_back(&frame_list, &frame->elem);
   ASSERT (frame != NULL);
   ASSERT (frame->page == NULL);
   return frame;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
   // if(vm_alloc_page(VM_ANON | VM_MARKER_0, addr, 1))
   // {
   //    vm_claim_page(addr);
   // }
   void *real_addr = pg_round_down(addr);
   void *cur_addr = thread_current()->rsp;
   while (real_addr < cur_addr){
      vm_alloc_page(VM_MARKER_0 | VM_ANON, real_addr, true);
      vm_claim_page(real_addr);
      real_addr += PGSIZE;
   }
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
      bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
   ASSERT(thread_current() != NULL);

   struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
   struct page *page = NULL;
   /* TODO: Validate the fault */
   /* TODO: Your code goes here */
   //start 20180109
   //printf("\nstart vm_try_handle_fault\n");
   if(addr == NULL || not_present == false || is_kernel_vaddr(addr))
      exit(-1);

   //stack growth - stack 주소 확인한다
   void *stack_rsp = user ? f->rsp : thread_current()->rsp;
   // Locate the page that faulted in the supplemental page table
   /*If the memory reference is valid, 
   use the supplemental page table entry to locate the data that goes in the page, 
   which might be in the file system, or in a swap slot, or it might simply be an all-zero page*/
   page = spt_find_page(spt, addr);
   //printf("after spt_find_page\n");

   if(page != NULL){
      /*If the supplemental page table indicates 
      that the user process should not expect any data at the address it was trying to access,*/
      //-----20180109???????????????????????
      /*or if the page lies within kernel virtual memory,*/
      if(is_kernel_vaddr(page->va))
         exit(-1);
      /*or if the access is an attempt to write to a read-only page, 
      then the access is invalid.*/
      if(write && page->writable != true)
         exit(-1);

      /*Obtain a frame to store the page*/ 
      /*If you implement sharing, the data you need may already be in a frame, in which case you must be able to locate that frame.*/
      /*Fetch the data into the frame, by reading it from the file system or swap, zeroing it, etc. 
      If you implement sharing, the page you need may already be in a frame, in which case no action is necessary in this step.*/
      return vm_do_claim_page (page);
   }
   if((addr > stack_rsp - PGSIZE )&& (USER_STACK - 0x100000) <= addr  &&  addr <= USER_STACK){
      vm_stack_growth(pg_round_down(addr));
      return true;
	}
   return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
   //start 20180109
   void *temp_aux = (page->uninit).aux;
   free(temp_aux);
   //end 20180109
   destroy (page);
   free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
   struct page *page = spt_find_page(&thread_current()->spt, va);
   /* TODO: Fill this function */
   if(page == NULL)
      return false;
   return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
   //start 20180109
   //printf("starting point of do_claim\n");
   if(page == NULL)
      return false;
   //end 20180109
   /*Obtain a frame to store the page*/ 
   /*If you implement sharing, the data you need may already be in a frame, in which case you must be able to locate that frame.*/
   struct frame *frame = vm_get_frame ();

   /*Fetch the data into the frame, by reading it from the file system or swap, zeroing it, etc. 
   If you implement sharing, the page you need may already be in a frame, in which case no action is necessary in this step.*/
   /* Set links */
   frame->page = page;
   page->frame = frame;
   //printf("after linking");
   /* TODO: Insert page table entry to map page's VA to frame's PA. */
   /*Point the page table entry for the faulting virtual address to the physical page. You can use the functions in threads/mmu.c*/
   if(pml4_get_page(thread_current()->pml4, page->va)!=NULL)
      return false;
   pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable);

   return swap_in (page, frame->kva);
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
   hash_init(&spt->spt_table, vm_hash_func, vm_less_func, NULL);
   lock_init(&spt_lock);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
      struct supplemental_page_table *src UNUSED) {
   struct hash_iterator i;
   lock_acquire(&spt_lock);
   hash_first(&i, &src->spt_table);
   struct page_load *temp_load;
   bool res = false;
   while(hash_next(&i)){
      struct page *p = hash_entry(hash_cur(&i), struct page, h_elem);
      if(p->operations->type == VM_UNINIT){
         temp_load = malloc(sizeof(struct page_load));
         memcpy(temp_load, p->uninit.aux, sizeof(struct page_load));
         res = vm_alloc_page_with_initializer(p->uninit.type, p->va, p->writable, p->uninit.init, temp_load);
      }else{
         if(vm_alloc_page(p->operations->type, p->va, p->writable)){
            res = true;
            struct page *d = spt_find_page(&thread_current()->spt, p->va);
            if(vm_claim_page(p->va)){
               memcpy(d->frame->kva, p->frame->kva, PGSIZE);
            }
         }else{return false;}
      }
   }
   lock_release(&spt_lock);
   return res;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
   /* TODO: Destroy all the supplemental_page_table hold by thread and
    * TODO: writeback all the modified contents to the storage. */
   if(hash_empty(&spt->spt_table))
		return;
   hash_apply(&spt->spt_table, hash_file_backup);
   hash_clear(&spt->spt_table, spt_destroy);
}

void spt_destroy(struct hash_elem *e, void *aux){
   struct page* page = hash_entry(e, struct page, h_elem);
   // if((page->uninit).type == VM_FILE)
      // munmap(page->va);
   destroy(page);
   free(page);
}

void 
hash_file_backup(struct hash_elem *e, void *aux)
{
   struct page* page = hash_entry(e, struct page, h_elem);
   if((page->uninit).type == VM_FILE)
      munmap(page->va);
}