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

static const char * tag_separators = " \t\r\n";

bool lcfgresource_value_map_tagstring( LCFGResource * res,
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
  const char * tag = strtok_r( input, tag_separators, &saveptr );
  while (tag != NULL) {
    if ( ! (*fn)( res, tag ) ) {
      ok = false;
      break;
    }
    tag = strtok_r( NULL, tag_separators, &saveptr );
  }
  free(input);

  return ok;
}

const char * lcfgresource_value_find_tag( const LCFGResource * res,
                                          const char * tag ) {

  if ( !lcfgresource_has_value(res) ) return NULL;

  const char * cur_value = lcfgresource_get_value(res);

  const char * location = NULL;
  size_t tag_len = strlen(tag);

  const char * match = strstr( cur_value, tag );
  while ( match != NULL ) {

    /* string starts with tag or space before tag */
    if ( cur_value == match || isspace( *( match - 1 ) ) ) {

      size_t match_len = strlen(match);

      /* string ends with tag or space after tag */
      if ( tag_len == match_len || isspace( *( match + tag_len ) ) ) {
        location = match;
        break;
      }

    }

    /* Move past the current (failed) match */
    match = strstr( match + tag_len, tag );
  }

  return location;
}

bool lcfgresource_value_has_tag( const LCFGResource * res, const char * tag ) {
  return ( tag != NULL && lcfgresource_value_find_tag( res, tag ) != NULL );
}

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
     subsequent whitespace separator. */

  size_t new_tag_len = 0;
  if ( removal ) {

    size_t whitecount=0;

    const char * after = location + old_tag_len;
    while( *after != '\0' && isspace(*after) ) {
      whitecount++;
      after++;
    }
    old_tag_len += whitecount;

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

bool lcfgresource_value_remove_tag( LCFGResource * res,
                                    const char * tag ) {

  return lcfgresource_value_replace_tag( res, tag, NULL );
}

bool lcfgresource_value_remove_tags( LCFGResource * res,
                                     const char * unwanted_tags ) {

  return lcfgresource_value_map_tagstring( res, unwanted_tags,
                                           &lcfgresource_value_remove_tag );
}

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

  /* Check if the final character in the string is already a space */
  bool add_space=false;
  if ( cur_len > 0 && !isspace( *( cur_value + cur_len - 1 ) ) ) {
    add_space=true;
    new_len++;  /* +1 for single space */
  }

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate more memory for LCFG resource value" );
    exit(EXIT_FAILURE);
  }

  char * to = result;

  if ( cur_len > 0 )
    to = stpncpy( to, cur_value, cur_len );

  if (add_space) {
    *to = ' ';
    to++;
  }

  to = stpncpy( to, extra_tag, extra_len );

  *to = '\0';

  assert( ( result + new_len ) == to );

  bool ok = lcfgresource_set_value( res, result );
  if ( !ok )
    free(result);

  return ok;
}

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

  /* Check if the first character in the string is already a space */
  bool add_space=false;
  if ( cur_len > 0 && !isspace( *cur_value ) ) {
    add_space=true;
    new_len++;  /* +1 for single space */
  }

  char * result = calloc( ( new_len + 1 ), sizeof(char) );
  if ( result == NULL ) {
    perror("Failed to allocate more memory for LCFG resource value" );
    exit(EXIT_FAILURE);
  }

  char * to = result;

  to = stpncpy( to, extra_tag, extra_len );

  if (add_space) {
    *to = ' ';
    to++;
  }

  if ( cur_len > 0 )
    to = stpncpy( to, cur_value, cur_len );

  *to = '\0';

  assert( ( result + new_len ) == to );

  bool ok = lcfgresource_set_value( res, result );
  if ( !ok )
    free(result);

  return ok;
}

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

bool lcfgresource_value_add_tags( LCFGResource * res,
                                  const char * extra_tags ) {

  return lcfgresource_value_map_tagstring( res, extra_tags,
                                           &lcfgresource_value_add_tag );
}

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

bool lcfgresource_value_remove( LCFGResource * res,
                                const char * string ) {

  return lcfgresource_value_replace( res, string, NULL );
}

/* eof */
