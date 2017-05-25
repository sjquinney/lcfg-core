/**
 * @file utils/slist.c
 * @brief Generic single-linked list support
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "utils.h"

/**
 * @brief Create and initialise a new list node
 *
 * This creates a simple wrapper @c LCFGSListNode node which is used
 * to hold a pointer to an item in a list.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * To avoid memory leaks, when the new structure is no longer required
 * the @c lcfgslistnode_destroy() function should be called.
 *
 * @param[in] data Pointer to list item
 *
 * @return Pointer to new @c LCFGSListNode
 *
 */

LCFGSListNode * lcfgslistnode_new( void * data ) {
  assert( data != NULL );

  LCFGSListNode * node = malloc( sizeof(LCFGSListNode) );
  if ( node == NULL ) {
    perror( "Failed to allocate memory for list node" );
    exit(EXIT_FAILURE);
  }

  /* Set default values */

  node->data = data;
  node->next = NULL;

  return node;
}

/**
 * @brief Destroy a resource node
 *
 * When the specified @c LCFGSListNode is no longer required this can
 * be used to free all associated memory.
 *
 * Note that destroying an @c LCFGSListNode does not destroy the
 * associated data for the list item, that must be done separately.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * node which has already been destroyed (or potentially was never
 * created).
 *
 * @param[in] node Pointer to @c LCFGSListNode to be destroyed.
 *
 */

void lcfgslistnode_destroy(LCFGSListNode * node) {

  if ( node == NULL ) return;

  node->data = NULL;
  node->next = NULL;

  free(node);
  node = NULL;

}

/* eof */
