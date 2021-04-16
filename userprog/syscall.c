#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

//edit
#include "filesys/file.h" //없어도 될듯?
#include "filesys/inode.h"
#include "filesys/filesys.h"


// register uint64_t *num asm ("rax") = (uint64_t *) num_;
//    register uint64_t *a1 asm ("rdi") = (uint64_t *) a1_;
//    register uint64_t *a2 asm ("rsi") = (uint64_t *) a2_;
//    register uint64_t *a3 asm ("rdx") = (uint64_t *) a3_;
//    register uint64_t *a4 asm ("r10") = (uint64_t *) a4_;
//    register uint64_t *a5 asm ("r8") = (uint64_t *) a5_;
//    register uint64_t *a6 asm ("r9") = (uint64_t *) a6_;

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

//start 20180109
void check_addr(const void* sp);
void check_stack_addr(const void* sp, void *va);
// struct semaphore file_lock; /*syncrhonization for file open, create, remove*/
int ans = -1;
static struct lock file_lock;
//end 20180109

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
   write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
         ((uint64_t)SEL_KCSEG) << 32);
   write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

   /* The interrupt service rountine should not serve any interrupts
    * until the syscall_entry swaps the userland stack to the kernel
    * mode stack. Therefore, we masked the FLAG_FL. */
   write_msr(MSR_SYSCALL_MASK,
         FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
   //edit
   // sema_init(&file_lock, 1);
   lock_init(&file_lock);
}



/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {
   // TODO: Your implementation goes here.
   int sys_num = f->R.rax;
   switch (sys_num)
   {
   case SYS_HALT:
      halt();
      break;
   case SYS_EXIT:
      check_addr(f->R.rdi);
      // check_stack_addr(f->rsp, f->R.rdi);
      exit(f->R.rdi);
      break;
   case SYS_FORK:
      check_addr(f->R.rdi);
      // check_stack_addr(f->rsp, f->R.rdi);
      f->R.rax = fork(f->R.rdi, f);
      break;
   case SYS_EXEC:
      check_addr(f->R.rdi);
      // check_stack_addr(f->rsp, f->R.rdi);
      f->R.rax = exec(f->R.rdi);
      break;
   case SYS_WAIT:
      check_addr(f->R.rdi);
      // check_stack_addr(f->rsp, f->R.rdi);
      f->R.rax = wait(f->R.rdi);
      break;
   case SYS_CREATE:
      check_addr(f->R.rdi);
      // check_stack_addr(f->rsp, f->R.rdi);
      check_addr(f->R.rsi);
      // check_stack_addr(f->rsp, f->R.rsi);
      f->R.rax = create(f->R.rdi, f->R.rsi);
      break;
   case SYS_REMOVE:
      check_addr(f->R.rdi);
      // check_stack_addr(f->rsp, f->R.rdi);
      bool temp = remove(f->R.rdi);
      f->R.rax = temp;
      break;
   case SYS_OPEN:
      check_addr(f->R.rdi);
      // check_stack_addr(f->rsp, f->R.rdi);
      ans = open(f->R.rdi);
      f->R.rax = ans;
      break;
   case SYS_FILESIZE:
      // check_stack_addr(f->rsp, f->R.rdi);
      ans = filesize(f->R.rdi);
      f->R.rax = ans;
      break;
   case SYS_READ:
      check_addr(f->R.rdi);
      check_addr(f->R.rsi);
      check_addr(f->R.rdx);
      // check_stack_addr(f->rsp, f->R.rsi);
      // check_stack_addr(f->rsp, f->R.rdi);
      // check_stack_addr(f->rsp, f->R.rdx);
      ans = read(f->R.rdi, f->R.rsi, f->R.rdx);
      f->R.rax = ans;
      break;
   case SYS_WRITE:
      check_addr(f->R.rdi);
      check_addr(f->R.rsi);
      check_addr(f->R.rdx);
      // check_stack_addr(f->rsp, f->R.rsi);
      // check_stack_addr(f->rsp, f->R.rdi);
      // check_stack_addr(f->rsp, f->R.rdx);
      ans = write(f->R.rdi, f->R.rsi, f->R.rdx);
      f->R.rax = ans;
      break;
   case SYS_SEEK:
      check_addr(f->R.rdi);
      check_addr(f->R.rsi);
      // check_stack_addr(f->rsp, f->R.rsi);
      // check_stack_addr(f->rsp, f->R.rdi);
      seek(f->R.rdi, f->R.rsi);
      break;
   case SYS_TELL:
      check_addr(f->R.rdi);
      // check_stack_addr(f->rsp, f->R.rdi);
      f->R.rax = tell(f->R.rdi);
      break;
   case SYS_CLOSE:
      check_addr(f->R.rdi);
      // check_stack_addr(f->rsp, f->R.rdi);
      close(f->R.rdi);
      break;
   }
}

void
halt (void) 
{
   power_off();
}

