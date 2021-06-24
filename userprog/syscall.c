#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

//start 20180109
#include "filesys/file.h" //없어도 될듯?
#include "filesys/inode.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "vm/file.h"
#include <hash.h>
//end 20180109
#include "filesys/fat.h"
#include "filesys/page_cache.h"

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
static bool sym_mkdir (const char *dir);
//end 20180109

bool format_scratch = true;

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
      f->R.rax = mount(f->R.rdi, f->R.rsi, f->R.rdx);
      break;
	case SYS_UMOUNT:
      f->R.rax = umount(f->R.rdi);
      break;
   case SYS_MMAP:  /* Map a file into memory. */
//      printf("helloooo\n");
//      check_addr(f->R.rdi);
      f->R.rax =  mmap(f->R.rdi, f->R.rsi, f->R.rdx, f->R.r10, f->R.r8); 
      break;
	case SYS_MUNMAP: /* Remove a memory mapping. */
      check_addr(f->R.rdi);
      munmap(f->R.rdi);
      break;
   //start of proj4
   case SYS_CHDIR:  /* Map a file into memory. */
      f->R.rax =  chdir(f->R.rdi); 
      break;
   case SYS_MKDIR:  /* Map a file into memory. */
      f->R.rax =  mkdir(f->R.rdi); 
      break;
   case SYS_READDIR:  /* Map a file into memory. */
      f->R.rax =  readdir(f->R.rdi, f->R.rsi); 
      break;
   case SYS_ISDIR:  /* Map a file into memory. */
      f->R.rax =  isdir(f->R.rdi); 
      break;
   case SYS_INUMBER:  /* Map a file into memory. */
      f->R.rax =  inumber(f->R.rdi); 
      break;
   case SYS_SYMLINK:  /* Map a file into memory. */
      f->R.rax =  symlink(f->R.rdi, f->R.rsi); 
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
} 

int
fork (const char *thread_name, struct intr_frame *f)
{
//   printf("let's do fork!\n");
   tid_t num = process_fork(thread_name, f);
   return num;
} 

int 
exec (const char *file)
{
   if(file == NULL || *file == NULL){
      exit(-1);
   } 
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
   if(strlen(file) == 0)
      exit(-1);
   // if(!thread_current()->t_sector)
   //    return false;
   lock_acquire(&file_lock);
   bool result = (filesys_create (file, initial_size));
   lock_release(&file_lock);
    return result;
}

void 
hash_file_claim(struct hash_elem *e, void *aux)
{
   struct page* page = hash_entry(e, struct page, h_elem);
   vm_claim_page(page->va);
}
bool 
remove (const char *file)
{
   if(file==NULL||*file==NULL) exit(-1);
   if(strlen(file) == 0)
      exit(-1);
   #ifdef VM
   // struct page *page = spt_find_page(&thread_current()->spt, file);
   // mmap(page->va);
   struct hash *h;
   // h = &(&thread_current()->spt)->spt_table;
   // h->aux = malloc(sizeof(struct file *));
   // h->aux = filesys_open(file);
   hash_apply(&(&thread_current()->spt)->spt_table, hash_file_claim);
   #endif
   lock_acquire(&file_lock);
   bool success = filesys_remove(file);
   lock_release(&file_lock);
   return success;
}

