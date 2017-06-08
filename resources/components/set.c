/**
 * @file resources/components/set.c
 * @brief Functions for working with sets of LCFG components
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "components.h"
#include "utils.h"

#define LCFG_COMPSET_DEFAULT_SIZE 113
#define LCFG_COMPSET_LOAD_INIT 0.5
#define LCFG_COMPSET_LOAD_MAX  0.7

static double lcfgcompset_load_factor( const LCFGComponentSet * compset ) {
  return ( (double) compset->entries / (double) compset->buckets );
}

static void lcfgcompset_resize( LCFGComponentSet * compset ) {

  double load_factor = lcfgcompset_load_factor(compset);

  size_t want_buckets = compset->buckets;
  if ( load_factor >= LCFG_COMPSET_LOAD_MAX ) {
    want_buckets = (size_t) 
      ( (double) compset->entries / LCFG_COMPSET_LOAD_INIT ) + 1;
  }

  if ( want_buckets > compset->buckets || compset->components == NULL ) {

    LCFGComponent ** old_set = compset->components;
    size_t old_buckets = compset->buckets;

    LCFGComponent ** new_set = calloc( (size_t) want_buckets,
                                       sizeof(LCFGComponent *) );

    compset->components = new_set;
    compset->entries    = 0;
    compset->buckets    = want_buckets;

    if ( old_set != NULL && compset->entries > 0 ) {

      unsigned int i;
      for ( i=0; i<old_buckets; i++ ) {
        LCFGComponent * comp = old_set[i];
        if (comp) {
          LCFGChange rc = lcfgcompset_insert_component( compset, comp );
          if ( rc == LCFG_CHANGE_ERROR ) {
            fprintf( stderr, "Failed to resize component set\n" );
            exit(EXIT_FAILURE);
          }
          lcfgcomponent_relinquish(comp);
        }
      }

      free(old_set);
    }

  }

}

/**
 * @brief Create and initialise a new empty set of components
 *
 * Creates a new @c LCFGComponentSet which represents an empty set
 * of components.
 *
 * If the memory allocation for the new structure is not successful the
 * @c exit() function will be called with a non-zero value.
 *
 * The reference count for the structure is initialised to 1. To avoid
 * memory leaks, when it is no longer required the
 * @c lcfgcompset_relinquish() function should be called.
 *
 * @return Pointer to new @c LCFGComponentSet
 *
 */

LCFGComponentSet * lcfgcompset_new() {

  LCFGComponentSet * compset = malloc( sizeof(LCFGComponentSet) );
  if ( compset == NULL ) {
    perror( "Failed to allocate memory for LCFG component set" );
    exit(EXIT_FAILURE);
  }

  compset->components = NULL;
  compset->entries    = 0;
  compset->buckets    = LCFG_COMPSET_DEFAULT_SIZE;
  compset->_refcount  = 1;

  lcfgcompset_resize(compset);

  return compset;
}

/**
 * @brief Destroy the component set
 *
 * When the specified @c LCFGComponentSet is no longer required this
 * will free all associated memory.
 *
 * *Reference Counting:* There is support for very simple reference
 * counting which allows an @c LCFGComponentSet to appear in multiple
 * situations. Incrementing and decrementing that reference counter is
 * the responsibility of the container code. See @c
 * lcfgcompset_acquire() and @c lcfgcompset_relinquish() for
 * details.
 *
 * This will iterate through the set of components to call @c
 * lcfgcomponent_relinquish() for each component. Note that if the
 * reference count on a component reaches zero then the
 * @c LCFGComponent will be destroyed.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * component set which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] compset Pointer to @c LCFGComponentSet to be destroyed.
 *
 */

void lcfgcompset_destroy(LCFGComponentSet * compset) {

  if ( compset == NULL ) return;

  LCFGComponent ** components = compset->components;

  unsigned int i;
  for (i=0; i < compset->buckets; i++ ) {
    if ( components[i] ) {
      lcfgcomponent_relinquish(components[i]);
      components[i] = NULL;
    }
  }

  free(compset->components);
  compset->components = NULL;

  free(compset);
  compset = NULL;
}

