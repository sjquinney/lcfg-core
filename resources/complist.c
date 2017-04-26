/**
 * @file resources/complist.c
 * @brief Functions for working with lists of LCFG components
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * $Date$
 * $Revision$
 */

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>

#include "components.h"
#include "utils.h"

/**
 * @brief Create and initialise a new component node
 *
 * This creates a simple wrapper @c LCFGComponentNode node which
 * is used to hold a pointer to an @c LCFGComponent as an item in a
 * @c LCFGComponent data structure.
 *
 * It is typically not necessary to call this function. The usual
 * approach is to use the @c lcfgcomplist_insert_next() or
 * @c lcfgcomplist_append() functions to add @c LCFGComponent structures
 * to the list.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * To avoid memory leaks, when the new structure is no longer required
 * the @c lcfgcomponentnode_destroy() function should be called.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 * @return Pointer to new @c LCFGComponentNode
 *
 */

LCFGComponentNode * lcfgcomponentnode_new(LCFGComponent * comp) {
  assert( comp != NULL );

  LCFGComponentNode * compnode = malloc( sizeof(LCFGComponentNode) );
  if ( compnode == NULL ) {
    perror( "Failed to allocate memory for LCFG component node" );
    exit(EXIT_FAILURE);
  }

  compnode->component = comp;
  compnode->next      = NULL;

  return compnode;
}

/**
 * @brief Destroy a component node
 *
 * When the specified @c LCFGComponentNode is no longer required this
 * can be used to free all associated memory. This will call
 * @c free(3) on each parameter of the struct and then set each value to
 * be @c NULL.
 *
 * Note that destroying an @c LCFGComponentNode does not destroy the
 * associated @c LCFGComponent, that must be done separately.
 *
 * It is typically not necessary to call this function. The usual
 * approach is to use the @c lcfgcomplist_remove_next() function to
 * remove a @c LCFGComponent from the list.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * component node which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] compnode Pointer to @c LCFGComponentNode to be destroyed.
 *
 */

void lcfgcomponentnode_destroy(LCFGComponentNode * compnode) {

  if ( compnode == NULL ) return;

  compnode->component = NULL;
  compnode->next      = NULL;

  free(compnode);
  compnode = NULL;

}

/**
 * @brief Create and initialise a new empty list of components
 *
 * Creates a new @c LCFGComponentList which represents an empty list
 * of components.
 *
 * If the memory allocation for the new structure is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * The reference count for the structure is initialised to 1. To avoid
 * memory leaks, when it is no longer required the
 * @c lcfgcomplist_relinquish() function should be called.
 *
 * @return Pointer to new @c LCFGComponentList
 *
 */

LCFGComponentList * lcfgcomplist_new(void) {

  LCFGComponentList * complist = malloc( sizeof(LCFGComponentList) );
  if ( complist == NULL ) {
    perror( "Failed to allocate memory for LCFG component list" );
    exit(EXIT_FAILURE);
  }

  complist->size = 0;
  complist->head = NULL;
  complist->tail = NULL;
  complist->_refcount = 1;

  return complist;
}

/**
 * @brief Destroy the component list
 *
 * When the specified @c LCFGComponentList is no longer required this
 * will free all associated memory.
 *
 * *Reference Counting:* There is support for very simple reference
 * counting which allows an @c LCFGComponentList to appear in multiple
 * situations. Incrementing and decrementing that reference counter is
 * the responsibility of the container code. See @c
 * lcfgcomplist_acquire() and @c lcfgcomplist_relinquish() for
 * details.
 *
 * This will iterate through the list of components to remove and
 * destroy each @c LCFGComponentNode item, it also calls @c
 * lcfgcomponent_relinquish() for each component. Note that if the
 * reference count on a component reaches zero then the @c LCFGComponent
 * will also be destroyed.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * component list which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] complist Pointer to @c LCFGComponentList to be destroyed.
 *
 */

void lcfgcomplist_destroy(LCFGComponentList * complist) {

  if ( complist == NULL ) return;

  while ( lcfgcomplist_size(complist) > 0 ) {
    LCFGComponent * comp = NULL;
    if ( lcfgcomplist_remove_next( complist, NULL, &comp )
         == LCFG_CHANGE_REMOVED ) {
      lcfgcomponent_relinquish(comp);
    }
  }

  free(complist);
  complist = NULL;
}

