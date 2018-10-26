/**
 * @file resources/iterator.c
 * @brief Functions for iterating through LCFG resource lists
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "components.h"
#include "reslist.h"

static LCFGResourceListIterator * lcfgreslistiter_new(LCFGResourceList * list) {
  assert( list != NULL );

  LCFGResourceListIterator * iterator =
    calloc( 1, sizeof(LCFGResourceListIterator) );
  if ( iterator == NULL ) {
    perror( "Failed to allocate memory for LCFG resource list iterator" );
    exit(EXIT_FAILURE);
  }

  lcfgreslist_acquire(list);

  iterator->list    = list;
  iterator->current = NULL;

  return iterator;
}

static void lcfgreslistiter_destroy( LCFGResourceListIterator * iterator ) {

  if ( iterator == NULL ) return;

  lcfgreslist_relinquish(iterator->list);
  iterator->list    = NULL;

  iterator->current = NULL;

  free(iterator);
  iterator = NULL;

}

static bool lcfgreslistiter_has_next( LCFGResourceListIterator * iterator ) {

  bool has_next = false;
  if ( iterator->current == NULL )
    has_next = !lcfgslist_is_empty(iterator->list);
  else
    has_next = ( lcfgslist_next(iterator->current) != NULL );

  return has_next;
}

static const LCFGResource * lcfgreslistiter_next(LCFGResourceListIterator * iterator) {

  if ( !lcfgreslistiter_has_next(iterator) ) return NULL;

  if ( iterator->current == NULL )
    iterator->current = lcfgslist_head(iterator->list);
  else
    iterator->current = lcfgslist_next(iterator->current);

  const LCFGResource * next = NULL;
  if ( iterator->current != NULL )
    next = lcfgslist_data(iterator->current);

  return next;
}

/**
 * @brief Create new resource list iterator
 *
 * This can be used to create a new @c LCFGComponentIterator for the
 * specified @c LCFGComponent initialised to the start of the list.
 *
 * By default this will only iterate through the list of currently
 * active resources, this is typically what is required. Components
 * support having multiple context-specific versions of resources. To
 * iterate through all variants of each resource enable the @c
 * all_priorities option.

 * It is allowable to have multiple iterators for each component. Note
 * that doing an in-place sort of the resource list whilst using an
 * iterator could thoroughly upset everything.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * To avoid memory leaks, when you no longer require access to the
 * iterator you should call @c lcfgcompiter_destroy().
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] all_priorities Controls iteration through multiple versions of resources
 *
 * @return Pointer to new @c LCFGComponentIterator
 *
 */

LCFGComponentIterator * lcfgcompiter_new( LCFGComponent * comp,
                                          bool all_priorities ) {

  LCFGComponentIterator * iterator = calloc( 1, sizeof(LCFGComponentIterator) );
  if ( iterator == NULL ) {
    perror( "Failed to allocate memory for LCFG resource iterator" );
    exit(EXIT_FAILURE);
  }

  lcfgcomponent_acquire(comp);

  iterator->comp           = comp;
  iterator->listiter       = NULL;
  iterator->current        = -1;
  iterator->all_priorities = all_priorities;

  return iterator;
}

/**
 * @brief Destroy resource list iterator
 *
 * When the specified @c LCFGComponentIterator is no longer required
 * this will free all associated memory.
 *
 * The @c lcfgcomponent_relinquish() function will be called on the
 * associated component. If there are no other references to the
 * component this will result in it being destroyed.
 *
 * @param[in] iterator Pointer to @c LCFGComponentIterator
 *
 */

void lcfgcompiter_destroy( LCFGComponentIterator * iterator ) {

  if ( iterator == NULL ) return;

  lcfgcomponent_relinquish(iterator->comp);
  iterator->comp = NULL;

  lcfgreslistiter_destroy(iterator->listiter);
  iterator->listiter = NULL;

  free(iterator);
  iterator = NULL;

}

/**
 * @brief Reset resource list iterator to start
 *
 * This can be used to reset the @c LCFGComponentIterator to the head of
 * the resource list.
 *
 * @param[in] iterator Pointer to @c LCFGComponentIterator
 *
 */

void lcfgcompiter_reset( LCFGComponentIterator * iterator ) {
  assert( iterator != NULL );

  lcfgreslistiter_destroy(iterator->listiter);
  iterator->listiter = NULL;

  iterator->current = -1;
}

/**
 * @brief Test if resource list iterator has another item
 *
 * This can be used to check if there are any further items available
 * in the resource list.
 *
 * @param[in] iterator Pointer to @c LCFGComponentIterator
 *
 * @return boolean indicating if any more items available
 *
 */

bool lcfgcompiter_has_next( LCFGComponentIterator * iterator ) {
  assert( iterator != NULL );

  bool has_next = ( iterator->listiter != NULL &&
		    lcfgreslistiter_has_next(iterator->listiter) );

  LCFGResourceList ** resources = iterator->comp->resources;

  unsigned long next;
  for ( next = (unsigned long) ( iterator->current + 1 );
	!has_next && next < iterator->comp->buckets;
	next++ ) {

    if ( resources[next] && !lcfgreslist_is_empty(resources[next]) )
      has_next = true;

  }

  return has_next;
}

/**
 * @brief Get next item from resource list iterator
 *
 * This can be used to fetch the next item in the resource list. If no
 * further resources are available the @c NULL value will be returned.
 *
 * @param[in] iterator Pointer to @c LCFGComponentIterator (or @c NULL)
 *
 * @return Pointer to next @c LCFGResource
 *
 */

const LCFGResource * lcfgcompiter_next(LCFGComponentIterator * iterator) {
  assert( iterator != NULL );

  const LCFGResource * next = NULL;

  if ( iterator->listiter != NULL )
    next = lcfgreslistiter_next(iterator->listiter);

  LCFGResourceList ** resources = iterator->comp->resources;

  while ( next == NULL &&
	  (unsigned int) (iterator->current + 1) < iterator->comp->buckets ) {

    iterator->current += 1;

    LCFGResourceList * list = resources[iterator->current];

    if ( list && !lcfgreslist_is_empty(list) ) {
      if ( iterator->all_priorities ) {
        lcfgreslistiter_destroy(iterator->listiter);
        iterator->listiter = lcfgreslistiter_new(list);

        next = lcfgreslistiter_next(iterator->listiter);
      } else {
        next = lcfgreslist_first_resource(list);
      }
    }

  }

  return next;
}

/* eof */