/**
 * @brief Acquire reference to component set
 *
 * This is used to record a reference to the @c LCFGComponentSet, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the structure is no
 * longer required the @c lcfgcompset_release() function should be
 * called.
 *
 * @param[in] compset Pointer to @c LCFGComponentSet
 *
 */

void lcfgcompset_acquire(LCFGComponentSet * compset) {
  assert( compset != NULL );

  compset->_refcount += 1;
}

/**
 * @brief Release reference to component set
 *
 * This is used to release a reference to the @c LCFGComponentSet
 * it does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgcompset_destroy() function
 * will be called to clean up the memory associated with the structure.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * component set which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] compset Pointer to @c LCFGComponentSet
 *
 */

void lcfgcompset_relinquish( LCFGComponentSet * compset ) {

  if ( compset == NULL ) return;

  if ( compset->_refcount > 0 )
    compset->_refcount -= 1;

  if ( compset->_refcount == 0 )
    lcfgcompset_destroy(compset);

}

/**
 * @brief Insert a component into the set
 *
 * This can be used to insert an @c LCFGComponent into the specified
 * @c LCFGComponentSet. If a component with the same name already
 * exists in the set it will be replaced.
 *
 * If the component is successfully inserted the @c LCFG_CHANGE_ADDED
 * value is returned, if it replaces a previous component with the
 * same name then the @c LCFG_CHANGE_REPLACED value is returned, if an
 * error occurs then @c LCFG_CHANGE_ERROR is returned.
 *
 * @param[in] compset Pointer to @c LCFGComponentSet
 * @param[in] comp Pointer to @c LCFGComponent
 * 
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcompset_insert_component( LCFGComponentSet * compset,
                                         LCFGComponent * comp ) {
  assert( compset != NULL );

  if ( !lcfgcomponent_is_valid(comp) ) return LCFG_CHANGE_ERROR;

  lcfgcomponent_acquire(comp);

  unsigned long hash =
    lcfgutils_string_djbhash( comp->name, NULL ) % compset->buckets;

  LCFGChange change = LCFG_CHANGE_NONE;

  LCFGComponent ** components = compset->components;

  unsigned long i;
  for ( i = hash; change == LCFG_CHANGE_NONE && i < compset->buckets; i++ ) {
    if ( !components[i] ) {
      components[i] = comp;
      change = LCFG_CHANGE_ADDED;
    } else if ( lcfgcomponent_same_name( comp, components[i] ) ) {
      lcfgcomponent_relinquish(components[i]);
      components[i] = comp;
      change = LCFG_CHANGE_REPLACED;
    }
  }

  for ( i = 0; change == LCFG_CHANGE_NONE && i<hash; i++ ) {
    if ( !components[i] ) {
      components[i] = comp;
      change = LCFG_CHANGE_ADDED;
    } else if ( lcfgcomponent_same_name( comp, components[i] ) ) {
      lcfgcomponent_relinquish(components[i]);
      components[i] = comp;
      change = LCFG_CHANGE_REPLACED;
    }
  }

  if ( change == LCFG_CHANGE_ADDED ) {
    compset->entries += 1;
  } else if ( change == LCFG_CHANGE_NONE ) {
    lcfgcomponent_relinquish(comp);
    change = LCFG_CHANGE_ERROR; /* promote to error */
  }

  return change;
}

/**
 * @brief Check if a set contains a particular component
 *
 * This can be used to search through an @c LCFGComponentSet to check if
 * it contains a component with a matching name. Note that the matching
 * is done using strcmp(3) which is case-sensitive.
 * 
 * This uses the @c lcfgcompset_find_component() function to find the
 * relevant component. If a @c NULL value is specified for the set or the
 * set is empty then a false value will be returned.
 *
 * @param[in] compset Pointer to @c LCFGComponentSet to be searched
 * @param[in] want_name The name of the required component
 *
 * @return Boolean value which indicates presence of component in set
 *
 */

bool lcfgcompset_has_component( const LCFGComponentSet * compset,
                                const char * want_name ) {
  assert( want_name != NULL );

  return ( lcfgcompset_find_component( compset, want_name ) != NULL );
}

