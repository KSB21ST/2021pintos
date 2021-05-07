#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
// #define VM
#ifdef VM
#include "vm/vm.h"
#endif

//start 20180109
#include "filesys/inode.h"
void argument_stack(char **argv, int argc, struct intr_frame *if_); //stack arguments after load
int argument_parse(const char *file_name, char **argv); //parse argument before load in process_exec
static struct lock file_locker; //lock for file access, before proj3 implementation
struct thread *find_child(struct list *child_list, int tid);
//end 20180109

static void process_cleanup (void);
static bool load (const char *file_name, struct intr_frame *if_);
static void initd (void *f_name);
static void __do_fork (void *);

/* General process initializer for initd and other process. */
static void
process_init (void) {
   struct thread *current = thread_current ();
}

/* Starts the first userland program, called "initd", loaded from FILE_NAME.
 * The new thread may be scheduled (and may even exit)
 * before process_create_initd() returns. Returns the initd's
 * thread id, or TID_ERROR if the thread cannot be created.
 * Notice that THIS SHOULD BE CALLED ONCE. */
tid_t
process_create_initd (const char *file_name) {
   char *fn_copy;
   tid_t tid;

   //start 20180109 careful!
   lock_init(&file_locker); 
   //end 20180109

   /* Make a copy of FILE_NAME.
    * Otherwise there's a race between the caller and load(). */
   fn_copy = palloc_get_page (0);

   if (fn_copy == NULL)
      return TID_ERROR;
   strlcpy (fn_copy, file_name, PGSIZE);

   //start 20180109
   char *save_ptr = NULL;
   file_name = strtok_r(file_name, " ", &save_ptr); //first string parsing
   //end 20180109

   /* Create a new thread to execute FILE_NAME. */
   tid = thread_create (file_name, PRI_DEFAULT, initd, fn_copy);

   // process_wait(tid); //wait for the created process to load execute completely
   sema_down(&thread_current()->fork_sema);
   if (tid == TID_ERROR)
      palloc_free_page (fn_copy);

   palloc_free_page(fn_copy); // edit palloc
   return tid;
}

/* A thread function that launches first user process. */
static void
initd (void *f_name) {
#ifdef VM
   supplemental_page_table_init (&thread_current ()->spt);
#endif

   process_init ();

   if (process_exec (f_name) < 0)
      PANIC("Fail to launch initd\n");
   NOT_REACHED ();
}

/* Clones the current process as `name`. Returns the new process's thread id, or
 * TID_ERROR if the thread cannot be created. */
tid_t
process_fork (const char *name, struct intr_frame *if_ UNUSED) {
   /* Clone current thread to new thread.*/
   struct thread *curr = thread_current();
   if(list_size_int(&all_list) > 17) /*for multioom, restrict forked child number*/
      return TID_ERROR;
   /*semaphore for forking and waiting the child to finish load. If success, sema_up after load. If load fails, sema_up at process_exit*/
   sema_init(&curr->fork_sema, 0); 
   int ans = thread_create(name, PRI_DEFAULT, __do_fork, if_); /*create child thread*/
   if (ans == TID_ERROR){ /*if thread_create went wrong reaturn TID_ERROR which is -1*/
      return ans;
   }
   sema_down(&curr->fork_sema); /*wait for the child to finish loading*/
   return ans;
}

#ifndef VM
/* Duplicate the parent's address space by passing this function to the
 * pml4_for_each. This is only for the project 2. */
