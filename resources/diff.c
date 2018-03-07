/**
 * @file resources/diff.c
 * @brief Functions for finding the differences between LCFG resources
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "differences.h"

/**
 * @brief Create and initialise a new resource diff
 *
 * Creates a new @c LCFGDiffResource and initialises the parameters to
 * the default values.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * The reference count for the structure is initialised to 1. To avoid
 * memory leaks, when it is no longer required the
 * @c lcfgdiffresource_relinquish() function should be called.
 *
 * @return Pointer to new @c LCFGDiffResource
 *
 */

LCFGDiffResource * lcfgdiffresource_new(void) {

  LCFGDiffResource * resdiff = malloc(sizeof(LCFGDiffResource) );
  if ( resdiff == NULL ) {
    perror( "Failed to allocate memory for LCFG resource diff" );
    exit(EXIT_FAILURE);
  }

  resdiff->old  = NULL;
  resdiff->new  = NULL;
  resdiff->_refcount = 1;

  return resdiff;
}

/**
 * @brief Destroy the resource diff
 *
 * When the specified @c LCFGResourceDiff is no longer required this
 * will free all associated memory. There is support for reference
 * counting so typically the @c lcfgdiffresource_relinquish() function
 * should be used.
 *
 * This will call @c lcfgresource_relinquish() for the old and new @c
 * LCFGResource structures. If the reference count on a resource drops
 * to zero it will also be destroyed
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * resource diff which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] resdiff Pointer to @c LCFGDiffResource to be destroyed.
 *
 */

void lcfgdiffresource_destroy(LCFGDiffResource * resdiff) {

  if ( resdiff == NULL ) return;

  lcfgresource_relinquish(resdiff->old);
  resdiff->old = NULL;

  lcfgresource_relinquish(resdiff->new);
  resdiff->new = NULL;

  free(resdiff);
  resdiff = NULL;

}

/**
 * @brief Acquire reference to resource diff
 *
 * This is used to record a reference to the @c LCFGDiffResource, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the structure is no
 * longer required the @c lcfgdiffresource_relinquish() function
 * should be called.
 *
 * @param[in] resdiff Pointer to @c LCFGDiffResource
 *
 */

void lcfgdiffresource_acquire( LCFGDiffResource * resdiff ) {
  assert( resdiff != NULL );

  resdiff->_refcount += 1;
}

/**
 * @brief Release reference to resource diff
 *
 * This is used to release a reference to the @c LCFGDiffResource, it
 * does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgdiffresource_destroy()
 * function will be called to clean up the memory associated with the
 * structure.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * resource diff which has already been destroyed (or potentially was
 * never created).
 *
 * @param[in] resdiff Pointer to @c LCFGDiffResource
 *
 */

void lcfgdiffresource_relinquish( LCFGDiffResource * resdiff ) {

  if ( resdiff == NULL ) return;

  if ( resdiff->_refcount > 0 )
    resdiff->_refcount -= 1;

  if ( resdiff->_refcount == 0 )
    lcfgdiffresource_destroy(resdiff);

}

/**
 * @brief Check if the diff has an old resource
 *
 * Checks if the specified @c LCFGResource currently contains an @e
 * old @c LCFGResource. If the diff represents a newly added resource
 * then the @e old @c LCFGResource will be @c NULL and this will
 * return a false value.
 *
 * @param[in] resdiff Pointer to an @c LCFGResource
 *
 * @return boolean which indicates if diff has old resource
 *
 */

bool lcfgdiffresource_has_old( const LCFGDiffResource * resdiff ) {
  assert( resdiff != NULL );

  return ( resdiff->old != NULL );
}

/**
 * @brief Get the old resource (if any)
 *
 * This returns the @e old @c LCFGResource for the @c
 * LCFGDiffResource. Note that if the diff represents a newly added
 * resource then this will return a @c NULL value.
 *
 * @param[in] resdiff Pointer to an @c LCFGDiffResource
 *
 * @return Pointer to the old resource for the diff (possibly @c NULL).
 *
 */

LCFGResource * lcfgdiffresource_get_old( const LCFGDiffResource * resdiff ) {
  assert( resdiff != NULL );

  return resdiff->old;
}