/**
 * @brief Find the component for a given name
 *
 * This can be used to search through an @c LCFGComponentSet to find
 * the component which has a matching name. Note that the matching is
 * done using strcmp(3) which is case-sensitive.
 *
 * To ensure the returned @c LCFGComponent is not destroyed when
 * the parent @c LCFGComponentSet is destroyed it is necessary to
 * call the @c lcfgcomponent_acquire() function.
 *
 * @param[in] compset Pointer to @c LCFGComponentSet to be searched
 * @param[in] want_name The name of the required component
 *
 * @return Pointer to an @c LCFGComponent (or the @c NULL value).
 *
 */

LCFGComponent * lcfgcompset_find_component( const LCFGComponentSet * compset,
                                            const char * want_name ) {
  assert( want_name != NULL );

  if ( lcfgcompset_is_empty(compset) ) return NULL;

  unsigned long hash =
    lcfgutils_string_djbhash( want_name, NULL ) % compset->buckets;

  LCFGComponent ** components = compset->components;

  LCFGComponent * result = NULL;

  /* Hitting an empty bucket is an immediate "failure" */
  bool failed = false;

  unsigned long i;
  for ( i = hash; result == NULL && !failed && i < compset->buckets; i++ ) {
    if ( !components[i] )
      failed = true;
    else if ( lcfgcomponent_match( components[i], want_name ) )
      result = components[i];
  }

  for ( i = 0; result == NULL && !failed && i<hash; i++ ) {
    if ( !components[i] )
      failed = true;
    else if ( lcfgcomponent_match( components[i], want_name ) )
      result = components[i];
  }

  return result;
}

/**
 * @brief Find or create a new component
 *
 * Searches the @c LCFGComponentSet to find an @c LCFGComponent with
 * the required name. If none is found then a new @c LCFGComponent is
 * created and added to the @c LCFGComponentSet.
 *
 * If an error occurs during the creation of a new component a @c NULL
 * value will be returned.
 *
 * @param[in] compset Pointer to @c LCFGComponentSet
 * @param[in] name The name of the required component
 *
 * @return The required @c LCFGComponent (or @c NULL)
 *
 */

LCFGComponent * lcfgcompset_find_or_create_component(
                                             LCFGComponentSet * compset,
                                             const char * name ) {

  assert( compset != NULL );
  assert( name != NULL );

  LCFGComponent * result = lcfgcompset_find_component( compset, name );

  if ( result != NULL ) return result;

  /* If not found then create new component and add it to the set */
  result = lcfgcomponent_new();

  char * comp_name = strdup(name);
  bool ok = lcfgcomponent_set_name( result, comp_name );

  if ( !ok ) {
    free(comp_name);
  } else {

    LCFGChange rc = lcfgcompset_insert_component( compset, result );

    if ( rc == LCFG_CHANGE_ERROR )
      ok = false;

  }

  lcfgcomponent_relinquish(result);

  if (!ok)
    result = NULL;

  return result;
}

/**
 * @brief Write set of serialised components to file stream
 *
 * This simply calls @c lcfgcomponent_print() for each component in
 * the @c LCFGComponentSet. The style and options are passed onto
 * that function.
 *
 * @param[in] compset Pointer to @c LCFGComponentSet
 * @param[in] style Integer indicating required style of formatting
 * @param[in] options Integer that controls formatting
 * @param[in] out Stream to which the set of components should be written
 *
 * @return Boolean indicating success
 *
 */

bool lcfgcompset_print( const LCFGComponentSet * compset,
                        LCFGResourceStyle style,
                        LCFGOption options,
                        FILE * out ) {
  assert( compset != NULL );

  if ( lcfgcompset_is_empty(compset) ) return true;

  LCFGComponent ** components = compset->components;

  bool ok = true;

  unsigned int i;
  for ( i=0; i < compset->buckets && ok; i++ ) {

    /* Not const here since we will sort the component */
    LCFGComponent * comp = components[i];

    if (comp) {
      lcfgcomponent_sort(comp);

      ok = lcfgcomponent_print( comp, style, options, out );
    }

  }

  return ok;
}

