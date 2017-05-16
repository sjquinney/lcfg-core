#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "resources.h"

inline static bool empty_string(const char * str) {
  return ( str == NULL || *str == '\0' );
}

static const char * tag_separators = " \t\r\n";

bool lcfgresource_value_map_tagstring( LCFGResource * res,
                                       const char * tagstring,
                                       LCFGResourceTagFunc fn ) {

  /* Only valid for strings and lists */
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) )
    return false;

  bool ok = true;

  char * input = strdup(tagstring);
  char * saveptr;
  char * tag = strtok_r( input, tag_separators, &saveptr );
  while (tag != NULL) {
    if ( ! (*fn)( res, tag ) ) {
      ok = false;
      break;
    }
    tag = strtok_r(NULL, tag_separators, &saveptr);
  }
  free(input);

  return ok;
}

char * lcfgresource_value_find_tag( const LCFGResource * res,
                                    const char * tag ) {

  if ( !lcfgresource_has_value(res) )
    return NULL;

  const char * cur_value = lcfgresource_get_value(res);

  char * location = NULL;
  int tag_len = strlen(tag);

  char * match = strstr( cur_value, tag );
  while ( match != NULL ) {

    /* string starts with tag or space before tag */
    if ( cur_value == match || isspace( *( match - 1 ) ) ) {

      int match_len = strlen(match);

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
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) )
    return false;

  /* Cannot replace an empty old tag */
  if ( empty_string(old_tag) )
    return false;

  /* Will not replace bad values into a tag list */
  if ( lcfgresource_is_list(res) && !lcfgresource_valid_list(new_tag) )
    return false;

  /* If the 'new' tag is empty then this becomes a "removal" operation. */

  bool removal = empty_string(new_tag);

  /* Check if there is currently a value */

  if ( !lcfgresource_has_value(res) ) {
    if ( removal ) {
      return true;   /* Removal successful as empty */
    } else {
      return false;  /* Cannot replace values in an empty string */
    }
  }

  const char * location = lcfgresource_value_find_tag( res, old_tag );
  if ( location == NULL ) {
    if ( removal ) { /* removal job finished if not found */
      return true;
    } else {
      return false;
    }
  }

  const char * cur_value = lcfgresource_get_value(res);

  int cur_len     = strlen(cur_value);
  int old_tag_len = strlen(old_tag);

  /* If the new tag is empty then this becomes a "removal"
     operation. That needs to not only remove the tag but any
     subsequent whitespace separator. */

  int new_tag_len = 0;
  if ( removal ) {

    int whitecount=0;

    char * after = ( (char *) location ) + old_tag_len;
    while( *after != '\0' && isspace(*after) ) {
      whitecount++;
      after++;
    }
    old_tag_len += whitecount;

  } else {
    new_tag_len = strlen(new_tag);
  }

  int new_len = cur_len - ( old_tag_len - new_tag_len );

  /* For efficiency, if the new_len is zero then just unset the value */

  if ( new_len == 0 )
    return lcfgresource_unset_value(res);

  char * new_value = calloc( ( new_len + 1 ), sizeof(char) );
  if ( new_value == NULL ) {
    perror("Failed to allocate memory for LCFG resource value");
    exit(EXIT_FAILURE);
  }

  char * target = new_value;

  int before = location - cur_value;
  if ( before > 0 ) {
    memcpy( target, cur_value, before );
    target += before;
  }

  if ( !removal ) {
    memcpy( target, new_tag, new_tag_len );
    target += new_tag_len;
  }

  int after  = cur_len - before - old_tag_len;
  if ( after > 0 ) {
    memcpy( target, location + old_tag_len, after );
    target += after;
  }

  *target = '\0';

  assert( (int)(target - new_value) == new_len );

  bool ok = lcfgresource_set_value( res, new_value );
  if ( !ok )
    free(new_value);

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
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) )
    return false;

  /* Just ignore attempts to append empty values */
  if ( empty_string(extra_value) )
    return true;

  /* Will not append bad values to a tag list */
  if ( lcfgresource_is_list(res) && !lcfgresource_valid_list(extra_value) )
    return false;

  const char * cur_value;
  int cur_len = 0;
  if ( lcfgresource_has_value(res) ) {
    cur_value = lcfgresource_get_value(res);
    cur_len   = strlen( cur_value );
  }

  int extra_len = strlen( extra_value );
  int new_len   = cur_len + extra_len;

  char * new_value = calloc( ( new_len + 1 ), sizeof(char) );
  if ( new_value == NULL ) {
    perror("Failed to allocate more memory for LCFG resource value" );
    exit(EXIT_FAILURE);
  }

  char * target = new_value;

  if ( cur_len > 0 ) {
    memcpy( target, cur_value, cur_len );
    target += cur_len;
  }

  memcpy( target, extra_value, extra_len );
  target += extra_len;

  *target = '\0';

  assert( (int)(target - new_value) == new_len );

  bool ok = lcfgresource_set_value( res, new_value );
  if ( !ok )
    free(new_value);

  return ok;
}

