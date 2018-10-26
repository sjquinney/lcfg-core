/**
 * @file resources/components/diff.c
 * @brief Functions for finding the differences between LCFG components
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date: 2017-05-12 10:44:21 +0100 (Fri, 12 May 2017) $
 * $Revision: 32703 $
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "differences.h"
#include "reslist.h"

/**
 * @brief Create and initialise a new component diff
 *
 * Creates a new @c LCFGDiffComponent and initialises the parameters to
 * the default values.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * The reference count for the structure is initialised to 1. To avoid
 * memory leaks, when it is no longer required the
 * @c lcfgdiffcomponent_relinquish() function should be called.
 *
 * @return Pointer to new @c LCFGDiffComponent
 *
 */

LCFGDiffComponent * lcfgdiffcomponent_new(void) {

  LCFGDiffComponent * compdiff = malloc( sizeof(LCFGDiffComponent) );
  if ( compdiff == NULL ) {
    perror( "Failed to allocate memory for LCFG component diff" );
    exit(EXIT_FAILURE);
  }

  /* Set default values */
  compdiff->name = NULL;
  compdiff->head = NULL;
  compdiff->tail = NULL;
  compdiff->size = 0;
  compdiff->change_type = LCFG_CHANGE_NONE;
  compdiff->_refcount = 1;

  return compdiff;
}

/**
 * @brief Destroy the component diff
 *
 * When the specified @c LCFGComponentDiff is no longer required this
 * will free all associated memory. There is support for reference
 * counting so typically the @c lcfgdiffcomponent_relinquish() function
 * should be used.
 *
 * This will call @c lcfgdiffresource_relinquish() on each resource
 * diff and thus also calls @c lcfgcomponent_relinquish() for the old
 * and new @c LCFGComponent structures. If the reference count on a
 * resource drops to zero it will also be destroyed
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * component diff which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] compdiff Pointer to @c LCFGDiffComponent to be destroyed.
 *
 */

void lcfgdiffcomponent_destroy(LCFGDiffComponent * compdiff ) {

  if ( compdiff == NULL ) return;

  while ( lcfgdiffcomponent_size(compdiff) > 0 ) {
    LCFGDiffResource * resdiff = NULL;
    if ( lcfgdiffcomponent_remove_next( compdiff, NULL, &resdiff )
         == LCFG_CHANGE_REMOVED ) {
      lcfgdiffresource_relinquish(resdiff);
    }
  }

  free(compdiff->name);
  compdiff->name = NULL;

  free(compdiff);
  compdiff = NULL;

}

/**
 * @brief Acquire reference to component diff
 *
 * This is used to record a reference to the @c LCFGDiffComponent, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the structure is no
 * longer required the @c lcfgdiffcomponent_relinquish() function
 * should be called.
 *
 * @param[in] compdiff Pointer to @c LCFGDiffComponent
 *
 */

void lcfgdiffcomponent_acquire( LCFGDiffComponent * compdiff ) {
  assert( compdiff != NULL );

  compdiff->_refcount += 1;
}

/**
 * @brief Release reference to component diff
 *
 * This is used to release a reference to the @c LCFGDiffComponent, it
 * does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgdiffcomponent_destroy()
 * function will be called to clean up the memory associated with the
 * structure.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * component diff which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] compdiff Pointer to @c LCFGDiffComponent
 *
 */

void lcfgdiffcomponent_relinquish( LCFGDiffComponent * compdiff ) {

  if ( compdiff == NULL ) return;

  if ( compdiff->_refcount > 0 )
    compdiff->_refcount -= 1;

  if ( compdiff->_refcount == 0 )
    lcfgdiffcomponent_destroy(compdiff);

}

/**
 * @brief Check if the component diff has a name
 *
 * Checks if the specified @c LCFGDiffComponent currently has a value
 * set for the @e name attribute.
 *
 * @param[in] compdiff Pointer to an @c LCFGDiffComponent
 *
 * @return boolean which indicates if a component diff has a name
 *
 */

bool lcfgdiffcomponent_has_name(const LCFGDiffComponent * compdiff) {
  return !isempty(compdiff->name);
}

/**
 * @brief Get the name for the component diff
 *
 * This returns the value of the @e name parameter for the @c
 * LCFGDiffComponent. If the component diff does not currently have a
 * @e name then the pointer returned will be @c NULL.
 *
 * It is important to note that this is @b NOT a copy of the string,
 * changing the returned string will modify the @e name of the
 * component.
 *
 * @param[in] compdiff Pointer to an @c LCFGDiffComponent
 *
 * @return The @e name for the component diff (possibly @c NULL).
 *
 */

char * lcfgdiffcomponent_get_name(const LCFGDiffComponent * compdiff) {
  return compdiff->name;
}