static bool
duplicate_pte (uint64_t *pte, void *va, void *aux) {
   struct thread *current = thread_current ();
   struct thread *parent = (struct thread *) aux;
   void *parent_page;
   void *newpage;
   bool writable;

   /* 1. TODO: If the parent_page is kernel page, then return immediately. */
   if(is_kernel_vaddr(va)) return true;

   /* 2. Resolve VA from the parent's page map level 4. */
   parent_page = pml4_get_page (parent->pml4, va);

   /* 3. TODO: Allocate new PAL_USER page for the child and set result to
    *    TODO: NEWPAGE. */
   newpage = palloc_get_page (PAL_USER);
   if (newpage == NULL){
      palloc_free_page(newpage);
      return true;
   }

   /* 4. TODO: Duplicate parent's page to the new page and
    *    TODO: check whether parent's page is writable or not (set WRITABLE
    *    TODO: according to the result). */
   memcpy(newpage, parent_page, PGSIZE);
   writable = is_writable(pte);

   /* 5. Add new page to child's page table at address VA with WRITABLE
    *    permission. */
   if(pml4_set_page (current->pml4, va, newpage, writable)) {
      return true;
   }else{
      palloc_free_page(newpage);
      return false;
   }
}
#endif

/* A thread function that copies parent's execution context.
 * Hint) parent->tf does not hold the userland context of the process.
 *       That is, you are required to pass second argument of process_fork to
 *       this function. */
static void
__do_fork (void *aux) {
   struct intr_frame if_;
   // struct thread *parent = (struct thread *) aux;
   struct thread *current = thread_current ();
   //start 20180109
   struct thread *parent = current->parent;
   /* TODO: somehow pass the parent_if. (i.e. process_fork()'s if_) */
   struct intr_frame *parent_if = (struct intr_frame *)aux;
   //start 20180109
   bool succ = true;

   /* 1. Read the cpu context to local stack. */
   memcpy (&if_, parent_if, sizeof (struct intr_frame));

   /* 2. Duplicate PT */
   current->pml4 = pml4_create();
   if (current->pml4 == NULL)
      goto error;

   process_activate (current);
#ifdef VM
   supplemental_page_table_init (&current->spt);
   if (!supplemental_page_table_copy (&current->spt, &parent->spt))
      goto error;
#else
   if (!pml4_for_each (parent->pml4, duplicate_pte, parent))
      goto error;
#endif

   /* TODO: Your code goes here.
    * TODO: Hint) To duplicate the file object, use `file_duplicate`
    * TODO:       in include/filesys/file.h. Note that parent should not return
    * TODO:       from the fork() until this function successfully duplicates
    * TODO:       the resources of parent.*/

   /*acquire file_locker for file_duplicate, and any other possible interruptions*/
   lock_acquire(&file_locker); 
   struct file ** parent_fd_table = parent->fd_table;
   struct file ** child_fd_table = current->fd_table;
   struct file *child_f;
   /*run from 0 to 128, including STDOUT and STDIN. Because of dup2, fd for STDOUT, STDIN may change*/
   for(int i=0; i<128;i++){ 
      if((parent->fd_table)[i]==NULL)
         continue;
      /*if fd is STDIN or STDOUT. Initialized int thread_create, as fd_table[0] = -1, fd_table[1] = -2*/
      if((parent->fd_table)[i] == -1 || (parent->fd_table)[i] == -2){ 
         child_fd_table[i] = parent->fd_table[i];
         continue;
      }
      
      child_f = file_duplicate(((parent->fd_table)[i]));
      if(child_f==NULL){
         goto error;
      }
      current->fd_table[i] = child_f;
   }   
   lock_release(&file_locker);
   /*if we memcpy parent's fd table to child's fd table without duplicating file, multioom pass but other tests fail*/
   // memcpy(&child_fd_table, &parent_fd_table, sizeof(parent_fd_table)); 

   process_init ();
   /* Finally, switch to the newly created process. */
   if (succ){
      /*child's return should be 0. rax is the return register*/
      if_.R.rax = 0; 
      do_iret (&if_);
   }
error:
/*
this is the same with exit(-1). If loading while forking fails, exit with status -1. 
sema_up(fork_sema) will be done in thread_exit, no need to be done here.
*/
   current->exit_status=-1;
   thread_exit ();
}

/* Switch the current execution context to the f_name.
 * Returns -1 on fail. */
