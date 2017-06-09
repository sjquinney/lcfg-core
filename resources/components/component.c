/**
 * @file resources/components/component.c
 * @brief Functions for working with LCFG components
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "components.h"
#include "utils.h"

/**
 * @brief Create and initialise a new resource node
 *
 * This creates a simple wrapper @c LCFGResourceNode node which
 * is used to hold a pointer to an @c LCFGResource as an item in a
 * @c LCFGComponent data structure.
 *
 * It is typically not necessary to call this function. The usual
 * approach is to use the @c lcfgcomponent_insert_next() or
 * @c lcfgcomponent_append() functions to add @c LCFGResource structures
 * to the list.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * To avoid memory leaks, when the new structure is no longer required
 * the @c lcfgresourcenode_destroy() function should be called.
 *
 * @param[in] res Pointer to @c LCFGResource
 *
 * @return Pointer to new @c LCFGResourceNode
 *
 */

LCFGResourceNode * lcfgresourcenode_new(LCFGResource * res) {
  assert( res != NULL );

  LCFGResourceNode * resnode = malloc( sizeof(LCFGResourceNode) );
  if ( resnode == NULL ) {
    perror( "Failed to allocate memory for LCFG resource node" );
    exit(EXIT_FAILURE);
  }

  /* Set default values */

  resnode->resource = res;
  resnode->next     = NULL;

  return resnode;
}

/**
 * @brief Destroy a resource node
 *
 * When the specified @c LCFGResourceNode is no longer required this
 * can be used to free all associated memory. This will call
 * @c free(3) on each parameter of the struct and then set each value to
 * be @c NULL.
 *
 * Note that destroying an @c LCFGResourceNode does not destroy the
 * associated @c LCFGResource, that must be done separately.
 *
 * It is typically not necessary to call this function. The usual
 * approach is to use the @c lcfgcomponent_remove_next() function to
 * remove a @c LCFGResource from the list.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * resource node which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] resnode Pointer to @c LCFGResourceNode to be destroyed.
 *
 */

void lcfgresourcenode_destroy(LCFGResourceNode * resnode) {

  if ( resnode == NULL ) return;

  resnode->resource = NULL;
  resnode->next     = NULL;

  free(resnode);
  resnode = NULL;

}

/**
 * @brief Create and initialise a new empty component
 *
 * Creates a new @c LCFGComponent which represents an empty component.
 *
 * If the memory allocation for the new structure is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * The reference count for the structure is initialised to 1. To avoid
 * memory leaks, when it is no longer required the
 * @c lcfgcomponent_relinquish() function should be called.
 *
 * @return Pointer to new @c LCFGComponent
 *
 */

LCFGComponent * lcfgcomponent_new(void) {

  LCFGComponent * comp = malloc( sizeof(LCFGComponent) );
  if ( comp == NULL ) {
    perror( "Failed to allocate memory for LCFG component" );
    exit(EXIT_FAILURE);
  }

  /* Set default values */

  comp->name = NULL;
  comp->merge_rules = LCFG_MERGE_RULE_NONE;
  comp->size = 0;
  comp->head = NULL;
  comp->tail = NULL;
  comp->_refcount = 1;

  return comp;
}

/**
 * @brief Remove all the resources for the component
 *
 * This will remove all the @c LCFGResource associated with the @c
 * LCFGComponent. The @c lcfgresource_relinquish() function will be
 * called on each one, if the reference count for any reaches zero
 * they will be destroyed.
 *
 * @param[in] comp The @c LCFGComponent to be emptied.
 *
 */

void lcfgcomponent_remove_all_resources( LCFGComponent * comp ) {

  while ( lcfgcomponent_size(comp) > 0 ) {
    LCFGResource * resource = NULL;
    if ( lcfgcomponent_remove_next( comp, NULL, &resource )
         == LCFG_CHANGE_REMOVED ) {
      lcfgresource_relinquish(resource);
    }
  }

}

/**
 * @brief Destroy the component
 *
 * When the specified @c LCFGComponent is no longer required this
 * will free all associated memory.
 *
 * *Reference Counting:* There is support for very simple reference
 * counting which allows an @c LCFGComponent to appear in multiple
 * situations. This is particular useful for code which needs to use
 * multiple iterators for a single component. Incrementing and
 * decrementing that reference counter is the responsibility of the
 * container code. See @c lcfgcomponent_acquire() and
 * @c lcfgcomponent_relinquish() for details.
 *
 * This will iterate through the list of resources to remove and
 * destroy each @c LCFGResourceNode item, it also calls @c
 * lcfgresource_relinquish() for each resource. Note that if the
 * reference count on a resource reaches zero then the @c LCFGResource
 * will also be destroyed.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * component which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] comp Pointer to @c LCFGComponent to be destroyed.
 *
 */

void lcfgcomponent_destroy(LCFGComponent * comp) {

  if ( comp == NULL ) return;

  lcfgcomponent_remove_all_resources(comp);

  free(comp->name);
  comp->name = NULL;

  free(comp);
  comp = NULL;

}

/**
 * @brief Acquire reference to component
 *
 * This is used to record a reference to the @c LCFGComponent, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the structure is no
 * longer required the @c lcfgcomponent_release() function should be
 * called.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 */

void lcfgcomponent_acquire(LCFGComponent * comp) {
  assert( comp != NULL );

  comp->_refcount += 1;
}

/**
 * @brief Release reference to component
 *
 * This is used to release a reference to the @c LCFGComponent
 * it does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgcomponent_destroy() function
 * will be called to clean up the memory associated with the structure.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * component which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 */

void lcfgcomponent_relinquish(LCFGComponent * comp) {

  if ( comp == NULL ) return;

  if ( comp->_refcount > 0 )
    comp->_refcount -= 1;

  if ( comp->_refcount == 0 )
    lcfgcomponent_destroy(comp);

}

/**
 * @brief Set the component merge rules
 *
 * An @c LCFGComponent may have a set of rules which control how
 * resources should be 'merged' when using the @c
 * lcfgcomponent_merge_resource() and @c lcfgcomponent_merge_component()
 * functions. For full details, see the documentation for the @c
 * lcfgcomponent_merge_resource() function. The following rules are
 * supported:
 *
 *   - LCFG_MERGE_RULE_NONE - null rule (the default)
 *   - LCFG_MERGE_RULE_KEEP_ALL - keep all resources
 *   - LCFG_MERGE_RULE_SQUASH_IDENTICAL - ignore additional identical resources
 *   - LCFG_MERGE_RULE_USE_PRIORITY - resolve conflicts using context priority 
 *   - LCFG_MERGE_RULE_USE_PREFIX - mutate resource according to prefix (TODO)
 *   - LCFG_MERGE_RULE_REPLACE - replace any existing resource which matches
 *
 * Rules can be used in any combination by using a @c '|' (bitwise
 * 'or').
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] new_rules Integer merge rules
 *
 * @return boolean indicating success
 *
 */

bool lcfgcomponent_set_merge_rules( LCFGComponent * comp,
                                    LCFGMergeRule new_rules ) {
  assert( comp != NULL );

  comp->merge_rules = new_rules;

  return true;
}

/**
 * @brief Get the current component merge rules
 *
 * An @c LCFGComponent may have a set of rules which control how
 * resources should be 'merged' when using the @c
 * lcfgcomponent_merge_resource() and @c lcfgcomponent_merge_component()
 * functions. For full details, see the documentation for the @c
 * lcfgcomponent_merge_resource() function.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 * @return Integer merge rules
 *
 */

LCFGMergeRule lcfgcomponent_get_merge_rules( const LCFGComponent * comp ) {
  assert( comp != NULL );

  return comp->merge_rules;
}

/**
 * @brief Check if a string is a valid LCFG component name
 *
 * Checks the contents of a specified string against the specification
 * for an LCFG component name.
 *
 * An LCFG component name MUST be at least one character in length. The
 * first character MUST be in the class @c [A-Za-z] and all other
 * characters MUST be in the class @c [A-Za-z0-9_]. This means they
 * are safe to use as variable names for languages such as bash.
 *
 * @param[in] name String to be tested
 *
 * @return boolean which indicates if string is a valid component name
 *
 */

