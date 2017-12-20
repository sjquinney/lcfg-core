/**
 * @file resources/mutate.c
 * @brief Functions for mutating values for string and list resources
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
 * @copyright 2014-2017 University of Edinburgh. All rights reserved. This project is released under the GNU Public License version 2.
 * $Date$
 * $Revision$
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "resources.h"
#include "utils.h"

static const char allowed_separators[] = " \t\r\n";
static const char standard_separator[] = " ";
static const size_t standard_sep_len = sizeof(standard_separator) - 1;

typedef bool (*LCFGResourceTagFunc)( LCFGResource * res, const char * tag );

static bool lcfgresource_value_map_tagstring( LCFGResource * res,
                                              const char * tagstring,
                                              LCFGResourceTagFunc fn ) {

  /* Only valid for strings and lists */
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) ) {
    errno = ENOTSUP;
    return false;
  }

  bool ok = true;

  char * input = strdup(tagstring);
  char * saveptr;
  const char * tag = strtok_r( input, allowed_separators, &saveptr );
  while (tag != NULL) {
    if ( ! (*fn)( res, tag ) ) {
      ok = false;
      break;
    }
    tag = strtok_r( NULL, allowed_separators, &saveptr );
  }
  free(input);

  return ok;
}

static const char * lcfgresource_value_find_tag( const LCFGResource * res,
                                                 const char * tag ) {
  assert( tag != NULL );

  if ( !lcfgresource_has_value(res) ) return NULL;

  const char * cur_value = lcfgresource_get_value(res);

  return lcfgutils_string_finditem( cur_value, tag, allowed_separators );
}

/**
 * @brief Check if the value for the resource contains a tag
 *
 * Searches the value for the @c LCFGResource to see if it contains
 * the specified tag.
 *
 * A @e tag is considered to be a sub-string which is space-separated
 * within the resource value (e.g. "foo" is a tag in the value "foo
 * bar baz" or "bar foo baz" or "bar baz foo"). For a string resource
 * the @e tag may be any string, for a list resource it must be a
 * valid LCFG tag name (see the lcfgresource_valid_tag() function docs
 * for details).
 *
 * @param res Pointer to @c LCFGResource
 * @param tag Tag to check for in the resource value
 *
 * @return Boolean which indicates if resource value contains tag
 *
 */

bool lcfgresource_value_has_tag( const LCFGResource * res, const char * tag ) {
  return ( tag != NULL && lcfgresource_value_find_tag( res, tag ) != NULL );
}

/**
 * @brief Replace the first instance of a tag in the value for the resource
 *
 * Finds the first instance of the specified tag in the value for the
 * @c LCFGResource and replaces it with the new tag. If the new tag is
 * @c NULL or the empty string then the original tag will be removed.
 *
 * A @e tag is considered to be a sub-string which is space-separated
 * within the resource value (e.g. "foo" is a tag in the value "foo
 * bar baz" or "bar foo baz" or "bar baz foo"). For a string resource
 * the @e tag may be any string, for a list resource it must be a
 * valid LCFG tag name (see the lcfgresource_valid_tag() function docs
 * for details).
 *
 * @param res Pointer to @c LCFGResource
 * @param old_tag Original tag to be replaced
 * @param new_tag Tag to be inserted in place of original
 *
 * @return Boolean which indicates success
 *
 */

