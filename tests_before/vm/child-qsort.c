/* Reads a 128 kB file onto the stack and "sorts" the bytes in
   it, using quick sort, a multi-pass divide and conquer
   algorithm.  The sorted data is written back to the same file
   in-place. */

#include <debug.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "tests/vm/qsort.h"

const char *test_name = "child-qsort";

int
main (int argc UNUSED, char *argv[]) 
{
  int handle;
  unsigned char buf[128 * 1024];
  size_t size;

  quiet = true;
  printf("\nbefore open\n");
  CHECK ((handle = open (argv[1])) > 1, "open \"%s\"", argv[1]);
  printf("before read\n");
  size = read (handle, buf, sizeof buf);
  printf("before qsort_bytes\n");
  qsort_bytes (buf, sizeof buf);
  printf("before seek\n");
  seek (handle, 0);
  printf("before write\n");
  write (handle, buf, size);
  printf("before close\n");
  close (handle);
  
  return 72;
}
