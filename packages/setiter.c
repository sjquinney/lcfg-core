#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "packages.h"
#include "utils.h"

LCFGPkgSetIterator * lcfgpkgsetiter_new( LCFGPackageSet * pkgset ) {

  LCFGPkgSetIterator * iterator = malloc( sizeof(LCFGPkgSetIterator) );
  if ( iterator == NULL ) {
    perror( "Failed to allocate memory for LCFG package iterator" );
    exit(EXIT_FAILURE);
  }

  lcfgpkgset_acquire(pkgset);

  iterator->set       = pkgset;
  iterator->current   = -1;
  iterator->listiter  = NULL;

  return iterator;
}

void lcfgpkgsetiter_destroy( LCFGPkgSetIterator * iterator ) {

  if ( iterator == NULL ) return;

  lcfgpkgset_relinquish(iterator->set);
  iterator->set = NULL;

  lcfgpkgiter_destroy(iterator->listiter);
  iterator->listiter = NULL;

  free(iterator);
  iterator = NULL;

}

void lcfgpkgsetiter_reset( LCFGPkgSetIterator * iterator ) {
  lcfgpkgiter_destroy(iterator->listiter);
  iterator->listiter = NULL;

  iterator->current = -1;
}

bool lcfgpkgsetiter_has_next( LCFGPkgSetIterator * iterator ) {

  bool has_next = ( iterator->listiter != NULL &&
		    lcfgpkgiter_has_next(iterator->listiter) );

  LCFGPackageList ** packages = iterator->set->packages;

  unsigned long next;
  for ( next = (unsigned long) ( iterator->current + 1 );
	!has_next && next < iterator->set->buckets;
	next++ ) {

    if ( packages[next] && !lcfgpkglist_is_empty(packages[next]) )
      has_next = true;

  }

  return has_next;
}

LCFGPackage * lcfgpkgsetiter_next(LCFGPkgSetIterator * iterator) {

  LCFGPackage * next = NULL;

  if ( iterator->listiter != NULL )
    next = lcfgpkgiter_next(iterator->listiter);

  LCFGPackageList ** packages = iterator->set->packages;

  while ( next == NULL &&
	  (unsigned int) (iterator->current + 1) < iterator->set->buckets ) {

    iterator->current += 1;

    LCFGPackageList * pkgs = packages[iterator->current];

    if ( pkgs && !lcfgpkglist_is_empty(pkgs) ) {
      lcfgpkgiter_destroy(iterator->listiter);
      iterator->listiter = lcfgpkgiter_new(pkgs);

      next = lcfgpkgiter_next(iterator->listiter);
    }

  }

  return next;
}
