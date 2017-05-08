/**
 * @file resources/tags/iterator.c
 * @brief Functions for iterating back and forth through LCFG resource tag lists
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date$
 * $Revision$
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "tags.h"

/**
 * @brief Create new tag list iterator
 *
 * This can be used to create a new @c LCFGTagIterator for the
 * specified @c LCFGTagList initialised to the start of the list.
 *
 * It is allowable to have multiple iterators for each list. Note that
 * doing an in-place sort of the list whilst using an iterator will
 * thoroughly upset everything.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * To avoid memory leaks, when you no longer require access to the
 * iterator you should call @c lcfgtagiter_destroy().
 *
 * @param[in] list Pointer to @c LCFGTagList
 *
 * @return Pointer to new @c LCFGTagIterator
 *
 */

LCFGTagIterator * lcfgtagiter_new( LCFGTagList * list ) {

  LCFGTagIterator * iterator = malloc( sizeof(LCFGTagIterator) );
  if ( iterator == NULL ) {
    perror( "Failed to allocate memory for LCFG tag iterator" );
    exit(EXIT_FAILURE);
  }

  lcfgtaglist_acquire(list);

  iterator->list    = list;
  iterator->current = NULL;

  return iterator;
}

/**
 * @brief Destroy tag list iterator
 *
 * When the specified @c LCFGTagIterator is no longer required
 * this will free all associated memory.
 *
 * The @c lcfgtaglist_relinquish() function will be called on the
 * associated tag list. If there are no other references to the
 * tag list this will result in the list being destroyed.
 *
 * @param[in] iterator Pointer to @c LCFGTagIterator
 *
 */

void lcfgtagiter_destroy( LCFGTagIterator * iterator ) {

  if ( iterator == NULL ) return;

  lcfgtaglist_relinquish(iterator->list);

  iterator->list    = NULL;
  iterator->current = NULL;

  free(iterator);
  iterator = NULL;

}

/**
 * @brief Reset tag list iterator to start
 *
 * This can be used to reset the @c LCFGTagIterator to the head of
 * the tag list.
 *
 * @param[in] iterator Pointer to @c LCFGTagIterator
 *
 */

void lcfgtagiter_reset( LCFGTagIterator * iterator ) {
  iterator->current = NULL;
}

/**
 * @brief Test if tag list iterator has another item
 *
 * This can be used to check if there are any further items available
 * in the tag list.
 *
 * @param[in] iterator Pointer to @c LCFGTagIterator
 *
 * @return boolean indicating if any more items available
 *
 */

bool lcfgtagiter_has_next( LCFGTagIterator * iterator ) {

  bool has_next = false;
  if ( iterator->current == NULL )
    has_next = !lcfgtaglist_is_empty(iterator->list);
  else
    has_next = ( lcfgtaglist_next(iterator->current) != NULL );

  return has_next;
}

/**
 * @brief Get next item from tag list iterator
 *
 * This can be used to fetch the next item in the tag list. If no
 * further tags are available the @c NULL value will be returned.
 *
 * @param[in] iterator Pointer to @c LCFGTagIterator (or @c NULL)
 *
 * @return Pointer to next @c LCFGTag
 *
 */

LCFGTag * lcfgtagiter_next(LCFGTagIterator * iterator) {

  if ( !lcfgtagiter_has_next(iterator) ) return NULL;

  if ( iterator->current == NULL )
    iterator->current = lcfgtaglist_head(iterator->list);
  else
    iterator->current = lcfgtaglist_next(iterator->current);

  LCFGTag * next = NULL;
  if ( iterator->current != NULL )
    next = lcfgtaglist_tag(iterator->current);

  return next;
}

/**
 * @brief Test if tag list iterator has a previous item
 *
 * This can be used to check if there are any previous items available
 * in the tag list.
 *
 * @param[in] iterator Pointer to @c LCFGTagIterator
 *
 * @return boolean indicating if any previous items available
 *
 */

bool lcfgtagiter_has_prev( LCFGTagIterator * iterator ) {

  bool has_prev = false;
  if ( iterator->current == NULL )
    has_prev = !lcfgtaglist_is_empty(iterator->list);
  else
    has_prev = ( lcfgtaglist_prev(iterator->current) != NULL );

  return has_prev;
}

/**
 * @brief Get previous item from tag list iterator
 *
 * This can be used to fetch the previous item in the tag list. If no
 * further tags are available the @c NULL value will be returned.
 *
 * @param[in] iterator Pointer to @c LCFGTagIterator (or @c NULL)
 *
 * @return Pointer to previous @c LCFGTag
 *
 */

LCFGTag * lcfgtagiter_prev(LCFGTagIterator * iterator) {

  if ( !lcfgtagiter_has_prev(iterator) ) return NULL;

  if ( iterator->current == NULL )
    iterator->current = lcfgtaglist_tail(iterator->list);
  else
    iterator->current = lcfgtaglist_prev(iterator->current);

  LCFGTag * prev = NULL;
  if ( iterator->current != NULL )
    prev = lcfgtaglist_tag(iterator->current);

  return prev;
}

/* eof */
