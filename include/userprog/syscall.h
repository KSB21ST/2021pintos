#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

// #include "lib/user/syscall.h"

//edit
#include "filesys/file.h" //없어도 될듯?
#include "filesys/inode.h"
#include "threads/interrupt.h"

void syscall_init (void);
void check_user_sp(const void* sp);
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
/* check the validity of user stack pointer */

#endif /* userprog/syscall.h */