#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/user/syscall.h"

void syscall_init (void);
void check_user_sp(const void* sp);
/* check the validity of user stack pointer */

#endif /* userprog/syscall.h */