/**
 * @brief Set the old resource for the diff
 *
 * Sets the @e old @c LCFGResource for the @c LCFGDiffResource. If the
 * diff represents a newly added resource then this should be set to
 * the @c NULL value.
 *
 * @param[in] resdiff Pointer to an @c LCFGDiffResource
 * @param[in] res Pointer to old @c LCFGResource 
 *
 * @return boolean indicating success
 *
 */

bool lcfgdiffresource_set_old( LCFGDiffResource * resdiff,
                               LCFGResource * res ) {
  assert( resdiff != NULL );

  LCFGResource * current = resdiff->old;
  lcfgresource_relinquish(current);

  if ( res != NULL )
    lcfgresource_acquire(res);

  resdiff->old = res;

  return true;
}

/**
 * @brief Check if the diff has a new resource
 *
 * Checks if the specified @c LCFGResource currently contains a @e new
 * @c LCFGResource. If the diff represents a removed resource then the
 * @e new @c LCFGResource will be @c NULL and this will return a false
 * value.
 *
 * @param[in] resdiff Pointer to an @c LCFGDiffResource
 *
 * @return boolean which indicates if diff has new resource
 *
 */

bool lcfgdiffresource_has_new( const LCFGDiffResource * resdiff ) {
  assert( resdiff != NULL );

  return ( resdiff->new != NULL );
}

/**
 * @brief Get the new resource (if any)
 *
 * This returns the @e new @c LCFGResource for the @c
 * LCFGDiffResource. Note that if the diff represents a removed
 * resource then this will return a @c NULL value.
 *
 * @param[in] resdiff Pointer to an @c LCFGDiffResource
 *
 * @return Pointer to the new resource for the diff (possibly @c NULL).
 *
 */

LCFGResource * lcfgdiffresource_get_new( const LCFGDiffResource * resdiff ) {
  assert( resdiff != NULL );

  return resdiff->new;
}

/**
 * @brief Set the new resource for the diff
 *
 * Sets the @e new @c LCFGResource for the @c LCFGDiffResource. If the
 * diff represents a removed resource then this should be set to the
 * @c NULL value.
 *
 * @param[in] resdiff Pointer to an @c LCFGDiffResource
 * @param[in] res Pointer to new @c LCFGResource 
 *
 * @return boolean indicating success
 *
 */

bool lcfgdiffresource_set_new( LCFGDiffResource * resdiff,
                               LCFGResource * res ) {
  assert( resdiff != NULL );

  LCFGResource * current = resdiff->new;
  lcfgresource_relinquish(current);

  if ( res != NULL )
    lcfgresource_acquire(res);

  resdiff->new = res;

  return true;
}

/**
 * @brief Get the name for the resource diff
 *
 * This returns the value of the @e name parameter for either of the
 * @e old and @e new @c LCFGResource. If neither resource has a name
 * or both are set to @c NULL this will return a @c NULL value.
 *
 * It is important to note that this is @b NOT a copy of the string,
 * changing the returned string will modify the @e name of the
 * resource.
 *
 * @param[in] resdiff Pointer to an @c LCFGDiffResource
 *
 * @return The @e name for the resource diff (possibly @c NULL).
 *
 */

const char * lcfgdiffresource_get_name( const LCFGDiffResource * resdiff ) {
  assert( resdiff != NULL );
  
  const LCFGResource * res = NULL;
  
  /* Check if there is an old resource with a name */
  if ( lcfgdiffresource_has_old(resdiff) )
    res = lcfgdiffresource_get_old(resdiff);

  if ( !lcfgresource_is_valid(res) )
    res = lcfgdiffresource_get_new(resdiff);

  const char * name = NULL;
  if ( lcfgresource_is_valid(res) )
    name = lcfgresource_get_name(res);

  return name;
}

/**
 * @brief Get the resource diff type
 *
 * This function can be used to compare the @e old and @e new @c
 * LCFGResource for the @c LCFGDiffResource. It will return one of the
 * following values:
 *
 *   - LCFG_CHANGE_NONE     - resources are same
 *   - LCFG_CHANGE_ADDED    - resource is new, did not previously exist
 *   - LCFG_CHANGE_REMOVED  - resource has been removed, no longer exists
 *   - LCFG_CHANGE_MODIFIED - resources have different values
 *
 * It is important to note that when both resources are available they
 * are only compared using the @e value. For example, changes in
 * derivation or type data are not considered to be significant.
 *
 * @param[in] resdiff Pointer to an @c LCFGDiffResource
 *
 * @return Integer representing the type of resource difference
 *
 */