/**
 * @brief Set the name for the component diff
 *
 * Sets the value of the @e name parameter for the @c
 * LCFGDiffComponent to that specified. It is important to note that
 * this does @b NOT take a copy of the string. Furthermore, once the
 * value is set the component diff assumes "ownership", the memory
 * will be freed if the name is further modified or the component is
 * destroyed.
 *
 * Before changing the value of the @e name to be the new string it
 * will be validated using the @c lcfgcomponent_valid_name()
 * function. If the new string is not valid then no change will occur,
 * the @c errno will be set to @c EINVAL and the function will return
 * a @c false value.
 *
 * @param[in] compdiff Pointer to an @c LCFGDiffComponent
 * @param[in] new_name String which is the new name
 *
 * @return boolean indicating success
 *
 */

bool lcfgdiffcomponent_set_name( LCFGDiffComponent * compdiff,
                                 char * new_name ) {

  bool ok = false;
  if ( lcfgcomponent_valid_name(new_name) ) {
    free(compdiff->name);

    compdiff->name = new_name;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/**
 * @brief Set the type of the component diff
 *
 * This can be used to change the type of the @c LCFGDiffComponent. By
 * default it is @c LCFG_CHANGE_NONE.
 *
 * @param[in] compdiff Pointer to an @c LCFGDiffComponent
 * @param[in] change_type Integer indicating type of diff
 *
 */

void lcfgdiffcomponent_set_type( LCFGDiffComponent * compdiff,
                                 LCFGChange change_type ) {
  compdiff->change_type = change_type;
}

/**
 * @brief Get the type of the component diff
 *
 * This can be used to retrieve the type of the @c
 * LCFGDiffComponent. It will be one of the following:
 *
 *   - @c LCFG_CHANGE_NONE - no change
 *   - @c LCFG_CHANGE_ADDED - entire component is newly added
 *   - @c LCFG_CHANGE_REMOVED - entire component is removed
 *   - @c LCFG_CHANGE_MODIFIED - resources have been modified
 *
 * @param[in] compdiff Pointer to an @c LCFGDiffComponent
 *
 */

LCFGChange lcfgdiffcomponent_get_type( const LCFGDiffComponent * compdiff ) {
  return compdiff->change_type;
}

/**
 * @brief Check if the diff represents any change
 *
 * This will return true if the @c LCFGDiffComponent represents the
 * addition, removal or modification of a component.
 *
 * @param[in] compdiff Pointer to an @c LCFGDiffComponent
 *
 * @return Boolean which indicates if diff represents any change
 *
 */

bool lcfgdiffcomponent_is_changed( const LCFGDiffComponent * compdiff ) {
  LCFGChange difftype = lcfgdiffcomponent_get_type(compdiff);
  return ( difftype == LCFG_CHANGE_ADDED   ||
	   difftype == LCFG_CHANGE_REMOVED ||
	   difftype == LCFG_CHANGE_MODIFIED );
}

/**
 * @brief Check if the diff does not represent a change
 *
 * This will return true if the @c LCFGDiffComponent does not represent
 * any type of change. The @e old and @e new @c LCFGComponent are both
 * present and there are no differences.
 *
 * @param[in] compdiff Pointer to an @c LCFGDiffComponent
 *
 * @return Boolean which indicates if diff represent no change
 *
 */

bool lcfgdiffcomponent_is_nochange( const LCFGDiffComponent * compdiff ) {
  return ( compdiff->change_type == LCFG_CHANGE_NONE );
}

/**
 * @brief Check if the diff represents a new component
 *
 * This will return true if the @c LCFGDiffComponent represents a newly
 * added @c LCFGComponent (i.e. the @e old @c LCFGComponent is @c NULL).
 *
 * @param[in] compdiff Pointer to an @c LCFGDiffComponent
 *
 * @return Boolean which indicates if diff represents new component
 *
 */

bool lcfgdiffcomponent_is_added( const LCFGDiffComponent * compdiff ) {
  return ( compdiff->change_type == LCFG_CHANGE_ADDED );
}

/**
 * @brief Check if the diff represents a modified value
 *
 * This will return true if the @c LCFGDiffComponent represents a
 * difference of values for the lists of @c LCFGResource for the @e
 * old and @e new @c LCFGComponent.
 *
 * @param[in] compdiff Pointer to an @c LCFGDiffComponent
 *
 * @return Boolean which indicates if diff represents changed value
 *
 */

bool lcfgdiffcomponent_is_modified( const LCFGDiffComponent * compdiff ) {
  return ( compdiff->change_type == LCFG_CHANGE_MODIFIED );
}

/**
 * @brief Check if the diff represents a removed component
 *
 * This will return true if the @c LCFGDiffComponent represents removed
 * @c LCFGComponent (i.e. the @e new @c LCFGComponent is @c NULL).
 *
 * @param[in] compdiff Pointer to an @c LCFGDiffComponent
 *
 * @return Boolean which indicates if diff represents removed component
 *
 */

bool lcfgdiffcomponent_is_removed( const LCFGDiffComponent * compdiff ) {
  return ( compdiff->change_type == LCFG_CHANGE_REMOVED );
}

/**
 * @brief Insert a resource diff into the list
 *
 * This can be used to insert an @c LCFGDiffResource into the
 * specified @c LCFGDiffComponent. The resource will be wrapped into
 * an @c LCFGSListNode using the @c lcfgslistnode_new() function.
 *
 * The resource diff will be inserted into the component diff
 * immediately after the specified @c LCFGSListNode. To insert the
 * resource diff at the head of the list the @c NULL value should be
 * passed for the node.
 *
 * If the resource is successfully inserted the @c LCFG_CHANGE_ADDED
 * value is returned, if an error occurs then @c LCFG_CHANGE_ERROR is
 * returned.
 *
 * @param[in] list Pointer to @c LCFGDiffComponent
 * @param[in] node Pointer to @c LCFGSListNode
 * @param[in] item Pointer to @c LCFGDiffResource
 * 
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgdiffcomponent_insert_next( LCFGDiffComponent * list,
                                          LCFGSListNode     * node,
                                          LCFGDiffResource  * item ) {
  assert( list != NULL );
  assert( item != NULL );

  LCFGSListNode * new_node = lcfgslistnode_new(item);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgdiffresource_acquire(item);

  if ( node == NULL ) { /* HEAD */

    if ( lcfgslist_is_empty(list) )
      list->tail = new_node;

    new_node->next = list->head;
    list->head     = new_node;

  } else {

    if ( node->next == NULL )
      list->tail = new_node;

    new_node->next = node->next;
    node->next     = new_node;

  }

  list->size++;

  return LCFG_CHANGE_ADDED;
}