/**
 * @brief Acquire reference to component list
 *
 * This is used to record a reference to the @c LCFGComponentList, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the structure is no
 * longer required the @c lcfgcomplist_release() function should be
 * called.
 *
 * @param[in] complist Pointer to @c LCFGComponentList
 *
 */

void lcfgcomplist_acquire(LCFGComponentList * complist) {
  assert( complist != NULL );

  complist->_refcount += 1;
}

/**
 * @brief Release reference to component list
 *
 * This is used to release a reference to the @c LCFGComponentList
 * it does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgcomplist_destroy() function
 * will be called to clean up the memory associated with the structure.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * component list which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] complist Pointer to @c LCFGComponentList
 *
 */

void lcfgcomplist_relinquish(LCFGComponentList * complist) {

  if ( complist == NULL ) return;

  if ( complist->_refcount > 0 )
    complist->_refcount -= 1;

  if ( complist->_refcount == 0 )
    lcfgcomplist_destroy(complist);

}

/**
 * @brief Insert a component into the list
 *
 * This can be used to insert an @c LCFGComponent into the
 * specified @c LCFGComponentList. The component will be wrapped into an
 * @c LCFGComponentNode using the @c lcfgcomponentnode_new() function.
 *
 * The component will be inserted into the list immediately after
 * the specified @c LCFGComponentNode. To insert the component at the
 * head of the list the @c NULL value should be passed for the node.
 *
 * If the component is successfully inserted the @c LCFG_CHANGE_ADDED
 * value is returned, if an error occurs then @c LCFG_CHANGE_ERROR is
 * returned.
 *
 * @param[in] complist Pointer to @c LCFGComponent
 * @param[in] compnode Pointer to @c LCFGComponentNode
 * @param[in] comp Pointer to @c LCFGComponent
 * 
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcomplist_insert_next( LCFGComponentList * complist,
				     LCFGComponentNode * compnode,
				     LCFGComponent     * comp ) {
  assert( complist != NULL );

  LCFGComponentNode * new_node = lcfgcomponentnode_new(comp);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgcomponent_acquire(comp);

  if ( compnode == NULL ) { /* HEAD */

    if ( lcfgcomplist_is_empty(complist) )
      complist->tail = new_node;

    new_node->next = complist->head;
    complist->head = new_node;

  } else {
    
    if ( compnode->next == NULL )
      complist->tail = new_node;

    new_node->next = compnode->next;
    compnode->next = new_node;

  }

  complist->size++;

  return LCFG_CHANGE_ADDED;
}

/**
 * @brief Remove a component from the list
 *
 * This can be used to remove an @c LCFGComponent from the specified
 * @c LCFGComponentList.
 *
 * The component removed from the list is immediately after the
 * specified @c LCFGComponentNode. To remove the component from the head
 * of the list the @c NULL value should be passed for the node.
 *
 * If the component is successfully removed the @c LCFG_CHANGE_REMOVED
 * value is returned, if an error occurs then @c LCFG_CHANGE_ERROR is
 * returned. If the list is already empty then the @c LCFG_CHANGE_NONE
 * value is returned.
 *
 * Note that, since a pointer to the @c LCFGComponent is returned
 * to the caller, the reference count will still be at least 1. To
 * avoid memory leaks, when the struct is no longer required it should
 * be released by calling @c lcfgcomponent_relinquish().
 *
 * @param[in] complist Pointer to @c LCFGComponent
 * @param[in] compnode Pointer to @c LCFGComponentNode
 * @param[out] comp Pointer to @c LCFGComponent
 * 
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcomplist_remove_next( LCFGComponentList * complist,
				     LCFGComponentNode * compnode,
				     LCFGComponent    ** comp ) {
  assert( complist != NULL );
  assert( comp != NULL );

  if ( lcfgcomplist_is_empty(complist) ) return LCFG_CHANGE_NONE;

  LCFGComponentNode * old_node = NULL;

  if ( compnode == NULL ) { /* HEAD */

    old_node       = complist->head;
    complist->head = complist->head->next;

    if ( lcfgcomplist_size(complist) == 1 )
      complist->tail = NULL;

  } else {

    if ( compnode->next == NULL ) return LCFG_CHANGE_ERROR;

    old_node       = compnode->next;
    compnode->next = compnode->next->next;

    if ( compnode->next == NULL )
      complist->tail = compnode;

  }

  complist->size--;

  *comp = lcfgcomplist_component(old_node);

  lcfgcomponentnode_destroy(old_node);

  return LCFG_CHANGE_REMOVED;
}