void
exit (int status) 
{	
   struct thread *t = thread_current();
   printf("%s: exit(%d)\n", t->name, status);
   t->exit_status = status;
   thread_exit();
   /*
  	struct thread *cur = thread_current();
	char real_file_name[128];
	int idx = 0, i;
	while((cur->name)[idx] != ' ' && (cur->name)[idx] != '\0'){
		real_file_name[idx] = (cur->name)[idx];
		idx++;
	}
	real_file_name[idx] = '\0';
	printf("%s: exit(%d)\n", real_file_name, status);
	cur->exit_status = status;

	for (i = 2; i < 128; i++){
		if(cur->fd_table[i] != 0){
			close(i);
		}
	}
	struct thread *temp_thread = NULL;
	struct list_elem *temp_elem = NULL;

	for(temp_elem = list_begin(&thread_current()->child_list);
			temp_elem != list_end(&thread_current()->child_list);
			temp_elem = list_next(temp_elem)){
				temp_thread = list_entry(temp_elem, struct thread, child_elem);

				process_wait(temp_thread->tid);
	}
	thread_exit();
   */
} 

int
fork (const char *thread_name, struct intr_frame *f)
{
   tid_t num = process_fork(thread_name, f);
   return num;
} 

int 
exec (const char *file)
{
   if(file == NULL || *file == NULL) exit(-1);
   tid_t tid = process_exec(file);
   return tid;
}

int 
wait (int pid){
   int ans = process_wait(pid);
   return ans;
}

bool
create (const char *file, unsigned initial_size)
{
   if (file == NULL) exit(-1);
   lock_acquire(&file_lock);
    bool result = (filesys_create (file, initial_size));
   lock_release(&file_lock);
    return result;
}

bool 
remove (const char *file)
{
   if(file==NULL||*file==NULL) exit(-1);
   lock_acquire(&file_lock);
   bool success = filesys_remove(file);
   lock_release(&file_lock);
     return success;
}

int 
open (const char *file)
{
   if(file==NULL) exit(-1);

   struct file* opened_file;
   struct thread *cur =thread_current();
   lock_acquire(&file_lock);
   opened_file = filesys_open (file);
   if(opened_file==NULL){
      lock_release(&file_lock);
      return -1;
   }
   /* if file is current process, deny write */
   if(!strcmp(file,cur->name))
      file_deny_write(opened_file);
   /* number of opened files should be less than 128 */
   /* check vacant room of fd_table */
   for(int i =2;i<128;i++){
      if(cur->fd_table[i]==0){
         cur->fd_table[i]=opened_file;
         lock_release(&file_lock);
         return i;
      }
   }
   lock_release(&file_lock);
   return;
}

int 
filesize (int fd)
{
   lock_acquire(&file_lock);
     struct thread *cur =thread_current();
   if(cur->fd_table[fd] == 0){
      lock_release(&file_lock);
      exit(-1);
   }else{
      lock_release(&file_lock);
      int ret = file_length(cur->fd_table[fd]);
      return ret;
   }
   lock_release(&file_lock);
   return;
}

int 
read (int fd, void *buffer, unsigned length)
{
   check_addr(buffer);
   int cnt = 0;
   struct thread *cur =thread_current();
   lock_acquire(&file_lock);
   if (fd > 1){
      if(cur->fd_table[fd] == 0){
         lock_release(&file_lock);
         exit(-1);
      }
      if(length > 0){
         cnt = file_read(cur->fd_table[fd], buffer, length);
      }
   }else if(fd == 0){
      for(int i=0; i<length; i++){
         if(input_getc() == NULL){
            break;
         }
         cnt++;
      }
   }else{
      lock_release(&file_lock);
      return -1;
   }
   lock_release(&file_lock);
   return cnt;
}

int 
write (int fd, const void *buffer, unsigned length)
{
   /* check the validity of buffer pointer */
   check_addr(buffer);
   int cnt = 0;
   lock_acquire(&file_lock);
   /* Fd 1 means standard output(ex)printf) */
   if(fd == 1){
      putbuf(buffer, length);
      lock_release(&file_lock);
      return length;
   }else{
      if(fd == 0) {
         lock_release(&file_lock);
         return;
      }
      struct thread *cur = thread_current();
      if(cur->fd_table[fd] == 0) {
         lock_release(&file_lock);
         return;
      }
      if(length > 0){
         cnt = file_write(cur->fd_table[fd], buffer, length);
      }
   }
   lock_release(&file_lock);
   return cnt;
}

void
seek (int fd, unsigned position) {
   #ifdef USERPROG
   lock_acquire(&file_lock);
   if (thread_current()->fd_table[fd] == 0) {
      lock_release(&file_lock);
      exit(-1);
   }
   file_seek(thread_current()->fd_table[fd], position);
   lock_release(&file_lock);
   #endif
}

unsigned
tell (int fd) {
   lock_acquire(&file_lock);
   off_t ans = file_tell (thread_current()->fd_table[fd]);
   lock_release(&file_lock);
   return ans;
}

void
close (int fd) {
   if(pml4_get_page (thread_current ()->pml4, fd) == NULL) return;
   struct file *_file;
   struct thread* t = thread_current();
   lock_acquire(&file_lock);
   if(fd<=1) return;
   _file = t->fd_table[fd];
   if(_file==NULL) return;
   file_close(_file);
   lock_release(&file_lock);
   t->fd_table[fd] = 0;
}

void 
check_addr(const void* va)
{
  if(!is_user_vaddr(va)){
    exit(-1);
  }
}

// void 
// check_stack_addr(const void* sp, void *va)
// {
//    if (va == NULL) return;
//    if(!is_user_vaddr(sp) || sp > va) exit(-1);
// }