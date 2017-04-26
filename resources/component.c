/**
 * @file component.c
 * @brief Functions for working with LCFG components
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
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

/* Used when creating environment variables from resources */

static const char * default_val_pfx  = "LCFG_%s_";
static const char * default_type_pfx = "LCFGTYPE_%s_";
static const char * env_placeholder  = "%s";
static const char * reslist_keyname  = "_RESOURCES";

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
  comp->size = 0;
  comp->head = NULL;
  comp->tail = NULL;
  comp->_refcount = 1;

  return comp;
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

  while ( lcfgcomponent_size(comp) > 0 ) {
    LCFGResource * resource = NULL;
    if ( lcfgcomponent_remove_next( comp, NULL, &resource )
         == LCFG_CHANGE_REMOVED ) {
      lcfgresource_relinquish(resource);
    }
  }

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
 * @brief Check if the component has a name
 *
 * Checks if the specified @c LCFGComponent currently has a value set
 * for the name attribute. Although a name is required for an LCFG
 * component to be valid it is possible for the value of the name to be
 * set to @c NULL when the structure is first created.
 *
 * @param[in] res Pointer to an @c LCFGComponent
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
 * This returns the value of the @E name parameter for the
 * @c LCFGComponent. If the component does not currently have a @e
 * name then the pointer returned will be @c NULL.
 *
 * It is important to note that this is @b NOT a copy of the string,
 * changing the returned string will modify the @e name of the
 * component.
 *
 * @param[in] res Pointer to an @c LCFGComponent
 *
 * @return The @e name for the component (possibly @c NULL).
 */

char * lcfgcomponent_get_name(const LCFGComponent * comp) {
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
 * @param[in] res Pointer to an @c LCFGComponent
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
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] resnode Pointer to @c LCFGResourceNode
 * @param[in] res Pointer to @c LCFGResource
 * 
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcomponent_insert_next( LCFGComponent    * comp,
                                      LCFGResourceNode * resnode,
                                      LCFGResource     * res ) {
  assert( comp != NULL );

  LCFGResourceNode * new_node = lcfgresourcenode_new(res);
  if ( new_node == NULL ) return LCFG_CHANGE_ERROR;

  lcfgresource_acquire(res);

  if ( resnode == NULL ) { /* HEAD */

    if ( lcfgcomponent_is_empty(comp) )
      comp->tail = new_node;

    new_node->next = comp->head;
    comp->head     = new_node;

  } else {
    
    if ( resnode->next == NULL )
      comp->tail = new_node;

    new_node->next = resnode->next;
    resnode->next  = new_node;

  }

  comp->size++;

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
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] resnode Pointer to @c LCFGResourceNode
 * @param[out] res Pointer to @c LCFGResource
 * 
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcomponent_remove_next( LCFGComponent    * comp,
                                      LCFGResourceNode * resnode,
                                      LCFGResource    ** res ) {
  assert( comp != NULL );

  if ( lcfgcomponent_is_empty(comp) ) return LCFG_CHANGE_NONE;

  LCFGResourceNode * old_node = NULL;

  if ( resnode == NULL ) { /* HEAD */

    old_node   = comp->head;
    comp->head = comp->head->next;

    if ( lcfgcomponent_size(comp) == 1 )
      comp->tail = NULL;

  } else {

    if ( resnode->next == NULL ) return LCFG_CHANGE_ERROR;

    old_node      = resnode->next;
    resnode->next = resnode->next->next;

    if ( resnode->next == NULL )
      comp->tail = resnode;

  }

  comp->size--;

  *res = lcfgcomponent_resource(old_node);

  lcfgresourcenode_destroy(old_node);

  return LCFG_CHANGE_REMOVED;
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
 * If the style is @c LCFG_RESOURCE_STYLE_EXPORT this will also
 * generate an export variable for the list of exported resource
 * names.
 *
 * Resources which are invalid will be ignored. Resources which do not
 * have values will only be printed if the @c LCFG_OPT_ALL_VALUES
 * option is specified. Resources which are inactive (i.e. they have a
 * negative priority) will also be ignored unless the
 * @c LCFG_OPT_ALL_PRIORITIES option is specified.
 *
 * @param[in] pkglist Pointer to @c LCFGComponent
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

  bool all_priorities = (options&LCFG_OPT_ALL_PRIORITIES);
  bool all_values     = (options&LCFG_OPT_ALL_VALUES);

  options |= LCFG_OPT_NEWLINE;

  const char * comp_name = lcfgcomponent_get_name(comp);

  bool print_export = ( style == LCFG_RESOURCE_STYLE_EXPORT );

  char * val_pfx  = NULL;
  char * type_pfx = NULL;

  LCFGTagList * export_res = NULL;
  if ( print_export ) {
    val_pfx  = lcfgutils_string_replace( default_val_pfx,
                                         env_placeholder, comp_name );
    type_pfx = lcfgutils_string_replace( default_type_pfx,
                                         env_placeholder, comp_name );

    export_res = lcfgtaglist_new();
  }

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

      ssize_t rc;
      if ( print_export ) {
        rc = lcfgresource_to_export( res, val_pfx, type_pfx, options,
                                     &buffer, &buf_size );

        /* Stash the resource name so we can create an env variable
           which holds the list of names. */

        if ( rc > 0 ) {
          const char * name = lcfgresource_get_name(res);
          char * add_msg = NULL;
          LCFGChange change = 
            lcfgtaglist_mutate_add( export_res, name, &add_msg );
          if ( change == LCFG_CHANGE_ERROR )
            ok = false;

          free(add_msg);
        }

      } else {
        rc = lcfgresource_to_string( res, comp_name, style, options,
                                     &buffer, &buf_size );
      }

      if ( rc < 0 )
        ok = false;

      if (ok) {
        if ( fputs( buffer, out ) < 0 )
          ok = false;
      }

    }
  }

  /* Export style also needs a list of resource names for the component */

  if ( ok && print_export ) {

    lcfgtaglist_sort(export_res);

    char * reslist = NULL;
    size_t bufsize = 0;
    ssize_t len = lcfgtaglist_to_string( export_res, LCFG_OPT_NONE,
                                         &reslist, &bufsize );
    if ( len < 0 ) {
      ok = false;
    } else {
      int rc = fprintf( out, "export %s%s='%s'\n", val_pfx, reslist_keyname, reslist );
      if ( rc < 0 )
        ok = false;
    }

    free(reslist);
  }

  free(buffer);

  lcfgtaglist_relinquish(export_res);
  free(val_pfx);
  free(type_pfx);

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