/**
 * @brief Remove a resource diff from the list
 *
 * This can be used to remove an @c LCFGDiffResource from the
 * specified @c LCFGDiffComponent.
 *
 * The resource diff removed from the component diff is immediately
 * after the specified @c LCFGSListNode. To remove the resource diff
 * from the head of the list the @c NULL value should be passed for
 * the node.
 *
 * If the resource diff is successfully removed the @c
 * LCFG_CHANGE_REMOVED value is returned, if an error occurs then @c
 * LCFG_CHANGE_ERROR is returned. If the list is already empty then
 * the @c LCFG_CHANGE_NONE value is returned.
 *
 * Note that, since a pointer to the @c LCFGDiffResource is returned
 * to the caller, the reference count will still be at least 1. To
 * avoid memory leaks, when the struct is no longer required it should
 * be released by calling @c lcfgdiffresource_relinquish().
 *
 * @param[in] list Pointer to @c LCFGDiffComponent
 * @param[in] node Pointer to @c LCFGSListNode
 * @param[out] item Pointer to @c LCFGDiffResource
 * 
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgdiffcomponent_remove_next( LCFGDiffComponent * list,
                                          LCFGSListNode     * node,
                                          LCFGDiffResource ** item ) {
  assert( list != NULL );

  if ( lcfgslist_is_empty(list) ) return LCFG_CHANGE_NONE;

  LCFGSListNode * old_node = NULL;

  if ( node == NULL ) { /* HEAD */

    old_node   = list->head;
    list->head = list->head->next;

    if ( lcfgslist_size(list) == 1 )
      list->tail = NULL;

  } else {

    if ( node->next == NULL ) return LCFG_CHANGE_ERROR;

    old_node   = node->next;
    node->next = node->next->next;

    if ( node->next == NULL )
      list->tail = node;

  }

  list->size--;

  *item = lcfgslist_data(old_node);

  lcfgslistnode_destroy(old_node);

  return LCFG_CHANGE_REMOVED;
}

/**
 * @brief Sort a list of resources for a component diff
 *
 * This does an in-place sort of the nodes of the list of @c
 * LCFGDiffResource for an @c LCFGDiffComponent by using the @c
 * lcfgdiffresource_compare() function.
 *
 * @param[in] list Pointer to @c LCFGDiffComponent
 *
 */

void lcfgdiffcomponent_sort( LCFGDiffComponent * list ) {
  assert( list != NULL );

  if ( lcfgslist_size(list) < 2 ) return;

  /* Oo. Oo. bubble sort .oO .oO */

  bool swapped=true;
  while (swapped) {
    swapped=false;

    LCFGSListNode * cur_node = NULL;
    for ( cur_node = lcfgslist_head(list);
          cur_node != NULL && lcfgslist_has_next(cur_node);
          cur_node = lcfgslist_next(cur_node) ) {

      LCFGDiffResource * item1 = lcfgslist_data(cur_node);
      LCFGDiffResource * item2 = lcfgslist_data(cur_node->next);

      if ( lcfgdiffresource_compare( item1, item2 ) > 0 ) {
        cur_node->data       = item2;
        cur_node->next->data = item1;
        swapped = true;
      }

    }
  }

}

/**
 * @brief Find the list node with a given name
 *
 * This can be used to search through an @c LCFGDiffComponent to find
 * the first node which has a matching name. Note that the
 * matching is done using strcmp(3) which is case-sensitive.
 *
 * A @c NULL value is returned if no matching node is found. Also, a
 * @c NULL value is returned if a @c NULL value or an empty list is
 * specified.
 *
 * @param[in] list Pointer to @c LCFGDiffComponent to be searched
 * @param[in] want_name The name of the required node
 *
 * @return Pointer to an @c LCFGSListNode (or the @c NULL value).
 *
 */