bool lcfgcomponent_valid_name(const char * name) {
  return lcfgresource_valid_name(name);
}

/**
 * @brief Check validity of component
 *
 * Checks the specified @c LCFGComponent to ensure that it is valid. For a
 * component to be considered valid the pointer must not be @c NULL and
 * the component must have a name.
 *
 * @param[in] comp Pointer to an @c LCFGComponent
 *
 * @return Boolean which indicates if component is valid
 *
 */

bool lcfgcomponent_is_valid( const LCFGComponent * comp ) {
  return ( comp != NULL && lcfgcomponent_has_name(comp) );
}

/**
 * @brief Check if the component has a name
 *
 * Checks if the specified @c LCFGComponent currently has a value set
 * for the name attribute. Although a name is required for an LCFG
 * component to be valid it is possible for the value of the name to be
 * set to @c NULL when the structure is first created.
 *
 * @param[in] comp Pointer to an @c LCFGComponent
 *
 * @return boolean which indicates if a component has a name
 *
 */

bool lcfgcomponent_has_name(const LCFGComponent * comp) {
  assert( comp != NULL );

  return !isempty(comp->name);
}

/**
 * @brief Get the name for the component
 *
 * This returns the value of the @e name parameter for the
 * @c LCFGComponent. If the component does not currently have a @e
 * name then the pointer returned will be @c NULL.
 *
 * It is important to note that this is @b NOT a copy of the string,
 * changing the returned string will modify the @e name of the
 * component.
 *
 * @param[in] comp Pointer to an @c LCFGComponent
 *
 * @return The @e name for the component (possibly @c NULL).
 */

const char * lcfgcomponent_get_name(const LCFGComponent * comp) {
  assert( comp != NULL );

  return comp->name;
}

/**
 * @brief Set the name for the component
 *
 * Sets the value of the @e name parameter for the @c LCFGComponent
 * to that specified. It is important to note that this does
 * @b NOT take a copy of the string. Furthermore, once the value is set
 * the component assumes "ownership", the memory will be freed if the
 * name is further modified or the component is destroyed.
 *
 * Before changing the value of the @e name to be the new string it
 * will be validated using the @c lcfgcomponent_valid_name()
 * function. If the new string is not valid then no change will occur,
 * the @c errno will be set to @c EINVAL and the function will return
 * a @c false value.
 *
 * @param[in] comp Pointer to an @c LCFGComponent
 * @param[in] new_name String which is the new name
 *
 * @return boolean indicating success
 *
 */

bool lcfgcomponent_set_name( LCFGComponent * comp, char * new_name ) {
  assert( comp != NULL );

  bool ok = false;
  if ( lcfgcomponent_valid_name(new_name) ) {
    free(comp->name);

    comp->name = new_name;
    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/**
 * @brief Insert a resource into the list
 *
 * This can be used to insert an @c LCFGResource into the
 * specified component. The resource will be wrapped into an
 * @c LCFGResourceNode using the @c lcfgresourcenode_new() function.
 *
 * The resource will be inserted into the component immediately after
 * the specified @c LCFGResourceNode. To insert the resource at the
 * head of the list the @c NULL value should be passed for the node.
 *
 * If the resource is successfully inserted the @c LCFG_CHANGE_ADDED
 * value is returned, if an error occurs then @c LCFG_CHANGE_ERROR is
 * returned.
 *
 * @param[in] list Pointer to @c LCFGComponent
 * @param[in] node Pointer to @c LCFGResourceNode
 * @param[in] item Pointer to @c LCFGResource
 * 
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcomponent_insert_next( LCFGComponent    * list,
                                      LCFGResourceNode * node,
                                      LCFGResource     * item ) {
  assert( list != NULL );

  LCFGResourceNode * new_node = lcfgresourcenode_new(item);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgresource_acquire(item);

  if ( node == NULL ) { /* HEAD */

    if ( lcfgcomponent_is_empty(list) )
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
 * @brief Remove a resource from the component
 *
 * This can be used to remove an @c LCFGResource from the specified
 * component.
 *
 * The resource removed from the component is immediately after the
 * specified @c LCFGResourceNode. To remove the resource from the head
 * of the list the @c NULL value should be passed for the node.
 *
 * If the resource is successfully removed the @c LCFG_CHANGE_REMOVED
 * value is returned, if an error occurs then @c LCFG_CHANGE_ERROR is
 * returned. If the list is already empty then the @c LCFG_CHANGE_NONE
 * value is returned.
 *
 * Note that, since a pointer to the @c LCFGResource is returned
 * to the caller, the reference count will still be at least 1. To
 * avoid memory leaks, when the struct is no longer required it should
 * be released by calling @c lcfgresource_relinquish().
 *
 * @param[in] list Pointer to @c LCFGComponent
 * @param[in] node Pointer to @c LCFGResourceNode
 * @param[out] item Pointer to @c LCFGResource
 * 
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcomponent_remove_next( LCFGComponent    * list,
                                      LCFGResourceNode * node,
                                      LCFGResource    ** item ) {
  assert( list != NULL );

  if ( lcfgcomponent_is_empty(list) ) return LCFG_CHANGE_NONE;

  LCFGResourceNode * old_node = NULL;

  if ( node == NULL ) { /* HEAD */

    old_node   = list->head;
    list->head = list->head->next;

    if ( lcfgcomponent_size(list) == 1 )
      list->tail = NULL;

  } else {

    if ( node->next == NULL ) return LCFG_CHANGE_ERROR;

    old_node   = node->next;
    node->next = node->next->next;

    if ( node->next == NULL )
      list->tail = node;

  }

  list->size--;

  *item = lcfgcomponent_resource(old_node);

  lcfgresourcenode_destroy(old_node);

  return LCFG_CHANGE_REMOVED;
}

/**
 * @brief Clone a component
 *
 * This will create a clone of the specified component. If the @c
 * deep_copy parameter is false then this is a shallow-copy and the
 * resources are shared by the two components. If the @c deep_copy
 * parameter is true then the resources will also be cloned using the
 * @c lcfgresource_clone() function.
 *
 * @param[in] comp Pointer to @c LCFGComponent to be cloned.
 * @param[in] deep_copy Boolean that controls whether resources are also cloned.
 *
 * @return Pointer to new clone @c LCFGComponent (@c NULL if error occurs)
 *
 */

LCFGComponent * lcfgcomponent_clone( LCFGComponent * comp, bool deep_copy ) {

  bool ok = true;

  LCFGComponent * comp_clone = lcfgcomponent_new();

  /* Copy over the name if present */

  if ( lcfgcomponent_has_name(comp) ) {
    char * new_name = strdup( lcfgcomponent_get_name(comp) );
    ok = lcfgcomponent_set_name( comp_clone, new_name );
    if ( !ok ) {
      free(new_name);
      goto cleanup;
    }
  }

  /* Copy over the merge rules */

  ok = lcfgcomponent_set_merge_rules( comp_clone,
				      lcfgcomponent_get_merge_rules(comp) );
  if (!ok) goto cleanup;

  /* Copy the resources */

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
	cur_node != NULL && ok;
	cur_node = lcfgcomponent_next(cur_node) ) {

    LCFGResource * res = lcfgcomponent_resource(cur_node);

    LCFGResource * res_clone = NULL;
    if ( deep_copy ) {
      res_clone = lcfgresource_clone(res);
      res = res_clone;
    }

    LCFGChange change = lcfgcomponent_append( comp_clone, res );
    if ( change == LCFG_CHANGE_ERROR ) ok = false;

    if ( res_clone != NULL )
      lcfgresource_relinquish(res_clone);
  }
  
 cleanup:

  if ( !ok ) {
    lcfgcomponent_relinquish(comp_clone);
    comp_clone = NULL;
  }

  return comp_clone;
}


/**
 * @brief Write list of formatted resources to file stream
 *
 * This uses @c lcfgresource_to_string() or @c
 * lcfgresource_to_export() to format each resource as a
 * string. See the documentation for those functions for full
 * details. The generated string is written to the specified file
 * stream which must have already been opened for writing.
 *
 * If the style is @c LCFG_RESOURCE_STYLE_EXPORT this will use the @c
 * lcfgcomponent_to_export() function.
 *
 * Resources which are invalid will be ignored. Resources which do not
 * have values will only be printed if the @c LCFG_OPT_ALL_VALUES
 * option is specified. Resources which are inactive (i.e. they have a
 * negative priority) will also be ignored unless the
 * @c LCFG_OPT_ALL_PRIORITIES option is specified.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] style Integer indicating required style of formatting
 * @param[in] options Integer that controls formatting
 * @param[in] out Stream to which the list of resources should be written
 *
 * @return Boolean indicating success
 *
 */

bool lcfgcomponent_print( const LCFGComponent * comp,
                          LCFGResourceStyle style,
                          LCFGOption options,
                          FILE * out ) {
  assert( comp != NULL );

  if ( lcfgcomponent_is_empty(comp) ) return true;

  /* Use a separate function for printing in the 'export' style */
  if ( style == LCFG_RESOURCE_STYLE_EXPORT )
    return lcfgcomponent_to_export( comp, NULL, NULL, options, out );

  bool all_priorities = (options&LCFG_OPT_ALL_PRIORITIES);
  bool all_values     = (options&LCFG_OPT_ALL_VALUES);

  options |= LCFG_OPT_NEWLINE;

  const char * comp_name = lcfgcomponent_get_name(comp);

  /* Preallocate string buffer for efficiency */

  size_t buf_size = 256;
  char * buffer = calloc( buf_size, sizeof(char) );
  if ( buffer == NULL ) {
    perror( "Failed to allocate memory for LCFG resource buffer" );
    exit(EXIT_FAILURE);
  }

  bool ok = true;

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
	cur_node != NULL && ok;
	cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res = lcfgcomponent_resource(cur_node);

    /* Not interested in resources for inactive contexts. Only print
       resources without values if the print_all option is specified */

    if ( ( all_values     || lcfgresource_has_value(res) ) &&
         ( all_priorities || lcfgresource_is_active(res) ) ) {

      ssize_t rc = lcfgresource_to_string( res, comp_name, style, options,
                                           &buffer, &buf_size );

      if ( rc < 0 )
        ok = false;

      if (ok) {
        if ( fputs( buffer, out ) < 0 )
          ok = false;
      }

    }
  }

  free(buffer);

  return ok;
}

