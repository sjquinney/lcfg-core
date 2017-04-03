
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "packages.h"

LCFGPackageIterator * lcfgpkgiter_new( LCFGPackageList * pkglist ) {

  LCFGPackageIterator * iterator = malloc( sizeof(LCFGPackageIterator) );
  if ( iterator == NULL ) {
    perror( "Failed to allocate memory for LCFG package iterator" );
    exit(EXIT_FAILURE);
  }

  lcfgpackagelist_acquire(pkglist);

  iterator->pkglist = pkglist;
  iterator->current = NULL;
  iterator->done    = false;

  return iterator;
}

void lcfgpkgiter_destroy( LCFGPackageIterator * iterator ) {

  if ( iterator == NULL ) return;

  lcfgpkglist_relinquish(iterator->pkglist);

  iterator->pkglist = NULL;
  iterator->current = NULL;

  free(iterator);
  iterator = NULL;

}

void lcfgpkgiter_reset( LCFGPackageIterator * iterator ) {
  iterator->current = NULL;
  iterator->done    = false;
}

bool lcfgpkgiter_has_next( LCFGPackageIterator * iterator ) {

  bool has_next = false;

  if ( !iterator->done ) {

    /* not yet started and something available */
    if ( iterator->current == NULL &&
         iterator->pkglist != NULL &&
         lcfgpkglist_size(iterator->pkglist) > 0 ) {
      has_next = true;
    } else {

      /* started and more to come */
      if ( iterator->current != NULL &&
           lcfgpkglist_next(iterator->current) != NULL ) {
        has_next = true;
      }
    }

  }

  return has_next;
}

LCFGPackage * lcfgpkgiter_next(LCFGPackageIterator * iterator) {

  if ( iterator->done )
    return NULL;

  if ( !lcfgpkgiter_has_next(iterator) ) {
    iterator->done = true;
    return NULL;
  }

  if ( iterator->current == NULL ) {
    iterator->current = lcfgpkglist_head(iterator->pkglist);
  } else {
    iterator->current = lcfgpkglist_next(iterator->current);
  }

  LCFGPackage * pkg = NULL;
  if ( iterator->current != NULL )
    pkg = lcfgpkglist_package(iterator->current);

  return pkg;
}

/* eof */
