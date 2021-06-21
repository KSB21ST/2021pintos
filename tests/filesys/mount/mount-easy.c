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
}*/

// own test case 2 (failed)
/*
void
test_main (void)
{
  CHECK (mkdir ("/a"), "mkdir \"/a\"");
  CHECK (mkdir ("/b"), "mkdir \"/b\"");
  CHECK (mkdir ("/c"), "mkdir \"/c\"");
  CHECK (mount ("a", 0, 1) == 0, "mount the filesys disk at \"/a\"");
  CHECK (chdir ("a/a"), "chdir \"/a/a\"");
  CHECK (mkdir ("d"), "mkdir \"/a/a/d\"");
  CHECK (chdir ("d"), "chdir \"/a/a/d\"");
  CHECK (chdir ("/"), "pop back to \"/\"");
  CHECK (umount ("a") == 0, "unmount the filesys disk from \"/a\"");
  CHECK (chdir ("/a/d"), "chdir \"/a/d\" (d is made in mount)");
  CHECK (mount ("/a", 0, 1) == 0, "mount the filesys disk at \"/a\"");
  CHECK (chdir ("/a/a/d"), "chdir to remounted directory");
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

// own test case 4 (failed)
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