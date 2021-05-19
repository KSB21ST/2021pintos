#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

#ifdef VM //changed lazy_load_segment from static to public to include in vm/file.c for do_mmap
bool file_lazy_load_segment (struct page *page, void *aux);
void * aux_load(struct file *file, off_t ofs, uint32_t read_bytes, uint32_t zero_bytes);
#endif

#endif /* userprog/process.h */