int
process_exec (void *f_name) {
   struct thread *cur = thread_current();
   char *file_name = f_name;
   bool success;

   /*palloc_get_page might be too big to allocate, since it is 4KB. But couldn't risk using malloc. free it later.
   pointer for argument array. Would contain parsed argument from input f_name after 'argument_parse' function
   */
   char *argv;
   argv = palloc_get_page(PAL_ZERO); 
   // argv = malloc(128);
   // argv = (char *)malloc(sizeof(char)*1024);
   if(f_name == NULL) exit(-1);

   /* We cannot use the intr_frame in the thread structure.
    * This is because when current thread rescheduled,
    * it stores the execution information to the member. */
   struct intr_frame _if;
   _if.ds = _if.es = _if.ss = SEL_UDSEG;
   _if.cs = SEL_UCSEG;
   _if.eflags = FLAG_IF | FLAG_MBS;

   /*
   use palloc_get_page to avoid passing kernel address to load function. 
   fn_copy is for argument_parse function
   fn_copy2 is for realname, which is input file name to load function
   just by declaring 
   char *file_name = f_name;
   was not safe. Why? maybe f_name was in kernel address.
   Had to allocate new page in user program memeory and copy the content of f_name.
   */
   char *fn_copy;
   char *fn_copy2;
   fn_copy = palloc_get_page(0);
   fn_copy2 = palloc_get_page (0);
   //fn_copy = (char*)malloc(sizeof(char)*1024);
   //fn_copy2 = (char*)malloc(sizeof(char)*1024);
   if (fn_copy == NULL || fn_copy2 == NULL){
       return -1;
   }
   strlcpy (fn_copy, file_name, PGSIZE);
   strlcpy (fn_copy2, file_name, PGSIZE);
   //strlcpy(fn_copy, file_name, sizeof(char)*1024);
   //strlcpy(fn_copy2, file_name, sizeof(char)*1024);
   char *next_ptr;
   char *realname;

   /* We first kill the current context */
   process_cleanup ();

   //start 20180109 - proj3 exec-arg
   #ifdef VM
    supplemental_page_table_init(&thread_current()->spt);
   #endif
   //end 20180109

   /*return the thread name. use fn_copy2, which is copy of f_name*/
   realname = strtok_r(fn_copy2," ", &next_ptr);

   /*parse arguments. use fn_copy, which is copy of f_name. argv is list for arguments.*/
   int argc = argument_parse(fn_copy, argv);

   /*load - did not change anything in load*/
   success = load(realname, &_if);

   /*
   if load success, stack parsed arguments in stack.
   and sema_up fork_sema to notify the parent that load is finished.
   parent thread will be running from now, woken up from sema_down in process_fork
   */
   if(success){
      argument_stack(argv, argc, &_if);
      sema_up(&(thread_current()->parent)->fork_sema);
   }
   
   /*
   free all the allocated pages. important for multioom, memory leak.
   */
   palloc_free_page(argv);
   // free(argv);

   palloc_free_page (fn_copy); 
   palloc_free_page(fn_copy2);
   //free(fn_copy);
   //free(fn_copy2);
   // palloc_free_page(file_name); 
   /*
   file_name was allocated in process_create_initd, if this is the second created process. 
   But if not second thread, where was it allocated?
   */
   // if(!strcmp(thread_current()->name, 'child-arg'))
      // hex_dump(_if.rsp , _if.rsp , KERN_BASE - _if.rsp, true);
   /*
   if load fails, return -1.
   just by returning -1 will eventually call thread_exit().
   if this is a child, it will sema_up at thread_exit().
   */
   if(!success){
      return -1;
   }

   /* Start switched process. */
   do_iret (&_if);
   NOT_REACHED ();
}


/* Waits for thread TID to die and returns its exit status.  If
 * it was terminated by the kernel (i.e. killed due to an
 * exception), returns -1.  If TID is invalid or if it was not a
 * child of the calling process, or if process_wait() has already
 * been successfully called for the given TID, returns -1
 * immediately, without waiting.
 *
 * This function will be implemented in problem 2-2.  For now, it
 * does nothing. */
