#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include <lcfg/packages.h>

int main (int argc, char *argv[]) {
  const char * filename = argv[1];

  const char * pkg_name = argc > 2 ? argv[2] : NULL;
  const char * pkg_arch = argc > 3 ? argv[3] : NULL;
  const char * pkg_ver  = argc > 4 ? argv[4] : NULL;
  const char * pkg_rel  = argc > 5 ? argv[5] : NULL;

  const char * defarch = default_architecture();

  LCFGPackageList * pkglist = NULL;
  char * msg = NULL;

  LCFGStatus rc = lcfgpkglist_from_cpp( filename, &pkglist, defarch, 0,
                                        &msg );
  bool ok = true;
  if ( rc == LCFG_STATUS_ERROR ) {
    ok = false;
    fprintf( stderr, "Failed to read packages from '%s': %s\n", filename, msg );
  } else {
    LCFGPackageList * matches = lcfgpkglist_match( pkglist,
                                                   pkg_name,
                                                   pkg_arch,
                                                   pkg_ver,
                                                   pkg_rel );

    if ( matches == NULL ) {
      ok = false;
      fprintf( stderr, "Failed to search packages\n" );
    } else {
      ok = lcfgpkglist_print( matches, defarch, LCFG_PKG_STYLE_RPM, 0, stdout );
    }
  }

  free(msg);

  return ( ok ? 0 : 1 );
}
