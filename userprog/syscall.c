// #include "userprog/syscall.h"
// #include <stdio.h>
// #include <syscall-nr.h>
// #include "threads/interrupt.h"
// #include "threads/thread.h"
// #include "threads/loader.h"
// #include "userprog/gdt.h"
// #include "threads/flags.h"
// #include "intrinsic.h"

// //start 20180109
// #include "filesys/filesys.h"
// #include "filesys/file.h"
// //end 2018109

// // register uint64_t *num asm ("rax") = (uint64_t *) num_;
// // 	register uint64_t *a1 asm ("rdi") = (uint64_t *) a1_;
// // 	register uint64_t *a2 asm ("rsi") = (uint64_t *) a2_;
// // 	register uint64_t *a3 asm ("rdx") = (uint64_t *) a3_;
// // 	register uint64_t *a4 asm ("r10") = (uint64_t *) a4_;
// // 	register uint64_t *a5 asm ("r8") = (uint64_t *) a5_;
// // 	register uint64_t *a6 asm ("r9") = (uint64_t *) a6_;

// void syscall_entry (void);
// void syscall_handler (struct intr_frame *);

// //start 20180109
// void check_user_sp(const void* sp);
// struct semaphore file_lock; /*syncrhonization for file open, create, remove*/
// int ans = -1;
// struct lock locker;
// // static void valid_string(const char* string);
// // static int get_user(const uint8_t* uaddr);
// //end 20180109

// /* System call.
//  *
//  * Previously system call services was handled by the interrupt handler
//  * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
//  * efficient path for requesting the system call, the `syscall` instruction.
//  *
//  * The syscall instruction works by reading the values from the the Model
//  * Specific Register (MSR). For the details, see the manual. */

// #define MSR_STAR 0xc0000081         /* Segment selector msr */
// #define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
// #define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

// void
// syscall_init (void) {
// 	//start 20180109
// 	sema_init(&file_lock, 1);
// 	lock_init(&locker);
// 	//end 20180109
// 	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
// 			((uint64_t)SEL_KCSEG) << 32);
// 	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

// 	/* The interrupt service rountine should not serve any interrupts
// 	 * until the syscall_entry swaps the userland stack to the kernel
// 	 * mode stack. Therefore, we masked the FLAG_FL. */
// 	write_msr(MSR_SYSCALL_MASK,
// 			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
// }