LCFGChange lcfgdiffresource_get_type( const LCFGDiffResource * resdiff ) {
  assert( resdiff != NULL );

  LCFGChange difftype = LCFG_CHANGE_NONE;

  if ( lcfgdiffresource_has_old(resdiff) ) {

    if ( lcfgdiffresource_has_new(resdiff) ) {

      const LCFGResource * old = lcfgdiffresource_get_old(resdiff);
      const LCFGResource * new = lcfgdiffresource_get_new(resdiff);

      if ( !lcfgresource_same_value( old, new ) )
        difftype = LCFG_CHANGE_MODIFIED;

    } else {
      difftype = LCFG_CHANGE_REMOVED;
    }

  } else {

    if ( lcfgdiffresource_has_new(resdiff) )
      difftype = LCFG_CHANGE_ADDED;

  }

  return difftype;
}

/**
 * @brief Check if the diff represents any change
 *
 * This will return true if the @c LCFGDiffResource represents the
 * addition, removal or modification of a resource.
 *
 * @param[in] resdiff Pointer to an @c LCFGDiffResource
 *
 * @return Boolean which indicates if diff represents any change
 *
 */

bool lcfgdiffresource_is_changed( const LCFGDiffResource * resdiff ) {
  assert( resdiff != NULL );

  LCFGChange difftype = lcfgdiffresource_get_type(resdiff);
  return ( difftype == LCFG_CHANGE_ADDED   ||
	   difftype == LCFG_CHANGE_REMOVED ||
	   difftype == LCFG_CHANGE_MODIFIED );
}

/**
 * @brief Check if the diff does not represent a change
 *
 * This will return true if the @c LCFGDiffResource does not represent
 * any type of change. The @e old and @e new @c LCFGResource are both
 * present and there are no differences.
 *
 * @param[in] resdiff Pointer to an @c LCFGDiffResource
 *
 * @return Boolean which indicates if diff represent no change
 *
 */

bool lcfgdiffresource_is_nochange( const LCFGDiffResource * resdiff ) {
  assert( resdiff != NULL );

  return ( lcfgdiffresource_get_type(resdiff) == LCFG_CHANGE_NONE );
}

/**
 * @brief Check if the diff represents a modified value
 *
 * This will return true if the @c LCFGDiffResource represents a
 * difference of value for the @e old and @e new @c LCFGResource.
 *
 * @param[in] resdiff Pointer to an @c LCFGDiffResource
 *
 * @return Boolean which indicates if diff represents changed value
 *
 */

bool lcfgdiffresource_is_modified( const LCFGDiffResource * resdiff ) {
  assert( resdiff != NULL );

  return ( lcfgdiffresource_get_type(resdiff) == LCFG_CHANGE_MODIFIED );
}

/**
 * @brief Check if the diff represents a new resource
 *
 * This will return true if the @c LCFGDiffResource represents a newly
 * added @c LCFGResource (i.e. the @e old @c LCFGResource is @c NULL).
 *
 * @param[in] resdiff Pointer to an @c LCFGDiffResource
 *
 * @return Boolean which indicates if diff represents new resource
 *
 */

bool lcfgdiffresource_is_added( const LCFGDiffResource * resdiff ) {
  assert( resdiff != NULL );

  return ( lcfgdiffresource_get_type(resdiff) == LCFG_CHANGE_ADDED );
}

/**
 * @brief Check if the diff represents a removed resource
 *
 * This will return true if the @c LCFGDiffResource represents removed
 * @c LCFGResource (i.e. the @e new @c LCFGResource is @c NULL).
 *
 * @param[in] resdiff Pointer to an @c LCFGDiffResource
 *
 * @return Boolean which indicates if diff represents removed resource
 *
 */

bool lcfgdiffresource_is_removed( const LCFGDiffResource * resdiff ) {
  assert( resdiff != NULL );

  return ( lcfgdiffresource_get_type(resdiff) == LCFG_CHANGE_REMOVED );
}