bool lcfgresource_value_replace_tag( LCFGResource * res,
                                     const char * old_tag,
                                     const char * new_tag ) {

  /* Only valid for strings and lists */
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) ) {
    errno = ENOTSUP;
    return false;
  }

  /* Cannot replace an empty old tag */
  if ( isempty(old_tag) ) return false;

  if ( strcmp( old_tag, new_tag ) == 0 ) return true;

  /* Will not replace bad values into a tag list */
  if ( lcfgresource_is_list(res) && !lcfgresource_valid_list(new_tag) ) {
    errno = EINVAL;
    return false;
  }

  /* If the 'new' tag is empty then this becomes a "removal" operation. */

  bool removal = isempty(new_tag);

  /* Check if there is currently a value */

  if ( !lcfgresource_has_value(res) ) {
    if ( removal )
      return true;   /* Removal successful as empty */
    else
      return false;  /* Cannot replace values in an empty string */
  }

  const char * location = lcfgresource_value_find_tag( res, old_tag );
  if ( location == NULL ) {
    if ( removal ) /* removal job finished if not found */
      return true;
    else
      return false;
  }

  const char * cur_value = lcfgresource_get_value(res);

  size_t cur_len     = strlen(cur_value);
  size_t old_tag_len = strlen(old_tag);

  /* If the new tag is empty then this becomes a "removal"
     operation. That needs to not only remove the tag but any
     subsequent separator characters. */

  size_t new_tag_len = 0;
  if ( removal ) {

    size_t sepcount=0;

    const char * after = NULL;
    for ( after = location + old_tag_len;
          *after != '\0' && strchr( allowed_separators, *after ) != NULL;
          after++ ) {
      sepcount++;
    }

    old_tag_len += sepcount;

  } else {
    new_tag_len = strlen(new_tag);
  }

  size_t new_len = cur_len - ( old_tag_len - new_tag_len );

  /* For efficiency, if the new_len is zero then just unset the value */

  if ( new_len == 0 ) return lcfgresource_unset_value(res);

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate memory for LCFG resource value");
    exit(EXIT_FAILURE);
  }

  char * to = result;

  size_t before_len = location - cur_value;
  if ( before_len > 0 )
    to = stpncpy( to, cur_value, before_len );

  if ( !removal )
    to = stpncpy( to, new_tag, new_tag_len );

  size_t after_len  = cur_len - before_len - old_tag_len;
  if ( after_len > 0 )
    to = stpncpy( to, location + old_tag_len, after_len );

  *to = '\0';

  assert( ( result + new_len ) == to );

  bool ok = lcfgresource_set_value( res, result );
  if ( !ok )
    free(result);

  return ok;
}

/**
 * @brief Remove the first instance of a tag in the value for the resource
 *
 * Finds and removes the first instance of the specified tag within
 * the value for the @c LCFGResource.
 *
 * A @e tag is considered to be a sub-string which is space-separated
 * within the resource value (e.g. "foo" is a tag in the value "foo
 * bar baz" or "bar foo baz" or "bar baz foo"). For a string resource
 * the @e tag may be any string, for a list resource it must be a
 * valid LCFG tag name (see the lcfgresource_valid_tag() function docs
 * for details).
 *
 * @param res Pointer to @c LCFGResource
 * @param unwanted_tag Tag to be removed
 *
 * @return Boolean which indicates success
 *
 */

bool lcfgresource_value_remove_tag( LCFGResource * res,
                                    const char * unwanted_tag ) {

  return lcfgresource_value_replace_tag( res, unwanted_tag, NULL );
}

/**
 * @brief Replace all instances of a tag in the value for the resource
 *
 * Finds and removes all tags in the space-separated list that are
 * found in the value for the @c LCFGResource.
 *
 * A @e tag is considered to be a sub-string which is space-separated
 * within the resource value (e.g. "foo" is a tag in the value "foo
 * bar baz" or "bar foo baz" or "bar baz foo"). For a string resource
 * the @e tag may be any string, for a list resource it must be a
 * valid LCFG tag name (see the lcfgresource_valid_tag() function docs
 * for details).
 *
 * @param res Pointer to @c LCFGResource
 * @param unwanted_tags Space-separated string of tags to be removed
 *
 * @return Boolean which indicates success
 *
 */

bool lcfgresource_value_remove_tags( LCFGResource * res,
                                     const char * unwanted_tags ) {

  return lcfgresource_value_map_tagstring( res, unwanted_tags,
                                           &lcfgresource_value_remove_tag );
}

/**
 * @brief Append a string to the value for the resource
 *
 * Appends the specified string to the value for the @c LCFGResource.
 *
 * @param res Pointer to @c LCFGResource
 * @param extra_value String to be appended
 *
 * @return Boolean which indicates success
 *
 */

