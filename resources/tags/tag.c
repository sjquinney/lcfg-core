/**
 * @file resources/tags/tag.c
 * @brief Functions for working with LCFG resource tags
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "utils.h"
#include "tags.h"

/**
 * @brief Create and initialise a new tag
 *
 * Creates a new @c LCFGTag and initialises the parameters to the
 * default values.
 *
 * If the memory allocation for the new structure is not successful
 * the @c exit() function will be called with a non-zero value.
 *
 * The reference count for the structure is initialised to 1. To avoid
 * memory leaks, when it is no longer required the
 * @c lcfgtag_relinquish() function should be called.
 *
 * @return Pointer to new @c LCFGTag
 *
 */

LCFGTag * lcfgtag_new() {

  LCFGTag * tag = malloc( sizeof(LCFGTag) );
  if ( tag == NULL ) {
    perror("Failed to allocate memory for LCFG resource tag");
    exit(EXIT_FAILURE);
  }

  /* Default values */

  tag->name      = NULL;
  tag->name_len  = 0;
  tag->hash      = 1;
  tag->_refcount = 1;

  return tag;
}

/**
 * @brief Destroy the tag
 *
 * When the specified @c LCFGTag is no longer required this will
 * free all associated memory. There is support for reference counting
 * so typically the @c lcfgtag_relinquish() function should be used.
 *
 * This will call @c free() on each parameter of the structure (or
 * @c lcfgtemplate_destroy() for the template parameter ) and then set each
 * value to be @c NULL.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * tag which has already been destroyed (or potentially was never
 * created).
 *
 * @param[in] tag Pointer to @c LCFGTag to be destroyed.
 *
 */

void lcfgtag_destroy( LCFGTag * tag ) {

  if ( tag == NULL ) return;

  free(tag->name);
  tag->name = NULL;

  free(tag);
  tag = NULL;

}

/**
 * @brief Acquire reference to tag
 *
 * This is used to record a reference to the @c LCFGTag, it
 * does this by simply incrementing the reference count.
 *
 * To avoid memory leaks, once the reference to the structure is no
 * longer required the @c lcfgtag_relinquish() function should be
 * called.
 *
 * @param[in] tag Pointer to @c LCFGTag
 *
 */

void lcfgtag_acquire( LCFGTag * tag ) {
  assert( tag != NULL );

  tag->_refcount += 1;
}

/**
 * @brief Release reference to tag
 *
 * This is used to release a reference to the @c LCFGTag,
 * it does this by simply decrementing the reference count. If the
 * reference count reaches zero the @c lcfgtag_destroy() function
 * will be called to clean up the memory associated with the structure.
 *
 * If the value of the pointer passed in is @c NULL then the function
 * has no affect. This means it is safe to call with a pointer to a
 * tag which has already been destroyed (or potentially was never
 * created).
 *
 * @param[in] tag Pointer to @c LCFGTag
 *
 */

void lcfgtag_relinquish( LCFGTag * tag ) {

  if ( tag == NULL ) return;

  if ( tag->_refcount > 0 )
    tag->_refcount -= 1;

  if ( tag->_refcount == 0 )
    lcfgtag_destroy(tag);

}

/**
 * @brief Check validity of tag
 *
 * Checks the specified @c LCFGTag to ensure that it is valid. For a
 * tag to be considered valid the pointer must not be @c NULL and
 * the tag must have a name.
 *
 * @param[in] tag Pointer to an @c LCFGTag
 *
 * @return Boolean which indicates if tag is valid
 *
 */

bool lcfgtag_is_valid( const LCFGTag * tag ) {
  return ( tag != NULL && lcfgtag_has_name(tag) );
}

/**
 * @brief Check if a string is a valid LCFG tag name
 *
 * Checks the contents of a specified string against the specification
 * for an LCFG tag name.
 *
 * An LCFG tag name MUST be at least one character in length and it
 * must NOT contain any whitespace characters.
 *
 * @param[in] name String to be tested
 *
 * @return boolean which indicates if string is a valid tag name
 *
 */

bool lcfgresource_valid_tag( const char * name ) {

  /* MUST NOT be a NULL.
     MUST have non-zero length.
     MUST NOT contain whitespace characters.
     TODO : decide if checking for !isword would be better */

  bool valid = !isempty(name);

  char * ptr;
  for ( ptr = (char *) name; valid && *ptr != '\0'; ptr++ )
    if ( isspace(*ptr) ) valid = false;

  return valid;
}

/**
 * @brief Check if the tag has a name
 *
 * Checks if the specified @c LCFGTag currently has a value set
 * for the @e name attribute. Although a name is required for an LCFG
 * tag to be valid it is possible for the value of the name to be
 * set to @c NULL when the structure is first created.
 *
 * @param[in] tag Pointer to an @c LCFGTag
 *
 * @return boolean which indicates if a tag has a name
 *
 */

bool lcfgtag_has_name( const LCFGTag * tag ) {
  assert( tag != NULL );

  return !isempty(tag->name);
}

