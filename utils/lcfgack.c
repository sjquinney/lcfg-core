/*************************************************************************

  This is a setuid helper program for libmsg. It is used to deliver
  a signal to the LCFG client from any process which is running with
  the appropriate user * or group *.

*************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <fcntl.h>
#include "utils.h"

int main(int argc, char *argv[]) {

  char buf[16];
  struct stat sbuf;
  int count;
  pid_t pid;

  uid_t uid = getuid();
  uid_t gid = getgid(); 

  int fd = open(PIDFILE,O_RDONLY);
  if (fd<0) return 1;
  if (fstat(fd,&sbuf)<0) return 1;
  count = read(fd,buf,15);
  close(fd);
  if (uid!=sbuf.st_uid && gid!=sbuf.st_gid) return 1;
  if (count<1) return 1;
  buf[count]='\0';
  pid = atoi(buf);
  if (pid<=1) return 0;
  if (kill(pid,SIGUSR2)<0) return 1;

  return 0;
}