// /* The main system call interface */
// void
// syscall_handler (struct intr_frame *f UNUSED) {
// 	// TODO: Your implementation goes here.
// 	// printf ("system call!\n");
// 	int sys_num = f->R.rax;
// 	// printf("sys_num: %d \n", sys_num);
// 	// hex_dump(f->rsp, f->rsp, USER_STACK-f->rsp, true);
// 	// printf("syscall num : %d %#x\n", *(uint64_t *)(f->R.rax));
// 	switch (sys_num)
// 	{
// 	case SYS_HALT:
// 		halt();
// 		break;
// 	case SYS_EXIT:
// 		check_user_sp(f->R.rdi);
// 		exit(f->R.rdi);
// 		break;
// 	case SYS_FORK:
// 		check_user_sp(f->R.rdi);
// 		f->R.rax = fork(f->R.rdi);
// 		break;
// 	case SYS_EXEC:
// 		check_user_sp(f->R.rdi);
// 		f->R.rax = exec(f->R.rdi);
// 		break;
// 	case SYS_WAIT:
// 		check_user_sp(f->R.rdi);
// 		f->R.rax = wait(f->R.rdi);
// 		break;
// 	case SYS_CREATE:
// 		check_user_sp(f->R.rdi);
// 		check_user_sp(f->R.rsi);
// 		if(pml4_get_page (thread_current ()->pml4, f->R.rdi) == NULL) exit(-1);
// 		f->R.rax = create(f->R.rdi, f->R.rsi);
// 		break;
// 	case SYS_REMOVE:
// 		check_user_sp(f->R.rdi);
// 		if(pml4_get_page (thread_current ()->pml4, f->R.rdi) == NULL) exit(-1);
// 		bool temp = remove(f->R.rdi);
// 		f->R.rax = temp;
// 		break;
// 	case SYS_OPEN:
// 		check_user_sp(f->R.rdi);
// 		if(pml4_get_page (thread_current ()->pml4, f->R.rdi) == NULL) exit(-1);
// 		ans = open(f->R.rdi);
// 		f->R.rax = ans;
// 		break;
// 	case SYS_FILESIZE:
// 		check_user_sp(f->R.rdi);
// 		ans = open(f->R.rdi);
// 		f->R.rax = ans;
// 		break;
// 	case SYS_READ:
// 		check_user_sp(f->R.rdi);
// 		check_user_sp(f->R.rsi);
// 		check_user_sp(f->R.rdx);
// 		// if(pml4_get_page (thread_current ()->pml4, f->R.rsi) == NULL) exit(-1);
// 		ans = read(f->R.rdi, f->R.rsi, f->R.rdx);
// 		f->R.rax = ans;
// 		break;
// 	case SYS_WRITE:
// 		check_user_sp(f->R.rdi);
// 		check_user_sp(f->R.rsi);
// 		check_user_sp(f->R.rdx);
// 		ans = write(f->R.rdi, f->R.rsi, f->R.rdx);
// 		f->R.rax = ans;
// 		break;
// 	case SYS_SEEK:
// 		check_user_sp(f->R.rdi);
// 		check_user_sp(f->R.rsi);
// 		seek(f->R.rdi, f->R.rsi);
// 		break;
// 	case SYS_TELL:
// 		check_user_sp(f->R.rdi);
// 		f->R.rax = tell(f->R.rdi);
// 		break;
// 	case SYS_CLOSE:
// 		check_user_sp(f->R.rdi);
// 		close(f->R.rdi);
// 		break;
// 	}
// }

// void
// halt (void) 
// {
// 	power_off();
// }

// void
// exit (int status) 
// {
// 	struct thread *t = thread_current();
// 	#ifdef USERPROG
//     t->exit_status = status;
// 	#endif
// 	printf("%s: exit(%d)\n", t->name, status);
// 	thread_exit();

// }

// pid_t 
// fork (const char *thread_name)
// {
// 	tid_t num = process_fork(thread_name);
// 	return num;
// }

// int 
// exec (const char *file)
// {
// 	tid_t tid = process_exec(file);
//  	return tid;
// }

// int 
// wait (pid_t pid){
// 	int ret = process_wait(pid);
// 	return ret;
// }

// bool 
// create (const char *file, unsigned initial_size)
// {
// 	if (file == NULL) exit(-1);
// 	sema_down(&file_lock);
//     bool result = (filesys_create (file, initial_size));
// 	sema_up(&file_lock);
//     return result;
// }

// bool 
// remove (const char *file)
// {
// 	if(file==NULL||*file==NULL){
//     	exit(-1);
//   	}
//  	sema_down(&file_lock);
//   	bool success = filesys_remove(file);
//   	sema_up(&file_lock);
//   	return success;
// }

// int 
// open (const char *file)
// {
// 	if(file==NULL) return-1;
// 	struct thread *cur =thread_current();
// 	// sema_down(&file_lock);
// 	lock_acquire(&locker);
// 	struct file* opened_file;
// 	opened_file = filesys_open (file);
//     if (opened_file == NULL) {
// 		// sema_up(&file_lock);
// 		lock_release(&locker);
// 		return -1;
// 	}
//     if(!strcmp(thread_current()->name, file)) file_deny_write(opened_file);
// 	if(cur->file_dt==NULL){
// 		// sema_up(&file_lock);
// 		lock_release(&locker);
// 		return -1;
// 	}
// 	cur->file_dt[cur->next_fd] = opened_file;
// 	cur->next_fd += 1;
//     // sema_up(&file_lock);
// 	lock_release(&locker);
//     return cur->next_fd;
// }