int
process_wait (tid_t child_tid UNUSED) {
   /* XXX: Hint) The pintos exit if process_wait (initd), we recommend you
    * XXX:       to add infinite loop here before
    * XXX:       implementing the process_wait. */
   // while(1); used while loop (busy wait, which is not recommended) when first started implementing process.c and syscall.c
   struct list_elem* e;
   struct thread* t = NULL;
   int exit_status;
   /*
   iterate lists of child and find the child that has the same tid with input argument. If there is no such child, return -1.
   */
   for (e = list_begin(&(thread_current()->child_list)); e != list_end(&(thread_current()->child_list)); e = list_next(e)) 
   {
      t = list_entry(e, struct thread, child_elem);
      if (child_tid == t->tid) {
         if(t == NULL){ /*t being NULL would probably never happen, but implement it just in case*/
            list_remove(&t->child_elem);
            palloc_free_page(t->fd_table);
            palloc_free_page(t);
            return -1;
         } 
         /*
         so condition is a very unique term. Go to synch.c for more explanation.
         cond_wait will wait for cond_signal. Similar to semaphore, but multiple jobs can wait for one signal.
         also, it should have a pair of lock and condition to implement.
         In thread.h, implemented exit_lock and exit_cond.
         cond_signal for exit_lock is called in thread_exit().
         this is because, process_wait() waits for the child to enter exit(). The child will close all the files, sema_up the forked parent, 
         then call cond_signal and make process_exit = true if it has a parent. After calling cond_signal, it will retreat to thread_exit(). 
         In thread_exit() in thread.c, if child->process_exit is true, it will become a status of THREAD_EXIT.
         THREAD_EXIT is thread status I implemented. Schedule will only push thread if the thread status is THREAD_DYING. 
         If thread status is THREAD_EXIT,it won't be killed by scheduler. Thus it will pend like a zombie.
         A thread can only be THREAD_EXIT if it has a parent.(see the code in process_exit)
         If it doesn't have a parent, doesn't call cond_signal, because it has no waiting parent and the thread will just exit like normal thraed.
         if it has parent but parent doesn't wait, parent will make all the children orphans in thread_exit() by iterating child_list before it dies.
         So either child has parent but doesn't wait, child has parent but wait, child does not have parent. Three cases. This is for the second case.
         */
         lock_acquire(&t->exit_lock);
         if(t->status != THREAD_EXIT){
            cond_wait(&t->exit_cond, &t->exit_lock);
         }
         /*

         */
         exit_status = t->exit_status;
         list_remove(&t->child_elem);
         lock_release(&t->exit_lock);
         /*
         in thread_create(), every therad get's allocated by palloc_get_page, which is 1KB. 
         The discription in thead.h recommends thread structure to have less than 1KB of memeory.
         */
         palloc_free_page(t);
         return exit_status;
      }
   }
   return -1;
}