LCFGSListNode * lcfgdiffcomponent_find_node( const LCFGDiffComponent * list,
                                             const char * want_name ) {
  assert( want_name != NULL );

  if ( lcfgslist_is_empty(list) ) return NULL;

  LCFGSListNode * result = NULL;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgslist_head(list);
	cur_node != NULL && result == NULL;
	cur_node = lcfgslist_next(cur_node) ) {

    const LCFGDiffResource * item = lcfgslist_data(cur_node); 

    if ( lcfgdiffresource_match( item, want_name ) )
      result = (LCFGSListNode *) cur_node;

  }

  return result;
}

/**
 * @brief Find the resource diff for a given name
 *
 * This can be used to search through an @c LCFGDiffComponent to find
 * the first @c LCFGDiffResource which has a matching name. Note that
 * the matching is done using strcmp(3) which is case-sensitive.
 *
 * To ensure the returned @c LCFGDiffResource is not destroyed when
 * the parent @c LCFGDiffComponent is destroyed you would need to call
 * the @c lcfgdiffresource_acquire() function.
 *
 * @param[in] list Pointer to @c LCFGDiffComponent to be searched
 * @param[in] want_name The name of the required resource
 *
 * @return Pointer to an @c LCFGDiffResource (or the @c NULL value).
 *
 */

LCFGDiffResource * lcfgdiffcomponent_find_resource(
					  const LCFGDiffComponent * list,
					  const char * want_name ) {
  assert( want_name != NULL );

  LCFGDiffResource * item = NULL;

  const LCFGSListNode * node = lcfgdiffcomponent_find_node( list, want_name );
  if ( node != NULL )
    item = lcfgslist_data(node);

  return item;
}

/**
 * @brief Check if a component diff contains a particular resource
 *
 * This can be used to search through an @c LCFGDiffComponent to check
 * if it contains an @c LCFGDiffResource with a matching name. Note
 * that the matching is done using strcmp(3) which is case-sensitive.
 * 
 * This uses the @c lcfgdiffcomponent_find_node() function to find the
 * relevant node. If a @c NULL value is specified for the list or the
 * list is empty then a false value will be returned.
 *
 * It is important to note that the existence of an @c
 * LCFGDiffResource in the list is not sufficient proof that it is in
 * any way changed. To check that for a specific resource diff use a
 * function like @c lcfgdiffresource_is_changed().
 *
 * @param[in] list Pointer to @c LCFGDiffComponent to be searched (may be @c NULL)
 * @param[in] want_name The name of the required resource
 *
 * @return Boolean value which indicates presence of resource in component diff
 *
 */

bool lcfgdiffcomponent_has_resource( const LCFGDiffComponent * list,
				     const char * want_name ) {
  assert( want_name != NULL );

  return ( lcfgdiffcomponent_find_node( list, want_name ) != NULL );
}

/**
 * Check if component was prodded
 *
 * The ngeneric framework has support for @e prodding the component to
 * force a reconfiguration action to take place even when no other
 * resources have changed. 
 *
 * The component is considered to have been prodded when there is an
 * entry in the diff for the "ng_prod" resource and it is either
 * modified or newly added and the new resource has a value. Removing
 * the resource or setting the value to the empty string does NOT
 * cause the component to be prodded.
 *
 * @param[in] compdiff Pointer to @c LCFGDiffComponent to be checked
 *
 * @return Boolean which indicates if the component was prodded
 *
 */

bool lcfgdiffcomponent_was_prodded( const LCFGDiffComponent * compdiff ) {

  /* Does not make much sense to prod a component when it is being
     added or removed */

  if ( lcfgdiffcomponent_get_type(compdiff) != LCFG_CHANGE_MODIFIED )
    return false;

  const LCFGDiffResource * resdiff =
    lcfgdiffcomponent_find_resource( compdiff, "ng_prod" );

  bool prodded = false;

  if ( resdiff != NULL && lcfgdiffresource_has_new(resdiff) ) {
    const LCFGResource * new_res = lcfgdiffresource_get_new(resdiff);
    if ( lcfgresource_has_value(new_res) )
      prodded = true;
  }

  return prodded;
}

/**
 * Check if a particular resource is changed
 *
 * Check if there are changes for the named resource in the diff.
 *
 * @param[in] compdiff Pointer to @c LCFGDiffComponent to be searched
 * @param[in] res_name Name of resource to be checked
 *
 * @return Boolean which indicates if the resource is changed
 *
 */

bool lcfgdiffcomponent_resource_is_changed( const LCFGDiffComponent * compdiff,
					    const char * res_name ) {

  const LCFGDiffResource * resdiff =
    lcfgdiffcomponent_find_resource( compdiff, res_name );

  return ( resdiff != NULL && lcfgdiffresource_is_changed(resdiff) );
}