/**
 * @brief Merge sets of components
 *
 * This will @e merge the components from the second @c
 * LCFGComponentSet into the first. If the component appears in both
 * sets the @c lcfgcomponent_merge_component() function will be used
 * to merge the two. For each resource in the second component if it
 * also exists in the first set it will be replaced if not it is just
 * added.
 *
 * If the component from the second set does NOT exist in the first
 * then it will only be added when the @c take_new parameter is set to
 * true. When the @c take_new parameter is false this is effectively
 * an "override-only" mode which only changes existing components.
 *
 * @param[in] compset1 Pointer to @c LCFGComponentSet to be merged to
 * @param[in] compset2 Pointer to @c LCFGComponentSet to be merged from
 * @param[in] take_new Boolean which controls whether components are added
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcompset_merge_components( LCFGComponentSet * compset1,
                                         const LCFGComponentSet * compset2,
                                         bool take_new,
                                         char ** msg ) {
  assert( compset1 != NULL );

  /* No overrides to apply if second list is empty */
  if ( lcfgcompset_is_empty(compset2) ) return LCFG_CHANGE_NONE;

  /* Only overriding existing components so nothing to do if list is empty */
  if ( lcfgcompset_is_empty(compset1) && !take_new ) return LCFG_CHANGE_NONE;

  LCFGChange change = LCFG_CHANGE_NONE;

  unsigned int i;
  for ( i=0; i < compset2->buckets && change != LCFG_CHANGE_ERROR; i++ ) {

    LCFGComponent * override_comp = (compset2->components)[i];

    if ( override_comp ) {
      const char * comp_name = lcfgcomponent_get_name(override_comp);

      LCFGComponent * target_comp =
        lcfgcompset_find_component( compset1, comp_name );

      LCFGChange rc = LCFG_CHANGE_NONE;
      if ( target_comp != NULL ) {
        rc = lcfgcomponent_merge_component( target_comp, override_comp, msg );
      } else if ( take_new ) {
        rc = lcfgcompset_insert_component( compset1, override_comp );
      }

      if ( rc == LCFG_CHANGE_ERROR ) {
        change = LCFG_CHANGE_ERROR;
      } else if ( rc != LCFG_CHANGE_NONE ) {
        change = LCFG_CHANGE_MODIFIED;
      }
    }

  }

  return change;
}

/**
 * @brief Copy components from one set to another
 *
 * This will copy all the components in the second @c LCFGComponentSet
 * into the first. If the component already exists in the target set
 * it will be replaced if not the component is simply added. This is
 * done using the @c lcfgcompset_insert_component() function.
 *
 * @param[in] compset1 Pointer to @c LCFGComponentSet to be copied to
 * @param[in] compset2 Pointer to @c LCFGComponentSet to be copied from
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcompset_transplant_components( LCFGComponentSet * compset1,
                                              const LCFGComponentSet * compset2,
                                              char ** msg ) {
  assert( compset1 != NULL );

  /* No overrides to apply if second list is empty */
  if ( lcfgcompset_is_empty(compset2) ) return LCFG_CHANGE_NONE;

  LCFGChange change = LCFG_CHANGE_NONE;

  unsigned int i;
  for ( i=0; i < compset2->buckets && change != LCFG_CHANGE_ERROR; i++ ) {

    LCFGComponent * override_comp = (compset2->components)[i];

    if ( override_comp ) {

      LCFGChange rc = lcfgcompset_insert_component( compset1, override_comp );

      if ( rc == LCFG_CHANGE_ERROR ) {
        const char * comp_name = lcfgcomponent_get_name(override_comp);
        lcfgutils_build_message( msg, "Failed to copy '%s' component",
                                 comp_name);
        change = LCFG_CHANGE_ERROR;
      } else if ( rc != LCFG_CHANGE_NONE ) {
        change = LCFG_CHANGE_MODIFIED;
      }

    }

  }

  return change;
}