bool lcfgresource_value_append( LCFGResource * res,
                                const char * extra_value ) {

  /* Only valid for strings and lists */
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) ) {
    errno = ENOTSUP;
    return false;
  }

  /* Just ignore attempts to append empty values */
  if ( isempty(extra_value) ) return true;

  /* Will not append bad values to a tag list */
  if ( lcfgresource_is_list(res) && !lcfgresource_valid_list(extra_value) ) {
    errno = EINVAL;
    return false;
  }

  const char * cur_value = NULL;
  size_t cur_len = 0;
  if ( lcfgresource_has_value(res) ) {
    cur_value = lcfgresource_get_value(res);
    cur_len   = strlen( cur_value );
  }

  size_t extra_len = strlen( extra_value );
  size_t new_len   = cur_len + extra_len;

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate more memory for LCFG resource value" );
    exit(EXIT_FAILURE);
  }

  char * to = result;

  if ( cur_len > 0 )
    to = stpncpy( to, cur_value, cur_len );

  to = stpncpy( to, extra_value, extra_len );

  *to = '\0';

  assert( ( result + new_len ) == to );

  bool ok = lcfgresource_set_value( res, result );
  if ( !ok )
    free(result);

  return ok;
}

/**
 * @brief Append a tag to the value for the resource
 *
 * Appends the specified tag to the value for the @c LCFGResource.
 *
 * A @e tag is considered to be a sub-string which is space-separated
 * within the resource value (e.g. "foo" is a tag in the value "foo
 * bar baz" or "bar foo baz" or "bar baz foo"). For a string resource
 * the @e tag may be any string, for a list resource it must be a
 * valid LCFG tag name (see the lcfgresource_valid_tag() function docs
 * for details).
 *
 * @param res Pointer to @c LCFGResource
 * @param extra_tag Tag to be appended
 *
 * @return Boolean which indicates success
 *
 */

bool lcfgresource_value_append_tag( LCFGResource * res,
                                    const char * extra_tag ) {

  /* Only valid for strings and lists */
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) ) {
    errno = ENOTSUP;
    return false;
  }

  /* Just ignore attempts to append empty tags */
  if ( isempty(extra_tag) ) return true;

  /* Will not append bad values to a tag list */
  if ( lcfgresource_is_list(res) && !lcfgresource_valid_list(extra_tag) ) {
    errno = EINVAL;
    return false;
  }

  const char * cur_value = NULL;
  size_t cur_len = 0;
  if ( lcfgresource_has_value(res) ) {
    cur_value = lcfgresource_get_value(res);
    cur_len   = strlen( cur_value );
  }

  size_t extra_len = strlen( extra_tag );
  size_t new_len   = cur_len + extra_len;

  /* Check if the final character in the string is already a separator */
  bool add_sep=false;
  if ( cur_len > 0 && strchr( allowed_separators, *( cur_value + cur_len - 1 ) ) == NULL ) {
    add_sep=true;
    new_len += standard_sep_len;  /* +1 for single space */
  }

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate more memory for LCFG resource value" );
    exit(EXIT_FAILURE);
  }

  char * to = result;

  if ( cur_len > 0 )
    to = stpncpy( to, cur_value, cur_len );

  if (add_sep)
    to = stpncpy( to, standard_separator, standard_sep_len );

  to = stpncpy( to, extra_tag, extra_len );

  *to = '\0';

  assert( ( result + new_len ) == to );

  bool ok = lcfgresource_set_value( res, result );
  if ( !ok )
    free(result);

  return ok;
}

/**
 * @brief Prepend a string to the value for the resource
 *
 * Prepends the specified string to the value for the @c LCFGResource.
 *
 * @param res Pointer to @c LCFGResource
 * @param extra_value String to be prepended
 *
 * @return Boolean which indicates success
 *
 */

