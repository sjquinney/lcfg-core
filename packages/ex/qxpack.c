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

  LCFGPackageSet * pkgset = NULL;
  char * msg = NULL;

  LCFGStatus rc = lcfgpkgset_from_rpmcfg( filename, &pkgset, defarch,
                                          LCFG_OPT_USE_META, &msg );
  bool ok = true;
  if ( rc == LCFG_STATUS_ERROR ) {
    ok = false;
    fprintf( stderr, "Failed to read packages from '%s': %s\n", filename, msg );
  } else {
    LCFGPackageSet * matches = lcfgpkgset_match( pkgset,
                                                 pkg_name,
                                                 pkg_arch,
                                                 pkg_ver,
                                                 pkg_rel );

    if ( matches == NULL ) {
      ok = false;
      fprintf( stderr, "Failed to search packages\n" );
    } else {
      ok = lcfgpkgset_print( matches, defarch, NULL,
                             LCFG_PKG_STYLE_XML, LCFG_OPT_USE_META, stdout );
    }

    lcfgpkgset_relinquish(matches);
  }

  free(msg);

  lcfgpkgset_relinquish(pkgset);

  return ( ok ? 0 : 1 );
}
