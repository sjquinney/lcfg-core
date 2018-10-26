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
#include "farmhash.h"
#include "reslist.h"

/* Component internal functions */

/* Computes the initial number of buckets which would be required for
   a hash given an expected number of entries. */

static unsigned long want_buckets( unsigned long entries ) {
  return (unsigned long) ( (double) entries / LCFG_COMP_LOAD_INIT ) + 1;
}

/* Computes the current load factor, this is just the percentage of
   buckets that are occupied. */

static double lcfgcomponent_load_factor( const LCFGComponent * comp ) {
  return ( (double) comp->entries / (double) comp->buckets );
}

static unsigned long lcfgcomponent_hash_string( const LCFGComponent * comp,
                                                const char * string ) {
  assert( comp != NULL );
  assert( string != NULL );

  size_t len = strlen(string);
  return farmhash64( string, len ) % comp->buckets;
}

/* Creates a new empty resource list then sets the 'merge rules' and
   'primary key' to be the same as for the parent component. */

static LCFGResourceList * lcfgcomponent_empty_list(const LCFGComponent * comp) {

  LCFGResourceList * new_list = lcfgreslist_new();
  new_list->merge_rules = comp->merge_rules;
  new_list->primary_key = comp->primary_key;

  return new_list;
}

/* Given a particular resource name this will return the associated
   bucket in the hash. Note that this does NOT guarantee there is a
   resource with that name already stored in the hash, the bucket
   might be empty. */

static bool lcfgcomponent_find_bucket( const LCFGComponent * comp,
                                       const char * want_name,
                                       unsigned long * bucket ) {
  assert( comp != NULL );
  assert( want_name != NULL );

  unsigned long hash = lcfgcomponent_hash_string( comp, want_name );

  LCFGResourceList ** resources = comp->resources;

  /* Find matching resource list or first empty bucket */

  bool found = false;

  unsigned long i;
  for ( i = hash; !found && i < comp->buckets; i++ ) {

    if ( !resources[i] ) {
      found = true;
    } else {
      const LCFGResource * head = lcfgreslist_first_resource(resources[i]);
      if ( head != NULL && lcfgresource_match( head, want_name ) )
        found = true;
    }

    if (found) *bucket = i;
  }

  for ( i = 0; !found && i < hash; i++ ) {

    if ( !resources[i] ) {
      found = true;
    } else {
      const LCFGResource * head = lcfgreslist_first_resource(resources[i]);
      if ( head != NULL && lcfgresource_match( head, want_name ) )
        found = true;
    }

    if (found) *bucket = i;
  }

  return found;
}

/* Finds the resource list in the hash for the specified resource
   name. Will return NULL if there is not currently anything stored
   with that name. */

static const LCFGResourceList * lcfgcomponent_find_list( const LCFGComponent * comp,
                                                   const char * want_name ) {

  assert( want_name != NULL );

  if ( lcfgcomponent_is_empty(comp) ) return NULL;

  unsigned long bucket;
  bool found = lcfgcomponent_find_bucket( comp, want_name, &bucket );

  const LCFGResourceList * result = NULL;
  if (found)
    result = (comp->resources)[bucket];

  return result;
}

/* Inserts the resource list into the specified bucket. If there is
   already a list stored in that bucket the reference count will be
   decremented. If the value specified for the resource list is NULL
   then it functions as a removal. */

static LCFGChange lcfgcomponent_set_bucket( LCFGComponent * comp,
                                            unsigned long bucket,
                                            LCFGResourceList * new ) {
  assert( comp != NULL );

  LCFGResourceList ** resources = comp->resources;

  LCFGResourceList * current = resources[bucket];

  LCFGChange change = LCFG_CHANGE_NONE;
  if ( current != new ) {

    resources[bucket] = new;

    if ( new != NULL ) {
      lcfgreslist_acquire(new);

      if ( current == NULL ) {   /* add */
        comp->entries++;

        change = LCFG_CHANGE_ADDED;
      } else {                   /* replace */
        change = LCFG_CHANGE_REPLACED;
      }

    } else {

      if ( current != NULL ) {   /* remove */
        comp->entries--;

        change = LCFG_CHANGE_REMOVED;
      }

    }

    lcfgreslist_relinquish(current);
  }

  return change;
}

/* Inserts a resource list into the correct bucket in the hash. */

static LCFGChange lcfgcomponent_insert_list( LCFGComponent * comp,
                                             LCFGResourceList * list ) {
  assert( comp != NULL );

  if ( lcfgreslist_is_empty(list) ) return LCFG_CHANGE_NONE;

  LCFGChange change = LCFG_CHANGE_NONE;

  const char * name = lcfgreslist_get_name(list);

  unsigned long bucket;
  if ( lcfgcomponent_find_bucket( comp, name, &bucket ) )
    change = lcfgcomponent_set_bucket( comp, bucket, list );
  else
    change = LCFG_CHANGE_ERROR;

  return change;
}

/* Resizes the hash to the 'best' size for the number of entries. For
   efficiency the code avoids constant resizes in small steps by only
   actually doing a resize once the maximum allowed 'load factor' is
   exceeded. */

