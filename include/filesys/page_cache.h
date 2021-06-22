#ifndef FILESYS_PAGE_CACHE_H
#define FILESYS_PAGE_CACHE_H
#include "vm/vm.h"

//for proj4-buffer cache
#include "devices/disk.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "threads/synch.h"
#include "hash.h"

struct page;
enum vm_type;

#define BUFFER_CACHE_NUM 12
#define WRITE_BEIND_PERIOD_MS 1000

struct page_cache {
    bool is_valid;
    disk_sector_t sector;
    bool access;
    bool dirty;
    uint8_t buffer[DISK_SECTOR_SIZE];
    struct lock lock;
};

struct read_ahead_entry {
  disk_sector_t sector;
  struct list_elem elem;
};

struct sector_to_index {
  disk_sector_t sector;
  int index;
  struct hash_elem helem;
};

static struct page_cache buffer_cache[BUFFER_CACHE_NUM];
static struct list read_ahead_queue;
static struct lock read_ahead_lock;
static struct condition read_ahead_cond;
static struct lock cache_lock;
static size_t remain_cache_space;
static struct hash sti_hash; /* sector to index */
static bool finished;
static struct lock finished_lock;

void page_cache_init (void);
bool page_cache_initializer (struct page *page, enum vm_type type, void *kva);
void page_cache_destroy (struct page *page);

static void load_disk (struct page_cache *info, disk_sector_t sector);
static void save_disk (struct page_cache *info);
static struct page_cache* get_or_evict_cache (disk_sector_t);
static struct page_cache* get_cache (disk_sector_t sector);
static struct page_cache* get_empty_cache (disk_sector_t);
static struct page_cache* evict_cache (disk_sector_t);
static void sti_set (disk_sector_t sector, int index);
static int sti_get (disk_sector_t sector);
static void sti_clear (disk_sector_t sector, int index);
static unsigned sti_hash_func(const struct hash_elem *, void *);
static bool sti_less_func(const struct hash_elem *, const struct hash_elem *, void *);
void  page_cache_read(disk_sector_t sector, void *buffer);
void page_cache_write(disk_sector_t sector, void *buffer);
void page_cache_read_len (disk_sector_t sector, void *buffer, off_t offset, off_t len);
void page_cache_write_len (disk_sector_t sector, void *buffer, off_t offset, off_t len);
#endif