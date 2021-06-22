/* page_cache.c: Implementation of Page Cache (Buffer Cache). */

#include "vm/vm.h"
static bool page_cache_readahead (struct page *page, void *kva);
static bool page_cache_writeback (struct page *page);
void page_cache_destroy (struct page *page);
static void page_cache_kworkerd (void *aux);
void page_read_ahead_signal(disk_sector_t sector);
disk_sector_t page_read_ahead_wait(void);
void clock_buffer_cache(bool is_dirty);


static bool page_cache_readahead_s (struct page *page, void *kva);
static bool page_cache_writeback_s (struct page *page);
void page_cache_destroy_s (struct page *page);
static void page_cache_kworkerd_s (void *aux);
void page_read_ahead_signal_s(disk_sector_t sector);
disk_sector_t page_read_ahead_wait_s(void);
void clock_buffer_cache_s(bool is_dirty);


/* DO NOT MODIFY this struct */
static const struct page_operations page_cache_op = {
	.swap_in = page_cache_readahead,
	.swap_out = page_cache_writeback,
	.destroy = page_cache_destroy,
	.type = VM_PAGE_CACHE,
};

tid_t page_cache_workerd;

//for proj4 - buffer cache
#include "filesys/page_cache.h"
#include "filesys/filesys.h"

static unsigned
sti_hash_func (const struct hash_elem *elem, void *aux UNUSED)
{
  struct sector_to_index *entry = hash_entry (elem, struct sector_to_index, helem);
  return hash_bytes(&entry->sector, sizeof (entry->sector));
}

static bool
sti_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct sector_to_index *a_entry = hash_entry(a, struct sector_to_index, helem);
  struct sector_to_index *b_entry = hash_entry(b, struct sector_to_index, helem);
  return (a_entry->sector) < (b_entry->sector);
}

/* The initializer of file vm */
void
pagecache_init (void) {
	/* TODO: Create a worker daemon for page cache with page_cache_kworkerd */
  finished = false;
	remain_cache_space = BUFFER_CACHE_NUM;
	for (int i = 0; i < BUFFER_CACHE_NUM; ++i)
  {
		buffer_cache[i].is_valid = false;
		lock_init (&buffer_cache[i].lock);
  }
  hash_init (&sti_hash, sti_hash_func, sti_less_func, NULL);
	list_init (&read_ahead_queue);
	lock_init(&read_ahead_lock);
	cond_init (&read_ahead_cond);
	lock_init (&cache_lock);
	lock_init (&finished_lock);
	page_cache_kworkerd(NULL);
	
}

/* Initialize the page cache */
bool
page_cache_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &page_cache_op;

}

void 
page_cache_read(disk_sector_t sector, void *buffer){
	struct page_cache* info = get_or_evict_cache (sector);
	memcpy (buffer, info->buffer, DISK_SECTOR_SIZE);
	info->access = true;
	lock_release (&info->lock);
  lock_acquire (&read_ahead_lock);
	struct read_ahead_entry* entry = malloc (sizeof (struct read_ahead_entry));
	entry->sector = sector + 1;
	// lock_acquire (&read_ahead_lock);
	list_push_back (&read_ahead_queue, &entry->elem);
	cond_signal (&read_ahead_cond, &read_ahead_lock);
	lock_release (&read_ahead_lock);
}

void 
page_cache_write(disk_sector_t sector, void *buffer){
	struct page_cache* info = get_or_evict_cache (sector);
	memcpy (info->buffer, buffer, DISK_SECTOR_SIZE);
	info->access = true;
	info->dirty = true;
  ASSERT (lock_held_by_current_thread (&info->lock));
	lock_release (&info->lock);
}

void
page_cache_read_len (disk_sector_t sector, void *buffer, off_t offset, off_t len){
	struct page_cache* info = get_or_evict_cache (sector);
	memcpy (buffer, info->buffer + offset, len);
	info->access = true;
	lock_release (&info->lock);
  page_read_ahead_signal(sector);
}

void
page_read_ahead_signal(disk_sector_t sector){
  struct read_ahead_entry* entry = malloc (sizeof (struct read_ahead_entry));
	entry->sector = sector + 1;
	lock_acquire (&read_ahead_lock);
	list_push_back (&read_ahead_queue, &entry->elem);
	cond_signal (&read_ahead_cond, &read_ahead_lock);
	lock_release (&read_ahead_lock);
}

void
page_cache_write_len (disk_sector_t sector, void *buffer, off_t offset, off_t len){
	struct page_cache* info = get_or_evict_cache (sector);
	memcpy (info->buffer + offset, buffer, len);
	info->access = true;
	info->dirty = true;
	lock_release (&info->lock);
}

/* Utilze the Swap in mechanism to implement readhead */
static bool
page_cache_readahead (struct page *page, void *kva) {
	while (true) 
	{
		lock_acquire (&finished_lock);
		if (finished) {
		lock_release (&finished_lock);
		break;
		}
		lock_release (&finished_lock);
    disk_sector_t sector = page_read_ahead_wait();

		struct page_cache *info = get_or_evict_cache (sector);
		lock_release (&info->lock);
	}
}