/**
 * @brief Load resources for components from status directory
 *
 * This creates a new @c LCFGComponentSet which holds a set of
 * components which are loaded using the @c
 * lcfgcomponent_from_status_file() function from the status files
 * found in the directory.
 *
 * It is expected that the file names will be valid component names,
 * any files with invalid names will simply be ignored. Empty files
 * will also be ignored.
 *
 * To limit which components are loaded a set of required names can be
 * specified as a @c LCFGTagList. If the list is empty or a @c NULL
 * value is passed in all components will be loaded.
 *
 * If the status directory does not exist an error will be returned
 * unless the @c LCFG_ALLOW_NOEXIST option is specified, in that case
 * an empty @c LCFGComponentSet will be returned.
 *
 * @param[in] status_dir Path to directory of status files
 * @param[out] result Reference to pointer for new @c LCFGComponentSet
 * @param[in] comps_wanted List of names for required components
 * @param[in] options Controls the behaviour of the process
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcompset_from_status_dir( const char * status_dir,
                                         LCFGComponentSet ** result,
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

  /* Create the new empty component set which will eventually be returned */

  LCFGComponentSet * compset = lcfgcompset_new();

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

        LCFGChange insert_rc = lcfgcompset_insert_component( compset,
                                                             component );

        if ( insert_rc == LCFG_CHANGE_ERROR ) {
          status = LCFG_STATUS_ERROR;
          lcfgutils_build_message( msg, "Failed to read status file '%s'",
                                   status_file );
        }

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
    lcfgcompset_relinquish(compset);
    compset = NULL;
  }

  *result = compset;

  return status;
}

/**
 * @brief Write out status files for all components in the set
 *
 * For each @c LCFGComponent in the @c LCFGComponentSet this will
 * call the @c lcfgcomponent_to_status_file() function to write out
 * the resource state as a status file. Any options specified will be
 * passed on to that function.
 *
 * @param[in] compset Pointer to @c LCFGComponentSet (may be @c NULL)
 * @param[in] status_dir Path to directory for status files
 * @param[in] options Controls the behaviour of the process
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcompset_to_status_dir( const LCFGComponentSet * compset,
                                      const char * status_dir,
                                      LCFGOption options,
                                      char ** msg ) {
  assert( status_dir != NULL );

  if ( isempty(status_dir) ) {
    lcfgutils_build_message( msg, "Invalid status directory name" );
    return LCFG_STATUS_ERROR;
  }

  if ( lcfgcompset_is_empty(compset) ) return LCFG_STATUS_OK;

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

  LCFGComponent ** components = compset->components;

  unsigned int i;
  for ( i = 0; rc != LCFG_STATUS_ERROR && i < compset->buckets; i++ ) {

    /* Not const here since we will sort the component */
    LCFGComponent * cur_comp = components[i];
    if ( !cur_comp ) continue;

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
 * @brief Export resources for all components in the set
 *
 * For each @c LCFGComponent in the @c LCFGComponentSet this will
 * call the @c lcfgcomponent_to_env() function to export the resource
 * state to the environment. The prefixes and any options specified
 * will be passed on to that function.
 *
 * The value prefix will typically be like @c LCFG_%s_ and the type
 * prefix will typically be like @c LCFGTYPE_%s_ where @c '%s' is
 * replaced with the name of the component. If the prefixes are not
 * specified (i.e. @c NULL values are given) the default prefixes are
 * used. 
 *
 * @param[in] compset Pointer to @c LCFGComponentSet
 * @param[in] val_pfx The prefix for the value variable name
 * @param[in] type_pfx The prefix for the type variable name
 * @param[in] options Controls the behaviour of the process
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcompset_to_env( const LCFGComponentSet * compset,
                               const char * val_pfx, const char * type_pfx,
                               LCFGOption options,
                               char ** msg ) {
  assert( compset != NULL );

  if ( lcfgcompset_is_empty(compset) ) return LCFG_STATUS_OK;

  LCFGStatus status = LCFG_STATUS_OK;

  LCFGComponent ** components = compset->components;

  unsigned int i;
  for ( i = 0; status != LCFG_STATUS_ERROR && i < compset->buckets; i++ ) {

    const LCFGComponent * comp = components[i];

    if (comp)
      status = lcfgcomponent_to_env( comp, val_pfx, type_pfx, options, msg );
  }

  return status;
}