int 
open (const char *file)
{
   if(file==NULL) exit(-1);
   if(strlen(file) == 0)
      return -1;
   if(!strcmp(file, ".") && !thread_current()->t_sector)
      return -1;
   if(!strcmp(file, "..") && !thread_current()->t_sector)
      return -1;

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
         // struct file *t_f = cur->fd_table[i];
         // printf("sector: %d in syscall open\n", t_f->inode->sector);
         // printf("fd: %d name: %s in syscall open\n", i, file);
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
   if(p->writable != true){//edit for cow{
      // printf("here???? \n");
      exit(-1);
   }
   #endif
   // check_addr(buffer);
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
      if(temp == NULL){
         cnt = -1;
      }else{
         cnt = file_read(cur->fd_table[fd], buffer, length);
      }
      // printf("after file_read\n");
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
   // check_addr(buffer);
   struct thread *cur = thread_current();
   int cnt = 0;
   lock_acquire(&file_lock);
   // printf("write file lock acquire, tid: %d\n", thread_current()->tid);
   struct file *temp = cur->fd_table[fd];
   if(temp == -2){
      putbuf(buffer, length);
      cnt = length;
   }else if(temp == -1){
      cnt = -1;
   }else{
      if(temp == NULL){
         cnt = -1;
      }else{
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
   // if(pml4_get_page (thread_current ()->pml4, fd) == NULL) return;
   struct file *_file;
   struct file *inside_file;
   struct thread* t = thread_current();
   struct file **file_table = t->fd_table;
   int cnt; //number of fd who has same file with given argument fd
   _file = file_table[fd];
   if(_file == -1 || _file == -2 || _file == NULL)
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
      t->fd_table[fd] = 0;
      return;
   }else{
      lock_acquire(&file_lock);
      t->open_cnt--;
      if(_file->inode->data._isdir){
         if(_file->inode->data._isscratch){
            dir_close_scratch(_file);
         }else{
            dir_close(_file);
         }
      }else{
         file_close(_file);
      }
      lock_release(&file_lock);
      t->fd_table[fd] = 0;
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
mmap (void *addr, unsigned long int length, int writable, int fd, off_t offset){
   // struct file *map_file = open(thread_current()->fd_table[fd]);
   struct file *map_file = thread_current()->fd_table[fd];
   for(int i=0; i<length; i++){
      if(is_kernel_vaddr(addr+i))
         return NULL;
   }
   if(map_file == NULL || length <= 0 || pg_ofs(addr) != 0 || offset > PGSIZE || addr == NULL){
      return NULL;
   }

   else{
      void *ans = do_mmap(addr, length, writable, map_file, offset);
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

bool
chdir (const char *dir) {
   // printf("start chdir: %s\n", dir);
   uint32_t temp_s = thread_current()->t_sector;
   if(dir == NULL) exit(-1);
   struct dir *last_dir;

   if(!strcmp(dir, "/")){
      last_dir = dir_open_root();
      // printf("[chdir] dir_sector: %d, dir->isscratch: %d, dir->mountpt: %d\n", last_dir->inode->sector, last_dir->inode->data._isscratch, last_dir->inode->data._mountpt);
      thread_current()->t_sector = last_dir->inode->sector;
      thread_current()->isscratch = false;
      return true;
   }
   else{

      char *file_name = palloc_get_page(0);
      char *name_copy = palloc_get_page(0);
      strlcpy(name_copy, dir, PGSIZE);
      struct dir * parent_dir = parse_path(name_copy, file_name);
      if(!parent_dir){
         palloc_free_page(file_name);
         palloc_free_page(name_copy);
         return false;
      }

      struct inode *inode = NULL;
      
      if(parent_dir->inode->data._isscratch){
         // printf("is scratch disk!\n");
         // if(parent_dir->inode->data._mountpt){
         //    dir_close_scratch(parent_dir);
         //    parent_dir = dir_open_scratch(inode_open_scratch(ROOT_DIR_SECTOR));
         // }
         if(!dir_lookup_scratch(parent_dir, file_name, &inode)){
            // printf("something's wrong!\n");
            palloc_free_page(name_copy);
            palloc_free_page(file_name);
            dir_close_scratch(parent_dir);
            return false;
         }
         dir_close_scratch(parent_dir);

         if(!inode->data._isdir){
            // printf("it is not a directory!\n");
            palloc_free_page(name_copy);
            palloc_free_page(file_name);
            dir_close_scratch(parent_dir);
            return false;
         }
         last_dir = dir_open_scratch(inode);
         thread_current()->t_sector = last_dir->inode->sector;
         thread_current()->isscratch = last_dir->inode->data.origin_isscratch;
         temp_s = thread_current()->t_sector;
         palloc_free_page(name_copy);
         palloc_free_page(file_name);
         // printf("ch done\n");

      }else{

         if(!dir_lookup(parent_dir, file_name, &inode)){
            // printf("something's wrong!\n");
            palloc_free_page(name_copy);
            palloc_free_page(file_name);
            dir_close(parent_dir);
            return false;
         }
         dir_close(parent_dir);

         if(!inode->data._isdir){
            // printf("it is not a directory!\n");
            palloc_free_page(name_copy);
            palloc_free_page(file_name);
            dir_close(parent_dir);
            return false;
         }
         last_dir = dir_open(inode);
         thread_current()->t_sector = last_dir->inode->sector;
         thread_current()->isscratch = last_dir->inode->data.origin_isscratch;
         temp_s = thread_current()->t_sector;
         palloc_free_page(name_copy);
         palloc_free_page(file_name);

      }
      return true;
   }
}

bool //very similar with filesys_create
mkdir (const char *dir) {
   // printf("start mkdir: %s\n", dir);
   if(strlen(dir) == 0)
      return false;

   lock_acquire(&file_lock);
   char *file_name = palloc_get_page(0);
   char *name_copy = palloc_get_page(0);
   strlcpy(name_copy, dir, PGSIZE);
   struct dir * t_dir = parse_path(name_copy, file_name);
   if(!t_dir){
      palloc_free_page(file_name);
      palloc_free_page(name_copy);
      lock_release(&file_lock);
      return false;
   }
   lock_release(&file_lock);

   disk_sector_t inode_sector = 0;
   // printf("before if statement\n");
   if(t_dir->inode->data._isscratch){
      if(t_dir->inode->data._mountpt){
         t_dir = dir_open_scratch(inode_open_scratch(ROOT_DIR_SECTOR));
      }
      inode_sector = fat_create_chain_scratch(0);
      bool i_1, i_2;
      // printf("before dir_create\n");
      i_1 = dir_create_scratch (inode_sector, 16);
      // printf("before dir_add\n");
      if(i_1)
         i_2 = dir_add_scratch (t_dir, file_name, inode_sector);
      // printf("after dir_add\n");

      bool success = (t_dir != NULL
            && inode_sector
            && i_1
            && i_2);
      if (!success && inode_sector != 0){
         fat_remove_chain_scratch(inode_sector, 0);
      }
      palloc_free_page(file_name);
      palloc_free_page(name_copy);
      struct inode * t_i = NULL;
      // printf("before dir_open\n");
      struct dir *new_dir = dir_open_scratch(inode_open_scratch(inode_sector));
      if(success == true && new_dir != NULL){
         dir_add_scratch(new_dir, ".", inode_sector);
         dir_add_scratch(new_dir, "..", t_dir->inode->sector);
      }
      dir_close_scratch (new_dir);
      dir_close_scratch (t_dir);
      // printf("mkdir is done\n");
      return success;

   }else{                           // have to handle mount filesys_disk
      
      if(t_dir->inode->data._mountpt){
         t_dir = dir_open_root();
      }

      inode_sector = fat_create_chain(0);
      bool i_1, i_2;
      i_1 = dir_create (inode_sector, 16);
      if(i_1)
         i_2 = dir_add (t_dir, file_name, inode_sector);

      bool success = (t_dir != NULL
            && inode_sector
            && i_1
            && i_2);
      if (!success && inode_sector != 0){
         // dir_remove(t_dir, file_name);
         fat_remove_chain(inode_sector, 0);
      }
      palloc_free_page(file_name);
      palloc_free_page(name_copy);
      struct inode * t_i = NULL;
      struct dir *new_dir = dir_open(inode_open(inode_sector));
      if(success == true && new_dir != NULL){
         dir_add(new_dir, ".", inode_sector);
         dir_add(new_dir, "..", t_dir->inode->sector);
      }
      dir_close (new_dir);
      dir_close (t_dir);
      return success;
   }
}

// readdir (int fd, char name[READDIR_MAX_LEN + 1]) {
bool
readdir (int fd, char *name) {
   struct thread* t = thread_current();
	// if(pml4_get_page (t->pml4, fd) == NULL) return;
   struct file *_file;
   struct file **file_table = t->fd_table;
   _file = file_table[fd];
   return dir_readdir(_file, name);
}

bool
isdir (int fd) {
   struct thread* t = thread_current();
	// if(pml4_get_page (t->pml4, fd) == NULL) return;
   struct file *_file;
   struct file **file_table = t->fd_table;
   _file = file_table[fd];
   if(_file == -1 || _file == -2)
      return;
   struct inode *_inode = inode_open(_file->inode->sector);
   bool isdir = _inode->data._isdir;
   inode_close(_inode);
   return isdir;
}

int
inumber (int fd) {
   struct thread* t = thread_current();
	// if(pml4_get_pag (t->pml4, fd) == NULL) return;
   struct file *_file;
   struct file **file_table = t->fd_table;
   _file = file_table[fd];
   if(_file == -1 || _file == -2)
      return;
   struct inode *_inode = _file->inode;
   return inode_get_inumber(_inode); //disk_sector_t 인데, int 로 캐스팅 안해줘도 됨..??
}

static bool //very similar with filesys_create
sym_mkdir (const char *dir) {
   // printf("dir: %s\n", dir);
   if(strlen(dir) == 0)
      return false;
   disk_sector_t inode_sector = 0;
	inode_sector = fat_create_chain(0);
   
   lock_acquire(&file_lock);
   char *file_name = palloc_get_page(0);
   char *name_copy = palloc_get_page(0);
   strlcpy(name_copy, dir, PGSIZE);
   struct dir *t_dir = parse_path(name_copy, file_name);
   // printf("t_dir->inode: %d, inode_sector: %d\n", t_dir->inode->sector, inode_sector);
   if(!t_dir){
      palloc_free_page(file_name);
      palloc_free_page(name_copy);
      fat_remove_chain(inode_sector, 0);
      return false;
   }
   lock_release(&file_lock);

   bool i_1, i_2;
   i_1 = dir_create (inode_sector, 0);
   if(i_1)
      i_2 = dir_add (t_dir, file_name, inode_sector);

   bool success = (t_dir != NULL
			&& inode_sector
			&& i_1
			&& i_2);

	if (!success && inode_sector != 0){
		// dir_remove(t_dir, file_name);
		fat_remove_chain(inode_sector, 0);
   }
   palloc_free_page(file_name);
   palloc_free_page(name_copy);
	dir_close (t_dir);
	return success;
}

//very similar with mkdir
int
symlink (const char* target, const char* linkpath) {
   if(!sym_mkdir(linkpath)){
      return -1;
   }
   // printf("after make symlink\n");
   // int fd = open(linkpath);
   // disk_sector_t sector = (disk_sector_t)inumber(fd);
   // printf("sector: %d\n", sector);
   // struct inode *symlink = inode_open(sector);
   char *file_name = palloc_get_page(0);
	char *name_copy = palloc_get_page(0);
	strlcpy(name_copy, linkpath, PGSIZE);
   // printf("linkpath: %s\n", linkpath);
	struct dir * parent_dir = parse_path(name_copy, file_name);

   struct inode *symlink;

   dir_lookup(parent_dir, file_name, &symlink);
   if(symlink == NULL){
      // printf("something's wrong!!!\n");
   }
   // printf("parent: %d, symlink: %d\n", parent_dir->inode->sector, symlink->sector);
   // printf("linkpath: %s, sector: %d in symlink\n", linkpath, symlink->sector);

   symlink->data._issym = true;
   symlink->data._isdir = false;
   ASSERT (strlen(target) < 100);
	strlcpy(symlink->data.link_path, target, strlen(target) + 1);

   // disk_write(filesys_disk, cluster_to_sector(symlink->sector), &symlink->data);
   page_cache_write(cluster_to_sector(symlink->sector), &symlink->data);
   dir_close(parent_dir);
   // inode_close(symlink);

   palloc_free_page(name_copy);
   palloc_free_page(file_name);
   return 0;

}

int mount (const char *path, int chan_no, int dev_no){
   if(chan_no == dev_no){
      return -1;
   }

   if(strlen(path) <= 0){
      return -1;
   }

   struct inode *inode = NULL;
   // printf("mount start\n");
	char *path_name = palloc_get_page(0);
	char *path_copy = palloc_get_page(0);
	strlcpy(path_copy, path, PGSIZE);
	struct dir * dir = parse_path(path_copy, path_name);
   // printf("after parse_path, dir sec: %d\n", dir->inode->data._isscratch);
   // printf("dir sec no: %d, name: %s\n", dir->inode->sector, path_name);
   // printf("filesys root, isscratch: %d, mountpt: %d\n", dir->inode->data._isscratch, dir->inode->data._mountpt);
   if(!dir){
		palloc_free_page(path_name);
		palloc_free_page(path_copy);
		return -1;
	}
   // printf("dir_sector: %d, dir->isscratch: %d, dir->mountpt: %d\n", dir->inode->sector, dir->inode->data._isscratch, dir->inode->data._mountpt);

	if (dir->inode->data._isscratch){
      dir_lookup_scratch (dir, path_name, &inode);
      dir_close_scratch (dir);
   }else{
      dir_lookup (dir, path_name, &inode);
      dir_close (dir);
   }

	if (inode == NULL){
      // printf("inode is null\n");
		palloc_free_page(path_name);
		palloc_free_page(path_copy);
		return -1;
	}   

   // printf("before if statement\n");
   if(chan_no == 1 && dev_no == 0){    // scratch disk
      // printf("start of 1, 0\n");
      scratch_disk = disk_get (1, 0);
      if (scratch_disk == NULL)
      	PANIC ("hd1:0 (hdb) not present, scratch disk initialization failed");

      if(format_scratch){
         // printf("here??\n");
         fat_init_scratch ();
         printf ("Formatting file system...");
         fat_create_scratch ();
         // disk_sector_t scratch_root_sec = cluster_to_sector_scratch(ROOT_DIR_CLUSTER);
         if(!dir_create_scratch(ROOT_DIR_SECTOR, 16))
            PANIC("root directory creation failed");
         struct dir *t_dir = dir_open_scratch(inode_open_scratch(ROOT_DIR_SECTOR));
         // printf("\nroot dir open is success!\n");
      	dir_add_scratch (t_dir, ".", ROOT_DIR_SECTOR);
         // printf("before dir_add\n");
         dir_add_scratch (t_dir, "..", ROOT_DIR_SECTOR);
         // struct dir *filesys_root = dir_open_root();
         // printf("filesys_root isscratch: %d\n", filesys_root->inode->data._isscratch);
         // printf("before dir_close \n");
         dir_close_scratch(t_dir);
         // printf("before fat_close\n");

         fat_close_scratch ();
         fat_open_scratch ();
         printf ("done.\n");
         format_scratch = false;
      }else if(mount_cnt[1] == 0){ // scratch fat is already created before.
         // printf("before fat_open_scratch\n");
         fat_open_scratch ();
         // printf("after fat open scratc\n");
         mount_cnt[1]++;
      }else{
         // printf("masaka...\n");
         mount_cnt[1]++;
      }


      struct dir *root = dir_open_scratch(inode_open_scratch(ROOT_DIR_SECTOR));
      // saving original data
      inode->data.origin_start = inode->data.start;
      inode->data.origin_length = inode->data.length;
      inode->data.origin_isdir = inode->data._isdir;
      inode->data.origin_issym = inode->data._issym;
      inode->data.origin_mountpt = inode->data._mountpt;
      inode->data.origin_isscratch = inode->data._isscratch;

      inode->data.start = root->inode->data.start;
      inode->data.length = root->inode->data.length;
      // cluster_t mount_point_clst = fat_create_chain_scratch(0);
      // marking "_mountpt = true && _isscratch = true" at mount point
      inode->data._mountpt = true;
      inode->data._isscratch = true;
      // disk_write(scratch_disk, cluster_to_sector_scratch(mount_point_clst), &inode->data);   // write at scratch_disk
      if(inode->data.origin_isscratch){
         disk_write(scratch_disk, cluster_to_sector_scratch(inode->sector), &inode->data);
         inode_close_scratch(inode);
      }else{
         // disk_write(filesys_disk, cluster_to_sector(inode->sector), &inode->data);              // write at filesys_disk
         page_cache_write(cluster_to_sector(inode->sector), &inode->data);
         inode_close(inode);
      }

      dir_close_scratch(root);


   }else{      // filesys disk

      if(mount_cnt[0] == 0){  // is it possible???
         fat_open();
      }
      mount_cnt[0]++;
      struct dir *root = dir_open_root();

      // saving original data
      inode->data.origin_start = inode->data.start;
      inode->data.origin_length = inode->data.length;
      inode->data.origin_isdir = inode->data._isdir;
      inode->data.origin_issym = inode->data._issym;
      inode->data.origin_mountpt = inode->data._mountpt;
      inode->data.origin_isscratch = inode->data._isscratch;

      inode->data.start = root->inode->data.start;
      inode->data.length = root->inode->data.length;
      inode->data._mountpt = true;
      inode->data._isscratch = false;

      if(inode->data.origin_isscratch){
         disk_write(scratch_disk, cluster_to_sector_scratch(inode->sector), &inode->data);
         inode_close_scratch(inode);
      }else{
         // disk_write(filesys_disk, cluster_to_sector(inode->sector), &inode->data);              // write at filesys_disk
         page_cache_write(cluster_to_sector(inode->sector), &inode->data);  
         inode_close(inode);
      }
      dir_close(root);

   }

   palloc_free_page(path_name);
	palloc_free_page(path_copy);

   return 0;
}


int umount (const char *path){
   // 해당 패스의 inode->data.start랑 등등을 다시 원래대로 바꿔주어야함.
   struct inode *inode = NULL;

	char *path_name = palloc_get_page(0);
	char *path_copy = palloc_get_page(0);
	strlcpy(path_copy, path, PGSIZE);
	struct dir * dir = parse_path(path_copy, path_name);
   // printf("dir sec no: %d, name: %s\n", dir->inode->sector, path_name);
   // printf("filesys root, isscratch: %d, mountpt: %d\n", dir->inode->data._isscratch, dir->inode->data._mountpt);
   if(!dir){
		palloc_free_page(path_name);
		palloc_free_page(path_copy);
		return -1;
	}
   // printf("[umount] dir_sector: %d, dir->isscratch: %d, dir->mountpt: %d\n", dir->inode->sector, dir->inode->data._isscratch, dir->inode->data._mountpt);
	if(dir->inode->data._isscratch){
      dir_lookup_scratch (dir, path_name, &inode);
      dir_close_scratch (dir);
   }else{
      dir_lookup (dir, path_name, &inode);
      dir_close (dir);
   }

	if (inode == NULL){
		palloc_free_page(path_name);
		palloc_free_page(path_copy);
		return -1;
	}
   if(!inode->data._mountpt){
      return -1;
   }

   if(inode->data._isscratch){
      mount_cnt[1]--;
      if(mount_cnt[1] == 0){
         fat_close_scratch();
      }
   }else{
      mount_cnt[0]--;
      if(mount_cnt[0] == 0){
         fat_close();
      }
   }
   inode->data.start = inode->data.origin_start;
   inode->data.length = inode->data.origin_length;
   inode->data._isdir = inode->data.origin_isdir;
   inode->data._issym = inode->data.origin_issym;
   inode->data._mountpt = inode->data.origin_mountpt;
   inode->data._isscratch = inode->data.origin_isscratch; 
   
   if(inode->data._isscratch){
      disk_write(scratch_disk, cluster_to_sector_scratch(inode->sector), &inode->data);
      inode_close_scratch(inode);
   }else{
      // disk_write(filesys_disk, cluster_to_sector(inode->sector), &inode->data);
      page_cache_write(cluster_to_sector(inode->sector), &inode->data);
      inode_close(inode);
   }
   // need to edit for considering disk type later.

   palloc_free_page(path_name);
   palloc_free_page(path_copy);

   return 0;
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
   // for (int i=0;i<size;i++){
      check_addr(buffer);
      struct page *p = spt_find_page(&thread_current()->spt, pg_round_down(buffer));
      if(!p)
         exit(-1);
      // if(writable && !p->writable)
      //    exit(-1);
   // }
}