LCFGStatus lcfgcomponent_from_statusfile( const char * filename,
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

  size_t line_len = 128;
  char * line = malloc( line_len * sizeof(char) );
  if ( line == NULL ) {
    perror( "Failed to allocate memory for status parser buffer" );
    exit(EXIT_FAILURE);
  }

  char * statusline = NULL;

  int linenum = 1;
  while( getline( &line, &line_len, fp ) != -1 ) {

    /* Need a copy of the string as it will be mangled by the parser */

    free(statusline);
    statusline = strdup(line);
    if ( statusline == NULL ) {
      perror( "Failed to copy status line" );
      exit(EXIT_FAILURE);
    }

    lcfgutils_chomp(statusline);

    /* Search for the '=' which separates status keys and values */

    char * sep = strchr( statusline, '=' );
    if ( sep == NULL ) {
      lcfgutils_build_message( msg, "Failed to parse line %d (missing '=' character)",
                linenum );
      ok = false;
      break;
    }

    /* Replace the '=' separator with a NULL so that can avoid
       unnecessarily dupping the string */

    *sep = '\0';

    /* The value is everything after the separator (could just be an
       empty string) */

    char * status_value = sep + 1;

    /* Find the component name (if any) and the resource name */

    char * this_hostname = NULL;
    char * this_compname = NULL;
    char * this_resname  = NULL;
    char this_type       = LCFG_RESOURCE_SYMBOL_VALUE;

    if ( !lcfgresource_parse_key( statusline, &this_hostname, &this_compname,
                                  &this_resname, &this_type ) ) {
      lcfgutils_build_message( msg, "Failed to parse line %d (invalid key '%s')",
                linenum, statusline );
      ok = false;
      break;
    }

    /* Check for valid resource name */

    if ( !lcfgresource_valid_name(this_resname) ) {
      lcfgutils_build_message( msg, "Failed to parse line %d (invalid resource name '%s')",
                linenum, this_resname );
      ok = false;
      break;
    }

    /* Insist on the component names matching */

    if ( this_compname != NULL && strcmp( this_compname, compname ) != 0 ) {
      lcfgutils_build_message( msg, "Failed to parse line %d (invalid component name '%s')",
                linenum, this_compname );
      ok = false;
      break;
    }

    /* Grab the resource or create a new one if necessary */

    LCFGResource * res =
      lcfgcomponent_find_or_create_resource( comp, this_resname );

    if ( res == NULL ) {
      lcfgutils_build_message( msg, "Failed to parse line %d of status file '%s'",
		linenum, statusfile );
      ok = false;
      break;
    }

    /* Apply the action which matches with the symbol at the start of
       the status line or assume this is a simple specification of the
       resource value. */

    char * this_value = strdup(status_value);

    /* Value strings may be html encoded as they can contain
       whitespace characters which would otherwise corrupt the status
       file formatting. */

    if ( this_type == LCFG_RESOURCE_SYMBOL_VALUE )
      lcfgutils_decode_html_entities_utf8( this_value, NULL );

    char * set_msg = NULL;
    bool set_ok = lcfgresource_set_attribute( res, this_type, this_value,
                                              &set_msg );

    if ( !set_ok ) {

      if ( set_msg != NULL ) {
        lcfgutils_build_message( msg, "Failed to process line %d (%s)",
                  linenum, set_msg );

        free(set_msg);
      } else {
        lcfgutils_build_message( msg, 
                  "Failed to process line %d (bad value '%s' for type '%c')",
                  linenum, status_value, this_type );
      }

      free(this_value);

      ok = false;
      break;
    }

    linenum++;
  }

  free(statusline);

  fclose(fp);

  free(line);

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
 * @param[in] res Pointer to @c LCFGResource
 * @param[in] val_pfx The prefix for the value variable name
 * @param[in] type_pfx The prefix for the type variable name
 * @param[in] options Integer which controls behaviour

 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcomponent_to_env( const LCFGComponent * comp,
				 const char * val_pfx, const char * type_pfx,
				 LCFGOption options,
                                 char ** msg ) {
  assert( comp != NULL );

  if ( lcfgcomponent_is_empty(comp) ) return LCFG_STATUS_OK;
  if ( !lcfgcomponent_has_name(comp) ) return LCFG_STATUS_ERROR;

  bool all_priorities = (options&LCFG_OPT_ALL_PRIORITIES);
  bool all_values     = (options&LCFG_OPT_ALL_VALUES);

  LCFGStatus status = LCFG_STATUS_OK;

  const char * comp_name = lcfgcomponent_get_name(comp);
  size_t name_len = strlen(comp_name);

  if ( val_pfx == NULL )
    val_pfx = default_val_pfx;

  if ( type_pfx == NULL )
    type_pfx = default_type_pfx;

  /* For security avoid using the user-specified prefix as a format string. */

  char * val_pfx2 = NULL;
  if ( strstr( val_pfx, env_placeholder ) != NULL ) {
    val_pfx2 = lcfgutils_string_replace( val_pfx,
					 env_placeholder, comp_name );
    val_pfx = val_pfx2;
  }

  char * type_pfx2 = NULL;

  /* No point doing this if the type data isn't required */
  if ( options&LCFG_OPT_USE_META ) {
    if ( strstr( type_pfx, env_placeholder ) != NULL ) {
      type_pfx2 = lcfgutils_string_replace( type_pfx,
					    env_placeholder, comp_name );
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

      status = lcfgresource_to_env( res, val_pfx, type_pfx, options );

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

    char * reslist_key = lcfgutils_string_join( "", val_pfx, reslist_keyname );

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
 * @param[out] msg msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcomponent_to_statusfile( const LCFGComponent * comp,
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

  int fd = mkstemp(tmpfile);
  FILE * out = fdopen( fd, "w" );
  if ( out == NULL ) {
    lcfgutils_build_message( msg, "Failed to open temporary status file '%s'",
              tmpfile );
    ok = false;
    goto cleanup;
  }

  /* For efficiency a buffer is pre-allocated. The initial size was
     chosen by looking at typical resource usage for Informatics. The
     buffer will be automatically grown when necessary, the aim is to
     minimise the number of reallocations required.  */

  size_t buf_size = 384;
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
 *
 * @return Pointer to an @c LCFGResourceNode (or the @c NULL value).
 *
 */

LCFGResourceNode * lcfgcomponent_find_node( const LCFGComponent * comp,
                                            const char * name ) {
  assert( comp != NULL );
  assert( name != NULL );

  if ( lcfgcomponent_is_empty(comp) ) return NULL;

  LCFGResourceNode * result = NULL;

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
	cur_node != NULL && result == NULL;
	cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res = lcfgcomponent_resource(cur_node); 

    if ( !lcfgresource_has_name(res) || !lcfgresource_is_active(res) ) continue;

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
 *
 * @return Pointer to an @c LCFGResource (or the @c NULL value).
 *
 */

LCFGResource * lcfgcomponent_find_resource( const LCFGComponent * comp,
                                            const char * name ) {
  assert( comp != NULL );
  assert( name != NULL );

  LCFGResource * res = NULL;

  const LCFGResourceNode * res_node = lcfgcomponent_find_node( comp, name );
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
 * @param[in] comp Pointer to @c LCFGComponent to be searched
 * @param[in] name The name of the required resource
 *
 * @return Boolean value which indicates presence of resource in component
 *
 */

bool lcfgcomponent_has_resource( const LCFGComponent * comp,
                                 const char * name ) {
  assert( comp != NULL );
  assert( name != NULL );

  return ( lcfgcomponent_find_node( comp, name ) != NULL );
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
 *
 * @return The required @c LCFGResource (or @c NULL)
 *
 */

LCFGResource * lcfgcomponent_find_or_create_resource( LCFGComponent * comp,
                                                      const char * name ) {
  assert( comp != NULL );
  assert( name != NULL );

  /* Only searches 'active' resources */

  LCFGResource * result = lcfgcomponent_find_resource( comp, name );

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
 * @brief Insert or merge a resource
 *
 * Searches the @c LCFGComponent for a matching @c LCFGResource with
 * the same name. If none is found the resource is simply added and @c
 * LCFG_CHANGE_ADDED is returned. If there is a match then the new
 * resource will be @e merged using to the priority (which comes from
 * the evaluation of the context expressions) of the two
 * resources. Whichever has the greatest priority will be retained. If
 * the new resource replaces the current then @c LCFG_CHANGE_REPLACED
 * is returned otherwise @c LCFG_CHANGE_NONE will be returned. If both
 * resources have the same priority then an unresolvable conflict
 * occurs and @c LCFG_CHANGE_ERROR is returned.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] new_res Pointer to @c LCFGResource
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcomponent_insert_or_merge_resource(
                                            LCFGComponent * comp,
                                            LCFGResource * new_res,
                                            char ** msg ) {
  assert( comp != NULL );
  assert( new_res != NULL );

  LCFGChange result = LCFG_CHANGE_ERROR;

  /* Name for resource is required */
  if ( !lcfgresource_has_name(new_res) ) return LCFG_CHANGE_ERROR;

  LCFGResourceNode * cur_node =
    lcfgcomponent_find_node( comp, lcfgresource_get_name(new_res) );

  if ( cur_node == NULL ) {
    result = lcfgcomponent_append( comp, new_res );
  } else {
    LCFGResource * cur_res = lcfgcomponent_resource(cur_node);

    int priority  = lcfgresource_get_priority(new_res);
    int opriority = lcfgresource_get_priority(cur_res);

    /* If the priority of the older version of this resource is
       greater than the proposed replacement then no change is
       required. */

    if ( opriority > priority ) {  /* nothing more to do */
      result = LCFG_CHANGE_NONE;
    } else if ( priority > opriority ||
                lcfgresource_same_value( cur_res, new_res ) ) {

      if ( !lcfgresource_same_type( cur_res, new_res ) ) {
        /* warn about different types */
      }

      /* replace current version of resource with new one */

      lcfgresource_acquire(new_res);
      cur_node->resource = new_res;

      lcfgresource_relinquish(cur_res);

      result = LCFG_CHANGE_REPLACED;

    } else {
      lcfgutils_build_message( msg, "Resource conflict" );
    }

  }

  return result;
}

/**
 * @brief Insert or replace a resource
 *
 * Searches the @c LCFGComponent for a matching @c LCFGResource with
 * the same name. If none is found the resource is simply added and @c
 * LCFG_CHANGE_ADDED is returned. If there is a match then the new
 * resource will replace the current and @c LCFG_CHANGE_REPLACED
 * is returned.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] new_res Pointer to @c LCFGResource
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcomponent_insert_or_replace_resource(
                                              LCFGComponent * comp,
                                              LCFGResource * new_res,
                                              char ** msg ) {
  assert( comp != NULL );
  assert( new_res != NULL );

  LCFGChange result = LCFG_CHANGE_ERROR;

  /* Name for resource is required */
  if ( !lcfgresource_has_name(new_res) ) return LCFG_CHANGE_ERROR;

  LCFGResourceNode * cur_node =
    lcfgcomponent_find_node( comp, lcfgresource_get_name(new_res) );

  if ( cur_node == NULL ) {
    result = lcfgcomponent_append( comp, new_res );
  } else {
    LCFGResource * cur_res = lcfgcomponent_resource(cur_node);

    /* replace current version of resource with new one */

    lcfgresource_acquire(new_res);
    cur_node->resource = new_res;

    lcfgresource_relinquish(cur_res);

    result = LCFG_CHANGE_REPLACED;
  }

  return result;
}

/**
 * @brief Merge overrides from one component to another
 *
 * Iterates through the list of resources in the overrides @c
 * LCFGComponent and merges them to the target component by calling
 * @c lcfgcomponent_insert_or_replace_resource().
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] overrides Pointer to override @c LCFGComponent
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcomponent_merge( LCFGComponent * comp,
                                const LCFGComponent * overrides,
                                char ** msg ) {
  assert( comp != NULL );

  if ( lcfgcomponent_is_empty(overrides) ) return LCFG_CHANGE_NONE;

  LCFGChange change = LCFG_CHANGE_NONE;

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(overrides);
	cur_node != NULL && change != LCFG_CHANGE_ERROR;
	cur_node = lcfgcomponent_next(cur_node) ) {

    LCFGResource * override_res = lcfgcomponent_resource(cur_node);

    LCFGChange rc =
      lcfgcomponent_insert_or_replace_resource( comp, override_res, msg );

    if ( rc == LCFG_CHANGE_ERROR ) {
      change = LCFG_CHANGE_ERROR;
    } else if ( rc != LCFG_CHANGE_NONE ) {
      change = LCFG_CHANGE_MODIFIED;
    }

  }

  return change;
}

/**
 * @brief Get the list of resource names as a string
 *
 * This generates a new string which contains a space-separated sorted
 * list of resource names for the @c LCFGComponent. If the component
 * is empty then an empty string will be returned.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 * @return Pointer to a new string (call @c free(3) when no longer required)
 *
 */

char * lcfgcomponent_get_resources_as_string(const LCFGComponent * comp) {
  assert( comp != NULL );

  if ( lcfgcomponent_is_empty(comp) ) return strdup("");

  LCFGTagList * reslist = lcfgtaglist_new();

  bool ok = true;

  const LCFGResourceNode * cur_node = NULL;
  for ( cur_node = lcfgcomponent_head(comp);
        cur_node != NULL && ok;
        cur_node = lcfgcomponent_next(cur_node) ) {

    const LCFGResource * res = lcfgcomponent_resource(cur_node);

    if ( !lcfgresource_is_active(res) || !lcfgresource_has_name(res) )
      continue;

    const char * res_name = lcfgresource_get_name(res);

    char * msg = NULL;
    LCFGChange change = lcfgtaglist_mutate_add( reslist, res_name, &msg );
    if ( change == LCFG_CHANGE_ERROR )
      ok = false;

    free(msg); /* Just ignoring any message */
  }

  size_t buf_len = 0;
  char * res_as_str = NULL;

  if (ok) {
    lcfgtaglist_sort(reslist);

    if ( lcfgtaglist_to_string( reslist, 0,
                                &res_as_str, &buf_len ) < 0 ) {
      ok = false;
    }
  }

  lcfgtaglist_relinquish(reslist);

  if ( !ok ) {
    free(res_as_str);
    res_as_str = NULL;
  }

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
 * @param[out] Reference to the pointer for the new @c LCFGComponent
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

  if ( val_pfx == NULL )
    val_pfx = default_val_pfx;

  if ( type_pfx == NULL )
    type_pfx = default_type_pfx;

  /* For security avoid using the user-specified prefix as a format string. */

  char * val_pfx2 = NULL;
  if ( strstr( val_pfx, env_placeholder ) != NULL ) {
    val_pfx2 = lcfgutils_string_replace( val_pfx,
                                         env_placeholder, compname_in );
    val_pfx = val_pfx2;
  }

  char * type_pfx2 = NULL;

  if ( strstr( type_pfx, env_placeholder ) != NULL ) {
    type_pfx2 = lcfgutils_string_replace( type_pfx,
                                          env_placeholder, compname_in );
    type_pfx = type_pfx2;
  }

  /* Declare variables here which need to be defined before a jump to
     the cleanup stage */

  LCFGStatus status = LCFG_STATUS_OK;
  LCFGComponent * comp = NULL;
  LCFGTagList * import_res = NULL;

  /* Find the list of resource names for the component */

  char * reslist_key = lcfgutils_string_join( "", val_pfx, reslist_keyname );

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
    status = lcfgresource_from_env( resname, val_pfx, type_pfx, 
                                    &res, msg );

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

/* eof */