/**
 * @brief Format the resource diff as a string
 *
 * This can be used to summarise the @c LCFGDiffResource as a
 * string. Mostly this is useful for generating log messages of
 * changes when importing new profiles.
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many strings, this can be a huge
 * performance benefit. If the buffer is initially unallocated then it
 * MUST be set to @c NULL. The current size of the buffer must be
 * passed and should be specified as zero if the buffer is initially
 * unallocated. If the generated string would be too long for the
 * current buffer then it will be resized and the size parameter is
 * updated.
 *
 * If the string is successfully generated then the length of the new
 * string is returned, note that this is distinct from the buffer
 * size. To avoid memory leaks, call @c free(3) on the buffer when no
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] resdiff Pointer to @c LCFGDiffResource
 * @param[in] prefix Prefix, usually the component name (may be @c NULL)
 * @param[in] pending Boolean which indicates whether this is a 'pending' change
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgdiffresource_to_string( const LCFGDiffResource * resdiff,
				    const char * prefix,
				    bool pending,
				    char ** result, size_t * size ) {
  assert( resdiff != NULL );

  size_t new_len = 0;

  const char * type;
  size_t type_len = 0;

  LCFGChange difftype = lcfgdiffresource_get_type(resdiff);
  switch(difftype)
    {
    case LCFG_CHANGE_ADDED:
      type     = "added";
      type_len = 5;
      break;
    case LCFG_CHANGE_REMOVED:
      type     = "removed";
      type_len = 7;
      break;
    case LCFG_CHANGE_MODIFIED:
      type     = "modified";
      type_len = 8;
      break;
    case LCFG_CHANGE_NONE:
    default:
      type     = "nochange";
      type_len = 8;
      break;
  }

  new_len += type_len;

  /* base of message */

  static const char base[] = "resource";
  size_t base_len = sizeof(base) - 1;

  new_len += ( base_len + 1 ); /* + 1 for ' ' (space) separator */

  /* If the changes are being held we mark them as "pending" */

  if (pending)
    new_len += 8; /* " pending" including ' ' (space) separator */

  /* The prefix is typically the component name */

  size_t prefix_len = prefix != NULL ? strlen(prefix) : 0;
  if ( prefix_len > 0 )
    new_len += ( prefix_len + 1 ); /* + 1 for '.' separator */

  /* There is a tiny risk of neither resource having a useful name */

  const char * name = lcfgdiffresource_get_name(resdiff);
  size_t name_len = name != NULL ? strlen(name) : 0;
  new_len += ( name_len + 2 );    /* +2 for ": " separator */

  /* Comment about add of resource with empty value */

  static const char isnull_comment[] = " (null)";
  const size_t isnull_comment_len = sizeof(isnull_comment) - 1;

  bool add_is_null = false;
  if ( difftype == LCFG_CHANGE_ADDED ) {
    const LCFGResource * new_res = lcfgdiffresource_get_new(resdiff);
    if ( !lcfgresource_has_value(new_res) ) {
      add_is_null = true;
      new_len += isnull_comment_len;
    }
  }

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    size_t new_size = new_len + 1;

    char * new_buf = realloc( *result, ( new_size * sizeof(char) ) );
    if ( new_buf == NULL ) {
      perror("Failed to allocate memory for LCFG resource diff string");
      exit(EXIT_FAILURE);
    } else {
      *result = new_buf;
      *size   = new_size;
    }

  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  /* Build the new string */

  char * to = *result;

  to = stpncpy( to, type, type_len );

  *to = ' ';
  to++;

  to = stpncpy( to, base, base_len );

  if (pending) {
    *to = ' ';
    to++;

    to = stpncpy( to, "pending", 7 );
  }

  to = stpncpy( to, ": ", 2 );

  if ( prefix_len > 0 ) {
    to = stpncpy( to, prefix, prefix_len );

    *to = '.';
    to++;
  }

  if ( name_len > 0 )
    to = stpncpy( to, name, name_len );

  if ( add_is_null )
    to = stpncpy( to, isnull_comment, isnull_comment_len );

  *to = '\0';

  assert( ( *result + new_len ) == to );

  return new_len;

}