// int 
// filesize (int fd)
// {
// 	sema_down(&file_lock);
//   	struct thread *cur =thread_current();
// 	if(fd >= cur->next_fd||fd<=1) return;
// 	struct file *temp = cur->file_dt[fd];
// 	if(temp == NULL) return-1;
// 	int tp = file_length(temp);
// 	sema_up(&file_lock);
// 	return tp;
// }

// int 
// read (int fd, void *buffer, unsigned length)
// {
// 	check_user_sp(buffer);
// 	if(pml4_get_page (thread_current ()->pml4, buffer) == NULL) return;
// 	sema_down(&file_lock);
// 	// lock_acquire(&locker);
// 	if (fd == 0) {
// 		for (int i = 0; i < length; i ++) {
// 			// if (((char *)buffer)[i] == '\0') break;  
// 			*(uint8_t*) (buffer + i) = input_getc();
// 		}	   
// 		// uint8_t ans = input_getc();
// 		sema_up(&file_lock);
// 		// lock_release(&locker);
// 		return length;
// 	} else if (fd >= 2) {
// 		if(fd>= thread_current()->next_fd) {
// 			sema_up(&file_lock);
// 			return;
// 		}
// 		struct file *tmp_file = thread_current()->file_dt[fd];

// 		if(tmp_file == NULL){
// 			sema_up(&file_lock);
// 			// lock_release(&locker);
// 			return;
// 		}

// 		off_t ret = file_read(tmp_file, buffer, length);
// 		sema_up(&file_lock);
// 		// lock_release(&locker);
// 		return ret;
// 	}else{
// 		sema_up(&file_lock);
// 		// lock_release(&locker);
// 		return;
// 	}
// }

// int 
// write (int fd, const void *buffer, unsigned length)
// {
// 	// // /* check the validity of buffer pointer */
// 	check_user_sp(buffer);
// 	if(pml4_get_page (thread_current ()->pml4, buffer) == NULL) return;

// 	struct thread *cur = thread_current();
// 	/* Fd 1 means standard output(ex)printf) */
// 	sema_down(&file_lock);
// 	// lock_acquire(&locker);
// 	if(fd == 1){
// 		putbuf(buffer, length);
// 		sema_up(&file_lock);
// 		// lock_release(&locker);
// 		return length;
// 	}else if(fd == 0){
// 		sema_up(&file_lock);
// 		// lock_release(&locker);
// 		return -1;
// 	}else{
// 		if (!is_user_vaddr(buffer + length))
//         {
//             sema_up(&file_lock);
// 			// lock_release(&locker);
//             exit(-1);
//         }else{
// 			struct file* cur_file = thread_current()->file_dt[fd];
// 			if (cur_file == NULL){
// 				// lock_release(&locker);
// 				sema_up(&file_lock);
// 				return 0;
// 			} 
// 			int ret = file_write(cur->file_dt[fd], buffer, length);
// 			sema_up(&file_lock);
// 			// lock_release(&locker);
// 			return ret;
// 		}
		
// 	}
	
// }

// void
// seek (int fd, unsigned position) {
// 	#ifdef USERPROG
// 	sema_down(&file_lock);
// 	if (thread_current()->file_dt[fd] == NULL) exit(-1);
// 	file_seek(thread_current()->file_dt[fd], position);
// 	sema_up(&file_lock);
// 	#endif
// }

// unsigned
// tell (int fd) {
// 	#ifdef USERPROG
// 	sema_down(&file_lock);
// 	off_t ans = file_tell (thread_current()->file_dt[fd]);
// 	sema_up(&file_lock);
// 	return ans;
// 	#endif
// }