/**
 * @brief Format the component diff for a @e hold file
 *
 * The LCFG client supports a @e secure mode which can be used to hold
 * back resource changes pending a manual review by the
 * administrator. To assist in the review process it produces a @e
 * hold file which contains a summary of all resource changes
 * (additions, removals and modifications of values). This function
 * can be used to serialise the component diff in the correct way for
 * inclusion in the @e hold file.
 *
 * @param[in] compdiff Pointer to @c LCFGDiffComponent
 * @param[in] holdfile File stream to which diff should be written
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgdiffcomponent_to_holdfile( const LCFGDiffComponent * compdiff,
                                          FILE * holdfile ) {

  if ( lcfgdiffcomponent_is_empty(compdiff) ) return LCFG_STATUS_OK;

  const char * prefix = lcfgdiffcomponent_has_name(compdiff) ?
                        lcfgdiffcomponent_get_name(compdiff) : NULL;

  size_t buf_size = 512;
  char * buffer = calloc( buf_size, sizeof(char) );
  if ( buffer == NULL ) {
    perror( "Failed to allocate memory for LCFG resource diff buffer" );
    exit(EXIT_FAILURE);
  }

  bool ok = true;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgdiffcomponent_head(compdiff);
	cur_node != NULL && ok;
	cur_node = lcfgdiffcomponent_next(cur_node) ) {

    const LCFGDiffResource * resdiff = lcfgdiffcomponent_resdiff(cur_node);

    ssize_t rc = lcfgdiffresource_to_hold( resdiff, prefix,
                                           &buffer, &buf_size );

    if ( rc > 0 ) {

      if ( fputs( buffer, holdfile ) < 0 )
        ok = false;

    } else if ( rc < 0 ) {
      ok = false;
    }

  }

  free(buffer);

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/**
 * @brief Find the differences between two components
 *
 * This takes two @c LCFGComponent and creates a new @c
 * LCFGDiffComponent to represent the differences (if any) between the
 * components.
 *
 * To avoid memory leaks, when it is no longer required the @c
 * lcfgdiffcomponent_relinquish() function should be called.
 *
 * @param[in] comp1 Pointer to the @e old @c LCFGComponent (may be @c NULL)
 * @param[in] comp2 Pointer to the @e new @c LCFGComponent (may be @c NULL)
 * @param[out] result Reference to pointer to the new @c LCFGDiffComponent
 *
 * @return Status value indicating success of the process
 *
 */

LCFGChange lcfgcomponent_diff( const LCFGComponent * comp1,
                               const LCFGComponent * comp2,
                               LCFGDiffComponent ** result ) {

  LCFGDiffComponent * compdiff = lcfgdiffcomponent_new();

  /* Try to get the name from either component */

  const char * name = NULL;
  if ( comp1 != NULL && lcfgcomponent_has_name(comp1) )
    name = lcfgcomponent_get_name(comp1);
  else if ( comp2 != NULL && lcfgcomponent_has_name(comp2) )
    name = lcfgcomponent_get_name(comp2);

  bool ok = true;

  if ( name != NULL ) {
    char * new_name = strdup(name);
    if ( !lcfgdiffcomponent_set_name( compdiff, new_name ) ) {
      ok = false;
      free(new_name);
    }
  }

  /* Look for removed and modified resources */

  if ( comp1 != NULL ) {

    LCFGResourceList ** resources1 = comp1->resources;

    unsigned long i;
    for ( i=0; i < comp1->buckets; i++ ) {

      const LCFGResourceList * list = resources1[i];
      if ( lcfgreslist_is_empty(list) ) continue;

      LCFGResource * res1 = (LCFGResource *) lcfgreslist_first_resource(list);

      const char * res1_name = lcfgresource_get_name(res1);

      LCFGResource * res2 =
        (LCFGResource *) lcfgcomponent_find_resource( comp2, res1_name );

      if ( res2 == NULL || !lcfgresource_same_value( res1, res2 ) ) {

        LCFGDiffResource * resdiff = NULL;
        LCFGChange res_change = lcfgresource_diff( res1, res2, &resdiff );

        /* Just ignore anything where there are no differences */

        if ( res_change == LCFG_CHANGE_ERROR ) {
          ok = false;
        } else if ( res_change != LCFG_CHANGE_NONE ) {

          LCFGChange append_rc = lcfgdiffcomponent_append( compdiff, resdiff );
          if ( append_rc == LCFG_CHANGE_ERROR )
            ok = false;

        }

        lcfgdiffresource_relinquish(resdiff);
      }
    }
  }

  /* Look for resources which have been added */
  if ( comp2 != NULL ) {

    LCFGResourceList ** resources2 = comp2->resources;

    unsigned long i;
    for ( i=0; i < comp2->buckets; i++ ) {

      const LCFGResourceList * list = resources2[i];
      if ( lcfgreslist_is_empty(list) ) continue;

      LCFGResource * res2 = (LCFGResource *) lcfgreslist_first_resource(list);

      const char * res2_name = lcfgresource_get_name(res2);

      /* Only interested in resources which are NOT in first component */

      if ( !lcfgcomponent_has_resource( comp1, res2_name ) ) {

        LCFGDiffResource * resdiff = NULL;
        LCFGChange res_change = lcfgresource_diff( NULL, res2, &resdiff );

        if ( res_change == LCFG_CHANGE_ERROR ) {
          ok = false;
        } else if ( res_change != LCFG_CHANGE_NONE ) {

          LCFGChange append_rc = lcfgdiffcomponent_append( compdiff, resdiff );
          if ( append_rc == LCFG_CHANGE_ERROR )
            ok = false;

        }

        lcfgdiffresource_relinquish(resdiff);

      }

    }
  }

  LCFGChange change_type = LCFG_CHANGE_NONE;

  if (ok) {

    if ( lcfgcomponent_is_empty(comp1) ) {

      if ( !lcfgcomponent_is_empty(comp2) )
        change_type = LCFG_CHANGE_ADDED;

    } else {

      if ( lcfgcomponent_is_empty(comp2) )
        change_type = LCFG_CHANGE_REMOVED;
      else if ( !lcfgdiffcomponent_is_empty( compdiff ) )
        change_type = LCFG_CHANGE_MODIFIED;

    }

    lcfgdiffcomponent_set_type( compdiff, change_type );

  } else {
    change_type = LCFG_CHANGE_ERROR;

    lcfgdiffcomponent_relinquish(compdiff);
    compdiff = NULL;
  }

  *result = compdiff;

  return change_type;
}