/**
 * @brief Write list of resources for shell evaluation
 *
 * This uses @c lcfgresource_to_export() to format each resource as a
 * string which can be used for shell variable evaluation. See the
 * documentation for that function for full details. The generated
 * string is written to the specified file stream which must have
 * already been opened for writing. This will also generate an export
 * variable for the list of exported resource names. This function is
 * similar to @c lcfgcomponent_print() but handles the extra
 * complexity with formatting the list for shell evaluation.
 *
 * Resources which are invalid will be ignored. Resources which do not
 * have values will only be printed if the @c LCFG_OPT_ALL_VALUES
 * option is specified. Resources which are inactive (i.e. they have a
 * negative priority) will also be ignored unless the
 * @c LCFG_OPT_ALL_PRIORITIES option is specified.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] val_pfx The prefix for the value variable name
 * @param[in] type_pfx The prefix for the type variable name
 * @param[in] options Integer that controls formatting
 * @param[in] out Stream to which the list of resources should be written
 *
 * @return Boolean indicating success
 *
 */

bool lcfgcomponent_to_export( const LCFGComponent * comp,
                              const char * val_pfx, const char * type_pfx,
                              LCFGOption options,
                              FILE * out ) {
  assert( comp != NULL );

  if ( !lcfgcomponent_is_valid(comp) ) return LCFG_STATUS_ERROR;
  if ( lcfgcomponent_is_empty(comp) )  return LCFG_STATUS_OK;

  bool all_priorities = (options&LCFG_OPT_ALL_PRIORITIES);
  bool all_values     = (options&LCFG_OPT_ALL_VALUES);

  const char * comp_name = lcfgcomponent_get_name(comp);

  if ( val_pfx  == NULL ) val_pfx  = LCFG_RESOURCE_ENV_VAL_PFX;
  if ( type_pfx == NULL ) type_pfx = LCFG_RESOURCE_ENV_TYPE_PFX;

  /* Need these to handle the copies (if any) and free them later */
  char * val_pfx2  = NULL;
  char * type_pfx2 = NULL;

  if ( lcfgresource_build_env_prefix( val_pfx, comp_name, &val_pfx2 )
       && val_pfx2 != NULL ) {
    val_pfx = val_pfx2;
  }

  /* No point doing this if the type data isn't required */
  if ( options&LCFG_OPT_USE_META ) {
    if ( lcfgresource_build_env_prefix( type_pfx, comp_name, &type_pfx2 )
         && type_pfx2 != NULL ) {
      type_pfx = type_pfx2;
    }
  }

  /* Preallocate string buffer for efficiency */

  size_t buf_size = 256;
  char * buffer = calloc( buf_size, sizeof(char) );
  if ( buffer == NULL ) {
    perror( "Failed to allocate memory for LCFG resource buffer" );
    exit(EXIT_FAILURE);
  }

  LCFGTagList * export_res = lcfgtaglist_new();
  bool ok = true;

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
	cur_node != NULL && ok;
	cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res = lcfgcomponent_resource(cur_node);

    /* Not interested in resources for inactive contexts. Only print
       resources without values if the print_all option is specified */

    if ( ( all_values     || lcfgresource_has_value(res) ) &&
         ( all_priorities || lcfgresource_is_active(res) ) ) {

      ssize_t rc = lcfgresource_to_export( res, NULL,
                                           val_pfx, type_pfx,
                                           options,
                                           &buffer, &buf_size );

      /* Stash the resource name so we can create an env variable
         which holds the list of names. */

      if ( rc < 0 ) {
        ok = false;
      } else {
        const char * res_name = lcfgresource_get_name(res);

        char * add_msg = NULL;
        LCFGChange add_rc = lcfgtaglist_mutate_add( export_res, res_name,
                                                    &add_msg );
        if ( add_rc == LCFG_CHANGE_ERROR )
          ok = false;

        free(add_msg);
      }

      if (ok) {
        if ( fputs( buffer, out ) < 0 )
          ok = false;
      }
    }

  }

  /* Export style also needs a list of resource names for the component */

  if ( ok && !lcfgtaglist_is_empty(export_res) ) {

    lcfgtaglist_sort(export_res);

    ssize_t len = lcfgtaglist_to_string( export_res, LCFG_OPT_NONE,
                                         &buffer, &buf_size );
    if ( len < 0 ) {
      ok = false;
    } else {
      int rc = fprintf( out, "export %s%s='%s'\n", val_pfx, 
                        LCFG_RESOURCE_ENV_LISTKEY, buffer );
      if ( rc < 0 )
        ok = false;
    }

  }

  free(buffer);

  lcfgtaglist_relinquish(export_res);
  free(val_pfx2);
  free(type_pfx2);

  return ok;
}

/**
 * @brief Sort a list of resources for a component
 *
 * This sorts the nodes of the list of resources for an @c
 * LCFGComponent by using the @c lcfgresource_compare() function.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 */

void lcfgcomponent_sort( LCFGComponent * comp ) {
  assert( comp != NULL );

  if ( lcfgcomponent_size(comp) < 2 ) return;

  /* Oo. Oo. bubble sort .oO .oO */

  bool swapped=true;
  while (swapped) {
    swapped=false;

    LCFGResourceNode * cur_node = NULL;
    for ( cur_node = lcfgcomponent_head(comp);
          cur_node != NULL && cur_node->next != NULL;
          cur_node = lcfgcomponent_next(cur_node) ) {

      LCFGResource * cur_res  = lcfgcomponent_resource(cur_node);
      LCFGResource * next_res = lcfgcomponent_resource(cur_node->next);

      if ( lcfgresource_compare( cur_res, next_res ) > 0 ) {
        cur_node->resource       = next_res;
        cur_node->next->resource = cur_res;
        swapped = true;
      }

    }
  }

}