/**
 * @brief Import resources for set of components
 *
 * For each component name in the @c LCFGTagList this will call the @c
 * lcfgcomponent_from_env() function to import the resource state from
 * the environment. The prefixes and any options specified will be
 * passed on to that function.
 *
 * If the @c LCFGTagList is empty nothing will be imported and an
 * empty @c LCFGComponentSet will be returned.
 *
 * The value prefix will typically be like @c LCFG_%s_ and the type
 * prefix will typically be like @c LCFGTYPE_%s_ where @c '%s' is
 * replaced with the name of the component. If the prefixes are not
 * specified (i.e. @c NULL values are given) the default prefixes are
 * used. 
 *
 * @param[in] val_pfx The prefix for the value variable name
 * @param[in] type_pfx The prefix for the type variable name
 * @param[out] result Reference to pointer for new @c LCFGComponentSet
 * @param[in] comps_wanted List of names for required components
 * @param[in] options Controls the behaviour of the process
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcompset_from_env( const char * val_pfx, const char * type_pfx,
                                  LCFGComponentSet ** result,
                                  LCFGTagList * comps_wanted,
                                  LCFGOption options,
                                  char ** msg ) {
  assert( comps_wanted != NULL );

  LCFGStatus status = LCFG_STATUS_OK;

  LCFGComponentSet * compset = lcfgcompset_new();

  LCFGTagIterator * tagiter = lcfgtagiter_new(comps_wanted);

  const LCFGTag * tag = NULL;

  while ( status != LCFG_STATUS_ERROR &&
          ( tag = lcfgtagiter_next(tagiter) ) != NULL ) {

    const char * comp_name = lcfgtag_get_name(tag);

    LCFGComponent * new_comp = NULL;

    status = lcfgcomponent_from_env( comp_name,
                                     val_pfx, type_pfx,
                                     &new_comp,
                                     options, msg );

    if ( status != LCFG_STATUS_ERROR ) {
      LCFGChange change = lcfgcompset_insert_component( compset, new_comp );
      if ( change == LCFG_CHANGE_ERROR )
        status = LCFG_STATUS_ERROR;
    }

    lcfgcomponent_relinquish(new_comp);
  }

  lcfgtagiter_destroy(tagiter);

  if ( status == LCFG_STATUS_ERROR ) {
    lcfgcompset_relinquish(compset);
    compset = NULL;
  }

  *result = compset;

  return status;
}

/**
 * @brief Get the set of component names as a taglist
 *
 * This generates a new @c LCFGTagList which contains a list of
 * component names for the @c LCFGComponentSet. If the set is empty
 * then an empty tag list will be returned. Will return @c NULL if an
 * error occurs.
 *
 * To avoid memory leaks, when the list is no longer required the 
 * @c lcfgtaglist_relinquish() function should be called.
 *
 * @param[in] compset Pointer to @c LCFGComponentSet
 *
 * @return Pointer to a new @c LCFGTagList of component names
 *
 */

LCFGTagList * lcfgcompset_get_components_as_taglist(
					  const LCFGComponentSet * compset ) {
  assert( compset != NULL );

  LCFGTagList * comp_names = lcfgtaglist_new();

  if ( lcfgcompset_is_empty(compset) ) return comp_names;

  LCFGChange change = LCFG_CHANGE_NONE;

  LCFGComponent ** components = compset->components;

  unsigned int i;
  for ( i = 0; change != LCFG_CHANGE_ERROR && i < compset->buckets; i++ ) {

    const LCFGComponent * comp = components[i];

    if (comp) {
      const char * comp_name = lcfgcomponent_get_name(comp);

      char * msg = NULL;

      change = lcfgtaglist_mutate_add( comp_names, comp_name, &msg );

      free(msg); /* Just ignore any messages */
    }
  }

  if ( change == LCFG_CHANGE_ERROR ) {
    lcfgtaglist_relinquish(comp_names);
    comp_names = NULL;
  } else {
    lcfgtaglist_sort(comp_names);
  }

  return comp_names;
}

/**
 * @brief Get the set of component names as a string
 *
 * This generates a new string which contains a space-separated sorted
 * list of component names for the @c LCFGComponentSet. If the set
 * is empty then an empty string will be returned.
 *
 * @param[in] compset Pointer to @c LCFGComponentSet
 *
 * @return Pointer to a new string (call @c free(3) when no longer required)
 *
 */