static void lcfgcomponent_resize( LCFGComponent * comp ) {

  LCFGResourceList ** cur_set = comp->resources;
  unsigned long cur_buckets   = comp->buckets;
  unsigned long cur_entries   = comp->entries;

  /* Decide if a resize is actually required */

  double load_factor = lcfgcomponent_load_factor(comp);

  unsigned long new_buckets = cur_buckets;
  if ( load_factor >= LCFG_COMP_LOAD_MAX )
    new_buckets = want_buckets(cur_entries);

  if ( cur_set != NULL && new_buckets <= cur_buckets ) return;

  /* Resize the hash */

  LCFGResourceList ** new_set = calloc( (size_t) new_buckets,
                                        sizeof(LCFGResourceList *) );
  if ( new_set == NULL ) {
    perror( "Failed to allocate memory for LCFG resources set" );
    exit(EXIT_FAILURE);
  }

  comp->resources = new_set;
  comp->entries   = 0;
  comp->buckets   = new_buckets;

  /* If there are any resources in the hash then they need to be transferred */

  if ( cur_set != NULL ) {

    LCFGChange change = LCFG_CHANGE_NONE;

    unsigned long i;
    for ( i=0; LCFGChangeOK(change) && i<cur_buckets; i++ )
      change = lcfgcomponent_insert_list( comp, cur_set[i] );

    if ( LCFGChangeOK(change) ) {

      /* Need to decrement reference count for each resource list in
         the old hash and free memory for previous hash */

      for ( i=0; LCFGChangeOK(change) && i<cur_buckets; i++ )
        lcfgreslist_relinquish(cur_set[i]);

      free(cur_set);

    } else { /* Restore previous state if error occurs */

      comp->resources = cur_set;
      comp->entries   = cur_entries;
      comp->buckets   = cur_buckets;

    }

  }

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

  LCFGComponent * comp = calloc( 1, sizeof(LCFGComponent) );
  if ( comp == NULL ) {
    perror( "Failed to allocate memory for LCFG component" );
    exit(EXIT_FAILURE);
  }

  comp->merge_rules = LCFG_MERGE_RULE_NONE;
  comp->primary_key = LCFG_COMP_PK_NAME;

  comp->resources   = NULL;
  comp->entries     = 0;
  comp->buckets     = LCFG_COMP_DEFAULT_SIZE;
  comp->_refcount   = 1;

  lcfgcomponent_resize(comp);

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

  unsigned long i;
  for ( i=0; i < comp->buckets; i++ )
    lcfgcomponent_set_bucket( comp, i, NULL );

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

static void lcfgcomponent_destroy(LCFGComponent * comp) {

  if ( comp == NULL ) return;

  lcfgcomponent_remove_all_resources(comp);

  free(comp->resources);
  comp->resources = NULL;

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
 * longer required the @c lcfgcomponent_relinquish() function should be
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
 * @brief Check if there are multiple references to the component
 *
 * The @c LCFGComponent structure supports reference counting - see
 * @c lcfgcomponent_acquire() and @c lcfgcomponent_relinquish(). This
 * will return a boolean which indicates if there are multiple
 * references.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 * @return Boolean which indicates if there are multiple references
 *
 */

bool lcfgcomponent_is_shared( const LCFGComponent * comp ) {
  assert( comp != NULL );
  return ( comp->_refcount > 1 );
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

  /* Apply to all child resource lists */

  bool ok = true;

  LCFGResourceList ** resources = comp->resources;

  unsigned long i;
  for ( i=0; ok && i < comp->buckets; i++ ) {
    if ( resources[i] )
      ok = lcfgreslist_set_merge_rules( resources[i], new_rules );
  }

  return ok;
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
 * @brief Get the number of resources in the component
 *
 * Returns the number of @c LCFGResource instances stored in the @c
 * LCFGComponent. Note that when the primary key is based on both name
 * and context the component may contain multiple resources with the
 * same name.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 * @return Integer number of resources
 *
 */

unsigned long lcfgcomponent_size( const LCFGComponent * comp ) {

  unsigned long size = 0;

  /* No point scanning the whole array if there are no entries */

  if ( comp != NULL && comp->entries > 0 ) {

    if ( comp->primary_key == LCFG_COMP_PK_NAME ) {
      size = comp->entries;
    } else {
      LCFGResourceList ** resources = comp->resources;

      unsigned long i;
      for ( i=0; i < comp->buckets; i++ ) {
        if ( resources[i] )
          size += lcfgreslist_size(resources[i]);
      }
    }

  }

  return size;
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
 * @brief Clone a component
 *
 * This will create a clone of the specified @c LCFGComponent. Note
 * that the component API supports Copy-On-Write, consequently all the
 * resources will initially be shared between the two components. To
 * avoid unexpected 'action at a distance' any modification of the
 * resources stored in the component will result in them becoming
 * duplicated so they are no longer shared.
 *
 * @param[in] comp Pointer to @c LCFGComponent to be cloned.
 *
 * @return Pointer to new clone @c LCFGComponent (@c NULL if error occurs)
 *
 */

LCFGComponent * lcfgcomponent_clone( const LCFGComponent * comp ) {

  bool ok = true;

  LCFGComponent * clone = lcfgcomponent_new();

  /* Copy over the name if present */

  if ( lcfgcomponent_has_name(comp) ) {
    char * new_name = strdup( lcfgcomponent_get_name(comp) );
    ok = lcfgcomponent_set_name( clone, new_name );
    if ( !ok ) {
      free(new_name);
      goto cleanup;
    }
  }

  clone->primary_key = comp->primary_key;
  clone->merge_rules = comp->merge_rules;

  /* Avoid multiple calls to resize() by increasing the size of the set
     of buckets in the clone before starting to merge resources */

  unsigned long new_buckets = want_buckets(comp->entries);
  if ( new_buckets > clone->buckets ) {
    clone->buckets = new_buckets;
    lcfgcomponent_resize(clone);
  }

  /* Copy over the resource lists - note that the resource lists will
     be shared between the original and clone components */

  LCFGChange insert_rc = LCFG_CHANGE_NONE;

  unsigned long i;
  for ( i=0; LCFGChangeOK(insert_rc) && i<comp->buckets; i++ )
    insert_rc = lcfgcomponent_insert_list( clone, (comp->resources)[i] );

  ok = LCFGChangeOK(insert_rc);

 cleanup:

  if ( !ok ) {
    lcfgcomponent_relinquish(clone);
    clone = NULL;
  }

  return clone;
}

/* Use for sorting entries by name */
struct LCFGComponentEntry {
  const char * name;
  unsigned int bucket;
};

typedef struct LCFGComponentEntry LCFGComponentEntry;

static int lcfgcomponent_entry_cmp( const void * a, const void * b ) {
  const char * a_name = ( (const LCFGComponentEntry *) a )->name;
  const char * b_name = ( (const LCFGComponentEntry *) b )->name;
  return strcasecmp( a_name, b_name );
}

/* Returns a sorted list of resource names along with the associated buckets */

static unsigned int lcfgcomponent_sorted_entries( const LCFGComponent * comp,
                                                  LCFGComponentEntry ** result ) {

   LCFGComponentEntry * entries =
     calloc( comp->entries + 1, sizeof(LCFGComponentEntry) );

   unsigned long count = 0;

   unsigned long i;
   for (i=0; i < comp->buckets; i++ ) {
     LCFGResourceList * list = (comp->resources)[i];

     if ( !lcfgreslist_is_empty(list) ) {

       unsigned int k = count++;
       entries[k].name = lcfgreslist_get_name(list);
       entries[k].bucket = i;

     }
   }

   if ( count > 0 ) {
     qsort( entries, count, sizeof(LCFGComponentEntry), lcfgcomponent_entry_cmp );
   } else {
     free(entries);
     entries = NULL;
   }

   *result = entries;

   return count;
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

  /* Use a separate function for printing in the 'export' style */
  if ( style == LCFG_RESOURCE_STYLE_EXPORT ) {
    LCFGStatus export_rc =
      lcfgcomponent_to_export( comp, NULL, NULL, options, out );
    return ( export_rc != LCFG_STATUS_ERROR );
  }

  if ( lcfgcomponent_is_empty(comp) ) return true;

  options |= LCFG_OPT_NEWLINE;

  /* Preallocate string buffer for efficiency */

  size_t buf_size = 512;
  char * buffer = calloc( buf_size, sizeof(char) );
  if ( buffer == NULL ) {
    perror( "Failed to allocate memory for LCFG resource buffer" );
    exit(EXIT_FAILURE);
  }

  LCFGComponentEntry * entries = NULL;
  unsigned int count = lcfgcomponent_sorted_entries( comp, &entries );

  const char * comp_name = lcfgcomponent_get_name(comp);
  LCFGResourceList ** resources = comp->resources;

  bool ok = true;

  unsigned long i;
  for ( i=0; i<count && ok; i++ ) {

    unsigned int bucket = entries[i].bucket;

    ok = lcfgreslist_print( resources[bucket], comp_name, style, options,
                            &buffer, &buf_size, out );
  }
  free(buffer);
  free(entries);

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

LCFGStatus lcfgcomponent_to_export( const LCFGComponent * comp,
                                    const char * val_pfx, const char * type_pfx,
                                    LCFGOption options,
                                    FILE * out ) {
  assert( comp != NULL );

  if ( !lcfgcomponent_is_valid(comp) ) return LCFG_STATUS_ERROR;
  if ( lcfgcomponent_is_empty(comp) )  return LCFG_STATUS_OK;

  bool all_values = (options&LCFG_OPT_ALL_VALUES);

  const char * comp_name = lcfgcomponent_get_name(comp);

  bool ok = true;

  /* For efficiency the prefixes are expanded to include the component name */

  char * val_pfx2  = NULL;
  char * type_pfx2 = NULL;

  size_t val_pfx_size = 0;
  ssize_t val_pfx_len =
    lcfgresource_build_env_var( NULL, comp_name,
                                LCFG_RESOURCE_ENV_VAL_PFX, val_pfx,
                                &val_pfx2, &val_pfx_size );

  if ( val_pfx_len < 0 ) {
    ok = false;
    goto cleanup;
  }

  /* No point doing this if the type data isn't required */
  if ( options&LCFG_OPT_USE_META ) {
    size_t type_pfx_size = 0;
    ssize_t type_pfx_len =
      lcfgresource_build_env_var( NULL, comp_name,
                                  LCFG_RESOURCE_ENV_TYPE_PFX, type_pfx,
                                  &type_pfx2, &type_pfx_size );

    if ( type_pfx_len < 0 ) {
      ok = false;
      goto cleanup;
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

  LCFGResourceList ** resources = comp->resources;

  unsigned long i;
  for ( i=0; i < comp->buckets; i++ ) {

    const LCFGResourceList * list = resources[i];
    if ( lcfgreslist_is_empty(list) ) continue;

    const LCFGResource * res = lcfgreslist_first_resource(list);

    if ( all_values || lcfgresource_has_value(res) ) {

      ssize_t rc = lcfgresource_to_export( res, NULL,
                                           val_pfx2, type_pfx2,
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
        if ( LCFGChangeError(add_rc) )
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

      char * reslist_key = NULL;
      size_t reslist_key_size = 0;

      ssize_t reslist_key_len =
        lcfgresource_build_env_var( LCFG_RESOURCE_ENV_LISTKEY,
                                    comp_name,
                                    LCFG_RESOURCE_ENV_VAL_PFX,
                                    val_pfx2,
                                    &reslist_key, &reslist_key_size );

      if ( reslist_key_len < 0 ) {
        free(reslist_key);
        ok = false;
        goto cleanup;
      }

      int rc = fprintf( out, "export %s='%s'\n", reslist_key, buffer );
      if ( rc < 0 )
        ok = false;

      free(reslist_key);
    }

  }

  free(buffer);
  lcfgtaglist_relinquish(export_res);

 cleanup:

  free(val_pfx2);
  free(type_pfx2);

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/* Checks for a resource with the required name, if none is already
   stored in the hash then the resource will be created and inserted. */

static LCFGChange lcfgcomponent_find_or_create_resource( LCFGComponent * comp,
                                                         const char * name,
                                                         LCFGResource ** result,
                                                         char ** msg ) {
  assert( comp != NULL );
  assert( name != NULL );

  LCFGChange change = LCFG_CHANGE_NONE;

  const LCFGResource * resource = lcfgcomponent_find_resource( comp, name );

  if ( resource != NULL ) {
    *result = (LCFGResource *) resource;
  } else {
    /* If not found then create new resource and add it to the comp */

    LCFGResource * new_res = lcfgresource_new();

    /* Setting name can fail if it is invalid */

    char * new_name = strdup(name);
    bool ok = lcfgresource_set_name( new_res, new_name );

    if ( !ok ) {
      change = LCFG_CHANGE_ERROR;
      lcfgutils_build_message( msg, "Failed to create new resource named '%s'",
                               new_name );
      free(new_name);
    } else {

      change = lcfgcomponent_merge_resource( comp, new_res, msg );

      if ( LCFGChangeOK(change) )
        *result = new_res;
      else
        *result = NULL;

    }

    lcfgresource_relinquish(new_res);
  }

  return change;
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

  bool ignore_meta = ! ( options & LCFG_OPT_USE_META );

  *result = NULL;

  LCFGComponent * comp = NULL;
  bool ok = true;

  /* Need a copy of the component name to insert into the
     LCFGComponent struct */

  char * comp_name = NULL;
  if ( compname_in != NULL ) {
    comp_name = strdup(compname_in);
  } else {
    if ( filename != NULL ) {
      comp_name = lcfgutils_basename( filename, NULL );
    } else {
      ok = false;
      lcfgutils_build_message( msg, "Either the component name or status file path MUST be specified" );
      goto cleanup;
    }
  }

  /* Create the new empty component which will eventually be returned */

  comp = lcfgcomponent_new();
  if ( !lcfgcomponent_set_name( comp, comp_name ) ) {
    ok = false;
    lcfgutils_build_message( msg, "Invalid name for component '%s'", comp_name );

    free(comp_name);

    goto cleanup;
  }

  const char * statusfile = filename != NULL ? filename : comp_name;

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

  LCFGResource * recent = NULL;

  const char * this_hostname = NULL;
  const char * this_compname = NULL;
  const char * this_resname  = NULL;
  const char * status_value  = NULL;
  char this_type             = LCFG_RESOURCE_SYMBOL_VALUE;

  int linenum = 1;
  while( getline( &statusline, &line_len, fp ) != -1 ) {

    lcfgutils_string_chomp(statusline);

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

    if ( ignore_meta &&
         this_type != LCFG_RESOURCE_SYMBOL_VALUE &&
         this_type != LCFG_RESOURCE_SYMBOL_TYPE ) {
      goto next_line;
    }

    /* Insist on the component names matching */

    if ( this_compname != NULL && 
         ( !lcfgcomponent_valid_name( this_compname) ||
           strcmp( this_compname, comp_name ) != 0 ) ) {
      lcfgutils_build_message( msg, "Failed to parse line %d (invalid component name '%s')",
                               linenum, this_compname );
      ok = false;
      break;
    }

    /* Grab the resource or create a new one if necessary */

    LCFGResource * res = NULL;
    if ( recent != NULL && lcfgresource_match( recent, this_resname ) ) {
      res = recent;
    } else {
      LCFGChange find_rc =
        lcfgcomponent_find_or_create_resource( comp, this_resname, &res, msg );

      if ( res == NULL || LCFGChangeError(find_rc) ) {
        lcfgutils_build_message( msg,
                                 "Failed to parse line %d of status file '%s'",
                                 linenum, statusfile );
        ok = false;
        break;
      }
    }

    /* Apply the action which matches with the symbol at the start of
       the status line or assume this is a simple specification of the
       resource value. */

    char * set_msg = NULL;
    size_t val_len = strlen(status_value);
    ok = lcfgresource_set_attribute( res, this_type,
				     status_value, val_len,
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

    recent = res;

  next_line:

    linenum++;
  }

  free(statusline);

  fclose(fp);

 cleanup:

  if ( !ok ) {
    if ( comp == NULL ) {
      free(comp_name);
    } else {
      lcfgcomponent_relinquish(comp);
      comp = NULL;
    }
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

  if ( !lcfgcomponent_is_valid(comp) ) return LCFG_STATUS_ERROR;
  if ( lcfgcomponent_is_empty(comp) )  return LCFG_STATUS_OK;

  bool all_values = (options&LCFG_OPT_ALL_VALUES);

  LCFGStatus status = LCFG_STATUS_OK;

  const char * comp_name = lcfgcomponent_get_name(comp);

  /* For efficiency the prefixes are expanded to include the component name */

  char * val_pfx2  = NULL;
  char * type_pfx2 = NULL;

  size_t val_pfx_size = 0;
  ssize_t val_pfx_len =
    lcfgresource_build_env_var( NULL, comp_name,
                                LCFG_RESOURCE_ENV_VAL_PFX, val_pfx,
                                &val_pfx2, &val_pfx_size );

  if ( val_pfx_len < 0 ) {
    lcfgutils_build_message( msg,
                             "Failed to build environment variable prefix" );
    status = LCFG_STATUS_ERROR;
    goto cleanup;
  }

  /* No point doing this if the type data isn't required */
  if ( options&LCFG_OPT_USE_META ) {
    size_t type_pfx_size = 0;
    ssize_t type_pfx_len =
      lcfgresource_build_env_var( NULL, comp_name,
                                  LCFG_RESOURCE_ENV_TYPE_PFX, type_pfx,
                                  &type_pfx2, &type_pfx_size );

    if ( type_pfx_len < 0 ) {
      lcfgutils_build_message( msg,
                               "Failed to build environment variable prefix" );
      status = LCFG_STATUS_ERROR;
      goto cleanup;
    }

  }

  LCFGTagList * export_res = lcfgtaglist_new();

  LCFGResourceList ** resources = comp->resources;

  unsigned long i;
  for ( i=0; i < comp->buckets; i++ ) {

    const LCFGResourceList * list = resources[i];
    if ( lcfgreslist_is_empty(list) ) continue;

    const LCFGResource * res = lcfgreslist_first_resource(list);

    if ( all_values || lcfgresource_has_value(res) ) {

      status = lcfgresource_to_env( res, NULL, val_pfx2, type_pfx2, options );

      if ( status == LCFG_STATUS_ERROR ) {
        *msg = lcfgresource_build_message( res, comp_name,
                           "Failed to set environment variable for resource" );
      } else {
        const char * res_name = lcfgresource_get_name(res);

        char * add_msg = NULL;
        LCFGChange add_rc = lcfgtaglist_mutate_add( export_res, res_name,
                                                    &add_msg );
        if ( LCFGChangeError(add_rc) )
          status = LCFG_STATUS_ERROR;

        free(add_msg);
      }
    }

  }

  if ( status != LCFG_STATUS_ERROR ) {

    /* Also create an environment variable which holds list of
       resource names for this component. */

    char * reslist_key = NULL;
    size_t reslist_key_size = 0;

    ssize_t reslist_key_len =
      lcfgresource_build_env_var( LCFG_RESOURCE_ENV_LISTKEY,
                                  NULL,
                                  LCFG_RESOURCE_ENV_VAL_PFX,
                                  val_pfx2,
                                  &reslist_key, &reslist_key_size );
    if ( reslist_key_len < 0 ) {
      free(reslist_key);
      lcfgutils_build_message( msg,
                               "Failed to build environment variable prefix" );
      status = LCFG_STATUS_ERROR;
      goto cleanup;
    }

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

 cleanup:

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

LCFGChange lcfgcomponent_to_status_file( const LCFGComponent * comp,
					 const char * filename,
					 LCFGOption options,
					 char ** msg ) {
  assert( comp != NULL );

  const char * comp_name = lcfgcomponent_get_name(comp);

  if ( filename == NULL && comp_name == NULL ) {
    lcfgutils_build_message( msg, "Either the target file name or component name is required" );
    return LCFG_CHANGE_ERROR;
  }

  const char * statusfile = filename != NULL ? filename : comp_name;

  LCFGChange change = LCFG_CHANGE_NONE;

  char * tmpfile = NULL;
  FILE * tmpfh = lcfgutils_safe_tmpfile( statusfile, &tmpfile );

  if ( tmpfh == NULL ) {
    change = LCFG_CHANGE_ERROR;
    lcfgutils_build_message( msg, "Failed to open status file" );
    goto cleanup;
  }

  bool print_ok = lcfgcomponent_print( comp, LCFG_RESOURCE_STYLE_STATUS,
                                       options, tmpfh );

  if (!print_ok) {
    change = LCFG_CHANGE_ERROR;
    lcfgutils_build_message( msg, "Failed to write to status file" );
  }

  /* Always attempt to close temporary file */

  if ( fclose(tmpfh) != 0 ) {
    change = LCFG_CHANGE_ERROR;
    lcfgutils_build_message( msg, "Failed to close status file" );
  }

  if ( LCFGChangeOK(change) )
    change = lcfgutils_file_update( filename, tmpfile, 0 );

 cleanup:

  /* This might have already gone but call unlink to ensure
     tidiness. Do not care about the result */

  if ( tmpfile != NULL ) {
    (void) unlink(tmpfile);
    free(tmpfile);
  }

  return change;
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
 * If a @c NULL value is specified for the component or the component
 * is empty then a @c NULL value will be returned.
 *
 * @param[in] comp Pointer to @c LCFGComponent to be searched
 * @param[in] want_name The name of the required resource
 *
 * @return Pointer to an @c LCFGResource (or the @c NULL value).
 *
 */

const LCFGResource * lcfgcomponent_find_resource( const LCFGComponent * comp,
                                                  const char * want_name ) {
  assert( want_name != NULL );

  const LCFGResourceList * list = lcfgcomponent_find_list( comp, want_name );

  const LCFGResource * result = NULL;
  if ( !lcfgreslist_is_empty(list) )
    result = lcfgreslist_first_resource( list );

  return result;
}

/**
 * @brief Check if a component contains a particular resource
 *
 * This can be used to search through an @c LCFGComponent to check if
 * it contains a resource with a matching name. Note that the matching
 * is done using strcmp(3) which is case-sensitive.
 * 
 * If a @c NULL value is specified for the component or the component
 * is empty then a false value will be returned.
 *
 * @param[in] comp Pointer to @c LCFGComponent to be searched (may be @c NULL)
 * @param[in] want_name The name of the required resource
 *
 * @return Boolean value which indicates presence of resource in component
 *
 */

bool lcfgcomponent_has_resource( const LCFGComponent * comp,
                                 const char * want_name ) {
  assert( want_name != NULL );

  return ( lcfgcomponent_find_resource( comp, want_name ) != NULL );
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
 * @param[in] resource Pointer to @c LCFGResource
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcomponent_merge_resource( LCFGComponent * comp,
                                         LCFGResource * resource,
                                         char ** msg ) {
  assert( comp != NULL );

  if ( !lcfgresource_is_valid(resource) ) return LCFG_CHANGE_ERROR;

  const char * name = lcfgresource_get_name(resource);

  unsigned long bucket;
  bool found = lcfgcomponent_find_bucket( comp, name, &bucket );

  LCFGChange change = LCFG_CHANGE_NONE;
  if ( !found ) {
    lcfgutils_build_message( msg,
                             "No free space for new entries in component" );
    change = LCFG_CHANGE_ERROR;
  } else {

    /* find the existing list or create a new one */

    LCFGResourceList * list = (comp->resources)[bucket];
    LCFGResourceList * new_list = NULL;
    if ( list == NULL ) {
      new_list = lcfgcomponent_empty_list(comp);
      list = new_list;
    } else if ( lcfgreslist_is_shared(list) ) { /* COW */
      new_list = lcfgreslist_clone(list);
      list = new_list;
    }

    change = lcfgreslist_merge_resource( list, resource, msg );

    if ( LCFGChangeOK(change) ) {
      LCFGChange set_rc = LCFG_CHANGE_NONE;

      if ( lcfgreslist_is_empty(list) ) { /* remove empty list */
        set_rc = lcfgcomponent_set_bucket( comp, bucket, NULL );
      } else if ( change != LCFG_CHANGE_NONE ) {
        set_rc = lcfgcomponent_set_bucket( comp, bucket, list );

        if ( set_rc == LCFG_CHANGE_ADDED )
          lcfgcomponent_resize(comp);
      }
      if ( LCFGChangeError(set_rc) ) change = set_rc;
    }

    lcfgreslist_relinquish(new_list);
      
  }

  return change;
}

/**
 * @brief Merge overrides from one component to another
 *
 * Iterates through the list of resources in the overrides @c
 * LCFGComponent and merges them to the target component by calling
 * @c lcfgcomponent_merge_resource().
 *
 * @param[in] comp1 Pointer to @c LCFGComponent
 * @param[in] comp2 Pointer to override @c LCFGComponent
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Integer value indicating type of change
 *
 */

LCFGChange lcfgcomponent_merge_component( LCFGComponent * comp1,
                                          const LCFGComponent * comp2,
                                          char ** msg ) {

  assert( comp1 != NULL );

  if ( lcfgcomponent_is_empty(comp2) ) return LCFG_CHANGE_NONE;

  LCFGChange change = LCFG_CHANGE_NONE;

  unsigned long i;
  for ( i=0; i<comp2->buckets && LCFGChangeOK(change); i++ ) {

    const LCFGResourceList * list2 = (comp2->resources)[i];
    if ( lcfgreslist_is_empty(list2) ) continue;

    const char * name = lcfgreslist_get_name(list2);

    unsigned long bucket;
    bool found = lcfgcomponent_find_bucket( comp1, name, &bucket );

    if ( !found ) {
      lcfgutils_build_message( msg,
                               "No free space for new entries in component" );
      change = LCFG_CHANGE_ERROR;
    } else {
      LCFGResourceList * list1 = (comp1->resources)[bucket];

      LCFGResourceList * new_list = NULL;
      if ( list1 == NULL ) {
        new_list = lcfgcomponent_empty_list(comp1);
        list1 = new_list;
      } else if ( lcfgreslist_is_shared(list1) ) { /* COW */
        new_list = lcfgreslist_clone(list1);
        list1 = new_list;
      }

      change = lcfgreslist_merge_list( list1, list2, msg );

      if ( LCFGChangeOK(change) ) {
        LCFGChange set_rc = LCFG_CHANGE_NONE;

        if ( lcfgreslist_is_empty(list1) ) { /* remove empty list */
          set_rc = lcfgcomponent_set_bucket( comp1, bucket, NULL );
        } else if ( change != LCFG_CHANGE_NONE ) {
          set_rc = lcfgcomponent_set_bucket( comp1, bucket, list1 );

          if ( set_rc == LCFG_CHANGE_ADDED )
            lcfgcomponent_resize(comp1);
        }
        if ( LCFGChangeError(set_rc) ) change = set_rc;
      }

      lcfgreslist_relinquish(new_list);

    }

  }

  return change;
}

/**
 * @brief Get the list of resource names as a taglist
 *
 * This generates a new @c LCFGTagList which contains a list of
 * resource names for the @c LCFGComponent.
 *
 * If the component is empty then an empty tag list will be
 * returned. Will return @c NULL if an error occurs.
 *
 * To avoid memory leaks, when the list is no longer required the 
 * @c lcfgtaglist_relinquish() function should be called.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 *
 * @return Pointer to a new @c LCFGTagList of resource names
 *
 */

LCFGTagList * lcfgcomponent_get_resources_as_taglist(const LCFGComponent * comp) {

  bool ok = true;
  LCFGTagList * reslist = lcfgtaglist_new();

  if ( !lcfgcomponent_is_empty(comp) ) {

    LCFGResourceList ** resources = comp->resources;

    unsigned long i;
    for ( i=0; i < comp->buckets; i++ ) {

      const LCFGResourceList * list = resources[i];
      if ( lcfgreslist_is_empty(list) ) continue;

      const char * res_name = lcfgreslist_get_name(list);

      char * msg = NULL;
      LCFGChange change = lcfgtaglist_mutate_add( reslist, res_name, &msg );
      if ( LCFGChangeError(change) )
        ok = false;

      free(msg); /* Just ignoring any message */
    }
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

  LCFGTagList * reslist = lcfgcomponent_get_resources_as_taglist(comp);

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

  /* Declare variables here which need to be defined before a jump to
     the cleanup stage */

  LCFGStatus status = LCFG_STATUS_OK;
  LCFGComponent * comp = NULL;
  LCFGTagList * import_res = NULL;

  char * val_pfx2  = NULL;
  char * type_pfx2 = NULL;
  ssize_t key_len = 0;
  size_t key_size = 0;

  key_len = lcfgresource_build_env_var( NULL, compname_in,
                                        LCFG_RESOURCE_ENV_VAL_PFX,
                                        val_pfx,
                                        &val_pfx2, &key_size );

  if ( key_len < 0 ) {
    status = LCFG_STATUS_ERROR;
    lcfgutils_build_message( msg, "Failed to build environment variable name" );
    goto cleanup;
  }

  key_len = lcfgresource_build_env_var( NULL, compname_in,
                                        LCFG_RESOURCE_ENV_TYPE_PFX,
                                        type_pfx,
                                        &type_pfx2, &key_size );

  if ( key_len < 0 ) {
    status = LCFG_STATUS_ERROR;
    lcfgutils_build_message( msg, "Failed to build environment variable name" );
    goto cleanup;
  }

  /* Find the list of resource names for the component */

  char * reslist_key = NULL;
  key_len = lcfgresource_build_env_var( LCFG_RESOURCE_ENV_LISTKEY,
                                        compname_in,
                                        LCFG_RESOURCE_ENV_VAL_PFX,
                                        val_pfx2,
                                        &reslist_key, &key_size );

  if ( key_len < 0 ) {
    status = LCFG_STATUS_ERROR;
    lcfgutils_build_message( msg, "Failed to build environment variable name" );
    goto cleanup;
  }

  const char * reslist_value = getenv(reslist_key);

  if ( !isempty(reslist_value) ) {

    status = lcfgtaglist_from_string( reslist_value, &import_res, msg );
    if ( status ==  LCFG_STATUS_ERROR )
      goto cleanup;

    /* Nothing more to do if the list of resources to be imported is empty */
    if ( lcfgtaglist_is_empty(import_res) )
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
    status = lcfgresource_from_env( resname, NULL, val_pfx2, type_pfx2, 
                                    &res, LCFG_OPT_NONE, msg );

    if ( status != LCFG_STATUS_ERROR ) {

      char * merge_msg = NULL;

      LCFGChange change = lcfgcomponent_merge_resource( comp, res, &merge_msg );
      if ( LCFGChangeError(change) ) {
        lcfgutils_build_message( msg, "Failed to import resource '%s': %s",
                                 resname, merge_msg );
        status = LCFG_STATUS_ERROR;
      }

      free(merge_msg);
    }

    lcfgresource_relinquish(res);
  }

  lcfgtagiter_destroy(tagiter);

 cleanup:

  free(reslist_key);
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
  assert( comp->name != NULL );

  size_t len = strlen(comp->name);
  return farmhash64( comp->name, len );
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

/**
 * @brief Compare names of two components
 *
 * This uses @c strcmp(3) to compare the values of the @e name
 * attribute for the two @c LCFGComponent. For simplicity, if the
 * value of the @e name attribute for a component is @c NULL then it
 * is treated as an empty string.
 *
 * @param[in] comp1 Pointer to @c LCFGComponent
 * @param[in] comp2 Pointer to @c LCFGComponent
 *
 * @return boolean which indicates if the names are the same
 *
 */

bool lcfgcomponent_same_name( const LCFGComponent * comp1,
                              const LCFGComponent * comp2 ) {
  assert( comp1 != NULL );
  assert( comp2 != NULL );

  const char * name1 = or_default( comp1->name, "" );
  const char * name2 = or_default( comp2->name, "" );

  return ( strcmp( name1, name2 ) == 0 );
}

/**
 * Select a subset of resources from a component
 *
 * This can be used to select a subset of resources from an @c
 * LCFGComponent and return them as a new @c LCFGComponent. The @e
 * name and the merge rules will be copied. Note that this does a
 * shallow copy of any resources so they are shared - modifying
 * them for the original component will affect the subset component
 * and vice-versa.
 *
 * By default if a resource is not found for any name in the specified
 * list this function will return an error. The @c
 * LCFG_OPT_ALLOW_NOEXIST option can be specified to silently ignore
 * this problem.
 *
 * @param[in] comp Pointer to @c LCFGComponent
 * @param[in] res_wanted An @c LCFGTagList of resource names
 * @param[out] result Reference to pointer to subset @c LCFGComponent
 * @param[in] options Integer which controls behaviour
 * @param[out] msg Pointer to any diagnostic messages
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgcomponent_select( const LCFGComponent * comp,
                                 LCFGTagList * res_wanted,
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
      goto cleanup;
    }
  }

  /* Also clone the merge rules */

  new_comp->primary_key = comp->primary_key;
  new_comp->merge_rules = comp->merge_rules;

  /* Preallocate a sufficiently large hash */

  unsigned long taglist_size = lcfgtaglist_size(res_wanted);
  unsigned long new_buckets = want_buckets(taglist_size);
  if ( new_buckets > new_comp->buckets ) {
    new_comp->buckets = new_buckets;
    lcfgcomponent_resize(new_comp);
  }

  /* Collect the required subset of resources */

  bool allow_noexist = ( options & LCFG_OPT_ALLOW_NOEXIST );

  LCFGTagIterator * tagiter = lcfgtagiter_new(res_wanted);
  const LCFGTag * restag = NULL;

  while ( status != LCFG_STATUS_ERROR &&
          ( restag = lcfgtagiter_next(tagiter) ) != NULL ) {

    const char * resname = lcfgtag_get_name(restag);
    if ( !lcfgresource_valid_name(resname) ) {
      lcfgutils_build_message( msg, "Invalid resource name '%s'", resname );
      status = LCFG_STATUS_ERROR;
      break;
    }

    LCFGResourceList * list =
      (LCFGResourceList * ) lcfgcomponent_find_list( comp, resname );

    if ( !lcfgreslist_is_empty(list) ) {

      LCFGChange insert_rc = lcfgcomponent_insert_list( new_comp, list );

      if ( LCFGChangeError(insert_rc) ) {
        lcfgutils_build_message( msg, "Failed to select resource named '%s'",
                                 resname );
        status = LCFG_STATUS_ERROR;
      } else if ( insert_rc == LCFG_CHANGE_ADDED ) {

        /* Should not be strictly necessary since we pre-allocated a
           hash which should be a suitable size but being
           careful... This will return immediately if space does not
           need to be adjusted */

        lcfgcomponent_resize(new_comp);
      }

    } else if ( !allow_noexist ) {
      lcfgutils_build_message( msg, "%s resource does not exist", resname );
      status = LCFG_STATUS_ERROR;
    }

  }

  lcfgtagiter_destroy(tagiter);

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
    lcfgcomponent_find_resource( comp, "ng_schema" );

  return ( ng_schema != NULL && lcfgresource_has_value(ng_schema) );
}


bool lcfgcomponent_update_signature( const LCFGComponent * comp,
                                     md5_state_t * md5state,
                                     char ** buffer, size_t * buf_size ) {
  assert( comp != NULL );

  LCFGComponentEntry * entries = NULL;
  unsigned int count = lcfgcomponent_sorted_entries( comp, &entries );

  const char * comp_name = lcfgcomponent_get_name(comp);
  LCFGResourceList ** resources = comp->resources;

  bool ok = true;

  unsigned long i;
  for ( i=0; i<count && ok; i++ ) {

    unsigned int bucket = entries[i].bucket;
    const LCFGResourceList * list = resources[bucket];

    if ( !lcfgreslist_is_empty(list) ) {

      const LCFGResource * res = lcfgreslist_first_resource(list);

      ssize_t rc = lcfgresource_to_status( res, comp_name, LCFG_OPT_USE_META,
                                           buffer, buf_size );

      if ( rc > 0 )
        lcfgutils_md5_append( md5state, (const md5_byte_t *) *buffer, rc );
      else
        ok = false;

    }
  }

  free(entries);

  return ok;
}

/* eof */
