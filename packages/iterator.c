/**
 * @file packages/iterator.c
 * @brief Functions for iterating through LCFG package lists
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "packages.h"
#include "utils.h"

/**
 * @brief Create new package list iterator
 *
 * This can be used to create a new @c LCFGPackageIterator for the
 * specified @c LCFGPackageList initialised to the start of the list.
 *
 * It is allowable to have multiple iterators for each list. Note that
 * doing an in-place sort of the list whilst using an iterator will
 * thoroughly upset everything.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * To avoid memory leaks, when you no longer require access to the
 * iterator you should call @c lcfgpkgiter_destroy().
 *
 * @param[in] list Pointer to @c LCFGPackageList
 *
 * @return Pointer to new @c LCFGPackageIterator
 *
 */

LCFGPackageIterator * lcfgpkgiter_new( LCFGPackageList * list ) {
  assert( list != NULL );

  LCFGPackageIterator * iterator = malloc( sizeof(LCFGPackageIterator) );
  if ( iterator == NULL ) {
    perror( "Failed to allocate memory for LCFG package iterator" );
    exit(EXIT_FAILURE);
  }

  lcfgpkglist_acquire(list);

  iterator->list    = list;
  iterator->current = NULL;

  return iterator;
}

/**
 * @brief Destroy package list iterator
 *
 * When the specified @c LCFGPackageIterator is no longer required
 * this will free all associated memory.
 *
 * The @c lcfgpkglist_relinquish() function will be called on the
 * associated package list. If there are no other references to the
 * package list this will result in the list being destroyed.
 *
 * @param[in] iterator Pointer to @c LCFGPackageIterator
 *
 */

void lcfgpkgiter_destroy( LCFGPackageIterator * iterator ) {

  if ( iterator == NULL ) return;

  lcfgpkglist_relinquish(iterator->list);

  iterator->list    = NULL;
  iterator->current = NULL;

  free(iterator);
  iterator = NULL;

}

/**
 * @brief Reset package list iterator to start
 *
 * This can be used to reset the @c LCFGPackageIterator to the head of
 * the package list.
 *
 * @param[in] iterator Pointer to @c LCFGPackageIterator
 *
 */

void lcfgpkgiter_reset( LCFGPackageIterator * iterator ) {
  iterator->current = NULL;
}

/**
 * @brief Test if package list iterator has another item
 *
 * This can be used to check if there are any further items available
 * in the package list.
 *
 * @param[in] iterator Pointer to @c LCFGPackageIterator
 *
 * @return boolean indicating if any more items available
 *
 */

bool lcfgpkgiter_has_next( LCFGPackageIterator * iterator ) {

  bool has_next = false;
  if ( iterator->current == NULL )
    has_next = !lcfgslist_is_empty(iterator->list);
  else
    has_next = ( lcfgslist_next(iterator->current) != NULL );

  return has_next;
}

/**
 * @brief Get next item from package list iterator
 *
 * This can be used to fetch the next item in the package list. If no
 * further packages are available the @c NULL value will be returned.
 *
 * @param[in] iterator Pointer to @c LCFGPackageIterator (or @c NULL)
 *
 * @return Pointer to next @c LCFGPackage
 *
 */

LCFGPackage * lcfgpkgiter_next(LCFGPackageIterator * iterator) {

  if ( !lcfgpkgiter_has_next(iterator) ) return NULL;

  if ( iterator->current == NULL )
    iterator->current = lcfgslist_head(iterator->list);
  else
    iterator->current = lcfgslist_next(iterator->current);

  LCFGPackage * next = NULL;
  if ( iterator->current != NULL )
    next = lcfgslist_data(iterator->current);

  return next;
}

/* eof */
