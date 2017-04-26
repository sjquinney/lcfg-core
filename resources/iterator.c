/**
 * @file resources/iterator.c
 * @brief Functions for iterating through LCFG resource lists
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date$
 * $Revision$
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "components.h"

/**
 * @brief Create new resource list iterator
 *
 * This can be used to create a new @c LCFGResourceIterator for the
 * specified @c LCFGComponent initialised to the start of the list.
 *
 * It is allowable to have multiple iterators for each component. Note
 * that doing an in-place sort of the resource list whilst using an
 * iterator could thoroughly upset everything.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * To avoid memory leaks, when you no longer require access to the
 * iterator you should call @c lcfgresiter_destroy().
 *
 * @param[in] component Pointer to @c LCFGComponent
 *
 * @return Pointer to new @c LCFGResourceIterator
 *
 */

LCFGResourceIterator * lcfgresiter_new( LCFGComponent * component ) {

  LCFGResourceIterator * iterator = malloc( sizeof(LCFGResourceIterator) );
  if ( iterator == NULL ) {
    perror( "Failed to allocate memory for LCFG resource iterator" );
    exit(EXIT_FAILURE);
  }

  lcfgcomponent_acquire(component);

  iterator->component = component;
  iterator->current   = NULL;

  return iterator;
}

/**
 * @brief Destroy resource list iterator
 *
 * When the specified @c LCFGResourceIterator is no longer required
 * this will free all associated memory.
 *
 * The @c lcfgcomponent_relinquish() function will be called on the
 * associated component. If there are no other references to the
 * component this will result in it being destroyed.
 *
 * @param[in] iterator Pointer to @c LCFGResourceIterator
 *
 */

void lcfgresiter_destroy( LCFGResourceIterator * iterator ) {

  if ( iterator == NULL ) return;

  lcfgcomponent_relinquish(iterator->component);

  iterator->component = NULL;
  iterator->current = NULL;

  free(iterator);
  iterator = NULL;

}

/**
 * @brief Reset resource list iterator to start
 *
 * This can be used to reset the @c LCFGResourceIterator to the head of
 * the resource list.
 *
 * @param[in] iterator Pointer to @c LCFGResourceIterator
 *
 */

void lcfgresiter_reset( LCFGResourceIterator * iterator ) {
  iterator->current = NULL;
}

/**
 * @brief Test if resource list iterator has another item
 *
 * This can be used to check if there are any further items available
 * in the resource list.
 *
 * @param[in] iterator Pointer to @c LCFGResourceIterator
 *
 * @return boolean indicating if any more items available
 *
 */

bool lcfgresiter_has_next( LCFGResourceIterator * iterator ) {

  bool has_next = false;
  if ( iterator->current == NULL )
    has_next = !lcfgcomponent_is_empty(iterator->component);
  else
    has_next = ( lcfgcomponent_next(iterator->current) != NULL );

  return has_next;
}

/**
 * @brief Get next item from resource list iterator
 *
 * This can be used to fetch the next item in the resource list. If no
 * further resources are available the @c NULL value will be returned.
 *
 * @param[in] iterator Pointer to @c LCFGResourceIterator (or @c NULL)
 *
 * @return Pointer to next @c LCFGResource
 *
 */

LCFGResource * lcfgresiter_next(LCFGResourceIterator * iterator) {

  if ( !lcfgresiter_has_next(iterator) ) return NULL;

  if ( iterator->current == NULL )
    iterator->current = lcfgcomponent_head(iterator->component);
  else
    iterator->current = lcfgcomponent_next(iterator->current);

  LCFGResource * res = NULL;
  if ( iterator->current != NULL )
    res = lcfgcomponent_resource(iterator->current);

  return res;
}

/* eof */