/**
 * @brief Find the component node with a given name
 *
 * This can be used to search through an @c LCFGComponentList to find
 * the first component node which has a matching name. Note that the
 * matching is done using strcmp(3) which is case-sensitive.
 *
 * A @c NULL value is returned if no matching node is found. Also, a
 * @c NULL value is returned if a @c NULL value or an empty list
 * is specified.
 *
 * @param[in] complist Pointer to @c LCFGComponentList to be searched
 * @param[in] want_name The name of the required component node
 *
 * @return Pointer to an @c LCFGComponentNode (or the @c NULL value).
 *
 */

LCFGComponentNode * lcfgcomplist_find_node( const LCFGComponentList * complist,
                                            const char * want_name ) {
  assert( complist != NULL );
  assert( want_name != NULL );

  if ( lcfgcomplist_is_empty(complist) ) return NULL;

  LCFGComponentNode * result = NULL;

  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = lcfgcomplist_head(complist);
	cur_node != NULL && result == NULL;
	cur_node = lcfgcomplist_next(cur_node) ) {

    const LCFGComponent * comp = lcfgcomplist_component(cur_node);

    if ( !lcfgcomponent_has_name(comp) ) continue;

    const char * comp_name = lcfgcomponent_get_name(comp);

    if ( strcmp( comp_name, want_name ) == 0 )
      result = (LCFGComponentNode *) cur_node;

  }

  return result;
}

/**
 * @brief Check if a list contains a particular component
 *
 * This can be used to search through an @c LCFGComponentList to check if
 * it contains a component with a matching name. Note that the matching
 * is done using strcmp(3) which is case-sensitive.
 * 
 * This uses the @c lcfgcomplist_find_node() function to find the
 * relevant node. If a @c NULL value is specified for the list or the
 * list is empty then a false value will be returned.
 *
 * @param[in] complist Pointer to @c LCFGComponentList to be searched
 * @param[in] name The name of the required component
 *
 * @return Boolean value which indicates presence of component in list
 *
 */

bool lcfgcomplist_has_component(  const LCFGComponentList * complist,
                                  const char * name ) {
  assert( complist != NULL );
  assert( name != NULL );

  return ( lcfgcomplist_find_node( complist, name ) != NULL );
}

/**
 * @brief Find the component for a given name
 *
 * This can be used to search through an @c LCFGComponentList to find
 * the first component which has a matching name. Note
 * that the matching is done using strcmp(3) which is case-sensitive.
 *
 * To ensure the returned @c LCFGComponent is not destroyed when
 * the parent @c LCFGComponentList is destroyed you would need to
 * call the @c lcfgcomponent_acquire() function.
 *
 * @param[in] complist Pointer to @c LCFGComponent to be searched
 * @param[in] want_name The name of the required component
 *
 * @return Pointer to an @c LCFGComponent (or the @c NULL value).
 *
 */

LCFGComponent * lcfgcomplist_find_component( const LCFGComponentList * complist,
                                             const char * want_name ) {
  assert( complist != NULL );
  assert( want_name != NULL );

  LCFGComponent * comp = NULL;

  const LCFGComponentNode * node =
    lcfgcomplist_find_node( complist, want_name );
  if ( node != NULL )
    comp = lcfgcomplist_component(node);

  return comp;
}

/**
 * @brief Find or create a new component
 *
 * Searches the @c LCFGComponentList for an @c LCFGComponent with the
 * required name. If none is found then a new @c LCFGComponent is
 * created and added to the @c LCFGComponent.
 *
 * If an error occurs during the creation of a new component a @c NULL
 * value will be returned.
 *
 * @param[in] complist Pointer to @c LCFGComponentList
 * @param[in] name The name of the required component
 *
 * @return The required @c LCFGComponent (or @c NULL)
 *
 */

LCFGComponent * lcfgcomplist_find_or_create_component(
                                             LCFGComponentList * complist,
                                             const char * name ) {
  assert( complist != NULL );
  assert( name != NULL );

  LCFGComponent * result = lcfgcomplist_find_component( complist, name );

  if ( result != NULL ) return result;

  /* If not found then create new component and add it to the list */
  result = lcfgcomponent_new();

  char * comp_name = strdup(name);
  bool ok = lcfgcomponent_set_name( result, comp_name );

  if ( !ok ) {
    free(comp_name);
  } else {

    if ( lcfgcomplist_append( complist, result ) == LCFG_CHANGE_ERROR )
      ok = false;

  }

  lcfgcomponent_relinquish(result);

  if (!ok)
    result = NULL;

  return result;
}

