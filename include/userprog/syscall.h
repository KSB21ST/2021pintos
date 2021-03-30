#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"


void syscall_init (void);

/* Map region identifier. */
typedef int off_t;
#define MAP_FAILED ((void *) NULL)

/* Maximum characters in a filename written by readdir(). */
#define READDIR_MAX_LEN 14

/* Typical return values from main() and arguments to exit(). */
#define EXIT_SUCCESS 0          /* Successful execution. */
#define EXIT_FAILURE 1          /* Unsuccessful execution. */

// //start 20180109

struct process{
	pid_t pid;
    struct list children; /*list of child processes*/

	struct list_elem child_elem; /*ist element to go inside child process list of my parent*/
	struct thread *parent; /*my parent thread*/
	int status; /*my state when I exit - status of 0 indicates success and nonzero values indicate errors.*/

	struct semaphore wait_child;
};
//eof 20180109

/* Projects 2 and later. */
void halt (void);
void exit (int status);
pid_t fork (const char *thread_name);
int exec (const char *file);
int wait (pid_t);
bool create (const char *file, unsigned initial_size);
bool remove (const char *file);
int open (const char *file);
int filesize (int fd);
int read (int fd, void *buffer, unsigned length);
int write (int fd, const void *buffer, unsigned length);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);

int dup2(int oldfd, int newfd);

#endif /* userprog/syscall.h */