bool lcfgresource_value_prepend( LCFGResource * res,
                                 const char * extra_value ) {

  /* Only valid for strings and lists */
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) ) {
    errno = ENOTSUP;
    return false;
  }

  /* Just ignore attempts to append empty values */
  if ( isempty(extra_value) ) return true;

  /* Will not prepend bad values to a tag list */
  if ( lcfgresource_is_list(res) && !lcfgresource_valid_list(extra_value) ) {
    errno = EINVAL;
    return false;
  }

  const char * cur_value = NULL;
  size_t cur_len = 0;
  if ( lcfgresource_has_value(res) ) {
    cur_value = lcfgresource_get_value(res);
    cur_len   = strlen( cur_value );
  }

  size_t extra_len = strlen( extra_value );
  size_t new_len   = extra_len + cur_len;

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate more memory for LCFG resource value" );
    exit(EXIT_FAILURE);
  }

  char * to = result;

  to = stpncpy( to, extra_value, extra_len );

  if ( cur_len > 0 )
    to = stpncpy( to, cur_value, cur_len );

  *to = '\0';

  assert( ( result + new_len ) == to );

  bool ok = lcfgresource_set_value( res, result );
  if ( !ok )
    free(result);

  return ok;
}

/**
 * @brief Prepend a tag to the value for the resource
 *
 * Prepends the specified tag to the value for the @c LCFGResource.
 *
 * A @e tag is considered to be a sub-string which is space-separated
 * within the resource value (e.g. "foo" is a tag in the value "foo
 * bar baz" or "bar foo baz" or "bar baz foo"). For a string resource
 * the @e tag may be any string, for a list resource it must be a
 * valid LCFG tag name (see the lcfgresource_valid_tag() function docs
 * for details).
 *
 * @param res Pointer to @c LCFGResource
 * @param extra_tag Tag to be prepended
 *
 * @return Boolean which indicates success
 *
 */

bool lcfgresource_value_prepend_tag( LCFGResource * res,
                                     const char * extra_tag ) {

  /* Only valid for strings and lists */
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) ) {
    errno = ENOTSUP;
    return false;
  }

  /* Just ignore attempts to prepend empty tags */
  if ( isempty(extra_tag) ) return true;

  /* Will not prepend bad values to a tag list */
  if ( lcfgresource_is_list(res) && !lcfgresource_valid_list(extra_tag) ) {
    errno = EINVAL;
    return false;
  }

  const char * cur_value = NULL;
  size_t cur_len = 0;
  if ( lcfgresource_has_value(res) ) {
    cur_value = lcfgresource_get_value(res);
    cur_len   = strlen( cur_value );
  }

  size_t extra_len = strlen( extra_tag );
  size_t new_len   = extra_len + cur_len;

  /* Check if the first character in the string is already a separator */
  bool add_sep=false;
  if ( cur_len > 0 && strchr( allowed_separators, *cur_value ) == NULL ) {
    add_sep=true;
    new_len += standard_sep_len;  /* +1 for single space */
  }

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate more memory for LCFG resource value" );
    exit(EXIT_FAILURE);
  }

  char * to = result;

  to = stpncpy( to, extra_tag, extra_len );

  if (add_sep)
    to = stpncpy( to, standard_separator, standard_sep_len );

  if ( cur_len > 0 )
    to = stpncpy( to, cur_value, cur_len );

  *to = '\0';

  assert( ( result + new_len ) == to );

  bool ok = lcfgresource_set_value( res, result );
  if ( !ok )
    free(result);

  return ok;
}

/**
 * @brief Append a tag to a resource value if it is not already present
 *
 * Searches for the specified tag in the value for the @c LCFGResource
 * and appends it if does not already exist.
 *
 * A @e tag is considered to be a sub-string which is space-separated
 * within the resource value (e.g. "foo" is a tag in the value "foo
 * bar baz" or "bar foo baz" or "bar baz foo"). For a string resource
 * the @e tag may be any string, for a list resource it must be a
 * valid LCFG tag name (see the lcfgresource_valid_tag() function docs
 * for details).
 *
 * @param res Pointer to @c LCFGResource
 * @param extra_tag Tag to be appended (if not present)
 *
 * @return Boolean which indicates success
 *
 */

bool lcfgresource_value_add_tag( LCFGResource * res,
                                 const char * extra_tag ) {

  /* Only valid for strings and lists */
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) ) {
    errno = ENOTSUP;
    return false;
  }

  /* Just ignore attempts to add empty tag */
  if ( isempty(extra_tag) ) return true;

  bool ok = true;
  if ( !lcfgresource_value_has_tag( res, extra_tag ) )
    ok = lcfgresource_value_append_tag( res, extra_tag );

  return ok;
}