// void
// close (int fd) {
// 	if(pml4_get_page (thread_current ()->pml4, fd) == NULL) return;
// 	struct thread *cur =thread_current();
// 	sema_down(&file_lock);
// 	struct file* file = cur->file_dt[fd];
// 	if (!file) {
// 		if(list_size_int(&cur->child_list)){
// 			cur->file_dt[fd] = NULL;
// 			file_close(file);
// 		}
// 	}
// 	sema_up(&file_lock);
// 	return;
// }

// void 
// check_user_sp(const void* sp)
// {
// //   if(!is_user_vaddr(sp) || sp == NULL || is_kernel_vaddr(sp))
// 	if(is_kernel_vaddr(sp) || sp > 0x47480000)
//   {
// 	printf("Invalid user sp!");
//     exit(-1);
//   }
// }

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
// 	register uint64_t *a1 asm ("rdi") = (uint64_t *) a1_;
// 	register uint64_t *a2 asm ("rsi") = (uint64_t *) a2_;
// 	register uint64_t *a3 asm ("rdx") = (uint64_t *) a3_;
// 	register uint64_t *a4 asm ("r10") = (uint64_t *) a4_;
// 	register uint64_t *a5 asm ("r8") = (uint64_t *) a5_;
// 	register uint64_t *a6 asm ("r9") = (uint64_t *) a6_;

void syscall_entry (void);
void syscall_handler (struct intr_frame *);

//start 20180109
void check_user_sp(const void* sp);
struct semaphore file_lock; /*syncrhonization for file open, create, remove*/
int ans = -1;
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
	sema_init(&file_lock, 1);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	//printf ("system call!\n");
	int sys_num = f->R.rax;
	//printf("sys_num: %d \n", sys_num);
	// hex_dump(f->rsp, f->rsp, USER_STACK-f->rsp, true);
	// printf("syscall num : %d %#x\n", *(uint64_t *)(f->R.rax));
	switch (sys_num)
	{
	case SYS_HALT:
		//printf("halt! \n");
		halt();
		break;
	case SYS_EXIT:
		//printf("exit! \n");
		check_user_sp(f->R.rdi);
		exit(f->R.rdi);
		break;
	case SYS_FORK:
		//printf("fork! \n");
		check_user_sp(f->R.rdi);
		f->R.rax = fork(f->R.rdi);
		break;
	case SYS_EXEC:
		//printf("exec \n");
		check_user_sp(f->R.rdi);
		f->R.rax = exec(f->R.rdi);
		break;
	case SYS_WAIT:
		//printf("wait \n");
		check_user_sp(f->R.rdi);
		f->R.rax = wait(f->R.rdi);
		break;
	case SYS_CREATE:
		//printf("create!\n");
		check_user_sp(f->R.rdi);
		check_user_sp(f->R.rsi);
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		//printf("remove \n");
		check_user_sp(f->R.rdi);
		bool temp = remove(f->R.rdi);
		f->R.rax = temp;
		break;
	case SYS_OPEN:
		//printf("open \n");
		check_user_sp(f->R.rdi);
		ans = open(f->R.rdi);
		f->R.rax = ans;
		break;
	case SYS_FILESIZE:
		//printf("filesize \n");
		ans = filesize(f->R.rdi);
		f->R.rax = ans;
		break;
	case SYS_READ:
		//printf("read \n");
		check_user_sp(f->R.rdi);
		check_user_sp(f->R.rsi);
		check_user_sp(f->R.rdx);
		// printf("f->R.rdi = %d, f->R.rsi = %d, f->R.rdx= %d\n" ,f->R.rdi,f->R.rsi, f->R.rdx);
		ans = read(f->R.rdi, f->R.rsi, f->R.rdx);
		f->R.rax = ans;
		break;
	case SYS_WRITE:
		//printf("write \n");
		check_user_sp(f->R.rdi);
		check_user_sp(f->R.rsi);
		check_user_sp(f->R.rdx);
		// printf("f->R.rdi = %d, f->R.rsi = %d, f->R.rdx= %d\n" ,f->R.rdi,f->R.rsi, f->R.rdx);
		ans = write(f->R.rdi, f->R.rsi, f->R.rdx);
		f->R.rax = ans;
		break;
	case SYS_SEEK:
		break;
	case SYS_TELL:
		break;
	case SYS_CLOSE:
		break;
	}
	//printf("syscall is done\n");
 
	// thread_exit();
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
	#ifdef USERPROG
    t->exit_status = status;
	#endif
	printf("%s: exit(%d)\n", t->name, status);
	thread_exit();
}