/**
 * @brief Read list of resources from status file
 *
 * This reads the contents of an LCFG status file and generates a new
 * @c LCFGComponent. A status file is used by an LCFG component to
 * store the current state of the resources.
 *
 * If the component name is not specified then the basename of the
 * file will be used. 
 *
 * An error is returned if the file does not exist unless the
 * @c LCFG_OPT_ALLOW_NOEXIST option is specified. If the file exists
 * but is empty then an empty @c LCFGComponent is returned.
 *
 * @param[in] filename Path to status file
 * @param[out] result Reference to pointer for new @c LCFGComponent
 * @param[in] compname_in Component name (optional)
 * @param[in] options Controls the behaviour of the process
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcomponent_from_status_file( const char * filename,
					   LCFGComponent ** result,
					   const char * compname_in,
					   LCFGOption options,
					   char ** msg ) {
  assert( filename != NULL );

  *result = NULL;

  LCFGComponent * comp = NULL;
  bool ok = true;

  /* Need a copy of the component name to insert into the
     LCFGComponent struct */

  char * compname = NULL;
  if ( compname_in != NULL ) {
    compname = strdup(compname_in);
  } else {
    if ( filename != NULL ) {
      compname = lcfgutils_basename( filename, NULL );
    } else {
      ok = false;
      lcfgutils_build_message( msg, "Either the component name or status file path MUST be specified" );
      goto cleanup;
    }
  }

  /* Create the new empty component which will eventually be returned */

  comp = lcfgcomponent_new();
  if ( !lcfgcomponent_set_name( comp, compname ) ) {
    ok = false;
    lcfgutils_build_message( msg, "Invalid name for component '%s'", compname );

    free(compname);

    goto cleanup;
  }

  const char * statusfile = filename != NULL ? filename : compname;

  FILE *fp;
  if ( (fp = fopen(statusfile, "r")) == NULL ) {

    if (errno == ENOENT) {

      if ( !(options&LCFG_OPT_ALLOW_NOEXIST) ) {
	ok = false;
	lcfgutils_build_message( msg,
				 "Component status file '%s' does not exist",
				 statusfile );
      }
    } else {
      ok = false;
      lcfgutils_build_message( msg,
			       "Component status file '%s' is not readable",
			       statusfile );
    }

    goto cleanup;
  }

  size_t line_len = 5120; /* Status files contain long derivation lines */
  char * statusline = calloc( line_len, sizeof(char) );
  if ( statusline == NULL ) {
    perror( "Failed to allocate memory for status parser buffer" );
    exit(EXIT_FAILURE);
  }

  int linenum = 1;
  while( getline( &statusline, &line_len, fp ) != -1 ) {

    lcfgutils_string_chomp(statusline);

    const char * this_hostname = NULL;
    const char * this_compname = NULL;
    const char * this_resname  = NULL;
    const char * status_value  = NULL;
    char this_type             = LCFG_RESOURCE_SYMBOL_VALUE;

    char * parse_msg = NULL;
    LCFGStatus parse_status = lcfgresource_parse_spec( statusline,
                                                       &this_hostname,
                                                       &this_compname,
                                                       &this_resname, 
                                                       &status_value,
                                                       &this_type,
                                                       &parse_msg );

    if ( parse_status == LCFG_STATUS_ERROR ) {
      lcfgutils_build_message( msg, "Failed to parse line %d (%s)",
                               linenum, parse_msg );
      ok = false;
      break;
    }
    free(parse_msg);

    /* Insist on the component names matching */

    if ( this_compname != NULL && 
         ( !lcfgcomponent_valid_name( this_compname) ||
           strcmp( this_compname, compname ) != 0 ) ) {
      lcfgutils_build_message( msg, "Failed to parse line %d (invalid component name '%s')",
                               linenum, this_compname );
      ok = false;
      break;
    }

    /* Grab the resource or create a new one if necessary */

    LCFGResource * res =
      lcfgcomponent_find_or_create_resource( comp, this_resname, true );

    if ( res == NULL ) {
      lcfgutils_build_message( msg,
			       "Failed to parse line %d of status file '%s'",
			       linenum, statusfile );
      ok = false;
      break;
    }

    /* Apply the action which matches with the symbol at the start of
       the status line or assume this is a simple specification of the
       resource value. */

    char * set_msg = NULL;
    ok = lcfgresource_set_attribute( res, this_type, status_value,
				     &set_msg );

    if ( !ok ) {

      if ( set_msg != NULL ) {
        lcfgutils_build_message( msg, "Failed to process line %d (%s)",
                  linenum, set_msg );

        free(set_msg);
      } else {
        lcfgutils_build_message( msg, 
                  "Failed to process line %d (bad value '%s' for type '%c')",
                  linenum, status_value, this_type );
      }

      break;
    }

    linenum++;
  }

  free(statusline);

  fclose(fp);

 cleanup:

  if ( !ok ) {
    lcfgcomponent_destroy(comp);
    comp = NULL;
  }

  *result = comp;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/**
 * @brief Export resources to the environment
 *
 * This exports value and type information for all the @c LCFGResource
 * for the @c LCFGComponent as environment variables. The variable
 * names are a combination of the resource name and any prefix
 * specified.
 *
 * This will also export a variable like @c LCFG_%s__RESOURCES which
 * holds a list of exported resource names.
 *
 * The value prefix will typically be like @c LCFG_%s_ and the type
 * prefix will typically be like @c LCFGTYPE_%s_ where @c '%s' is
 * replaced with the name of the component. If the prefixes are not
 * specified (i.e. @c NULL values are given) the default prefixes are
 * used. 
 *
 * Often only the value variable is required so, for efficiency, the
 * type variable will only be set when the @c LCFG_OPT_USE_META option
 * is specified.
 *
 * Resources without values will not be exported unless the @c
 * LCFG_OPT_ALL_VALUES option is specified. Inactive resources will
 * not be exported unless the @c LCFG_OPT_ALL_PRIORITIES option is
 * specified.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] val_pfx The prefix for the value variable name
 * @param[in] type_pfx The prefix for the type variable name
 * @param[in] options Integer which controls behaviour
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcomponent_to_env( const LCFGComponent * comp,
				 const char * val_pfx, const char * type_pfx,
				 LCFGOption options,
                                 char ** msg ) {
  assert( comp != NULL );

  if ( !lcfgcomponent_is_valid(comp) ) return LCFG_STATUS_ERROR;
  if ( lcfgcomponent_is_empty(comp) )  return LCFG_STATUS_OK;

  bool all_priorities = (options&LCFG_OPT_ALL_PRIORITIES);
  bool all_values     = (options&LCFG_OPT_ALL_VALUES);

  LCFGStatus status = LCFG_STATUS_OK;

  const char * comp_name = lcfgcomponent_get_name(comp);

  if ( val_pfx  == NULL ) val_pfx  = LCFG_RESOURCE_ENV_VAL_PFX;
  if ( type_pfx == NULL ) type_pfx = LCFG_RESOURCE_ENV_TYPE_PFX;

  /* Need these to handle the copies (if any) and free them later */
  char * val_pfx2  = NULL;
  char * type_pfx2 = NULL;

  if ( lcfgresource_build_env_prefix( val_pfx, comp_name, &val_pfx2 )
       && val_pfx2 != NULL ) {
    val_pfx = val_pfx2;
  }

  /* No point doing this if the type data isn't required */
  if ( options&LCFG_OPT_USE_META ) {
    if ( lcfgresource_build_env_prefix( type_pfx, comp_name, &type_pfx2 )
         && type_pfx2 != NULL ) {
      type_pfx = type_pfx2;
    }
  }

  LCFGTagList * export_res = lcfgtaglist_new();

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
	cur_node != NULL && status != LCFG_STATUS_ERROR;
	cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res = lcfgcomponent_resource(cur_node);

    if ( ( all_values     || lcfgresource_has_value(res) ) &&
         ( all_priorities || lcfgresource_is_active(res) ) ) {

      status = lcfgresource_to_env( res, NULL, val_pfx, type_pfx, options );

      if ( status == LCFG_STATUS_ERROR ) {
        *msg = lcfgresource_build_message( res, comp_name,
                           "Failed to set environment variable for resource" );
      } else {
        const char * res_name = lcfgresource_get_name(res);

        char * add_msg = NULL;
        LCFGChange add_rc = lcfgtaglist_mutate_add( export_res, res_name,
                                                    &add_msg );
        if ( add_rc == LCFG_CHANGE_ERROR )
          status = LCFG_STATUS_ERROR;

        free(add_msg);
      }
    }

  }

  if ( status != LCFG_STATUS_ERROR ) {

    /* Also create an environment variable which holds list of
       resource names for this component. */

    char * reslist_key =
      lcfgutils_string_join( "", val_pfx, LCFG_RESOURCE_ENV_LISTKEY );

    lcfgtaglist_sort(export_res);

    char * reslist_value = NULL;
    size_t bufsize = 0;
    ssize_t len = lcfgtaglist_to_string( export_res, LCFG_OPT_NONE,
                                         &reslist_value, &bufsize );
    if ( len < 0 ) {
      status = LCFG_STATUS_ERROR;
    } else {
      if ( setenv( reslist_key, reslist_value, 1 ) != 0 )
        status = LCFG_STATUS_ERROR;
    }

    free(reslist_key);
    free(reslist_value);

  }

  lcfgtaglist_relinquish(export_res);
  free(val_pfx2);
  free(type_pfx2);

  return status;
}

