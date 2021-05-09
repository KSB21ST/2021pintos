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
#include "vm/file.h"


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
void check_buffer(void *buffer, unsigned size);
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
   //start 20180109 - proj3 stack growth
   thread_current()->rsp = f->rsp;
   //end 20180109
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
      check_addr(f->R.rsi);
      ans = read(f->R.rdi, f->R.rsi, f->R.rdx);
      f->R.rax = ans;
      break;
   case SYS_WRITE:
      check_addr(f->R.rsi);
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
   case SYS_DUP2:
      // check_addr(f->R.rdi);
      check_addr(f->R.rsi);
      f->R.rax = dup2(f->R.rdi, f->R.rsi);
      break;
   case SYS_MOUNT:
      break;
	case SYS_UMOUNT:
      break;
   case SYS_MMAP:  /* Map a file into memory. */
      check_addr(f->R.rdi);
      f->R.rax =  mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8); 
      break;
	case SYS_MUNMAP: /* Remove a memory mapping. */
      check_addr(f->R.rdi);
      munmap(f->R.rdi);
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
   #ifndef VM
   if(cur->open_cnt > 10)
      return-1;
   #endif
   lock_acquire(&file_lock);
   opened_file = filesys_open (file);
   if(opened_file==NULL){
      lock_release(&file_lock);
      return -1;
   }
   cur->open_cnt++;
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
   // remove(opened_file);
   return;
}

int 
filesize (int fd)
{
   struct thread *cur =thread_current();
   if(cur->fd_table[fd] == -1 || cur->fd_table[fd] == -2)
      return;
   lock_acquire(&file_lock);
   if(cur->fd_table[fd] == 0){
      lock_release(&file_lock);
      exit(-1);
   }else{
      int ret = file_length(cur->fd_table[fd]);
      lock_release(&file_lock);
      return ret;
   }
   lock_release(&file_lock);
   return;
}

int 
read (int fd, void *buffer, unsigned length)
{  
   #ifdef VM
   check_buffer(buffer, length);
   struct page *p = spt_find_page(&thread_current()->spt, pg_round_down(buffer));
   if(p->writable != true)
      exit(-1);
   #endif
   check_addr(buffer);
   int cnt = 0;
   struct thread *cur =thread_current();
   lock_acquire(&file_lock);
   struct file *temp = cur->fd_table[fd];
   if(temp == -1){
      for(int i=0; i<length; i++){
         if(input_getc() == NULL){
            break;
         }
         cnt++;
      }
   }else if(temp == -2){
      cnt = -1;
   }else{
      if(temp == NULL)
         cnt = -1;
      cnt = file_read(cur->fd_table[fd], buffer, length);
   }
   lock_release(&file_lock);
   return cnt;
}

int 
write (int fd, const void *buffer, unsigned length)
{
   /* check the validity of buffer pointer */
   // check_buffer(buffer);
   // check_addr(buffer);
   #ifdef VM
   check_buffer(buffer, length);
   #endif
   check_addr(buffer);
   struct thread *cur = thread_current();
   int cnt = 0;
   lock_acquire(&file_lock);
   struct file *temp = cur->fd_table[fd];
   if(temp == -2){
         putbuf(buffer, length);
         cnt = length;
   }else if(temp == -1){
      cnt = NULL;
   }else{
      if(temp == NULL)
         cnt = NULL;
      cnt = file_write(cur->fd_table[fd], buffer, length);
   }
   lock_release(&file_lock);
   return cnt;
}

void
seek (int fd, unsigned position) {
   #ifdef USERPROG
   lock_acquire(&file_lock);
   struct file *temp = thread_current()->fd_table[fd];
   if (thread_current()->fd_table[fd] == 0) {
      lock_release(&file_lock);
      exit(-1);
   }
   if(temp != -1 && temp != -2){
      file_seek(thread_current()->fd_table[fd], position);
      lock_release(&file_lock);
      return;
   }
   lock_release(&file_lock);
   #endif
}

unsigned
tell (int fd) {
   lock_acquire(&file_lock);
   struct file *temp = thread_current()->fd_table[fd];
   if(temp == -1 || temp == -2){
      lock_release(&file_lock);
      return;
   }
   off_t ans = file_tell (thread_current()->fd_table[fd]);
   lock_release(&file_lock);
   return ans;
}

void 
close (int fd) {
   if(pml4_get_page (thread_current ()->pml4, fd) == NULL) return;
   struct file *_file;
   struct file *inside_file;
   struct thread* t = thread_current();
   struct file **file_table = t->fd_table;
   int cnt; //number of fd who has same file with given argument fd
   _file = file_table[fd];
   if(_file == -1 || _file == -2)
      return;
   // lock_acquire(&file_lock);
   // if(fd<=1) return;
   // _file = t->fd_table[fd];
   // if(_file==NULL || _file == -1 || _file == -2) return;
   // file_close(_file);
   // lock_release(&file_lock);
   // t->fd_table[fd] = 0;
   for (int i=0;i<128;i++){
      inside_file = file_table[i];
      if(i == fd) continue;
      if(inside_file == _file)
         cnt++;
   }
   if (cnt > 0){ // _file 을 가리키고 있는 fd가 더 있다는 뜻이다.
      t->fd_table[fd] = NULL;
      return;
   }else{
      lock_acquire(&file_lock);
      t->open_cnt--;
      file_close(_file);
      lock_release(&file_lock);
      t->fd_table[fd] = NULL;
   }
      
}


int 
dup2 (int oldfd, int newfd)
{
   if(!is_user_vaddr(oldfd)){
      return -1;;
   }
   struct thread *t = thread_current();
   struct file *old_file = t->fd_table[oldfd];
   struct file *new_file = t->fd_table[newfd];
   struct file *n_file = NULL;
   if(old_file == new_file)
      return newfd;
   if(old_file == NULL)
      return -1;
   // memcpy(&n_file, &old_file, sizeof(old_file));
   close(newfd); //new_file 이 -1이나 -2이면 0으로 되돌려줘야 하나?
   t->fd_table[newfd] = old_file;

   return newfd;
}

/* Project 3 and optionally project 4. */
/* Map a file into memory. */
void *
mmap (void *addr, unsigned long length, int writable, int fd, off_t offset){
   // struct file *map_file = open(thread_current()->fd_table[fd]);
   struct file *map_file = thread_current()->fd_table[fd];
   if(map_file == NULL || length == 0 || pg_ofs(addr) != 0 || offset > PGSIZE || addr == NULL){
      return NULL;
   }
   else{
      lock_acquire(&file_lock);
      void *ans = do_mmap(addr, length, writable, map_file, offset);
      lock_release(&file_lock);
      return ans;
   }
}

/* Remove a memory mapping. */
void 
munmap (void *addr)
{
   lock_acquire(&file_lock);
   do_munmap(addr);
   lock_release(&file_lock);
}



void 
check_addr(const void* va)
{
  if(!is_user_vaddr(va)){
    exit(-1);
  }
}

void
check_buffer(void *buffer, unsigned size)
{
   for (int i=0;i<size;i++){
      check_addr(buffer);
      struct page *p = spt_find_page(&thread_current()->spt, pg_round_down(buffer));
      if(!p)
         exit(-1);
      // if(writable && !p->writable)
      //    exit(-1);
   }
}