/**
 * @brief Set the name for the tag
 *
 * Sets the value of the @e name parameter for the @c LCFGTag
 * to that specified. It is important to note that this does
 * @b NOT take a copy of the string. Furthermore, once the value is set
 * the tag assumes "ownership", the memory will be freed if the
 * name is further modified or the tag is destroyed.
 *
 * Before changing the value of the @e name to be the new string it
 * will be validated using the @c lcfgresource_valid_tag()
 * function. If the new string is not valid then no change will occur,
 * the @c errno will be set to @c EINVAL and the function will return
 * a @c false value.
 *
 * @param[in] tag Pointer to an @c LCFGTag
 * @param[in] new_name String which is the new name
 *
 * @return boolean indicating success
 *
 */

bool lcfgtag_set_name( LCFGTag * tag, char * new_name ) {
  assert( tag != NULL );

  bool ok = false;
  if ( lcfgresource_valid_tag(new_name) ) {
    free(tag->name);

    tag->name     = new_name;
    tag->name_len = strlen(new_name);
    tag->hash     = lcfgutils_string_djbhash( new_name, NULL );

    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

/**
 * @brief Get the name for the tag
 *
 * This returns the value of the @e name parameter for the @c
 * LCFGTag. If the tag does not currently have a @e name then the
 * pointer returned will be @c NULL.
 *
 * It is important to note that this is @b NOT a copy of the string,
 * changing the returned string will modify the @e name of the
 * tag.
 *
 * @param[in] tag Pointer to an @c LCFGTag
 *
 * @return The @e name for the tag (possibly @c NULL).
 */

char * lcfgtag_get_name( const LCFGTag * tag ) {
  assert( tag != NULL );

  return tag->name;
}

/**
 * @brief Get the length of the name for the tag
 *
 * Since the length of the @e name for an @c LCFGTag is required quite
 * frequently, when a name is set for a tag the length of that name is
 * cached for efficiency.  This returns the value of the @e name_len
 * parameter for the @c LCFGTag. If the tag does not currently have a
 * @e name then the length will be zero.
 *
 * @param[in] tag Pointer to an @c LCFGTag
 *
 * @return The length of the @e name for the tag.
 *
 */

size_t lcfgtag_get_length( const LCFGTag * tag ) {
  assert( tag != NULL );

  return tag->name_len;
}

/**
 * @brief Get the hash for the tag name
 *
 * This returns the integer hash value for the @e name for the @c
 * LCFGTag. This is computed using the DJB hash algorithm.
 *
 * @param[in] tag Pointer to an @c LCFGTag
 *
 * @return The integer hash for the @e name for the tag.
 *
 */

unsigned int lcfgtag_get_hash( const LCFGTag * tag ) {
  assert( tag != NULL );

  return tag->hash;
}

/**
 * @brief Creating a new tag from a string
 *
 * This will create a new @c LCFGTag using the contents of the
 * specified string as the tag name. Any leading whitespace in the
 * string will be ignored. Note that this does NOT alter or take
 * "ownership" of the original input string.
 *
 * If the string does not contain a valid tag name the
 * @c LCFG_STATUS_ERROR value will be returned.
 *
 * @param[in] input The input string
 * @param[out] result Reference to the pointer for the new @c LCFGTag
 * @param[out] msg Pointer to any diagnostic messages.
 *
 * @return Status value indicating success of the process
 *
 */

LCFGStatus lcfgtag_from_string( const char * input,
                                LCFGTag ** result,
                                char ** msg ) {

  if ( isempty(input) ) {
    lcfgutils_build_message( msg, "Empty tag" );
    return LCFG_STATUS_ERROR;
  }

  /* Move past any leading whitespace */

  char * start = (char *) input;
  while ( *start != '\0' && isspace(*start) ) start++;

  if ( isempty(start) ) {
    lcfgutils_build_message( msg, "Empty tag" );
    return LCFG_STATUS_ERROR;
  }

  LCFGTag * tag = lcfgtag_new();

  char * name = strdup(start);
  bool ok = lcfgtag_set_name( tag, name );

  if ( !ok ) {
    lcfgutils_build_message( msg, "Invalid tag name '%s'", name );
    free(name);
    lcfgtag_destroy(tag);
    tag = NULL;
  }

  *result = tag;

  return ( ok ? LCFG_STATUS_OK : LCFG_STATUS_ERROR );
}

/**
 * @brief Compare the tag names
 *
 * This compares the names for two tags, this is mostly useful
 * for sorting lists of tags. An integer value is returned which
 * indicates lesser than, equal to or greater than in the same way as
 * @c strcmp(3).
 *
 * @param[in] tag Pointer to @c LCFGTag
 * @param[in] other Pointer to @c LCFGTag
 * 
 * @return Integer (-1,0,+1) indicating lesser,equal,greater
 *
 */

int lcfgtag_compare( const LCFGTag * tag, const LCFGTag * other ) {
  assert( tag != NULL );
  assert( other != NULL );

  return strcmp( tag->name, other->name );
}

/**
 * @brief Test if tag name matches string
 *
 * This compares the name of the @c LCFGTag with the specified string.
 *
 * @param[in] tag Pointer to @c LCFGTag
 * @param[in] name The name to check for a match
 *
 * @return boolean indicating equality of values
 *
 */

bool lcfgtag_match( const LCFGTag * tag, const char * name ) {
  assert( tag != NULL );
  assert( name != NULL );

  size_t required_len = strlen(name);

  return ( required_len == tag->name_len && 
           strncmp( tag->name, name,  tag->name_len ) == 0 );
}

/* eof */

