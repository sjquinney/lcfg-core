/**
 * @file packages/setiter.c
 * @brief Functions for iterating through LCFG package sets
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "packages.h"
#include "utils.h"

/**
 * @brief Create new package set iterator
 *
 * This can be used to create a new @c LCFGPackageIterator for the
 * specified @c LCFGPackageSet initialised to the start of the set.
 *
 * It is allowable to have multiple iterators for each set. Note that
 * there is no way to control the order in which the iterator walks
 * through the set packages.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * To avoid memory leaks, when you no longer require access to the
 * iterator you should call @c lcfgpkgiter_destroy().
 *
 * @param[in] pkgset Pointer to @c LCFGPackageSet
 *
 * @return Pointer to new @c LCFGPkgSetIterator
 *
 */

LCFGPkgSetIterator * lcfgpkgsetiter_new( LCFGPackageSet * pkgset ) {
  assert( pkgset != NULL );

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

/**
 * @brief Destroy package set iterator
 *
 * When the specified @c LCFGPkgSetIterator is no longer required
 * this will free all associated memory.
 *
 * The @c lcfgpkgset_relinquish() function will be called on the
 * associated package set. If there are no other references to the
 * package set this will result in the set being destroyed.
 *
 * @param[in] iterator Pointer to @c LCFGPkgSetIterator
 *
 */

void lcfgpkgsetiter_destroy( LCFGPkgSetIterator * iterator ) {

  if ( iterator == NULL ) return;

  lcfgpkgset_relinquish(iterator->set);
  iterator->set = NULL;

  lcfgpkgiter_destroy(iterator->listiter);
  iterator->listiter = NULL;

  free(iterator);
  iterator = NULL;

}

/**
 * @brief Reset package set iterator to start
 *
 * This can be used to reset the @c LCFGPkgSetIterator to the head of
 * the package set.
 *
 * @param[in] iterator Pointer to @c LCFGPkgSetIterator
 *
 */

void lcfgpkgsetiter_reset( LCFGPkgSetIterator * iterator ) {
  lcfgpkgiter_destroy(iterator->listiter);
  iterator->listiter = NULL;

  iterator->current = -1;
}

/**
 * @brief Test if package set iterator has another item
 *
 * This can be used to check if there are any further items available
 * in the package set.
 *
 * @param[in] iterator Pointer to @c LCFGPkgSetIterator
 *
 * @return boolean indicating if any more items available
 *
 */

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

/**
 * @brief Get next item from package set iterator
 *
 * This can be used to fetch the next item in the package set. If no
 * further packages are available the @c NULL value will be returned.
 *
 * @param[in] iterator Pointer to @c LCFGPkgSetIterator (or @c NULL)
 *
 * @return Pointer to next @c LCFGPackage
 *
 */

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

/* eof */