/**
 * @brief Format the resource diff for a @e hold file
 *
 * The LCFG client supports a @e secure mode which can be used to hold
 * back resource changes pending a manual review by the
 * administrator. To assist in the review process it produces a @e
 * hold file which contains a summary of all resource changes
 * (additions, removals and modifications of values). This function
 * can be used to serialise the resource diff in the correct way for
 * inclusion in the @e hold file.
 *
 * This function uses a string buffer which may be pre-allocated if
 * nececesary to improve efficiency. This makes it possible to reuse
 * the same buffer for generating many strings, this can be a huge
 * performance benefit. If the buffer is initially unallocated then it
 * MUST be set to @c NULL. The current size of the buffer must be
 * passed and should be specified as zero if the buffer is initially
 * unallocated. If the generated string would be too long for the
 * current buffer then it will be resized and the size parameter is
 * updated.
 *
 * If the string is successfully generated then the length of the new
 * string is returned, note that this is distinct from the buffer
 * size. To avoid memory leaks, call @c free(3) on the buffer when no
 * longer required. If an error occurs this function will return -1.
 *
 * @param[in] resdiff Pointer to @c LCFGDiffResource
 * @param[in] prefix Prefix, usually the component name (may be @c NULL)
 * @param[in,out] result Reference to the pointer to the string buffer
 * @param[in,out] size Reference to the size of the string buffer
 *
 * @return The length of the new string (or -1 for an error).
 *
 */

ssize_t lcfgdiffresource_to_hold( const LCFGDiffResource * resdiff,
                                  const char * prefix,
                                  char ** result, size_t * size ) {
  assert( resdiff != NULL );

  const char * name = lcfgdiffresource_get_name(resdiff);
  if ( name == NULL ) return -1;

  /* Get the value for the old resource or default to an empty string
     if nothing else is available. */

  const char * old_value = NULL;
  char * old_value_enc = NULL;
  if ( lcfgdiffresource_has_old(resdiff) ) {
    const LCFGResource * old_res = lcfgdiffresource_get_old(resdiff);

    if ( lcfgresource_has_value(old_res) ) {
      if ( lcfgresource_value_needs_encode(old_res) ) {
	old_value_enc = lcfgresource_enc_value(old_res);
	old_value = old_value_enc;
      } else {
	old_value = lcfgresource_get_value(old_res);
      }
    }

  }

  /* Get the value for the new resource or default to an empty string
     if nothing else is available. */

  const char * new_value = NULL;
  char * new_value_enc = NULL;
  if ( lcfgdiffresource_has_new(resdiff) ) {
    const LCFGResource * new_res = lcfgdiffresource_get_new(resdiff);

    if ( lcfgresource_has_value(new_res) ) {
      if ( lcfgresource_value_needs_encode(new_res) ) {
	new_value_enc = lcfgresource_enc_value(new_res);
	new_value = new_value_enc;
      } else {
	new_value = lcfgresource_get_value(new_res);
      }
    }

  }

  size_t prefix_len = 0;
  size_t name_len = 0;
  size_t old_val_len = 0;
  size_t new_val_len = 0;

  static const char old_marker[] = " - ";
  static const size_t old_marker_len = sizeof(old_marker) - 1;

  static const char new_marker[] = " + ";
  static const size_t new_marker_len = sizeof(new_marker) - 1;

  /* Find the required buffer size */

  /* Additions where the new resource has no value and removals where
     the old resource has no value are not worth reporting so simply
     avoid that here. */

  bool show_change = false;
  if ( isempty(old_value) ) {
    if ( !isempty(new_value) ) 
      show_change = true;
  } else {
    if ( isempty(new_value) || strcmp(old_value, new_value ) != 0 )
      show_change = true;
  }

  size_t new_len = 0;
  if (show_change) {

    if ( prefix != NULL ) {
      prefix_len = strlen(prefix);
      new_len += (  prefix_len + 1 );    /* +1 for '.' separator */
    }

    name_len = strlen(name);
    new_len += ( name_len + 2 );         /* +2 for ':' and newline */

    if ( old_value != NULL )
      old_val_len = strlen(old_value);

    new_len += ( old_val_len + old_marker_len + 1 );  /* +1 newline */

    if ( new_value != NULL )
      new_val_len = strlen(new_value);

    new_len +=  ( new_val_len + new_marker_len + 1 ); /* +1 for newline */
  }

  /* Allocate the required space */

  if ( *result == NULL || *size < ( new_len + 1 ) ) {
    size_t new_size = new_len + 1;

    char * new_buf = realloc( *result, ( new_size * sizeof(char) ) );
    if ( new_buf == NULL ) {
      perror("Failed to allocate memory for LCFG resource string");
      exit(EXIT_FAILURE);
    } else {
      *result = new_buf;
      *size   = new_size;
    }

  }

  /* Always initialise the characters of the full space to nul */
  memset( *result, '\0', *size );

  if ( new_len == 0 ) {
    free(old_value_enc);
    free(new_value_enc);
    return new_len;
  }

  char * to = *result;

  if ( prefix != NULL ) {
    to = stpncpy( to, prefix, prefix_len );
    *to = '.';
    to++;
  }

  to = stpncpy( to, name, name_len );
  to = stpncpy( to, ":\n", 2 );

  to = stpncpy( to, old_marker, old_marker_len );

  if ( old_val_len > 0 )
    to = stpncpy( to, old_value, old_val_len );

  to = stpncpy( to, "\n",  1 );

  to = stpncpy( to, new_marker, new_marker_len );

  if ( new_val_len > 0 )
    to = stpncpy( to, new_value, new_val_len );

  to = stpncpy( to, "\n",  1 );

  *to = '\0';

  free(old_value_enc);
  free(new_value_enc);

  assert( ( *result + new_len ) == to );

  return new_len;
}

