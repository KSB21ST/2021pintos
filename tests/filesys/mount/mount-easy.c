#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"


void
test_main (void) 
{
  CHECK (mkdir ("/a"), "mkdir \"/a\"");
  CHECK (mount ("a", 1, 0) == 0, "mount the second disk at \"/a\"");
  CHECK (chdir ("a"), "chdir \"/a\"");
  CHECK (mkdir ("b"), "mkdir \"/a/b\"");
  CHECK (chdir ("b"), "chdir \"/a/b\"");
  CHECK (chdir ("/"), "pop back to \"/\"");
  CHECK (umount ("a") == 0, "unmount the second disk from \"/a\"");
  CHECK (!chdir ("a/b"), "chdir to unmounted directory (must fail)");
  CHECK (mount ("a", 1, 0) == 0, "mount the second disk at \"/a\"");
  CHECK (chdir ("a/b"), "chdir to re-mounted directory");
}


// own test case 1
/*
void
test_main (void)
{
  CHECK (mkdir ("/a"), "mkdir \"/a\"");
  CHECK (mkdir ("/b"), "mkdir \"/b\"");
  CHECK (mkdir ("/c"), "mkdir \"/c\"");
  CHECK (mount ("a", 0, 1) == 0, "mount the filesys disk at \"/a\"");
  CHECK (chdir ("a/b"), "chdir \"/a/b\"");
  CHECK (mkdir ("d"), "mkdir \"/a/b/d\"");
  CHECK (chdir ("d"), "chdir \"/a/b/d\"");
  CHECK (chdir ("/"), "pop back to \"/\"");
  CHECK (umount ("a") == 0, "unmount the filesys disk from \"/a\"");
  CHECK (chdir ("b/d"), "chdir \"/b/d\" (d is made in mount)");
  CHECK (mount ("/a", 0, 1) == 0, "mount the filesys disk at \"/a\"");
  CHECK (chdir ("/a/b/d"), "chdir to remounted directory");
}
*/
// own test case 2
/*
void
test_main (void)
{
  CHECK (mkdir ("/a"), "mkdir \"/a\"");
  CHECK (mkdir ("/b"), "mkdir \"/b\"");
  CHECK (mkdir ("/c"), "mkdir \"/c\"");
  CHECK (mount ("a", 0, 1) == 0, "mount the filesys disk at \"/a\"");
  CHECK (chdir ("a/b"), "chdir \"/a/b\"");
  CHECK (mkdir ("d"), "mkdir \"/a/b/d\"");
  CHECK (chdir ("d"), "chdir \"/a/b/d\"");
  CHECK (chdir ("/"), "pop back to \"/\"");
  CHECK (umount ("a") == 0, "unmount the filesys disk from \"/a\"");
  CHECK (chdir ("/b/d"), "chdir \"/b/d\" (d is made in mount)");
  CHECK (mount ("/a", 0, 1) == 0, "mount the filesys disk at \"/a\"");
  CHECK (chdir ("/a/b/d"), "chdir to remounted directory");
}
*/
/*  need to fix parse_path... 
    /a/a/d should be /a/d 
    but /a/a/d is translated to /d
*/


// own test case 3
/*
void
test_main (void)
{
  CHECK (mkdir ("/a"), "mkdir \"/a\"");
  CHECK (mkdir ("/a/b"), "mkdir \"/a/b\"");
  CHECK (mkdir ("/a/c"), "mkdir \"/a/c\"");
  CHECK (mount ("a", 1, 0) == 0, "mount the scratch disk at \"/a\"");
  CHECK (chdir ("a"), "chdir \"/a\"");
  CHECK (mkdir ("d"), "mkdir \"/a/d\"");
  CHECK (chdir ("d"), "chdir \"/a/d\"");
  CHECK (!chdir ("/a/b"), "chdir \"/a/b\" (must fail)");
  CHECK (chdir ("/"), "pop back to \"/\"");
  CHECK (umount ("a") == 0, "unmount the scratch disk from \"/a\"");
  CHECK (chdir ("/a/b"), "chdir \"/a/b\"");
  CHECK (mount ("/a", 0, 1) == 0, "mount the filesys disk at \"/a\"");
  CHECK (!chdir ("/a/b"), "chdir \"/a/b\" (must fail)");
}
*/

