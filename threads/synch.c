/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

//edit
bool cmp_max_wait_pri(struct list_elem * a, struct list_elem * b, void *aux);
#define max(a,b)  (((a) > (b)) ? (a) : (b))
bool compare_priority2(struct list_elem * a, struct list_elem * b, void *aux);
void rec_donate_pri(struct thread *);
void donate_priority(struct lock *);

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
   ASSERT (sema != NULL);

   sema->value = value;
   list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
void
sema_down (struct semaphore *sema) {
   enum intr_level old_level;

   ASSERT (sema != NULL);
   ASSERT (!intr_context ());

   old_level = intr_disable ();
   while (sema->value == 0) {
      // printf("\n thread %s priority %d \n", thread_name(), thread_get_priority());
      list_push_back (&sema->waiters, &thread_current ()->elem);
      thread_block ();
   }
   sema->value--;
   intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
   enum intr_level old_level;
   bool success;

   ASSERT (sema != NULL);

   old_level = intr_disable ();
   if (sema->value > 0)
   {
      sema->value--;
      success = true;
   }
   else
      success = false;
   intr_set_level (old_level);

   return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) {
   enum intr_level old_level;

   ASSERT (sema != NULL);

   old_level = intr_disable ();
   if (!list_empty (&sema->waiters)){
      //edit
      list_sort(&sema->waiters, &compare_priority, NULL);

      thread_unblock (list_entry (list_pop_front (&sema->waiters),
               struct thread, elem));     
      
   }
   sema->value++;
   intr_set_level (old_level);
   thread_yield();
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
   struct semaphore sema[2];
   int i;

   printf ("Testing semaphores...");
   sema_init (&sema[0], 0);
   sema_init (&sema[1], 0);
   thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
   for (i = 0; i < 10; i++)
   {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
   }
   printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
   struct semaphore *sema = sema_;
   int i;

   for (i = 0; i < 10; i++)
   {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
   }
}
 
/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock) {
   ASSERT (lock != NULL);

   lock->holder = NULL;

   sema_init (&lock->semaphore, 1);

   //edit
   lock->max_pri = 0;
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock) {
   ASSERT (lock != NULL);
   ASSERT (!intr_context ());
   ASSERT (!lock_held_by_current_thread (lock));


   //edit-mlfqs
   if(thread_mlfqs){
      sema_down(&lock->semaphore);
      lock->holder = thread_current ();
      return;
   }

   //edit
   struct semaphore *sema = &lock->semaphore;
   //when the current thread tries to acquire lock, but the lock is owned by thread with smaller priority
   if( (lock->holder != NULL) && (thread_get_priority() > lock->holder->priority)){ 
      /*for donate-priority-nest: acquire 하려는 lock을 가지고 있는 holder가
      기다리고 있는 lock 이 있다면 그 lock의 holder에게도 priority를 donate 해준다*/

      list_push_back(&thread_current()->locks_wait, &lock->w_elem);
      rec_donate_pri(thread_current());
      donate_priority(lock);
      thread_yield();
   }      
   sema_down(&lock->semaphore);
   //if the current thread can successfully acquire the lock:
   lock->max_pri = thread_get_priority();
   lock->holder = thread_current ();
   list_push_back(&thread_current()->locks_have, &lock->elem);  //push this lock to the list of locks that this thread has
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
   bool success;

   ASSERT (lock != NULL);
   ASSERT (!lock_held_by_current_thread (lock));
   success = sema_try_down (&lock->semaphore);
   //edit
   if (success)
      lock->holder = thread_current ();
   return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) {
   ASSERT (lock != NULL);
   ASSERT (lock_held_by_current_thread (lock));

   //edit-mlfqs
   if(thread_mlfqs){
      lock->holder = NULL;
      sema_up (&lock->semaphore);
      return;
   }

   struct thread *lock_holder = lock->holder;
   struct list *lock_list = &lock_holder->locks_have;
   //edit
   /*release the locks, but if there is another lock that this thread
   is holding that donated a bigger priority than the original priority of the thread,
   thread should be that priority.
   */
   list_remove(&lock->elem);
   lock->max_pri = 0;
   lock->holder = NULL;
   if (!list_empty (&(&lock->semaphore)->waiters)){
      //edit
      list_remove(&lock->w_elem);
   }
   sema_up (&lock->semaphore);
   int most_max_pri = 0;

   if(!list_empty(lock_list)){
      struct list_elem *e ;

      for (e = list_begin (lock_list); e != list_end (lock_list); e = list_next (e)) {
         struct lock *tp_lock = list_entry (e, struct lock, elem);
         struct semaphore *tp_sema = &tp_lock->semaphore;
         struct list *lock_waiters = &tp_sema->waiters;
         if(!list_empty(lock_waiters)){
            struct thread *tp_thread = list_entry(list_max(&tp_sema->waiters, &compare_priority, NULL), struct thread, elem);
            int tp_max_pri = tp_thread->priority;
            if (most_max_pri < tp_max_pri){
               most_max_pri = tp_max_pri;
            }
         }
      }  
         // struct lock *tp_lock = list_entry (e, struct lock, elem);
         // int tp_max_pri = tp_lock->max_pri;
         // if (most_max_pri < tp_max_pri){
         //    most_max_pri = tp_max_pri;
         // }
      }

   if(lock_holder->original_priority != -1){
      if(lock_holder->original_priority < most_max_pri){
         lock_holder->priority = most_max_pri;
      }else{
         lock_holder->priority = lock_holder->original_priority;
         lock_holder->dontaed_priority = false;
         lock_holder->original_priority = -1;
      }
   }else{
      lock_holder->dontaed_priority = false;
   }                                                                       
   thread_yield();
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
   ASSERT (lock != NULL);

   return lock->holder == thread_current ();
}
 
