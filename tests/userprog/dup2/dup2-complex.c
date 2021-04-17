/* This is the final boss of Pintos Project 2.
     
   Written by Minkyu Jung, Jinyoung Oh <cs330_ta@casys.kaist.ac.kr>
*/

#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <syscall.h>
#include <random.h>
#include "tests/lib.h"
#include "tests/userprog/boundary.h"
#include "tests/userprog/sample.inc"

const char *test_name = "dup2-complex";

char magic[] = {
  "Pintos is funny\n"
};

int
main (int argc UNUSED, char *argv[] UNUSED) {
  char *buffer;
  int byte_cnt = 0;
  int fd1, fd2, fd3 = 0x1CE, fd4 = 0x1CE - 0xC0FFEE, fd5, fd6;

  close (0);

  CHECK ((fd1 = open ("sample.txt")) > -1, "open \"sample.txt\"");
  CHECK ((fd2 = open ("sample.txt")) > -1, "open \"sample.txt\"");

  buffer = get_boundary_area () - sizeof sample / 2;

  byte_cnt += read (fd1, buffer + byte_cnt, 10); //fd1 position 10 //////fd1, fd2 sample.txt

  seek (fd2, 10); //fd2 position 10
  byte_cnt += read (fd2, buffer + byte_cnt, 10); //fd2 position 20 ////////fd1, fd2 sample.txt

  CHECK (dup2 (fd2, fd3) > 1, "first dup2()"); //fd3 points to fd2 /////fd1, 2, 3 sample.txt

  byte_cnt += read (fd3, buffer + byte_cnt, 10); //fd2, fd3 position 30

  seek (fd1, 15); //fd1 position 25 (line 36)
  byte_cnt += (read (fd1, buffer + 15, 30) - 15); //fd1 position 55

  dup2 (dup2 (fd3, fd3), dup2 (fd1, fd2)); //fd2 points to fd1 and returns fd2.  //////fd1, 2, 3 sample.txt
  seek (fd2, tell (fd1)); //printf("seek fd2: %d \n", fd2);
  
  byte_cnt += read (fd2, buffer + byte_cnt, 17 + 2 * dup2 (fd4, fd1)); //printf("read fd2: %d \n", fd2); ///////fd1, fd2, fd3, fd4 sample.txt

  close (fd1);  //fd1 null, fd2, 3, 4 sample.txt
  close (fd2);  //fd1, fd2 null fd 3, fd4 sample.txt

  seek (fd3, 60);
  byte_cnt += read (fd3, buffer + byte_cnt, 10); //sample.txt 읽는다

  dup2 (dup2 (fd3, fd2), fd1);  //fd1, fd2, fd3 sample.txt
  byte_cnt += read (fd2, buffer + byte_cnt, 10); //sample.txt 읽는다
  byte_cnt += read (fd1, buffer + byte_cnt, 10); //sample.txt 읽는다

  for (fd5 = 10; fd5 == fd1 || fd5 == fd2 || fd5 == fd3 || fd5 == fd4; fd5++){}
  dup2 (1, fd5); //fd1, fd2, fd3, fd4 sample.txt, fd5 stdout // fd5에 쓰는 모든 게 출력됨 

  write (fd5, magic, sizeof magic - 1); //print "Pintos is funny" (fd5->stdout)

  create ("cheer", sizeof sample);        /*dup(fd, 1) -> printf 하는 모든게 fd에 쓰임 // dup2(1, fd) -> fd에 쓰는 모든 게 output으로 출력됨*/
  create ("up", sizeof sample);
  
  fd4 = open ("cheer"); //fd1, fd2, fd3 sample.txt, fd4 cheer, fd5 stdout
  fd6 = open ("up"); //fd1, fd2, fd3 sample.txt, fd4 cheer, fd5 stdout, fd6 up
  dup2 (fd6, 1); //앞으로 printf로 되는 모든 것들이 fd6에 쓰이게 된다. fd1, fd2, fd3 sample.txt, fd4 cheer, fd5 stdout, fd6 up, stdout fd6 //앞으로 fd6로 출력됨

  msg ("%d", byte_cnt); //fd6 에 출력한다
  snprintf (magic, sizeof magic, "%d", byte_cnt); //stdout buffer에 Pintos is funny  넣는다 -> fd6에 쓰임
  write (fd4, magic, strlen (magic)); //"cheer" file에 "pintos is funny" 쓴다

  pid_t pid;
  if (!(pid = fork ("child"))){ // child
    msg ("child begin");
    close (fd1); //fd1 null, fd2, fd3 sample.txt, fd4 cheer, fd5 stdout, fd6 up, stdout fd6
    close (fd2); //fd1, fd2 null, fd3 sample.txt, fd4 cheer, fd5 stdout, fd6 up, stdout fd6
    dup2 (fd4, fd2); ///fd1 null, fd3 sample.txt, fd2, fd4 cheer, fd5 stdout, fd6 up, stdout fd6
    dup2 (fd3, fd1); //fd1, fd3 sample.txt   fd2, fd4 cheer   fd5 stdout   fd6 up   stdout fd6
    seek (fd2, 0); //seek cheer 
    byte_cnt = read (fd2, magic, 3); //cheer 읽는다
    msg ("%d", byte_cnt); //fd6 에 쓴다
    byte_cnt = atoi (magic); 
    msg ("%d", byte_cnt); //fd6에 쓴다
    
    read (fd1, buffer, 20); //sample.txt 읽는다
    seek (fd4, 0); //cheer 찾는다
    int write_cnt = write (fd4, buffer, 20); //cheer에 쓴다

    byte_cnt += write_cnt;
    close (fd1); 
    close (fd2);
    close (fd3);
    close (fd4);
    close (fd5);
    close (fd6); //fd1, 2, 3, 4, 5, 6 null
    seek (fd4, 0); //null을 찾는다

    msg ("child end");
    exit (byte_cnt);
  } 

  // parent
  int cur_pos = wait (pid); //wait 하기 때문에 child 종료된 후에 실행 // fd1, fd2, fd3 sample.txt, fd4 cheer, fd5 stdout, fd6 up, stdout fd6 
  dup2 (fd5, 1); //fd1, fd2, fd3 sample.txt, fd4 cheer, fd5 1 stdout, fd6 up, 인제 제대로 프린트됨
  seek (fd4, 0); //seek cheer
  byte_cnt += read (fd4, buffer + byte_cnt, 20); //read fd4
  close (fd4); //fd4 null, fd1, fd2, fd3 sample.txt, fd5 1 stdout, fd6 up,

  seek (fd2, cur_pos); //seek sample.txt
  byte_cnt += read (fd2, buffer + byte_cnt , sizeof sample - byte_cnt); //read sample.txt

  seek (1, 0); //seek stdout

  if (strcmp (sample, buffer)) {
    msg ("expected text:\n%s", sample);
    msg ("text actually read:\n%s", buffer);
    fail ("expected text differs from actual");
  } else {
    msg ("Parent success"); //sample, buffer 사이즈 비교해서 똑같으면 성공
    close (1);
  }
}
