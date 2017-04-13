#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <fnmatch.h>

#include "packages.h"

LCFGPackageList * lcfgpkglist_match( const LCFGPackageList * pkglist,
                                     const char * name,
                                     const char * arch,
                                     const char * ver,
                                     const char * rel ) {
  assert( pkglist != NULL );

  LCFGPackageList * result = lcfgpkglist_new();

  if ( lcfgpkglist_size(pkglist) == 0 ) return result;

  /* Run the search */

  bool ok = true;

  LCFGPackageNode * cur_node = NULL;
  for ( cur_node = lcfgpkglist_head(pkglist);
        cur_node != NULL && ok;
        cur_node = lcfgpkglist_next(cur_node) ) {

    LCFGPackage * pkg = lcfgpkglist_package(cur_node);

    bool matched = true;

    if ( matched && !isempty(name) ) {
      const char * pkg_name = lcfgpackage_get_name(pkg);
      matched = ( !isempty(pkg_name) && fnmatch( name, pkg_name, 0 ) == 0 );
    }

    if ( matched && !isempty(arch) ) {
      const char * pkg_arch = lcfgpackage_get_arch(pkg);
      matched = ( !isempty(pkg_arch) && fnmatch( arch, pkg_arch, 0 ) == 0 );
    }

    if ( matched && !isempty(ver) ) {
      const char * pkg_ver = lcfgpackage_get_version(pkg);
      matched = ( !isempty(pkg_ver) && fnmatch( ver, pkg_ver, 0 ) == 0 );
    }

    if ( matched && !isempty(rel) ) {
      const char * pkg_rel = lcfgpackage_get_release(pkg);
      matched = ( !isempty(pkg_rel) && fnmatch( rel, pkg_rel, 0 ) == 0 );
    }

    if ( matched ) {
      LCFGChange append_rc = lcfgpkglist_append( result, pkg );
      if ( append_rc == LCFG_CHANGE_ERROR )
        ok = false;
    }

  }

  if ( !ok ) {
    lcfgpkglist_destroy(result);
    result = NULL;
  }

  return result;
}

/* eof */