bool lcfgcomplist_print( const LCFGComponentList * complist,
                         LCFGResourceStyle style,
                         LCFGOption options,
                         FILE * out ) {
  assert( complist != NULL );

  if ( lcfgcomplist_is_empty(complist) ) return true;

  bool ok = true;

  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = lcfgcomplist_head(complist);
	cur_node != NULL && ok;
	cur_node = lcfgcomplist_next(cur_node) ) {

    const LCFGComponent * comp = lcfgcomplist_component(cur_node);

    ok = lcfgcomponent_print( comp, style, options, out );
  }

  return ok;
}

/**
 * @brief Insert or replace a component
 *
 * Searches the @c LCFGComponentList for a matching @c LCFGComponent with
 * the same name. If none is found the component is simply added and @c
 * LCFG_CHANGE_ADDED is returned. If there is a match then the new
 * component will replace the current and @c LCFG_CHANGE_REPLACED
 * is returned.
 *
 * @param[in] complist Pointer to @c LCFGComponentList
 * @param[in] new_comp Pointer to @c LCFGComponent
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcomplist_insert_or_replace_component(
                                              LCFGComponentList * complist,
                                              LCFGComponent * new_comp,
                                              char ** msg ) {
  assert( complist != NULL );
  assert( new_comp != NULL );

  LCFGChange result = LCFG_CHANGE_ERROR;

  /* Name for component is required */
  if ( !lcfgcomponent_has_name(new_comp) ) return LCFG_CHANGE_ERROR;

  LCFGComponentNode * cur_node =
    lcfgcomplist_find_node( complist, lcfgcomponent_get_name(new_comp) );

  if ( cur_node == NULL ) {
    result = lcfgcomplist_append( complist, new_comp );
  } else {
    LCFGComponent * cur_comp = lcfgcomplist_component(cur_node);

    /* replace current version of resource with new one */

    lcfgcomponent_acquire(new_comp);
    cur_node->component = new_comp;

    lcfgcomponent_relinquish(cur_comp);

    result = LCFG_CHANGE_REPLACED;
  }

  return result;
}

LCFGChange lcfgcomplist_merge( LCFGComponentList * list1,
			       const LCFGComponentList * list2,
			       bool take_new,
			       char ** msg ) {
  assert( list1 != NULL );

  /* No overrides to apply if second list is empty */
  if ( lcfgcomplist_is_empty(list2) ) return LCFG_CHANGE_NONE;

  /* Only overriding existing components so nothing to do if list is empty */
  if ( lcfgcomplist_is_empty(list1) && !take_new ) return LCFG_CHANGE_NONE;

  LCFGChange change = LCFG_CHANGE_NONE;

  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = lcfgcomplist_head(list2);
	cur_node != NULL && change != LCFG_CHANGE_ERROR;
	cur_node = lcfgcomplist_next(cur_node) ) {

    LCFGComponent * override_comp = lcfgcomplist_component(cur_node);
    if ( !lcfgcomponent_has_name(override_comp) ) continue;

    const char * comp_name =  lcfgcomponent_get_name(override_comp);

    LCFGComponent * target_comp =
      lcfgcomplist_find_component( list1, comp_name );

    LCFGChange rc = LCFG_CHANGE_NONE;
    if ( target_comp != NULL ) {
      rc = lcfgcomponent_merge( target_comp, override_comp, msg );
    } else if ( take_new ) {
      rc = lcfgcomplist_append( list1, override_comp );
    }

    if ( rc == LCFG_CHANGE_ERROR ) {
      change = LCFG_CHANGE_ERROR;
    } else if ( rc != LCFG_CHANGE_NONE ) {
      change = LCFG_CHANGE_MODIFIED;
    }

  }

  return change;
}

LCFGChange lcfgcomplist_transplant_components( LCFGComponentList * list1,
					       const LCFGComponentList * list2,
					       char ** msg ) {
  assert( list1 != NULL );

  if ( lcfgcomplist_is_empty(list2) ) return LCFG_STATUS_OK;

  LCFGStatus change = LCFG_CHANGE_NONE;

  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = lcfgcomplist_head(list2);
	cur_node != NULL && change != LCFG_CHANGE_ERROR;
	cur_node = lcfgcomplist_next(cur_node) ) {

    LCFGComponent * cur_comp = lcfgcomplist_component(cur_node);

    LCFGChange rc =
      lcfgcomplist_insert_or_replace_component( list1, cur_comp, msg );

    if ( rc == LCFG_CHANGE_ERROR ) {
      change = LCFG_CHANGE_ERROR;
    } else if ( rc != LCFG_CHANGE_NONE ) {
      change = LCFG_CHANGE_MODIFIED;
    }

  }

  return change;
}