/**
 * @brief Write list of resources to status file
 *
 * This can be used to create an LCFG status file which stores the
 * state for the resources of the component. The resources are
 * serialised using the @c lcfgresource_to_status() function.
 *
 * If the filename is not specified a file will be created with the
 * component name.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] filename Path of status file to be created (optional)
 * @param[in] options Controls the behaviour of the process
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcomponent_to_status_file( const LCFGComponent * comp,
					 const char * filename,
					 LCFGOption options,
					 char ** msg ) {
  assert( comp != NULL );

  bool all_priorities = (options&LCFG_OPT_ALL_PRIORITIES);

  const char * compname = lcfgcomponent_get_name(comp);

  if ( filename == NULL && compname == NULL ) {
    lcfgutils_build_message( msg, "Either the target file name or component name is required" );
    return LCFG_STATUS_ERROR;
  }

  const char * statusfile = filename != NULL ? filename : compname;

  char * tmpfile = lcfgutils_safe_tmpfile(statusfile);

  bool ok = true;

  FILE * out = NULL;
  int fd = mkstemp(tmpfile);
  if ( fd >= 0 )
    out = fdopen( fd, "w" );

  if ( out == NULL ) {
    if ( fd >= 0 ) close(fd);

    lcfgutils_build_message( msg, "Failed to open temporary status file '%s'",
              tmpfile );
    ok = false;
    goto cleanup;
  }

  /* For efficiency a buffer is pre-allocated. The initial size was
     chosen by looking at typical resource usage for Informatics. The
     buffer will be automatically grown when necessary, the aim is to
     minimise the number of reallocations required.  */

  size_t buf_size = 5120;
  char * buffer = calloc( buf_size, sizeof(char) );
  if ( buffer == NULL ) {
    perror( "Failed to allocate memory for LCFG resource buffer" );
    exit(EXIT_FAILURE);
  }

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
	cur_node != NULL;
	cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res = lcfgcomponent_resource(cur_node);

    /* Not interested in resources for inactive contexts */

    if ( !lcfgresource_is_active(res) && !all_priorities ) continue;

    ssize_t rc = lcfgresource_to_status( res, compname, LCFG_OPT_NONE,
					 &buffer, &buf_size );

    if ( rc > 0 ) {

      if ( fputs( buffer, out ) < 0 )
	ok = false;

    } else {
      ok = false;
    }

    if (!ok) {
      lcfgutils_build_message( msg, "Failed to write to status file" );
      break;
    }

  }

  free(buffer);

  if ( fclose(out) != 0 ) {
    lcfgutils_build_message( msg, "Failed to close status file" );
    ok = false;
  }

  if (ok) {
    if ( rename( tmpfile, statusfile ) != 0 ) {
      lcfgutils_build_message( msg, "Failed to rename temporary status file to '%s'",
                statusfile );
      ok = false;
    }
  }

 cleanup:

  if ( tmpfile != NULL ) {
    unlink(tmpfile);
    free(tmpfile);
  }

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/**
 * @brief Find the resource node with a given name
 *
 * This can be used to search through an @c LCFGComponent to find
 * the first resource node which has a matching name. Note that the
 * matching is done using strcmp(3) which is case-sensitive.
 *
 * A @c NULL value is returned if no matching node is found. Also, a
 * @c NULL value is returned if a @c NULL value or an empty component
 * is specified.
 *
 * @param[in] comp Pointer to @c LCFGComponent to be searched
 * @param[in] name The name of the required resource node
 * @param[in] all_priorities Search through all resources (not just active)
 *
 * @return Pointer to an @c LCFGResourceNode (or the @c NULL value).
 *
 */

LCFGResourceNode * lcfgcomponent_find_node( const LCFGComponent * comp,
                                            const char * name,
                                            bool all_priorities ) {
  assert( name != NULL );

  if ( lcfgcomponent_is_empty(comp) ) return NULL;

  LCFGResourceNode * result = NULL;

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
	cur_node != NULL && result == NULL;
	cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res = lcfgcomponent_resource(cur_node); 

    if ( !lcfgresource_has_name(res) || 
         ( !all_priorities && !lcfgresource_is_active(res) ) ) continue;

    const char * res_name = lcfgresource_get_name(res);

    if ( strcmp( res_name, name ) == 0 )
      result = (LCFGResourceNode *) cur_node;

  }

  return result;
}

/**
 * @brief Find the resource for a given name
 *
 * This can be used to search through an @c LCFGComponent to find
 * the first resource which has a matching name. Note
 * that the matching is done using strcmp(3) which is case-sensitive.
 *
 * To ensure the returned @c LCFGResource is not destroyed when
 * the parent @c LCFGComponent is destroyed you would need to
 * call the @c lcfgresource_acquire() function.
 *
 * @param[in] comp Pointer to @c LCFGComponent to be searched
 * @param[in] name The name of the required resource
 * @param[in] all_priorities Search through all resources (not just active)
 *
 * @return Pointer to an @c LCFGResource (or the @c NULL value).
 *
 */

LCFGResource * lcfgcomponent_find_resource( const LCFGComponent * comp,
                                            const char * name,
                                            bool all_priorities ) {
  assert( name != NULL );

  LCFGResource * res = NULL;

  const LCFGResourceNode * res_node = 
    lcfgcomponent_find_node( comp, name, all_priorities );
  if ( res_node != NULL )
    res = lcfgcomponent_resource(res_node);

  return res;
}

/**
 * @brief Check if a component contains a particular resource
 *
 * This can be used to search through an @c LCFGComponent to check if
 * it contains a resource with a matching name. Note that the matching
 * is done using strcmp(3) which is case-sensitive.
 * 
 * This uses the @c lcfgcomponent_find_node() function to find the
 * relevant node. If a @c NULL value is specified for the list or the
 * list is empty then a false value will be returned.
 *
 * @param[in] comp Pointer to @c LCFGComponent to be searched (may be @c NULL)
 * @param[in] name The name of the required resource
 * @param[in] all_priorities Search through all resources (not just active)
 *
 * @return Boolean value which indicates presence of resource in component
 *
 */