pid_t 
fork (const char *thread_name)
{
	tid_t num = process_fork(thread_name);
}

int 
exec (const char *file)
{
	tid_t tid = process_exec(file);
 	return tid;
}

int 
wait (pid_t pid){
	int ans = process_wait(pid);
	return ans;
}

bool 
create (const char *file, unsigned initial_size)
{
	//printf("in create\n");
	if (file == NULL) exit(-1);
	// sema_down(&file_lock);
    bool result = (filesys_create (file, initial_size));
	// sema_up(&file_lock);
    return result;
}

bool 
remove (const char *file)
{
	//printf("in remove\n");
	if(file==NULL||*file==NULL){
    	exit(-1);
  	}
 	// sema_down(&file_lock);
  	bool success = filesys_remove(file);
  	// sema_up(&file_lock);
  	return success;
}

int 
open (const char *file)
{
	//printf("in open\n");
	if(file==NULL) exit(-1);

	//sema_down(&file_lock);
	struct file* opened_file;
	struct thread *cur =thread_current();
	int fd_num;
	opened_file = filesys_open (file);
	//printf("file pointer: %#x\n", opened_file);
	if(opened_file==NULL)
		//sema_up(&file_lock);
		return -1;
	/* if file is current process, deny write */
	if(!strcmp(file,cur->name))
		file_deny_write(opened_file);
	/* number of opened files should be less than 128 */
	/* check vacant room of fd_table */
	#ifdef USERPROG
		for(int i =2;i<128;i++){
			if(cur->fd_table[i]==NULL){
				cur->fd_table[i]=opened_file;
				fd_num=i;
				//printf("fd in open syscall: %d \n", fd_num);
				//sema_up(&file_lock);
				return fd_num;
			}
		}
	#endif
	//sema_up(&file_lock);
	return -1;
}

int 
filesize (int fd)
{
	//printf("fd in filesize syscall: %d \n", fd);
  	struct thread *cur =thread_current();
	if(cur->fd_table[fd] == NULL){
		exit(-1);
	}else{
		return file_length(cur->fd_table[fd]);
	}
}

int 
read (int fd, void *buffer, unsigned length)
{
	check_user_sp(buffer);
	int cnt = 0;
	struct thread *cur =thread_current();
	sema_down(&file_lock);
	//printf("fd: %d\n", fd);
	if (fd > 1){
		if(cur->fd_table[fd] == NULL){
			sema_up(&file_lock);
			exit(-1);
		}
		//printf("REACHED??\n");
		//cnt = length;
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
	}
	sema_up(&file_lock);

	return cnt;

}

int 
write (int fd, const void *buffer, unsigned length)
{
	/* check the validity of buffer pointer */
	check_user_sp(buffer);
	int cnt = 0;
	/* Fd 1 means standard output(ex)printf) */
	if(fd == 1){
		putbuf(buffer, length);
		return length;
	}else{
		struct thread *cur = thread_current();
		if(length > 0){
			cnt = file_write(cur->fd_table[fd], buffer, length);
		}
	}
	return cnt;
}

void 
check_user_sp(const void* sp)
{
  if(!is_user_vaddr(sp)){
	printf("Invalid user sp!");
    exit(-1);
  }
}