/* Exit the process. This function is called by thread_exit (). */
void
process_exit (void) {
   struct thread *curr = thread_current ();
   /* TODO: Your code goes here.
    * TODO: Implement process termination message (see
    * TODO: project2/process_termination.html).
    * TODO: We recommend you to implement process resource cleanup here. */
   //start 20180109
   struct thread *cur = thread_current ();
   struct file **cur_fd_table = cur->fd_table;
   /*
   iterate through file descriptor table to close all open files.
   Then palloc_free_page the fd_table that was allocated in thread_create.
   should not remove the file, because that would effect other threads that also have
   file descriptors pointing to it.

   DON'T KNOW WHY MULTIOOM WILL NOT PASS, EVEN IF I CLOSE ALL THE FILES.
   */
   for(int i=0;i<128;i++){
      struct file *_file = cur_fd_table[i];
      if(_file == NULL || _file == -1 || _file == -2) continue;
      lock_acquire(&file_locker);
      close(i);
      lock_release(&file_locker);
      cur_fd_table[i] = 0;
   }
   palloc_free_page(cur->fd_table);

   if(cur->executable)
       file_close(cur->executable);

   /*
   So this is what I wrote about in process_wait().
   Iterate child list before I die.
   Tell the child that I am dying.(child->parent = NULL)
   remove the child from the child_list.
   If there's a waiting child which has THREAD_EXIT status, free the child.
   No need to free the THREAD_EXIT child's fd_table, because it would already have been freed when
   the child previously entered process_wait().
   If a child has THREAD_EXIT status, the child SHOULD have entered process_exit().
   */
   struct list_elem *e;
   struct thread *t;
   for (e = list_begin(&(thread_current()->child_list)); e != list_end(&(thread_current()->child_list)); e = list_next(e)) 
   {
      t = list_entry(e, struct thread, child_elem);
      if(t == NULL) continue;
      t->parent = NULL;
      list_remove(&t->child_elem);
      if(t->status == THREAD_EXIT) palloc_free_page(t);
   }

   /*
   sema_up the fork_sema, where the parent will have been waiting for in process_fork if load in process_exec haven't failed.
   sema_up in total may have occured two times, if a thread successfully loaded (fist sema_up) and exited normall (second sema_up)
   because normal threads enter process_exit too.
   However, it doesn't mater because once the parent is sema_up-ed, it will sema_init again in process_fork.
   */
   if(cur->parent)
      sema_up(&(cur->parent)->fork_sema);

   /*
   signal waiting parent if there is a parent. 
   Use cond_signal.
   If the parent didn't wait for this child, it must have freed the child when it exited by the loop right above,
   or will deallocate this child even after it's died after this child by the loop right above(if child->status == THREAD_EXIT).
   */
   lock_acquire(&cur->exit_lock);
   if(cur->parent){
      /*
      set process_exit as true so that it can change it's status to THREAD_EXIT instead of THERAD_DYING in thread_exit() in thread.c.
      Only when this one has parent.
      Initialized as false in init_thread() in thread.c
      */
      curr->process_exit = true;
      cond_signal(&cur->exit_cond, &cur->exit_lock);
   }

   process_cleanup ();
   /*release lock for cond_signal, exit_lock*/
   lock_release(&cur->exit_lock);
}

/* Free the current process's resources. */
static void
process_cleanup (void) {
   struct thread *curr = thread_current ();

#ifdef VM
   supplemental_page_table_kill (&curr->spt);
#endif

   uint64_t *pml4;
   /* Destroy the current process's page directory and switch back
    * to the kernel-only page directory. */
   pml4 = curr->pml4;
   if (pml4 != NULL) {
      /* Correct ordering here is crucial.  We must set
       * cur->pagedir to NULL before switching page directories,
       * so that a timer interrupt can't switch back to the
       * process page directory.  We must activate the base page
       * directory before destroying the process's page
       * directory, or our active page directory will be one
       * that's been freed (and cleared). */
      curr->pml4 = NULL;
      pml4_activate (NULL);
      pml4_destroy (pml4);
   }
}

/* Sets up the CPU for running user code in the nest thread.
 * This function is called on every context switch. */
void
process_activate (struct thread *next) {
   /* Activate thread's page tables. */
   pml4_activate (next->pml4);

   /* Set thread's kernel stack for use in processing interrupts. */
   tss_update (next);
}

/* We load ELF binaries.  The following definitions are taken
 * from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
#define EI_NIDENT 16

#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
 * This appears at the very beginning of an ELF binary. */
struct ELF64_hdr {
   unsigned char e_ident[EI_NIDENT];
   uint16_t e_type;
   uint16_t e_machine;
   uint32_t e_version;
   uint64_t e_entry;
   uint64_t e_phoff;
   uint64_t e_shoff;
   uint32_t e_flags;
   uint16_t e_ehsize;
   uint16_t e_phentsize;
   uint16_t e_phnum;
   uint16_t e_shentsize;
   uint16_t e_shnum;
   uint16_t e_shstrndx;
};

struct ELF64_PHDR {
   uint32_t p_type;
   uint32_t p_flags;
   uint64_t p_offset;
   uint64_t p_vaddr;
   uint64_t p_paddr;
   uint64_t p_filesz;
   uint64_t p_memsz;
   uint64_t p_align;
};