/**
 * @brief Check for differences between sets of components
 *
 * This takes two @c LCFGComponentSet and returns lists of names of
 * components which have been removed, added or modified. It does not
 * return any details about which resources have changed just that
 * something has changed.
 *
 * This will return @c LCFG_CHANGE_MODIFIED if there are any
 * differences and @c LCFG_CHANGE_NONE otherwise.
 *
 * @param[in] compset1 Pointer to the @e old @c LCFGComponentSet (may be @c NULL)
 * @param[in] compset2 Pointer to the @e new @c LCFGComponentSet (may be @c NULL)
 * @param[out] modified Reference to pointer to @c LCFGTagList of components which are modified
 * @param[out] added Reference to pointer to @c LCFGTagList of components which are newly added
 * @param[out] removed Reference to pointer to @c LCFGTagList of components which are removed
 *
 * @return Integer value indicating type of difference
 *
 */

LCFGChange lcfgcompset_quickdiff( const LCFGComponentSet * compset1,
                                  const LCFGComponentSet * compset2,
                                  LCFGTagList ** modified,
                                  LCFGTagList ** added,
                                  LCFGTagList ** removed ) {

  *modified = NULL;
  *added    = NULL;
  *removed  = NULL;

  LCFGTagList * names1;
  if ( compset1 == NULL )
    names1 = lcfgtaglist_new();
  else
    names1 = lcfgcompset_get_components_as_taglist(compset1);

  LCFGTagList * names2;
  if ( compset2 == NULL )
    names2 = lcfgtaglist_new();
  else
    names2 = lcfgcompset_get_components_as_taglist(compset2);

  LCFGTagList * common_comps  = lcfgtaglist_set_intersection( names1, names2 );
  LCFGTagList * added_comps   = lcfgtaglist_set_subtract( names2, names1 );
  LCFGTagList * removed_comps = lcfgtaglist_set_subtract( names1, names2 );

  lcfgtaglist_relinquish(names1);
  lcfgtaglist_relinquish(names2);

  LCFGChange change = LCFG_CHANGE_NONE;

  /* Look for modified components */

  LCFGTagList * modified_comps = lcfgtaglist_new();

  LCFGTagIterator * iter = lcfgtagiter_new(common_comps);
  LCFGTag * tag = NULL;
  while ( change != LCFG_CHANGE_ERROR &&
          ( tag = lcfgtagiter_next(iter) ) != NULL ) {

    const char * comp_name = lcfgtag_get_name(tag);

    const LCFGComponent * comp1 =
      lcfgcompset_find_component( compset1, comp_name );
    const LCFGComponent * comp2 =
      lcfgcompset_find_component( compset2, comp_name );

    LCFGChange comp_diff = lcfgcomponent_quickdiff( comp1, comp2 );
    if ( comp_diff == LCFG_CHANGE_MODIFIED ) {

      LCFGChange rc = lcfgtaglist_append_tag( modified_comps, tag );
      if ( rc == LCFG_CHANGE_ERROR )
        change = LCFG_CHANGE_ERROR;

    }

  }

  lcfgtagiter_destroy(iter);

  lcfgtaglist_relinquish(common_comps);

  if ( change == LCFG_CHANGE_ERROR ) {

    lcfgtaglist_relinquish(added_comps);
    added_comps = NULL;
    lcfgtaglist_relinquish(removed_comps);
    removed_comps = NULL;
    lcfgtaglist_relinquish(modified_comps);
    modified_comps = NULL;

  } else {

    if ( !lcfgtaglist_is_empty(added_comps)   ||
         !lcfgtaglist_is_empty(removed_comps) ||
         !lcfgtaglist_is_empty(modified_comps) ) {
      change = LCFG_CHANGE_MODIFIED;
    }

  }

  /* return the results */

  *added    = added_comps;
  *removed  = removed_comps;
  *modified = modified_comps;

  return change;
}