// own test case 4
/*
void
test_main (void) 
{
  int fd;
  CHECK (mkdir ("mount_point"), "mkdir \"mount_point\"");
  CHECK (mount ("mount_point", 1, 0) == 0, "mount \"mount_point\"");
  CHECK (chdir ("mount_point"), "chdir \"mount_point\"");
  CHECK (create ("b", 512), "create \"b\"");
  CHECK (create ("c", 512), "create \"c\"");
  CHECK (remove ("c"), "remove \"c\"");
  CHECK (open ("c") == -1, "open \"c\" fails");
  CHECK (chdir ("/"), "chdir \"/\"");
  CHECK (umount ("mount_point") == 0, "umount \"mount_point\"");
  CHECK (mkdir ("mount_point_2"), "mkdir \"mount_point_2\"");
  CHECK (create ("mount_point_2/a", 512), "create \"a\"");
  CHECK (mount ("mount_point_2", 1, 0) == 0, "mount \"mount_point_2\"");
  CHECK (chdir ("mount_point_2"), "chdir \"mount_point_2\"");
  CHECK ((fd=open ("b")) > 1, "open \"b\"");
  close(fd);
  CHECK (chdir ("/"), "chdir \"/\"");
  CHECK (umount ("mount_point_2") == 0, "umount \"mount_point_2\"");
  CHECK (open ("a") == -1, "open \"a\" fails");

}
*/


// own test case 5
/*
void
test_main (void)
{
  CHECK (mkdir ("/a"), "mkdir \"/a\"");
  CHECK (mount ("a", 1, 1) != 0, "try to mount swap disk (must fail)");
}
*/

// own test case 6
/*
void
test_main (void)
{
  CHECK (mkdir ("/a"), "mkdir \"/a\"");
  CHECK (umount ("a", 1, 0) != 0, "try to umount for not mounted dir (must fail)");
}
*/

// own test case 7
/*
void
test_main (void)
{
  CHECK (mkdir ("a"), "mkdir \"/a\"");
  CHECK (mkdir ("b"), "mkdir \"/b\"");
  CHECK (mkdir ("c"), "mkdir \"/c\"");
  CHECK (mount ("a", 1, 0) == 0, "mount the second disk at \"/a\"");
  CHECK (chdir ("a"), "chdir \"/a\"");
  CHECK (mkdir ("d"), "mkdir \"/a/d\"");
  CHECK (chdir ("d"), "chdir \"/a/d\"");
  CHECK (mount ("/b", 1, 0) == 0, "mount the second disk at \"/b\"");
  CHECK (chdir ("/b"), "chdir \"/b\"");
  CHECK (mkdir ("e"), "mkdir \"/b/e\"");
  CHECK (chdir ("e"), "chdir \"/b/e\"");
  CHECK (chdir ("/a/e"), "chdir \"/a/e\"");
  CHECK (chdir ("/"), "pop back to \"/\"");
  CHECK (umount ("a") == 0, "unmount the second disk from \"/a\"");
  CHECK (chdir ("/b/d"), "chdir \"/b/d\"");
  CHECK (mount ("/a", 1, 0) == 0, "mount the second disk at \"/a\"");
  CHECK (chdir ("/a/d"), "chdir \"/a/d\"");
  CHECK (chdir ("/a/e"), "chdir \"/a/e\"");
  CHECK (chdir ("/b/d"), "chdir \"/b/d\"");
  CHECK (chdir ("/b/e"), "chdir \"/b/e\"");
  
}
*/

// own test case 8
/*
void
test_main (void)
{
  CHECK (mkdir ("a"), "mkdir \"/a\"");
  CHECK (mount ("a", 1, 0) == 0, "mount the second disk at \"/a\"");
  CHECK (chdir ("a"), "chdir \"/a\"");
  CHECK (mkdir ("b"), "mkdir \"/a/b\"");
  CHECK (mount ("b", 0, 1) == 0, "mount the filesys disk at \"/a/b\"");
  CHECK (chdir ("b"), "chdir \"/a/b\"");
  CHECK (mkdir ("c"), "mkdir \"/a/b/c\" (actually at the root of filesys)");
  CHECK (chdir ("/"), "pop back to \"/\"");
  CHECK (umount ("/a/b") == 0, "umount filesys disk at \"/a/b\"");
  CHECK (umount ("/a") == 0, "umount scratch disk at \"/a\"");
  CHECK (chdir ("c"), "must exist dir \"/c\""); // is it right??
}
*/