LCFGStatus lcfgcomplist_from_status_dir( const char * status_dir,
                                         LCFGComponentList ** result,
                                         const LCFGTagList * comps_wanted,
					 LCFGOption options,
                                         char ** msg ) {
  assert( status_dir != NULL );

  *result = NULL;

  if ( isempty(status_dir) ) {
    lcfgutils_build_message( msg, "Invalid status directory name" );
    return LCFG_STATUS_ERROR;
  }

  LCFGStatus status = LCFG_STATUS_OK;

  /* Create the new empty component list which will eventually be returned */

  LCFGComponentList * complist = lcfgcomplist_new();

  DIR * dh;
  if ( ( dh = opendir(status_dir) ) == NULL ) {

    if ( errno == ENOENT || errno == ENOTDIR ) {

      if ( !(options&LCFG_OPT_ALLOW_NOEXIST) ) {
	lcfgutils_build_message( msg, "Status directory '%s' does not exist", status_dir );
	status = LCFG_STATUS_ERROR;
      }

    } else {
      lcfgutils_build_message( msg, "Status directory '%s' is not readable", status_dir );
      status = LCFG_STATUS_ERROR;
    }

    goto cleanup;
  }

  struct dirent *entry;
  struct stat sb;

  while( ( entry = readdir(dh) ) != NULL ) {

    char * comp_name = entry->d_name;

    if ( *comp_name == '.' ) continue; /* ignore any dot files */

    /* ignore any file which is not a valid component name */
    if ( !lcfgcomponent_valid_name(comp_name) ) continue;

    /* Ignore any filename which is not in the list of wanted components. */
    if ( !lcfgtaglist_is_empty(comps_wanted) &&
         !lcfgtaglist_contains( comps_wanted, comp_name ) ) {
      continue;
    }

    char * status_file  = lcfgutils_catfile( status_dir, comp_name );

    if ( stat( status_file, &sb ) == 0 && S_ISREG(sb.st_mode) ) {

      LCFGComponent * component = NULL;
      char * read_msg = NULL;
      status = lcfgcomponent_from_status_file( status_file,
					       &component,
					       comp_name,
					       options,
					       &read_msg );

      if ( status == LCFG_STATUS_ERROR ) {
        lcfgutils_build_message( msg, "Failed to read status file '%s': %s",
                  status_file, read_msg );
      }

      free(read_msg);

      if ( status != LCFG_STATUS_ERROR && !lcfgcomponent_is_empty(component) ) {

        char * insert_msg = NULL;
        LCFGChange insert_rc =
          lcfgcomplist_insert_or_replace_component( complist, component,
                                                    &insert_msg );

        if ( insert_rc == LCFG_CHANGE_ERROR ) {
          status = LCFG_STATUS_ERROR;
          lcfgutils_build_message( msg, "Failed to read status file '%s': %s",
                    status_file, insert_msg );
        }

	free(insert_msg);

      }

      lcfgcomponent_relinquish(component);
    }

    free(status_file);
    status_file = NULL;

    if ( status != LCFG_STATUS_OK ) break;
  }

  closedir(dh);

 cleanup:

  if ( status == LCFG_STATUS_ERROR ) {
    lcfgcomplist_destroy(complist);
    complist = NULL;
  }

  *result = complist;

  return status;
}