/**
 * @brief Check for differences between two components
 *
 * This takes two @c LCFGComponent and returns information about
 * whether the component has been removed, added or modified. It does
 * not return any details about which resources have changed just that
 * something has changed.
 *
 * This will return one of:
 *
 *   - @c LCFG_CHANGE_NONE - no changes
 *   - @c LCFG_CHANGE_ADDED - entire component is newly added
 *   - @c LCFG_CHANGE_REMOVED - entire component is removed
 *   - @c LCFG_CHANGE_MODIFIED - at least one resource has changed
 *
 * @param[in] comp1 Pointer to the @e old @c LCFGComponent (may be @c NULL)
 * @param[in] comp2 Pointer to the @e new @c LCFGComponent (may be @c NULL)
 *
 * @return Integer value indicating type of difference
 *
 */

LCFGChange lcfgcomponent_quickdiff( const LCFGComponent * comp1,
                                    const LCFGComponent * comp2 ) {

  if ( comp1 == NULL ) {

    if ( comp2 == NULL )
      return LCFG_CHANGE_NONE;
    else if ( comp2 != NULL )
      return LCFG_CHANGE_ADDED;

  } else {

    if ( comp2 == NULL )
      return LCFG_CHANGE_REMOVED;
    else if ( lcfgcomponent_size(comp1) != lcfgcomponent_size(comp2) )
      return LCFG_CHANGE_MODIFIED;

  }

  LCFGChange status = LCFG_CHANGE_NONE;

  /* Look for resources which have been removed or modified */
  LCFGResourceList ** resources1 = comp1->resources;

  unsigned long i;
  for ( i=0; status == LCFG_CHANGE_NONE && i < comp1->buckets; i++ ) {

    const LCFGResourceList * list = resources1[i];
    if ( !lcfgreslist_is_empty(list) ) {

      const LCFGResource * res1 = lcfgreslist_first_resource(list);

      const char * res1_name = lcfgresource_get_name(res1);

      const LCFGResource * res2 =
        lcfgcomponent_find_resource( comp2, res1_name );

      if ( res2 == NULL || !lcfgresource_same_value( res1, res2 ) )
        status = LCFG_CHANGE_MODIFIED;

    }
  }

  /* Look for resources which have been added */

  LCFGResourceList ** resources2 = comp2->resources;

  for ( i=0; status == LCFG_CHANGE_NONE && i < comp2->buckets; i++ ) {

    const LCFGResourceList * list = resources2[i];
    if ( !lcfgreslist_is_empty(list) ) {
      const char * res2_name = lcfgreslist_get_name(list);

      if ( !lcfgcomponent_has_resource( comp1, res2_name ) )
        status = LCFG_CHANGE_MODIFIED;

    }
  }

  return status;
}

/**
 * @brief Check if the component diff has a particular name
 *
 * This can be used to check if the name for the @c LCFGDiffComponent
 * matches with that specified. The name for the diff is retrieved
 * using the @c lcfgdiffcomponent_get_name().
 *
 * @param[in] compdiff Pointer to @c LCFGDiffComponent
 * @param[in] want_name Component name to match
 *
 * @return Boolean which indicates if diff name matches
 *
 */

bool lcfgdiffcomponent_match( const LCFGDiffComponent * compdiff,
                              const char * want_name ) {
  assert( compdiff != NULL );
  assert( want_name != NULL );

  const char * name = lcfgdiffcomponent_get_name(compdiff);

  return ( !isempty(name) && strcmp( name, want_name ) == 0 );
}

/**
 * @brief Compare two component diffs
 *
 * This compares the names for two @c LCFGDiffComponent, this is mostly
 * useful for sorting lists of diffs. An integer value is returned
 * which indicates lesser than, equal to or greater than in the same
 * way as @c strcmp(3).
 *
 * @param[in] compdiff1 Pointer to @c LCFGDiffComponent
 * @param[in] compdiff2 Pointer to @c LCFGDiffComponent
 * 
 * @return Integer (-1,0,+1) indicating lesser,equal,greater
 *
 */

int lcfgdiffcomponent_compare( const LCFGDiffComponent * compdiff1, 
                               const LCFGDiffComponent * compdiff2 ) {
  assert( compdiff1 != NULL );
  assert( compdiff2 != NULL );

  const char * name1 = lcfgdiffcomponent_get_name(compdiff1);
  if ( name1 == NULL )
    name1 = "";

  const char * name2 = lcfgdiffcomponent_get_name(compdiff2);
  if ( name2 == NULL )
    name2 = "";

  return strcmp( name1, name2 );
}

