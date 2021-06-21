#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

// #include "lib/user/syscall.h"

//edit
#include "filesys/file.h" //없어도 될듯?
#include "filesys/inode.h"
#include "threads/interrupt.h"

void syscall_init (void);
void check_addr(const void* va);
void check_stack_addr(const void* sp, void *va);
void halt (void);
void exit (int status);
int fork(const char *thread_name,struct intr_frame *f);
int exec (const char *cmd_line);
int wait (int pid);
bool create(const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close(int fd);
int dup2 (int oldfd, int newfd);
/* check the validity of user stack pointer */
/* Project 3 and optionally project 4. */
void *mmap (void *addr, unsigned long int length, int writable, int fd, off_t offset);
void munmap (void *addr);

//for proj4
bool chdir (const char *dir);
bool mkdir (const char *);
bool readdir (int, char *);
bool isdir (int fd);
int inumber (int fd);
int symlink (const char* target, const char* linkpath);

int mount (const char *path, int chan_no, int dev_no);
int umount (const char *path);

#endif /* userprog/syscall.h */