/**
 * @brief Check if the resource diff has a particular name
 *
 * This can be used to check if the name for the @c LCFGDiffResource
 * matches with that specified. The name for the diff is retrieved
 * using the @c lcfgdiffresource_get_name().
 *
 * @param[in] resdiff Pointer to @c LCFGDiffResource
 * @param[in] want_name Resource name to match
 *
 * @return Boolean which indicates if diff name matches
 *
 */

bool lcfgdiffresource_match( const LCFGDiffResource * resdiff,
                             const char * want_name ) {
  assert( resdiff != NULL );
  assert( want_name != NULL );

  const char * name = lcfgdiffresource_get_name(resdiff);

  return ( !isempty(name) && strcmp( name, want_name ) == 0 );
}

/**
 * @brief Compare two resource diffs
 *
 * This compares the names for two @c LCFGDiffResource, this is mostly
 * useful for sorting lists of diffs. An integer value is returned
 * which indicates lesser than, equal to or greater than in the same
 * way as @c strcmp(3).
 *
 * @param[in] resdiff1 Pointer to @c LCFGDiffResource
 * @param[in] resdiff2 Pointer to @c LCFGDiffResource
 * 
 * @return Integer (-1,0,+1) indicating lesser,equal,greater
 *
 */

int lcfgdiffresource_compare( const LCFGDiffResource * resdiff1, 
                              const LCFGDiffResource * resdiff2 ) {
  assert( resdiff1 != NULL );
  assert( resdiff2 != NULL );

  const char * name1 = lcfgdiffresource_get_name(resdiff1);
  if ( name1 == NULL )
    name1 = "";

  const char * name2 = lcfgdiffresource_get_name(resdiff2);
  if ( name2 == NULL )
    name2 = "";

  return strcmp( name1, name2 );
}

/**
 * @brief Find the differences between two resources
 *
 * This takes two @c LCFGResource and creates a new @c
 * LCFGDiffResource to represent the differences (if any) between the
 * resources.
 *
 * To avoid memory leaks, when it is no longer required the @c
 * lcfgdiffresource_relinquish() function should be called.
 *
 * @param[in] old_res Pointer to the @e old @c LCFGResource (may be @c NULL)
 * @param[in] new_res Pointer to the @e new @c LCFGResource (may be @c NULL)
 * @param[out] result Reference to pointer to the new @c LCFGDiffResource
 *
 * @return Integer representing the type of differences
 *
 */

LCFGChange lcfgresource_diff( LCFGResource * old_res,
                              LCFGResource * new_res,
                              LCFGDiffResource ** result ) {

  bool ok = true;

  /* If both resources are specified require that they have same name */

  if ( old_res != NULL && new_res != NULL )
    ok = lcfgresource_same_name( old_res, new_res );

  LCFGDiffResource * resdiff = NULL;

  if (ok) {
    resdiff = lcfgdiffresource_new();
    lcfgdiffresource_set_old( resdiff, old_res );
    lcfgdiffresource_set_new( resdiff, new_res );
  }

  *result = resdiff;

  return ( ok ? lcfgdiffresource_get_type(resdiff) : LCFG_CHANGE_ERROR );
}

/* eof */