/* One semaphore in a list. */
struct semaphore_elem {
   struct list_elem elem;              /* List element. */
   struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
   ASSERT (cond != NULL);

   list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) {
   struct semaphore_elem waiter;

   ASSERT (cond != NULL);
   ASSERT (lock != NULL);
   ASSERT (!intr_context ());
   ASSERT (lock_held_by_current_thread (lock));

   sema_init (&waiter.semaphore, 0);
   list_push_back (&cond->waiters, &waiter.elem);
   lock_release (lock);
   sema_down (&waiter.semaphore);
   lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
   ASSERT (cond != NULL);
   ASSERT (lock != NULL);
   ASSERT (!intr_context ());
   ASSERT (lock_held_by_current_thread (lock));
   
   if (!list_empty (&cond->waiters)){
      //edit
      list_sort(&cond->waiters, &compare_priority2, NULL);
      sema_up (&list_entry (list_pop_front (&cond->waiters),
               struct semaphore_elem, elem)->semaphore);
   }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
   ASSERT (cond != NULL);
   ASSERT (lock != NULL);

   while (!list_empty (&cond->waiters))
      cond_signal (cond, lock);
}

//edit
bool compare_priority2(struct list_elem * a, struct list_elem * b, void *aux){
   struct semaphore_elem *temp1 = list_entry(a, struct semaphore_elem, elem);
   struct semaphore_elem *temp2 = list_entry(b, struct semaphore_elem, elem);
   struct thread *temp1_thread = list_entry(list_front(&(&temp1->semaphore)->waiters), struct thread, elem);
   struct thread *temp2_thread = list_entry(list_front(&(&temp2->semaphore)->waiters), struct thread, elem);
   if((temp1_thread->priority) > (temp2_thread->priority)){
      return true;
   }else{
      return false;
   }
}

//edit
void rec_donate_pri(struct thread *t){
   struct list *waiting_l = &t->locks_wait;
   if(!list_empty(waiting_l)){
      struct list_elem *e;
      for (e = list_begin (waiting_l); e != list_end (waiting_l); e = list_next (e)) {
         struct lock *tp_lock = list_entry(e, struct lock, w_elem); 
         struct thread *lock_holder = tp_lock->holder;
         donate_priority(tp_lock);

         //recursively donate priority
         rec_donate_pri(lock_holder);
      }
   return;
   }
}

//edit
void donate_priority(struct lock *lock){
   struct thread *lock_holder = lock->holder;
   if(lock_holder != NULL){
      if(lock_holder->original_priority == -1){
         lock_holder->original_priority = lock_holder->priority;
      }
      lock_holder->priority = thread_get_priority();
      lock_holder->dontaed_priority = true;
      lock->max_pri = thread_get_priority();
   }
}