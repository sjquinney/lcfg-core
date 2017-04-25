/**
 * @file tags.c
 * @brief Functions for working with LCFG resource tags
 * @author Stephen Quinney <squinney@inf.ed.ac.uk>
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

LCFGTag * lcfgtag_new() {

  LCFGTag * tag = malloc( sizeof(LCFGTag) );
  if ( tag == NULL ) {
    perror("Failed to allocate memory for LCFG resource tag");
    exit(EXIT_FAILURE);
  }

  /* Default values */

  tag->name      = NULL;
  tag->name_len  = 0;
  tag->_refcount = 1;

  return tag;
}

void lcfgtag_destroy( LCFGTag * tag ) {

  if ( tag == NULL ) return;

  free(tag->name);
  tag->name = NULL;

  free(tag);
  tag = NULL;

}

void lcfgtag_acquire( LCFGTag * tag ) {
  assert( tag != NULL );

  tag->_refcount += 1;
}

void lcfgtag_relinquish( LCFGTag * tag ) {

  if ( tag == NULL ) return;

  if ( tag->_refcount > 0 )
    tag->_refcount -= 1;

  if ( tag->_refcount == 0 )
    lcfgtag_destroy(tag);

}

bool lcfgtag_is_valid( const LCFGTag * tag ) {
  return ( tag != NULL && tag->name != NULL );
}

bool lcfgresource_valid_tag( const char * value ) {

  /* MUST NOT be a NULL.
     MUST have non-zero length.
     MUST NOT contain whitespace characters.
     TODO : decide if checking for !isword would be better */

  bool valid = !isempty(value);

  char * ptr;
  for ( ptr = (char *) value; valid && *ptr != '\0'; ptr++ )
    if ( isspace(*ptr) ) valid = false;

  return valid;
}

bool lcfgtag_set_name( LCFGTag * tag, char * new_name ) {
  assert( tag != NULL );

  bool ok = false;
  if ( lcfgresource_valid_tag(new_name) ) {
    free(tag->name);

    tag->name     = new_name;
    tag->name_len = strlen(new_name);

    ok = true;
  } else {
    errno = EINVAL;
  }

  return ok;
}

char * lcfgtag_get_name( const LCFGTag * tag ) {
  assert( tag != NULL );

  return tag->name;
}

size_t lcfgtag_get_length( const LCFGTag * tag ) {
  assert( tag != NULL );

  return tag->name_len;
}

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

int lcfgtag_compare( const LCFGTag * tag, const LCFGTag * other ) {
  assert( tag != NULL );
  assert( other != NULL );

  return strcmp( tag->name, other->name );
}

bool lcfgtag_matches( const LCFGTag * tag, const char * name ) {
  assert( tag != NULL );
  assert( name != NULL );

  size_t required_len = strlen(name);

  return ( required_len == tag->name_len && 
           strncmp( tag->name, name,  tag->name_len ) == 0 ); 
}

/* eof */