bool lcfgcomponent_has_resource( const LCFGComponent * comp,
                                 const char * name,
                                 bool all_priorities ) {
  assert( name != NULL );

  return ( lcfgcomponent_find_node( comp, name, all_priorities ) != NULL );
}

/**
 * @brief Find or create a new resource
 *
 * Searches the @c LCFGComponent for an @c LCFGResource with the
 * required name. If none is found then a new @c LCFGResource is
 * created and added to the @c LCFGComponent.
 *
 * If an error occurs during the creation of a new resource a @c NULL
 * value will be returned.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] name The name of the required resource
 * @param[in] all_priorities Search through all resources (not just active)
 *
 * @return The required @c LCFGResource (or @c NULL)
 *
 */

LCFGResource * lcfgcomponent_find_or_create_resource( LCFGComponent * comp,
                                                      const char * name,
                                                      bool all_priorities ) {
  assert( comp != NULL );
  assert( name != NULL );

  /* Only searches 'active' resources */

  LCFGResource * result =
    lcfgcomponent_find_resource( comp, name, all_priorities );

  if ( result != NULL ) return result;

  /* If not found then create new resource and add it to the component */
  result = lcfgresource_new();

  /* Setting name can fail if it is invalid */

  char * new_name = strdup(name);
  bool ok = lcfgresource_set_name( result, new_name );

  if ( !ok ) {
    free(new_name);
  } else {

    if ( lcfgcomponent_append( comp, result ) == LCFG_CHANGE_ERROR )
      ok = false;

  }

  lcfgresource_relinquish(result);

  if (!ok)
    result = NULL;

  return result;
}

/**
 * @brief Merge resource into component
 *
 * Merges an @c LCFGResource into an existing @c LCFGComponent
 * according to the particular merge rules specified for the component.
 *
 * The action of merging a resource into a component differs from
 * simply appending in that a search is done to check if a resource
 * with the same name is already present in the component. By default,
 * with no rules specified, merging a resource when it is already
 * present is not permitted. This behaviour can be modified in various
 * ways, the following rules are supported (in this order):
 *
 *   - LCFG_MERGE_RULE_NONE - null rule (the default)
 *   - LCFG_MERGE_RULE_USE_PREFIX - mutate value according to prefix (TODO)
 *   - LCFG_MERGE_RULE_SQUASH_IDENTICAL - ignore additional identical resources
 *   - LCFG_MERGE_RULE_KEEP_ALL - keep all resources
 *   - LCFG_MERGE_RULE_USE_PRIORITY - resolve conflicts using context priority
 * 
 * Rules can be used in any combination by using a @c '|' (bitwise
 * 'or'), for example @c LCFG_MERGE_RULE_SQUASH_IDENTICAL can be
 * combined with @c LCFG_MERGE_RULE_KEEP_ALL to keep all resources which
 * are not identical. The combination of rules can result in some very
 * complex scenarios so care should be take to choose the best set of
 * rules.
 *
 * A rule controls whether a change is accepted or rejected. If it is
 * accepted the change can result in the removal, addition or
 * replacement of a resource. If a rule neither explicitly accepts or
 * rejects a resource then the next rule in the list is applied. If no
 * rule leads to the acceptance of a change then it is rejected.
 *
 * <b>Squash identical</b>: If the resources are the same, according
 * to the @c lcfgresource_equals() function (which compares name,
 * value and context), then the current entry is replaced with the new
 * one (which effectively updates the derivation information).
 *
 * <b>Keep all</b>: Keep all resources (i.e. ignore any conflicts).
 *
 * <b>Use priority</b>: Compare the values of the priority which is
 * the result of evaluating the context expression (if any) for the
 * resource. If the new resource has a greater priority then it
 * replaces the current one. If the current has a greater priority
 * then the new resource is ignored. If the priorities are the same
 * the conflict remains unresolved.
 *
 * The process can successfully result in any of the following being returned:
 *
 *   - @c LCFG_CHANGE_NONE - the component is unchanged
 *   - @c LCFG_CHANGE_ADDED - the new resource was added
 *   - @c LCFG_CHANGE_REMOVED - the current resource was removed
 *   - @c LCFG_CHANGE_REPLACED - the current resource was replaced with the new one
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] new_res Pointer to @c LCFGResource
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcomponent_merge_resource( LCFGComponent * comp,
                                         LCFGResource * new_res,
                                         char ** msg ) {
  assert( comp != NULL );
  assert( new_res != NULL );

  LCFGMergeRule merge_rules = lcfgcomponent_get_merge_rules(comp);

  /* Define these ahead of any jumps to the "apply" label */

  LCFGResourceNode * prev_node = NULL;
  LCFGResourceNode * cur_node  = NULL;
  LCFGResource * cur_res  = NULL;

  /* Actions */

  bool remove_old = false;
  bool append_new = false;
  bool accept     = false;

  if ( !lcfgresource_is_valid(new_res) ) {
    lcfgutils_build_message( msg, "Resource is invalid" );
    goto apply;
  }

  /* Doing a search here rather than calling find_node so that the
     previous node can also be selected. That is needed for removals. */

  const char * match_name = new_res->name;

  const LCFGResourceNode * node = NULL;
  for ( node = lcfgcomponent_head(comp);
        node != NULL && cur_node == NULL;
        node = lcfgcomponent_next(node) ) {

    const LCFGResource * res = lcfgcomponent_resource(node);

    if ( !lcfgresource_is_valid(res) ) continue;

    if ( lcfgresource_match( res, match_name ) ) {
      cur_node  = (LCFGResourceNode *) node;
      cur_res   = lcfgcomponent_resource(cur_node);
    } else {
      prev_node = (LCFGResourceNode *) node; /* used later if removal is required */
    }

  }

  /* 0. Merging a pointer to a struct which is already in the list is
        a no-op. Note that this does not prevent the same resource
        appearing multiple times in the list if they are in different
        structs. */

  if ( cur_res == new_res ) {
    accept = true;
    goto apply;
  }

  /* 1. TODO: mutations */


  /* 2. If the resource is not currently in the component then just append */

  if ( cur_res == NULL ) {
    append_new = true;
    accept     = true;
    goto apply;
  }

  /* 3. If the resource in the component is identical then replace
        (updates the derivation) */

  if ( merge_rules&LCFG_MERGE_RULE_SQUASH_IDENTICAL ) {

    if ( lcfgresource_equals( cur_res, new_res ) ) {
      remove_old = true;
      append_new = true;
      accept     = true;
      goto apply;
    }
  }

  /* 4. Might want to just keep everything */

  if ( merge_rules&LCFG_MERGE_RULE_KEEP_ALL ) {
    append_new = true;
    accept     = true;
    goto apply;
  }

  /* 5. Just replace existing with new */

  if ( merge_rules&LCFG_MERGE_RULE_REPLACE ) {
      remove_old = true;
      append_new = true;
      accept     = true;
      goto apply;
  }

  /* 6. Use the priorities from the context evaluations */

  if ( merge_rules&LCFG_MERGE_RULE_USE_PRIORITY ) {

    int priority  = lcfgresource_get_priority(new_res);
    int opriority = lcfgresource_get_priority(cur_res);

    /* same priority for both is a conflict */

    if ( priority > opriority ) {
      remove_old = true;
      append_new = true;
      accept     = true;
    } else if ( priority < opriority ) {
      accept     = true; /* no change, old res is higher priority */
    }

    goto apply;
  }

 apply:
  ;

  /* Note that is permissible for a new resource to be "accepted"
     without any changes occurring to the component */

  LCFGChange result = LCFG_CHANGE_NONE;

  if ( accept ) {

    if ( remove_old && cur_node != NULL ) {

      LCFGResource * old_res = NULL;
      LCFGChange remove_rc =
        lcfgcomponent_remove_next( comp, prev_node, &old_res );

      if ( remove_rc == LCFG_CHANGE_REMOVED ) {
        lcfgresource_relinquish(old_res);
        result = LCFG_CHANGE_REMOVED;
      } else {
        lcfgutils_build_message( msg, "Failed to remove old resource" );
        result = LCFG_CHANGE_ERROR;
      }

    }

    if ( append_new && result != LCFG_CHANGE_ERROR ) {
      LCFGChange append_rc = lcfgcomponent_append( comp, new_res );

      if ( append_rc == LCFG_CHANGE_ADDED ) {

        if ( result == LCFG_CHANGE_REMOVED ) {
          result = LCFG_CHANGE_REPLACED;
        } else {
          result = LCFG_CHANGE_ADDED;
        }

      } else {
        lcfgutils_build_message( msg, "Failed to append new resource" );
        result = LCFG_CHANGE_ERROR;
      }

    }

  } else {
    result = LCFG_CHANGE_ERROR;

    if ( *msg == NULL )
      *msg = lcfgresource_build_message( cur_res, lcfgcomponent_get_name(comp),
                                         "conflict" );

  }

  return result;
}


