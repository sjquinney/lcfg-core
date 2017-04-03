#include <stdio.h>
#include <stdlib.h>

#include <lcfg/packages.h>

int main(int argc, char *argv[]) {

  const char * rpmdir  = argv[1];
  const char * outfile = argv[2];

  LCFGPackageList * pkglist = NULL;
  char * msg = NULL;

  LCFGStatus read_rc = lcfgpkglist_from_rpm_dir( rpmdir, &pkglist, &msg );

  if ( read_rc == LCFG_STATUS_ERROR ) {
    fprintf( stderr, "Failed to read rpm directory '%s': %s\n", rpmdir, msg );
  } else {

    LCFGChange write_rc = lcfgpkglist_to_rpmlist( pkglist,
                                                  NULL,
                                                  outfile, 
                                                  0,
                                                  &msg );

    if ( write_rc == LCFG_CHANGE_ERROR )
      fprintf( stderr, "Failed to write rpmlist '%s': %s\n", outfile, msg );

  }

  lcfgpkglist_destroy(pkglist);
  free(msg);

  return 0;
}