disk_sector_t
page_read_ahead_wait(void){
  lock_acquire (&read_ahead_lock);
  // lock_acquire (&cache_lock);
  while (list_empty (&read_ahead_queue))
  {
    cond_wait (&read_ahead_cond, &read_ahead_lock);
  }
  struct read_ahead_entry* entry = list_entry (list_pop_front (&read_ahead_queue), struct read_ahead_entry, elem);
  disk_sector_t sector = entry->sector;
  free(entry);
  lock_release (&read_ahead_lock);
  return sector;
}

/* Utilze the Swap out mechanism to implement writeback */
static bool
page_cache_writeback (struct page *page) {
	while (true) 
	{
		lock_acquire (&finished_lock);
		if (finished) {
      lock_release (&finished_lock);
      break;
		}
		lock_release (&finished_lock);
    clock_buffer_cache(true);
		timer_msleep (WRITE_BEIND_PERIOD_MS);
	}
}

/* Destory the page_cache. */
void
page_cache_destroy (struct page *page) {
	lock_acquire (&finished_lock);
	finished = true;
	lock_release (&finished_lock);
  clock_buffer_cache(false);
}

void
clock_buffer_cache(bool is_dirty){
  lock_acquire (&cache_lock);
  bool check;
	for (int i = 0; i < BUFFER_CACHE_NUM; ++i)
	{
    if(is_dirty){
      check = buffer_cache[i].is_valid && buffer_cache[i].dirty;
    }else{
      check = buffer_cache[i].is_valid;
    }
		lock_acquire (&buffer_cache[i].lock);
		if (check) 
		{
      if ((&buffer_cache[i])->dirty)
      {
        disk_write (filesys_disk, (&buffer_cache[i])->sector, (&buffer_cache[i])->buffer);
        (&buffer_cache[i])->dirty = false;
      }
		}
		lock_release (&buffer_cache[i].lock);
	}
  ASSERT(lock_held_by_current_thread(&cache_lock));
	lock_release (&cache_lock);
}

/* Worker thread for page cache */
static void
page_cache_kworkerd (void *aux) {
	thread_create ("cache_read_ahead", 0, page_cache_readahead, NULL);
  thread_create ("cache_write_behind", 0, page_cache_writeback, NULL);
}

static struct page_cache*
get_or_evict_cache (disk_sector_t sector)
{
  lock_acquire (&cache_lock);
  struct page_cache *info = NULL;
  int index = sti_get (sector);
  if (index >= 0 && index < BUFFER_CACHE_NUM)
  {
    info = &buffer_cache[index];
    lock_acquire (&info->lock);
  }
  if (info != NULL){
    lock_release (&cache_lock);
    return info;
  }
  info = NULL;
  if (remain_cache_space != 0)
  {
    for (int i = 0; i < BUFFER_CACHE_NUM; ++i)
    {
      lock_acquire (&buffer_cache[i].lock);
      if (!buffer_cache[i].is_valid)
      {
        struct sector_to_index *entry = malloc (sizeof (struct sector_to_index));
        entry->sector = sector;
        entry->index = i;
        hash_insert (&sti_hash, &entry->helem);
        remain_cache_space -= 1;
        info = &buffer_cache[i];
        break;
      }
      lock_release (&buffer_cache[i].lock);
    }
  }
  if (info == NULL) 
    info = evict_cache (sector);
  info->is_valid = true;
  lock_release (&cache_lock);
  disk_read (filesys_disk, sector, info->buffer);
  info->access = false;
  info->sector = sector;
  info->dirty = false;
  return info;
}

static struct page_cache*
evict_cache (disk_sector_t sector)
{
  static size_t clock = 0;
  while (true)
  {
    lock_acquire (&buffer_cache[clock].lock);
    if (buffer_cache[clock].is_valid == true && buffer_cache[clock].access == true)
    {
      buffer_cache[clock].access = false;
      lock_release (&buffer_cache[clock].lock);
    }
    else if (!buffer_cache[clock].is_valid)
    {
      lock_release (&buffer_cache[clock].lock);
    }
    else
    {
      break;
    }
    clock ++;
    clock %= BUFFER_CACHE_NUM;
  }
  struct page_cache *info = &buffer_cache[clock];
  sti_clear (info->sector);
  sti_set (sector, clock);

  if (info->dirty)
  {
    disk_write (filesys_disk, info->sector, info->buffer);
    info->dirty = false;
  }
  info->is_valid = false;
  return info;
}

static int
sti_get (disk_sector_t sector)
{
  struct sector_to_index temp;
  temp.sector = sector;
  struct hash_elem *elem = hash_find (&sti_hash, &temp.helem);
  if (elem == NULL)
    return -1;
  struct sector_to_index *entry = hash_entry (elem, struct sector_to_index, helem);
  return entry->index;
}

static void 
sti_clear (disk_sector_t sector)
{
  struct sector_to_index temp;
  temp.sector = sector;
  struct hash_elem *removed = hash_delete (&sti_hash, &temp.helem);
  free (hash_entry (removed, struct sector_to_index, helem));
}

static void 
sti_set (disk_sector_t sector, int index)
{
  ASSERT (lock_held_by_current_thread (&cache_lock));
  struct sector_to_index *entry = malloc (sizeof (struct sector_to_index));
  entry->index = index;
  entry->sector = sector;
  struct hash_elem *already = hash_insert (&sti_hash, &entry->helem);
  if (already != NULL)
  {
    struct sector_to_index *entry = hash_entry (already, struct sector_to_index, helem);
  }
  ASSERT (already == NULL);
}
