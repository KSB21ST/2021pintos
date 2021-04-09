#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"


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
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	// printf ("system call!\n");
	int sys_num = f->R.rax;
	// printf("sys_num: %d \n", sys_num);
	// hex_dump(f->rsp, f->rsp, USER_STACK-f->rsp, true);
	// printf("syscall num : %d %#x\n", *(uint64_t *)(f->R.rax));
	switch (sys_num)
	{
	case SYS_HALT:
		// printf("halt! \n");
		halt();
		break;
	case SYS_EXIT:
		// printf("exit! \n");
		check_user_sp(f->R.rdi);
		exit(f->R.rdi);
		break;
	case SYS_FORK:
		// printf("fork! \n");
		check_user_sp(f->R.rdi);
		f->R.rax = fork(f->R.rdi);
		break;
	case SYS_EXEC:
		// printf("exec \n");
		check_user_sp(f->R.rdi);
		f->R.rax = exec(f->R.rdi);
		break;
	case SYS_WAIT:
		// printf("wait \n");
		check_user_sp(f->R.rdi);
		f->R.rax = wait(f->R.rdi);
		break;
	case SYS_CREATE:
		// printf("create!\n");
		check_user_sp(f->R.rdi);
		check_user_sp(f->R.rsi);
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		// printf("remove \n");
		check_user_sp(f->R.rdi);
		bool temp = remove(f->R.rdi);
		f->R.rax = temp;
		break;
	case SYS_OPEN:
		// printf("open \n");
		check_user_sp(f->R.rdi);
		ans = open(f->R.rdi);
		f->R.rax = ans;
		break;
	case SYS_FILESIZE:
		// printf("filesize \n");
		check_user_sp(f->R.rdi);
		ans = open(f->R.rdi);
		f->R.rax = ans;
		break;
	case SYS_READ:
		check_user_sp(f->R.rdi);
		check_user_sp(f->R.rsi);
		check_user_sp(f->R.rdx);
		// printf("f->R.rdi = %d, f->R.rsi = %d, f->R.rdx= %d\n" ,f->R.rdi,f->R.rsi, f->R.rdx);
		ans = read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		// printf("write \n");
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
	if (file == NULL) exit(-1);
	// sema_down(&file_lock);
    bool result = (filesys_create (file, initial_size));
	// sema_up(&file_lock);
    return result;
}

bool 
remove (const char *file)
{
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
	if(file==NULL) exit(-1);
	struct file* opened_file;
	struct thread *cur =thread_current();
	int fd_num;
	opened_file = filesys_open (file);
	if(opened_file==NULL)
		return -1;
	/* if file is current process, deny write */
	if(!strcmp(file,cur->name))
		file_deny_write(opened_file);
	/* number of opened files should be less than 128 */
	/* check vacant room of fd_table */
	#ifdef USERPROG
	for(int i =2;i<130;i++){
		if(cur->fd[i]==NULL){
			cur->fd[i]=opened_file;
			fd_num=i;
			return fd_num;
		}
	}
	#endif
	return -1;
}

int 
filesize (int fd)
{
  struct thread *cur =thread_current();
//   return file_length(cur->fd_table[fd]);
}

int 
read (int fd, void *buffer, unsigned length)
{
	check_user_sp(buffer);
   
	int cnt = -1;
		char *bf_copy = buffer;
		if (fd == 0){
			cnt = 0;
			while(bf_copy[cnt] != '\0'){
				cnt++;
				if (cnt == length){
				break;
			}
		}
	}
		return cnt;

		return -1;
}

int 
write (int fd, const void *buffer, unsigned length)
{
	/* check the validity of buffer pointer */
	check_user_sp(buffer);
	/* Fd 1 means standard output(ex)printf) */
	if(fd ==1)
		putbuf(buffer, length);
	return length;
}

void 
check_user_sp(const void* sp)
{
  if(!is_user_vaddr(sp)){
	printf("Invalid user sp!");
    exit(-1);
  }
}