/**
 * @brief Append a list of tags to a resource value if not already present
 *
 * For each tag in the space-separated list this searches the value
 * for the @c LCFGResource and appends the tag if does not already
 * exist.
 *
 * A @e tag is considered to be a sub-string which is space-separated
 * within the resource value (e.g. "foo" is a tag in the value "foo
 * bar baz" or "bar foo baz" or "bar baz foo"). For a string resource
 * the @e tag may be any string, for a list resource it must be a
 * valid LCFG tag name (see the lcfgresource_valid_tag() function docs
 * for details).
 *
 * @param res Pointer to @c LCFGResource
 * @param extra_tags Space-separated string of tags to be appended (if not present)
 *
 * @return Boolean which indicates success
 *
 */

bool lcfgresource_value_add_tags( LCFGResource * res,
                                  const char * extra_tags ) {

  return lcfgresource_value_map_tagstring( res, extra_tags,
                                           &lcfgresource_value_add_tag );
}

/**
 * @brief Replace a substring within the value for the resource
 *
 * Searches for the specified substring and replaces the first
 * instance found with the new substring. The new substring may be @c
 * NULL or the empty string in which case the original substring is
 * simply removed.
 *
 * @param res Pointer to @c LCFGResource
 * @param old_string Original string to be replaced
 * @param new_string New string to be inserted in place of original
 *
 * @return Boolean which indicates success
 *
 */

bool lcfgresource_value_replace( LCFGResource * res,
                                 const char * old_string,
                                 const char * new_string ) {

  /* Only valid for strings and lists */
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) ) {
    errno = ENOTSUP;
    return false;
  }

  /* Cannot replace an empty old value */
  if ( isempty(old_string) ) return false;

  if ( strcmp( old_string, new_string ) == 0 ) return true;

  /* Will not replace bad values into a tag list */
  if ( lcfgresource_is_list(res) && !lcfgresource_valid_list(new_string) ) {
    errno = EINVAL;
    return false;
  }

  /* If the 'new' string is empty then this becomes a "removal"
     operation. */

  bool removal = isempty(new_string);

  /* Check if there is currently a value */

  if ( !lcfgresource_has_value(res) ) {
    if ( removal )
      return true;   /* Removal successful as empty */
    else
      return false;  /* Cannot replace values in an empty string */
  }

  const char * cur_value = lcfgresource_get_value(res);

  /* Search for the 'old' string that will be replaced */

  const char * location = strstr( cur_value, old_string );
  if ( location == NULL ) {
    if ( removal ) /* removal job successfully finished if not found */
      return true;
    else
      return false;
  }

  size_t cur_len     = strlen(cur_value);
  size_t old_str_len = strlen(old_string);

  size_t new_str_len = removal ? 0 : strlen(new_string);

  size_t new_len = cur_len - ( old_str_len - new_str_len );

  /* For efficiency, if the new_len is zero then just unset the value */

  if ( new_len == 0 )
    return lcfgresource_unset_value(res);

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate memory for LCFG resource value");
    exit(EXIT_FAILURE);
  }

  char * to = result;

  size_t before_len = location - cur_value;
  if ( before_len > 0 )
    to = stpncpy( to, cur_value, before_len );

  if ( !removal )
    to = stpncpy( to, new_string, new_str_len );

  size_t after_len = cur_len - before_len - old_str_len;
  if ( after_len > 0 )
    to = stpncpy( to, location + old_str_len, after_len );

  *to = '\0';

  assert( ( result + new_len ) == to );

  bool ok = lcfgresource_set_value( res, result );
  if ( !ok )
    free(result);

  return ok;
}

/**
 * @brief Remove a substring from the value for the resource
 *
 * Searches for the specified substring and removes the first instance
 * from the value for the @c LCFGResource.
 *
 * @param res Pointer to @c LCFGResource
 * @param unwanted_string String to be removed
 *
 * @return Boolean which indicates success
 *
 */

bool lcfgresource_value_remove( LCFGResource * res,
                                const char * unwanted_string ) {

  return lcfgresource_value_replace( res, unwanted_string, NULL );
}

/* eof */