char * lcfgcompset_get_components_as_string(
                                         const LCFGComponentSet * compset ) {
  assert( compset != NULL );

  if ( lcfgcompset_is_empty(compset) ) return strdup("");

  LCFGTagList * comp_names =
    lcfgcompset_get_components_as_taglist(compset);

  if ( comp_names == NULL ) return NULL;

  size_t buf_len = 0;
  char * res_as_str = NULL;

  if ( lcfgtaglist_to_string( comp_names, 0, &res_as_str, &buf_len ) < 0 ) {
    free(res_as_str);
    res_as_str = NULL;
  }

  lcfgtaglist_relinquish(comp_names);

  return res_as_str;
}

/**
 * @brief Get the list of ngeneric component names as a taglist
 *
 * This generates a new @c LCFGTagList which contains a list of names
 * for components in the @c LCFGComponentSet which have 'ngeneric'
 * resources. If the set is empty then an empty tag list will be
 * returned.
 *
 * To avoid memory leaks, when the list is no longer required the 
 * @c lcfgtaglist_relinquish() function should be called.
 *
 * @param[in] compset Pointer to @c LCFGComponentSet
 *
 * @return Pointer to a new @c LCFGTagList of ngeneric component names
 *
 */

LCFGTagList * lcfgcompset_ngeneric_components( const LCFGComponentSet * compset ) {

  LCFGTagList * comp_names = lcfgtaglist_new();

  if ( !lcfgcompset_is_empty(compset) ) return comp_names;

  LCFGChange change = LCFG_CHANGE_NONE;

  LCFGComponent ** components = compset->components;

  unsigned int i;
  for ( i = 0; change != LCFG_CHANGE_ERROR && i < compset->buckets; i++ ) {

    const LCFGComponent * comp = components[i];

    if ( comp && lcfgcomponent_is_ngeneric(comp) ) {

      const char * comp_name = lcfgcomponent_get_name(comp);

      char * msg = NULL;

      change = lcfgtaglist_mutate_add( comp_names, comp_name, &msg );

      free(msg); /* Just ignoring any message */
    }

  }

  if ( change == LCFG_CHANGE_ERROR ) {
    lcfgtaglist_relinquish(comp_names);
    comp_names = NULL;
  } else {
    lcfgtaglist_sort(comp_names);
  }

  return comp_names;
}

/**
 * @brief Compute the MD5 digest for the components
 *
 * This can be used to generate the hex-encoded MD5 digest signature
 * string for the resource data held in the specified @c
 * LCFGComponentSet. This is used by the LCFG client to identify the
 * profile.
 *
 * To avoid memory leaks, call @c free(3) on the generated string when
 * no longer required.
 *
 * @param[in] compset Pointer to @c LCFGComponentSet
 *
 * @return New string for MD5 signature (call @c free() when no longer required)
 *
 */

char * lcfgcompset_signature( const LCFGComponentSet * compset ) {

  if ( lcfgcompset_is_empty(compset) ) return NULL;

  /* Initialise the MD5 support */

  md5_state_t md5state;
  lcfgutils_md5_init(&md5state);

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

  LCFGComponent ** components = compset->components;

  unsigned int i;
  for ( i=0; i < compset->buckets; i++ ) {

    const LCFGComponent * comp = components[i];

    if (!comp) continue;

    const char * comp_name = lcfgcomponent_get_name(comp);

    const LCFGResourceNode * res_node = NULL;
    for ( res_node = lcfgcomponent_head(comp);
	  res_node != NULL;
	  res_node = lcfgcomponent_next(res_node) ) {

      const LCFGResource * res = lcfgcomponent_resource(res_node);

      if ( lcfgresource_is_valid(res) && lcfgresource_is_active(res) ) {

	ssize_t rc = lcfgresource_to_status( res, comp_name, LCFG_OPT_USE_META,
					     &buffer, &buf_size );
	if ( rc > 0 ) {
	  lcfgutils_md5_append( &md5state,
				(const md5_byte_t *) buffer, rc );
	}

      }

    }
  }

  md5_byte_t digest[16];
  lcfgutils_md5_finish( &md5state, digest );

  char * hex_digest = NULL;
  if ( !lcfgutils_md5_hexdigest( digest, &hex_digest ) ) {
    free(hex_digest);
    hex_digest = NULL;
  }

  free(buffer);

  return hex_digest;
}

/* eof */