/**
 * @brief Merge overrides from one component to another
 *
 * Iterates through the list of resources in the overrides @c
 * LCFGComponent and merges them to the target component by calling
 * @c lcfgcomponent_merge_resource().
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] overrides Pointer to override @c LCFGComponent
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcomponent_merge_component( LCFGComponent * comp,
					  const LCFGComponent * overrides,
					  char ** msg ) {
  assert( comp != NULL );

  if ( lcfgcomponent_is_empty(overrides) ) return LCFG_CHANGE_NONE;

  /* Using a (shallow) clone so we don't affect the original list if
     an error occurs */

  LCFGComponent * comp_clone = lcfgcomponent_clone( comp, false );
  if ( comp_clone == NULL ) return LCFG_CHANGE_ERROR;

  LCFGChange change = LCFG_CHANGE_NONE;

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(overrides);
	cur_node != NULL && change != LCFG_CHANGE_ERROR;
	cur_node = lcfgcomponent_next(cur_node) ) {

    LCFGResource * override_res = lcfgcomponent_resource(cur_node);

    LCFGChange rc =
      lcfgcomponent_merge_resource( comp_clone, override_res, msg );

    if ( rc == LCFG_CHANGE_ERROR ) {
      change = LCFG_CHANGE_ERROR;
    } else if ( rc != LCFG_CHANGE_NONE ) {
      change = LCFG_CHANGE_MODIFIED;
    }

  }

  if ( change == LCFG_CHANGE_MODIFIED ) {
    /* push changes back to original list */

    /* For efficiency - empty the original then steal the brains of
       the clone. */

    lcfgcomponent_remove_all_resources(comp);

    comp->head = comp_clone->head;
    comp->tail = comp_clone->tail;
    comp->size = comp_clone->size;

    comp_clone->head = NULL;
    comp_clone->tail = NULL;
    comp_clone->size = 0;

  }

  lcfgcomponent_relinquish(comp_clone);

  return change;
}

/**
 * @brief Get the list of resource names as a taglist
 *
 * This generates a new @c LCFGTagList which contains a list of
 * resource names for the @c LCFGComponent. If the component is empty
 * then an empty tag list will be returned. Only those resources which
 * are considered to be @e active ( a priority value of zero or
 * greater) will be included unless the @c LCFG_OPT_ALL_PRIORITIES
 * option is specified.  Will return @c NULL if an error occurs.
 *
 * To avoid memory leaks, when the list is no longer required the 
 * @c lcfgtaglist_relinquish() function should be called.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] options Integer which controls behaviour.
 *
 * @return Pointer to a new @c LCFGTagList of resource names
 *
 */

LCFGTagList * lcfgcomponent_get_resources_as_taglist(const LCFGComponent * comp,
						     LCFGOption options ) {
  assert( comp != NULL );

  bool all_priorities = (options&LCFG_OPT_ALL_PRIORITIES);

  LCFGTagList * reslist = lcfgtaglist_new();

  bool ok = true;

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
        cur_node != NULL && ok;
        cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res = lcfgcomponent_resource(cur_node);

    /* Ignore any without names. Ignore inactive unless all_priorities */
    if ( !lcfgresource_is_valid(res) ||
	 ( !all_priorities && !lcfgresource_is_active(res) ) )
      continue;

    const char * res_name = lcfgresource_get_name(res);

    char * msg = NULL;
    LCFGChange change = lcfgtaglist_mutate_add( reslist, res_name, &msg );
    if ( change == LCFG_CHANGE_ERROR )
      ok = false;

    free(msg); /* Just ignoring any message */
  }

  if (!ok) {
    lcfgtaglist_relinquish(reslist);
    reslist = NULL;
  }

  return reslist;
}

/**
 * @brief Get the list of resource names as a string
 *
 * This generates a new string which contains a space-separated sorted
 * list of resource names for the @c LCFGComponent. If the component
 * is empty then an empty string will be returned. Only those
 * resources which are considered to be @e active ( a priority value
 * of zero or greater) will be included unless the
 * @c LCFG_OPT_ALL_PRIORITIES option is specified.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] options Integer which controls behaviour.
 *
 * @return Pointer to a new string (call @c free(3) when no longer required)
 *
 */

char * lcfgcomponent_get_resources_as_string( const LCFGComponent * comp,
					      LCFGOption options ) {
  assert( comp != NULL );

  if ( lcfgcomponent_is_empty(comp) ) return strdup("");

  LCFGTagList * reslist =
    lcfgcomponent_get_resources_as_taglist( comp, options );

  if ( reslist == NULL ) return NULL;

  lcfgtaglist_sort(reslist);

  size_t buf_len = 0;
  char * res_as_str = NULL;

  if ( lcfgtaglist_to_string( reslist, 0, &res_as_str, &buf_len ) < 0 ) {
    free(res_as_str);
    res_as_str = NULL;
  }

  lcfgtaglist_relinquish(reslist);

  return res_as_str;
}

