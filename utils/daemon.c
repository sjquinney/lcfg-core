/*************************************************************************

  Close filehandles etc to start daemon

*************************************************************************/

#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

/************************************************************************/
  int main ( int argc, char *argv[] )
/************************************************************************/
{
  if ( argc < 2 ) return 1;

  int fd;

  for (fd=3; fd<NOFILE; ++fd) { close(fd); }
  umask(0);
  if ( chdir("/") != 0 ) {
    perror("Failed to change to root directory");
    return 1;
  }

  execvp(argv[1],&argv[1]);

  return 1; /* should not be reached */
}