/**
 * Get the names of the resources for a particular type of change
 *
 * This searches through the list of resources for the @c
 * LCFGDiffComponent to find those which are changed in the specified
 * ways (.e.g. added, removed, modified). It is possible to combine
 * change types so that a single search can match multiple change
 * types. This is done by using a bitwise-OR of the appropriate values
 * (e.g. @c LCFG_CHANGE_ADDED|LCFG_CHANGE_MODIFIED).
 *
 * If only a single type of change is required then it may be simpler
 * to use one of @c lcfgdiffcomponent_modified(), @c
 * lcfgdiffcomponent_added() or @c lcfgdiffcomponent_removed(). If a
 * list of all changed resources (of any type) is required then the @c
 * lcfgdiffcomponent_changed() is most suitable.
 *
 * To avoid memory leaks, when the list of names is no longer required
 * the @c lcfgtaglist_relinquish() function should be called.
 *
 * @param[in] compdiff Pointer to @c LCFGDiffComponent
 * @param[in] change_type Integer indicating types of change
 * @param[out] result Reference to pointer to @c LCFGTagList of resource names
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgdiffcomponent_names_for_type(const LCFGDiffComponent * compdiff,
                                            LCFGChange change_type,
                                            LCFGTagList ** result ) {

  LCFGTagList * res_names = lcfgtaglist_new();

  bool ok = true;

  const LCFGSListNode * cur_node = NULL;
  for ( cur_node = lcfgdiffcomponent_head(compdiff);
	cur_node != NULL && ok;
	cur_node = lcfgdiffcomponent_next(cur_node) ) {

    const LCFGDiffResource * resdiff = lcfgdiffcomponent_resdiff(cur_node);

    const char * res_name = lcfgdiffresource_get_name(resdiff);

    if ( res_name != NULL && 
         change_type & lcfgdiffresource_get_type(resdiff) ) {

      char * msg = NULL;
      LCFGChange change = lcfgtaglist_mutate_add( res_names, res_name, &msg );
      if ( change == LCFG_CHANGE_ERROR )
        ok = false;

      free(msg); /* Just ignoring any message */
    }

  }

  if (ok) {
    lcfgtaglist_sort(res_names);
  } else {
    lcfgtaglist_relinquish(res_names);
    res_names = NULL;
  }

  *result = res_names;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/**
 * Get the names of the changed resources.
 *
 * This searches through the list of resources for the @c
 * LCFGDiffComponent and returns a list of names for those which are
 * changed in any way (may be added, removed or modified).
 *
 * This uses @c lcfgdiffcomponent_names_for_type() to do the search.
 *
 * To avoid memory leaks, when the list of names is no longer required
 * the @c lcfgtaglist_relinquish() function should be called.
 *
 * @param[in] compdiff Pointer to @c LCFGDiffComponent
 * @param[out] res_names Reference to pointer to @c LCFGTagList of resource names
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgdiffcomponent_changed( const LCFGDiffComponent * compdiff,
                                      LCFGTagList ** res_names ) {

  return lcfgdiffcomponent_names_for_type( compdiff,
                    LCFG_CHANGE_ADDED|LCFG_CHANGE_REMOVED|LCFG_CHANGE_MODIFIED,
                                           res_names );
}

/**
 * Get the names of the added resources.
 *
 * This searches through the list of resources for the @c
 * LCFGDiffComponent and returns a list of names for those which are
 * newly added.
 *
 * This uses @c lcfgdiffcomponent_names_for_type() to do the search.
 *
 * To avoid memory leaks, when the list of names is no longer required
 * the @c lcfgtaglist_relinquish() function should be called.
 *
 * @param[in] compdiff Pointer to @c LCFGDiffComponent
 * @param[out] res_names Reference to pointer to @c LCFGTagList of resource names
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgdiffcomponent_added( const LCFGDiffComponent * compdiff,
                                    LCFGTagList ** res_names ) {

  return lcfgdiffcomponent_names_for_type( compdiff,
                                           LCFG_CHANGE_ADDED,
                                           res_names);
}

/**
 * Get the names of the removed resources.
 *
 * This searches through the list of resources for the @c
 * LCFGDiffComponent and returns a list of names for those which are
 * removed.
 *
 * This uses @c lcfgdiffcomponent_names_for_type() to do the search.
 *
 * To avoid memory leaks, when the list of names is no longer required
 * the @c lcfgtaglist_relinquish() function should be called.
 *
 * @param[in] compdiff Pointer to @c LCFGDiffComponent
 * @param[out] res_names Reference to pointer to @c LCFGTagList of resource names
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgdiffcomponent_removed( const LCFGDiffComponent * compdiff,
                                      LCFGTagList ** res_names ) {

  return lcfgdiffcomponent_names_for_type( compdiff,
                                           LCFG_CHANGE_REMOVED,
                                           res_names );
}

/**
 * Get the names of the modified resources.
 *
 * This searches through the list of resources for the @c
 * LCFGDiffComponent and returns a list of names for those which have
 * been modified (note that this does NOT include those which have
 * been added or removed).
 *
 * This uses @c lcfgdiffcomponent_names_for_type() to do the search.
 *
 * To avoid memory leaks, when the list of names is no longer required
 * the @c lcfgtaglist_relinquish() function should be called.
 *
 * @param[in] compdiff Pointer to @c LCFGDiffComponent
 * @param[out] res_names Reference to pointer to @c LCFGTagList of resource names
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgdiffcomponent_modified( const LCFGDiffComponent * compdiff,
                                       LCFGTagList ** res_names ) {

  return lcfgdiffcomponent_names_for_type( compdiff,
                                           LCFG_CHANGE_MODIFIED,
                                           res_names );
}

/* eof */