/**
 * @brief Import a component from the environment
 *
 * This can be used to import the values and type information for the
 * resources in a component from the current environment variables.
 *
 * The value prefix will typically be like @c LCFG_%s_ and the type
 * prefix will typically be like @c LCFGTYPE_%s_ where @c '%s' is
 * replaced with the name of the component. If the prefixes are not
 * specified (i.e. @c NULL values are given) the default prefixes are
 * used. 
 *
 * This gets the list of resource names from the value of an
 * environment variable like @c LCFG_%s__RESOURCES (e.g. it uses the
 * value prefix), if that variable is not found nothing will be loaded
 * and @c LCFG_STATUS_ERROR will be returned unless the 
 * @c LCFG_OPT_ALLOW_NOEXIST option is specified.
 *
 * The value and type information for each resource in the list is
 * imported using the @c lcfgresource_from_env() function.
 *
 * To avoid memory leaks, when the newly created component structure
 * is no longer required you should call the @c lcfgcomponent_relinquish() 
 * function.
 *
 * @param[in] compname_in The name of the component
 * @param[in] val_pfx The prefix for the value variable name
 * @param[in] type_pfx The prefix for the type variable name
 * @param[out] result Reference to the pointer for the new @c LCFGComponent
 * @param[in] options Integer which controls behaviour
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcomponent_from_env( const char * compname_in,
                                   const char * val_pfx, const char * type_pfx,
                                   LCFGComponent ** result,
                                   LCFGOption options,
                                   char ** msg ) {
  assert( compname_in != NULL );

  if ( !lcfgcomponent_valid_name(compname_in) ) {
    lcfgutils_build_message( msg, "Invalid component name '%s'", compname_in );
    return LCFG_STATUS_ERROR;
  }

  if ( val_pfx  == NULL ) val_pfx  = LCFG_RESOURCE_ENV_VAL_PFX;
  if ( type_pfx == NULL ) type_pfx = LCFG_RESOURCE_ENV_TYPE_PFX;

  /* Need these to handle the copies (if any) and free them later */
  char * val_pfx2  = NULL;
  char * type_pfx2 = NULL;

  if ( lcfgresource_build_env_prefix( val_pfx, compname_in, &val_pfx2 )
       && val_pfx2 != NULL ) {
    val_pfx = val_pfx2;
  }

  if ( lcfgresource_build_env_prefix( type_pfx, compname_in, &type_pfx2 )
       && type_pfx2 != NULL ) {
    type_pfx = type_pfx2;
  }

  /* Declare variables here which need to be defined before a jump to
     the cleanup stage */

  LCFGStatus status = LCFG_STATUS_OK;
  LCFGComponent * comp = NULL;
  LCFGTagList * import_res = NULL;

  /* Find the list of resource names for the component */

  char * reslist_key =
    lcfgutils_string_join( "", val_pfx, LCFG_RESOURCE_ENV_LISTKEY );

  const char * reslist_value = getenv(reslist_key);

  free(reslist_key);

  if ( !isempty(reslist_value) ) {

    status = lcfgtaglist_from_string( reslist_value, &import_res, msg );
    if ( status ==  LCFG_STATUS_ERROR )
      goto cleanup;

  } else if ( !(options&LCFG_OPT_ALLOW_NOEXIST) ) {
    lcfgutils_build_message( msg, 
                         "No resources found in environment for '%s' component",
                             compname_in );
    status = LCFG_STATUS_ERROR;
    goto cleanup;
  }

  /* Create an empty component with the required name */

  comp = lcfgcomponent_new();
  char * compname = strdup(compname_in);
  if ( !lcfgcomponent_set_name( comp, compname ) ) {
    lcfgutils_build_message( msg, "Invalid component name '%s'", compname );
    status = LCFG_STATUS_ERROR;
    goto cleanup;
  }

  /* Nothing more to do if the list of resources to be imported is empty */
  if ( lcfgtaglist_is_empty(import_res) ) goto cleanup;

  LCFGTagIterator * tagiter = lcfgtagiter_new(import_res);
  const LCFGTag * restag = NULL;

  while ( status != LCFG_STATUS_ERROR &&
	  ( restag = lcfgtagiter_next(tagiter) ) != NULL ) {

    const char * resname = lcfgtag_get_name(restag);

    if ( !lcfgresource_valid_name(resname) ) {
      lcfgutils_build_message( msg, "Invalid resource name '%s'", resname );
      status = LCFG_STATUS_ERROR;
      break;
    }

    LCFGResource * res = NULL;
    status = lcfgresource_from_env( resname, NULL, val_pfx, type_pfx, 
                                    &res, LCFG_OPT_NONE, msg );

    if ( status != LCFG_STATUS_ERROR ) {

      LCFGChange change = lcfgcomponent_append( comp, res );
      if ( change == LCFG_CHANGE_ERROR ) {
        lcfgutils_build_message( msg, "Failed to import resource '%s'",
                                 resname );
        status = LCFG_STATUS_ERROR;
      }

    }

    lcfgresource_relinquish(res);
  }

  lcfgtagiter_destroy(tagiter);

 cleanup:

  free(val_pfx2);
  free(type_pfx2);

  lcfgtaglist_relinquish(import_res);

  if ( status ==  LCFG_STATUS_ERROR ) {
    lcfgcomponent_relinquish(comp);
    comp = NULL;
  }

  *result = comp;

  return status;
}

/**
 * @brief Calculate the hash for a component
 *
 * This will calculate the hash for the component using the value for
 * the @e name parameter. It does this using the @c
 * lcfgutils_string_djbhash() function.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 * @return The hash for the component name
 *
 */

unsigned long lcfgcomponent_hash( const LCFGComponent * comp ) {
  assert( comp != NULL );
  return lcfgutils_string_djbhash( comp->name, NULL );
}

/**
 * @brief Compare the component names
 *
 * This compares the names for two @c LCFGComponent, this is mostly
 * useful for sorting lists of components. An integer value is
 * returned which indicates lesser than, equal to or greater than in
 * the same way as @c strcmp(3).
 *
 * @param[in] comp1 Pointer to @c LCFGComponent
 * @param[in] comp2 Pointer to @c LCFGComponent
 * 
 * @return Integer (-1,0,+1) indicating lesser,equal,greater
 *
 */

int lcfgcomponent_compare( const LCFGComponent * comp1,
                           const LCFGComponent * comp2 ) {
  assert( comp1 != NULL );
  assert( comp2 != NULL );

  const char * comp1_name = or_default( comp1->name, "" );
  const char * comp2_name = or_default( comp2->name, "" );

  return strcmp( comp1_name, comp2_name );
}

/**
 * @brief Test if component name matches string
 *
 * This compares the name of the @c LCFGComponent with the specified string.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] name The name to check for a match
 *
 * @return boolean indicating equality of values
 *
 */

bool lcfgcomponent_match( const LCFGComponent * comp,
                          const char * name ) {
  assert( comp != NULL );
  assert( name != NULL );

  const char * comp_name = or_default( comp->name, "" );

  return ( strcmp( comp_name, name ) == 0 );
}

bool lcfgcomponent_same_name( const LCFGComponent * comp1,
                              const LCFGComponent * comp2 ) {
  assert( comp1 != NULL );
  assert( comp2 != NULL );

  const char * name1 = or_default( comp1->name, "" );
  const char * name2 = or_default( comp2->name, "" );

  return ( strcmp( name1, name2 ) == 0 );
}

LCFGStatus lcfgcomponent_subset( const LCFGComponent * comp,
                                 const LCFGTagList * res_wanted,
                                 LCFGComponent ** result,
                                 LCFGOption options,
                                 char ** msg ) {
  assert( comp != NULL );

  LCFGComponent * new_comp = lcfgcomponent_new();

  LCFGStatus status = LCFG_STATUS_OK;

  /* Clone the name if there is one */

  if ( !isempty(comp->name) ) {
    char * new_name = strdup(comp->name);
    if ( !lcfgcomponent_set_name( new_comp, new_name ) ) {
      status = LCFG_STATUS_ERROR;
      lcfgutils_build_message( msg, "Invalid component name '%s'", comp->name );

      free(new_name);
    }
  }

  /* Also clone the merge rules */

  new_comp->merge_rules = comp->merge_rules;

  /* Collect the required subset of resources */

  const bool all_priorities = (options&LCFG_OPT_ALL_PRIORITIES);
  const bool take_all = lcfgtaglist_is_empty(res_wanted);

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
        cur_node != NULL && status != LCFG_STATUS_ERROR;
        cur_node = lcfgcomponent_next(cur_node) ) {

    LCFGResource * res = lcfgcomponent_resource(cur_node);

    if ( lcfgresource_is_valid(res) && 
         ( all_priorities || lcfgresource_is_active(res) ) ) {

      if ( take_all || 
           lcfgtaglist_contains( res_wanted, lcfgresource_get_name(res) ) ) {

        LCFGChange change = lcfgcomponent_append( new_comp, res );
        if ( change == LCFG_CHANGE_ERROR ) {
          status = LCFG_STATUS_ERROR;
          lcfgutils_build_message( msg, "Failed to add resource to component" );
        }

      }

    }

  }

 cleanup:

  if ( status == LCFG_STATUS_ERROR ) {
    lcfgcomponent_relinquish(new_comp);
    new_comp = NULL;
  }

  *result = new_comp;

  return status;
}

/**
 * @brief Test if the component is based upon ngeneric
 *
 * Most LCFG components are use the ngeneric framework and thus
 * consume the associated schema. This function can be used to check
 * if the specified @c LCFGComponent has ngeneric resources. This is
 * done by searching for a @c ng_schema resource and checking that it
 * has a value.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 * @return Boolean which indicates if component uses ngeneric
 *
 */

bool lcfgcomponent_is_ngeneric( const LCFGComponent * comp ) {

  const LCFGResource * ng_schema = 
    lcfgcomponent_find_resource( comp, "ng_schema", false );

  return ( ng_schema != NULL && lcfgresource_has_value(ng_schema) );
}

/* eof */
