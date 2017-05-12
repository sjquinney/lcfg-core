/**
 * @file resources/components/diff.c
 * @brief Functions for finding the differences between LCFG components
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date: 2017-05-12 10:44:21 +0100 (Fri, 12 May 2017) $
 * $Revision: 32703 $
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "utils.h"
#include "differences.h"

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
 * If the @c md5state parameter is not @c NULL this function will call
 * @c lcfgutils_md5_append() to update the state for each line written
 * into the file. This is used to generate a signature for the entire
 * file.
 *
 * @param[in] compdiff Pointer to @c LCFGDiffComponent
 * @param[in] holdfile File stream to which diff should be written
 * @param[in] md5state MD5 state structure to which data is written
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgdiffcomponent_to_holdfile( const LCFGDiffComponent * compdiff,
                                          FILE * holdfile,
                                          md5_state_t * md5state ) {

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

      if ( md5state != NULL )
        lcfgutils_md5_append( md5state, (const md5_byte_t *) buffer, rc );

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

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = comp1 != NULL ? lcfgcomponent_head(comp1) : NULL;
	cur_node != NULL && ok;
	cur_node = lcfgcomponent_next(cur_node) ) {

    LCFGResource * res1 = lcfgcomponent_resource(cur_node);

    /* Only diff 'active' resources which have a name attribute */
    if ( !lcfgresource_is_active(res1) || !lcfgresource_is_valid(res1) )
      continue;

    const char * res1_name = lcfgresource_get_name(res1);

    LCFGResource * res2 = comp2 != NULL ?
      lcfgcomponent_find_resource( comp2, res1_name, false ) : NULL;

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

  /* Look for resources which have been added */

  for ( cur_node = comp2 != NULL ? lcfgcomponent_head(comp2) : NULL;
	cur_node != NULL && ok;
	cur_node = lcfgcomponent_next(cur_node) ) {

    LCFGResource * res2 = lcfgcomponent_resource(cur_node);

    /* Only diff 'active' resources which have a name attribute */
    if ( !lcfgresource_is_active(res2) || !lcfgresource_is_valid(res2) )
      continue;

    const char * res2_name = lcfgresource_get_name(res2);

    /* Only interested in resources which are NOT in first component */

    if ( comp1 == NULL ||
	 !lcfgcomponent_has_resource( comp1, res2_name, false ) ) {

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
 * @brief Check for differences between lists of components
 *
 * This takes two @c LCFGComponentList and returns lists of names of
 * components which have been removed, added or modified. It does not
 * return any details about which resources have changed just that
 * something has changed.
 *
 * This will return @c LCFG_CHANGE_MODIFIED if there are any
 * differences and @c LCFG_CHANGE_NONE otherwise.
 *
 * @param[in] list1 Pointer to the @e old @c LCFGComponentList (may be @c NULL)
 * @param[in] list2 Pointer to the @e new @c LCFGComponentList (may be @c NULL)
 * @param[out] modified Reference to pointer to @c LCFGTagList of components which are modified
 * @param[out] added Reference to pointer to @c LCFGTagList of components which are newly added
 * @param[out] removed Reference to pointer to @c LCFGTagList of components which are removed
 *
 * @return Integer value indicating type of difference
 *
 */

LCFGChange lcfgcomplist_quickdiff( const LCFGComponentList * list1,
                                   const LCFGComponentList * list2,
                                   LCFGTagList ** modified,
                                   LCFGTagList ** added,
                                   LCFGTagList ** removed ) {

  *modified = NULL;
  *added    = NULL;
  *removed  = NULL;

  LCFGChange status = LCFG_CHANGE_NONE;

  /* Look for components which have been removed or modified */
  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = list1 != NULL ? lcfgcomplist_head(list1) : NULL;
	cur_node != NULL && status != LCFG_CHANGE_ERROR;
	cur_node = lcfgcomplist_next(cur_node) ) {

    const LCFGComponent * comp1 = lcfgcomplist_component(cur_node);
    if ( !lcfgcomponent_is_valid(comp1) ) continue;

    const char * comp1_name = lcfgcomponent_get_name(comp1);

    const LCFGComponent * comp2 =
      lcfgcomplist_find_component( list2, comp1_name );

    LCFGChange comp_diff = lcfgcomponent_quickdiff( comp1, comp2 );

    LCFGTagList ** taglist = NULL;

    switch(comp_diff)
      {
      case LCFG_CHANGE_ERROR:
        status = LCFG_CHANGE_ERROR;
        break;
      case LCFG_CHANGE_MODIFIED:
        status  = LCFG_CHANGE_MODIFIED;
        taglist = &( *modified );
        break;
      case LCFG_CHANGE_REMOVED:
        status  = LCFG_CHANGE_MODIFIED;
        taglist = &( *removed );
        break;
      case LCFG_CHANGE_NONE:
      case LCFG_CHANGE_ADDED:
      case LCFG_CHANGE_REPLACED:
        break; /* nothing to do */
      }

    if ( taglist != NULL ) { /* A component name needs storing */

      if ( *taglist == NULL )
        *taglist = lcfgtaglist_new();

      char * tagmsg = NULL;
      if ( lcfgtaglist_mutate_add( *taglist, comp1_name, &tagmsg )
           == LCFG_CHANGE_ERROR ) {
        status = LCFG_CHANGE_ERROR;
      }
      free(tagmsg);

    }

  }

  /* Look for components which have been added */

  for ( cur_node = list2 != NULL ? lcfgcomplist_head(list2) : NULL;
	cur_node != NULL && status != LCFG_CHANGE_ERROR;
	cur_node = lcfgcomplist_next(cur_node) ) {

    const LCFGComponent * comp2 = lcfgcomplist_component(cur_node);
    if ( !lcfgcomponent_is_valid(comp2) ) continue;

    const char * comp2_name = lcfgcomponent_get_name(comp2);

    if ( !lcfgcomplist_has_component( list1, comp2_name ) ) {
      status = LCFG_CHANGE_MODIFIED;

      if ( *added == NULL )
        *added = lcfgtaglist_new();

      char * tagmsg = NULL;
      if ( lcfgtaglist_mutate_add( *added, comp2_name, &tagmsg )
           == LCFG_CHANGE_ERROR ) {
        status = LCFG_CHANGE_ERROR;
      }
      free(tagmsg);

    }

  }

  if ( status == LCFG_CHANGE_ERROR || status == LCFG_CHANGE_NONE ) {
    lcfgtaglist_relinquish(*modified);
    *modified = NULL;
    lcfgtaglist_relinquish(*added);
    *added    = NULL;
    lcfgtaglist_relinquish(*removed);
    *removed  = NULL;
  }

  return status;
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
  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = comp1 != NULL ? lcfgcomponent_head(comp1) : NULL;
	cur_node != NULL;
	cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res1 = lcfgcomponent_resource(cur_node);

    /* Only diff 'active' resources which have a name attribute */
    if ( !lcfgresource_is_active(res1) || !lcfgresource_is_valid(res1) )
      continue;

    const char * res1_name = lcfgresource_get_name(res1);

    const LCFGResource * res2 =
      lcfgcomponent_find_resource( comp2, res1_name, false );

    if ( res2 == NULL || !lcfgresource_same_value( res1, res2 ) ) {
      status = LCFG_CHANGE_MODIFIED;
      break;
    }

  }

  if ( status == LCFG_CHANGE_NONE ) {

    /* Look for resources which have been added */

    for ( cur_node = comp2 != NULL ? lcfgcomponent_head(comp2) : NULL;
          cur_node != NULL;
          cur_node = lcfgcomponent_next(cur_node) ) {

      const LCFGResource * res2 = lcfgcomponent_resource(cur_node);

      /* Only diff 'active' resources which have a name attribute */
      if ( !lcfgresource_is_active(res2) || !lcfgresource_is_valid(res2) )
	continue;

      const char * res2_name = lcfgresource_get_name(res2);

      if ( !lcfgcomponent_has_resource( comp1, res2_name, false ) ) {
        status = LCFG_CHANGE_MODIFIED;
        break;
      }

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

/* eof */
