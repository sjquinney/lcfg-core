/*************************************************************************

  Close filehandles etc to start daemon

*************************************************************************/

#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

/************************************************************************/
  int main ( int argc, const char *argv[] )
/************************************************************************/
{
  if ( argc < 2 ) return 1;

  int fd;

  for (fd=3; fd<NOFILE; ++fd) { close(fd); }
  umask(0);
  chdir("/");
  execvp(argv[1],&argv[1]);

  return 1; /* should not be reached */
}