bool lcfgresource_value_append_tag( LCFGResource * res,
                                    const char * extra_tag ) {

  /* Only valid for strings and lists */
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) )
    return false;

  /* Just ignore attempts to append empty tags */
  if ( empty_string(extra_tag) )
    return true;

  /* Will not append bad values to a tag list */
  if ( lcfgresource_is_list(res) && !lcfgresource_valid_list(extra_tag) )
    return false;

  const char * cur_value;
  int cur_len = 0;
  if ( lcfgresource_has_value(res) ) {
    cur_value = lcfgresource_get_value(res);
    cur_len   = strlen( cur_value );
  }

  int extra_len = strlen( extra_tag );
  int new_len   = cur_len + extra_len;

  /* Check if the final character in the string is already a space */
  bool add_space=false;
  if ( cur_len > 0 && !isspace( *( cur_value + cur_len - 1 ) ) ) {
    add_space=true;
    new_len++;  /* +1 for single space */
  }

  char * new_value = calloc( ( new_len + 1 ), sizeof(char) );
  if ( new_value == NULL ) {
    perror("Failed to allocate more memory for LCFG resource value" );
    exit(EXIT_FAILURE);
  }

  char * target = new_value;

  if ( cur_len > 0 ) {
    memcpy( target, cur_value, cur_len );
    target += cur_len;
  }

  if (add_space) {
    *target = ' ';
    target++;
  }

  memcpy( target, extra_tag, extra_len );
  target += extra_len;

  *target = '\0';

  assert( (int)(target - new_value) == new_len );

  bool ok = lcfgresource_set_value( res, new_value );
  if ( !ok )
    free(new_value);

  return ok;
}

bool lcfgresource_value_prepend( LCFGResource * res,
                                 const char * extra_value ) {

  /* Only valid for strings and lists */
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) )
    return false;

  /* Just ignore attempts to append empty values */
  if ( empty_string(extra_value) )
    return true;

  /* Will not prepend bad values to a tag list */
  if ( lcfgresource_is_list(res) && !lcfgresource_valid_list(extra_value) )
    return false;

  const char * cur_value;
  int cur_len = 0;
  if ( lcfgresource_has_value(res) ) {
    cur_value = lcfgresource_get_value(res);
    cur_len   = strlen( cur_value );
  }

  int extra_len = strlen( extra_value );
  int new_len   = extra_len + cur_len;

  char * new_value = calloc( ( new_len + 1 ), sizeof(char) );
  if ( new_value == NULL ) {
    perror("Failed to allocate more memory for LCFG resource value" );
    exit(EXIT_FAILURE);
  }

  char * target = new_value;

  memcpy( target, extra_value, extra_len );
  target += extra_len;

  if ( cur_len > 0 ) {
    memcpy( target, cur_value, cur_len );
    target += cur_len;
  }

  *target = '\0';

  assert( (int)(target - new_value) == new_len );

  bool ok = lcfgresource_set_value( res, new_value );
  if ( !ok )
    free(new_value);

  return ok;
}