LCFGStatus lcfgcomplist_to_status_dir( const LCFGComponentList * complist,
				       const char * status_dir,
				       LCFGOption options,
				       char ** msg ) {
  assert( complist != NULL );
  assert( status_dir != NULL );

  if ( isempty(status_dir) ) {
    lcfgutils_build_message( msg, "Invalid status directory name" );
    return LCFG_STATUS_ERROR;
  }

  if ( lcfgcomplist_is_empty(complist) ) return LCFG_STATUS_OK;

  LCFGStatus rc = LCFG_STATUS_OK;

  struct stat sb;
  if ( stat( status_dir, &sb ) != 0 ) {

    if ( errno == ENOENT ) {

      if ( mkdir( status_dir, 0700 ) != 0 ) {
	rc = LCFG_STATUS_ERROR;
	lcfgutils_build_message( msg, "Cannot write component status files into '%s', directory does not exist and cannot be created", status_dir );
      }

    } else {
      rc = LCFG_STATUS_ERROR;
      lcfgutils_build_message( msg, "Cannot write component status files into '%s', directory is not accessible", status_dir );
    }

  } else if ( !S_ISDIR(sb.st_mode) ) {
    rc = LCFG_STATUS_ERROR;
    lcfgutils_build_message( msg, "Cannot write component status files into '%s', path exists but is not a directory", status_dir );
  }

  if ( rc != LCFG_STATUS_OK ) return rc;

  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = lcfgcomplist_head(complist);
	cur_node != NULL && rc != LCFG_STATUS_ERROR;
	cur_node = lcfgcomplist_next(cur_node) ) {

    LCFGComponent * cur_comp = lcfgcomplist_component(cur_node);

    if ( !lcfgcomponent_has_name(cur_comp) ) continue;

    const char * comp_name = lcfgcomponent_get_name(cur_comp);

    char * statfile = lcfgutils_catfile( status_dir, comp_name );

    /* Sort the list of resources so that the status file is always
       produced in the same order - makes comparisons simpler. */

    lcfgcomponent_sort(cur_comp);

    char * comp_msg = NULL;
    rc = lcfgcomponent_to_status_file( cur_comp, statfile,
				       options, &comp_msg );

    if ( rc == LCFG_STATUS_ERROR ) {
      lcfgutils_build_message( msg, "Failed to write status file for '%s' component: %s",
		comp_name, comp_msg );
    }

    free(comp_msg);
    free(statfile);

  }

  return rc;
}

/**
 * @brief Get the list of component names as a taglist
 *
 * This generates a new @c LCFGTagList which contains a list of
 * component names for the @c LCFGComponentList. If the list is empty
 * then an empty tag list will be returned. Will return @c NULL if an
 * error occurs.
 *
 * To avoid memory leaks, when the list is no longer required the 
 * @c lcfgtaglist_relinquish() function should be called.
 *
 * @param[in] complist Pointer to @c LCFGComponentList
 * @param[in] options Integer which controls behaviour.
 *
 * @return Pointer to a new @c LCFGTagList of component names
 *
 */

LCFGTagList * lcfgcomplist_get_components_as_taglist(
					        const LCFGComponent * complist,
						LCFGOption options ) {
  assert( complist != NULL );

  bool all_priorities = (options&LCFG_OPT_ALL_PRIORITIES);

  LCFGTagList * comp_names = lcfgtaglist_new();

  bool ok = true;

  const LCFGComponentNode * cur_node = NULL;
  for ( cur_node = lcfgcomplist_head(complist);
        cur_node != NULL && ok;
        cur_node = lcfgcomplist_next(cur_node) ) {

    const LCFGComponent * comp = lcfgcomplist_component(cur_node);

    /* Ignore any without names */
    if ( !lcfgcomponent_is_valid(comp) ) continue;

    const char * comp_name = lcfgcomponent_get_name(comp);

    char * msg = NULL;
    LCFGChange change = lcfgtaglist_mutate_add( comp_names, comp_name, &msg );
    if ( change == LCFG_CHANGE_ERROR )
      ok = false;

    free(msg); /* Just ignoring any message */
  }

  if (!ok) {
    lcfgtaglist_relinquish(comp_names);
    comp_names = NULL;
  }

  return comp_names;
}

/**
 * @brief Get the list of component names as a string
 *
 * This generates a new string which contains a space-separated sorted
 * list of component names for the @c LCFGComponentList. If the list
 * is empty then an empty string will be returned.
 *
 * @param[in] complist Pointer to @c LCFGComponent
 * @param[in] options Integer which controls behaviour.
 *
 * @return Pointer to a new string (call @c free(3) when no longer required)
 *
 */

char * lcfgcomplist_get_components_as_string(const LCFGComponentList * complist,
					     LCFGOption options ) {
  assert( complist != NULL );

  if ( lcfgcomplist_is_empty(complist) ) return strdup("");

  LCFGTagList * comp_names =
    lcfgcomplist_get_components_as_taglist( complist, options );

  if ( comp_names == NULL ) return NULL;

  lcfgtaglist_sort(comp_names);

  size_t buf_len = 0;
  char * res_as_str = NULL;

  if ( lcfgtaglist_to_string( comp_names, 0, &res_as_str, &buf_len ) < 0 ) {
    free(res_as_str);
    res_as_str = NULL;
  }

  lcfgtaglist_relinquish(comp_names);

  return res_as_str;
}

/* eof */