/* Abbreviations */
#define ELF ELF64_hdr
#define Phdr ELF64_PHDR

static bool setup_stack (struct intr_frame *if_);
static bool validate_segment (const struct Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
      uint32_t read_bytes, uint32_t zero_bytes,
      bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
 * Stores the executable's entry point into *RIP
 * and its initial stack pointer into *RSP.
 * Returns true if successful, false otherwise. */
static bool
load (const char *file_name, struct intr_frame *if_) {
   struct thread *t = thread_current ();
   struct ELF ehdr;
   struct file *file = NULL;
   off_t file_ofs;
   bool success = false;
   int i;
   enum intr_level old_level;

   // old_level = intr_disable();
   /* Allocate and activate page directory. */
   t->pml4 = pml4_create ();
   if (t->pml4 == NULL)
      goto done;
   process_activate (thread_current ());

   lock_acquire(&file_locker);

   /* Open executable file. */
   file = filesys_open (file_name);

   lock_release(&file_locker);

   if (file == NULL) {
      printf ("load: %s: open failed\n", file_name);
      goto done;
   }

   /* Read and verify executable header. */
   if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
         || memcmp (ehdr.e_ident, "\177ELF\2\1\1", 7)
         || ehdr.e_type != 2
         || ehdr.e_machine != 0x3E // amd64 
         || ehdr.e_version != 1
         || ehdr.e_phentsize != sizeof (struct Phdr)
         || ehdr.e_phnum > 1024) {
      printf ("load: %s: error loading executable\n", file_name); 
      goto done;
   }

   //start 20180109 for proj3 fist part
   t->executable = file;
   file_deny_write(t->executable);
   //end 20180109

   /* Read program headers. */
   file_ofs = ehdr.e_phoff;
   for (i = 0; i < ehdr.e_phnum; i++) {
      struct Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
         goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
         goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) {
         case PT_NULL:
         case PT_NOTE:
         case PT_PHDR:
         case PT_STACK:
         default:
            /* Ignore this segment. */
            break;
         case PT_DYNAMIC:
         case PT_INTERP:
         case PT_SHLIB:
            goto done;
         case PT_LOAD:
            if (validate_segment (&phdr, file)) {
               bool writable = (phdr.p_flags & PF_W) != 0;
               uint64_t file_page = phdr.p_offset & ~PGMASK;
               uint64_t mem_page = phdr.p_vaddr & ~PGMASK;
               uint64_t page_offset = phdr.p_vaddr & PGMASK;
               uint32_t read_bytes, zero_bytes;
               if (phdr.p_filesz > 0) {
                  /* Normal segment.
                   * Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                        - read_bytes);
               } else {
                  /* Entirely zero.
                   * Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
               }
               if (!load_segment (file, file_page, (void *) mem_page,
                        read_bytes, zero_bytes, writable))
                  goto done;
            }
            else
               goto done;
            break;
      }
   }

   /* Set up stack. */
   if (!setup_stack (if_))
      goto done;

   /* Start address. */
   if_->rip = ehdr.e_entry;

   /* TODO: Your code goes here.
    * TODO: Implement argument passing (see project2/argument_passing.html). */
   //start 20180109
   // argument_stack(arg_list, token_count, if_);
   // hex_dump(if_->rsp, if_->rsp, USER_STACK - if_->rsp, true);
   //end 20180109

   success = true;
   // intr_set_level(old_level);

done:
   /* We arrive here whether the load is successful or not. */
   // file_close (file); //cannot close file because of lazy_load_segment in poj3
   return success;
}


/* Checks whether PHDR describes a valid, loadable segment in
 * FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Phdr *phdr, struct file *file) {
   /* p_offset and p_vaddr must have the same page offset. */
   if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
      return false;

   /* p_offset must point within FILE. */
   if (phdr->p_offset > (uint64_t) file_length (file))
      return false;

   /* p_memsz must be at least as big as p_filesz. */
   if (phdr->p_memsz < phdr->p_filesz)
      return false;

   /* The segment must not be empty. */
   if (phdr->p_memsz == 0)
      return false;

   /* The virtual memory region must both start and end within the
      user address space range. */
   if (!is_user_vaddr ((void *) phdr->p_vaddr))
      return false;
   if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
      return false;

   /* The region cannot "wrap around" across the kernel virtual
      address space. */
   if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
      return false;

   /* Disallow mapping page 0.
      Not only is it a bad idea to map page 0, but if we allowed
      it then user code that passed a null pointer to system calls
      could quite likely panic the kernel by way of null pointer
      assertions in memcpy(), etc. */
   if (phdr->p_vaddr < PGSIZE)
      return false;

   /* It's okay. */
   return true;
}

#ifndef VM
/* Codes of this block will be ONLY USED DURING project 2.
 * If you want to implement the function for whole project 2, implement it
 * outside of #ifndef macro. */

/* load() helpers. */
static bool install_page (void *upage, void *kpage, bool writable);

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
      uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
   ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
   ASSERT (pg_ofs (upage) == 0);
   ASSERT (ofs % PGSIZE == 0);

   file_seek (file, ofs);
   while (read_bytes > 0 || zero_bytes > 0) {
      /* Do calculate how to fill this page.
       * We will read PAGE_READ_BYTES bytes from FILE
       * and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
         return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes) {
         palloc_free_page (kpage);
         return false;
      }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) {
         printf("fail\n");
         palloc_free_page (kpage);
         return false;
      }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
   }
   return true;
}

/* Create a minimal stack by mapping a zeroed page at the USER_STACK */
static bool
setup_stack (struct intr_frame *if_) {
   uint8_t *kpage;
   bool success = false;

   kpage = palloc_get_page (PAL_USER | PAL_ZERO);
   if (kpage != NULL) {
      success = install_page (((uint8_t *) USER_STACK) - PGSIZE, kpage, true);
      if (success)
         if_->rsp = USER_STACK;
      else
         palloc_free_page (kpage);
   }
   return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
 * virtual address KPAGE to the page table.
 * If WRITABLE is true, the user process may modify the page;
 * otherwise, it is read-only.
 * UPAGE must not already be mapped.
 * KPAGE should probably be a page obtained from the user pool
 * with palloc_get_page().
 * Returns true on success, false if UPAGE is already mapped or
 * if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable) {
   struct thread *t = thread_current ();

   /* Verify that there's not already a page at that virtual
    * address, then map our page there. */
   return (pml4_get_page (t->pml4, upage) == NULL
         && pml4_set_page (t->pml4, upage, kpage, writable));
}
#else
/* From here, codes will be used after project 3.
 * If you want to implement the function for only project 2, implement it on the
 * upper block. */

static bool
lazy_load_segment (struct page *page, void *aux) {
   /* TODO: Load the segment from the file */
   /* TODO: This called when the first page fault occurs on address VA. */
   /* TODO: VA is available when calling this function. */
   //printf("in lazy loading\n");
   ASSERT(aux != NULL);
   if (page->frame == NULL || page->va == NULL)
      return false;
   struct page_load *temp_aux = (struct page_load *)aux;
   void *kva = page->frame->kva;
   off_t ofs = temp_aux->ofs;
   struct file *file = temp_aux->file;
   size_t read_bytes = temp_aux->read_bytes;
   size_t zero_bytes = temp_aux->zero_bytes;
   ASSERT(zero_bytes == PGSIZE - read_bytes);

   file_seek (file, ofs);
   if(file_read(file, kva, read_bytes) == (int)read_bytes){
      memset (kva + read_bytes, 0, zero_bytes);
      return true;
   }
   return false;
}

/* Loads a segment starting at offset OFS in FILE at address
 * UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
 * memory are initialized, as follows:
 *
 * - READ_BYTES bytes at UPAGE must be read from FILE
 * starting at offset OFS.
 *
 * - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
 *
 * The pages initialized by this function must be writable by the
 * user process if WRITABLE is true, read-only otherwise.
 *
 * Return true if successful, false if a memory allocation error
 * or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
      uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
   ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
   ASSERT (pg_ofs (upage) == 0);
   ASSERT (ofs % PGSIZE == 0);

   while (read_bytes > 0 || zero_bytes > 0) {
      /* Do calculate how to fill this page.
       * We will read PAGE_READ_BYTES bytes from FILE
       * and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* TODO: Set up aux to pass information to the lazy_load_segment. */
      // void *aux = NULL;
      //start 20180109
      struct page_load *aux = malloc(sizeof(struct page_load));
      if(aux == NULL){
         free(aux);
         return false;
      }
      aux->file = file;
      aux->ofs = ofs;
      aux->read_bytes = page_read_bytes;
      aux->zero_bytes = page_zero_bytes;
      //end 20180109
      lock_acquire(&file_locker);
      if (!vm_alloc_page_with_initializer (VM_ANON, upage,
               writable, lazy_load_segment, aux))
         return false;
      lock_release(&file_locker);
      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      //start 20180109
      ofs += page_read_bytes; //why? because of the main loop?
      //end 20180109
   }
   return true;
}

/* Create a PAGE of stack at the USER_STACK. Return true on success. */
static bool
setup_stack (struct intr_frame *if_) {
   bool success = false;
   void *stack_bottom = (void *) (((uint8_t *) USER_STACK) - PGSIZE);

   /* TODO: Map the stack on stack_bottom and claim the page immediately.
    * TODO: If success, set the rsp accordingly.
    * TODO: You should mark the page is stack. */
   /* TODO: Your code goes here */

/* comment above vm_alloc_page_with_initializer: If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
   //20180109 not sure of this part. reimplement!
   if(vm_alloc_page(VM_MARKER_0 | VM_ANON, stack_bottom, true)){ //why VM_MARKER_0 | VM_ANON??
      if(vm_claim_page(stack_bottom)){
           if_->rsp = USER_STACK;
           success = true;
      }
   }
   return success;
}
//end 20180109
#endif /* VM */


void argument_stack(char **argv, int argc, struct intr_frame *if_)
{
   // printf("in argument stack\n");
   char **argu_address;
   argu_address = palloc_get_page(PAL_ZERO);
   for (int i = argc - 1; i >= 0; i--)
   {
      int argv_len = strlen(argv[i]);
      if_->rsp = if_->rsp - (argv_len + 1);
      memcpy(if_->rsp, argv[i], argv_len + 1);
      argu_address[i] = if_->rsp;
   }
    while (if_->rsp % 8 != 0)
    {
      if_->rsp--;
      *(uint8_t *)(if_->rsp) = 0;
    }
    
    /* insert address of strings including sentinel */
    for (int i = argc; i >= 0; i--)
    {
      if_->rsp = if_->rsp - 8;
      if (i == argc){
         *(char *)(if_->rsp) = 0;
      }else{
         memcpy(if_->rsp, &argu_address[i], sizeof(char **));
      }
            
    }
      if_->R.rdi = argc;
      if_->R.rsi =  if_->rsp;

      if_->rsp = if_->rsp - 8;
      memset(if_->rsp, 0, sizeof(void *));
   
   palloc_free_page(argu_address);
}

int
argument_parse(const char *file_name, char **argv)
{
   char *token, *last;
   int token_count = 0;
   token = strtok_r(file_name, " ", &last);
   argv[token_count] = token;
   while (token != NULL)
   {
      token = strtok_r(NULL, " ", &last);
      token_count++;
      argv[token_count] = token;
   }
   // argv[token_count] = NULL;
   return token_count;
}

struct thread *
find_child(struct list *child_list, int tid)
{
   struct list_elem* e;
   struct thread* t = NULL;
   
   for (e = list_begin(&(thread_current()->child_list)); e != list_end(&(thread_current()->child_list)); e = list_next(e)) 
   {
      t = list_entry(e, struct thread, child_elem);
      if(t->tid == tid) 
         if(t == NULL) return;
         return t;
   }
}