bool lcfgresource_value_prepend_tag( LCFGResource * res,
                                     const char * extra_tag ) {

  /* Only valid for strings and lists */
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) ) {
    return false;
  }

  /* Just ignore attempts to prepend empty tags */
  if ( empty_string(extra_tag) )
    return true;

  /* Will not prepend bad values to a tag list */
  if ( lcfgresource_is_list(res) && !lcfgresource_valid_list(extra_tag) )
    return false;

  const char * cur_value;
  int cur_len = 0;
  if ( lcfgresource_has_value(res) ) {
    cur_value = lcfgresource_get_value(res);
    cur_len   = strlen( cur_value );
  }

  int extra_len = strlen( extra_tag );
  int new_len   = extra_len + cur_len;

  /* Check if the first character in the string is already a space */
  bool add_space=false;
  if ( cur_len > 0 && !isspace( *cur_value ) ) {
    add_space=true;
    new_len++;  /* +1 for single space */
  }

  char * new_value = calloc( ( new_len + 1 ), sizeof(char) );
  if ( new_value == NULL ) {
    perror("Failed to allocate more memory for LCFG resource value" );
    exit(EXIT_FAILURE);
  }

  char * target = new_value;

  memcpy( target, extra_tag, extra_len );
  target += extra_len;

  if (add_space) {
    *target = ' ';
    target++;
  }

  if ( cur_len > 0 ) {
    memcpy( target, cur_value, cur_len );
    target += cur_len;
  }

  *target = '\0';

  assert( (int)(target - new_value) == new_len );

  bool ok = lcfgresource_set_value( res, new_value );
  if ( !ok )
    free(new_value);

  return ok;
}

bool lcfgresource_value_add_tag( LCFGResource * res,
                                 const char * extra_tag ) {

  /* Only valid for strings and lists */
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) )
    return false;

  /* Just ignore attempts to add empty tag */
  if ( empty_string(extra_tag) )
    return true;

  bool ok = true;
  if ( !lcfgresource_value_has_tag( res, extra_tag ) ) {
    ok = lcfgresource_value_append_tag( res, extra_tag );
  }

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
  if ( !lcfgresource_is_string(res) && !lcfgresource_is_list(res) )
    return false;

  /* Cannot replace an empty old value */
  if ( empty_string(old_string) )
    return false;

  /* Will not replace bad values into a tag list */
  if ( lcfgresource_is_list(res) && !lcfgresource_valid_list(new_string) )
    return false;

  /* If the 'new' string is empty then this becomes a "removal"
     operation. */

  bool removal = empty_string(new_string);

  /* Check if there is currently a value */

  if ( !lcfgresource_has_value(res) ) {
    if ( removal ) {
      return true;   /* Removal successful as empty */
    } else {
      return false;  /* Cannot replace values in an empty string */
    }
  }

  const char * cur_value = lcfgresource_get_value(res);

  /* Search for the 'old' string that will be replaced */

  const char * location = strstr( cur_value, old_string );
  if ( location == NULL ) {
    if ( removal ) { /* removal job successfully finished if not found */
      return true;
    } else {
      return false;
    }
  }

  int cur_len     = strlen(cur_value);
  int old_str_len = strlen(old_string);

  int new_str_len = removal ? 0 : strlen(new_string);

  int new_len = cur_len - ( old_str_len - new_str_len );

  /* For efficiency, if the new_len is zero then just unset the value */

  if ( new_len == 0 )
    return lcfgresource_unset_value(res);

  char * new_value = calloc( ( new_len + 1 ), sizeof(char) );
  if ( new_value == NULL ) {
    perror("Failed to allocate memory for LCFG resource value");
    exit(EXIT_FAILURE);
  }

  char * target = new_value;

  int before = location - cur_value;
  if ( before > 0 ) {
    memcpy( target, cur_value, before );
    target += before;
  }

  if ( !removal ) {
    memcpy( target, new_string, new_str_len );
    target += new_str_len;
  }

  int after = cur_len - before - old_str_len;
  if ( after > 0 ) {
    memcpy( target, location + old_str_len, after );
    target += after;
  }

  *target = '\0';

  assert( (int)(target - new_value) == new_len );

  bool ok = lcfgresource_set_value( res, new_value );
  if ( !ok )
    free(new_value);

  return ok;
}

bool lcfgresource_value_remove( LCFGResource * res,
                                const char * string ) {

  return lcfgresource_value_replace( res, string, NULL );
}

/* eof */
